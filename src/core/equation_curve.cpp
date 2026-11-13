#include "blcad/core/equation_curve.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace blcad {
std::string_view to_string(CurveUnit unit) noexcept {
  switch (unit) {
  case CurveUnit::Scalar:
    return "scalar";
  case CurveUnit::Length:
    return "length";
  case CurveUnit::Angle:
    return "angle";
  }
  return "invalid";
}
std::string_view to_string(EquationCurveMode mode) noexcept {
  switch (mode) {
  case EquationCurveMode::Cartesian2D:
    return "cartesian_2d";
  case EquationCurveMode::Polar2D:
    return "polar_2d";
  case EquationCurveMode::Parametric2D:
    return "parametric_2d";
  case EquationCurveMode::Parametric3D:
    return "parametric_3d";
  }
  return "invalid";
}
std::string_view to_string(EquationCurveClosure closure) noexcept {
  switch (closure) {
  case EquationCurveClosure::Open:
    return "open";
  case EquationCurveClosure::Periodic:
    return "periodic";
  }
  return "invalid";
}
namespace {
std::array<bool, 3> used(EquationCurveMode mode) {
  switch (mode) {
  case EquationCurveMode::Cartesian2D:
    return {false, true, false};
  case EquationCurveMode::Polar2D:
    return {true, false, false};
  case EquationCurveMode::Parametric2D:
    return {true, true, false};
  case EquationCurveMode::Parametric3D:
    return {true, true, true};
  }
  return {};
}
} // namespace
Result<EquationCurve>
EquationCurve::create(SketchEntityId id, EquationCurveMode mode, CurveVariable variable,
                      std::string lower, std::string upper, std::array<std::string, 3> coordinates,
                      double tolerance_mm, EquationCurveClosure closure, bool construction) {
  const auto fail = [&](std::string message) {
    return Result<EquationCurve>::failure(Error::validation(id.value(), std::move(message)));
  };
  if (id.empty() || to_string(mode) == "invalid" || to_string(variable.unit) == "invalid" ||
      to_string(closure) == "invalid")
    return fail("Equation curve requires valid identity, mode, domain unit and closure");
  if (!std::isfinite(tolerance_mm) || tolerance_mm <= 0)
    return fail("Equation curve modeling tolerance must be positive and finite");
  if ((mode == EquationCurveMode::Cartesian2D && variable.unit != CurveUnit::Length) ||
      (mode == EquationCurveMode::Polar2D && variable.unit != CurveUnit::Angle))
    return fail("Equation curve mode has incompatible domain units");
  std::vector<ParameterId> dependencies;
  const auto references = [&](const std::string& source,
                              std::optional<CurveVariable> local) -> Result<std::size_t> {
    auto refs = CurveExpression::references(source, std::move(local), id.value());
    if (!refs)
      return Result<std::size_t>::failure(refs.error());
    for (const auto& ref : refs.value())
      if (std::find(dependencies.begin(), dependencies.end(), ref) == dependencies.end())
        dependencies.push_back(ref);
    return Result<std::size_t>::success(refs.value().size());
  };
  for (const auto& bound : {lower, upper}) {
    auto parsed = references(bound, std::nullopt);
    if (!parsed)
      return Result<EquationCurve>::failure(parsed.error());
  }
  const auto active = used(mode);
  for (std::size_t i = 0; i < 3; ++i) {
    if (!active[i]) {
      if (!coordinates[i].empty())
        return fail("Equation curve has coordinates outside its mode");
      continue;
    }
    auto parsed = references(coordinates[i], variable);
    if (!parsed)
      return Result<EquationCurve>::failure(parsed.error());
  }
  return Result<EquationCurve>::success(EquationCurve(
      std::move(id), mode, std::move(variable), std::move(lower), std::move(upper),
      std::move(coordinates), tolerance_mm, closure, construction, std::move(dependencies)));
}
Result<EquationCurveProgram> EquationCurveProgram::compile(const PartDocument& document,
                                                           const EquationCurve& curve) {
  const auto expected = CurveExpressionUnit::from(curve.variable().unit);
  auto lower = CurveExpression::compile(document, curve.lower(), std::nullopt,
                                        curve.id().value() + "/lower");
  if (!lower)
    return Result<EquationCurveProgram>::failure(lower.error());
  auto upper = CurveExpression::compile(document, curve.upper(), std::nullopt,
                                        curve.id().value() + "/upper");
  if (!upper)
    return Result<EquationCurveProgram>::failure(upper.error());
  if (lower.value().unit() != expected || upper.value().unit() != expected)
    return Result<EquationCurveProgram>::failure(
        Error::validation(curve.id().value(), "Equation domain bounds have incompatible units"));
  std::array<std::optional<CurveExpression>, 3> coordinates;
  const auto active = used(curve.mode());
  for (std::size_t i = 0; i < 3; ++i) {
    if (!active[i])
      continue;
    auto expression =
        CurveExpression::compile(document, curve.coordinates()[i], curve.variable(),
                                 curve.id().value() + "/coordinate" + std::to_string(i));
    if (!expression)
      return Result<EquationCurveProgram>::failure(expression.error());
    if (expression.value().unit() != CurveExpressionUnit{1, 0})
      return Result<EquationCurveProgram>::failure(Error::validation(
          curve.id().value(), "Equation coordinates require signed real lengths"));
    coordinates[i] = std::move(expression.value());
  }
  EquationCurveProgram program(curve, std::move(lower.value()), std::move(upper.value()),
                               std::move(coordinates));
  auto bounds = program.domain(document);
  if (!bounds)
    return Result<EquationCurveProgram>::failure(bounds.error());
  return Result<EquationCurveProgram>::success(std::move(program));
}
Result<std::pair<double, double>> EquationCurveProgram::domain(const PartDocument& document) const {
  auto lower = lower_.evaluate(document);
  if (!lower)
    return Result<std::pair<double, double>>::failure(lower.error());
  auto upper = upper_.evaluate(document);
  if (!upper)
    return Result<std::pair<double, double>>::failure(upper.error());
  const double a = lower.value().value, b = upper.value().value;
  if (!(a < b) || !std::isfinite(b - a))
    return Result<std::pair<double, double>>::failure(
        Error::validation(curve_.id().value(), "Equation domain must have a finite positive span"));
  return Result<std::pair<double, double>>::success({a, b});
}
Result<EquationCurveJet> EquationCurveProgram::evaluate(const PartDocument& document,
                                                        double parameter) const {
  auto bounds = domain(document);
  if (!bounds)
    return Result<EquationCurveJet>::failure(bounds.error());
  if (!std::isfinite(parameter) || parameter < bounds.value().first ||
      parameter > bounds.value().second)
    return Result<EquationCurveJet>::failure(Error::validation(
        curve_.id().value(), "Equation parameter lies outside its finite domain"));
  std::array<CurveExpressionJet, 3> coordinates{};
  for (std::size_t i = 0; i < 3; ++i)
    if (coordinates_[i]) {
      auto value = coordinates_[i]->evaluate(document, parameter);
      if (!value)
        return Result<EquationCurveJet>::failure(value.error());
      coordinates[i] = value.value();
    }
  EquationCurveJet result{{coordinates[0].value, coordinates[1].value, coordinates[2].value},
                          {coordinates[0].first, coordinates[1].first, coordinates[2].first},
                          {coordinates[0].second, coordinates[1].second, coordinates[2].second}};
  if (curve_.mode() == EquationCurveMode::Cartesian2D) {
    result.value.x = parameter;
    result.first.x = 1;
  } else if (curve_.mode() == EquationCurveMode::Polar2D) {
    const auto r = coordinates[0];
    const double c = std::cos(parameter), s = std::sin(parameter);
    result = {{r.value * c, r.value * s, 0},
              {r.first * c - r.value * s, r.first * s + r.value * c, 0},
              {r.second * c - 2 * r.first * s - r.value * c,
               r.second * s + 2 * r.first * c - r.value * s, 0}};
  }
  for (const auto& p : {result.value, result.first, result.second})
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
      return Result<EquationCurveJet>::failure(Error::validation(
          curve_.id().value(), "Equation coordinate/derivative exceeds finite numeric range"));
  return Result<EquationCurveJet>::success(result);
}

Result<EquationCurveIntervalAnalysis>
EquationCurveProgram::analyze_interval(const PartDocument& document, double lower,
                                       double upper) const {
  const auto fail = [&](std::string text) {
    return Result<EquationCurveIntervalAnalysis>::failure(
        Error::validation(curve_.id().value(), std::move(text)));
  };
  auto domain_bounds = domain(document);
  if (!domain_bounds)
    return Result<EquationCurveIntervalAnalysis>::failure(domain_bounds.error());
  if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper ||
      lower < domain_bounds.value().first || upper > domain_bounds.value().second)
    return fail("Interval lies outside the equation domain");
  auto lower_bound = lower_.bounds(document, 0, 0), upper_bound = upper_.bounds(document, 0, 0);
  if (!lower_bound || !upper_bound)
    return Result<EquationCurveIntervalAnalysis>::failure(!lower_bound ? lower_bound.error()
                                                                       : upper_bound.error());
  const double domain_error =
      std::max(lower_bound.value().rounding_error, upper_bound.value().rounding_error);
  const auto maximum = [](CurveInterval a) {
    return std::max(std::abs(a.lower), std::abs(a.upper));
  };
  const auto minimum = [](CurveInterval a) {
    return a.lower <= 0 && a.upper >= 0 ? 0 : std::min(std::abs(a.lower), std::abs(a.upper));
  };
  std::array<CurveExpressionBounds, 3> values{};
  for (std::size_t i = 0; i < 3; ++i)
    if (coordinates_[i]) {
      auto interval = coordinates_[i]->bounds(document, lower - domain_error, upper + domain_error);
      if (!interval)
        return Result<EquationCurveIntervalAnalysis>::failure(interval.error());
      values[i] = interval.value();
    }
  EquationCurveIntervalAnalysis result;
  if (curve_.mode() == EquationCurveMode::Cartesian2D) {
    result.maximum_second_derivative = maximum(values[1].second);
    result.minimum_speed = std::hypot(1., minimum(values[1].first));
    result.maximum_speed = std::hypot(1., maximum(values[1].first));
    result.rounding_error_mm = values[1].rounding_error;
  } else if (curve_.mode() == EquationCurveMode::Polar2D) {
    const auto r = values[0];
    result.maximum_second_derivative =
        std::hypot(maximum(r.second) + maximum(r.value), 2 * maximum(r.first));
    result.minimum_speed = std::hypot(minimum(r.value), minimum(r.first));
    result.maximum_speed = std::hypot(maximum(r.value), maximum(r.first));
    result.rounding_error_mm =
        std::sqrt(2.) * (r.rounding_error + 64 * std::numeric_limits<double>::epsilon() *
                                                std::max(1., maximum(r.value)));
  } else {
    for (const auto& v : values) {
      result.maximum_second_derivative =
          std::hypot(result.maximum_second_derivative, maximum(v.second));
      result.minimum_speed = std::hypot(result.minimum_speed, minimum(v.first));
      result.maximum_speed = std::hypot(result.maximum_speed, maximum(v.first));
      result.rounding_error_mm = std::hypot(result.rounding_error_mm, v.rounding_error);
    }
  }
  result.rounding_error_mm += result.maximum_speed * domain_error;
  if (result.maximum_second_derivative > 0)
    result.maximum_second_derivative =
        std::nextafter(result.maximum_second_derivative, std::numeric_limits<double>::infinity());
  if (result.minimum_speed > 0)
    result.minimum_speed = std::nextafter(result.minimum_speed, 0.);
  if (result.maximum_speed > 0)
    result.maximum_speed =
        std::nextafter(result.maximum_speed, std::numeric_limits<double>::infinity());
  if (result.rounding_error_mm > 0)
    result.rounding_error_mm =
        std::nextafter(result.rounding_error_mm, std::numeric_limits<double>::infinity());
  if (!std::isfinite(result.maximum_second_derivative) || !std::isfinite(result.minimum_speed) ||
      !std::isfinite(result.maximum_speed) || !std::isfinite(result.rounding_error_mm))
    return fail("Equation interval exceeds finite numeric resolution");
  return Result<EquationCurveIntervalAnalysis>::success(result);
}
Result<bool> EquationCurveProgram::proves_closure(const PartDocument& document) const {
  if (curve_.mode() == EquationCurveMode::Cartesian2D)
    return Result<bool>::success(false);
  for (const auto& coordinate : coordinates_)
    if (coordinate) {
      auto periodic = coordinate->proves_period(document, lower_, upper_);
      if (!periodic)
        return periodic;
      if (!periodic.value()) {
        auto identical = coordinate->proves_endpoint_identity(document, lower_, upper_);
        if (!identical || !identical.value())
          return identical;
      }
    }
  if (curve_.mode() == EquationCurveMode::Polar2D) {
    auto angular = CurveExpression::compile(document, "sin(" + curve_.variable().name + ")",
                                            curve_.variable(), curve_.id().value());
    if (!angular)
      return Result<bool>::failure(angular.error());
    return angular.value().proves_period(document, lower_, upper_);
  }
  return Result<bool>::success(true);
}
} // namespace blcad
