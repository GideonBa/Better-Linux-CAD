#include "blcad/core/part_document.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad {

namespace {

constexpr double k_tolerance = 1.0e-9;

Result<std::size_t> add_dependency_if_missing(DependencyGraph& graph, std::string dependency,
                                               std::string dependent) {
  if (graph.has_dependency(dependency, dependent)) {
    return Result<std::size_t>::success(graph.dependency_count());
  }
  return graph.add_dependency(std::move(dependency), std::move(dependent));
}

[[nodiscard]] Vector3 vector_between(Point3 from, Point3 to) noexcept { return Vector3{to.x - from.x, to.y - from.y, to.z - from.z}; }
[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept { return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z, left.x * right.y - left.y * right.x}; }
[[nodiscard]] double length(Vector3 vector) noexcept { return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z); }
[[nodiscard]] bool are_collinear(Point3 first, Point3 second, Point3 third) noexcept { return length(cross(vector_between(first, second), vector_between(first, third))) <= k_tolerance; }
[[nodiscard]] bool is_supported_derived_face(SemanticFace face) noexcept { return face == SemanticFace::Top || face == SemanticFace::Bottom || face == SemanticFace::Right || face == SemanticFace::Left || face == SemanticFace::Front || face == SemanticFace::Back; }
[[nodiscard]] bool has_additive_source_feature(const PartDocument& document, FeatureId feature_id) { const Feature* feature = document.find_feature(feature_id); return feature != nullptr && feature->type() == FeatureType::AdditiveExtrude; }

[[nodiscard]] Result<std::size_t> validate_generated_edge_reference(const PartDocument& document, const SemanticEdgeReference& reference, const std::string& object_id) {
  if (!has_additive_source_feature(document, reference.source_feature())) return Result<std::size_t>::failure(Error::validation(object_id, "semantic edge source feature must be an additive extrude in part document"));
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_generated_vertex_reference(const PartDocument& document, const SemanticVertexReference& reference, const std::string& object_id) {
  if (!has_additive_source_feature(document, reference.source_feature())) return Result<std::size_t>::failure(Error::validation(object_id, "semantic vertex source feature must be an additive extrude in part document"));
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_semantic_reference_target(const PartDocument& document, const SemanticReferenceTarget& target, const std::string& object_id) {
  if (!has_additive_source_feature(document, target.source_feature())) return Result<std::size_t>::failure(Error::validation(object_id, "semantic reference source feature must be an additive extrude in part document"));
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_relation_references(const PartDocument& document, const ConstructionRelation& relation, const std::string& object_id) {
  switch (relation.type()) {
  case ConstructionRelationType::PlaneOffsetFromPlane:
    if (!document.has_workplane_id(relation.source_plane())) return Result<std::size_t>::failure(Error::validation(object_id, "plane offset source plane must exist in part document"));
    if (document.find_parameter(relation.offset_parameter()) == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "plane offset parameter must exist in part document"));
    break;
  case ConstructionRelationType::LineThroughTwoPoints:
    if (document.find_construction_point(relation.first_point()) == nullptr || document.find_construction_point(relation.second_point()) == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "line through two points references must exist in part document"));
    break;
  case ConstructionRelationType::PlaneThroughThreePoints: {
    const ConstructionPoint* first = document.find_construction_point(relation.first_point());
    const ConstructionPoint* second = document.find_construction_point(relation.second_point());
    const ConstructionPoint* third = document.find_construction_point(relation.third_point());
    if (first == nullptr || second == nullptr || third == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "plane through three points references must exist in part document"));
    if (are_collinear(first->position(), second->position(), third->position())) return Result<std::size_t>::failure(Error::validation(object_id, "plane through three points requires non-collinear points"));
    break;
  }
  case ConstructionRelationType::PointOnPlane:
    if (document.find_construction_point(relation.first_point()) == nullptr || !document.has_workplane_id(relation.source_plane())) return Result<std::size_t>::failure(Error::validation(object_id, "point-on-plane relation references must exist in part document"));
    break;
  case ConstructionRelationType::PointOnLine:
    if (document.find_construction_point(relation.first_point()) == nullptr || document.find_construction_line(relation.source_line()) == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "point-on-line relation references must exist in part document"));
    break;
  case ConstructionRelationType::PointOnGeneratedEdge:
    if (!relation.generated_edge().has_value()) return Result<std::size_t>::failure(Error::validation(object_id, "point-on-generated-edge relation must carry a generated edge reference"));
    break;
  case ConstructionRelationType::PointOnGeneratedVertex:
    if (!relation.generated_vertex().has_value()) return Result<std::size_t>::failure(Error::validation(object_id, "point-on-generated-vertex relation must carry a generated vertex reference"));
    break;
  case ConstructionRelationType::LineOnPlane:
    if (document.find_construction_line(relation.source_line()) == nullptr || !document.has_workplane_id(relation.source_plane())) return Result<std::size_t>::failure(Error::validation(object_id, "line-on-plane relation references must exist in part document"));
    break;
  case ConstructionRelationType::PlaneParallelToPlaneThroughPoint:
    if (!document.has_workplane_id(relation.source_plane()) || document.find_construction_point(relation.first_point()) == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "plane parallel to plane through point references must exist in part document"));
    break;
  case ConstructionRelationType::LineParallelToLineThroughPoint:
    if (document.find_construction_line(relation.source_line()) == nullptr || document.find_construction_point(relation.first_point()) == nullptr) return Result<std::size_t>::failure(Error::validation(object_id, "line parallel to line through point references must exist in part document"));
    break;
  case ConstructionRelationType::LineParallelToGeneratedEdgeThroughPoint:
    if (document.find_construction_point(relation.first_point()) == nullptr || !relation.generated_edge().has_value()) return Result<std::size_t>::failure(Error::validation(object_id, "line parallel to generated edge through point references must exist in part document"));
    return validate_generated_edge_reference(document, relation.generated_edge().value(), object_id);
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> add_parameter_dependencies(DependencyGraph& graph, const std::vector<ParameterId>& parameter_dependencies, const std::string& dependent_id) {
  for (const auto& parameter_id : parameter_dependencies) { auto dependency = add_dependency_if_missing(graph, parameter_id.value(), dependent_id); if (dependency.has_error()) return dependency; }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> add_relation_dependencies(DependencyGraph& graph, const ConstructionRelation& relation, const std::string& dependent_id) {
  for (const std::string& referenced_node_id : relation.referenced_node_ids()) { auto dependency = add_dependency_if_missing(graph, referenced_node_id, dependent_id); if (dependency.has_error()) return dependency; }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> validate_projected_sketch_references(const PartDocument& document, const Sketch& sketch) {
  for (const auto& point : sketch.projected_points()) {
    if (point.source() == ProjectedSketchPointSource::ConstructionPoint) {
      if (document.find_construction_point(point.construction_point()) == nullptr) return Result<std::size_t>::failure(Error::validation(point.id().value(), "projected construction point must exist in part document"));
    } else if (point.semantic_vertex().has_value()) {
      auto valid_vertex = validate_generated_vertex_reference(document, point.semantic_vertex().value(), point.id().value()); if (valid_vertex.has_error()) return valid_vertex;
    }
  }
  for (const auto& line : sketch.projected_lines()) {
    if (line.source() == ProjectedSketchLineSource::ConstructionLine) {
      if (document.find_construction_line(line.construction_line()) == nullptr) return Result<std::size_t>::failure(Error::validation(line.id().value(), "projected construction line must exist in part document"));
    } else if (line.semantic_edge().has_value()) {
      auto valid_edge = validate_generated_edge_reference(document, line.semantic_edge().value(), line.id().value()); if (valid_edge.has_error()) return valid_edge;
    }
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> add_semantic_reference_dependency(DependencyGraph& graph, const FeatureId& source_feature, const std::string& reference_node_id, const std::string& sketch_id) {
  auto source_to_reference = add_dependency_if_missing(graph, source_feature.value(), reference_node_id); if (source_to_reference.has_error()) return source_to_reference;
  return add_dependency_if_missing(graph, reference_node_id, sketch_id);
}

[[nodiscard]] Result<std::size_t> add_projected_reference_dependencies(DependencyGraph& graph, const Sketch& sketch) {
  for (const auto& point : sketch.projected_points()) {
    if (point.source() == ProjectedSketchPointSource::ConstructionPoint) { auto dependency = add_dependency_if_missing(graph, point.construction_point().value(), sketch.id().value()); if (dependency.has_error()) return dependency; }
    else { const auto& reference = point.semantic_vertex().value(); auto dependency = add_semantic_reference_dependency(graph, reference.source_feature(), reference.node_id(), sketch.id().value()); if (dependency.has_error()) return dependency; }
  }
  for (const auto& line : sketch.projected_lines()) {
    if (line.source() == ProjectedSketchLineSource::ConstructionLine) { auto dependency = add_dependency_if_missing(graph, line.construction_line().value(), sketch.id().value()); if (dependency.has_error()) return dependency; }
    else { const auto& reference = line.semantic_edge().value(); auto dependency = add_semantic_reference_dependency(graph, reference.source_feature(), reference.node_id(), sketch.id().value()); if (dependency.has_error()) return dependency; }
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

} // namespace

Result<PartDocument> PartDocument::create(DocumentId id, std::string name) { const auto object_id = id.empty() ? std::string("part_document") : id.value(); if (id.empty()) return Result<PartDocument>::failure(Error::validation(object_id, "part document id must not be empty")); if (name.empty()) return Result<PartDocument>::failure(Error::validation(object_id, "part document name must not be empty")); return Result<PartDocument>::success(PartDocument(std::move(id), std::move(name))); }

Result<std::size_t> PartDocument::add_parameter(Parameter parameter) { if (has_parameter_id(parameter.id())) return Result<std::size_t>::failure(Error::validation(parameter.id().value(), "parameter id must be unique within part document")); if (has_parameter_name(parameter.name())) return Result<std::size_t>::failure(Error::validation(parameter.id().value(), "parameter name must be unique within part document")); auto graph = dependency_graph_; const auto added_node = graph.add_node(parameter.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); parameters_.push_back(std::move(parameter)); return Result<std::size_t>::success(parameters_.size() - 1); }

Result<std::size_t> PartDocument::add_datum_plane(DatumPlane datum_plane) { if (has_datum_plane_id(datum_plane.id())) return Result<std::size_t>::failure(Error::validation(datum_plane.id().value(), "datum plane id must be unique within part document")); if (has_derived_workplane_id(datum_plane.id()) || has_construction_plane_id(ConstructionPlaneId(datum_plane.id().value()))) return Result<std::size_t>::failure(Error::validation(datum_plane.id().value(), "workplane id must be unique within part document")); datum_planes_.push_back(std::move(datum_plane)); return Result<std::size_t>::success(datum_planes_.size() - 1); }

Result<std::size_t> PartDocument::add_construction_point(ConstructionPoint point) { if (has_construction_point_id(point.id())) return Result<std::size_t>::failure(Error::validation(point.id().value(), "construction point id must be unique within part document")); for (const auto& parameter_id : point.parameter_dependencies()) if (!has_parameter_id(parameter_id)) return Result<std::size_t>::failure(Error::validation(point.id().value(), "construction point parameter dependency must exist in part document")); if (point.relation().has_value()) { auto valid_relation = validate_relation_references(*this, point.relation().value(), point.id().value()); if (valid_relation.has_error()) return Result<std::size_t>::failure(valid_relation.error()); } auto graph = dependency_graph_; const auto added_node = graph.add_node(point.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); const auto parameter_dependencies = add_parameter_dependencies(graph, point.parameter_dependencies(), point.id().value()); if (parameter_dependencies.has_error()) return Result<std::size_t>::failure(parameter_dependencies.error()); if (point.relation().has_value()) { const auto relation_dependencies = add_relation_dependencies(graph, point.relation().value(), point.id().value()); if (relation_dependencies.has_error()) return Result<std::size_t>::failure(relation_dependencies.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); construction_points_.push_back(std::move(point)); return Result<std::size_t>::success(construction_points_.size() - 1); }

Result<std::size_t> PartDocument::add_construction_line(ConstructionLine line) { if (has_construction_line_id(line.id())) return Result<std::size_t>::failure(Error::validation(line.id().value(), "construction line id must be unique within part document")); for (const auto& parameter_id : line.parameter_dependencies()) if (!has_parameter_id(parameter_id)) return Result<std::size_t>::failure(Error::validation(line.id().value(), "construction line parameter dependency must exist in part document")); if (line.kind() != ConstructionLineKind::Explicit) { if (!line.relation().has_value()) return Result<std::size_t>::failure(Error::validation(line.id().value(), "relation-driven construction line must carry a relation")); const ConstructionRelation& relation = line.relation().value(); const bool expected_relation = (line.kind() == ConstructionLineKind::ThroughTwoPoints && relation.type() == ConstructionRelationType::LineThroughTwoPoints) || (line.kind() == ConstructionLineKind::ParallelToLineThroughPoint && relation.type() == ConstructionRelationType::LineParallelToLineThroughPoint) || (line.kind() == ConstructionLineKind::ParallelToGeneratedEdgeThroughPoint && relation.type() == ConstructionRelationType::LineParallelToGeneratedEdgeThroughPoint); if (!expected_relation) return Result<std::size_t>::failure(Error::validation(line.id().value(), "relation-driven construction line kind does not match relation type")); auto valid_relation = validate_relation_references(*this, relation, line.id().value()); if (valid_relation.has_error()) return Result<std::size_t>::failure(valid_relation.error()); } auto graph = dependency_graph_; const auto added_node = graph.add_node(line.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); const auto parameter_dependencies = add_parameter_dependencies(graph, line.parameter_dependencies(), line.id().value()); if (parameter_dependencies.has_error()) return Result<std::size_t>::failure(parameter_dependencies.error()); if (line.relation().has_value()) { const auto relation_dependencies = add_relation_dependencies(graph, line.relation().value(), line.id().value()); if (relation_dependencies.has_error()) return Result<std::size_t>::failure(relation_dependencies.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); construction_lines_.push_back(std::move(line)); return Result<std::size_t>::success(construction_lines_.size() - 1); }

Result<std::size_t> PartDocument::add_construction_plane(ConstructionPlane plane) { if (has_construction_plane_id(plane.id())) return Result<std::size_t>::failure(Error::validation(plane.id().value(), "construction plane id must be unique within part document")); if (has_workplane_id(plane.workplane_id())) return Result<std::size_t>::failure(Error::validation(plane.id().value(), "workplane id must be unique within part document")); for (const auto& parameter_id : plane.parameter_dependencies()) if (!has_parameter_id(parameter_id)) return Result<std::size_t>::failure(Error::validation(plane.id().value(), "construction plane parameter dependency must exist in part document")); if (plane.kind() != ConstructionPlaneKind::Explicit) { if (!plane.relation().has_value()) return Result<std::size_t>::failure(Error::validation(plane.id().value(), "relation-driven construction plane must carry a relation")); const ConstructionRelation& relation = plane.relation().value(); const bool expected_relation = (plane.kind() == ConstructionPlaneKind::OffsetFromPlane && relation.type() == ConstructionRelationType::PlaneOffsetFromPlane) || (plane.kind() == ConstructionPlaneKind::ThroughThreePoints && relation.type() == ConstructionRelationType::PlaneThroughThreePoints) || (plane.kind() == ConstructionPlaneKind::ParallelToPlaneThroughPoint && relation.type() == ConstructionRelationType::PlaneParallelToPlaneThroughPoint); if (!expected_relation) return Result<std::size_t>::failure(Error::validation(plane.id().value(), "relation-driven construction plane kind does not match relation type")); auto valid_relation = validate_relation_references(*this, relation, plane.id().value()); if (valid_relation.has_error()) return Result<std::size_t>::failure(valid_relation.error()); } auto graph = dependency_graph_; const auto added_node = graph.add_node(plane.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); const auto parameter_dependencies = add_parameter_dependencies(graph, plane.parameter_dependencies(), plane.id().value()); if (parameter_dependencies.has_error()) return Result<std::size_t>::failure(parameter_dependencies.error()); if (plane.relation().has_value()) { const auto relation_dependencies = add_relation_dependencies(graph, plane.relation().value(), plane.id().value()); if (relation_dependencies.has_error()) return Result<std::size_t>::failure(relation_dependencies.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); construction_planes_.push_back(std::move(plane)); return Result<std::size_t>::success(construction_planes_.size() - 1); }

Result<std::size_t> PartDocument::add_derived_workplane(DerivedWorkplane workplane) { if (has_workplane_id(workplane.id())) return Result<std::size_t>::failure(Error::validation(workplane.id().value(), "workplane id must be unique within part document")); const Feature* source_feature = find_feature(workplane.face_reference().source_feature()); if (source_feature == nullptr) return Result<std::size_t>::failure(Error::validation(workplane.id().value(), "derived workplane source feature must exist in part document")); if (source_feature->type() != FeatureType::AdditiveExtrude) return Result<std::size_t>::failure(Error::validation(workplane.id().value(), "derived workplane source feature must be an additive extrude")); if (!is_supported_derived_face(workplane.face_reference().face())) return Result<std::size_t>::failure(Error::validation(workplane.id().value(), "only top, bottom, right, left, front, and back semantic faces are supported")); auto graph = dependency_graph_; const auto added_node = graph.add_node(workplane.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); auto source_dependency = add_dependency_if_missing(graph, workplane.face_reference().source_feature().value(), workplane.id().value()); if (source_dependency.has_error()) return Result<std::size_t>::failure(source_dependency.error()); auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); derived_workplanes_.push_back(std::move(workplane)); return Result<std::size_t>::success(derived_workplanes_.size() - 1); }

Result<std::size_t> PartDocument::add_sketch(Sketch sketch) { if (has_sketch_id(sketch.id())) return Result<std::size_t>::failure(Error::validation(sketch.id().value(), "sketch id must be unique within part document")); if (!has_workplane_id(sketch.workplane())) return Result<std::size_t>::failure(Error::validation(sketch.id().value(), "sketch workplane must exist in part document")); auto valid_projected_references = validate_projected_sketch_references(*this, sketch); if (valid_projected_references.has_error()) return Result<std::size_t>::failure(valid_projected_references.error()); for (const auto& profile : sketch.rectangle_profiles()) { if (!has_parameter_id(profile.width_parameter())) return Result<std::size_t>::failure(Error::validation(profile.id().value(), "rectangle width parameter must exist in part document")); if (!has_parameter_id(profile.height_parameter())) return Result<std::size_t>::failure(Error::validation(profile.id().value(), "rectangle height parameter must exist in part document")); } for (const auto& profile : sketch.circle_profiles()) if (!has_parameter_id(profile.diameter_parameter())) return Result<std::size_t>::failure(Error::validation(profile.id().value(), "circle diameter parameter must exist in part document")); auto graph = dependency_graph_; const auto added_node = graph.add_node(sketch.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); if (has_derived_workplane_id(sketch.workplane()) || has_construction_plane_id(ConstructionPlaneId(sketch.workplane().value()))) { auto workplane_dependency = add_dependency_if_missing(graph, sketch.workplane().value(), sketch.id().value()); if (workplane_dependency.has_error()) return Result<std::size_t>::failure(workplane_dependency.error()); } const auto projected_reference_dependencies = add_projected_reference_dependencies(graph, sketch); if (projected_reference_dependencies.has_error()) return Result<std::size_t>::failure(projected_reference_dependencies.error()); for (const auto& profile : sketch.rectangle_profiles()) { auto width_dependency = add_dependency_if_missing(graph, profile.width_parameter().value(), sketch.id().value()); if (width_dependency.has_error()) return Result<std::size_t>::failure(width_dependency.error()); auto height_dependency = add_dependency_if_missing(graph, profile.height_parameter().value(), sketch.id().value()); if (height_dependency.has_error()) return Result<std::size_t>::failure(height_dependency.error()); } for (const auto& profile : sketch.circle_profiles()) { auto diameter_dependency = add_dependency_if_missing(graph, profile.diameter_parameter().value(), sketch.id().value()); if (diameter_dependency.has_error()) return Result<std::size_t>::failure(diameter_dependency.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); sketches_.push_back(std::move(sketch)); return Result<std::size_t>::success(sketches_.size() - 1); }

Result<std::size_t> PartDocument::add_feature(Feature feature) { if (has_feature_id(feature.id())) return Result<std::size_t>::failure(Error::validation(feature.id().value(), "feature id must be unique within part document")); if (!has_sketch_id(feature.input_sketch())) return Result<std::size_t>::failure(Error::validation(feature.id().value(), "feature input sketch must exist in part document")); if (feature.type() == FeatureType::AdditiveExtrude && !has_parameter_id(feature.length_parameter())) return Result<std::size_t>::failure(Error::validation(feature.id().value(), "additive extrude length parameter must exist in part document")); if (feature.type() == FeatureType::SubtractiveExtrude && !has_feature_id(feature.target_feature())) return Result<std::size_t>::failure(Error::validation(feature.id().value(), "subtractive extrude target feature must exist in part document")); auto graph = dependency_graph_; const auto added_node = graph.add_node(feature.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); auto sketch_dependency = add_dependency_if_missing(graph, feature.input_sketch().value(), feature.id().value()); if (sketch_dependency.has_error()) return Result<std::size_t>::failure(sketch_dependency.error()); if (feature.type() == FeatureType::AdditiveExtrude) { auto length_dependency = add_dependency_if_missing(graph, feature.length_parameter().value(), feature.id().value()); if (length_dependency.has_error()) return Result<std::size_t>::failure(length_dependency.error()); } if (feature.type() == FeatureType::SubtractiveExtrude) { auto target_dependency = add_dependency_if_missing(graph, feature.target_feature().value(), feature.id().value()); if (target_dependency.has_error()) return Result<std::size_t>::failure(target_dependency.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); features_.push_back(std::move(feature)); return Result<std::size_t>::success(features_.size() - 1); }

Result<std::size_t> PartDocument::add_reference_status(ReferenceStatusRecord status) { if (has_reference_status_id(status.id())) return Result<std::size_t>::failure(Error::validation(status.id().value(), "reference status id must be unique within part document")); if (status.status() == ReferenceStatusKind::Resolved) { auto valid_target = validate_semantic_reference_target(*this, status.target(), status.id().value()); if (valid_target.has_error()) return Result<std::size_t>::failure(valid_target.error()); } auto graph = dependency_graph_; const auto added_node = graph.add_node(status.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); if (find_feature(status.target().source_feature()) != nullptr) { auto dependency = add_dependency_if_missing(graph, status.target().source_feature().value(), status.id().value()); if (dependency.has_error()) return Result<std::size_t>::failure(dependency.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); reference_statuses_.push_back(std::move(status)); return Result<std::size_t>::success(reference_statuses_.size() - 1); }

Result<std::size_t> PartDocument::add_reference_remap(ReferenceRemapRecord remap) { if (has_reference_remap_id(remap.id())) return Result<std::size_t>::failure(Error::validation(remap.id().value(), "reference remap id must be unique within part document")); auto valid_replacement = validate_semantic_reference_target(*this, remap.replacement(), remap.id().value()); if (valid_replacement.has_error()) return Result<std::size_t>::failure(valid_replacement.error()); auto graph = dependency_graph_; const auto added_node = graph.add_node(remap.id().value()); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); auto replacement_dependency = add_dependency_if_missing(graph, remap.replacement().source_feature().value(), remap.id().value()); if (replacement_dependency.has_error()) return Result<std::size_t>::failure(replacement_dependency.error()); if (find_feature(remap.original().source_feature()) != nullptr) { auto original_dependency = add_dependency_if_missing(graph, remap.original().source_feature().value(), remap.id().value()); if (original_dependency.has_error()) return Result<std::size_t>::failure(original_dependency.error()); } auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); reference_remaps_.push_back(std::move(remap)); return Result<std::size_t>::success(reference_remaps_.size() - 1); }

Result<std::size_t> PartDocument::add_sketch_origin_override(SketchOriginOverrideRecord origin_override) { if (has_sketch_origin_override_id(origin_override.sketch())) return Result<std::size_t>::failure(Error::validation(origin_override.sketch().value(), "sketch origin override must be unique per sketch")); if (find_sketch(origin_override.sketch()) == nullptr) return Result<std::size_t>::failure(Error::validation(origin_override.sketch().value(), "sketch origin override sketch must exist in part document")); auto graph = dependency_graph_; const std::string node_id = origin_override.sketch().value() + ".origin_override"; const auto added_node = graph.add_node(node_id); if (added_node.has_error()) return Result<std::size_t>::failure(added_node.error()); auto dependency = add_dependency_if_missing(graph, origin_override.sketch().value(), node_id); if (dependency.has_error()) return Result<std::size_t>::failure(dependency.error()); auto invalidation_state = invalidation_state_; const auto synced_state = invalidation_state.sync_from_graph(graph); if (synced_state.has_error()) return Result<std::size_t>::failure(synced_state.error()); dependency_graph_ = std::move(graph); invalidation_state_ = std::move(invalidation_state); sketch_origin_overrides_.push_back(std::move(origin_override)); return Result<std::size_t>::success(sketch_origin_overrides_.size() - 1); }

Result<std::vector<std::string>> PartDocument::mark_parameter_changed(ParameterId id) { if (id.empty()) return Result<std::vector<std::string>>::failure(Error::validation("parameter", "parameter id must not be empty")); if (!has_parameter_id(id)) return Result<std::vector<std::string>>::failure(Error::validation(id.value(), "parameter must exist in part document")); return invalidation_state_.mark_changed(dependency_graph_, id.value()); }
Result<std::vector<std::string>> PartDocument::set_parameter_value(ParameterId id, Quantity value) { if (id.empty()) return Result<std::vector<std::string>>::failure(Error::validation("parameter", "parameter id must not be empty")); const Parameter* existing = find_parameter(id); if (existing == nullptr) return Result<std::vector<std::string>>::failure(Error::validation(id.value(), "parameter must exist in part document")); auto updated = existing->with_value(value); if (updated.has_error()) return Result<std::vector<std::string>>::failure(updated.error()); for (auto& parameter : parameters_) if (parameter.id() == id) { parameter = std::move(updated.value()); break; } return invalidation_state_.mark_changed(dependency_graph_, id.value()); }
Result<RecomputePlan> PartDocument::create_recompute_plan() const { return RecomputePlan::from_graph_and_invalidation_state(dependency_graph_, invalidation_state_); }
void PartDocument::mark_all_clean() noexcept { invalidation_state_.mark_all_clean(); }

const DocumentId& PartDocument::id() const noexcept { return id_; }
const std::string& PartDocument::name() const noexcept { return name_; }
const std::vector<Parameter>& PartDocument::parameters() const noexcept { return parameters_; }
const std::vector<DatumPlane>& PartDocument::datum_planes() const noexcept { return datum_planes_; }
const std::vector<ConstructionPoint>& PartDocument::construction_points() const noexcept { return construction_points_; }
const std::vector<ConstructionLine>& PartDocument::construction_lines() const noexcept { return construction_lines_; }
const std::vector<ConstructionPlane>& PartDocument::construction_planes() const noexcept { return construction_planes_; }
const std::vector<DerivedWorkplane>& PartDocument::derived_workplanes() const noexcept { return derived_workplanes_; }
const std::vector<Sketch>& PartDocument::sketches() const noexcept { return sketches_; }
const std::vector<Feature>& PartDocument::features() const noexcept { return features_; }
const std::vector<ReferenceStatusRecord>& PartDocument::reference_statuses() const noexcept { return reference_statuses_; }
const std::vector<ReferenceRemapRecord>& PartDocument::reference_remaps() const noexcept { return reference_remaps_; }
const std::vector<SketchOriginOverrideRecord>& PartDocument::sketch_origin_overrides() const noexcept { return sketch_origin_overrides_; }
const DependencyGraph& PartDocument::dependency_graph() const noexcept { return dependency_graph_; }
const InvalidationState& PartDocument::invalidation_state() const noexcept { return invalidation_state_; }
std::size_t PartDocument::parameter_count() const noexcept { return parameters_.size(); }
std::size_t PartDocument::datum_plane_count() const noexcept { return datum_planes_.size(); }
std::size_t PartDocument::construction_point_count() const noexcept { return construction_points_.size(); }
std::size_t PartDocument::construction_line_count() const noexcept { return construction_lines_.size(); }
std::size_t PartDocument::construction_plane_count() const noexcept { return construction_planes_.size(); }
std::size_t PartDocument::derived_workplane_count() const noexcept { return derived_workplanes_.size(); }
std::size_t PartDocument::sketch_count() const noexcept { return sketches_.size(); }
std::size_t PartDocument::feature_count() const noexcept { return features_.size(); }
std::size_t PartDocument::reference_status_count() const noexcept { return reference_statuses_.size(); }
std::size_t PartDocument::reference_remap_count() const noexcept { return reference_remaps_.size(); }
std::size_t PartDocument::sketch_origin_override_count() const noexcept { return sketch_origin_overrides_.size(); }
const Parameter* PartDocument::find_parameter(ParameterId id) const noexcept { for (const auto& parameter : parameters_) if (parameter.id() == id) return &parameter; return nullptr; }
const Parameter* PartDocument::find_parameter(std::string_view name) const noexcept { for (const auto& parameter : parameters_) if (parameter.name() == name) return &parameter; return nullptr; }
const DatumPlane* PartDocument::find_datum_plane(DatumPlaneId id) const noexcept { for (const auto& datum_plane : datum_planes_) if (datum_plane.id() == id) return &datum_plane; return nullptr; }
const ConstructionPoint* PartDocument::find_construction_point(ConstructionPointId id) const noexcept { for (const auto& point : construction_points_) if (point.id() == id) return &point; return nullptr; }
const ConstructionLine* PartDocument::find_construction_line(ConstructionLineId id) const noexcept { for (const auto& line : construction_lines_) if (line.id() == id) return &line; return nullptr; }
const ConstructionPlane* PartDocument::find_construction_plane(ConstructionPlaneId id) const noexcept { for (const auto& plane : construction_planes_) if (plane.id() == id) return &plane; return nullptr; }
const DerivedWorkplane* PartDocument::find_derived_workplane(DatumPlaneId id) const noexcept { for (const auto& workplane : derived_workplanes_) if (workplane.id() == id) return &workplane; return nullptr; }
const Sketch* PartDocument::find_sketch(SketchId id) const noexcept { for (const auto& sketch : sketches_) if (sketch.id() == id) return &sketch; return nullptr; }
const Feature* PartDocument::find_feature(FeatureId id) const noexcept { for (const auto& feature : features_) if (feature.id() == id) return &feature; return nullptr; }
const ReferenceStatusRecord* PartDocument::find_reference_status(ReferenceStatusId id) const noexcept { for (const auto& status : reference_statuses_) if (status.id() == id) return &status; return nullptr; }
const ReferenceRemapRecord* PartDocument::find_reference_remap(ReferenceRemapId id) const noexcept { for (const auto& remap : reference_remaps_) if (remap.id() == id) return &remap; return nullptr; }
const SketchOriginOverrideRecord* PartDocument::find_sketch_origin_override(SketchId id) const noexcept { for (const auto& origin : sketch_origin_overrides_) if (origin.sketch() == id) return &origin; return nullptr; }
bool PartDocument::has_workplane_id(const DatumPlaneId& id) const noexcept { return has_datum_plane_id(id) || has_derived_workplane_id(id) || has_construction_plane_id(ConstructionPlaneId(id.value())); }
PartDocument::PartDocument(DocumentId id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}
bool PartDocument::has_parameter_id(const ParameterId& id) const noexcept { return find_parameter(id) != nullptr; }
bool PartDocument::has_parameter_name(std::string_view name) const noexcept { return find_parameter(name) != nullptr; }
bool PartDocument::has_datum_plane_id(const DatumPlaneId& id) const noexcept { return find_datum_plane(id) != nullptr; }
bool PartDocument::has_construction_point_id(const ConstructionPointId& id) const noexcept { return find_construction_point(id) != nullptr; }
bool PartDocument::has_construction_line_id(const ConstructionLineId& id) const noexcept { return find_construction_line(id) != nullptr; }
bool PartDocument::has_construction_plane_id(const ConstructionPlaneId& id) const noexcept { return find_construction_plane(id) != nullptr; }
bool PartDocument::has_derived_workplane_id(const DatumPlaneId& id) const noexcept { return find_derived_workplane(id) != nullptr; }
bool PartDocument::has_sketch_id(const SketchId& id) const noexcept { return find_sketch(id) != nullptr; }
bool PartDocument::has_feature_id(const FeatureId& id) const noexcept { return find_feature(id) != nullptr; }
bool PartDocument::has_reference_status_id(const ReferenceStatusId& id) const noexcept { return find_reference_status(id) != nullptr; }
bool PartDocument::has_reference_remap_id(const ReferenceRemapId& id) const noexcept { return find_reference_remap(id) != nullptr; }
bool PartDocument::has_sketch_origin_override_id(const SketchId& id) const noexcept { return find_sketch_origin_override(id) != nullptr; }

} // namespace blcad
