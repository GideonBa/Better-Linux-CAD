#include "blcad/core/parameter_expression.hpp"

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace blcad {
namespace {

struct DimensionedValue {
  double magnitude = 0.0;
  int length_power = 0;
};

// Recursive-descent parser state over one formula string.
struct ExpressionParser {
  const PartDocument& document;
  std::string_view formula;
  std::string object_id;
  std::size_t position = 0U;
  std::vector<ParameterId> referenced_parameters;

  [[nodiscard]] Error error(std::string message) const {
    return Error::validation(object_id, std::move(message));
  }

  void skip_whitespace() noexcept {
    while (position < formula.size() &&
           std::isspace(static_cast<unsigned char>(formula[position])) != 0) {
      ++position;
    }
  }

  [[nodiscard]] bool consume(char expected) noexcept {
    skip_whitespace();
    if (position < formula.size() && formula[position] == expected) {
      ++position;
      return true;
    }
    return false;
  }

  [[nodiscard]] char peek() noexcept {
    skip_whitespace();
    return position < formula.size() ? formula[position] : '\0';
  }

  [[nodiscard]] Result<DimensionedValue> parse_expression() {
    auto left = parse_term();
    if (left.has_error()) {
      return left;
    }

    while (true) {
      const char operation = peek();
      if (operation != '+' && operation != '-') {
        return left;
      }
      ++position;

      auto right = parse_term();
      if (right.has_error()) {
        return right;
      }
      if (left.value().length_power != right.value().length_power) {
        return Result<DimensionedValue>::failure(
            error("expression addition and subtraction require matching units"));
      }
      left.value().magnitude = operation == '+' ? left.value().magnitude + right.value().magnitude
                                                : left.value().magnitude - right.value().magnitude;
    }
  }

  [[nodiscard]] Result<DimensionedValue> parse_term() {
    auto left = parse_factor();
    if (left.has_error()) {
      return left;
    }

    while (true) {
      const char operation = peek();
      if (operation != '*' && operation != '/') {
        return left;
      }
      ++position;

      auto right = parse_factor();
      if (right.has_error()) {
        return right;
      }

      if (operation == '*') {
        left.value().length_power += right.value().length_power;
        left.value().magnitude *= right.value().magnitude;
      } else {
        if (right.value().magnitude == 0.0) {
          return Result<DimensionedValue>::failure(error("expression division by zero"));
        }
        left.value().length_power -= right.value().length_power;
        left.value().magnitude /= right.value().magnitude;
      }
      if (left.value().length_power < 0 || left.value().length_power > 1) {
        return Result<DimensionedValue>::failure(
            error("expression produces an unsupported unit dimension"));
      }
    }
  }

  [[nodiscard]] Result<DimensionedValue> parse_factor() {
    skip_whitespace();
    if (consume('-')) {
      auto negated = parse_factor();
      if (negated.has_error()) {
        return negated;
      }
      negated.value().magnitude = -negated.value().magnitude;
      return negated;
    }
    if (consume('(')) {
      auto inner = parse_expression();
      if (inner.has_error()) {
        return inner;
      }
      if (!consume(')')) {
        return Result<DimensionedValue>::failure(error("expression is missing a closing ')'"));
      }
      return inner;
    }

    const char next = peek();
    if (std::isdigit(static_cast<unsigned char>(next)) != 0 || next == '.') {
      return parse_number();
    }
    if (std::isalpha(static_cast<unsigned char>(next)) != 0 || next == '_') {
      return parse_parameter_reference();
    }
    return Result<DimensionedValue>::failure(error("expression contains an unexpected character"));
  }

  [[nodiscard]] Result<DimensionedValue> parse_number() {
    const std::size_t start = position;
    while (position < formula.size() &&
           (std::isdigit(static_cast<unsigned char>(formula[position])) != 0 ||
            formula[position] == '.')) {
      ++position;
    }

    const std::string text(formula.substr(start, position - start));
    std::size_t parsed_length = 0U;
    double magnitude = 0.0;
    try {
      magnitude = std::stod(text, &parsed_length);
    } catch (...) {
      return Result<DimensionedValue>::failure(error("expression contains an invalid number"));
    }
    if (parsed_length != text.size() || !std::isfinite(magnitude)) {
      return Result<DimensionedValue>::failure(error("expression contains an invalid number"));
    }

    // Optional 'mm' unit suffix turns the literal into a length.
    const std::size_t before_unit = position;
    skip_whitespace();
    if (position + 1U < formula.size() + 1U && formula.substr(position, 2U) == "mm") {
      const std::size_t after = position + 2U;
      const bool boundary =
          after >= formula.size() ||
          (std::isalnum(static_cast<unsigned char>(formula[after])) == 0 && formula[after] != '_');
      if (boundary) {
        position = after;
        return Result<DimensionedValue>::success(DimensionedValue{magnitude, 1});
      }
    }
    position = before_unit;
    return Result<DimensionedValue>::success(DimensionedValue{magnitude, 0});
  }

  [[nodiscard]] Result<DimensionedValue> parse_parameter_reference() {
    const std::size_t start = position;
    while (position < formula.size() &&
           (std::isalnum(static_cast<unsigned char>(formula[position])) != 0 ||
            formula[position] == '_')) {
      ++position;
    }

    const std::string name(formula.substr(start, position - start));
    const Parameter* parameter = document.find_parameter(std::string_view(name));
    if (parameter == nullptr) {
      return Result<DimensionedValue>::failure(
          error("expression references unknown parameter '" + name + "'"));
    }

    if (std::find(referenced_parameters.begin(), referenced_parameters.end(), parameter->id()) ==
        referenced_parameters.end()) {
      referenced_parameters.push_back(parameter->id());
    }

    if (parameter->type() == ParameterType::Length) {
      return Result<DimensionedValue>::success(
          DimensionedValue{parameter->value().millimeters(), 1});
    }
    return Result<DimensionedValue>::success(
        DimensionedValue{static_cast<double>(parameter->value().count_value()), 0});
  }
};

} // namespace

Result<ParameterExpressionEvaluation>
ParameterExpressionEvaluator::evaluate(const PartDocument& document, std::string_view formula,
                                       ParameterType target_type,
                                       std::string_view object_id) const {
  ExpressionParser parser{document, formula, std::string(object_id), 0U, {}};

  auto value = parser.parse_expression();
  if (value.has_error()) {
    return Result<ParameterExpressionEvaluation>::failure(value.error());
  }
  parser.skip_whitespace();
  if (parser.position != formula.size()) {
    return Result<ParameterExpressionEvaluation>::failure(
        parser.error("expression has trailing content"));
  }

  const int required_power = target_type == ParameterType::Length ? 1 : 0;
  if (value.value().length_power != required_power) {
    return Result<ParameterExpressionEvaluation>::failure(
        parser.error("expression unit does not match the target parameter type"));
  }

  auto quantity = target_type == ParameterType::Length
                      ? Quantity::length_mm(value.value().magnitude, object_id)
                      : Quantity::count(value.value().magnitude, object_id);
  if (quantity.has_error()) {
    return Result<ParameterExpressionEvaluation>::failure(quantity.error());
  }

  return Result<ParameterExpressionEvaluation>::success(
      ParameterExpressionEvaluation{quantity.value(), std::move(parser.referenced_parameters)});
}

} // namespace blcad
