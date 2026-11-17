#include "direct_edit_json.hpp"
#include <stdexcept>

namespace blcad::detail {
namespace {
using json = nlohmann::json;
void require(bool yes, const char* message) {
  if (!yes)
    throw std::invalid_argument(message);
}
void exact(const json& value, std::initializer_list<const char*> keys) {
  require(value.is_object() && value.size() == keys.size(), "Malformed Direct Edit record fields");
  for (const auto* key : keys)
    require(value.contains(key), "Missing Direct Edit record field");
}
template <class T> json coordinates(T value) {
  return {value.x, value.y, value.z};
}
Vector3 vector(const json& v) {
  require(v.is_array() && v.size() == 3, "Direct Edit vectors require three coordinates");
  for (const auto& c : v)
    require(c.is_number(), "Direct Edit coordinates must be numbers");
  return {v[0].get<double>(), v[1].get<double>(), v[2].get<double>()};
}
const char* capability(DirectEditFaceCapability value) {
  switch (value) {
  case DirectEditFaceCapability::Planar:
    return "planar";
  case DirectEditFaceCapability::Cylindrical:
    return "cylindrical";
  case DirectEditFaceCapability::AnySurface:
    return "surface";
  }
  throw std::invalid_argument("Unknown Direct Edit face capability");
}
} // namespace

json direct_edit_to_json(const DirectEditFeature& feature, const DirectEditFaceWriter& face) {
  json selections = json::array();
  for (const auto& entry : feature.selections())
    selections.push_back(
        {{"reference", face(entry.reference)}, {"expected", capability(entry.expected)}});
  auto operation = std::visit(
      [](const auto& op) -> json {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, DirectFaceMove>)
          return {{"mode", "move"},
                  {"x", op.x.value()},
                  {"y", op.y.value()},
                  {"z", op.z.value()},
                  {"signs", coordinates(op.signs)}};
        else if constexpr (std::is_same_v<T, DirectFaceRotate>)
          return {{"mode", "rotate"}, {"axis", coordinates(op.axis)}, {"angle", op.angle.value()}};
        else if constexpr (std::is_same_v<T, DirectFaceOffset>)
          return {{"mode", "offset"}, {"distance", op.distance.value()}, {"reverse", op.reverse}};
        else if constexpr (std::is_same_v<T, DirectFaceSize>)
          return {{"mode", "size"},
                  {"size", op.size.value()},
                  {"measurement", op.mode == DirectEditSizeMode::Radius ? "radius" : "diameter"}};
        else
          return {{"mode", "delete_heal"}};
      },
      feature.operation());
  return {{"id", feature.id().value()},
          {"name", feature.name()},
          {"target_body", feature.target_body().value()},
          {"predecessor", feature.predecessor().value()},
          {"operation_version", 1},
          {"faces", selections},
          {"operation", operation},
          {"frame",
           {{"origin_mm", coordinates(feature.frame().origin_mm)},
            {"x_axis", coordinates(feature.frame().x_axis)},
            {"y_axis", coordinates(feature.frame().y_axis)}}},
          {"policy",
           {{"tolerance_mm", feature.policy().tolerance_mm},
            {"blends", "reject_affected"},
            {"healing", "extend_adjacent"}}}};
}

Result<DirectEditFeature> direct_edit_from_json(const json& v, const DirectEditFaceReader& face) {
  try {
    exact(v, {"id", "name", "target_body", "predecessor", "operation_version", "faces", "operation",
              "frame", "policy"});
    require(v.at("operation_version").is_number_integer() && v.at("operation_version") == 1,
            "Unsupported Direct Edit operation version");
    require(v.at("faces").is_array(), "Direct Edit faces must be an array");
    std::vector<DirectEditFace> selections;
    for (const auto& entry : v.at("faces")) {
      exact(entry, {"reference", "expected"});
      auto ref = face(entry.at("reference"), PartFeatureInputRole::SurfaceSourceFace);
      if (!ref)
        return Result<DirectEditFeature>::failure(ref.error());
      DirectEditFaceCapability expected;
      if (entry.at("expected") == "planar")
        expected = DirectEditFaceCapability::Planar;
      else if (entry.at("expected") == "cylindrical")
        expected = DirectEditFaceCapability::Cylindrical;
      else if (entry.at("expected") == "surface")
        expected = DirectEditFaceCapability::AnySurface;
      else
        throw std::invalid_argument("Unknown Direct Edit face capability");
      selections.push_back({std::move(ref.value()), expected});
    }
    const auto& frame = v.at("frame");
    exact(frame, {"origin_mm", "x_axis", "y_axis"});
    const auto origin = vector(frame.at("origin_mm"));
    DirectEditFrame axes{
        {origin.x, origin.y, origin.z}, vector(frame.at("x_axis")), vector(frame.at("y_axis"))};
    const auto& policy = v.at("policy");
    exact(policy, {"tolerance_mm", "blends", "healing"});
    require(policy.at("tolerance_mm").is_number() && policy.at("blends") == "reject_affected" &&
                policy.at("healing") == "extend_adjacent",
            "Unsupported Direct Edit blend/healing policy");
    DirectEditPolicy healing{policy.at("tolerance_mm").get<double>()};
    const auto& op = v.at("operation");
    require(op.is_object() && op.contains("mode"), "Missing Direct Edit operation mode");
    DirectEditOperation operation = DirectFaceDeleteHeal{};
    if (op.at("mode") == "move") {
      exact(op, {"mode", "x", "y", "z", "signs"});
      operation = DirectFaceMove{
          ParameterId(op.at("x").get<std::string>()), ParameterId(op.at("y").get<std::string>()),
          ParameterId(op.at("z").get<std::string>()), vector(op.at("signs"))};
    } else if (op.at("mode") == "rotate") {
      exact(op, {"mode", "axis", "angle"});
      operation =
          DirectFaceRotate{vector(op.at("axis")), ParameterId(op.at("angle").get<std::string>())};
    } else if (op.at("mode") == "offset") {
      exact(op, {"mode", "distance", "reverse"});
      require(op.at("reverse").is_boolean(), "Direct Edit offset reverse must be Boolean");
      operation = DirectFaceOffset{ParameterId(op.at("distance").get<std::string>()),
                                   op.at("reverse").get<bool>()};
    } else if (op.at("mode") == "size") {
      exact(op, {"mode", "size", "measurement"});
      require(op.at("measurement") == "radius" || op.at("measurement") == "diameter",
              "Unknown Direct Edit size measurement");
      operation = DirectFaceSize{ParameterId(op.at("size").get<std::string>()),
                                 op.at("measurement") == "radius" ? DirectEditSizeMode::Radius
                                                                  : DirectEditSizeMode::Diameter};
    } else if (op.at("mode") == "delete_heal")
      exact(op, {"mode"});
    else
      throw std::invalid_argument("Unknown Direct Edit operation mode");
    return DirectEditFeature::create(FeatureId(v.at("id").get<std::string>()),
                                     v.at("name").get<std::string>(),
                                     BodyId(v.at("target_body").get<std::string>()),
                                     FeatureId(v.at("predecessor").get<std::string>()),
                                     std::move(selections), std::move(operation), axes, healing);
  } catch (const std::exception& e) {
    return Result<DirectEditFeature>::failure(Error::validation("direct_edit", e.what()));
  }
}
} // namespace blcad::detail
