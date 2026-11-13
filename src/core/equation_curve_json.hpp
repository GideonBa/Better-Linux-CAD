#pragma once
#include "blcad/core/equation_curve.hpp"
#include <nlohmann/json.hpp>

namespace blcad::detail {
inline bool equation_fields(const nlohmann::json& value, std::initializer_list<const char*> names) {
  if (!value.is_object() || value.size() != names.size())
    return false;
  for (const auto* name : names)
    if (!value.contains(name))
      return false;
  return true;
}
inline nlohmann::json equation_curve_to_json(const EquationCurve& curve) {
  nlohmann::json coordinates = nlohmann::json::object();
  const auto& values = curve.coordinates();
  if (curve.mode() == EquationCurveMode::Cartesian2D)
    coordinates["y"] = values[1];
  else if (curve.mode() == EquationCurveMode::Polar2D)
    coordinates["r"] = values[0];
  else {
    coordinates["x"] = values[0];
    coordinates["y"] = values[1];
    if (curve.mode() == EquationCurveMode::Parametric3D)
      coordinates["z"] = values[2];
  }
  return {{"id", curve.id().value()},
          {"mode", std::string(to_string(curve.mode()))},
          {"variable", curve.variable().name},
          {"domain",
           {{"unit", std::string(to_string(curve.variable().unit))},
            {"lower", curve.lower()},
            {"upper", curve.upper()}}},
          {"coordinates", coordinates},
          {"tolerance_mm", curve.tolerance_mm()},
          {"policy", std::string(curve.policy())},
          {"closure", std::string(to_string(curve.closure()))},
          {"construction", curve.construction()}};
}
inline Result<EquationCurve> equation_curve_from_json(const nlohmann::json& value) {
  const auto fail = [](std::string message) {
    return Result<EquationCurve>::failure(
        Error::validation("equation_curve.json", std::move(message)));
  };
  try {
    if (!equation_fields(value, {"id", "mode", "variable", "domain", "coordinates", "tolerance_mm",
                                 "policy", "closure", "construction"}) ||
        !value.at("id").is_string() || !value.at("mode").is_string() ||
        !value.at("variable").is_string() || !value.at("tolerance_mm").is_number() ||
        !value.at("construction").is_boolean() || value.at("policy") != "adaptive_v1" ||
        !value.at("closure").is_string())
      return fail("Equation curve requires exact typed intent fields and adaptive_v1 policy");
    const auto& domain = value.at("domain");
    if (!equation_fields(domain, {"unit", "lower", "upper"}) || !domain.at("unit").is_string() ||
        !domain.at("lower").is_string() || !domain.at("upper").is_string())
      return fail("Equation domain requires typed unit and expression bounds");
    std::optional<CurveUnit> unit;
    for (auto item : {CurveUnit::Scalar, CurveUnit::Length, CurveUnit::Angle})
      if (domain.at("unit") == to_string(item))
        unit = item;
    std::optional<EquationCurveMode> mode;
    for (auto item : {EquationCurveMode::Cartesian2D, EquationCurveMode::Polar2D,
                      EquationCurveMode::Parametric2D, EquationCurveMode::Parametric3D})
      if (value.at("mode") == to_string(item))
        mode = item;
    std::optional<EquationCurveClosure> closure;
    for (auto item : {EquationCurveClosure::Open, EquationCurveClosure::Periodic})
      if (value.at("closure") == to_string(item))
        closure = item;
    if (!unit || !mode || !closure)
      return fail("Unknown equation curve mode, domain unit or closure policy");
    const auto& coordinates = value.at("coordinates");
    const bool correct =
        *mode == EquationCurveMode::Cartesian2D    ? equation_fields(coordinates, {"y"})
        : *mode == EquationCurveMode::Polar2D      ? equation_fields(coordinates, {"r"})
        : *mode == EquationCurveMode::Parametric2D ? equation_fields(coordinates, {"x", "y"})
                                                   : equation_fields(coordinates, {"x", "y", "z"});
    if (!correct)
      return fail("Equation coordinates do not match their mode");
    for (const auto& coordinate : coordinates)
      if (!coordinate.is_string())
        return fail("Equation coordinates must be expressions");
    std::array<std::string, 3> source;
    if (*mode == EquationCurveMode::Cartesian2D)
      source[1] = coordinates.at("y").get<std::string>();
    else if (*mode == EquationCurveMode::Polar2D)
      source[0] = coordinates.at("r").get<std::string>();
    else {
      source[0] = coordinates.at("x").get<std::string>();
      source[1] = coordinates.at("y").get<std::string>();
      if (*mode == EquationCurveMode::Parametric3D)
        source[2] = coordinates.at("z").get<std::string>();
    }
    return EquationCurve::create(
        SketchEntityId(value.at("id").get<std::string>()), *mode,
        {value.at("variable").get<std::string>(), *unit}, domain.at("lower").get<std::string>(),
        domain.at("upper").get<std::string>(), source, value.at("tolerance_mm").get<double>(),
        *closure, value.at("construction").get<bool>());
  } catch (const nlohmann::json::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::detail
