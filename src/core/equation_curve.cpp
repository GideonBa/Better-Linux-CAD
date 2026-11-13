#include "blcad/core/equation_curve.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>

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
} // namespace blcad
