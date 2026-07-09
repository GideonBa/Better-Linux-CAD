#include "blcad/core/part_document_json.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace blcad {

namespace {

using json = nlohmann::json;

constexpr std::string_view k_schema = "blcad.part_document.mvp1";
constexpr int k_version = 1;

[[nodiscard]] Error json_error(std::string message) {
  return Error::validation("part_document_json", std::move(message));
}

[[nodiscard]] Error json_file_error(const std::filesystem::path& path, std::string message) {
  const auto object_id = path.empty() ? std::string("part_document_json_file") : path.string();
  return Error::validation(object_id, std::move(message));
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

[[nodiscard]] Point3 point3_from_json(const json& value) {
  return Point3{value.at("x").get<double>(), value.at("y").get<double>(),
                value.at("z").get<double>()};
}

[[nodiscard]] json vector3_to_json(Vector3 vector) {
  return json{{"x", vector.x}, {"y", vector.y}, {"z", vector.z}};
}

[[nodiscard]] Vector3 vector3_from_json(const json& value) {
  return Vector3{value.at("x").get<double>(), value.at("y").get<double>(),
                 value.at("z").get<double>()};
}

[[nodiscard]] json parameter_ids_to_json(const std::vector<ParameterId>& ids) {
  json result = json::array();
  for (const auto& id : ids) {
    result.push_back(id.value());
  }
  return result;
}

[[nodiscard]] std::vector<ParameterId> parameter_ids_from_json(const json& values) {
  std::vector<ParameterId> ids;
  ids.reserve(values.size());
  for (const auto& value : values) {
    ids.emplace_back(value.get<std::string>());
  }
  return ids;
}

[[nodiscard]] json sketch_entity_ids_to_json(const std::vector<SketchEntityId>& ids) {
  json result = json::array();
  for (const auto& id : ids) {
    result.push_back(id.value());
  }
  return result;
}

[[nodiscard]] std::vector<SketchEntityId> sketch_entity_ids_from_json(const json& values) {
  std::vector<SketchEntityId> ids;
  ids.reserve(values.size());
  for (const auto& value : values) {
    ids.emplace_back(value.get<std::string>());
  }
  return ids;
}

[[nodiscard]] Result<SemanticFace> semantic_face_from_json(const json& value) {
  const auto face = value.get<std::string>();
  if (face == "top") {
    return Result<SemanticFace>::success(SemanticFace::Top);
  }

  if (face == "bottom") {
    return Result<SemanticFace>::success(SemanticFace::Bottom);
  }

  if (face == "right") {
    return Result<SemanticFace>::success(SemanticFace::Right);
  }

  if (face == "left") {
    return Result<SemanticFace>::success(SemanticFace::Left);
  }

  if (face == "front") {
    return Result<SemanticFace>::success(SemanticFace::Front);
  }

  if (face == "back") {
    return Result<SemanticFace>::success(SemanticFace::Back);
  }

  return Result<SemanticFace>::failure(json_error("unsupported semantic face in part document json"));
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

[[nodiscard]] std::vector<ParameterId> parameter_dependencies_from_object(const json& value) {
  if (!value.contains("parameter_dependencies")) {
    return {};
  }

  return parameter_ids_from_json(value.at("parameter_dependencies"));
}

[[nodiscard]] json construction_relation_to_json(const ConstructionRelation& relation) {
  json relation_json{{"id", relation.id().value()}, {"type", std::string(to_string(relation.type()))}};

  if (relation.type() == ConstructionRelationType::PlaneOffsetFromPlane) {
    relation_json["source_plane"] = relation.source_plane().value();
    relation_json["offset_parameter"] = relation.offset_parameter().value();
  }

  if (relation.type() == ConstructionRelationType::LineThroughTwoPoints) {
    relation_json["first_point"] = relation.first_point().value();
    relation_json["second_point"] = relation.second_point().value();
  }

  if (relation.type() == ConstructionRelationType::PlaneThroughThreePoints) {
    relation_json["first_point"] = relation.first_point().value();
    relation_json["second_point"] = relation.second_point().value();
    relation_json["third_point"] = relation.third_point().value();
  }

  return relation_json;
}

[[nodiscard]] Result<ConstructionRelation> construction_relation_from_json(const json& relation_json) {
  const auto type = relation_json.at("type").get<std::string>();
  const auto id = ConstructionRelationId(relation_json.at("id").get<std::string>());

  if (type == "plane_offset_from_plane") {
    return ConstructionRelation::create_plane_offset_from_plane(
        id, DatumPlaneId(relation_json.at("source_plane").get<std::string>()),
        ParameterId(relation_json.at("offset_parameter").get<std::string>()));
  }

  if (type == "line_through_two_points") {
    return ConstructionRelation::create_line_through_two_points(
        id, ConstructionPointId(relation_json.at("first_point").get<std::string>()),
        ConstructionPointId(relation_json.at("second_point").get<std::string>()));
  }

  if (type == "plane_through_three_points") {
    return ConstructionRelation::create_plane_through_three_points(
        id, ConstructionPointId(relation_json.at("first_point").get<std::string>()),
        ConstructionPointId(relation_json.at("second_point").get<std::string>()),
        ConstructionPointId(relation_json.at("third_point").get<std::string>()));
  }

  return Result<ConstructionRelation>::failure(
      json_error("unsupported construction relation type in part document json"));
}

[[nodiscard]] Result<ConstructionPoint> construction_point_from_json(const json& point_json) {
  if (point_json.at("kind").get<std::string>() != "explicit") {
    return Result<ConstructionPoint>::failure(
        json_error("only explicit construction points are supported"));
  }

  return ConstructionPoint::create_explicit(
      ConstructionPointId(point_json.at("id").get<std::string>()),
      point_json.at("name").get<std::string>(), point3_from_json(point_json.at("position")),
      parameter_dependencies_from_object(point_json));
}

[[nodiscard]] Result<ConstructionLine> construction_line_from_json(const json& line_json) {
  const auto kind = line_json.at("kind").get<std::string>();

  if (kind == "explicit") {
    return ConstructionLine::create_explicit(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), point3_from_json(line_json.at("point")),
        vector3_from_json(line_json.at("direction")), parameter_dependencies_from_object(line_json));
  }

  if (kind == "through_two_points") {
    auto relation = construction_relation_from_json(line_json.at("relation"));
    if (relation.has_error()) {
      return Result<ConstructionLine>::failure(relation.error());
    }

    return ConstructionLine::create_through_two_points(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), relation.value());
  }

  return Result<ConstructionLine>::failure(
      json_error("unsupported construction line kind in part document json"));
}

[[nodiscard]] Result<ConstructionPlane> construction_plane_from_json(const json& plane_json) {
  const auto kind = plane_json.at("kind").get<std::string>();

  if (kind == "explicit") {
    return ConstructionPlane::create_explicit(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), point3_from_json(plane_json.at("origin")),
        vector3_from_json(plane_json.at("x_axis")), vector3_from_json(plane_json.at("y_axis")),
        vector3_from_json(plane_json.at("normal")), parameter_dependencies_from_object(plane_json));
  }

  if (kind == "offset_from_plane") {
    auto relation = construction_relation_from_json(plane_json.at("relation"));
    if (relation.has_error()) {
      return Result<ConstructionPlane>::failure(relation.error());
    }

    return ConstructionPlane::create_offset_from_plane(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), relation.value());
  }

  if (kind == "through_three_points") {
    auto relation = construction_relation_from_json(plane_json.at("relation"));
    if (relation.has_error()) {
      return Result<ConstructionPlane>::failure(relation.error());
    }

    return ConstructionPlane::create_through_three_points(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), relation.value());
  }

  return Result<ConstructionPlane>::failure(
      json_error("unsupported construction plane kind in part document json"));
}

[[nodiscard]] Result<DerivedWorkplane> derived_workplane_from_json(
    const json& derived_workplane_json) {
  if (derived_workplane_json.at("kind").get<std::string>() != "feature_face") {
    return Result<DerivedWorkplane>::failure(
        json_error("only feature-face derived workplanes are supported"));
  }

  auto face = semantic_face_from_json(derived_workplane_json.at("face"));
  if (face.has_error()) {
    return Result<DerivedWorkplane>::failure(face.error());
  }

  auto face_reference = SemanticFaceReference::create(
      FeatureId(derived_workplane_json.at("source_feature").get<std::string>()), face.value());
  if (face_reference.has_error()) {
    return Result<DerivedWorkplane>::failure(face_reference.error());
  }

  return DerivedWorkplane::create_on_feature_face(
      DatumPlaneId(derived_workplane_json.at("id").get<std::string>()),
      derived_workplane_json.at("name").get<std::string>(), face_reference.value());
}

[[nodiscard]] Result<Sketch> sketch_from_json(const json& sketch_json) {
  auto sketch = Sketch::create(SketchId(sketch_json.at("id").get<std::string>()),
                               sketch_json.at("name").get<std::string>(),
                               DatumPlaneId(sketch_json.at("workplane").get<std::string>()));
  if (sketch.has_error()) {
    return sketch;
  }

  const json line_segments =
      sketch_json.contains("line_segments") ? sketch_json.at("line_segments") : json::array();
  for (const auto& line_json : line_segments) {
    auto line = LineSegment::create(SketchEntityId(line_json.at("id").get<std::string>()),
                                    point2_from_json(line_json.at("start")),
                                    point2_from_json(line_json.at("end")));
    if (line.has_error()) {
      return Result<Sketch>::failure(line.error());
    }

    auto added = sketch.value().add_entity(line.value());
    if (added.has_error()) {
      return Result<Sketch>::failure(added.error());
    }
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

  const json closed_profiles =
      sketch_json.contains("closed_profiles") ? sketch_json.at("closed_profiles") : json::array();
  for (const auto& profile_json : closed_profiles) {
    auto profile = ClosedProfile::create(
        ProfileId(profile_json.at("id").get<std::string>()),
        sketch_entity_ids_from_json(profile_json.at("line_segments")));
    if (profile.has_error()) {
      return Result<Sketch>::failure(profile.error());
    }

    auto added = sketch.value().add_profile(profile.value());
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

  root["construction_points"] = json::array();
  for (const auto& point : document.construction_points()) {
    root["construction_points"].push_back(
        json{{"id", point.id().value()},
             {"name", point.name()},
             {"kind", "explicit"},
             {"position", point3_to_json(point.position())},
             {"parameter_dependencies", parameter_ids_to_json(point.parameter_dependencies())}});
  }

  root["construction_lines"] = json::array();
  for (const auto& line : document.construction_lines()) {
    json line_json{{"id", line.id().value()},
                   {"name", line.name()},
                   {"kind", std::string(to_string(line.kind()))}};

    if (line.kind() == ConstructionLineKind::Explicit) {
      line_json["point"] = point3_to_json(line.point());
      line_json["direction"] = vector3_to_json(line.direction());
      line_json["parameter_dependencies"] = parameter_ids_to_json(line.parameter_dependencies());
    } else if (line.relation().has_value()) {
      line_json["relation"] = construction_relation_to_json(line.relation().value());
    }

    root["construction_lines"].push_back(std::move(line_json));
  }

  root["construction_planes"] = json::array();
  for (const auto& plane : document.construction_planes()) {
    json plane_json{{"id", plane.id().value()},
                    {"name", plane.name()},
                    {"kind", std::string(to_string(plane.kind()))}};

    if (plane.kind() == ConstructionPlaneKind::Explicit) {
      plane_json["origin"] = point3_to_json(plane.origin());
      plane_json["x_axis"] = vector3_to_json(plane.x_axis());
      plane_json["y_axis"] = vector3_to_json(plane.y_axis());
      plane_json["normal"] = vector3_to_json(plane.normal());
      plane_json["parameter_dependencies"] = parameter_ids_to_json(plane.parameter_dependencies());
    } else if (plane.relation().has_value()) {
      plane_json["relation"] = construction_relation_to_json(plane.relation().value());
    }

    root["construction_planes"].push_back(std::move(plane_json));
  }

  root["derived_workplanes"] = json::array();
  for (const auto& workplane : document.derived_workplanes()) {
    root["derived_workplanes"].push_back(
        json{{"id", workplane.id().value()},
             {"name", workplane.name()},
             {"kind", "feature_face"},
             {"source_feature", workplane.face_reference().source_feature().value()},
             {"face", std::string(to_string(workplane.face_reference().face()))}});
  }

  root["sketches"] = json::array();
  for (const auto& sketch : document.sketches()) {
    json sketch_json{{"id", sketch.id().value()},
                     {"name", sketch.name()},
                     {"workplane", sketch.workplane().value()},
                     {"line_segments", json::array()},
                     {"rectangle_profiles", json::array()},
                     {"circle_profiles", json::array()},
                     {"closed_profiles", json::array()}};

    for (const auto& line : sketch.line_segments()) {
      sketch_json["line_segments"].push_back(json{{"id", line.id().value()},
                                                    {"start", point2_to_json(line.start())},
                                                    {"end", point2_to_json(line.end())}});
    }

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

    for (const auto& profile : sketch.closed_profiles()) {
      sketch_json["closed_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"line_segments", sketch_entity_ids_to_json(profile.line_segments())}});
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

    const json construction_point_array =
        root.contains("construction_points") ? root.at("construction_points") : json::array();
    for (const auto& point_json : construction_point_array) {
      auto point = construction_point_from_json(point_json);
      if (point.has_error()) {
        return Result<PartDocument>::failure(point.error());
      }

      auto added = document.value().add_construction_point(point.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    const json construction_line_array =
        root.contains("construction_lines") ? root.at("construction_lines") : json::array();
    for (const auto& line_json : construction_line_array) {
      auto line = construction_line_from_json(line_json);
      if (line.has_error()) {
        return Result<PartDocument>::failure(line.error());
      }

      auto added = document.value().add_construction_line(line.value());
      if (added.has_error()) {
        return Result<PartDocument>::failure(added.error());
      }
    }

    const json construction_plane_array =
        root.contains("construction_planes") ? root.at("construction_planes") : json::array();
    std::vector<bool> added_construction_plane(construction_plane_array.size(), false);
    std::size_t remaining_construction_planes = construction_plane_array.size();
    while (remaining_construction_planes > 0U) {
      bool progress = false;

      for (std::size_t index = 0; index < construction_plane_array.size(); ++index) {
        if (added_construction_plane[index]) {
          continue;
        }

        auto plane = construction_plane_from_json(construction_plane_array.at(index));
        if (plane.has_error()) {
          return Result<PartDocument>::failure(plane.error());
        }

        auto added = document.value().add_construction_plane(plane.value());
        if (added.has_error()) {
          const auto message = added.error().message();
          if (message == "plane offset source plane must exist in part document") {
            continue;
          }

          return Result<PartDocument>::failure(added.error());
        }

        added_construction_plane[index] = true;
        --remaining_construction_planes;
        progress = true;
      }

      if (!progress) {
        return Result<PartDocument>::failure(
            json_error("could not resolve construction plane json dependencies"));
      }
    }

    const auto& sketch_array = root.at("sketches");
    const auto& feature_array = root.at("features");
    const json derived_workplane_array =
        root.contains("derived_workplanes") ? root.at("derived_workplanes") : json::array();

    std::vector<bool> added_sketch(sketch_array.size(), false);
    std::vector<bool> added_feature(feature_array.size(), false);
    std::vector<bool> added_workplane(derived_workplane_array.size(), false);
    std::size_t remaining = sketch_array.size() + feature_array.size() + derived_workplane_array.size();

    while (remaining > 0U) {
      bool progress = false;

      for (std::size_t index = 0; index < sketch_array.size(); ++index) {
        if (added_sketch[index]) {
          continue;
        }

        auto sketch = sketch_from_json(sketch_array.at(index));
        if (sketch.has_error()) {
          return Result<PartDocument>::failure(sketch.error());
        }

        if (!document.value().has_workplane_id(sketch.value().workplane())) {
          continue;
        }

        auto added = document.value().add_sketch(sketch.value());
        if (added.has_error()) {
          return Result<PartDocument>::failure(added.error());
        }

        added_sketch[index] = true;
        --remaining;
        progress = true;
      }

      for (std::size_t index = 0; index < feature_array.size(); ++index) {
        if (added_feature[index]) {
          continue;
        }

        auto feature = feature_from_json(feature_array.at(index));
        if (feature.has_error()) {
          return Result<PartDocument>::failure(feature.error());
        }

        if (document.value().find_sketch(feature.value().input_sketch()) == nullptr) {
          continue;
        }

        if (feature.value().type() == FeatureType::AdditiveExtrude &&
            document.value().find_parameter(feature.value().length_parameter()) == nullptr) {
          return Result<PartDocument>::failure(
              json_error("additive extrude length parameter must exist in part document"));
        }

        if (feature.value().type() == FeatureType::SubtractiveExtrude &&
            document.value().find_feature(feature.value().target_feature()) == nullptr) {
          continue;
        }

        auto added = document.value().add_feature(feature.value());
        if (added.has_error()) {
          return Result<PartDocument>::failure(added.error());
        }

        added_feature[index] = true;
        --remaining;
        progress = true;
      }

      for (std::size_t index = 0; index < derived_workplane_array.size(); ++index) {
        if (added_workplane[index]) {
          continue;
        }

        auto workplane = derived_workplane_from_json(derived_workplane_array.at(index));
        if (workplane.has_error()) {
          return Result<PartDocument>::failure(workplane.error());
        }

        if (document.value().find_feature(workplane.value().face_reference().source_feature()) == nullptr) {
          continue;
        }

        auto added = document.value().add_derived_workplane(workplane.value());
        if (added.has_error()) {
          return Result<PartDocument>::failure(added.error());
        }

        added_workplane[index] = true;
        --remaining;
        progress = true;
      }

      if (!progress) {
        return Result<PartDocument>::failure(
            json_error("could not resolve part document json dependencies"));
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

Result<std::uintmax_t> write_part_document_json_file(const PartDocument& document,
                                                     const std::filesystem::path& path) {
  if (path.empty()) {
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "part document json file path must not be empty"));
  }

  const auto serialized = serialize_part_document_to_json(document);
  if (serialized.has_error()) {
    return Result<std::uintmax_t>::failure(serialized.error());
  }

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not open part document json file for writing"));
  }

  output << serialized.value();
  output.flush();
  if (!output) {
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not write part document json file"));
  }

  output.close();
  if (!output) {
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not close part document json file after writing"));
  }

  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not stat written part document json file"));
  }

  return Result<std::uintmax_t>::success(size);
}

Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path) {
  if (path.empty()) {
    return Result<PartDocument>::failure(
        json_file_error(path, "part document json file path must not be empty"));
  }

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return Result<PartDocument>::failure(
        json_file_error(path, "could not open part document json file for reading"));
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return Result<PartDocument>::failure(
        json_file_error(path, "could not read part document json file"));
  }

  return deserialize_part_document_from_json(buffer.str());
}

} // namespace blcad
