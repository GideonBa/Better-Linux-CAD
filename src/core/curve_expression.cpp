#include "blcad/core/curve_expression.hpp"
#include "blcad/core/part_document.hpp"
#include "curve_expression_internal.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>
#include <utility>

namespace blcad {

namespace {
using curve_expression_detail::CurveExpressionData;
using curve_expression_detail::CurveExpressionNode;
using curve_expression_detail::CurveOp;
using curve_expression_detail::parameter_unit;
bool letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool digit(char c) {
  return c >= '0' && c <= '9';
}
std::optional<CurveOp> function(std::string_view name) {
  if (name == "sin")
    return CurveOp::Sin;
  if (name == "cos")
    return CurveOp::Cos;
  if (name == "tan")
    return CurveOp::Tan;
  if (name == "sqrt")
    return CurveOp::Sqrt;
  if (name == "exp")
    return CurveOp::Exp;
  if (name == "log")
    return CurveOp::Log;
  if (name == "abs")
    return CurveOp::Abs;
  return std::nullopt;
}

struct Parser {
  std::string_view source;
  std::optional<CurveVariable> variable;
  const PartDocument* document;
  CurveExpressionData data;
  std::size_t position{}, depth{};

  [[noreturn]] void fail(std::string message) const {
    throw Error::validation(data.object_id,
                            std::move(message) + " at byte " + std::to_string(position));
  }
  void whitespace() {
    while (position < source.size() && (source[position] == ' ' || source[position] == '\t' ||
                                        source[position] == '\n' || source[position] == '\r'))
      ++position;
  }
  bool consume(char c) {
    whitespace();
    if (position < source.size() && source[position] == c) {
      ++position;
      return true;
    }
    return false;
  }
  char peek() {
    whitespace();
    return position < source.size() ? source[position] : '\0';
  }
  std::size_t append(CurveExpressionNode node) {
    if (data.nodes.size() >= 512)
      fail("Curve expression exceeds 512 nodes");
    data.nodes.push_back(std::move(node));
    return data.nodes.size() - 1;
  }
  std::size_t constant(double number, CurveExpressionUnit unit = {}, bool pi_symbol = false,
                       double pi_scale = 1, int pi_divisor = 1) {
    CurveExpressionNode node;
    node.number = number;
    node.unit = unit;
    node.pi_symbol = pi_symbol;
    node.pi_scale = pi_scale;
    node.pi_divisor = pi_divisor;
    return append(std::move(node));
  }
  std::size_t operation(CurveOp op, std::size_t a, std::size_t b = 0, int exponent = 0) {
    const auto left = data.nodes[a];
    const auto right = data.nodes[b];
    CurveExpressionNode node;
    node.op = op;
    node.a = a;
    node.b = b;
    node.exponent = exponent;
    node.unit = left.unit;
    node.unknown_unit = left.unknown_unit;
    node.variable = left.variable;
    if (op == CurveOp::Add || op == CurveOp::Subtract || op == CurveOp::Multiply ||
        op == CurveOp::Divide) {
      node.variable = node.variable || right.variable;
      node.unknown_unit = node.unknown_unit || right.unknown_unit;
      if (!node.unknown_unit) {
        if (op == CurveOp::Add || op == CurveOp::Subtract) {
          if (left.unit != right.unit)
            fail("Curve addition/subtraction requires matching units");
        } else {
          const int sign = op == CurveOp::Multiply ? 1 : -1;
          node.unit = {left.unit.length_power + sign * right.unit.length_power,
                       left.unit.angle_power + sign * right.unit.angle_power};
        }
      }
    } else if (op == CurveOp::Power) {
      node.variable = node.variable && exponent != 0;
      node.unit = {left.unit.length_power * exponent, left.unit.angle_power * exponent};
    } else if (op == CurveOp::Sin || op == CurveOp::Cos || op == CurveOp::Tan) {
      if (!node.unknown_unit && left.unit != CurveExpressionUnit{} &&
          left.unit != CurveExpressionUnit{0, 1})
        fail("Curve trigonometry requires scalar radians or angles");
      node.unit = {};
      node.unknown_unit = false;
    } else if (op == CurveOp::Exp || op == CurveOp::Log) {
      if (!node.unknown_unit && left.unit != CurveExpressionUnit{})
        fail("Curve exp/log requires a scalar");
      node.unit = {};
      node.unknown_unit = false;
    } else if (op == CurveOp::Sqrt) {
      if (!node.unknown_unit && (left.unit.length_power % 2 || left.unit.angle_power % 2))
        fail("Curve sqrt requires even unit powers");
      node.unit = {left.unit.length_power / 2, left.unit.angle_power / 2};
    }
    if (!node.unknown_unit &&
        (std::abs(node.unit.length_power) > 8 || std::abs(node.unit.angle_power) > 8))
      fail("Curve unit power exceeds -8..8");
    return append(std::move(node));
  }
  std::size_t expression() {
    auto a = term();
    while (true) {
      const char c = peek();
      if (c != '+' && c != '-')
        return a;
      ++position;
      a = operation(c == '+' ? CurveOp::Add : CurveOp::Subtract, a, term());
    }
  }
  std::size_t term() {
    auto a = unary();
    while (true) {
      const char c = peek();
      if (c != '*' && c != '/')
        return a;
      ++position;
      a = operation(c == '*' ? CurveOp::Multiply : CurveOp::Divide, a, unary());
    }
  }
  std::size_t unary() {
    if (++depth > 64)
      fail("Curve expression exceeds 64 nested levels");
    struct Guard {
      std::size_t& depth;
      ~Guard() {
        --depth;
      }
    } guard{depth};
    if (consume('+'))
      return unary();
    if (consume('-'))
      return operation(CurveOp::Negate, unary());
    auto a = atom();
    if (consume('^')) {
      int sign = 1;
      if (consume('-'))
        sign = -1;
      else
        (void)consume('+');
      whitespace();
      const auto start = position;
      while (position < source.size() && digit(source[position]))
        ++position;
      int exponent{};
      const auto parsed =
          std::from_chars(source.data() + start, source.data() + position, exponent);
      if (parsed.ec != std::errc{} || parsed.ptr != source.data() + position || start == position ||
          exponent > 8)
        fail("Curve power requires an integer literal from -8 to 8");
      a = operation(CurveOp::Power, a, 0, sign * exponent);
    }
    return a;
  }
  std::size_t atom() {
    if (consume('(')) {
      auto a = expression();
      if (!consume(')'))
        fail("Curve expression is missing ')'");
      return a;
    }
    const char c = peek();
    if (digit(c) || c == '.') {
      double number{};
      const auto parsed =
          std::from_chars(source.data() + position, source.data() + source.size(), number);
      if (parsed.ec != std::errc{} || parsed.ptr == source.data() + position ||
          !std::isfinite(number))
        fail("Invalid curve numeric literal");
      position = static_cast<std::size_t>(parsed.ptr - source.data());
      whitespace();
      const auto start = position;
      while (position < source.size() && letter(source[position]))
        ++position;
      const auto suffix = source.substr(start, position - start);
      if (suffix == "mm")
        return constant(number, {1, 0});
      if (suffix == "rad")
        return constant(number, {0, 1});
      if (suffix == "deg")
        return constant(number * (std::numbers::pi / 180), {0, 1}, true, number, 180);
      position = start;
      return constant(number);
    }
    if (consume('@')) {
      if (!consume('{'))
        fail("Curve parameter reference requires @{ParameterId}");
      const auto start = position;
      while (position < source.size() && source[position] != '}') {
        if (source[position] == '{' || static_cast<unsigned char>(source[position]) < 32)
          fail("Invalid curve parameter ID token");
        ++position;
      }
      if (position == source.size() || position == start || position - start > 256)
        fail("Invalid curve parameter ID token");
      const std::string id(source.substr(start, position - start));
      ++position;
      CurveExpressionNode node;
      node.op = CurveOp::Parameter;
      node.parameter = id;
      if (document) {
        const auto* p = document->find_parameter(ParameterId(id));
        if (!p)
          fail("Unknown curve parameter '" + id + "'");
        node.unit = parameter_unit(*p);
      } else
        node.unknown_unit = true;
      const ParameterId reference(id);
      if (std::find(data.dependencies.begin(), data.dependencies.end(), reference) ==
          data.dependencies.end())
        data.dependencies.push_back(reference);
      return append(std::move(node));
    }
    if (letter(c)) {
      const auto start = position++;
      while (position < source.size() && (letter(source[position]) || digit(source[position])))
        ++position;
      const auto name = source.substr(start, position - start);
      if (consume('(')) {
        const auto op = function(name);
        if (!op)
          fail("Unknown curve function '" + std::string(name) + "'");
        const auto a = expression();
        if (!consume(')'))
          fail("Curve function requires one argument and ')'");
        return operation(*op, a);
      }
      if (name == "pi")
        return constant(std::numbers::pi, {}, true);
      if (name == "e")
        return constant(std::numbers::e);
      if (variable && name == variable->name) {
        CurveExpressionNode node;
        node.op = CurveOp::Variable;
        node.variable = true;
        node.unit = CurveExpressionUnit::from(variable->unit);
        return append(std::move(node));
      }
      fail("Unknown curve symbol '" + std::string(name) + "'; parameters require @{ParameterId}");
    }
    fail("Unexpected curve expression character");
  }
  CurveExpressionData parse() {
    if (source.empty() || source.size() > 4096)
      fail("Curve expression must contain 1..4096 bytes");
    if (variable) {
      if (variable->unit != CurveUnit::Scalar && variable->unit != CurveUnit::Length &&
          variable->unit != CurveUnit::Angle)
        fail("Unknown curve variable unit");
      const auto& name = variable->name;
      if (name.empty() || name.size() > 32 || !letter(name.front()) ||
          !std::all_of(name.begin(), name.end(), [](char c) { return letter(c) || digit(c); }) ||
          name == "pi" || name == "e" || function(name))
        fail("Invalid or reserved curve independent-variable name");
    }
    (void)expression();
    whitespace();
    if (position != source.size())
      fail("Trailing curve expression content");
    return std::move(data);
  }
};
CurveExpressionData parse(std::string_view source, std::optional<CurveVariable> variable,
                          const PartDocument* document, std::string object_id) {
  CurveExpressionData data;
  data.object_id = std::move(object_id);
  if (variable)
    data.variable_unit = CurveExpressionUnit::from(variable->unit);
  if (document)
    data.document = document->id();
  return Parser{source, std::move(variable), document, std::move(data)}.parse();
}
CurveExpressionJet product(CurveExpressionJet a, CurveExpressionJet b) {
  return {a.value * b.value, a.first * b.value + a.value * b.first,
          a.second * b.value + 2 * a.first * b.first + a.value * b.second};
}
} // namespace

Result<CurveExpression> CurveExpression::compile(const PartDocument& document,
                                                 std::string_view source,
                                                 std::optional<CurveVariable> variable,
                                                 std::string object_id) {
  try {
    auto data = parse(source, std::move(variable), &document, std::move(object_id));
    return Result<CurveExpression>::success(
        CurveExpression(std::make_shared<const CurveExpressionData>(std::move(data))));
  } catch (const Error& error) {
    return Result<CurveExpression>::failure(error);
  }
}
Result<std::vector<ParameterId>> CurveExpression::references(std::string_view source,
                                                             std::optional<CurveVariable> variable,
                                                             std::string object_id) {
  try {
    return Result<std::vector<ParameterId>>::success(
        parse(source, std::move(variable), nullptr, std::move(object_id)).dependencies);
  } catch (const Error& error) {
    return Result<std::vector<ParameterId>>::failure(error);
  }
}
CurveExpressionUnit CurveExpression::unit() const noexcept {
  return data_->nodes.back().unit;
}
const std::vector<ParameterId>& CurveExpression::parameter_dependencies() const noexcept {
  return data_->dependencies;
}
Result<CurveExpressionJet> CurveExpression::evaluate(const PartDocument& document,
                                                     double parameter) const {
  const auto fail = [&](std::string message) {
    throw Error::validation(data_->object_id, std::move(message));
  };
  try {
    if (data_->document != document.id())
      fail("Curve expression belongs to another Part");
    if (!std::isfinite(parameter))
      fail("Curve independent variable must be finite");
    std::vector<CurveExpressionJet> values;
    values.reserve(data_->nodes.size());
    for (const auto& node : data_->nodes) {
      CurveExpressionJet result{};
      if (node.op == CurveOp::Constant)
        result.value = node.number;
      else if (node.op == CurveOp::Variable)
        result = {parameter, 1, 0};
      else if (node.op == CurveOp::Parameter) {
        const auto* p = document.find_parameter(ParameterId(node.parameter));
        if (!p)
          fail("Curve parameter no longer exists: " + node.parameter);
        if (parameter_unit(*p) != node.unit)
          fail("Curve parameter unit changed: " + node.parameter);
        if (p->type() == ParameterType::Length)
          result.value = p->value().millimeters();
        else if (p->type() == ParameterType::Angle)
          result.value = p->value().degrees() * (std::numbers::pi / 180);
        else {
          if (p->value().count_value() > 9007199254740992ULL)
            fail("Curve Count reference exceeds exact numeric range");
          result.value = static_cast<double>(p->value().count_value());
        }
      } else {
        const auto a = values[node.a], b = values[node.b];
        switch (node.op) {
        case CurveOp::Add:
          result = {a.value + b.value, a.first + b.first, a.second + b.second};
          break;
        case CurveOp::Subtract:
          result = {a.value - b.value, a.first - b.first, a.second - b.second};
          break;
        case CurveOp::Negate:
          result = {-a.value, -a.first, -a.second};
          break;
        case CurveOp::Multiply:
          result = product(a, b);
          break;
        case CurveOp::Divide: {
          if (b.value == 0)
            fail("Curve expression division by zero");
          const double inverse = 1 / b.value;
          result = product(a, {inverse, -b.first * inverse * inverse,
                               2 * b.first * b.first * inverse * inverse * inverse -
                                   b.second * inverse * inverse});
          break;
        }
        case CurveOp::Power: {
          const int n = node.exponent;
          if (n == 0) {
            result.value = 1;
            break;
          }
          if (n == 1) {
            result = a;
            break;
          }
          if (n < 0 && a.value == 0)
            fail("Curve negative power of zero");
          const double first = n * std::pow(a.value, n - 1);
          const double second = n * (n - 1) * std::pow(a.value, n - 2);
          result = {std::pow(a.value, n), first * a.first,
                    second * a.first * a.first + first * a.second};
          break;
        }
        default: {
          double v{}, first{}, second{};
          switch (node.op) {
          case CurveOp::Sin:
            v = std::sin(a.value);
            first = std::cos(a.value);
            second = -v;
            break;
          case CurveOp::Cos:
            v = std::cos(a.value);
            first = -std::sin(a.value);
            second = -v;
            break;
          case CurveOp::Tan: {
            const double cosine = std::cos(a.value);
            if (std::abs(cosine) < 1e-14)
              fail("Curve tan has an unsupported pole");
            v = std::tan(a.value);
            first = 1 / (cosine * cosine);
            second = 2 * v * first;
            break;
          }
          case CurveOp::Exp:
            v = std::exp(a.value);
            first = v;
            second = v;
            break;
          case CurveOp::Log:
            if (a.value <= 0)
              fail("Curve log requires a positive argument");
            v = std::log(a.value);
            first = 1 / a.value;
            second = -first * first;
            break;
          case CurveOp::Sqrt:
            if (a.value < 0 || (a.value == 0 && data_->nodes[node.a].variable))
              fail("Curve sqrt has an unsupported singularity");
            v = std::sqrt(a.value);
            if (v != 0) {
              first = .5 / v;
              second = -.25 / (a.value * v);
            }
            break;
          case CurveOp::Abs:
            if (a.value == 0 && data_->nodes[node.a].variable)
              fail("Curve abs has an unsupported singular derivative");
            v = std::abs(a.value);
            first = a.value < 0 ? -1 : 1;
            break;
          default:
            fail("Invalid compiled curve operation");
          }
          result = {v, first * a.first, second * a.first * a.first + first * a.second};
          break;
        }
        }
      }
      if (!std::isfinite(result.value) || !std::isfinite(result.first) ||
          !std::isfinite(result.second))
        fail("Curve expression exceeds finite numeric range");
      values.push_back(result);
    }
    return Result<CurveExpressionJet>::success(values.back());
  } catch (const Error& error) {
    return Result<CurveExpressionJet>::failure(error);
  }
}

} // namespace blcad
