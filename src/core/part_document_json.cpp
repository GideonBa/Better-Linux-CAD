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
  for (const auto& id : ids)
    result.push_back(id.value());
  return result;
}

[[nodiscard]] std::vector<ParameterId> parameter_ids_from_json(const json& values) {
  std::vector<ParameterId> ids;
  ids.reserve(values.size());
  for (const auto& value : values)
    ids.emplace_back(value.get<std::string>());
  return ids;
}

[[nodiscard]] json sketch_entity_ids_to_json(const std::vector<SketchEntityId>& ids) {
  json result = json::array();
  for (const auto& id : ids)
    result.push_back(id.value());
  return result;
}

[[nodiscard]] std::vector<SketchEntityId> sketch_entity_ids_from_json(const json& values) {
  std::vector<SketchEntityId> ids;
  ids.reserve(values.size());
  for (const auto& value : values)
    ids.emplace_back(value.get<std::string>());
  return ids;
}

[[nodiscard]] json
composite_inner_contours_to_json(const std::vector<std::vector<SketchEntityId>>& contours) {
  json result = json::array();
  for (const auto& contour : contours)
    result.push_back(sketch_entity_ids_to_json(contour));
  return result;
}

[[nodiscard]] std::vector<std::vector<SketchEntityId>>
composite_inner_contours_from_json(const json& values) {
  std::vector<std::vector<SketchEntityId>> contours;
  contours.reserve(values.size());
  for (const auto& value : values)
    contours.push_back(sketch_entity_ids_from_json(value));
  return contours;
}

[[nodiscard]] Result<SemanticFace> semantic_face_from_json(const json& value) {
  const auto face = value.get<std::string>();
  if (face == "top")
    return Result<SemanticFace>::success(SemanticFace::Top);
  if (face == "bottom")
    return Result<SemanticFace>::success(SemanticFace::Bottom);
  if (face == "right")
    return Result<SemanticFace>::success(SemanticFace::Right);
  if (face == "left")
    return Result<SemanticFace>::success(SemanticFace::Left);
  if (face == "front")
    return Result<SemanticFace>::success(SemanticFace::Front);
  if (face == "back")
    return Result<SemanticFace>::success(SemanticFace::Back);
  return Result<SemanticFace>::failure(
      json_error("unsupported semantic face in part document json"));
}

[[nodiscard]] Result<SemanticEdge> semantic_edge_from_json(const json& value) {
  const auto edge = value.get<std::string>();
  if (edge == "top_front")
    return Result<SemanticEdge>::success(SemanticEdge::TopFront);
  if (edge == "top_back")
    return Result<SemanticEdge>::success(SemanticEdge::TopBack);
  if (edge == "top_right")
    return Result<SemanticEdge>::success(SemanticEdge::TopRight);
  if (edge == "top_left")
    return Result<SemanticEdge>::success(SemanticEdge::TopLeft);
  if (edge == "bottom_front")
    return Result<SemanticEdge>::success(SemanticEdge::BottomFront);
  if (edge == "bottom_back")
    return Result<SemanticEdge>::success(SemanticEdge::BottomBack);
  if (edge == "bottom_right")
    return Result<SemanticEdge>::success(SemanticEdge::BottomRight);
  if (edge == "bottom_left")
    return Result<SemanticEdge>::success(SemanticEdge::BottomLeft);
  if (edge == "front_right")
    return Result<SemanticEdge>::success(SemanticEdge::FrontRight);
  if (edge == "front_left")
    return Result<SemanticEdge>::success(SemanticEdge::FrontLeft);
  if (edge == "back_right")
    return Result<SemanticEdge>::success(SemanticEdge::BackRight);
  if (edge == "back_left")
    return Result<SemanticEdge>::success(SemanticEdge::BackLeft);
  return Result<SemanticEdge>::failure(
      json_error("unsupported semantic edge in part document json"));
}

[[nodiscard]] Result<SemanticVertex> semantic_vertex_from_json(const json& value) {
  const auto vertex = value.get<std::string>();
  if (vertex == "top_front_right")
    return Result<SemanticVertex>::success(SemanticVertex::TopFrontRight);
  if (vertex == "top_front_left")
    return Result<SemanticVertex>::success(SemanticVertex::TopFrontLeft);
  if (vertex == "top_back_right")
    return Result<SemanticVertex>::success(SemanticVertex::TopBackRight);
  if (vertex == "top_back_left")
    return Result<SemanticVertex>::success(SemanticVertex::TopBackLeft);
  if (vertex == "bottom_front_right")
    return Result<SemanticVertex>::success(SemanticVertex::BottomFrontRight);
  if (vertex == "bottom_front_left")
    return Result<SemanticVertex>::success(SemanticVertex::BottomFrontLeft);
  if (vertex == "bottom_back_right")
    return Result<SemanticVertex>::success(SemanticVertex::BottomBackRight);
  if (vertex == "bottom_back_left")
    return Result<SemanticVertex>::success(SemanticVertex::BottomBackLeft);
  return Result<SemanticVertex>::failure(
      json_error("unsupported semantic vertex in part document json"));
}

[[nodiscard]] json semantic_edge_reference_to_json(const SemanticEdgeReference& reference) {
  return json{{"source_feature", reference.source_feature().value()},
              {"edge", std::string(to_string(reference.edge()))}};
}

[[nodiscard]] Result<SemanticEdgeReference> semantic_edge_reference_from_json(const json& value) {
  auto edge = semantic_edge_from_json(value.at("edge"));
  if (edge.has_error())
    return Result<SemanticEdgeReference>::failure(edge.error());
  return SemanticEdgeReference::create(FeatureId(value.at("source_feature").get<std::string>()),
                                       edge.value());
}

[[nodiscard]] json semantic_vertex_reference_to_json(const SemanticVertexReference& reference) {
  return json{{"source_feature", reference.source_feature().value()},
              {"vertex", std::string(to_string(reference.vertex()))}};
}

[[nodiscard]] Result<SemanticVertexReference>
semantic_vertex_reference_from_json(const json& value) {
  auto vertex = semantic_vertex_from_json(value.at("vertex"));
  if (vertex.has_error())
    return Result<SemanticVertexReference>::failure(vertex.error());
  return SemanticVertexReference::create(FeatureId(value.at("source_feature").get<std::string>()),
                                         vertex.value());
}

[[nodiscard]] json semantic_target_to_json(const SemanticReferenceTarget& target) {
  json result{{"kind", std::string(to_string(target.kind()))},
              {"source_feature", target.source_feature().value()}};
  if (target.kind() == SemanticReferenceKind::Face)
    result["face"] = std::string(to_string(target.face().value()));
  if (target.kind() == SemanticReferenceKind::Edge)
    result["edge"] = std::string(to_string(target.edge().value()));
  if (target.kind() == SemanticReferenceKind::Vertex)
    result["vertex"] = std::string(to_string(target.vertex().value()));
  return result;
}

[[nodiscard]] Result<SemanticReferenceTarget> semantic_target_from_json(const json& value) {
  const auto kind = value.at("kind").get<std::string>();
  const auto source = FeatureId(value.at("source_feature").get<std::string>());
  if (kind == "face") {
    auto face = semantic_face_from_json(value.at("face"));
    if (face.has_error())
      return Result<SemanticReferenceTarget>::failure(face.error());
    return SemanticReferenceTarget::create_face(source, face.value());
  }
  if (kind == "edge") {
    auto edge = semantic_edge_from_json(value.at("edge"));
    if (edge.has_error())
      return Result<SemanticReferenceTarget>::failure(edge.error());
    return SemanticReferenceTarget::create_edge(source, edge.value());
  }
  if (kind == "vertex") {
    auto vertex = semantic_vertex_from_json(value.at("vertex"));
    if (vertex.has_error())
      return Result<SemanticReferenceTarget>::failure(vertex.error());
    return SemanticReferenceTarget::create_vertex(source, vertex.value());
  }
  return Result<SemanticReferenceTarget>::failure(
      json_error("unsupported semantic reference target kind in part document json"));
}

[[nodiscard]] json reference_status_to_json(const ReferenceStatusRecord& status) {
  json result{{"id", status.id().value()},
              {"status", std::string(to_string(status.status()))},
              {"target", semantic_target_to_json(status.target())}};
  if (!status.message().empty())
    result["message"] = status.message();
  return result;
}

[[nodiscard]] Result<ReferenceStatusRecord> reference_status_from_json(const json& value) {
  auto target = semantic_target_from_json(value.at("target"));
  if (target.has_error())
    return Result<ReferenceStatusRecord>::failure(target.error());
  const auto status = value.at("status").get<std::string>();
  if (status == "resolved")
    return ReferenceStatusRecord::create_resolved(
        ReferenceStatusId(value.at("id").get<std::string>()), target.value());
  if (status == "lost")
    return ReferenceStatusRecord::create_lost(ReferenceStatusId(value.at("id").get<std::string>()),
                                              target.value(),
                                              value.at("message").get<std::string>());
  return Result<ReferenceStatusRecord>::failure(
      json_error("unsupported reference status in part document json"));
}

[[nodiscard]] json reference_remap_to_json(const ReferenceRemapRecord& remap) {
  return json{{"id", remap.id().value()},
              {"original", semantic_target_to_json(remap.original())},
              {"replacement", semantic_target_to_json(remap.replacement())},
              {"reason", remap.reason()}};
}

[[nodiscard]] Result<ReferenceRemapRecord> reference_remap_from_json(const json& value) {
  auto original = semantic_target_from_json(value.at("original"));
  if (original.has_error())
    return Result<ReferenceRemapRecord>::failure(original.error());
  auto replacement = semantic_target_from_json(value.at("replacement"));
  if (replacement.has_error())
    return Result<ReferenceRemapRecord>::failure(replacement.error());
  return ReferenceRemapRecord::create(ReferenceRemapId(value.at("id").get<std::string>()),
                                      original.value(), replacement.value(),
                                      value.at("reason").get<std::string>());
}

[[nodiscard]] json sketch_origin_override_to_json(const SketchOriginOverrideRecord& origin) {
  return json{{"sketch", origin.sketch().value()},
              {"local_origin", point2_to_json(origin.local_origin())}};
}

[[nodiscard]] Result<SketchOriginOverrideRecord>
sketch_origin_override_from_json(const json& value) {
  return SketchOriginOverrideRecord::create(SketchId(value.at("sketch").get<std::string>()),
                                            point2_from_json(value.at("local_origin")));
}

[[nodiscard]] Result<Quantity> length_quantity_from_json(const json& parameter_json,
                                                         const std::string& object_id) {
  if (parameter_json.at("unit").get<std::string>() != "mm")
    return Result<Quantity>::failure(json_error("only millimeter length parameters are supported"));
  return Quantity::length_mm(parameter_json.at("value").get<double>(), object_id);
}

[[nodiscard]] Result<Quantity> count_quantity_from_json(const json& parameter_json,
                                                        const std::string& object_id) {
  if (parameter_json.at("unit").get<std::string>() != "1")
    return Result<Quantity>::failure(json_error("count parameters must use unit \"1\""));
  return Quantity::count(parameter_json.at("value").get<double>(), object_id);
}

[[nodiscard]] Result<Parameter> parameter_from_json(const json& parameter_json) {
  const auto id = parameter_json.at("id").get<std::string>();
  const auto type = parameter_json.at("type").get<std::string>();
  if (type != "length" && type != "count")
    return Result<Parameter>::failure(json_error("only length and count parameters are supported"));
  if (parameter_json.at("scope").get<std::string>() != "part")
    return Result<Parameter>::failure(json_error("only part-scope parameters are supported"));
  if (type == "count") {
    auto quantity = count_quantity_from_json(parameter_json, id);
    if (quantity.has_error())
      return Result<Parameter>::failure(quantity.error());
    return Parameter::create_count(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                   quantity.value());
  }
  auto quantity = length_quantity_from_json(parameter_json, id);
  if (quantity.has_error())
    return Result<Parameter>::failure(quantity.error());
  return Parameter::create_length(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                  quantity.value());
}

[[nodiscard]] Result<DatumPlane> datum_plane_from_json(const json& datum_plane_json) {
  if (datum_plane_json.at("kind").get<std::string>() != "xy")
    return Result<DatumPlane>::failure(json_error("only XY datum planes are supported"));
  return DatumPlane::xy(DatumPlaneId(datum_plane_json.at("id").get<std::string>()),
                        datum_plane_json.at("name").get<std::string>());
}

[[nodiscard]] std::vector<ParameterId> parameter_dependencies_from_object(const json& value) {
  if (!value.contains("parameter_dependencies"))
    return {};
  return parameter_ids_from_json(value.at("parameter_dependencies"));
}

[[nodiscard]] json construction_relation_to_json(const ConstructionRelation& relation) {
  json relation_json{{"id", relation.id().value()},
                     {"type", std::string(to_string(relation.type()))}};
  switch (relation.type()) {
  case ConstructionRelationType::PlaneOffsetFromPlane:
    relation_json["source_plane"] = relation.source_plane().value();
    relation_json["offset_parameter"] = relation.offset_parameter().value();
    break;
  case ConstructionRelationType::LineThroughTwoPoints:
    relation_json["first_point"] = relation.first_point().value();
    relation_json["second_point"] = relation.second_point().value();
    break;
  case ConstructionRelationType::PlaneThroughThreePoints:
    relation_json["first_point"] = relation.first_point().value();
    relation_json["second_point"] = relation.second_point().value();
    relation_json["third_point"] = relation.third_point().value();
    break;
  case ConstructionRelationType::PointOnPlane:
    relation_json["point"] = relation.first_point().value();
    relation_json["plane"] = relation.source_plane().value();
    break;
  case ConstructionRelationType::PointOnLine:
    relation_json["point"] = relation.first_point().value();
    relation_json["line"] = relation.source_line().value();
    break;
  case ConstructionRelationType::PointOnGeneratedEdge:
    relation_json["point"] = relation.first_point().value();
    relation_json["generated_edge"] =
        semantic_edge_reference_to_json(relation.generated_edge().value());
    break;
  case ConstructionRelationType::PointOnGeneratedVertex:
    relation_json["point"] = relation.first_point().value();
    relation_json["generated_vertex"] =
        semantic_vertex_reference_to_json(relation.generated_vertex().value());
    break;
  case ConstructionRelationType::LineOnPlane:
    relation_json["line"] = relation.source_line().value();
    relation_json["plane"] = relation.source_plane().value();
    break;
  case ConstructionRelationType::PlaneParallelToPlaneThroughPoint:
    relation_json["source_plane"] = relation.source_plane().value();
    relation_json["through_point"] = relation.first_point().value();
    break;
  case ConstructionRelationType::LineParallelToLineThroughPoint:
    relation_json["source_line"] = relation.source_line().value();
    relation_json["through_point"] = relation.first_point().value();
    break;
  case ConstructionRelationType::LineParallelToGeneratedEdgeThroughPoint:
    relation_json["generated_edge"] =
        semantic_edge_reference_to_json(relation.generated_edge().value());
    relation_json["through_point"] = relation.first_point().value();
    break;
  }
  return relation_json;
}

[[nodiscard]] Result<ConstructionRelation>
construction_relation_from_json(const json& relation_json) {
  const auto type = relation_json.at("type").get<std::string>();
  const auto id = ConstructionRelationId(relation_json.at("id").get<std::string>());
  if (type == "plane_offset_from_plane")
    return ConstructionRelation::create_plane_offset_from_plane(
        id, DatumPlaneId(relation_json.at("source_plane").get<std::string>()),
        ParameterId(relation_json.at("offset_parameter").get<std::string>()));
  if (type == "line_through_two_points")
    return ConstructionRelation::create_line_through_two_points(
        id, ConstructionPointId(relation_json.at("first_point").get<std::string>()),
        ConstructionPointId(relation_json.at("second_point").get<std::string>()));
  if (type == "plane_through_three_points")
    return ConstructionRelation::create_plane_through_three_points(
        id, ConstructionPointId(relation_json.at("first_point").get<std::string>()),
        ConstructionPointId(relation_json.at("second_point").get<std::string>()),
        ConstructionPointId(relation_json.at("third_point").get<std::string>()));
  if (type == "point_on_plane")
    return ConstructionRelation::create_point_on_plane(
        id, ConstructionPointId(relation_json.at("point").get<std::string>()),
        DatumPlaneId(relation_json.at("plane").get<std::string>()));
  if (type == "point_on_line")
    return ConstructionRelation::create_point_on_line(
        id, ConstructionPointId(relation_json.at("point").get<std::string>()),
        ConstructionLineId(relation_json.at("line").get<std::string>()));
  if (type == "point_on_generated_edge") {
    auto edge = semantic_edge_reference_from_json(relation_json.at("generated_edge"));
    if (edge.has_error())
      return Result<ConstructionRelation>::failure(edge.error());
    return ConstructionRelation::create_point_on_generated_edge(
        id, ConstructionPointId(relation_json.at("point").get<std::string>()), edge.value());
  }
  if (type == "point_on_generated_vertex") {
    auto vertex = semantic_vertex_reference_from_json(relation_json.at("generated_vertex"));
    if (vertex.has_error())
      return Result<ConstructionRelation>::failure(vertex.error());
    return ConstructionRelation::create_point_on_generated_vertex(
        id, ConstructionPointId(relation_json.at("point").get<std::string>()), vertex.value());
  }
  if (type == "line_on_plane")
    return ConstructionRelation::create_line_on_plane(
        id, ConstructionLineId(relation_json.at("line").get<std::string>()),
        DatumPlaneId(relation_json.at("plane").get<std::string>()));
  if (type == "plane_parallel_to_plane_through_point")
    return ConstructionRelation::create_plane_parallel_to_plane_through_point(
        id, DatumPlaneId(relation_json.at("source_plane").get<std::string>()),
        ConstructionPointId(relation_json.at("through_point").get<std::string>()));
  if (type == "line_parallel_to_line_through_point")
    return ConstructionRelation::create_line_parallel_to_line_through_point(
        id, ConstructionLineId(relation_json.at("source_line").get<std::string>()),
        ConstructionPointId(relation_json.at("through_point").get<std::string>()));
  if (type == "line_parallel_to_generated_edge_through_point") {
    auto edge = semantic_edge_reference_from_json(relation_json.at("generated_edge"));
    if (edge.has_error())
      return Result<ConstructionRelation>::failure(edge.error());
    return ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
        id, edge.value(),
        ConstructionPointId(relation_json.at("through_point").get<std::string>()));
  }
  return Result<ConstructionRelation>::failure(
      json_error("unsupported construction relation type in part document json"));
}

[[nodiscard]] Result<ConstructionPoint> construction_point_from_json(const json& point_json) {
  const auto kind = point_json.at("kind").get<std::string>();
  if (kind == "explicit")
    return ConstructionPoint::create_explicit(
        ConstructionPointId(point_json.at("id").get<std::string>()),
        point_json.at("name").get<std::string>(), point3_from_json(point_json.at("position")),
        parameter_dependencies_from_object(point_json));
  auto relation = construction_relation_from_json(point_json.at("relation"));
  if (relation.has_error())
    return Result<ConstructionPoint>::failure(relation.error());
  if (kind == "on_generated_edge")
    return ConstructionPoint::create_on_generated_edge(
        ConstructionPointId(point_json.at("id").get<std::string>()),
        point_json.at("name").get<std::string>(), relation.value());
  if (kind == "on_generated_vertex")
    return ConstructionPoint::create_on_generated_vertex(
        ConstructionPointId(point_json.at("id").get<std::string>()),
        point_json.at("name").get<std::string>(), relation.value());
  return Result<ConstructionPoint>::failure(
      json_error("unsupported construction point kind in part document json"));
}

[[nodiscard]] Result<ConstructionLine> construction_line_from_json(const json& line_json) {
  const auto kind = line_json.at("kind").get<std::string>();
  if (kind == "explicit")
    return ConstructionLine::create_explicit(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), point3_from_json(line_json.at("point")),
        vector3_from_json(line_json.at("direction")),
        parameter_dependencies_from_object(line_json));
  auto relation = construction_relation_from_json(line_json.at("relation"));
  if (relation.has_error())
    return Result<ConstructionLine>::failure(relation.error());
  if (kind == "through_two_points")
    return ConstructionLine::create_through_two_points(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), relation.value());
  if (kind == "parallel_to_line_through_point")
    return ConstructionLine::create_parallel_to_line_through_point(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), relation.value());
  if (kind == "parallel_to_generated_edge_through_point")
    return ConstructionLine::create_parallel_to_generated_edge_through_point(
        ConstructionLineId(line_json.at("id").get<std::string>()),
        line_json.at("name").get<std::string>(), relation.value());
  return Result<ConstructionLine>::failure(
      json_error("unsupported construction line kind in part document json"));
}

[[nodiscard]] Result<ConstructionPlane> construction_plane_from_json(const json& plane_json) {
  const auto kind = plane_json.at("kind").get<std::string>();
  if (kind == "explicit")
    return ConstructionPlane::create_explicit(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), point3_from_json(plane_json.at("origin")),
        vector3_from_json(plane_json.at("x_axis")), vector3_from_json(plane_json.at("y_axis")),
        vector3_from_json(plane_json.at("normal")), parameter_dependencies_from_object(plane_json));
  auto relation = construction_relation_from_json(plane_json.at("relation"));
  if (relation.has_error())
    return Result<ConstructionPlane>::failure(relation.error());
  if (kind == "offset_from_plane")
    return ConstructionPlane::create_offset_from_plane(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), relation.value());
  if (kind == "through_three_points")
    return ConstructionPlane::create_through_three_points(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), relation.value());
  if (kind == "parallel_to_plane_through_point")
    return ConstructionPlane::create_parallel_to_plane_through_point(
        ConstructionPlaneId(plane_json.at("id").get<std::string>()),
        plane_json.at("name").get<std::string>(), relation.value());
  return Result<ConstructionPlane>::failure(
      json_error("unsupported construction plane kind in part document json"));
}

[[nodiscard]] Result<DerivedWorkplane>
derived_workplane_from_json(const json& derived_workplane_json) {
  if (derived_workplane_json.at("kind").get<std::string>() != "feature_face")
    return Result<DerivedWorkplane>::failure(
        json_error("only feature-face derived workplanes are supported"));
  auto face = semantic_face_from_json(derived_workplane_json.at("face"));
  if (face.has_error())
    return Result<DerivedWorkplane>::failure(face.error());
  auto reference = SemanticFaceReference::create(
      FeatureId(derived_workplane_json.at("source_feature").get<std::string>()), face.value());
  if (reference.has_error())
    return Result<DerivedWorkplane>::failure(reference.error());
  return DerivedWorkplane::create_on_feature_face(
      DatumPlaneId(derived_workplane_json.at("id").get<std::string>()),
      derived_workplane_json.at("name").get<std::string>(), reference.value());
}

[[nodiscard]] Result<ProjectedSketchPoint> projected_point_from_json(const json& point_json) {
  const auto source = point_json.at("source").get<std::string>();
  const auto id = SketchEntityId(point_json.at("id").get<std::string>());
  if (source == "construction_point")
    return ProjectedSketchPoint::create_from_construction_point(
        id, ConstructionPointId(point_json.at("construction_point").get<std::string>()));
  if (source == "semantic_vertex") {
    auto vertex = semantic_vertex_reference_from_json(point_json.at("semantic_vertex"));
    if (vertex.has_error())
      return Result<ProjectedSketchPoint>::failure(vertex.error());
    return ProjectedSketchPoint::create_from_semantic_vertex(id, vertex.value());
  }
  return Result<ProjectedSketchPoint>::failure(
      json_error("unsupported projected sketch point source in part document json"));
}

[[nodiscard]] Result<ProjectedSketchLine> projected_line_from_json(const json& line_json) {
  const auto source = line_json.at("source").get<std::string>();
  const auto id = SketchEntityId(line_json.at("id").get<std::string>());
  if (source == "construction_line")
    return ProjectedSketchLine::create_from_construction_line(
        id, ConstructionLineId(line_json.at("construction_line").get<std::string>()));
  if (source == "semantic_edge") {
    auto edge = semantic_edge_reference_from_json(line_json.at("semantic_edge"));
    if (edge.has_error())
      return Result<ProjectedSketchLine>::failure(edge.error());
    return ProjectedSketchLine::create_from_semantic_edge(id, edge.value());
  }
  return Result<ProjectedSketchLine>::failure(
      json_error("unsupported projected sketch line source in part document json"));
}

[[nodiscard]] Result<ReferenceGeneratedLine>
reference_generated_line_from_json(const json& line_json) {
  const auto id = SketchEntityId(line_json.at("id").get<std::string>());
  const auto start = SketchConstraintId(line_json.at("start_constraint").get<std::string>());
  const auto end = SketchConstraintId(line_json.at("end_constraint").get<std::string>());
  if (line_json.contains("direction_constraint"))
    return ReferenceGeneratedLine::create_with_projected_line_direction(
        id, start, end,
        SketchConstraintId(line_json.at("direction_constraint").get<std::string>()));
  return ReferenceGeneratedLine::create_from_projected_point_constraints(id, start, end);
}

[[nodiscard]] json reference_generated_line_to_json(const ReferenceGeneratedLine& line) {
  json result{{"id", line.id().value()},
              {"start_constraint", line.start_constraint().value()},
              {"end_constraint", line.end_constraint().value()}};
  if (line.direction_constraint().has_value())
    result["direction_constraint"] = line.direction_constraint().value().value();
  return result;
}

[[nodiscard]] Result<ArcSegment> arc_segment_from_json(const json& arc_json) {
  return ArcSegment::create_three_point(
      SketchEntityId(arc_json.at("id").get<std::string>()), point2_from_json(arc_json.at("start")),
      point2_from_json(arc_json.at("mid")), point2_from_json(arc_json.at("end")));
}

[[nodiscard]] json arc_segment_to_json(const ArcSegment& arc) {
  return json{{"id", arc.id().value()},
              {"start", point2_to_json(arc.start())},
              {"mid", point2_to_json(arc.mid())},
              {"end", point2_to_json(arc.end())}};
}

[[nodiscard]] Result<SplineSegment> spline_segment_from_json(const json& spline_json) {
  if (spline_json.value("kind", std::string("cubic_bezier")) != "cubic_bezier") {
    return Result<SplineSegment>::failure(
        json_error("only cubic_bezier spline segments are supported"));
  }
  return SplineSegment::create_cubic_bezier(
      SketchEntityId(spline_json.at("id").get<std::string>()),
      point2_from_json(spline_json.at("start")), point2_from_json(spline_json.at("control1")),
      point2_from_json(spline_json.at("control2")), point2_from_json(spline_json.at("end")));
}

[[nodiscard]] json spline_segment_to_json(const SplineSegment& spline) {
  return json{{"id", spline.id().value()},
              {"kind", "cubic_bezier"},
              {"start", point2_to_json(spline.start())},
              {"control1", point2_to_json(spline.control1())},
              {"control2", point2_to_json(spline.control2())},
              {"end", point2_to_json(spline.end())}};
}

[[nodiscard]] Result<SketchTrimExtendOperation>
trim_extend_operation_from_json(const json& operation_json) {
  const auto kind = operation_json.at("kind").get<std::string>();
  const auto id = SketchTrimOperationId(operation_json.at("id").get<std::string>());
  const auto target = SketchEntityId(operation_json.at("target_entity").get<std::string>());
  const auto replacement = point2_from_json(operation_json.at("replacement_endpoint"));
  if (kind == "trim")
    return SketchTrimExtendOperation::create_trim(id, target, replacement);
  if (kind == "extend")
    return SketchTrimExtendOperation::create_extend(id, target, replacement);
  return Result<SketchTrimExtendOperation>::failure(
      json_error("unsupported sketch trim/extend operation kind in part document json"));
}

[[nodiscard]] json trim_extend_operation_to_json(const SketchTrimExtendOperation& operation) {
  return json{{"id", operation.id().value()},
              {"kind", std::string(to_string(operation.kind()))},
              {"target_entity", operation.target_entity().value()},
              {"replacement_endpoint", point2_to_json(operation.replacement_endpoint())}};
}

[[nodiscard]] Result<SketchTangentContinuity>
tangent_continuity_from_json(const json& continuity_json) {
  const auto kind = continuity_json.at("kind").get<std::string>();
  if (kind != "tangent") {
    return Result<SketchTangentContinuity>::failure(
        json_error("unsupported sketch tangent continuity kind in part document json"));
  }
  return SketchTangentContinuity::create_tangent(
      SketchConstraintId(continuity_json.at("id").get<std::string>()),
      SketchEntityId(continuity_json.at("first_entity").get<std::string>()),
      SketchEntityId(continuity_json.at("second_entity").get<std::string>()));
}

[[nodiscard]] json tangent_continuity_to_json(const SketchTangentContinuity& continuity) {
  return json{{"id", continuity.id().value()},
              {"kind", std::string(to_string(continuity.kind()))},
              {"first_entity", continuity.first_entity().value()},
              {"second_entity", continuity.second_entity().value()}};
}

[[nodiscard]] Result<SketchReferenceTarget> reference_target_from_json(const json& target_json) {
  const auto kind = target_json.at("kind").get<std::string>();
  const auto entity = SketchEntityId(target_json.at("entity").get<std::string>());
  if (kind == "line_segment")
    return SketchReferenceTarget::create_line_segment(entity);
  if (kind == "line_segment_start")
    return SketchReferenceTarget::create_line_segment_start(entity);
  if (kind == "line_segment_end")
    return SketchReferenceTarget::create_line_segment_end(entity);
  if (kind == "projected_point")
    return SketchReferenceTarget::create_projected_point(entity);
  if (kind == "projected_line")
    return SketchReferenceTarget::create_projected_line(entity);
  return Result<SketchReferenceTarget>::failure(
      json_error("unsupported sketch reference target kind in part document json"));
}

[[nodiscard]] json reference_target_to_json(const SketchReferenceTarget& target) {
  return json{{"kind", std::string(to_string(target.kind()))}, {"entity", target.entity().value()}};
}

[[nodiscard]] Result<SketchConstraint> sketch_constraint_from_json(const json& constraint_json) {
  const auto kind = constraint_json.at("kind").get<std::string>();
  const auto id = SketchConstraintId(constraint_json.at("id").get<std::string>());
  auto constrained = reference_target_from_json(constraint_json.at("constrained"));
  if (constrained.has_error())
    return Result<SketchConstraint>::failure(constrained.error());
  auto reference = reference_target_from_json(constraint_json.at("reference"));
  if (reference.has_error())
    return Result<SketchConstraint>::failure(reference.error());
  if (kind == "coincident_to_projected_point")
    return SketchConstraint::create_coincident_to_projected_point(id, constrained.value(),
                                                                  reference.value());
  if (kind == "parallel_to_projected_line")
    return SketchConstraint::create_parallel_to_projected_line(id, constrained.value(),
                                                               reference.value());
  if (kind == "collinear_with_projected_line")
    return SketchConstraint::create_collinear_with_projected_line(id, constrained.value(),
                                                                  reference.value());
  return Result<SketchConstraint>::failure(
      json_error("unsupported sketch constraint kind in part document json"));
}

[[nodiscard]] json sketch_constraint_to_json(const SketchConstraint& constraint) {
  return json{{"id", constraint.id().value()},
              {"kind", std::string(to_string(constraint.kind()))},
              {"constrained", reference_target_to_json(constraint.constrained_target())},
              {"reference", reference_target_to_json(constraint.reference_target())}};
}

[[nodiscard]] Result<SketchGeometricConstraint>
geometric_constraint_from_json(const json& constraint_json) {
  const auto kind = constraint_json.at("kind").get<std::string>();
  const auto id = SketchConstraintId(constraint_json.at("id").get<std::string>());
  auto first = reference_target_from_json(constraint_json.at("first"));
  if (first.has_error())
    return Result<SketchGeometricConstraint>::failure(first.error());
  if (kind == "fixed")
    return SketchGeometricConstraint::create_fixed(id, first.value());
  if (kind == "horizontal")
    return SketchGeometricConstraint::create_horizontal(id, first.value());
  if (kind == "vertical")
    return SketchGeometricConstraint::create_vertical(id, first.value());
  auto second = reference_target_from_json(constraint_json.at("second"));
  if (second.has_error())
    return Result<SketchGeometricConstraint>::failure(second.error());
  if (kind == "parallel")
    return SketchGeometricConstraint::create_parallel(id, first.value(), second.value());
  if (kind == "perpendicular")
    return SketchGeometricConstraint::create_perpendicular(id, first.value(), second.value());
  if (kind == "equal_length")
    return SketchGeometricConstraint::create_equal_length(id, first.value(), second.value());
  return Result<SketchGeometricConstraint>::failure(
      json_error("unsupported sketch geometric constraint kind in part document json"));
}

[[nodiscard]] json geometric_constraint_to_json(const SketchGeometricConstraint& constraint) {
  json result{{"id", constraint.id().value()},
              {"kind", std::string(to_string(constraint.kind()))},
              {"first", reference_target_to_json(constraint.first_target())}};
  if (constraint.second_target().has_value())
    result["second"] = reference_target_to_json(constraint.second_target().value());
  return result;
}

[[nodiscard]] Result<SketchDrivingDimension>
driving_dimension_from_json(const json& dimension_json) {
  const auto kind = dimension_json.at("kind").get<std::string>();
  const auto id = SketchDimensionId(dimension_json.at("id").get<std::string>());
  auto first = reference_target_from_json(dimension_json.at("first"));
  if (first.has_error())
    return Result<SketchDrivingDimension>::failure(first.error());
  auto second = reference_target_from_json(dimension_json.at("second"));
  if (second.has_error())
    return Result<SketchDrivingDimension>::failure(second.error());
  const auto parameter = ParameterId(dimension_json.at("parameter").get<std::string>());
  if (kind == "horizontal_distance")
    return SketchDrivingDimension::create_horizontal_distance(id, first.value(), second.value(),
                                                              parameter);
  if (kind == "vertical_distance")
    return SketchDrivingDimension::create_vertical_distance(id, first.value(), second.value(),
                                                            parameter);
  if (kind == "aligned_distance")
    return SketchDrivingDimension::create_aligned_distance(id, first.value(), second.value(),
                                                           parameter);
  if (kind == "point_to_point_distance")
    return SketchDrivingDimension::create_point_to_point_distance(id, first.value(), second.value(),
                                                                  parameter);
  return Result<SketchDrivingDimension>::failure(
      json_error("unsupported sketch driving dimension kind in part document json"));
}

[[nodiscard]] json driving_dimension_to_json(const SketchDrivingDimension& dimension) {
  return json{{"id", dimension.id().value()},
              {"kind", std::string(to_string(dimension.kind()))},
              {"first", reference_target_to_json(dimension.first_target())},
              {"second", reference_target_to_json(dimension.second_target())},
              {"parameter", dimension.parameter().value()}};
}

[[nodiscard]] json projected_point_to_json(const ProjectedSketchPoint& point) {
  json point_json{{"id", point.id().value()}, {"source", std::string(to_string(point.source()))}};
  if (point.source() == ProjectedSketchPointSource::ConstructionPoint)
    point_json["construction_point"] = point.construction_point().value();
  else
    point_json["semantic_vertex"] =
        semantic_vertex_reference_to_json(point.semantic_vertex().value());
  return point_json;
}

[[nodiscard]] json projected_line_to_json(const ProjectedSketchLine& line) {
  json line_json{{"id", line.id().value()}, {"source", std::string(to_string(line.source()))}};
  if (line.source() == ProjectedSketchLineSource::ConstructionLine)
    line_json["construction_line"] = line.construction_line().value();
  else
    line_json["semantic_edge"] = semantic_edge_reference_to_json(line.semantic_edge().value());
  return line_json;
}

[[nodiscard]] Result<Sketch> sketch_from_json(const json& sketch_json) {
  auto sketch = Sketch::create(SketchId(sketch_json.at("id").get<std::string>()),
                               sketch_json.at("name").get<std::string>(),
                               DatumPlaneId(sketch_json.at("workplane").get<std::string>()));
  if (sketch.has_error())
    return sketch;

  for (const auto& line_json : sketch_json.value("line_segments", json::array())) {
    auto line = LineSegment::create(SketchEntityId(line_json.at("id").get<std::string>()),
                                    point2_from_json(line_json.at("start")),
                                    point2_from_json(line_json.at("end")));
    if (line.has_error())
      return Result<Sketch>::failure(line.error());
    auto added = sketch.value().add_entity(line.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& arc_json : sketch_json.value("arc_segments", json::array())) {
    auto arc = arc_segment_from_json(arc_json);
    if (arc.has_error())
      return Result<Sketch>::failure(arc.error());
    auto added = sketch.value().add_entity(arc.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& spline_json : sketch_json.value("spline_segments", json::array())) {
    auto spline = spline_segment_from_json(spline_json);
    if (spline.has_error())
      return Result<Sketch>::failure(spline.error());
    auto added = sketch.value().add_entity(spline.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& point_json : sketch_json.value("projected_points", json::array())) {
    auto point = projected_point_from_json(point_json);
    if (point.has_error())
      return Result<Sketch>::failure(point.error());
    auto added = sketch.value().add_reference(point.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& line_json : sketch_json.value("projected_lines", json::array())) {
    auto line = projected_line_from_json(line_json);
    if (line.has_error())
      return Result<Sketch>::failure(line.error());
    auto added = sketch.value().add_reference(line.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& line_json : sketch_json.value("reference_generated_lines", json::array())) {
    auto line = reference_generated_line_from_json(line_json);
    if (line.has_error())
      return Result<Sketch>::failure(line.error());
    auto added = sketch.value().add_reference(line.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& operation_json : sketch_json.value("trim_extend_operations", json::array())) {
    auto operation = trim_extend_operation_from_json(operation_json);
    if (operation.has_error())
      return Result<Sketch>::failure(operation.error());
    auto added = sketch.value().add_trim_extend_operation(operation.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& continuity_json : sketch_json.value("tangent_continuities", json::array())) {
    auto continuity = tangent_continuity_from_json(continuity_json);
    if (continuity.has_error())
      return Result<Sketch>::failure(continuity.error());
    auto added = sketch.value().add_tangent_continuity(continuity.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint_json : sketch_json.value("constraints", json::array())) {
    auto constraint = sketch_constraint_from_json(constraint_json);
    if (constraint.has_error())
      return Result<Sketch>::failure(constraint.error());
    auto added = sketch.value().add_constraint(constraint.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint_json : sketch_json.value("geometric_constraints", json::array())) {
    auto constraint = geometric_constraint_from_json(constraint_json);
    if (constraint.has_error())
      return Result<Sketch>::failure(constraint.error());
    auto added = sketch.value().add_constraint(constraint.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& dimension_json : sketch_json.value("driving_dimensions", json::array())) {
    auto dimension = driving_dimension_from_json(dimension_json);
    if (dimension.has_error())
      return Result<Sketch>::failure(dimension.error());
    auto added = sketch.value().add_dimension(dimension.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& rectangle_json : sketch_json.value("rectangle_profiles", json::array())) {
    auto rectangle = RectangleProfile::create(
        ProfileId(rectangle_json.at("id").get<std::string>()),
        ParameterId(rectangle_json.at("width_parameter").get<std::string>()),
        ParameterId(rectangle_json.at("height_parameter").get<std::string>()),
        point2_from_json(rectangle_json.at("center")));
    if (rectangle.has_error())
      return Result<Sketch>::failure(rectangle.error());
    auto added = sketch.value().add_profile(rectangle.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& circle_json : sketch_json.value("circle_profiles", json::array())) {
    auto circle =
        CircleProfile::create(ProfileId(circle_json.at("id").get<std::string>()),
                              ParameterId(circle_json.at("diameter_parameter").get<std::string>()),
                              point2_from_json(circle_json.at("center")));
    if (circle.has_error())
      return Result<Sketch>::failure(circle.error());
    auto added = sketch.value().add_profile(circle.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile_json : sketch_json.value("closed_profiles", json::array())) {
    auto profile =
        ClosedProfile::create(ProfileId(profile_json.at("id").get<std::string>()),
                              sketch_entity_ids_from_json(profile_json.at("line_segments")));
    if (profile.has_error())
      return Result<Sketch>::failure(profile.error());
    auto added = sketch.value().add_profile(profile.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile_json : sketch_json.value("arc_closed_profiles", json::array())) {
    auto profile =
        ArcClosedProfile::create(ProfileId(profile_json.at("id").get<std::string>()),
                                 sketch_entity_ids_from_json(profile_json.at("curve_segments")));
    if (profile.has_error())
      return Result<Sketch>::failure(profile.error());
    auto added = sketch.value().add_profile(profile.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile_json : sketch_json.value("composite_closed_profiles", json::array())) {
    auto profile = CompositeClosedProfile::create(
        ProfileId(profile_json.at("id").get<std::string>()),
        sketch_entity_ids_from_json(profile_json.at("outer_contour")),
        composite_inner_contours_from_json(profile_json.at("inner_contours")));
    if (profile.has_error())
      return Result<Sketch>::failure(profile.error());
    auto added = sketch.value().add_profile(profile.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& pattern_json : sketch_json.value("circular_hole_patterns", json::array())) {
    auto pattern = CircularHolePattern::create(
        ProfileId(pattern_json.at("id").get<std::string>()),
        ParameterId(pattern_json.at("radius_parameter").get<std::string>()),
        ParameterId(pattern_json.at("count_parameter").get<std::string>()),
        ParameterId(pattern_json.at("hole_diameter_parameter").get<std::string>()),
        point2_from_json(pattern_json.at("center")), pattern_json.value("angle_offset_deg", 0.0));
    if (pattern.has_error())
      return Result<Sketch>::failure(pattern.error());
    auto added = sketch.value().add_profile(pattern.value());
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  return sketch;
}

[[nodiscard]] Result<ExtrudeDirection> extrude_direction_from_json(const json& value) {
  const auto direction = value.get<std::string>();
  if (direction == "+Z" || direction == "sketch_normal")
    return Result<ExtrudeDirection>::success(ExtrudeDirection::SketchNormal);
  if (direction == "opposite_sketch_normal")
    return Result<ExtrudeDirection>::success(ExtrudeDirection::OppositeSketchNormal);
  return Result<ExtrudeDirection>::failure(
      json_error("unsupported extrude direction in part document json"));
}

[[nodiscard]] Result<Feature> feature_from_json(const json& feature_json) {
  const auto type = feature_json.at("type").get<std::string>();
  auto direction = extrude_direction_from_json(feature_json.at("direction"));
  if (direction.has_error())
    return Result<Feature>::failure(direction.error());
  if (type == "additive_extrude")
    return Feature::create_additive_extrude(
        FeatureId(feature_json.at("id").get<std::string>()),
        feature_json.at("name").get<std::string>(),
        SketchId(feature_json.at("input_sketch").get<std::string>()),
        ParameterId(feature_json.at("length_parameter").get<std::string>()), direction.value());
  if (type == "subtractive_extrude") {
    if (feature_json.at("depth").get<std::string>() != "through_all")
      return Result<Feature>::failure(
          json_error("only through_all subtractive extrude depth is supported"));
    return Feature::create_subtractive_extrude(
        FeatureId(feature_json.at("id").get<std::string>()),
        feature_json.at("name").get<std::string>(),
        SketchId(feature_json.at("input_sketch").get<std::string>()),
        FeatureId(feature_json.at("target_feature").get<std::string>()),
        SubtractiveExtrudeDepth::ThroughAll, direction.value());
  }
  return Result<Feature>::failure(json_error("unsupported feature type in part document json"));
}

[[nodiscard]] bool sketch_dependencies_available(const PartDocument& document,
                                                 const Sketch& sketch) {
  if (!document.has_workplane_id(sketch.workplane()))
    return false;
  for (const auto& point : sketch.projected_points()) {
    if (point.source() == ProjectedSketchPointSource::ConstructionPoint &&
        document.find_construction_point(point.construction_point()) == nullptr)
      return false;
    if (point.source() == ProjectedSketchPointSource::SemanticVertex &&
        document.find_feature(point.semantic_vertex().value().source_feature()) == nullptr)
      return false;
  }
  for (const auto& line : sketch.projected_lines()) {
    if (line.source() == ProjectedSketchLineSource::ConstructionLine &&
        document.find_construction_line(line.construction_line()) == nullptr)
      return false;
    if (line.source() == ProjectedSketchLineSource::SemanticEdge &&
        document.find_feature(line.semantic_edge().value().source_feature()) == nullptr)
      return false;
  }
  for (const auto& dimension : sketch.driving_dimensions()) {
    if (document.find_parameter(dimension.parameter()) == nullptr)
      return false;
  }
  return true;
}

} // namespace

Result<std::string> serialize_part_document_to_json(const PartDocument& document) {
  json root;
  root["schema"] = k_schema;
  root["version"] = k_version;
  root["document"] = json{{"id", document.id().value()}, {"name", document.name()}};
  root["parameters"] = json::array();
  for (const auto& parameter : document.parameters())
    root["parameters"].push_back(json{{"id", parameter.id().value()},
                                      {"name", parameter.name()},
                                      {"type", std::string(to_string(parameter.type()))},
                                      {"scope", std::string(to_string(parameter.scope()))},
                                      {"unit", std::string(parameter.value().unit())},
                                      {"value", parameter.value().millimeters()}});
  root["datum_planes"] = json::array();
  for (const auto& datum_plane : document.datum_planes())
    root["datum_planes"].push_back(json{{"id", datum_plane.id().value()},
                                        {"name", datum_plane.name()},
                                        {"kind", "xy"},
                                        {"origin", point3_to_json(datum_plane.origin())},
                                        {"x_axis", vector3_to_json(datum_plane.x_axis())},
                                        {"y_axis", vector3_to_json(datum_plane.y_axis())},
                                        {"normal", vector3_to_json(datum_plane.normal())}});
  root["construction_points"] = json::array();
  for (const auto& point : document.construction_points()) {
    json point_json{{"id", point.id().value()},
                    {"name", point.name()},
                    {"kind", std::string(to_string(point.kind()))}};
    if (point.kind() == ConstructionPointKind::Explicit) {
      point_json["position"] = point3_to_json(point.position());
      point_json["parameter_dependencies"] = parameter_ids_to_json(point.parameter_dependencies());
    } else if (point.relation().has_value())
      point_json["relation"] = construction_relation_to_json(point.relation().value());
    root["construction_points"].push_back(std::move(point_json));
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
    } else if (line.relation().has_value())
      line_json["relation"] = construction_relation_to_json(line.relation().value());
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
    } else if (plane.relation().has_value())
      plane_json["relation"] = construction_relation_to_json(plane.relation().value());
    root["construction_planes"].push_back(std::move(plane_json));
  }
  root["derived_workplanes"] = json::array();
  for (const auto& workplane : document.derived_workplanes())
    root["derived_workplanes"].push_back(
        json{{"id", workplane.id().value()},
             {"name", workplane.name()},
             {"kind", "feature_face"},
             {"source_feature", workplane.face_reference().source_feature().value()},
             {"face", std::string(to_string(workplane.face_reference().face()))}});
  root["sketches"] = json::array();
  for (const auto& sketch : document.sketches()) {
    json sketch_json{{"id", sketch.id().value()},
                     {"name", sketch.name()},
                     {"workplane", sketch.workplane().value()},
                     {"line_segments", json::array()},
                     {"arc_segments", json::array()},
                     {"spline_segments", json::array()},
                     {"projected_points", json::array()},
                     {"projected_lines", json::array()},
                     {"reference_generated_lines", json::array()},
                     {"trim_extend_operations", json::array()},
                     {"tangent_continuities", json::array()},
                     {"constraints", json::array()},
                     {"geometric_constraints", json::array()},
                     {"driving_dimensions", json::array()},
                     {"rectangle_profiles", json::array()},
                     {"circle_profiles", json::array()},
                     {"closed_profiles", json::array()},
                     {"arc_closed_profiles", json::array()},
                     {"composite_closed_profiles", json::array()},
                     {"circular_hole_patterns", json::array()}};
    for (const auto& line : sketch.line_segments())
      sketch_json["line_segments"].push_back(json{{"id", line.id().value()},
                                                  {"start", point2_to_json(line.start())},
                                                  {"end", point2_to_json(line.end())}});
    for (const auto& arc : sketch.arc_segments())
      sketch_json["arc_segments"].push_back(arc_segment_to_json(arc));
    for (const auto& spline : sketch.spline_segments())
      sketch_json["spline_segments"].push_back(spline_segment_to_json(spline));
    for (const auto& point : sketch.projected_points())
      sketch_json["projected_points"].push_back(projected_point_to_json(point));
    for (const auto& line : sketch.projected_lines())
      sketch_json["projected_lines"].push_back(projected_line_to_json(line));
    for (const auto& line : sketch.reference_generated_lines())
      sketch_json["reference_generated_lines"].push_back(reference_generated_line_to_json(line));
    for (const auto& operation : sketch.trim_extend_operations())
      sketch_json["trim_extend_operations"].push_back(trim_extend_operation_to_json(operation));
    for (const auto& continuity : sketch.tangent_continuities())
      sketch_json["tangent_continuities"].push_back(tangent_continuity_to_json(continuity));
    for (const auto& constraint : sketch.constraints())
      sketch_json["constraints"].push_back(sketch_constraint_to_json(constraint));
    for (const auto& constraint : sketch.geometric_constraints())
      sketch_json["geometric_constraints"].push_back(geometric_constraint_to_json(constraint));
    for (const auto& dimension : sketch.driving_dimensions())
      sketch_json["driving_dimensions"].push_back(driving_dimension_to_json(dimension));
    for (const auto& profile : sketch.rectangle_profiles())
      sketch_json["rectangle_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"center", point2_to_json(profile.center())},
               {"width_parameter", profile.width_parameter().value()},
               {"height_parameter", profile.height_parameter().value()}});
    for (const auto& profile : sketch.circle_profiles())
      sketch_json["circle_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"center", point2_to_json(profile.center())},
               {"diameter_parameter", profile.diameter_parameter().value()}});
    for (const auto& profile : sketch.closed_profiles())
      sketch_json["closed_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"line_segments", sketch_entity_ids_to_json(profile.line_segments())}});
    for (const auto& profile : sketch.arc_closed_profiles())
      sketch_json["arc_closed_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"curve_segments", sketch_entity_ids_to_json(profile.curve_segments())}});
    for (const auto& profile : sketch.composite_closed_profiles())
      sketch_json["composite_closed_profiles"].push_back(
          json{{"id", profile.id().value()},
               {"outer_contour", sketch_entity_ids_to_json(profile.outer_contour())},
               {"inner_contours", composite_inner_contours_to_json(profile.inner_contours())}});
    for (const auto& pattern : sketch.circular_hole_patterns())
      sketch_json["circular_hole_patterns"].push_back(
          json{{"id", pattern.id().value()},
               {"center", point2_to_json(pattern.center())},
               {"angle_offset_deg", pattern.angle_offset_deg()},
               {"radius_parameter", pattern.radius_parameter().value()},
               {"count_parameter", pattern.count_parameter().value()},
               {"hole_diameter_parameter", pattern.hole_diameter_parameter().value()}});
    root["sketches"].push_back(std::move(sketch_json));
  }
  root["features"] = json::array();
  for (const auto& feature : document.features()) {
    json feature_json{{"id", feature.id().value()},
                      {"name", feature.name()},
                      {"type", std::string(to_string(feature.type()))},
                      {"input_sketch", feature.input_sketch().value()},
                      {"direction", std::string(to_string(feature.direction()))}};
    if (feature.type() == FeatureType::AdditiveExtrude)
      feature_json["length_parameter"] = feature.length_parameter().value();
    if (feature.type() == FeatureType::SubtractiveExtrude) {
      feature_json["target_feature"] = feature.target_feature().value();
      feature_json["depth"] = std::string(to_string(feature.subtractive_depth()));
    }
    root["features"].push_back(std::move(feature_json));
  }
  root["reference_statuses"] = json::array();
  for (const auto& status : document.reference_statuses())
    root["reference_statuses"].push_back(reference_status_to_json(status));
  root["reference_remaps"] = json::array();
  for (const auto& remap : document.reference_remaps())
    root["reference_remaps"].push_back(reference_remap_to_json(remap));
  root["sketch_origin_overrides"] = json::array();
  for (const auto& origin : document.sketch_origin_overrides())
    root["sketch_origin_overrides"].push_back(sketch_origin_override_to_json(origin));
  return Result<std::string>::success(root.dump(2));
}

Result<PartDocument> deserialize_part_document_from_json(std::string_view content) {
  try {
    const json root = json::parse(content.begin(), content.end());
    if (!root.is_object())
      return Result<PartDocument>::failure(json_error("part document json root must be an object"));
    if (root.at("schema").get<std::string>() != k_schema)
      return Result<PartDocument>::failure(json_error("unsupported part document json schema"));
    if (root.at("version").get<int>() != k_version)
      return Result<PartDocument>::failure(json_error("unsupported part document json version"));
    const auto& document_json = root.at("document");
    auto document = PartDocument::create(DocumentId(document_json.at("id").get<std::string>()),
                                         document_json.at("name").get<std::string>());
    if (document.has_error())
      return document;
    for (const auto& parameter_json : root.at("parameters")) {
      auto parameter = parameter_from_json(parameter_json);
      if (parameter.has_error())
        return Result<PartDocument>::failure(parameter.error());
      auto added = document.value().add_parameter(parameter.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    for (const auto& datum_plane_json : root.at("datum_planes")) {
      auto datum_plane = datum_plane_from_json(datum_plane_json);
      if (datum_plane.has_error())
        return Result<PartDocument>::failure(datum_plane.error());
      auto added = document.value().add_datum_plane(datum_plane.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    for (const auto& point_json : root.value("construction_points", json::array())) {
      auto point = construction_point_from_json(point_json);
      if (point.has_error())
        return Result<PartDocument>::failure(point.error());
      auto added = document.value().add_construction_point(point.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json construction_plane_array = root.value("construction_planes", json::array());
    std::vector<bool> added_construction_plane(construction_plane_array.size(), false);
    std::size_t remaining_construction_planes = construction_plane_array.size();
    while (remaining_construction_planes > 0U) {
      bool progress = false;
      for (std::size_t index = 0; index < construction_plane_array.size(); ++index) {
        if (added_construction_plane[index])
          continue;
        auto plane = construction_plane_from_json(construction_plane_array.at(index));
        if (plane.has_error())
          return Result<PartDocument>::failure(plane.error());
        auto added = document.value().add_construction_plane(plane.value());
        if (added.has_error()) {
          const auto message = added.error().message();
          if (message == "plane offset source plane must exist in part document" ||
              message ==
                  "plane parallel to plane through point references must exist in part document")
            continue;
          return Result<PartDocument>::failure(added.error());
        }
        added_construction_plane[index] = true;
        --remaining_construction_planes;
        progress = true;
      }
      if (!progress)
        return Result<PartDocument>::failure(
            json_error("could not resolve construction plane json dependencies"));
    }
    const auto& sketch_array = root.at("sketches");
    const auto& feature_array = root.at("features");
    const json derived_workplane_array = root.value("derived_workplanes", json::array());
    const json construction_line_array = root.value("construction_lines", json::array());
    std::vector<bool> added_sketch(sketch_array.size(), false),
        added_feature(feature_array.size(), false),
        added_workplane(derived_workplane_array.size(), false),
        added_construction_line(construction_line_array.size(), false);
    std::size_t remaining = sketch_array.size() + feature_array.size() +
                            derived_workplane_array.size() + construction_line_array.size();
    while (remaining > 0U) {
      bool progress = false;
      for (std::size_t index = 0; index < sketch_array.size(); ++index) {
        if (added_sketch[index])
          continue;
        auto sketch = sketch_from_json(sketch_array.at(index));
        if (sketch.has_error())
          return Result<PartDocument>::failure(sketch.error());
        if (!sketch_dependencies_available(document.value(), sketch.value()))
          continue;
        auto added = document.value().add_sketch(sketch.value());
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        added_sketch[index] = true;
        --remaining;
        progress = true;
      }
      for (std::size_t index = 0; index < feature_array.size(); ++index) {
        if (added_feature[index])
          continue;
        auto feature = feature_from_json(feature_array.at(index));
        if (feature.has_error())
          return Result<PartDocument>::failure(feature.error());
        if (document.value().find_sketch(feature.value().input_sketch()) == nullptr)
          continue;
        if (feature.value().type() == FeatureType::AdditiveExtrude &&
            document.value().find_parameter(feature.value().length_parameter()) == nullptr)
          return Result<PartDocument>::failure(
              json_error("additive extrude length parameter must exist in part document"));
        if (feature.value().type() == FeatureType::SubtractiveExtrude &&
            document.value().find_feature(feature.value().target_feature()) == nullptr)
          continue;
        auto added = document.value().add_feature(feature.value());
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        added_feature[index] = true;
        --remaining;
        progress = true;
      }
      for (std::size_t index = 0; index < derived_workplane_array.size(); ++index) {
        if (added_workplane[index])
          continue;
        auto workplane = derived_workplane_from_json(derived_workplane_array.at(index));
        if (workplane.has_error())
          return Result<PartDocument>::failure(workplane.error());
        if (document.value().find_feature(workplane.value().face_reference().source_feature()) ==
            nullptr)
          continue;
        auto added = document.value().add_derived_workplane(workplane.value());
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        added_workplane[index] = true;
        --remaining;
        progress = true;
      }
      for (std::size_t index = 0; index < construction_line_array.size(); ++index) {
        if (added_construction_line[index])
          continue;
        auto line = construction_line_from_json(construction_line_array.at(index));
        if (line.has_error())
          return Result<PartDocument>::failure(line.error());
        auto added = document.value().add_construction_line(line.value());
        if (added.has_error()) {
          const auto message = added.error().message();
          if (message ==
                  "line parallel to line through point references must exist in part document" ||
              message == "line parallel to generated edge through point references must exist in "
                         "part document")
            continue;
          return Result<PartDocument>::failure(added.error());
        }
        added_construction_line[index] = true;
        --remaining;
        progress = true;
      }
      if (!progress)
        return Result<PartDocument>::failure(
            json_error("could not resolve part document json dependencies"));
    }
    for (const auto& status_json : root.value("reference_statuses", json::array())) {
      auto status = reference_status_from_json(status_json);
      if (status.has_error())
        return Result<PartDocument>::failure(status.error());
      auto added = document.value().add_reference_status(status.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    for (const auto& remap_json : root.value("reference_remaps", json::array())) {
      auto remap = reference_remap_from_json(remap_json);
      if (remap.has_error())
        return Result<PartDocument>::failure(remap.error());
      auto added = document.value().add_reference_remap(remap.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    for (const auto& origin_json : root.value("sketch_origin_overrides", json::array())) {
      auto origin = sketch_origin_override_from_json(origin_json);
      if (origin.has_error())
        return Result<PartDocument>::failure(origin.error());
      auto added = document.value().add_sketch_origin_override(origin.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
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
  if (path.empty())
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "part document json file path must not be empty"));
  const auto serialized = serialize_part_document_to_json(document);
  if (serialized.has_error())
    return Result<std::uintmax_t>::failure(serialized.error());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not open part document json file for writing"));
  output << serialized.value();
  output.flush();
  if (!output)
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not write part document json file"));
  output.close();
  if (!output)
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not close part document json file after writing"));
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return Result<std::uintmax_t>::failure(
        json_file_error(path, "could not stat written part document json file"));
  return Result<std::uintmax_t>::success(size);
}

Result<PartDocument> read_part_document_json_file(const std::filesystem::path& path) {
  if (path.empty())
    return Result<PartDocument>::failure(
        json_file_error(path, "part document json file path must not be empty"));
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return Result<PartDocument>::failure(
        json_file_error(path, "could not open part document json file for reading"));
  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof())
    return Result<PartDocument>::failure(
        json_file_error(path, "could not read part document json file"));
  return deserialize_part_document_from_json(buffer.str());
}

} // namespace blcad
