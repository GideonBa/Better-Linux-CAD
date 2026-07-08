#include "blcad/core/part_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace blcad {

namespace {

using json = nlohmann::json;

constexpr std::string_view k_schema = "blcad.part_document.mvp1";
constexpr int k_version = 1;

[[nodiscard]] Error json_error(std::string message) {
  return Error::validation("part_document_json", std::move(message));
}

[[nodiscard]] json point2_to_json(Point2 point) {
  return json{{"x", point.x}, {"y", point.y}};
}

[[nodiscard]] Point2 point2_from_json(const json& value) {
  return Point2{value.at("x").get<double>(), value.at("y").get<double>()};
}

[[nodiscard]] json point3_to_json(Point3 point) {
  return json{{"x", point.x}, {"y", point.y}, {"z", point.z}};
}

[[nodiscard]] json vector3_to_json(Vector3 vector) {
  return json{{"x", vector.x}, {"y", vector.y}, {"z", vector.z}};
}

[[nodiscard]] Result<Quantity> quantity_from_json(const json& parameter_json,
                                                  const std::string& object_id) {
  if (parameter_json.at("unit").get<std::string>() != "mm") {
    return Result<Quantity>::failure(json_error("only millimeter length parameters are supported"));
  }

  return Quantity::length_mm(parameter_json.at("value").get<double>(), object_id);
}

[[nodiscard]] Result<Parameter> parameter_from_json(const json& parameter_json) {
  const auto id = parameter_json.at("id").get<std::string>();

  if (parameter_json.at("type").get<std::string>() != "length") {
    return Result<Parameter>::failure(json_error("only length parameters are supported"));
  }

  if (parameter_json.at("scope").get<std::string>() != "part") {
    return Result<Parameter>::failure(json_error("only part-scope parameters are supported"));
  }

  auto quantity = quantity_from_json(parameter_json, id);
  if (quantity.has_error()) {
    return Result<Parameter>::failure(quantity.error());
  }

  return Parameter::create_length(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                  quantity.value());
}

[[nodiscard]] Result<DatumPlane> datum_plane_from_json(const json& datum_plane_json) {
  if (datum_plane_json.at("kind").get<std::string>() != "xy") {
    return Result<DatumPlane>::failure(json_error("only XY datum planes are supported"));
  }

  return DatumPlane::xy(DatumPlaneId(datum_plane_json.at("id").get<std::string>()),
                        datum_plane_json.at("name").get<std::string>());
}

[[nodiscard]] Result<Sketch> sketch_from_json(const json& sketch_json) {
  auto sketch = Sketch::create(SketchId(sketch_json.at("id").get<std::string>()),
                               sketch_json.at("name").get<std::string>(),
                               DatumPlaneId(sketch_json.at("workplane").get<std::string>()));
  if (sketch.has_error()) {
    return sketch;
  }

  for (const auto& rectangle_json : sketch_json.at("rectangle_profiles")) {
    auto rectangle = RectangleProfile::create(
        ProfileId(rectangle_json.at("id").get<std::string>()),
        ParameterId(rectangle_json.at("width_parameter").get<std::string>()),
        ParameterId(rectangle_json.at("height_parameter").get<std::string>()),
        point2_from_json(rectangle_json.at("center")));
    if (rectangle.has_error()) {
      return Result<Sketch>::failure(rectangle.error());
    }

    auto added = sketch.value().add_profile(rectangle.value());
    if (added.has_error()) {
      return Result<Sketch>::failure(added.error());
    }
  }

  for (const auto& circle_json : sketch_json.at("circle_profiles")) {
    auto circle = CircleProfile::create(
        ProfileId(circle_json.at("id").get<std::string>()),
        ParameterId(circle_json.at("diameter_parameter").get<std::string>()),
        point2_from_json(circle_json.at("center")));
    if (circle.has_error()) {
      return Result<Sketch>::failure(circle.error());
    }

    auto added = sketch.value().add_profile(circle.value());
    if (added.has_error()) {
      return Result<Sketch>::failure(added.error());
    }
  }

  return sketch;
}

[[nodiscard]] Result<Feature> feature_from_json(const json& feature_json) {
  const auto type = feature_json.at("type").get<std::string>();

  if (feature_json.at("direction").get<std::string>() != "+Z") {
    return Result<Feature>::failure(json_error("only +Z extrude direction is supported"));
  }

  if (type == "additive_extrude") {
    return Feature::create_additive_extrude(
        FeatureId(feature_json.at("id").get<std::string>()),
        feature_json.at("name").get<std::string>(),
        SketchId(feature_json.at("input_sketch").get<std::string>()),
        ParameterId(feature_json.at("length_parameter").get<std::string>()));
  }

  if (type == "subtractive_extrude") {
    if (feature_json.at("depth").get<std::string>() != "through_all") {
      return Result<Feature>::failure(
          json_error("only through_all subtractive extrude depth is supported"));
    }

    return Feature::create_subtractive_extrude(
        FeatureId(feature_json.at("id").get<std::string>()),
        feature_json.at("name").get<std::string>(),
        SketchId(feature_json.at("input_sketch").get<std::string>()),
        FeatureId(feature_json.at("target_feature").get<std::string>()));
  }

  return Result<Feature>::failure(json_error("unsupported feature type in part document json"));
}

} // namespace

Result<std::string> serialize_part_document_to_json(const PartDocument& document) {
  json root;
  root["schema"] = k_schema;
  root["version"] = k_version;
  root["document"] = json{{"id", document.id().value()}, {"name", document.name()}};

  root["parameters"] = json::array();
  for (const auto& parameter : document.parameters()) {
    root["parameters"].push_back(json{{"id", parameter.id().value()},
                                       {"name", parameter.name()},
                                       {"type", std::string(to_string(parameter.type()))},
                                       {"scope", std::string(to_string(parameter.scope()))},
                                       {"unit", std::string(parameter.value().unit())},
                                       {"value", parameter.value().millimeters()}});
  }

  root["datum_planes"] = json::array();
  for (const auto& datum_plane : document.datum_planes()) {
    root["datum_planes"].push_back(json{{"id", datum_plane.id().value()},
                                         {"name", datum_plane.name()},
                                         {"kind", "xy"},
                                         {"origin", point3_to_json(datum_plane.origin())},
                                         {"x_axis", vector3_to_json(datum_plane.x_axis())},
                                         {"y_axis", vector3_to_json(datum_plane.y_axis())},
                                         {"normal", vector3_to_json(datum_plane.normal())}});
  }

  root["sketches"] = json::array();
  for (const auto& sketch : document.sketches()) {
    json sketch_json{{"id", sketch.id().value()},
                     {"name", sketch.name()},
                     {"workplane", sketch.workplane().value()},
                     {"rectangle_profiles", json::array()},
                     {"circle_profiles", json::array()}};

    for (const auto& profile : sketch.rectangle_profiles()) {
      sketch_json["rectangle_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"center", point2_to_json(profile.center())},
               {"width_parameter", profile.width_parameter().value()},
               {"height_parameter", profile.height_parameter().value()}});
    }

    for (const auto& profile : sketch.circle_profiles()) {
      sketch_json["circle_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"center", point2_to_json(profile.center())},
               {"diameter_parameter", profile.diameter_parameter().value()}});
    }

    root["sketches"].push_back(std::move(sketch_json));
  }

  root["features"] = json::array();
  for (const auto& feature : document.features()) {
    json feature_json{{"id", feature.id().value()},
                      {"name", feature.name()},
                      {"type", std::string(to_string(feature.type()))},
                      {"input_sketch", feature.input_sketch().value()},
                      {"direction", std::string(to_string(feature.direction()))}};

    if (feature.type() == FeatureType::AdditiveExtrude) {
      feature_json["length_parameter"] = feature.length_parameter().value();
    }

    if (feature.type() == FeatureType::SubtractiveExtrude) {
      feature_json["target_feature"] = feature.target_feature().value();
      feature_json["depth"] = std::string(to_string(feature.subtractive_depth()));
    }

    root["features"].push_back(std::move(feature_json));
  }

  return Result<std::string>::success(root.dump(2));
}

Result<PartDocument> deserialize_part_document_from_json(std::string_view content) {
  try {
    const json root = json::parse(content.begin(), content.end());

    if (!root.is_object()) {
      return Result<PartDocument>::failure(json_error("part document json root must be an object"));
    }

    if (root.at("schema").get<std::string>() != k_schema) {
      return Result<PartDocument>::failure(json_error("unsupported part document json schema"));
    }

    if (root.at("version").get<int>() != k_version) {
      return Result<PartDocument>::failure(json_error("unsupported part document json version"));
    }

    const auto& document_json = root.at("document");
    auto document = PartDocument::create(DocumentId(document_json.at("id").get<std::string>()),
                                         document_json.at("name").get<std::string>());
    if (document.has_error()) {
      return document;
    }

    for (const auto& parameter_json : root.at("parameters")) {
      auto parameter = parameter_from_json(parameter_json);
      if (parameter.has_error()) {
        return Result<PartDocument>::failure(parameter.error());
      }

      auto added = document.value().add_parameter(parameter.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    for (const auto& datum_plane_json : root.at("datum_planes")) {
      auto datum_plane = datum_plane_from_json(datum_plane_json);
      if (datum_plane.has_error()) {
        return Result<PartDocument>::failure(datum_plane.error());
      }

      auto added = document.value().add_datum_plane(datum_plane.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    for (const auto& sketch_json : root.at("sketches")) {
      auto sketch = sketch_from_json(sketch_json);
      if (sketch.has_error()) {
        return Result<PartDocument>::failure(sketch.error());
      }

      auto added = document.value().add_sketch(sketch.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    for (const auto& feature_json : root.at("features")) {
      auto feature = feature_from_json(feature_json);
      if (feature.has_error()) {
        return Result<PartDocument>::failure(feature.error());
      }

      auto added = document.value().add_feature(feature.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    return document;
  } catch (const json::exception& exception) {
    return Result<PartDocument>::failure(
        json_error(std::string("invalid part document json: ") + exception.what()));
  } catch (const std::exception& exception) {
    return Result<PartDocument>::failure(
        json_error(std::string("invalid part document json: ") + exception.what()));
  }
}

} // namespace blcad
