#include "blcad/core/part_document.hpp"
#include "curve_expression_internal.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>
#include <utility>

namespace blcad {
namespace {
using curve_expression_detail::CurveExpressionData;
using curve_expression_detail::CurveOp;
using curve_expression_detail::parameter_unit;
} // namespace
namespace {
using Interval = CurveInterval;
using Bounds = CurveExpressionBounds;
[[noreturn]] void interval_failure() {
  throw Error::validation("curve.interval",
                          "Equation interval is singular or outside finite numeric resolution");
}
Interval enclosure(long double lower, long double upper) {
  if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
    interval_failure();
  double a = static_cast<double>(lower), b = static_cast<double>(upper);
  if (static_cast<long double>(a) > lower)
    a = std::nextafter(a, -std::numeric_limits<double>::infinity());
  if (static_cast<long double>(b) < upper)
    b = std::nextafter(b, std::numeric_limits<double>::infinity());
  // One more representable step encloses arithmetic rounding; retain exact zero.
  if (a != 0)
    a = std::nextafter(a, -std::numeric_limits<double>::infinity());
  if (b != 0)
    b = std::nextafter(b, std::numeric_limits<double>::infinity());
  if (!std::isfinite(a) || !std::isfinite(b))
    interval_failure();
  return {a, b};
}
Interval add(Interval a, Interval b) {
  return enclosure(static_cast<long double>(a.lower) + b.lower,
                   static_cast<long double>(a.upper) + b.upper);
}
Interval negate(Interval a) {
  return {-a.upper, -a.lower};
}
Interval multiply(Interval a, Interval b) {
  if ((a.lower == 0 && a.upper == 0) || (b.lower == 0 && b.upper == 0))
    return {};
  std::array<long double, 4> values{
      static_cast<long double>(a.lower) * b.lower, static_cast<long double>(a.lower) * b.upper,
      static_cast<long double>(a.upper) * b.lower, static_cast<long double>(a.upper) * b.upper};
  return enclosure(*std::min_element(values.begin(), values.end()),
                   *std::max_element(values.begin(), values.end()));
}
Interval scale(Interval a, double b) {
  return multiply(a, {b, b});
}
Interval inverse(Interval a) {
  if (a.lower <= 0 && a.upper >= 0)
    interval_failure();
  return enclosure(1.L / a.upper, 1.L / a.lower);
}
Interval power(Interval a, int n) {
  if (n == 0)
    return {1, 1};
  if (n < 0)
    return inverse(power(a, -n));
  const long double first = std::pow(static_cast<long double>(a.lower), n),
                    last = std::pow(static_cast<long double>(a.upper), n);
  return enclosure(n % 2 == 0 && a.lower <= 0 && a.upper >= 0 ? 0 : std::min(first, last),
                   std::max(first, last));
}
Interval trig(Interval a, bool cosine) {
  if (std::max(std::abs(a.lower), std::abs(a.upper)) > 1e12)
    interval_failure();
  if (a.upper - a.lower >= 2 * std::numbers::pi)
    return {-1, 1};
  const auto fn = [&](long double x) { return cosine ? std::cos(x) : std::sin(x); };
  long double lo = std::min(fn(a.lower), fn(a.upper)), hi = std::max(fn(a.lower), fn(a.upper));
  const long double pi = std::numbers::pi_v<long double>;
  // Inflate critical-point detection for argument reduction and libm rounding.
  const long double guard = 64 * std::numeric_limits<double>::epsilon() *
                            std::max(1., std::max(std::abs(a.lower), std::abs(a.upper)));
  const long double offset = cosine ? 0 : pi / 2;
  if (std::ceil((a.lower - guard - offset) / (2 * pi)) <=
      std::floor((a.upper + guard - offset) / (2 * pi)))
    hi = 1;
  if (std::ceil((a.lower - guard - offset - pi) / (2 * pi)) <=
      std::floor((a.upper + guard - offset - pi) / (2 * pi)))
    lo = -1;
  constexpr long double error = 64 * std::numeric_limits<double>::epsilon();
  return enclosure(std::max(-1.L, lo - error), std::min(1.L, hi + error));
}
double magnitude(Interval a) {
  return std::max(std::abs(a.lower), std::abs(a.upper));
}
Bounds product(Bounds a, Bounds b) {
  return {multiply(a.value, b.value), add(multiply(a.first, b.value), multiply(a.value, b.first)),
          add(add(multiply(a.second, b.value), scale(multiply(a.first, b.first), 2)),
              multiply(a.value, b.second)),
          magnitude(a.value) * b.rounding_error + magnitude(b.value) * a.rounding_error};
}
Bounds compose(Bounds a, Interval value, Interval first, Interval second) {
  return {value, multiply(first, a.first),
          add(multiply(second, power(a.first, 2)), multiply(first, a.second)),
          magnitude(first) * a.rounding_error};
}
} // namespace
Result<CurveExpressionBounds> CurveExpression::bounds(const PartDocument& document, double lower,
                                                      double upper) const {
  try {
    if (data_->document != document.id() || !std::isfinite(lower) || !std::isfinite(upper) ||
        lower > upper)
      interval_failure();
    std::vector<Bounds> values;
    for (const auto& node : data_->nodes) {
      Bounds result{};
      if (node.op == CurveOp::Constant) {
        result.rounding_error = 64 * std::numeric_limits<double>::epsilon() * std::abs(node.number);
        result.value = enclosure(static_cast<long double>(node.number) - result.rounding_error,
                                 static_cast<long double>(node.number) + result.rounding_error);
      } else if (node.op == CurveOp::Variable)
        result = {{lower, upper}, {1, 1}, {}};
      else if (node.op == CurveOp::Parameter) {
        const auto* p = document.find_parameter(ParameterId(node.parameter));
        if (!p || parameter_unit(*p) != node.unit)
          interval_failure();
        double v = 0;
        if (p->type() == ParameterType::Length)
          v = p->value().millimeters();
        else if (p->type() == ParameterType::Angle)
          v = p->value().degrees() * (std::numbers::pi / 180);
        else {
          if (p->value().count_value() > 9007199254740992ULL)
            interval_failure();
          v = static_cast<double>(p->value().count_value());
        }
        result.value = {v, v};
        if (p->type() == ParameterType::Angle) {
          result.rounding_error = 64 * std::numeric_limits<double>::epsilon() * std::abs(v);
          result.value = enclosure(static_cast<long double>(v) - result.rounding_error,
                                   static_cast<long double>(v) + result.rounding_error);
        }
      } else {
        const auto a = values[node.a], b = values[node.b];
        switch (node.op) {
        case CurveOp::Add:
          result = {add(a.value, b.value), add(a.first, b.first), add(a.second, b.second),
                    a.rounding_error + b.rounding_error};
          break;
        case CurveOp::Subtract:
          result = {add(a.value, negate(b.value)), add(a.first, negate(b.first)),
                    add(a.second, negate(b.second)), a.rounding_error + b.rounding_error};
          break;
        case CurveOp::Negate:
          result = {negate(a.value), negate(a.first), negate(a.second), a.rounding_error};
          break;
        case CurveOp::Multiply:
          result = product(a, b);
          break;
        case CurveOp::Divide: {
          const auto inv = inverse(b.value);
          result = product(a, compose(b, inv, negate(power(inv, 2)), scale(power(inv, 3), 2)));
          break;
        }
        case CurveOp::Power:
          if (node.exponent == 0)
            result.value = {1, 1};
          else if (node.exponent == 1)
            result = a;
          else
            result = compose(
                a, power(a.value, node.exponent),
                scale(power(a.value, node.exponent - 1), node.exponent),
                scale(power(a.value, node.exponent - 2), node.exponent * (node.exponent - 1)));
          break;
        case CurveOp::Sin:
          result =
              compose(a, trig(a.value, false), trig(a.value, true), negate(trig(a.value, false)));
          break;
        case CurveOp::Cos:
          result = compose(a, trig(a.value, true), negate(trig(a.value, false)),
                           negate(trig(a.value, true)));
          break;
        case CurveOp::Tan: {
          const auto sine = trig(a.value, false), cosine = trig(a.value, true);
          const auto tan = multiply(sine, inverse(cosine)), sec2 = power(inverse(cosine), 2);
          result = compose(a, tan, sec2, scale(multiply(tan, sec2), 2));
          break;
        }
        case CurveOp::Exp: {
          const auto v = enclosure(std::exp(static_cast<long double>(a.value.lower)),
                                   std::exp(static_cast<long double>(a.value.upper)));
          result = compose(a, v, v, v);
          break;
        }
        case CurveOp::Log:
          if (a.value.lower <= 0)
            interval_failure();
          result = compose(a,
                           enclosure(std::log(static_cast<long double>(a.value.lower)),
                                     std::log(static_cast<long double>(a.value.upper))),
                           inverse(a.value), negate(power(inverse(a.value), 2)));
          break;
        case CurveOp::Sqrt: {
          if (a.value.lower < 0 || (a.value.lower == 0 && data_->nodes[node.a].variable))
            interval_failure();
          if (a.value.lower == 0 && a.value.upper == 0)
            break;
          const auto v = enclosure(std::sqrt(static_cast<long double>(a.value.lower)),
                                   std::sqrt(static_cast<long double>(a.value.upper)));
          result = compose(a, v, scale(inverse(v), .5), scale(inverse(multiply(a.value, v)), -.25));
          break;
        }
        case CurveOp::Abs:
          if (a.value.lower <= 0 && a.value.upper >= 0 && data_->nodes[node.a].variable)
            interval_failure();
          result = a.value.upper < 0 ? Bounds{negate(a.value), negate(a.first), negate(a.second),
                                              a.rounding_error}
                                     : a;
          break;
        default:
          interval_failure();
        }
        if (node.op != CurveOp::Negate && node.op != CurveOp::Abs) {
          const bool elementary = node.op == CurveOp::Add || node.op == CurveOp::Subtract ||
                                  node.op == CurveOp::Multiply || node.op == CurveOp::Divide;
          const double factor = elementary ? 4 : 64;
          const double scale =
              elementary ? magnitude(result.value) : std::max(1., magnitude(result.value));
          result.rounding_error += factor * std::numeric_limits<double>::epsilon() * scale +
                                   std::numeric_limits<double>::denorm_min();
        }
      }
      if (!std::isfinite(result.rounding_error))
        interval_failure();
      values.push_back(result);
    }
    return Result<CurveExpressionBounds>::success(values.back());
  } catch (const Error& error) {
    return Result<CurveExpressionBounds>::failure(
        Error::validation(data_->object_id, error.message()));
  }
}

namespace {
bool exact_zero(Interval a) {
  return a.lower == 0 && a.upper == 0;
}
bool no_harmonics(const CurveAnalyticForm& a) {
  return exact_zero(a.cosine_bounds) && exact_zero(a.sine_bounds);
}
bool constant_form(const CurveAnalyticForm& a) {
  return no_harmonics(a) && exact_zero(a.polynomial_bounds[1]) &&
         exact_zero(a.polynomial_bounds[2]) && exact_zero(a.polynomial_bounds[3]);
}
CurveAnalyticForm scaled_form(CurveAnalyticForm a, double factor,
                              std::optional<Interval> enclosure_factor = std::nullopt) {
  const auto range = enclosure_factor.value_or(Interval{factor, factor});
  for (std::size_t i = 0; i < 4; ++i) {
    a.polynomial[i] *= factor;
    a.polynomial_bounds[i] = multiply(a.polynomial_bounds[i], range);
  }
  a.cosine *= factor;
  a.sine *= factor;
  a.cosine_bounds = multiply(a.cosine_bounds, range);
  a.sine_bounds = multiply(a.sine_bounds, range);
  a.frequency_sensitivity *= magnitude(range);
  return a;
}
std::optional<CurveAnalyticForm> sum_form(CurveAnalyticForm a, CurveAnalyticForm b) {
  if (no_harmonics(a)) {
    a.frequency = b.frequency;
    a.frequency_bounds = b.frequency_bounds;
  }
  if (no_harmonics(b)) {
    b.frequency = a.frequency;
    b.frequency_bounds = a.frequency_bounds;
  }
  if (a.frequency != b.frequency)
    return std::nullopt;
  for (std::size_t i = 0; i < 4; ++i) {
    a.polynomial[i] += b.polynomial[i];
    a.polynomial_bounds[i] = add(a.polynomial_bounds[i], b.polynomial_bounds[i]);
  }
  a.cosine += b.cosine;
  a.sine += b.sine;
  a.cosine_bounds = add(a.cosine_bounds, b.cosine_bounds);
  a.sine_bounds = add(a.sine_bounds, b.sine_bounds);
  a.frequency_bounds = {std::min(a.frequency_bounds.lower, b.frequency_bounds.lower),
                        std::max(a.frequency_bounds.upper, b.frequency_bounds.upper)};
  a.frequency_sensitivity += b.frequency_sensitivity;
  return a;
}
std::optional<CurveAnalyticForm> multiply_form(CurveAnalyticForm a, CurveAnalyticForm b) {
  if (constant_form(a))
    return scaled_form(b, a.polynomial[0], a.polynomial_bounds[0]);
  if (constant_form(b))
    return scaled_form(a, b.polynomial[0], b.polynomial_bounds[0]);
  if (!no_harmonics(a) || !no_harmonics(b))
    return std::nullopt;
  CurveAnalyticForm result;
  for (std::size_t i = 0; i < 4; ++i)
    for (std::size_t j = 0; j < 4; ++j) {
      if (i + j >= 4 && !exact_zero(a.polynomial_bounds[i]) && !exact_zero(b.polynomial_bounds[j]))
        return std::nullopt;
      if (i + j < 4) {
        result.polynomial[i + j] += a.polynomial[i] * b.polynomial[j];
        result.polynomial_bounds[i + j] =
            add(result.polynomial_bounds[i + j],
                multiply(a.polynomial_bounds[i], b.polynomial_bounds[j]));
      }
    }
  return result;
}
Interval constant_function_range(CurveOp op, Interval a) {
  switch (op) {
  case CurveOp::Tan:
    return multiply(trig(a, false), inverse(trig(a, true)));
  case CurveOp::Sqrt:
    if (a.lower < 0)
      interval_failure();
    return enclosure(std::sqrt(static_cast<long double>(a.lower)),
                     std::sqrt(static_cast<long double>(a.upper)));
  case CurveOp::Exp:
    return enclosure(std::exp(static_cast<long double>(a.lower)),
                     std::exp(static_cast<long double>(a.upper)));
  case CurveOp::Log:
    if (a.lower <= 0)
      interval_failure();
    return enclosure(std::log(static_cast<long double>(a.lower)),
                     std::log(static_cast<long double>(a.upper)));
  case CurveOp::Abs:
    return enclosure(a.lower <= 0 && a.upper >= 0 ? 0
                                                  : std::min(std::abs(a.lower), std::abs(a.upper)),
                     magnitude(a));
  default:
    interval_failure();
  }
}
} // namespace
Result<std::optional<CurveAnalyticForm>>
CurveExpression::analytic_form(const PartDocument& document) const {
  using Form = std::optional<CurveAnalyticForm>;
  if (data_->document != document.id())
    return Result<Form>::failure(
        Error::validation(data_->object_id, "Equation belongs to another Part"));
  std::vector<Form> values;
  for (const auto& node : data_->nodes) {
    Form result;
    try {
      CurveAnalyticForm constant;
      if (node.op == CurveOp::Constant) {
        constant.polynomial[0] = node.number;
        const double error = 64 * std::numeric_limits<double>::epsilon() * std::abs(node.number);
        constant.polynomial_bounds[0] = enclosure(static_cast<long double>(node.number) - error,
                                                  static_cast<long double>(node.number) + error);
        result = constant;
      } else if (node.op == CurveOp::Variable) {
        constant.polynomial[1] = 1;
        constant.polynomial_bounds[1] = {1, 1};
        result = constant;
      } else if (node.op == CurveOp::Parameter) {
        const auto* p = document.find_parameter(ParameterId(node.parameter));
        if (!p || parameter_unit(*p) != node.unit ||
            (p->type() == ParameterType::Count && p->value().count_value() > 9007199254740992ULL))
          return Result<Form>::failure(
              Error::validation(data_->object_id, "Equation parameter lost or changed type/range"));
        const double v = p->type() == ParameterType::Length ? p->value().millimeters()
                         : p->type() == ParameterType::Angle
                             ? p->value().degrees() * (std::numbers::pi / 180)
                             : static_cast<double>(p->value().count_value());
        constant.polynomial[0] = v;
        const double error = p->type() == ParameterType::Angle
                                 ? 64 * std::numeric_limits<double>::epsilon() * std::abs(v)
                                 : 0;
        constant.polynomial_bounds[0] =
            enclosure(static_cast<long double>(v) - error, static_cast<long double>(v) + error);
        result = constant;
      } else {
        const auto a = values[node.a], b = values[node.b];
        if (a)
          switch (node.op) {
          case CurveOp::Negate:
            result = scaled_form(*a, -1);
            break;
          case CurveOp::Add:
            if (b)
              result = sum_form(*a, *b);
            break;
          case CurveOp::Subtract:
            if (b)
              result = sum_form(*a, scaled_form(*b, -1));
            break;
          case CurveOp::Multiply:
            if (b)
              result = multiply_form(*a, *b);
            break;
          case CurveOp::Divide:
            if (b && constant_form(*b) && b->polynomial[0] != 0)
              result = scaled_form(*a, 1 / b->polynomial[0], inverse(b->polynomial_bounds[0]));
            break;
          case CurveOp::Power:
            if (node.exponent >= 0) {
              constant.polynomial[0] = 1;
              constant.polynomial_bounds[0] = {1, 1};
              result = constant;
              for (int i = 0; i < node.exponent && result; ++i)
                result = multiply_form(*result, *a);
            } else if (constant_form(*a) && a->polynomial[0] != 0) {
              constant.polynomial[0] = std::pow(a->polynomial[0], node.exponent);
              constant.polynomial_bounds[0] = power(a->polynomial_bounds[0], node.exponent);
              result = constant;
            }
            break;
          case CurveOp::Sin:
          case CurveOp::Cos:
            if (no_harmonics(*a) && exact_zero(a->polynomial_bounds[2]) &&
                exact_zero(a->polynomial_bounds[3])) {
              const double phase = a->polynomial[0], w = a->polynomial[1];
              if (w == 0) {
                if (!exact_zero(a->polynomial_bounds[1]))
                  break;
                constant.polynomial[0] =
                    node.op == CurveOp::Sin ? std::sin(phase) : std::cos(phase);
                constant.polynomial_bounds[0] =
                    trig(a->polynomial_bounds[0], node.op == CurveOp::Cos);
              } else {
                constant.frequency = w;
                constant.frequency_bounds = a->polynomial_bounds[1];
                constant.cosine = node.op == CurveOp::Sin ? std::sin(phase) : std::cos(phase);
                constant.sine = node.op == CurveOp::Sin ? std::cos(phase) : -std::sin(phase);
                constant.cosine_bounds = trig(a->polynomial_bounds[0], node.op == CurveOp::Cos);
                constant.sine_bounds = node.op == CurveOp::Sin
                                           ? trig(a->polynomial_bounds[0], true)
                                           : negate(trig(a->polynomial_bounds[0], false));
                constant.frequency_sensitivity =
                    magnitude(constant.cosine_bounds) + magnitude(constant.sine_bounds);
              }
              result = constant;
            }
            break;
          default:
            if (constant_form(*a)) {
              const double v = a->polynomial[0];
              double out = std::numeric_limits<double>::quiet_NaN();
              if (node.op == CurveOp::Tan && std::abs(std::cos(v)) >= 1e-14)
                out = std::tan(v);
              if (node.op == CurveOp::Sqrt && v >= 0)
                out = std::sqrt(v);
              if (node.op == CurveOp::Exp)
                out = std::exp(v);
              if (node.op == CurveOp::Log && v > 0)
                out = std::log(v);
              if (node.op == CurveOp::Abs)
                out = std::abs(v);
              if (std::isfinite(out)) {
                constant.polynomial[0] = out;
                constant.polynomial_bounds[0] =
                    constant_function_range(node.op, a->polynomial_bounds[0]);
                result = constant;
              }
            }
            break;
          }
      }
    } catch (const Error&) {
      result.reset();
    }
    if (result && result->frequency < 0) {
      result->frequency = -result->frequency;
      result->frequency_bounds = negate(result->frequency_bounds);
      result->sine = -result->sine;
      result->sine_bounds = negate(result->sine_bounds);
    }
    if (result) {
      bool finite = std::isfinite(result->frequency) && std::isfinite(result->cosine) &&
                    std::isfinite(result->sine) && std::isfinite(result->frequency_sensitivity);
      for (double p : result->polynomial)
        finite = finite && std::isfinite(p);
      if (!finite)
        result.reset();
    }
    values.push_back(result);
  }
  return Result<Form>::success(values.back());
}

namespace {
// Bounded exact rational arithmetic proves symbolic multiples of pi. Overflow
// makes a proof unavailable; it never falls back to endpoint proximity.
struct SymbolicUnavailable {};
struct ExactRational {
  std::int64_t numerator{}, denominator{1};
  ExactRational() = default;
  ExactRational(std::int64_t n, std::int64_t d = 1) : numerator(n), denominator(d) {
    if (d <= 0 || n == std::numeric_limits<std::int64_t>::min())
      throw SymbolicUnavailable{};
    const auto divisor = std::gcd(n, d);
    numerator /= divisor;
    denominator /= divisor;
  }
  static std::int64_t product(std::int64_t a, std::int64_t b) {
    if (a && b && std::abs(a) > std::numeric_limits<std::int64_t>::max() / std::abs(b))
      throw SymbolicUnavailable{};
    return a * b;
  }
  static ExactRational from_double(double value) {
    if (!std::isfinite(value))
      throw SymbolicUnavailable{};
    if (value == 0)
      return {};
    int exponent{};
    const double fraction = std::frexp(value, &exponent);
    auto n = static_cast<std::int64_t>(std::ldexp(fraction, 53));
    exponent -= 53;
    while (exponent < 0 && n % 2 == 0) {
      n /= 2;
      ++exponent;
    }
    if (exponent >= 0) {
      if (exponent >= 63)
        throw SymbolicUnavailable{};
      return ExactRational(product(n, std::int64_t{1} << exponent));
    }
    if (exponent < -62)
      throw SymbolicUnavailable{};
    return ExactRational(n, std::int64_t{1} << -exponent);
  }
  bool zero() const {
    return numerator == 0;
  }
  friend ExactRational operator-(ExactRational a) {
    return {-a.numerator, a.denominator};
  }
  friend ExactRational operator+(ExactRational a, ExactRational b) {
    const auto divisor = std::gcd(a.denominator, b.denominator);
    const auto first = product(a.numerator, b.denominator / divisor);
    const auto second = product(b.numerator, a.denominator / divisor);
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    if ((second > 0 && first > max - second) || (second < 0 && first < -max - second))
      throw SymbolicUnavailable{};
    return {first + second, product(a.denominator, b.denominator / divisor)};
  }
  friend ExactRational operator-(ExactRational a, ExactRational b) {
    return a + -b;
  }
  friend ExactRational operator*(ExactRational a, ExactRational b) {
    const auto first = std::gcd(a.numerator, b.denominator),
               second = std::gcd(b.numerator, a.denominator);
    return {product(a.numerator / first, b.numerator / second),
            product(a.denominator / second, b.denominator / first)};
  }
  friend ExactRational operator/(ExactRational a, ExactRational b) {
    if (b.zero())
      throw SymbolicUnavailable{};
    return a *
           ExactRational(b.numerator < 0 ? -b.denominator : b.denominator, std::abs(b.numerator));
  }
};
struct ExactPi {
  ExactRational constant, pi;
  bool zero() const {
    return constant.zero() && pi.zero();
  }
  friend ExactPi operator-(ExactPi a) {
    return {-a.constant, -a.pi};
  }
  friend ExactPi operator+(ExactPi a, ExactPi b) {
    return {a.constant + b.constant, a.pi + b.pi};
  }
  friend ExactPi operator-(ExactPi a, ExactPi b) {
    return a + -b;
  }
  friend ExactPi operator*(ExactPi a, ExactPi b) {
    if (!a.pi.zero() && !b.pi.zero())
      throw SymbolicUnavailable{};
    return {a.constant * b.constant, a.pi * b.constant + a.constant * b.pi};
  }
  friend ExactPi operator/(ExactPi a, ExactPi b) {
    if (!b.pi.zero()) {
      if (!b.constant.zero() || !a.constant.zero())
        throw SymbolicUnavailable{};
      return {a.pi / b.pi, {}};
    }
    return {a.constant / b.constant, a.pi / b.constant};
  }
};
struct ExactAffine {
  ExactPi constant, slope;
  friend ExactAffine operator-(ExactAffine a) {
    return {-a.constant, -a.slope};
  }
  friend ExactAffine operator+(ExactAffine a, ExactAffine b) {
    return {a.constant + b.constant, a.slope + b.slope};
  }
  friend ExactAffine operator-(ExactAffine a, ExactAffine b) {
    return a + -b;
  }
  friend ExactAffine operator*(ExactAffine a, ExactAffine b) {
    if (!a.slope.zero() && !b.slope.zero())
      throw SymbolicUnavailable{};
    return {a.constant * b.constant, a.slope * b.constant + a.constant * b.slope};
  }
  friend ExactAffine operator/(ExactAffine a, ExactAffine b) {
    if (!b.slope.zero())
      throw SymbolicUnavailable{};
    return {a.constant / b.constant, a.slope / b.constant};
  }
};
std::vector<std::optional<ExactAffine>> symbolic_affine(const CurveExpressionData& data,
                                                        const PartDocument& document) {
  std::vector<std::optional<ExactAffine>> values;
  for (const auto& node : data.nodes) {
    std::optional<ExactAffine> value;
    try {
      ExactAffine constant;
      if (node.op == CurveOp::Constant) {
        if (node.pi_symbol)
          constant.constant.pi =
              ExactRational::from_double(node.pi_scale) / ExactRational(node.pi_divisor);
        else
          constant.constant.constant = ExactRational::from_double(node.number);
        value = constant;
      } else if (node.op == CurveOp::Variable) {
        constant.slope.constant = ExactRational(1);
        value = constant;
      } else if (node.op == CurveOp::Parameter) {
        const auto* p = document.find_parameter(ParameterId(node.parameter));
        if (!p || parameter_unit(*p) != node.unit)
          throw SymbolicUnavailable{};
        if (p->type() == ParameterType::Angle)
          constant.constant.pi =
              ExactRational::from_double(p->value().degrees()) / ExactRational(180);
        else
          constant.constant.constant = ExactRational::from_double(
              p->type() == ParameterType::Length ? p->value().millimeters()
                                                 : static_cast<double>(p->value().count_value()));
        value = constant;
      } else {
        const auto& a = values[node.a];
        const auto& b = values[node.b];
        if (a)
          switch (node.op) {
          case CurveOp::Negate:
            value = -*a;
            break;
          case CurveOp::Add:
            if (b)
              value = *a + *b;
            break;
          case CurveOp::Subtract:
            if (b)
              value = *a - *b;
            break;
          case CurveOp::Multiply:
            if (b)
              value = *a * *b;
            break;
          case CurveOp::Divide:
            if (b)
              value = *a / *b;
            break;
          case CurveOp::Power: {
            ExactAffine result;
            result.constant.constant = ExactRational(1);
            for (int i = 0; i < std::abs(node.exponent); ++i)
              result = result * *a;
            if (node.exponent < 0) {
              ExactAffine one;
              one.constant.constant = ExactRational(1);
              result = one / result;
            }
            value = result;
            break;
          }
          default:
            break;
          }
      }
    } catch (const SymbolicUnavailable&) {
      value.reset();
    }
    values.push_back(value);
  }
  return values;
}
} // namespace
Result<bool> CurveExpression::proves_period(const PartDocument& document,
                                            const CurveExpression& lower,
                                            const CurveExpression& upper) const {
  if (data_->document != document.id() || lower.data_->document != document.id() ||
      upper.data_->document != document.id() || lower.unit() != upper.unit())
    return Result<bool>::failure(Error::validation(
        data_->object_id, "Period bounds require the owning Part and matching units"));
  if (data_->variable_unit && *data_->variable_unit != lower.unit())
    return Result<bool>::failure(Error::validation(
        data_->object_id, "Period bounds do not match the independent-variable unit"));
  for (const auto* expression : {this, &lower, &upper}) {
    auto valid = expression->analytic_form(document);
    if (!valid)
      return Result<bool>::failure(valid.error());
  }
  const auto a = symbolic_affine(*lower.data_, document).back(),
             b = symbolic_affine(*upper.data_, document).back();
  if (!a || !b || !a->slope.zero() || !b->slope.zero())
    return Result<bool>::success(false);
  try {
    const auto span = b->constant - a->constant;
    const auto forms = symbolic_affine(*data_, document);
    std::vector<bool> periodic;
    for (const auto& node : data_->nodes) {
      bool value = !node.variable;
      if (node.op == CurveOp::Variable)
        value = false;
      else if (node.variable) {
        if (node.op == CurveOp::Sin || node.op == CurveOp::Cos || node.op == CurveOp::Tan) {
          value = periodic[node.a];
          if (!value && forms[node.a]) {
            try {
              const auto phase = forms[node.a]->slope * span;
              const auto turns = phase.pi / ExactRational(node.op == CurveOp::Tan ? 1 : 2);
              value = phase.constant.zero() && turns.denominator == 1 && turns.numerator != 0 &&
                      std::abs(turns.numerator) <= 1048576;
            } catch (const SymbolicUnavailable&) {
              value = false;
            }
          }
        } else if (node.op == CurveOp::Add || node.op == CurveOp::Subtract ||
                   node.op == CurveOp::Multiply || node.op == CurveOp::Divide)
          value = periodic[node.a] && periodic[node.b];
        else
          value = periodic[node.a];
      }
      periodic.push_back(value);
    }
    return Result<bool>::success(periodic.back());
  } catch (const SymbolicUnavailable&) {
    return Result<bool>::success(false);
  }
}

namespace {
using ExactPolynomial = std::array<ExactPi, 4>;
ExactPolynomial multiply_polynomial(const ExactPolynomial& a, const ExactPolynomial& b) {
  ExactPolynomial result;
  for (std::size_t i = 0; i < 4; ++i)
    for (std::size_t j = 0; j < 4; ++j) {
      if (a[i].zero() || b[j].zero())
        continue;
      if (i + j >= 4)
        throw SymbolicUnavailable{};
      result[i + j] = result[i + j] + a[i] * b[j];
    }
  return result;
}
std::optional<ExactPolynomial> symbolic_polynomial(const CurveExpressionData& data,
                                                   const PartDocument& document) {
  const auto affine = symbolic_affine(data, document);
  std::vector<std::optional<ExactPolynomial>> values;
  for (std::size_t index = 0; index < data.nodes.size(); ++index) {
    const auto& node = data.nodes[index];
    std::optional<ExactPolynomial> result;
    try {
      if (affine[index]) {
        ExactPolynomial p;
        p[0] = affine[index]->constant;
        p[1] = affine[index]->slope;
        result = p;
      } else if (node.op != CurveOp::Constant && node.op != CurveOp::Parameter &&
                 node.op != CurveOp::Variable) {
        const auto& a = values[node.a];
        const auto& b = values[node.b];
        if (a) {
          ExactPolynomial p;
          if (node.op == CurveOp::Add || node.op == CurveOp::Subtract) {
            if (b) {
              for (std::size_t i = 0; i < 4; ++i)
                p[i] = node.op == CurveOp::Add ? (*a)[i] + (*b)[i] : (*a)[i] - (*b)[i];
              result = p;
            }
          } else if (node.op == CurveOp::Negate) {
            for (std::size_t i = 0; i < 4; ++i)
              p[i] = -(*a)[i];
            result = p;
          } else if (node.op == CurveOp::Multiply && b)
            result = multiply_polynomial(*a, *b);
          else if (node.op == CurveOp::Divide && b && (*b)[1].zero() && (*b)[2].zero() &&
                   (*b)[3].zero()) {
            for (std::size_t i = 0; i < 4; ++i)
              p[i] = (*a)[i] / (*b)[0];
            result = p;
          } else if (node.op == CurveOp::Power && node.exponent >= 0) {
            p[0].constant = ExactRational(1);
            for (int i = 0; i < node.exponent; ++i)
              p = multiply_polynomial(p, *a);
            result = p;
          }
        }
      }
    } catch (const SymbolicUnavailable&) {
      result.reset();
    }
    values.push_back(result);
  }
  return values.back();
}
ExactPi evaluate_polynomial(const ExactPolynomial& p, ExactPi t) {
  auto value = p[3];
  for (int i = 2; i >= 0; --i)
    value = value * t + p[static_cast<std::size_t>(i)];
  return value;
}
} // namespace
Result<bool> CurveExpression::proves_endpoint_identity(const PartDocument& document,
                                                       const CurveExpression& lower,
                                                       const CurveExpression& upper) const {
  if (data_->document != document.id() || lower.data_->document != document.id() ||
      upper.data_->document != document.id() || lower.unit() != upper.unit() ||
      (data_->variable_unit && *data_->variable_unit != lower.unit()))
    return Result<bool>::failure(Error::validation(
        data_->object_id, "Closure bounds require the owning Part and independent-variable units"));
  for (const auto* expression : {this, &lower, &upper}) {
    auto valid = expression->analytic_form(document);
    if (!valid)
      return Result<bool>::failure(valid.error());
  }
  try {
    const auto a = symbolic_affine(*lower.data_, document).back(),
               b = symbolic_affine(*upper.data_, document).back();
    const auto p = symbolic_polynomial(*data_, document);
    if (!a || !b || !p || !a->slope.zero() || !b->slope.zero())
      return Result<bool>::success(false);
    return Result<bool>::success(
        (evaluate_polynomial(*p, a->constant) - evaluate_polynomial(*p, b->constant)).zero());
  } catch (const SymbolicUnavailable&) {
    return Result<bool>::success(false);
  }
}

} // namespace blcad
