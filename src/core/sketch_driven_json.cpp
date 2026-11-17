#include "sketch_driven_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include <stdexcept>
namespace blcad::detail {
namespace {
using json = nlohmann::json;
void require(bool okay, const char* message) {
  if (!okay)
    throw std::invalid_argument(message);
}
template <class E>
E choice(const json& value, std::initializer_list<std::pair<const char*, E>> entries) {
  require(value.is_string(), "Rib/Web enum must be a string");
  for (const auto& [key, mode] : entries)
    if (value == key)
      return mode;
  throw std::invalid_argument("Unknown Rib/Web mode");
}
} // namespace
json sketch_driven_to_json(const SketchDrivenFeature& feature, const SketchDrivenFaceWriter& face) {
  const auto& r = std::get<RibWebIntent>(feature.definition().intent);
  json curves = json::array();
  for (const auto& curve : r.source.curves)
    curves.push_back(curve.value());
  json extent = std::visit(
      [&](const auto& e) -> json {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, RibDistanceExtent>)
          return {{"mode", "distance"}, {"distance", e.distance.value()}};
        else if constexpr (std::is_same_v<T, RibBodyExtent>)
          return {{"mode", "body"}, {"body", e.body.value()}};
        else
          return {{"mode", "face"}, {"face", face(e.face)}};
      },
      r.extent);
  const char* side = r.side == RibThicknessSide::Symmetric ? "symmetric"
                     : r.side == RibThicknessSide::First   ? "first"
                     : r.side == RibThicknessSide::Second  ? "second"
                                                           : "asymmetric";
  return {
      {"id", feature.id().value()},
      {"name", feature.name()},
      {"target_body", feature.target_body().value()},
      {"mode", r.kind == RibWebKind::Rib ? "rib" : "web"},
      {"source",
       {{"sketch", r.source.sketch.value()},
        {"profile", r.source.profile ? json(r.source.profile->value()) : json(nullptr)},
        {"curves", curves}}},
      {"plane", r.plane == RibExtrusionPlane::PerpendicularToSketch ? "perpendicular" : "parallel"},
      {"direction", {r.direction.x, r.direction.y, r.direction.z}},
      {"side", side},
      {"thickness", r.thickness.value()},
      {"second_thickness", r.second_thickness ? json(r.second_thickness->value()) : json(nullptr)},
      {"draft", r.draft ? json(r.draft->value()) : json(nullptr)},
      {"extent", extent},
      {"crossings", r.crossings == RibCrossingPolicy::Union ? "union" : "reject"}};
}
Result<SketchDrivenFeature> sketch_driven_from_json(const json& value,
                                                    const SketchDrivenFaceReader& face) {
  try {
    require(value.is_object() && value.size() == 13, "Malformed Rib/Web record");
    const auto id = FeatureId(value.at("id").get<std::string>());
    const auto name = value.at("name").get<std::string>();
    const auto body = BodyId(value.at("target_body").get<std::string>());
    const auto& source = value.at("source");
    require(source.is_object() && source.size() == 3 && source.at("curves").is_array(),
            "Malformed Rib/Web Sketch source");
    RibSketchSource sketch{SketchId(source.at("sketch").get<std::string>()), {}, std::nullopt};
    if (!source.at("profile").is_null())
      sketch.profile = ProfileId(source.at("profile").get<std::string>());
    for (const auto& curve : source.at("curves"))
      sketch.curves.emplace_back(curve.get<std::string>());
    const auto& direction = value.at("direction");
    require(direction.is_array() && direction.size() == 3,
            "Rib/Web direction requires three finite coordinates");
    for (const auto& coordinate : direction)
      require(coordinate.is_number(), "Rib/Web direction coordinates must be numeric");
    const auto& extent = value.at("extent");
    require(extent.is_object() && extent.size() == 2, "Malformed Rib/Web termination");
    RibExtent termination = RibDistanceExtent{ParameterId("")};
    if (extent.at("mode") == "distance")
      termination = RibDistanceExtent{ParameterId(extent.at("distance").get<std::string>())};
    else if (extent.at("mode") == "body")
      termination = RibBodyExtent{BodyId(extent.at("body").get<std::string>())};
    else if (extent.at("mode") == "face") {
      auto ref = face(extent.at("face"), PartFeatureInputRole::ExtrudeLimitFace);
      if (!ref)
        return Result<SketchDrivenFeature>::failure(ref.error());
      termination = RibFaceExtent{std::move(ref.value())};
    } else
      throw std::invalid_argument("Unknown Rib/Web termination mode");
    RibWebIntent rib{
        choice<RibWebKind>(value.at("mode"), {{"rib", RibWebKind::Rib}, {"web", RibWebKind::Web}}),
        std::move(sketch),
        choice<RibExtrusionPlane>(value.at("plane"),
                                  {{"perpendicular", RibExtrusionPlane::PerpendicularToSketch},
                                   {"parallel", RibExtrusionPlane::ParallelToSketch}}),
        {direction[0].get<double>(), direction[1].get<double>(), direction[2].get<double>()},
        choice<RibThicknessSide>(value.at("side"), {{"symmetric", RibThicknessSide::Symmetric},
                                                    {"first", RibThicknessSide::First},
                                                    {"second", RibThicknessSide::Second},
                                                    {"asymmetric", RibThicknessSide::Asymmetric}}),
        ParameterId(value.at("thickness").get<std::string>()),
        std::nullopt,
        std::nullopt,
        std::move(termination),
        choice<RibCrossingPolicy>(value.at("crossings"), {{"union", RibCrossingPolicy::Union},
                                                          {"reject", RibCrossingPolicy::Reject}})};
    if (!value.at("second_thickness").is_null())
      rib.second_thickness = ParameterId(value.at("second_thickness").get<std::string>());
    if (!value.at("draft").is_null())
      rib.draft = ParameterId(value.at("draft").get<std::string>());
    return SketchDrivenFeature::create_rib_web(id, name, body, std::move(rib));
  } catch (const std::exception& e) {
    return Result<SketchDrivenFeature>::failure(Error::validation("sketch_driven", e.what()));
  }
}
} // namespace blcad::detail
