#include "blcad/core/part_document_json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
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

[[nodiscard]] Result<SemanticCircularEdge> semantic_circular_edge_from_json(const json& value) {
  const std::string edge = value.get<std::string>();
  if (edge == "source_rim")
    return Result<SemanticCircularEdge>::success(SemanticCircularEdge::SourceRim);
  if (edge == "opposite_rim")
    return Result<SemanticCircularEdge>::success(SemanticCircularEdge::OppositeRim);
  return Result<SemanticCircularEdge>::failure(json_error("unsupported semantic circular edge"));
}

[[nodiscard]] json edge_treatment_edge_to_json(const EdgeReference& edge) {
  if (edge.source_kind() == PartFeatureInputSourceKind::SemanticLinearEdge) {
    const auto& source = std::get<SemanticEdgeReference>(edge.source());
    return json{{"source_kind", "semantic_linear_edge"},
                {"source_feature", source.source_feature().value()},
                {"edge", std::string(to_string(source.edge()))}};
  }
  const auto& source = std::get<SemanticCircularEdgeReference>(edge.source());
  return json{{"source_kind", "semantic_circular_edge"},
              {"source_feature", source.source_feature().value()},
              {"source_profile", source.source_profile().value()},
              {"edge", std::string(to_string(source.edge()))}};
}

[[nodiscard]] Result<EdgeReference> edge_treatment_edge_from_json(const json& value,
                                                                  PartFeatureInputRole role) {
  if (!value.is_object() || !value.contains("source_kind") || !value.at("source_kind").is_string())
    return Result<EdgeReference>::failure(json_error("edge treatment edge requires source_kind"));
  const std::string kind = value.at("source_kind").get<std::string>();
  if (kind == "semantic_linear_edge" && value.size() == 3U && value.contains("source_feature") &&
      value.at("source_feature").is_string() && value.contains("edge") &&
      value.at("edge").is_string()) {
    auto edge = semantic_edge_from_json(value.at("edge"));
    if (edge.has_error())
      return Result<EdgeReference>::failure(edge.error());
    auto reference = SemanticEdgeReference::create(
        FeatureId(value.at("source_feature").get<std::string>()), edge.value());
    if (reference.has_error())
      return Result<EdgeReference>::failure(reference.error());
    return EdgeReference::create_linear(role, std::move(reference.value()));
  }
  if (kind == "semantic_circular_edge" && value.size() == 4U && value.contains("source_feature") &&
      value.at("source_feature").is_string() && value.contains("source_profile") &&
      value.at("source_profile").is_string() && value.contains("edge") &&
      value.at("edge").is_string()) {
    auto edge = semantic_circular_edge_from_json(value.at("edge"));
    if (edge.has_error())
      return Result<EdgeReference>::failure(edge.error());
    auto reference = SemanticCircularEdgeReference::create(
        FeatureId(value.at("source_feature").get<std::string>()),
        ProfileId(value.at("source_profile").get<std::string>()), edge.value());
    if (reference.has_error())
      return Result<EdgeReference>::failure(reference.error());
    return EdgeReference::create_circular(role, std::move(reference.value()));
  }
  return Result<EdgeReference>::failure(json_error("unsupported or malformed edge treatment edge"));
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

[[nodiscard]] Result<Quantity> angle_quantity_from_json(const json& parameter_json,
                                                        const std::string& object_id) {
  if (parameter_json.at("unit").get<std::string>() != "deg")
    return Result<Quantity>::failure(json_error("angle parameters must use unit \"deg\""));
  return Quantity::angle_deg(parameter_json.at("value").get<double>(), object_id);
}

[[nodiscard]] Result<Parameter> parameter_from_json(const json& parameter_json) {
  const auto id = parameter_json.at("id").get<std::string>();
  const auto type = parameter_json.at("type").get<std::string>();
  if (type != "length" && type != "count" && type != "angle")
    return Result<Parameter>::failure(
        json_error("only length, count, and angle parameters are supported"));
  if (parameter_json.at("scope").get<std::string>() != "part")
    return Result<Parameter>::failure(json_error("only part-scope parameters are supported"));
  if (type == "count") {
    auto quantity = count_quantity_from_json(parameter_json, id);
    if (quantity.has_error())
      return Result<Parameter>::failure(quantity.error());
    return Parameter::create_count(ParameterId(id), parameter_json.at("name").get<std::string>(),
                                   quantity.value());
  }
  if (type == "angle") {
    auto quantity = angle_quantity_from_json(parameter_json, id);
    if (quantity.has_error())
      return Result<Parameter>::failure(quantity.error());
    return Parameter::create_angle(ParameterId(id), parameter_json.at("name").get<std::string>(),
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

[[nodiscard]] Result<BodyKind> body_kind_from_json(const json& value) {
  const auto kind = value.get<std::string>();
  if (kind == "solid")
    return Result<BodyKind>::success(BodyKind::Solid);
  if (kind == "surface")
    return Result<BodyKind>::success(BodyKind::Surface);
  return Result<BodyKind>::failure(json_error("unsupported body kind in part document json"));
}

[[nodiscard]] Result<BodyVisibility> body_visibility_from_json(const json& value) {
  const auto visibility = value.get<std::string>();
  if (visibility == "visible")
    return Result<BodyVisibility>::success(BodyVisibility::Visible);
  if (visibility == "hidden")
    return Result<BodyVisibility>::success(BodyVisibility::Hidden);
  return Result<BodyVisibility>::failure(
      json_error("unsupported body visibility in part document json"));
}

[[nodiscard]] Result<Body> body_from_json(const json& value) {
  if (!value.is_object())
    return Result<Body>::failure(json_error("body entry must be an object in part document json"));
  for (const auto* field : {"id", "name", "kind", "visibility"}) {
    if (!value.contains(field))
      return Result<Body>::failure(
          json_error("body entry must contain id, name, kind, and visibility"));
    if (!value.at(field).is_string())
      return Result<Body>::failure(
          json_error("body id, name, kind, and visibility must be strings"));
  }
  auto kind = body_kind_from_json(value.at("kind"));
  if (kind.has_error())
    return Result<Body>::failure(kind.error());
  auto visibility = body_visibility_from_json(value.at("visibility"));
  if (visibility.has_error())
    return Result<Body>::failure(visibility.error());
  return Body::create(BodyId(value.at("id").get<std::string>()),
                      value.at("name").get<std::string>(), kind.value(), visibility.value());
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

[[nodiscard]] Result<DatumAxis> datum_axis_from_json(const json& axis_json) {
  const auto kind = axis_json.at("kind").get<std::string>();
  if (kind == "explicit")
    return DatumAxis::create_explicit(
        DatumAxisId(axis_json.at("id").get<std::string>()), axis_json.at("name").get<std::string>(),
        point3_from_json(axis_json.at("origin")), vector3_from_json(axis_json.at("direction")),
        parameter_dependencies_from_object(axis_json));
  if (kind == "from_construction_line")
    return DatumAxis::create_from_construction_line(
        DatumAxisId(axis_json.at("id").get<std::string>()), axis_json.at("name").get<std::string>(),
        ConstructionLineId(axis_json.at("source_construction_line").get<std::string>()));
  return Result<DatumAxis>::failure(
      json_error("unsupported datum axis kind in part document json"));
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

[[nodiscard]] json extrude_limit_face_to_json(const FaceReference& reference) {
  json value{{"role", std::string(to_string(reference.role()))},
             {"capability", std::string(to_string(reference.expected_capability()))},
             {"source_kind", std::string(to_string(reference.source_kind()))}};
  if (reference.source_kind() == PartFeatureInputSourceKind::SemanticPlanarFace) {
    const auto& face = std::get<SemanticFaceReference>(reference.source());
    value["source_feature"] = face.source_feature().value();
    value["face"] = std::string(to_string(face.face()));
  } else {
    const auto& face = std::get<SemanticCylindricalFaceReference>(reference.source());
    value["source_feature"] = face.source_feature().value();
    value["source_profile"] = face.source_profile().value();
    value["face"] = std::string(to_string(face.face()));
  }
  return value;
}

[[nodiscard]] Result<FaceReference> extrude_limit_face_from_json(const json& value) {
  if (!value.is_object() || value.value("role", "") != "extrude_limit_face" ||
      value.value("capability", "") != "face")
    return Result<FaceReference>::failure(
        json_error("extrude limit face requires role and Face capability"));
  const std::string source_kind = value.value("source_kind", "");
  if (source_kind == "semantic_planar_face") {
    if (value.size() != 5U || !value.contains("source_feature") || !value.contains("face"))
      return Result<FaceReference>::failure(
          json_error("semantic planar extrude limit face has invalid fields"));
    auto face = semantic_face_from_json(value.at("face"));
    if (face.has_error())
      return Result<FaceReference>::failure(face.error());
    auto identity = SemanticFaceReference::create(
        FeatureId(value.at("source_feature").get<std::string>()), face.value());
    if (identity.has_error())
      return Result<FaceReference>::failure(identity.error());
    return FaceReference::create_planar(PartFeatureInputRole::ExtrudeLimitFace,
                                        std::move(identity.value()));
  }
  if (source_kind == "semantic_cylindrical_face") {
    if (value.size() != 6U || !value.contains("source_feature") ||
        !value.contains("source_profile") || !value.contains("face"))
      return Result<FaceReference>::failure(
          json_error("semantic cylindrical extrude limit face has invalid fields"));
    if (value.at("face").get<std::string>() != "wall")
      return Result<FaceReference>::failure(
          json_error("unsupported semantic cylindrical face in extrude extent"));
    auto identity = SemanticCylindricalFaceReference::create(
        FeatureId(value.at("source_feature").get<std::string>()),
        ProfileId(value.at("source_profile").get<std::string>()));
    if (identity.has_error())
      return Result<FaceReference>::failure(identity.error());
    return FaceReference::create_cylindrical(PartFeatureInputRole::ExtrudeLimitFace,
                                             std::move(identity.value()));
  }
  return Result<FaceReference>::failure(json_error("unsupported extrude limit face source kind"));
}

[[nodiscard]] json extrude_feature_intent_to_json(const ExtrudeFeatureIntent& intent) {
  const auto& extent = intent.extent();
  json value{{"extent", std::string(to_string(extent.mode()))}};
  if (extent.first_distance_parameter().has_value())
    value["first_distance_parameter"] = extent.first_distance_parameter()->value();
  if (extent.second_distance_parameter().has_value())
    value["second_distance_parameter"] = extent.second_distance_parameter()->value();
  if (extent.first_face().has_value())
    value["first_face"] = extrude_limit_face_to_json(*extent.first_face());
  if (extent.second_face().has_value())
    value["second_face"] = extrude_limit_face_to_json(*extent.second_face());
  if (intent.taper_angle_deg().has_value())
    value["taper_angle_deg"] = *intent.taper_angle_deg();
  if (intent.thin().has_value()) {
    const auto& thin = *intent.thin();
    value["thin"] = json{{"mode", std::string(to_string(thin.mode()))},
                         {"first_thickness_parameter", thin.first_thickness_parameter().value()}};
    if (thin.second_thickness_parameter().has_value())
      value["thin"]["second_thickness_parameter"] = thin.second_thickness_parameter()->value();
  }
  return value;
}

[[nodiscard]] Result<ExtrudeFeatureIntent> extrude_feature_intent_from_json(const json& value) {
  if (!value.is_object() || !value.contains("extent") || !value.at("extent").is_string())
    return Result<ExtrudeFeatureIntent>::failure(
        json_error("extrude intent requires string extent"));
  const std::string mode = value.at("extent").get<std::string>();
  std::size_t required_mode_fields = 0U;
  if (mode == "distance" || mode == "symmetric" || mode == "to_face")
    required_mode_fields = 1U;
  else if (mode == "two_sided" || mode == "between")
    required_mode_fields = 2U;
  const std::size_t expected_fields = 1U + required_mode_fields +
                                      (value.contains("taper_angle_deg") ? 1U : 0U) +
                                      (value.contains("thin") ? 1U : 0U);
  if (value.size() != expected_fields)
    return Result<ExtrudeFeatureIntent>::failure(
        json_error("extrude intent has fields not valid for its extent mode"));
  Result<ExtrudeExtentIntent> extent = Result<ExtrudeExtentIntent>::failure(
      json_error("unsupported extrude extent in part document json"));
  if (mode == "distance")
    extent = ExtrudeExtentIntent::distance(
        ParameterId(value.at("first_distance_parameter").get<std::string>()));
  else if (mode == "symmetric")
    extent = ExtrudeExtentIntent::symmetric(
        ParameterId(value.at("first_distance_parameter").get<std::string>()));
  else if (mode == "two_sided")
    extent = ExtrudeExtentIntent::two_sided(
        ParameterId(value.at("first_distance_parameter").get<std::string>()),
        ParameterId(value.at("second_distance_parameter").get<std::string>()));
  else if (mode == "through_all")
    extent = Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent::through_all());
  else if (mode == "to_next")
    extent = Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent::to_next());
  else if (mode == "to_face") {
    auto face = extrude_limit_face_from_json(value.at("first_face"));
    if (face.has_error())
      return Result<ExtrudeFeatureIntent>::failure(face.error());
    extent = ExtrudeExtentIntent::to_face(std::move(face.value()));
  } else if (mode == "between") {
    auto first = extrude_limit_face_from_json(value.at("first_face"));
    auto second = extrude_limit_face_from_json(value.at("second_face"));
    if (first.has_error())
      return Result<ExtrudeFeatureIntent>::failure(first.error());
    if (second.has_error())
      return Result<ExtrudeFeatureIntent>::failure(second.error());
    extent = ExtrudeExtentIntent::between(std::move(first.value()), std::move(second.value()));
  }
  if (extent.has_error())
    return Result<ExtrudeFeatureIntent>::failure(extent.error());

  std::optional<ExtrudeThinIntent> thin;
  if (value.contains("thin")) {
    const auto& thin_json = value.at("thin");
    if (!thin_json.is_object() || !thin_json.contains("mode") ||
        !thin_json.contains("first_thickness_parameter"))
      return Result<ExtrudeFeatureIntent>::failure(
          json_error("extrude thin intent requires mode and first thickness parameter"));
    const std::string thin_mode = thin_json.at("mode").get<std::string>();
    ExtrudeThinMode parsed_mode;
    if (thin_mode == "one_sided")
      parsed_mode = ExtrudeThinMode::OneSided;
    else if (thin_mode == "two_sided")
      parsed_mode = ExtrudeThinMode::TwoSided;
    else if (thin_mode == "mid_plane")
      parsed_mode = ExtrudeThinMode::MidPlane;
    else
      return Result<ExtrudeFeatureIntent>::failure(
          json_error("unsupported extrude thin mode in part document json"));
    const std::size_t expected_thin_fields = parsed_mode == ExtrudeThinMode::TwoSided ? 3U : 2U;
    if (thin_json.size() != expected_thin_fields)
      return Result<ExtrudeFeatureIntent>::failure(
          json_error("extrude thin intent has fields not valid for its mode"));
    std::optional<ParameterId> second;
    if (thin_json.contains("second_thickness_parameter"))
      second = ParameterId(thin_json.at("second_thickness_parameter").get<std::string>());
    auto parsed = ExtrudeThinIntent::create(
        parsed_mode, ParameterId(thin_json.at("first_thickness_parameter").get<std::string>()),
        second);
    if (parsed.has_error())
      return Result<ExtrudeFeatureIntent>::failure(parsed.error());
    thin = std::move(parsed.value());
  }
  std::optional<double> taper;
  if (value.contains("taper_angle_deg"))
    taper = value.at("taper_angle_deg").get<double>();
  return ExtrudeFeatureIntent::create(std::move(extent.value()), taper, std::move(thin));
}

[[nodiscard]] Result<std::optional<FeatureBodyResultContext>>
feature_body_result_context_from_json(const json& feature_json) {
  const bool has_mode = feature_json.contains("operation_mode");
  const bool has_target = feature_json.contains("target_body");
  const bool has_produced = feature_json.contains("produced_body");
  if (!has_mode) {
    if (has_target || has_produced)
      return Result<std::optional<FeatureBodyResultContext>>::failure(
          json_error("feature body references require operation_mode in part document json"));
    return Result<std::optional<FeatureBodyResultContext>>::success(std::nullopt);
  }
  if (!feature_json.at("operation_mode").is_string() ||
      (has_target && !feature_json.at("target_body").is_string()) ||
      (has_produced && !feature_json.at("produced_body").is_string()))
    return Result<std::optional<FeatureBodyResultContext>>::failure(
        json_error("feature body operation fields must be strings"));
  const auto spelling = feature_json.at("operation_mode").get<std::string>();
  FeatureBodyOperationMode mode;
  if (spelling == "new_body")
    mode = FeatureBodyOperationMode::NewBody;
  else if (spelling == "join")
    mode = FeatureBodyOperationMode::Join;
  else if (spelling == "cut")
    mode = FeatureBodyOperationMode::Cut;
  else if (spelling == "intersect")
    mode = FeatureBodyOperationMode::Intersect;
  else
    return Result<std::optional<FeatureBodyResultContext>>::failure(
        json_error("unsupported feature body operation mode in part document json"));
  std::optional<BodyId> target_body;
  if (has_target)
    target_body = BodyId(feature_json.at("target_body").get<std::string>());
  std::optional<BodyId> produced_body;
  if (has_produced)
    produced_body = BodyId(feature_json.at("produced_body").get<std::string>());
  auto context =
      FeatureBodyResultContext::create(mode, std::move(target_body), std::move(produced_body));
  if (context.has_error())
    return Result<std::optional<FeatureBodyResultContext>>::failure(context.error());
  return Result<std::optional<FeatureBodyResultContext>>::success(std::move(context.value()));
}

[[nodiscard]] Result<Feature> feature_from_json(const json& feature_json) {
  const auto type = feature_json.at("type").get<std::string>();
  auto direction = extrude_direction_from_json(feature_json.at("direction"));
  if (direction.has_error())
    return Result<Feature>::failure(direction.error());
  Result<Feature> feature =
      Result<Feature>::failure(json_error("unsupported feature type in part document json"));
  std::optional<ExtrudeFeatureIntent> intent;
  if (feature_json.contains("extrude")) {
    auto parsed = extrude_feature_intent_from_json(feature_json.at("extrude"));
    if (parsed.has_error())
      return Result<Feature>::failure(parsed.error());
    intent = std::move(parsed.value());
  }
  if (type == "additive_extrude") {
    if (intent.has_value())
      feature = Feature::create_additive_extrude(
          FeatureId(feature_json.at("id").get<std::string>()),
          feature_json.at("name").get<std::string>(),
          SketchId(feature_json.at("input_sketch").get<std::string>()), std::move(*intent),
          direction.value());
    else
      feature = Feature::create_additive_extrude(
          FeatureId(feature_json.at("id").get<std::string>()),
          feature_json.at("name").get<std::string>(),
          SketchId(feature_json.at("input_sketch").get<std::string>()),
          ParameterId(feature_json.at("length_parameter").get<std::string>()), direction.value());
  } else if (type == "subtractive_extrude") {
    if (intent.has_value())
      feature = Feature::create_subtractive_extrude(
          FeatureId(feature_json.at("id").get<std::string>()),
          feature_json.at("name").get<std::string>(),
          SketchId(feature_json.at("input_sketch").get<std::string>()),
          FeatureId(feature_json.at("target_feature").get<std::string>()), std::move(*intent),
          direction.value());
    else {
      if (feature_json.at("depth").get<std::string>() != "through_all")
        return Result<Feature>::failure(
            json_error("only through_all subtractive extrude depth is supported"));
      feature = Feature::create_subtractive_extrude(
          FeatureId(feature_json.at("id").get<std::string>()),
          feature_json.at("name").get<std::string>(),
          SketchId(feature_json.at("input_sketch").get<std::string>()),
          FeatureId(feature_json.at("target_feature").get<std::string>()),
          SubtractiveExtrudeDepth::ThroughAll, direction.value());
    }
  }
  if (feature.has_error())
    return feature;
  auto context = feature_body_result_context_from_json(feature_json);
  if (context.has_error())
    return Result<Feature>::failure(context.error());
  if (!context.value().has_value())
    return feature;
  return feature.value().with_body_result_context(std::move(context.value().value()));
}

[[nodiscard]] json revolve_profile_to_json(const ProfileRegionReference& profile) {
  return json{{"sketch", profile.sketch().value()}, {"profile", profile.profile().value()}};
}

[[nodiscard]] json revolve_axis_to_json(const AxisReference& axis) {
  json value{{"source_kind", std::string(to_string(axis.source_kind()))}};
  switch (axis.source_kind()) {
  case PartFeatureInputSourceKind::DatumAxis:
    value["datum_axis"] = std::get<DatumAxisId>(axis.source()).value();
    break;
  case PartFeatureInputSourceKind::ConstructionLine:
    value["construction_line"] = std::get<ConstructionLineId>(axis.source()).value();
    break;
  case PartFeatureInputSourceKind::SemanticAxis: {
    const auto& semantic = std::get<SemanticAxisReference>(axis.source());
    value["source_feature"] = semantic.source_feature().value();
    value["axis"] = std::string(to_string(semantic.axis()));
    break;
  }
  case PartFeatureInputSourceKind::SemanticLinearEdge:
    value["semantic_edge"] =
        semantic_edge_reference_to_json(std::get<SemanticEdgeReference>(axis.source()));
    break;
  default:
    break;
  }
  return value;
}

[[nodiscard]] json pattern_source_to_json(const PatternSourceReference& source) {
  if (source.kind() == PatternSourceKind::Feature)
    return json{{"kind", "feature"}, {"feature", std::get<FeatureId>(source.source()).value()}};
  return json{{"kind", "body"}, {"body", std::get<BodyId>(source.source()).value()}};
}

[[nodiscard]] Result<PatternSourceReference> pattern_source_from_json(const json& value) {
  if (!value.is_object() || !value.contains("kind") || !value.at("kind").is_string())
    return Result<PatternSourceReference>::failure(
        json_error("pattern source requires string kind"));
  const std::string kind = value.at("kind").get<std::string>();
  if (kind == "feature" && value.size() == 2U && value.contains("feature") &&
      value.at("feature").is_string())
    return PatternSourceReference::feature(FeatureId(value.at("feature").get<std::string>()));
  if (kind == "body" && value.size() == 2U && value.contains("body") &&
      value.at("body").is_string())
    return PatternSourceReference::body(BodyId(value.at("body").get<std::string>()));
  return Result<PatternSourceReference>::failure(
      json_error("unsupported or malformed pattern source"));
}

[[nodiscard]] Result<AxisReference> pattern_axis_from_json(const json& axis_json) {
  if (!axis_json.is_object() || !axis_json.contains("source_kind") ||
      !axis_json.at("source_kind").is_string())
    return Result<AxisReference>::failure(json_error("pattern axis requires source_kind"));
  const std::string source_kind = axis_json.at("source_kind").get<std::string>();
  if (source_kind == "datum_axis" && axis_json.size() == 2U && axis_json.contains("datum_axis") &&
      axis_json.at("datum_axis").is_string())
    return AxisReference::create_datum_axis(
        PartFeatureInputRole::PatternAxis,
        DatumAxisId(axis_json.at("datum_axis").get<std::string>()));
  if (source_kind == "construction_line" && axis_json.size() == 2U &&
      axis_json.contains("construction_line") && axis_json.at("construction_line").is_string())
    return AxisReference::create_construction_line(
        PartFeatureInputRole::PatternAxis,
        ConstructionLineId(axis_json.at("construction_line").get<std::string>()),
        PartFeatureInputCapability::Axis);
  if (source_kind == "semantic_axis" && axis_json.size() == 3U &&
      axis_json.contains("source_feature") && axis_json.contains("axis") &&
      axis_json.at("source_feature").is_string() && axis_json.at("axis").is_string() &&
      axis_json.at("axis").get<std::string>() == "axis") {
    auto semantic =
        SemanticAxisReference::create(FeatureId(axis_json.at("source_feature").get<std::string>()));
    if (semantic.has_error())
      return Result<AxisReference>::failure(semantic.error());
    return AxisReference::create_semantic_axis(PartFeatureInputRole::PatternAxis,
                                               std::move(semantic.value()));
  }
  if (source_kind == "semantic_linear_edge" && axis_json.size() == 2U &&
      axis_json.contains("semantic_edge") && axis_json.at("semantic_edge").is_object()) {
    auto edge = semantic_edge_reference_from_json(axis_json.at("semantic_edge"));
    if (edge.has_error())
      return Result<AxisReference>::failure(edge.error());
    return AxisReference::create_semantic_edge(PartFeatureInputRole::PatternAxis,
                                               std::move(edge.value()),
                                               PartFeatureInputCapability::Axis);
  }
  return Result<AxisReference>::failure(json_error("unsupported pattern axis source kind"));
}

[[nodiscard]] json mirror_source_to_json(const MirrorSourceReference& source) {
  if (source.kind() == MirrorSourceKind::Feature)
    return json{{"kind", "feature"}, {"feature", std::get<FeatureId>(source.source()).value()}};
  return json{{"kind", "body"}, {"body", std::get<BodyId>(source.source()).value()}};
}

[[nodiscard]] Result<MirrorSourceReference> mirror_source_from_json(const json& value) {
  if (!value.is_object() || !value.contains("kind") || !value.at("kind").is_string())
    return Result<MirrorSourceReference>::failure(json_error("mirror source requires string kind"));
  const std::string kind = value.at("kind").get<std::string>();
  if (kind == "feature" && value.size() == 2U && value.contains("feature") &&
      value.at("feature").is_string())
    return MirrorSourceReference::feature(FeatureId(value.at("feature").get<std::string>()));
  if (kind == "body" && value.size() == 2U && value.contains("body") &&
      value.at("body").is_string())
    return MirrorSourceReference::body(BodyId(value.at("body").get<std::string>()));
  return Result<MirrorSourceReference>::failure(
      json_error("unsupported or malformed mirror source"));
}

[[nodiscard]] json mirror_plane_to_json(const PlaneReference& plane) {
  json value{{"source_kind", std::string(to_string(plane.source_kind()))}};
  if (plane.source_kind() == PartFeatureInputSourceKind::DatumPlane)
    value["datum_plane"] = std::get<DatumPlaneId>(plane.source()).value();
  else if (plane.source_kind() == PartFeatureInputSourceKind::ConstructionPlane)
    value["construction_plane"] = std::get<ConstructionPlaneId>(plane.source()).value();
  else {
    const auto& face = std::get<SemanticFaceReference>(plane.source());
    value["source_feature"] = face.source_feature().value();
    value["face"] = std::string(to_string(face.face()));
  }
  return value;
}

[[nodiscard]] Result<PlaneReference> mirror_plane_from_json(const json& value) {
  if (!value.is_object() || !value.contains("source_kind") || !value.at("source_kind").is_string())
    return Result<PlaneReference>::failure(json_error("mirror plane requires source_kind"));
  const std::string kind = value.at("source_kind").get<std::string>();
  if (kind == "datum_plane" && value.size() == 2U && value.contains("datum_plane") &&
      value.at("datum_plane").is_string())
    return PlaneReference::create_datum_plane(
        PartFeatureInputRole::MirrorPlane,
        DatumPlaneId(value.at("datum_plane").get<std::string>()));
  if (kind == "construction_plane" && value.size() == 2U && value.contains("construction_plane") &&
      value.at("construction_plane").is_string())
    return PlaneReference::create_construction_plane(
        PartFeatureInputRole::MirrorPlane,
        ConstructionPlaneId(value.at("construction_plane").get<std::string>()));
  if (kind == "semantic_planar_face" && value.size() == 3U && value.contains("source_feature") &&
      value.at("source_feature").is_string() && value.contains("face") &&
      value.at("face").is_string()) {
    auto face = semantic_face_from_json(value.at("face"));
    if (face.has_error())
      return Result<PlaneReference>::failure(face.error());
    auto semantic = SemanticFaceReference::create(
        FeatureId(value.at("source_feature").get<std::string>()), face.value());
    if (semantic.has_error())
      return Result<PlaneReference>::failure(semantic.error());
    return PlaneReference::create_semantic_face(PartFeatureInputRole::MirrorPlane,
                                                std::move(semantic.value()));
  }
  return Result<PlaneReference>::failure(json_error("unsupported or malformed mirror plane"));
}

[[nodiscard]] json pattern_body_context_to_json(const FeatureBodyResultContext& context) {
  json value{{"operation_mode", std::string(to_string(context.operation_mode()))}};
  if (context.target_body().has_value())
    value["target_body"] = context.target_body()->value();
  if (context.produced_body().has_value())
    value["produced_body"] = context.produced_body()->value();
  return value;
}

[[nodiscard]] json linear_pattern_to_json(const LinearPatternFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"type", "linear_pattern"},
             {"sources", json::array()},
             {"direction", revolve_axis_to_json(feature.direction())},
             {"count_parameter", feature.count_parameter().value()},
             {"extent", json{{"mode", std::string(to_string(feature.extent_mode()))},
                             {"parameter", feature.extent_parameter().value()},
                             {"direction", std::string(to_string(feature.direction_sign()))}}}};
  for (const auto& source : feature.sources())
    value["sources"].push_back(pattern_source_to_json(source));
  value.update(pattern_body_context_to_json(feature.body_result_context()));
  return value;
}

[[nodiscard]] json circular_pattern_to_json(const CircularPatternFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"type", "circular_pattern"},
             {"sources", json::array()},
             {"axis", revolve_axis_to_json(feature.axis())},
             {"count_parameter", feature.count_parameter().value()},
             {"total_angle_deg", feature.total_angle_deg()},
             {"equal_spacing", feature.equal_spacing()}};
  for (const auto& source : feature.sources())
    value["sources"].push_back(pattern_source_to_json(source));
  value.update(pattern_body_context_to_json(feature.body_result_context()));
  return value;
}

[[nodiscard]] json mirror_feature_to_json(const MirrorFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"type", "mirror"},
             {"sources", json::array()},
             {"mirror_plane", mirror_plane_to_json(feature.mirror_plane())}};
  for (const auto& source : feature.sources())
    value["sources"].push_back(mirror_source_to_json(source));
  value.update(pattern_body_context_to_json(feature.body_result_context()));
  return value;
}

[[nodiscard]] Result<MirrorFeature> mirror_feature_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
      !value.contains("name") || !value.at("name").is_string() ||
      value.value("type", "") != "mirror" || !value.contains("sources") ||
      !value.at("sources").is_array() || !value.contains("mirror_plane") ||
      !value.at("mirror_plane").is_object() || !value.contains("operation_mode"))
    return Result<MirrorFeature>::failure(
        json_error("mirror feature requires valid mandatory intent fields"));
  auto context = feature_body_result_context_from_json(value);
  if (context.has_error())
    return Result<MirrorFeature>::failure(context.error());
  if (!context.value().has_value())
    return Result<MirrorFeature>::failure(
        json_error("mirror feature requires body result context"));
  const std::size_t expected_fields =
      6U + (value.contains("target_body") ? 1U : 0U) + (value.contains("produced_body") ? 1U : 0U);
  if (value.size() != expected_fields)
    return Result<MirrorFeature>::failure(json_error("mirror feature has unsupported fields"));
  std::vector<MirrorSourceReference> sources;
  sources.reserve(value.at("sources").size());
  for (const auto& source_json : value.at("sources")) {
    auto source = mirror_source_from_json(source_json);
    if (source.has_error())
      return Result<MirrorFeature>::failure(source.error());
    sources.push_back(std::move(source.value()));
  }
  auto plane = mirror_plane_from_json(value.at("mirror_plane"));
  if (plane.has_error())
    return Result<MirrorFeature>::failure(plane.error());
  return MirrorFeature::create(FeatureId(value.at("id").get<std::string>()),
                               value.at("name").get<std::string>(), std::move(sources),
                               std::move(plane.value()), std::move(*context.value()));
}

[[nodiscard]] json fillet_feature_to_json(const FilletFeature& feature) {
  json value{
      {"id", feature.id().value()}, {"name", feature.name()},
      {"type", "fillet"},           {"target_body", feature.target_body().value()},
      {"edges", json::array()},     {"radius_parameter", feature.radius_parameter().value()}};
  for (const auto& edge : feature.edges())
    value["edges"].push_back(edge_treatment_edge_to_json(edge));
  return value;
}

[[nodiscard]] json chamfer_feature_to_json(const ChamferFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"type", "chamfer"},
             {"target_body", feature.target_body().value()},
             {"edges", json::array()},
             {"mode", std::string(to_string(feature.mode()))},
             {"first_parameter", feature.first_parameter().value()}};
  for (const auto& edge : feature.edges())
    value["edges"].push_back(edge_treatment_edge_to_json(edge));
  if (feature.second_parameter().has_value())
    value["second_parameter"] = feature.second_parameter()->value();
  return value;
}

using ParsedEdgeTreatment = std::variant<FilletFeature, ChamferFeature>;

[[nodiscard]] Result<ParsedEdgeTreatment> edge_treatment_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
      !value.contains("name") || !value.at("name").is_string() || !value.contains("type") ||
      !value.at("type").is_string() || !value.contains("target_body") ||
      !value.at("target_body").is_string() || !value.contains("edges") ||
      !value.at("edges").is_array())
    return Result<ParsedEdgeTreatment>::failure(
        json_error("edge treatment requires valid mandatory intent fields"));
  const std::string type = value.at("type").get<std::string>();
  const PartFeatureInputRole role =
      type == "fillet" ? PartFeatureInputRole::FilletEdge : PartFeatureInputRole::ChamferEdge;
  std::vector<EdgeReference> edges;
  edges.reserve(value.at("edges").size());
  for (const auto& edge_json : value.at("edges")) {
    auto edge = edge_treatment_edge_from_json(edge_json, role);
    if (edge.has_error())
      return Result<ParsedEdgeTreatment>::failure(edge.error());
    edges.push_back(std::move(edge.value()));
  }
  const FeatureId id(value.at("id").get<std::string>());
  const std::string name = value.at("name").get<std::string>();
  const BodyId target(value.at("target_body").get<std::string>());
  if (type == "fillet" && value.size() == 6U && value.contains("radius_parameter") &&
      value.at("radius_parameter").is_string()) {
    auto feature =
        FilletFeature::create(id, name, target, std::move(edges),
                              ParameterId(value.at("radius_parameter").get<std::string>()));
    if (feature.has_error())
      return Result<ParsedEdgeTreatment>::failure(feature.error());
    return Result<ParsedEdgeTreatment>::success(std::move(feature.value()));
  }
  if (type != "chamfer" || !value.contains("mode") || !value.at("mode").is_string() ||
      !value.contains("first_parameter") || !value.at("first_parameter").is_string())
    return Result<ParsedEdgeTreatment>::failure(
        json_error("unsupported or malformed edge treatment"));
  const std::string mode = value.at("mode").get<std::string>();
  const ParameterId first(value.at("first_parameter").get<std::string>());
  Result<ChamferFeature> feature =
      Result<ChamferFeature>::failure(json_error("unsupported or malformed chamfer mode"));
  if (mode == "equal_distance" && value.size() == 7U)
    feature = ChamferFeature::create_equal_distance(id, name, target, std::move(edges), first);
  else if ((mode == "two_distance" || mode == "distance_angle") && value.size() == 8U &&
           value.contains("second_parameter") && value.at("second_parameter").is_string()) {
    const ParameterId second(value.at("second_parameter").get<std::string>());
    feature =
        mode == "two_distance"
            ? ChamferFeature::create_two_distance(id, name, target, std::move(edges), first, second)
            : ChamferFeature::create_distance_angle(id, name, target, std::move(edges), first,
                                                    second);
  }
  if (feature.has_error())
    return Result<ParsedEdgeTreatment>::failure(feature.error());
  return Result<ParsedEdgeTreatment>::success(std::move(feature.value()));
}

[[nodiscard]] json shell_removal_face_to_json(const FaceReference& reference) {
  json value{{"role", std::string(to_string(reference.role()))},
             {"capability", std::string(to_string(reference.expected_capability()))},
             {"source_kind", std::string(to_string(reference.source_kind()))}};
  if (reference.source_kind() == PartFeatureInputSourceKind::SemanticPlanarFace) {
    const auto& face = std::get<SemanticFaceReference>(reference.source());
    value["source_feature"] = face.source_feature().value();
    value["face"] = std::string(to_string(face.face()));
  } else {
    const auto& face = std::get<SemanticCylindricalFaceReference>(reference.source());
    value["source_feature"] = face.source_feature().value();
    value["source_profile"] = face.source_profile().value();
    value["face"] = std::string(to_string(face.face()));
  }
  return value;
}

[[nodiscard]] Result<FaceReference> shell_removal_face_from_json(const json& value) {
  if (!value.is_object() || value.value("role", "") != "shell_removal_face" ||
      value.value("capability", "") != "face")
    return Result<FaceReference>::failure(
        json_error("shell removal face requires role and Face capability"));
  const std::string source_kind = value.value("source_kind", "");
  if (source_kind == "semantic_planar_face") {
    if (value.size() != 5U || !value.contains("source_feature") ||
        !value.at("source_feature").is_string() || !value.contains("face") ||
        !value.at("face").is_string())
      return Result<FaceReference>::failure(
          json_error("semantic planar shell removal face has invalid fields"));
    auto face = semantic_face_from_json(value.at("face"));
    if (face.has_error())
      return Result<FaceReference>::failure(face.error());
    auto identity = SemanticFaceReference::create(
        FeatureId(value.at("source_feature").get<std::string>()), face.value());
    if (identity.has_error())
      return Result<FaceReference>::failure(identity.error());
    return FaceReference::create_planar(PartFeatureInputRole::ShellRemovalFace,
                                        std::move(identity.value()));
  }
  if (source_kind == "semantic_cylindrical_face") {
    if (value.size() != 6U || !value.contains("source_feature") ||
        !value.at("source_feature").is_string() || !value.contains("source_profile") ||
        !value.at("source_profile").is_string() || value.value("face", "") != "wall")
      return Result<FaceReference>::failure(
          json_error("semantic cylindrical shell removal face has invalid fields"));
    auto identity = SemanticCylindricalFaceReference::create(
        FeatureId(value.at("source_feature").get<std::string>()),
        ProfileId(value.at("source_profile").get<std::string>()));
    if (identity.has_error())
      return Result<FaceReference>::failure(identity.error());
    return FaceReference::create_cylindrical(PartFeatureInputRole::ShellRemovalFace,
                                             std::move(identity.value()));
  }
  return Result<FaceReference>::failure(
      json_error("unsupported shell removal face source kind"));
}

[[nodiscard]] json shell_feature_to_json(const ShellFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"target_body", feature.target_body().value()},
             {"removed_faces", json::array()},
             {"thickness_parameter", feature.thickness_parameter().value()},
             {"direction", std::string(to_string(feature.direction()))}};
  for (const FaceReference& face : feature.removed_faces())
    value["removed_faces"].push_back(shell_removal_face_to_json(face));
  return value;
}

[[nodiscard]] Result<ShellFeature> shell_feature_from_json(const json& value) {
  if (!value.is_object() || value.size() != 6U || !value.contains("id") ||
      !value.at("id").is_string() || !value.contains("name") ||
      !value.at("name").is_string() || !value.contains("target_body") ||
      !value.at("target_body").is_string() || !value.contains("removed_faces") ||
      !value.at("removed_faces").is_array() || !value.contains("thickness_parameter") ||
      !value.at("thickness_parameter").is_string() || !value.contains("direction") ||
      !value.at("direction").is_string())
    return Result<ShellFeature>::failure(
        json_error("shell feature requires exactly its mandatory intent fields"));
  std::vector<FaceReference> removed_faces;
  removed_faces.reserve(value.at("removed_faces").size());
  for (const auto& face_json : value.at("removed_faces")) {
    auto face = shell_removal_face_from_json(face_json);
    if (face.has_error())
      return Result<ShellFeature>::failure(face.error());
    removed_faces.push_back(std::move(face.value()));
  }
  const std::string direction = value.at("direction").get<std::string>();
  if (direction != "inward" && direction != "outward")
    return Result<ShellFeature>::failure(
        json_error("unsupported shell direction in part document json"));
  return ShellFeature::create(
      FeatureId(value.at("id").get<std::string>()), value.at("name").get<std::string>(),
      BodyId(value.at("target_body").get<std::string>()), std::move(removed_faces),
      ParameterId(value.at("thickness_parameter").get<std::string>()),
      direction == "inward" ? ShellDirection::Inward : ShellDirection::Outward);
}

using ParsedPartPattern = std::variant<LinearPatternFeature, CircularPatternFeature>;

[[nodiscard]] Result<ParsedPartPattern> part_pattern_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.contains("name") ||
      !value.contains("type") || !value.contains("sources") || !value.contains("count_parameter") ||
      !value.contains("operation_mode") || !value.at("id").is_string() ||
      !value.at("name").is_string() || !value.at("type").is_string() ||
      !value.at("sources").is_array() || !value.at("count_parameter").is_string())
    return Result<ParsedPartPattern>::failure(
        json_error("part pattern requires valid mandatory intent fields"));
  std::vector<PatternSourceReference> sources;
  for (const auto& source_json : value.at("sources")) {
    auto source = pattern_source_from_json(source_json);
    if (source.has_error())
      return Result<ParsedPartPattern>::failure(source.error());
    sources.push_back(std::move(source.value()));
  }
  auto context = feature_body_result_context_from_json(value);
  if (context.has_error())
    return Result<ParsedPartPattern>::failure(context.error());
  if (!context.value().has_value())
    return Result<ParsedPartPattern>::failure(
        json_error("part pattern requires body result context"));
  const FeatureId id(value.at("id").get<std::string>());
  const std::string name = value.at("name").get<std::string>();
  const ParameterId count(value.at("count_parameter").get<std::string>());
  const std::string type = value.at("type").get<std::string>();
  if (type == "linear_pattern") {
    if (!value.contains("direction") || !value.at("direction").is_object() ||
        !value.contains("extent") || !value.at("extent").is_object())
      return Result<ParsedPartPattern>::failure(
          json_error("linear pattern requires direction and extent"));
    auto direction = pattern_axis_from_json(value.at("direction"));
    if (direction.has_error())
      return Result<ParsedPartPattern>::failure(direction.error());
    const auto& extent = value.at("extent");
    if (extent.size() != 3U || !extent.contains("mode") || !extent.contains("parameter") ||
        !extent.contains("direction") || !extent.at("mode").is_string() ||
        !extent.at("parameter").is_string() || !extent.at("direction").is_string())
      return Result<ParsedPartPattern>::failure(json_error("linear pattern extent is malformed"));
    const std::string mode = extent.at("mode").get<std::string>();
    const std::string sign = extent.at("direction").get<std::string>();
    if ((mode != "spacing" && mode != "total_extent") || (sign != "positive" && sign != "negative"))
      return Result<ParsedPartPattern>::failure(
          json_error("linear pattern extent has unsupported mode or direction"));
    auto feature = LinearPatternFeature::create(
        id, name, std::move(sources), std::move(direction.value()), count,
        mode == "spacing" ? LinearPatternExtentMode::Spacing : LinearPatternExtentMode::TotalExtent,
        ParameterId(extent.at("parameter").get<std::string>()),
        sign == "positive" ? PatternDirectionSign::Positive : PatternDirectionSign::Negative,
        std::move(*context.value()));
    if (feature.has_error())
      return Result<ParsedPartPattern>::failure(feature.error());
    return Result<ParsedPartPattern>::success(std::move(feature.value()));
  }
  if (type == "circular_pattern") {
    if (!value.contains("axis") || !value.at("axis").is_object() ||
        !value.contains("total_angle_deg") || !value.at("total_angle_deg").is_number() ||
        !value.contains("equal_spacing") || !value.at("equal_spacing").is_boolean())
      return Result<ParsedPartPattern>::failure(
          json_error("circular pattern requires axis, total angle, and equal spacing"));
    auto axis = pattern_axis_from_json(value.at("axis"));
    if (axis.has_error())
      return Result<ParsedPartPattern>::failure(axis.error());
    auto feature = CircularPatternFeature::create(
        id, name, std::move(sources), std::move(axis.value()), count,
        value.at("total_angle_deg").get<double>(), value.at("equal_spacing").get<bool>(),
        std::move(*context.value()));
    if (feature.has_error())
      return Result<ParsedPartPattern>::failure(feature.error());
    return Result<ParsedPartPattern>::success(std::move(feature.value()));
  }
  return Result<ParsedPartPattern>::failure(json_error("unsupported part pattern type"));
}

[[nodiscard]] json revolve_feature_to_json(const RevolveFeature& feature) {
  json value{{"id", feature.id().value()},
             {"name", feature.name()},
             {"type", std::string(to_string(feature.kind()))},
             {"profile", revolve_profile_to_json(feature.profile())},
             {"axis", revolve_axis_to_json(feature.axis())}};
  json extent{{"mode", std::string(to_string(feature.extent().mode()))}};
  if (feature.extent().angle_deg().has_value())
    extent["angle_deg"] = *feature.extent().angle_deg();
  if (feature.extent().side().has_value())
    extent["side"] = std::string(to_string(*feature.extent().side()));
  value["extent"] = std::move(extent);
  const auto& context = feature.body_result_context();
  value["operation_mode"] = std::string(to_string(context.operation_mode()));
  if (context.target_body().has_value())
    value["target_body"] = context.target_body()->value();
  if (context.produced_body().has_value())
    value["produced_body"] = context.produced_body()->value();
  return value;
}

[[nodiscard]] Result<RevolveFeature> revolve_feature_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.contains("name") ||
      !value.contains("type") || !value.contains("profile") || !value.contains("axis") ||
      !value.contains("extent") || !value.contains("operation_mode"))
    return Result<RevolveFeature>::failure(
        json_error("revolve feature requires all mandatory intent fields"));
  if (!value.at("id").is_string() || !value.at("name").is_string() ||
      !value.at("type").is_string() || !value.at("profile").is_object() ||
      !value.at("axis").is_object() || !value.at("extent").is_object())
    return Result<RevolveFeature>::failure(
        json_error("revolve feature fields have invalid json types"));

  const auto& profile_json = value.at("profile");
  if (profile_json.size() != 2U || !profile_json.contains("sketch") ||
      !profile_json.contains("profile") || !profile_json.at("sketch").is_string() ||
      !profile_json.at("profile").is_string())
    return Result<RevolveFeature>::failure(
        json_error("revolve profile requires only string sketch and profile fields"));
  auto profile =
      ProfileRegionReference::create(SketchId(profile_json.at("sketch").get<std::string>()),
                                     ProfileId(profile_json.at("profile").get<std::string>()),
                                     PartFeatureInputRole::RevolveProfile);
  if (profile.has_error())
    return Result<RevolveFeature>::failure(profile.error());

  const auto& axis_json = value.at("axis");
  if (!axis_json.contains("source_kind") || !axis_json.at("source_kind").is_string())
    return Result<RevolveFeature>::failure(json_error("revolve axis requires source_kind"));
  const std::string source_kind = axis_json.at("source_kind").get<std::string>();
  Result<AxisReference> axis =
      Result<AxisReference>::failure(json_error("unsupported revolve axis source kind"));
  if (source_kind == "datum_axis" && axis_json.size() == 2U && axis_json.contains("datum_axis") &&
      axis_json.at("datum_axis").is_string())
    axis = AxisReference::create_datum_axis(
        PartFeatureInputRole::RevolveAxis,
        DatumAxisId(axis_json.at("datum_axis").get<std::string>()));
  else if (source_kind == "construction_line" && axis_json.size() == 2U &&
           axis_json.contains("construction_line") && axis_json.at("construction_line").is_string())
    axis = AxisReference::create_construction_line(
        PartFeatureInputRole::RevolveAxis,
        ConstructionLineId(axis_json.at("construction_line").get<std::string>()),
        PartFeatureInputCapability::Axis);
  else if (source_kind == "semantic_axis" && axis_json.size() == 3U &&
           axis_json.contains("source_feature") && axis_json.contains("axis") &&
           axis_json.at("source_feature").is_string() && axis_json.at("axis").is_string()) {
    if (axis_json.at("axis").get<std::string>() != "axis")
      return Result<RevolveFeature>::failure(json_error("unsupported semantic revolve axis"));
    auto semantic =
        SemanticAxisReference::create(FeatureId(axis_json.at("source_feature").get<std::string>()));
    if (semantic.has_error())
      return Result<RevolveFeature>::failure(semantic.error());
    axis = AxisReference::create_semantic_axis(PartFeatureInputRole::RevolveAxis,
                                               std::move(semantic.value()));
  } else if (source_kind == "semantic_linear_edge" && axis_json.size() == 2U &&
             axis_json.contains("semantic_edge") && axis_json.at("semantic_edge").is_object()) {
    auto edge = semantic_edge_reference_from_json(axis_json.at("semantic_edge"));
    if (edge.has_error())
      return Result<RevolveFeature>::failure(edge.error());
    axis = AxisReference::create_semantic_edge(PartFeatureInputRole::RevolveAxis,
                                               std::move(edge.value()),
                                               PartFeatureInputCapability::Axis);
  }
  if (axis.has_error())
    return Result<RevolveFeature>::failure(axis.error());

  const auto& extent_json = value.at("extent");
  if (!extent_json.contains("mode") || !extent_json.at("mode").is_string())
    return Result<RevolveFeature>::failure(json_error("revolve extent requires mode"));
  const std::string mode = extent_json.at("mode").get<std::string>();
  Result<RevolveAngleExtent> extent =
      Result<RevolveAngleExtent>::failure(json_error("unsupported revolve extent mode"));
  if (mode == "full" && extent_json.size() == 1U)
    extent = Result<RevolveAngleExtent>::success(RevolveAngleExtent::full());
  else if (mode == "angle" && extent_json.size() == 3U && extent_json.contains("angle_deg") &&
           extent_json.at("angle_deg").is_number() && extent_json.contains("side") &&
           extent_json.at("side").is_string()) {
    const std::string side = extent_json.at("side").get<std::string>();
    if (side != "positive" && side != "negative")
      return Result<RevolveFeature>::failure(json_error("unsupported revolve side"));
    extent = RevolveAngleExtent::angle(extent_json.at("angle_deg").get<double>(),
                                       side == "positive" ? RevolveSide::Positive
                                                          : RevolveSide::Negative);
  } else if (mode == "symmetric" && extent_json.size() == 2U && extent_json.contains("angle_deg") &&
             extent_json.at("angle_deg").is_number())
    extent = RevolveAngleExtent::symmetric(extent_json.at("angle_deg").get<double>());
  if (extent.has_error())
    return Result<RevolveFeature>::failure(extent.error());

  auto context = feature_body_result_context_from_json(value);
  if (context.has_error())
    return Result<RevolveFeature>::failure(context.error());
  if (!context.value().has_value())
    return Result<RevolveFeature>::failure(
        json_error("revolve feature requires body result context"));
  const std::string type = value.at("type").get<std::string>();
  if (type == "revolve")
    return RevolveFeature::create_revolve(
        FeatureId(value.at("id").get<std::string>()), value.at("name").get<std::string>(),
        std::move(profile.value()), std::move(axis.value()), std::move(extent.value()),
        std::move(context.value().value()));
  if (type == "revolve_cut")
    return RevolveFeature::create_revolve_cut(
        FeatureId(value.at("id").get<std::string>()), value.at("name").get<std::string>(),
        std::move(profile.value()), std::move(axis.value()), std::move(extent.value()),
        std::move(context.value().value()));
  return Result<RevolveFeature>::failure(json_error("unsupported revolve feature type"));
}

[[nodiscard]] bool extrude_reference_features_available(const PartDocument& document,
                                                        const Feature& feature) {
  const auto available = [&document](const std::optional<FaceReference>& face) {
    return !face.has_value() || document.find_feature(FeatureId(face->source_node_id())) != nullptr;
  };
  return available(feature.extrude_intent().extent().first_face()) &&
         available(feature.extrude_intent().extent().second_face());
}

[[nodiscard]] Result<BodyBooleanFeature> body_boolean_feature_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.contains("operation") ||
      !value.contains("target_body") || !value.contains("tool_bodies") ||
      !value.contains("result_mode") || !value.contains("keep_tool_bodies"))
    return Result<BodyBooleanFeature>::failure(
        json_error("body boolean feature requires all mandatory fields"));
  if (!value.at("id").is_string() || !value.at("operation").is_string() ||
      !value.at("target_body").is_string() || !value.at("tool_bodies").is_array() ||
      !value.at("result_mode").is_string() || !value.at("keep_tool_bodies").is_boolean() ||
      (value.contains("produced_body") && !value.at("produced_body").is_string()))
    return Result<BodyBooleanFeature>::failure(
        json_error("body boolean feature fields have invalid json types"));

  BodyBooleanOperation operation;
  const std::string operation_spelling = value.at("operation").get<std::string>();
  if (operation_spelling == "add")
    operation = BodyBooleanOperation::Add;
  else if (operation_spelling == "subtract")
    operation = BodyBooleanOperation::Subtract;
  else if (operation_spelling == "intersect")
    operation = BodyBooleanOperation::Intersect;
  else
    return Result<BodyBooleanFeature>::failure(
        json_error("unsupported body boolean operation in part document json"));

  BodyBooleanResultMode result_mode;
  const std::string mode_spelling = value.at("result_mode").get<std::string>();
  if (mode_spelling == "modify_target")
    result_mode = BodyBooleanResultMode::ModifyTarget;
  else if (mode_spelling == "new_body")
    result_mode = BodyBooleanResultMode::NewBody;
  else
    return Result<BodyBooleanFeature>::failure(
        json_error("unsupported body boolean result mode in part document json"));

  std::vector<BodyId> tools;
  tools.reserve(value.at("tool_bodies").size());
  for (const auto& tool : value.at("tool_bodies")) {
    if (!tool.is_string())
      return Result<BodyBooleanFeature>::failure(
          json_error("body boolean tool body ids must be strings"));
    tools.emplace_back(tool.get<std::string>());
  }
  std::optional<BodyId> produced;
  if (value.contains("produced_body"))
    produced = BodyId(value.at("produced_body").get<std::string>());
  return BodyBooleanFeature::create(FeatureId(value.at("id").get<std::string>()), operation,
                                    BodyId(value.at("target_body").get<std::string>()),
                                    std::move(tools), result_mode, std::move(produced),
                                    value.at("keep_tool_bodies").get<bool>());
}

[[nodiscard]] Result<BodyTransformCoordinateSpace>
body_transform_coordinate_space_from_json(const json& value) {
  if (!value.is_string())
    return Result<BodyTransformCoordinateSpace>::failure(
        json_error("body transform coordinate_space must be a string"));
  const std::string spelling = value.get<std::string>();
  if (spelling == "world")
    return Result<BodyTransformCoordinateSpace>::success(BodyTransformCoordinateSpace::World);
  if (spelling == "body_local")
    return Result<BodyTransformCoordinateSpace>::success(BodyTransformCoordinateSpace::BodyLocal);
  if (spelling == "sketch_local")
    return Result<BodyTransformCoordinateSpace>::success(BodyTransformCoordinateSpace::SketchLocal);
  if (spelling == "construction_reference")
    return Result<BodyTransformCoordinateSpace>::success(
        BodyTransformCoordinateSpace::ConstructionReference);
  return Result<BodyTransformCoordinateSpace>::failure(
      json_error("unsupported body transform coordinate space"));
}

[[nodiscard]] Result<BodyTransformRotationAxis>
body_transform_rotation_axis_from_json(const json& value) {
  if (!value.is_object() || !value.contains("kind") || !value.at("kind").is_string())
    return Result<BodyTransformRotationAxis>::failure(
        json_error("body transform rotation axis requires a kind"));
  const std::string kind = value.at("kind").get<std::string>();
  if (kind == "explicit") {
    if (!value.contains("origin") || !value.contains("direction"))
      return Result<BodyTransformRotationAxis>::failure(
          json_error("explicit body rotation axis requires origin and direction"));
    return BodyTransformRotationAxis::create_explicit(point3_from_json(value.at("origin")),
                                                      vector3_from_json(value.at("direction")));
  }
  if (kind == "datum_axis") {
    if (!value.contains("datum_axis") || !value.at("datum_axis").is_string())
      return Result<BodyTransformRotationAxis>::failure(
          json_error("body rotation datum axis requires a string id"));
    return BodyTransformRotationAxis::create_datum_axis(
        DatumAxisId(value.at("datum_axis").get<std::string>()));
  }
  if (kind == "construction_line") {
    if (!value.contains("construction_line") || !value.at("construction_line").is_string())
      return Result<BodyTransformRotationAxis>::failure(
          json_error("body rotation construction line requires a string id"));
    return BodyTransformRotationAxis::create_construction_line(
        ConstructionLineId(value.at("construction_line").get<std::string>()));
  }
  if (kind == "semantic_edge") {
    if (!value.contains("semantic_edge"))
      return Result<BodyTransformRotationAxis>::failure(
          json_error("body rotation semantic edge requires a reference"));
    auto edge = semantic_edge_reference_from_json(value.at("semantic_edge"));
    if (edge.has_error())
      return Result<BodyTransformRotationAxis>::failure(edge.error());
    return BodyTransformRotationAxis::create_semantic_edge(std::move(edge.value()));
  }
  return Result<BodyTransformRotationAxis>::failure(
      json_error("unsupported body transform rotation axis kind"));
}

[[nodiscard]] Result<BodyTransform> body_transform_from_json(const json& value) {
  if (!value.is_object() || !value.contains("id") || !value.contains("body") ||
      !value.contains("kind") || !value.contains("coordinate_space") ||
      !value.contains("apply_to_owned_sketches") ||
      !value.contains("apply_to_owned_construction_geometry"))
    return Result<BodyTransform>::failure(
        json_error("body transform requires all mandatory fields"));
  if (!value.at("id").is_string() || !value.at("body").is_string() ||
      !value.at("kind").is_string() || !value.at("apply_to_owned_sketches").is_boolean() ||
      !value.at("apply_to_owned_construction_geometry").is_boolean() ||
      (value.contains("coordinate_reference") && !value.at("coordinate_reference").is_string()))
    return Result<BodyTransform>::failure(json_error("body transform fields have invalid types"));

  auto coordinate_space = body_transform_coordinate_space_from_json(value.at("coordinate_space"));
  if (coordinate_space.has_error())
    return Result<BodyTransform>::failure(coordinate_space.error());
  std::optional<std::string> coordinate_reference;
  if (value.contains("coordinate_reference"))
    coordinate_reference = value.at("coordinate_reference").get<std::string>();
  const BodyTransformId id(value.at("id").get<std::string>());
  const BodyId body(value.at("body").get<std::string>());
  const bool apply_sketches = value.at("apply_to_owned_sketches").get<bool>();
  const bool apply_construction = value.at("apply_to_owned_construction_geometry").get<bool>();
  const std::string kind = value.at("kind").get<std::string>();
  if (kind == "translate") {
    if (!value.contains("translation_mm") || value.contains("rotation_axis") ||
        value.contains("angle_deg") || value.contains("scale_factor") || value.contains("center"))
      return Result<BodyTransform>::failure(
          json_error("translate body transform requires only translation_mm parameters"));
    return BodyTransform::create_translate(
        id, body, coordinate_space.value(), std::move(coordinate_reference),
        vector3_from_json(value.at("translation_mm")), apply_sketches, apply_construction);
  }
  if (kind == "rotate") {
    if (!value.contains("rotation_axis") || !value.contains("angle_deg") ||
        !value.at("angle_deg").is_number() || value.contains("translation_mm") ||
        value.contains("scale_factor") || value.contains("center"))
      return Result<BodyTransform>::failure(
          json_error("rotate body transform requires only rotation_axis and angle_deg parameters"));
    auto axis = body_transform_rotation_axis_from_json(value.at("rotation_axis"));
    if (axis.has_error())
      return Result<BodyTransform>::failure(axis.error());
    return BodyTransform::create_rotate(id, body, coordinate_space.value(),
                                        std::move(coordinate_reference), std::move(axis.value()),
                                        value.at("angle_deg").get<double>(), apply_sketches,
                                        apply_construction);
  }
  if (kind == "uniform_scale") {
    if (!value.contains("scale_factor") || !value.at("scale_factor").is_number() ||
        !value.contains("center") || value.contains("translation_mm") ||
        value.contains("rotation_axis") || value.contains("angle_deg"))
      return Result<BodyTransform>::failure(
          json_error("uniform_scale body transform requires only factor and center parameters"));
    return BodyTransform::create_uniform_scale(
        id, body, coordinate_space.value(), std::move(coordinate_reference),
        value.at("scale_factor").get<double>(), point3_from_json(value.at("center")),
        apply_sketches, apply_construction);
  }
  return Result<BodyTransform>::failure(json_error("unsupported body transform kind"));
}

[[nodiscard]] Result<SketchOwnership> sketch_ownership_from_json(const json& value) {
  if (!value.is_object() || !value.contains("sketch") || !value.contains("owning_body") ||
      !value.contains("association") || !value.at("sketch").is_string() ||
      !value.at("owning_body").is_string() || !value.at("association").is_string())
    return Result<SketchOwnership>::failure(
        json_error("sketch ownership requires string sketch owning_body and association fields"));
  const std::string spelling = value.at("association").get<std::string>();
  SketchAssociation association;
  if (spelling == "drives_body")
    association = SketchAssociation::DrivesBody;
  else if (spelling == "consumed_by_body")
    association = SketchAssociation::ConsumedByBody;
  else if (spelling == "reference_only")
    association = SketchAssociation::ReferenceOnly;
  else
    return Result<SketchOwnership>::failure(json_error("unsupported sketch ownership association"));
  return SketchOwnership::create(SketchId(value.at("sketch").get<std::string>()),
                                 BodyId(value.at("owning_body").get<std::string>()), association);
}

[[nodiscard]] json body_transform_rotation_axis_to_json(const BodyTransformRotationAxis& axis) {
  json value{{"kind", std::string(to_string(axis.kind()))}};
  switch (axis.kind()) {
  case BodyTransformRotationAxisKind::Explicit:
    value["origin"] = point3_to_json(axis.explicit_origin());
    value["direction"] = vector3_to_json(axis.explicit_direction());
    break;
  case BodyTransformRotationAxisKind::DatumAxis:
    value["datum_axis"] = axis.datum_axis().value();
    break;
  case BodyTransformRotationAxisKind::ConstructionLine:
    value["construction_line"] = axis.construction_line().value();
    break;
  case BodyTransformRotationAxisKind::SemanticEdge:
    value["semantic_edge"] = semantic_edge_reference_to_json(*axis.semantic_edge());
    break;
  }
  return value;
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
  root["bodies"] = json::array();
  for (const auto& body : document.bodies())
    root["bodies"].push_back(json{{"id", body.id().value()},
                                  {"name", body.name()},
                                  {"kind", std::string(to_string(body.kind()))},
                                  {"visibility", std::string(to_string(body.visibility()))}});
  root["parameters"] = json::array();
  for (const auto& parameter : document.parameters()) {
    json parameter_json{{"id", parameter.id().value()},
                        {"name", parameter.name()},
                        {"type", std::string(to_string(parameter.type()))},
                        {"scope", std::string(to_string(parameter.scope()))},
                        {"unit", std::string(parameter.value().unit())},
                        {"value", parameter.value().millimeters()}};
    if (parameter.formula().has_value())
      parameter_json["formula"] = parameter.formula().value();
    root["parameters"].push_back(std::move(parameter_json));
  }
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
  root["datum_axes"] = json::array();
  for (const auto& datum_axis : document.datum_axes()) {
    json axis_json{{"id", datum_axis.id().value()},
                   {"name", datum_axis.name()},
                   {"kind", std::string(to_string(datum_axis.kind()))}};
    if (datum_axis.kind() == DatumAxisKind::Explicit) {
      axis_json["origin"] = point3_to_json(datum_axis.origin());
      axis_json["direction"] = vector3_to_json(datum_axis.direction());
      axis_json["parameter_dependencies"] =
          parameter_ids_to_json(datum_axis.parameter_dependencies());
    } else {
      axis_json["source_construction_line"] = datum_axis.source_construction_line().value();
    }
    root["datum_axes"].push_back(std::move(axis_json));
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
    if (feature.type() == FeatureType::AdditiveExtrude &&
        feature.extrude_intent().is_historical_additive_default())
      feature_json["length_parameter"] = feature.length_parameter().value();
    if (feature.type() == FeatureType::SubtractiveExtrude) {
      feature_json["target_feature"] = feature.target_feature().value();
      if (feature.extrude_intent().is_historical_subtractive_default())
        feature_json["depth"] = std::string(to_string(feature.subtractive_depth()));
    }
    const bool explicit_extrude_intent =
        (feature.type() == FeatureType::AdditiveExtrude &&
         !feature.extrude_intent().is_historical_additive_default()) ||
        (feature.type() == FeatureType::SubtractiveExtrude &&
         !feature.extrude_intent().is_historical_subtractive_default());
    if (explicit_extrude_intent)
      feature_json["extrude"] = extrude_feature_intent_to_json(feature.extrude_intent());
    if (feature.body_result_context().has_value()) {
      const auto& context = feature.body_result_context().value();
      feature_json["operation_mode"] = std::string(to_string(context.operation_mode()));
      if (context.target_body().has_value())
        feature_json["target_body"] = context.target_body()->value();
      if (context.produced_body().has_value())
        feature_json["produced_body"] = context.produced_body()->value();
    }
    root["features"].push_back(std::move(feature_json));
  }
  root["revolve_features"] = json::array();
  for (const auto& feature : document.revolve_features())
    root["revolve_features"].push_back(revolve_feature_to_json(feature));
  root["part_patterns"] = json::array();
  auto pattern_order = document.dependency_graph().topological_order();
  if (pattern_order.has_error())
    return Result<std::string>::failure(pattern_order.error());
  for (const std::string& node : pattern_order.value()) {
    if (const auto* linear = document.find_linear_pattern_feature(FeatureId(node)))
      root["part_patterns"].push_back(linear_pattern_to_json(*linear));
    if (const auto* circular = document.find_circular_pattern_feature(FeatureId(node)))
      root["part_patterns"].push_back(circular_pattern_to_json(*circular));
  }
  root["mirror_features"] = json::array();
  for (const std::string& node : pattern_order.value())
    if (const auto* mirror = document.find_mirror_feature(FeatureId(node)))
      root["mirror_features"].push_back(mirror_feature_to_json(*mirror));
  root["edge_treatments"] = json::array();
  for (const std::string& node : pattern_order.value()) {
    if (const auto* fillet = document.find_fillet_feature(FeatureId(node)))
      root["edge_treatments"].push_back(fillet_feature_to_json(*fillet));
    if (const auto* chamfer = document.find_chamfer_feature(FeatureId(node)))
      root["edge_treatments"].push_back(chamfer_feature_to_json(*chamfer));
  }
  root["shell_features"] = json::array();
  for (const std::string& node : pattern_order.value())
    if (const auto* shell = document.find_shell_feature(FeatureId(node)))
      root["shell_features"].push_back(shell_feature_to_json(*shell));
  root["body_booleans"] = json::array();
  for (const auto& feature : document.body_boolean_features()) {
    json feature_json{{"id", feature.id().value()},
                      {"operation", std::string(to_string(feature.operation()))},
                      {"target_body", feature.target_body().value()},
                      {"tool_bodies", json::array()},
                      {"result_mode", std::string(to_string(feature.result_mode()))},
                      {"keep_tool_bodies", feature.keep_tool_bodies()}};
    for (const BodyId& tool : feature.tool_bodies())
      feature_json["tool_bodies"].push_back(tool.value());
    if (feature.produced_body().has_value())
      feature_json["produced_body"] = feature.produced_body()->value();
    root["body_booleans"].push_back(std::move(feature_json));
  }
  root["sketch_ownerships"] = json::array();
  for (const SketchOwnership& ownership : document.sketch_ownerships())
    root["sketch_ownerships"].push_back(
        json{{"sketch", ownership.sketch().value()},
             {"owning_body", ownership.owning_body().value()},
             {"association", std::string(to_string(ownership.association()))}});
  root["body_transforms"] = json::array();
  for (const BodyTransform& transform : document.body_transforms()) {
    json value{
        {"id", transform.id().value()},
        {"body", transform.body().value()},
        {"kind", std::string(to_string(transform.kind()))},
        {"coordinate_space", std::string(to_string(transform.coordinate_space()))},
        {"apply_to_owned_sketches", transform.apply_to_owned_sketches()},
        {"apply_to_owned_construction_geometry", transform.apply_to_owned_construction_geometry()}};
    if (transform.coordinate_reference().has_value())
      value["coordinate_reference"] = *transform.coordinate_reference();
    switch (transform.kind()) {
    case BodyTransformKind::Translate:
      value["translation_mm"] = vector3_to_json(transform.translation_mm());
      break;
    case BodyTransformKind::Rotate:
      value["rotation_axis"] = body_transform_rotation_axis_to_json(*transform.rotation_axis());
      value["angle_deg"] = transform.angle_deg();
      break;
    case BodyTransformKind::UniformScale:
      value["scale_factor"] = transform.scale_factor();
      value["center"] = point3_to_json(transform.scale_center());
      break;
    }
    root["body_transforms"].push_back(std::move(value));
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
    const json body_array = root.value("bodies", json::array());
    if (!body_array.is_array())
      return Result<PartDocument>::failure(
          json_error("bodies must be an array in part document json"));
    for (const auto& body_json : body_array) {
      auto body = body_from_json(body_json);
      if (body.has_error())
        return Result<PartDocument>::failure(body.error());
      auto added = document.value().add_body(std::move(body.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    for (const auto& parameter_json : root.at("parameters")) {
      if (parameter_json.contains("formula")) {
        // Expression parameters re-derive value and dependency edges from the
        // persisted formula; earlier file order guarantees inputs exist.
        const std::string type_name = parameter_json.at("type").get<std::string>();
        auto type = type_name == "count"   ? ParameterType::Count
                    : type_name == "angle" ? ParameterType::Angle
                                           : ParameterType::Length;
        auto added = document.value().add_expression_parameter(
            ParameterId(parameter_json.at("id").get<std::string>()),
            parameter_json.at("name").get<std::string>(), type,
            parameter_json.at("formula").get<std::string>());
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        continue;
      }
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
            feature.value().extrude_intent().is_historical_additive_default() &&
            document.value().find_parameter(feature.value().length_parameter()) == nullptr)
          return Result<PartDocument>::failure(
              json_error("additive extrude length parameter must exist in part document"));
        if (feature.value().type() == FeatureType::SubtractiveExtrude &&
            document.value().find_feature(feature.value().target_feature()) == nullptr)
          continue;
        if (!extrude_reference_features_available(document.value(), feature.value()))
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
    for (const auto& axis_json : root.value("datum_axes", json::array())) {
      auto axis = datum_axis_from_json(axis_json);
      if (axis.has_error())
        return Result<PartDocument>::failure(axis.error());
      auto added = document.value().add_datum_axis(axis.value());
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json revolve_array = root.value("revolve_features", json::array());
    if (!revolve_array.is_array())
      return Result<PartDocument>::failure(
          json_error("revolve_features must be an array in part document json"));
    for (const auto& revolve_json : revolve_array) {
      auto feature = revolve_feature_from_json(revolve_json);
      if (feature.has_error())
        return Result<PartDocument>::failure(feature.error());
      auto added = document.value().add_revolve_feature(std::move(feature.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json body_boolean_array = root.value("body_booleans", json::array());
    if (!body_boolean_array.is_array())
      return Result<PartDocument>::failure(
          json_error("body_booleans must be an array in part document json"));
    for (const auto& boolean_json : body_boolean_array) {
      auto feature = body_boolean_feature_from_json(boolean_json);
      if (feature.has_error())
        return Result<PartDocument>::failure(feature.error());
      auto added = document.value().add_body_boolean_feature(std::move(feature.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json pattern_array = root.value("part_patterns", json::array());
    if (!pattern_array.is_array())
      return Result<PartDocument>::failure(
          json_error("part_patterns must be an array in part document json"));
    std::vector<std::optional<ParsedPartPattern>> pending_patterns;
    pending_patterns.reserve(pattern_array.size());
    for (const auto& pattern_json : pattern_array) {
      auto pattern = part_pattern_from_json(pattern_json);
      if (pattern.has_error())
        return Result<PartDocument>::failure(pattern.error());
      pending_patterns.emplace_back(std::move(pattern.value()));
    }
    const json mirror_array = root.value("mirror_features", json::array());
    if (!mirror_array.is_array())
      return Result<PartDocument>::failure(
          json_error("mirror_features must be an array in part document json"));
    std::vector<std::optional<MirrorFeature>> pending_mirrors;
    pending_mirrors.reserve(mirror_array.size());
    for (const auto& mirror_json : mirror_array) {
      auto feature = mirror_feature_from_json(mirror_json);
      if (feature.has_error())
        return Result<PartDocument>::failure(feature.error());
      pending_mirrors.emplace_back(std::move(feature.value()));
    }
    const auto feature_exists = [&document](const FeatureId& id) {
      return document.value().find_feature(id) != nullptr ||
             document.value().find_revolve_feature(id) != nullptr ||
             document.value().find_linear_pattern_feature(id) != nullptr ||
             document.value().find_circular_pattern_feature(id) != nullptr ||
             document.value().find_mirror_feature(id) != nullptr ||
             document.value().find_body_boolean_feature(id) != nullptr;
    };
    const auto sources_exist = [&feature_exists](const auto& sources) {
      return std::all_of(sources.begin(), sources.end(), [&feature_exists](const auto& source) {
        using SourceKind = decltype(source.kind());
        if constexpr (std::is_same_v<SourceKind, PatternSourceKind>) {
          return source.kind() == PatternSourceKind::Body ||
                 feature_exists(std::get<FeatureId>(source.source()));
        } else {
          return source.kind() == MirrorSourceKind::Body ||
                 feature_exists(std::get<FeatureId>(source.source()));
        }
      });
    };
    std::size_t remaining_part_features = pending_patterns.size() + pending_mirrors.size();
    while (remaining_part_features > 0U) {
      bool progress = false;
      for (auto& pending : pending_patterns) {
        if (!pending.has_value())
          continue;
        const bool ready = std::visit(
            [&sources_exist](const auto& feature) { return sources_exist(feature.sources()); },
            *pending);
        if (!ready)
          continue;
        Result<std::size_t> added = std::holds_alternative<LinearPatternFeature>(*pending)
                                        ? document.value().add_linear_pattern_feature(
                                              std::move(std::get<LinearPatternFeature>(*pending)))
                                        : document.value().add_circular_pattern_feature(std::move(
                                              std::get<CircularPatternFeature>(*pending)));
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        pending.reset();
        --remaining_part_features;
        progress = true;
      }
      for (auto& pending : pending_mirrors) {
        if (!pending.has_value() || !sources_exist(pending->sources()))
          continue;
        auto added = document.value().add_mirror_feature(std::move(*pending));
        if (added.has_error())
          return Result<PartDocument>::failure(added.error());
        pending.reset();
        --remaining_part_features;
        progress = true;
      }
      if (!progress)
        return Result<PartDocument>::failure(
            json_error("could not resolve Pattern/Mirror feature dependencies"));
    }
    const json edge_treatment_array = root.value("edge_treatments", json::array());
    if (!edge_treatment_array.is_array())
      return Result<PartDocument>::failure(
          json_error("edge_treatments must be an array in part document json"));
    for (const auto& treatment_json : edge_treatment_array) {
      auto treatment = edge_treatment_from_json(treatment_json);
      if (treatment.has_error())
        return Result<PartDocument>::failure(treatment.error());
      Result<std::size_t> added = std::holds_alternative<FilletFeature>(treatment.value())
                                      ? document.value().add_fillet_feature(
                                            std::move(std::get<FilletFeature>(treatment.value())))
                                      : document.value().add_chamfer_feature(
                                            std::move(std::get<ChamferFeature>(treatment.value())));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json shell_array = root.value("shell_features", json::array());
    if (!shell_array.is_array())
      return Result<PartDocument>::failure(
          json_error("shell_features must be an array in part document json"));
    for (const auto& shell_json : shell_array) {
      auto shell = shell_feature_from_json(shell_json);
      if (shell.has_error())
        return Result<PartDocument>::failure(shell.error());
      auto added = document.value().add_shell_feature(std::move(shell.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json ownership_array = root.value("sketch_ownerships", json::array());
    if (!ownership_array.is_array())
      return Result<PartDocument>::failure(
          json_error("sketch_ownerships must be an array in part document json"));
    for (const auto& ownership_json : ownership_array) {
      auto ownership = sketch_ownership_from_json(ownership_json);
      if (ownership.has_error())
        return Result<PartDocument>::failure(ownership.error());
      auto added = document.value().add_sketch_ownership(std::move(ownership.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
    }
    const json transform_array = root.value("body_transforms", json::array());
    if (!transform_array.is_array())
      return Result<PartDocument>::failure(
          json_error("body_transforms must be an array in part document json"));
    for (const auto& transform_json : transform_array) {
      auto transform = body_transform_from_json(transform_json);
      if (transform.has_error())
        return Result<PartDocument>::failure(transform.error());
      auto added = document.value().add_body_transform(std::move(transform.value()));
      if (added.has_error())
        return Result<PartDocument>::failure(added.error());
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
