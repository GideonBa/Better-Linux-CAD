#include "blcad/core/part_document.hpp"

#include "blcad/core/parameter_expression.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] std::string body_dependency_node_id(const BodyId& id) {
  return "body:" + id.value();
}

[[nodiscard]] std::string sketch_ownership_node_id(const SketchId& id) {
  return "sketch_ownership:" + id.value();
}

[[nodiscard]] std::vector<std::string> dependency_sources_of(const DependencyGraph& graph,
                                                             std::string_view dependent) {
  std::vector<std::string> sources;
  for (const auto& edge : graph.dependencies())
    if (edge.second == dependent)
      sources.push_back(edge.first);
  return sources;
}

Result<std::size_t> add_dependency_if_missing(DependencyGraph& graph, std::string dependency,
                                              std::string dependent) {
  if (graph.has_dependency(dependency, dependent)) {
    return Result<std::size_t>::success(graph.dependency_count());
  }
  return graph.add_dependency(std::move(dependency), std::move(dependent));
}

[[nodiscard]] std::string reference_generated_line_node_id(const Sketch& sketch,
                                                           const ReferenceGeneratedLine& line) {
  return sketch.id().value() + ".reference_generated_line." + line.id().value();
}

[[nodiscard]] Vector3 vector_between(Point3 from, Point3 to) noexcept {
  return Vector3{to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept {
  return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                 left.x * right.y - left.y * right.x};
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

[[nodiscard]] bool are_collinear(Point3 first, Point3 second, Point3 third) noexcept {
  return length(cross(vector_between(first, second), vector_between(first, third))) <= k_tolerance;
}

[[nodiscard]] bool is_supported_derived_face(SemanticFace face) noexcept {
  return face == SemanticFace::Top || face == SemanticFace::Bottom || face == SemanticFace::Right ||
         face == SemanticFace::Left || face == SemanticFace::Front || face == SemanticFace::Back;
}

[[nodiscard]] bool has_additive_source_feature(const PartDocument& document, FeatureId feature_id) {
  const Feature* feature = document.find_feature(feature_id);
  return feature != nullptr && feature->type() == FeatureType::AdditiveExtrude;
}

[[nodiscard]] Result<std::size_t>
validate_generated_edge_reference(const PartDocument& document,
                                  const SemanticEdgeReference& reference,
                                  const std::string& object_id) {
  if (!has_additive_source_feature(document, reference.source_feature())) {
    return Result<std::size_t>::failure(Error::validation(
        object_id, "semantic edge source feature must be an additive extrude in part document"));
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t>
validate_generated_vertex_reference(const PartDocument& document,
                                    const SemanticVertexReference& reference,
                                    const std::string& object_id) {
  if (!has_additive_source_feature(document, reference.source_feature())) {
    return Result<std::size_t>::failure(Error::validation(
        object_id, "semantic vertex source feature must be an additive extrude in part document"));
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_extrude_length_parameter(const PartDocument& document,
                                                                    const ParameterId& parameter_id,
                                                                    const std::string& object_id,
                                                                    std::string_view role) {
  const Parameter* parameter = document.find_parameter(parameter_id);
  if (parameter == nullptr || parameter->type() != ParameterType::Length)
    return Result<std::size_t>::failure(Error::validation(
        object_id, std::string(role) + " must reference an existing length parameter"));
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> validate_extrude_feature_intent(const PartDocument& document,
                                                                  const Feature& feature) {
  const auto& extent = feature.extrude_intent().extent();
  for (const auto* parameter :
       {&extent.first_distance_parameter(), &extent.second_distance_parameter()}) {
    if (!parameter->has_value())
      continue;
    auto valid = validate_extrude_length_parameter(document, **parameter, feature.id().value(),
                                                   "extrude distance");
    if (valid.has_error())
      return valid;
  }
  for (const auto* face : {&extent.first_face(), &extent.second_face()}) {
    if (!face->has_value())
      continue;
    auto valid = validate_part_feature_input_reference(document, **face);
    if (valid.has_error())
      return valid;
  }
  if (feature.extrude_intent().thin().has_value()) {
    const auto& thin = *feature.extrude_intent().thin();
    auto first = validate_extrude_length_parameter(document, thin.first_thickness_parameter(),
                                                   feature.id().value(), "thin thickness");
    if (first.has_error())
      return first;
    if (thin.second_thickness_parameter().has_value()) {
      auto second = validate_extrude_length_parameter(document, *thin.second_thickness_parameter(),
                                                      feature.id().value(), "thin thickness");
      if (second.has_error())
        return second;
    }
  }
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> add_extrude_feature_dependencies(DependencyGraph& graph,
                                                                   const Feature& feature) {
  const auto& extent = feature.extrude_intent().extent();
  for (const auto* parameter :
       {&extent.first_distance_parameter(), &extent.second_distance_parameter()}) {
    if (!parameter->has_value())
      continue;
    auto dependency = add_dependency_if_missing(graph, (**parameter).value(), feature.id().value());
    if (dependency.has_error())
      return dependency;
  }
  for (const auto* face : {&extent.first_face(), &extent.second_face()}) {
    if (!face->has_value())
      continue;
    auto dependency =
        add_dependency_if_missing(graph, (**face).source_node_id(), feature.id().value());
    if (dependency.has_error())
      return dependency;
  }
  if (feature.extrude_intent().thin().has_value()) {
    const auto& thin = *feature.extrude_intent().thin();
    auto first = add_dependency_if_missing(graph, thin.first_thickness_parameter().value(),
                                           feature.id().value());
    if (first.has_error())
      return first;
    if (thin.second_thickness_parameter().has_value()) {
      auto second = add_dependency_if_missing(graph, thin.second_thickness_parameter()->value(),
                                              feature.id().value());
      if (second.has_error())
        return second;
    }
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t>
validate_semantic_reference_target(const PartDocument& document,
                                   const SemanticReferenceTarget& target,
                                   const std::string& object_id) {
  if (!has_additive_source_feature(document, target.source_feature())) {
    return Result<std::size_t>::failure(Error::validation(
        object_id,
        "semantic reference source feature must be an additive extrude in part document"));
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_relation_references(const PartDocument& document,
                                                               const ConstructionRelation& relation,
                                                               const std::string& object_id) {
  switch (relation.type()) {
  case ConstructionRelationType::PlaneOffsetFromPlane:
    if (!document.has_workplane_id(relation.source_plane())) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "plane offset source plane must exist in part document"));
    }
    if (document.find_parameter(relation.offset_parameter()) == nullptr) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "plane offset parameter must exist in part document"));
    }
    break;
  case ConstructionRelationType::LineThroughTwoPoints:
    if (document.find_construction_point(relation.first_point()) == nullptr ||
        document.find_construction_point(relation.second_point()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "line through two points references must exist in part document"));
    }
    break;
  case ConstructionRelationType::PlaneThroughThreePoints: {
    const ConstructionPoint* first = document.find_construction_point(relation.first_point());
    const ConstructionPoint* second = document.find_construction_point(relation.second_point());
    const ConstructionPoint* third = document.find_construction_point(relation.third_point());
    if (first == nullptr || second == nullptr || third == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "plane through three points references must exist in part document"));
    }
    if (are_collinear(first->position(), second->position(), third->position())) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "plane through three points requires non-collinear points"));
    }
    break;
  }
  case ConstructionRelationType::PointOnPlane:
    if (document.find_construction_point(relation.first_point()) == nullptr ||
        !document.has_workplane_id(relation.source_plane())) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "point-on-plane relation references must exist in part document"));
    }
    break;
  case ConstructionRelationType::PointOnLine:
    if (document.find_construction_point(relation.first_point()) == nullptr ||
        document.find_construction_line(relation.source_line()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "point-on-line relation references must exist in part document"));
    }
    break;
  case ConstructionRelationType::PointOnGeneratedEdge:
    if (!relation.generated_edge().has_value()) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "point-on-generated-edge relation must carry a generated edge reference"));
    }
    break;
  case ConstructionRelationType::PointOnGeneratedVertex:
    if (!relation.generated_vertex().has_value()) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "point-on-generated-vertex relation must carry a generated vertex reference"));
    }
    break;
  case ConstructionRelationType::LineOnPlane:
    if (document.find_construction_line(relation.source_line()) == nullptr ||
        !document.has_workplane_id(relation.source_plane())) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "line-on-plane relation references must exist in part document"));
    }
    break;
  case ConstructionRelationType::PlaneParallelToPlaneThroughPoint:
    if (!document.has_workplane_id(relation.source_plane()) ||
        document.find_construction_point(relation.first_point()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          object_id,
          "plane parallel to plane through point references must exist in part document"));
    }
    break;
  case ConstructionRelationType::LineParallelToLineThroughPoint:
    if (document.find_construction_line(relation.source_line()) == nullptr ||
        document.find_construction_point(relation.first_point()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          object_id, "line parallel to line through point references must exist in part document"));
    }
    break;
  case ConstructionRelationType::LineParallelToGeneratedEdgeThroughPoint:
    if (document.find_construction_point(relation.first_point()) == nullptr ||
        !relation.generated_edge().has_value()) {
      return Result<std::size_t>::failure(Error::validation(
          object_id,
          "line parallel to generated edge through point references must exist in part document"));
    }
    return validate_generated_edge_reference(document, relation.generated_edge().value(),
                                             object_id);
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t>
add_parameter_dependencies(DependencyGraph& graph,
                           const std::vector<ParameterId>& parameter_dependencies,
                           const std::string& dependent_id) {
  for (const auto& parameter_id : parameter_dependencies) {
    auto dependency = add_dependency_if_missing(graph, parameter_id.value(), dependent_id);
    if (dependency.has_error())
      return dependency;
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> add_relation_dependencies(DependencyGraph& graph,
                                                            const ConstructionRelation& relation,
                                                            const std::string& dependent_id) {
  for (const std::string& referenced_node_id : relation.referenced_node_ids()) {
    auto dependency = add_dependency_if_missing(graph, referenced_node_id, dependent_id);
    if (dependency.has_error())
      return dependency;
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> validate_projected_sketch_references(const PartDocument& document,
                                                                       const Sketch& sketch) {
  for (const auto& point : sketch.projected_points()) {
    if (point.source() == ProjectedSketchPointSource::ConstructionPoint) {
      if (document.find_construction_point(point.construction_point()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            point.id().value(), "projected construction point must exist in part document"));
      }
    } else if (point.semantic_vertex().has_value()) {
      auto valid_vertex = validate_generated_vertex_reference(
          document, point.semantic_vertex().value(), point.id().value());
      if (valid_vertex.has_error())
        return valid_vertex;
    }
  }
  for (const auto& line : sketch.projected_lines()) {
    if (line.source() == ProjectedSketchLineSource::ConstructionLine) {
      if (document.find_construction_line(line.construction_line()) == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            line.id().value(), "projected construction line must exist in part document"));
      }
    } else if (line.semantic_edge().has_value()) {
      auto valid_edge = validate_generated_edge_reference(document, line.semantic_edge().value(),
                                                          line.id().value());
      if (valid_edge.has_error())
        return valid_edge;
    }
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] const ProjectedSketchPoint*
find_projected_point_target(const Sketch& sketch, const SketchReferenceTarget& target) noexcept {
  if (target.kind() != SketchReferenceTargetKind::ProjectedPoint)
    return nullptr;
  return sketch.find_projected_point(target.entity());
}

[[nodiscard]] const ProjectedSketchLine*
find_projected_line_target(const Sketch& sketch, const SketchReferenceTarget& target) noexcept {
  if (target.kind() != SketchReferenceTargetKind::ProjectedLine)
    return nullptr;
  return sketch.find_projected_line(target.entity());
}

[[nodiscard]] Result<std::size_t>
add_semantic_reference_dependency(DependencyGraph& graph, const FeatureId& source_feature,
                                  const std::string& reference_node_id,
                                  const std::string& dependent_id) {
  auto source_to_reference =
      add_dependency_if_missing(graph, source_feature.value(), reference_node_id);
  if (source_to_reference.has_error())
    return source_to_reference;
  return add_dependency_if_missing(graph, reference_node_id, dependent_id);
}

[[nodiscard]] Result<std::size_t> add_projected_point_dependency(DependencyGraph& graph,
                                                                 const ProjectedSketchPoint& point,
                                                                 const std::string& dependent_id) {
  if (point.source() == ProjectedSketchPointSource::ConstructionPoint) {
    return add_dependency_if_missing(graph, point.construction_point().value(), dependent_id);
  }
  const auto& reference = point.semantic_vertex().value();
  return add_semantic_reference_dependency(graph, reference.source_feature(), reference.node_id(),
                                           dependent_id);
}

[[nodiscard]] Result<std::size_t> add_projected_line_dependency(DependencyGraph& graph,
                                                                const ProjectedSketchLine& line,
                                                                const std::string& dependent_id) {
  if (line.source() == ProjectedSketchLineSource::ConstructionLine) {
    return add_dependency_if_missing(graph, line.construction_line().value(), dependent_id);
  }
  const auto& reference = line.semantic_edge().value();
  return add_semantic_reference_dependency(graph, reference.source_feature(), reference.node_id(),
                                           dependent_id);
}

[[nodiscard]] Result<std::size_t>
add_constraint_reference_dependency(DependencyGraph& graph, const Sketch& sketch,
                                    const SketchConstraint& constraint,
                                    const std::string& dependent_id) {
  if (const ProjectedSketchPoint* point =
          find_projected_point_target(sketch, constraint.reference_target())) {
    return add_projected_point_dependency(graph, *point, dependent_id);
  }
  if (const ProjectedSketchLine* line =
          find_projected_line_target(sketch, constraint.reference_target())) {
    return add_projected_line_dependency(graph, *line, dependent_id);
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> validate_reference_generated_lines(const Sketch& sketch) {
  for (const auto& line : sketch.reference_generated_lines()) {
    const SketchConstraint* start = sketch.find_constraint(line.start_constraint());
    const SketchConstraint* end = sketch.find_constraint(line.end_constraint());
    if (start == nullptr || end == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          line.id().value(), "reference-generated line endpoint constraints must exist in sketch"));
    }
    if (start->kind() != SketchConstraintKind::CoincidentToProjectedPoint ||
        end->kind() != SketchConstraintKind::CoincidentToProjectedPoint ||
        start->constrained_target().entity() != line.id() ||
        end->constrained_target().entity() != line.id() ||
        start->constrained_target().kind() != SketchReferenceTargetKind::LineSegmentStart ||
        end->constrained_target().kind() != SketchReferenceTargetKind::LineSegmentEnd) {
      return Result<std::size_t>::failure(
          Error::validation(line.id().value(), "reference-generated line endpoints must be defined "
                                               "by start/end projected-point constraints"));
    }
    if (line.direction_constraint().has_value()) {
      const SketchConstraint* direction =
          sketch.find_constraint(line.direction_constraint().value());
      if (direction == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            line.id().value(),
            "reference-generated line direction constraint must exist in sketch"));
      }
      if ((direction->kind() != SketchConstraintKind::ParallelToProjectedLine &&
           direction->kind() != SketchConstraintKind::CollinearWithProjectedLine) ||
          direction->constrained_target().entity() != line.id() ||
          direction->constrained_target().kind() != SketchReferenceTargetKind::LineSegment) {
        return Result<std::size_t>::failure(
            Error::validation(line.id().value(), "reference-generated line direction must be a "
                                                 "projected-line constraint on the helper line"));
      }
    }
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> validate_driving_dimensions(const PartDocument& document,
                                                              const Sketch& sketch) {
  for (const auto& dimension : sketch.driving_dimensions()) {
    if (document.find_parameter(dimension.parameter()) == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          dimension.id().value(), "driving dimension parameter must exist in part document"));
    }
  }
  return Result<std::size_t>::success(0);
}

[[nodiscard]] Result<std::size_t> add_projected_reference_dependencies(DependencyGraph& graph,
                                                                       const Sketch& sketch) {
  for (const auto& point : sketch.projected_points()) {
    auto dependency = add_projected_point_dependency(graph, point, sketch.id().value());
    if (dependency.has_error())
      return dependency;
  }
  for (const auto& line : sketch.projected_lines()) {
    auto dependency = add_projected_line_dependency(graph, line, sketch.id().value());
    if (dependency.has_error())
      return dependency;
  }
  for (const auto& line : sketch.reference_generated_lines()) {
    const std::string line_node = reference_generated_line_node_id(sketch, line);
    auto node = graph.add_node(line_node);
    if (node.has_error())
      return node;
    const SketchConstraint* start = sketch.find_constraint(line.start_constraint());
    const SketchConstraint* end = sketch.find_constraint(line.end_constraint());
    if (start == nullptr || end == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          line.id().value(), "reference-generated line endpoint constraints must exist in sketch"));
    }
    auto start_dependency = add_constraint_reference_dependency(graph, sketch, *start, line_node);
    if (start_dependency.has_error())
      return start_dependency;
    auto end_dependency = add_constraint_reference_dependency(graph, sketch, *end, line_node);
    if (end_dependency.has_error())
      return end_dependency;
    if (line.direction_constraint().has_value()) {
      const SketchConstraint* direction =
          sketch.find_constraint(line.direction_constraint().value());
      if (direction == nullptr) {
        return Result<std::size_t>::failure(Error::validation(
            line.id().value(),
            "reference-generated line direction constraint must exist in sketch"));
      }
      auto direction_dependency =
          add_constraint_reference_dependency(graph, sketch, *direction, line_node);
      if (direction_dependency.has_error())
        return direction_dependency;
    }
    auto sketch_dependency = add_dependency_if_missing(graph, line_node, sketch.id().value());
    if (sketch_dependency.has_error())
      return sketch_dependency;
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> add_driving_dimension_dependencies(DependencyGraph& graph,
                                                                     const Sketch& sketch) {
  for (const auto& dimension : sketch.driving_dimensions()) {
    auto dependency =
        add_dependency_if_missing(graph, dimension.parameter().value(), sketch.id().value());
    if (dependency.has_error())
      return dependency;
  }
  return Result<std::size_t>::success(graph.dependency_count());
}

[[nodiscard]] Result<std::size_t> sync_graph(DependencyGraph graph,
                                             InvalidationState& invalidation_state,
                                             DependencyGraph& dependency_graph) {
  auto synced_state = invalidation_state.sync_from_graph(graph);
  if (synced_state.has_error())
    return Result<std::size_t>::failure(synced_state.error());
  dependency_graph = std::move(graph);
  return Result<std::size_t>::success(dependency_graph.node_count());
}

} // namespace

Result<PartDocument> PartDocument::create(DocumentId id, std::string name) {
  const auto object_id = id.empty() ? std::string("part_document") : id.value();
  if (id.empty())
    return Result<PartDocument>::failure(
        Error::validation(object_id, "part document id must not be empty"));
  if (name.empty())
    return Result<PartDocument>::failure(
        Error::validation(object_id, "part document name must not be empty"));
  return Result<PartDocument>::success(PartDocument(std::move(id), std::move(name)));
}

Result<std::size_t> PartDocument::add_parameter(Parameter parameter) {
  if (has_parameter_id(parameter.id()))
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter id must be unique within part document"));
  if (has_parameter_name(parameter.name()))
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter name must be unique within part document"));
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(parameter.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  parameters_.push_back(std::move(parameter));
  return Result<std::size_t>::success(parameters_.size() - 1U);
}

Result<std::size_t> PartDocument::add_expression_parameter(ParameterId id, std::string name,
                                                           ParameterType type,
                                                           std::string formula) {
  const auto object_id = id.empty() ? std::string("parameter") : id.value();
  if (has_parameter_id(id))
    return Result<std::size_t>::failure(
        Error::validation(object_id, "parameter id must be unique within part document"));
  if (has_parameter_name(name))
    return Result<std::size_t>::failure(
        Error::validation(object_id, "parameter name must be unique within part document"));

  const ParameterExpressionEvaluator evaluator;
  auto evaluation = evaluator.evaluate(*this, formula, type, object_id);
  if (evaluation.has_error())
    return Result<std::size_t>::failure(evaluation.error());

  auto parameter = Parameter::create_expression(id, std::move(name), type, std::move(formula),
                                                evaluation.value().value);
  if (parameter.has_error())
    return Result<std::size_t>::failure(parameter.error());

  auto graph = dependency_graph_;
  auto added_node = graph.add_node(id.value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const ParameterId& referenced : evaluation.value().referenced_parameters) {
    auto edge = add_dependency_if_missing(graph, referenced.value(), id.value());
    if (edge.has_error())
      return Result<std::size_t>::failure(edge.error());
  }
  if (graph.has_cycle())
    return Result<std::size_t>::failure(
        Error::dependency(object_id, "expression parameter must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  parameters_.push_back(std::move(parameter.value()));
  return Result<std::size_t>::success(parameters_.size() - 1U);
}

Result<std::size_t> PartDocument::add_datum_plane(DatumPlane datum_plane) {
  if (has_datum_plane_id(datum_plane.id()))
    return Result<std::size_t>::failure(Error::validation(
        datum_plane.id().value(), "datum plane id must be unique within part document"));
  if (has_derived_workplane_id(datum_plane.id()) ||
      has_construction_plane_id(ConstructionPlaneId(datum_plane.id().value())))
    return Result<std::size_t>::failure(Error::validation(
        datum_plane.id().value(), "workplane id must be unique within part document"));
  datum_planes_.push_back(std::move(datum_plane));
  return Result<std::size_t>::success(datum_planes_.size() - 1U);
}

Result<std::size_t> PartDocument::add_datum_axis(DatumAxis datum_axis) {
  if (has_datum_axis_id(datum_axis.id()))
    return Result<std::size_t>::failure(Error::validation(
        datum_axis.id().value(), "datum axis id must be unique within part document"));
  for (const auto& parameter_id : datum_axis.parameter_dependencies())
    if (!has_parameter_id(parameter_id))
      return Result<std::size_t>::failure(Error::validation(
          datum_axis.id().value(), "datum axis parameter dependency must exist in part document"));
  if (datum_axis.kind() == DatumAxisKind::FromConstructionLine &&
      !has_construction_line_id(datum_axis.source_construction_line()))
    return Result<std::size_t>::failure(Error::validation(
        datum_axis.id().value(),
        "construction-line-derived datum axis source line must exist in part document"));
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(datum_axis.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto parameter_dependencies = add_parameter_dependencies(
      graph, datum_axis.parameter_dependencies(), datum_axis.id().value());
  if (parameter_dependencies.has_error())
    return Result<std::size_t>::failure(parameter_dependencies.error());
  if (datum_axis.kind() == DatumAxisKind::FromConstructionLine) {
    auto line_dependency = add_dependency_if_missing(
        graph, datum_axis.source_construction_line().value(), datum_axis.id().value());
    if (line_dependency.has_error())
      return Result<std::size_t>::failure(line_dependency.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  datum_axes_.push_back(std::move(datum_axis));
  return Result<std::size_t>::success(datum_axes_.size() - 1U);
}

Result<std::size_t> PartDocument::add_construction_point(ConstructionPoint point) {
  if (has_construction_point_id(point.id()))
    return Result<std::size_t>::failure(Error::validation(
        point.id().value(), "construction point id must be unique within part document"));
  for (const auto& parameter_id : point.parameter_dependencies())
    if (!has_parameter_id(parameter_id))
      return Result<std::size_t>::failure(
          Error::validation(point.id().value(),
                            "construction point parameter dependency must exist in part document"));
  if (point.relation().has_value()) {
    auto valid_relation =
        validate_relation_references(*this, point.relation().value(), point.id().value());
    if (valid_relation.has_error())
      return Result<std::size_t>::failure(valid_relation.error());
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(point.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto parameter_dependencies =
      add_parameter_dependencies(graph, point.parameter_dependencies(), point.id().value());
  if (parameter_dependencies.has_error())
    return Result<std::size_t>::failure(parameter_dependencies.error());
  if (point.relation().has_value()) {
    auto relation_dependencies =
        add_relation_dependencies(graph, point.relation().value(), point.id().value());
    if (relation_dependencies.has_error())
      return Result<std::size_t>::failure(relation_dependencies.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  construction_points_.push_back(std::move(point));
  return Result<std::size_t>::success(construction_points_.size() - 1U);
}

Result<std::size_t> PartDocument::add_construction_line(ConstructionLine line) {
  if (has_construction_line_id(line.id()))
    return Result<std::size_t>::failure(Error::validation(
        line.id().value(), "construction line id must be unique within part document"));
  for (const auto& parameter_id : line.parameter_dependencies())
    if (!has_parameter_id(parameter_id))
      return Result<std::size_t>::failure(Error::validation(
          line.id().value(), "construction line parameter dependency must exist in part document"));
  if (line.kind() != ConstructionLineKind::Explicit) {
    if (!line.relation().has_value())
      return Result<std::size_t>::failure(Error::validation(
          line.id().value(), "relation-driven construction line must carry a relation"));
    const ConstructionRelation& relation = line.relation().value();
    const bool expected_relation =
        (line.kind() == ConstructionLineKind::ThroughTwoPoints &&
         relation.type() == ConstructionRelationType::LineThroughTwoPoints) ||
        (line.kind() == ConstructionLineKind::ParallelToLineThroughPoint &&
         relation.type() == ConstructionRelationType::LineParallelToLineThroughPoint) ||
        (line.kind() == ConstructionLineKind::ParallelToGeneratedEdgeThroughPoint &&
         relation.type() == ConstructionRelationType::LineParallelToGeneratedEdgeThroughPoint);
    if (!expected_relation)
      return Result<std::size_t>::failure(
          Error::validation(line.id().value(),
                            "relation-driven construction line kind does not match relation type"));
    auto valid_relation = validate_relation_references(*this, relation, line.id().value());
    if (valid_relation.has_error())
      return Result<std::size_t>::failure(valid_relation.error());
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(line.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto parameter_dependencies =
      add_parameter_dependencies(graph, line.parameter_dependencies(), line.id().value());
  if (parameter_dependencies.has_error())
    return Result<std::size_t>::failure(parameter_dependencies.error());
  if (line.relation().has_value()) {
    auto relation_dependencies =
        add_relation_dependencies(graph, line.relation().value(), line.id().value());
    if (relation_dependencies.has_error())
      return Result<std::size_t>::failure(relation_dependencies.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  construction_lines_.push_back(std::move(line));
  return Result<std::size_t>::success(construction_lines_.size() - 1U);
}

Result<std::size_t> PartDocument::add_construction_plane(ConstructionPlane plane) {
  if (has_construction_plane_id(plane.id()))
    return Result<std::size_t>::failure(Error::validation(
        plane.id().value(), "construction plane id must be unique within part document"));
  if (has_workplane_id(plane.workplane_id()))
    return Result<std::size_t>::failure(
        Error::validation(plane.id().value(), "workplane id must be unique within part document"));
  for (const auto& parameter_id : plane.parameter_dependencies())
    if (!has_parameter_id(parameter_id))
      return Result<std::size_t>::failure(
          Error::validation(plane.id().value(),
                            "construction plane parameter dependency must exist in part document"));
  if (plane.kind() != ConstructionPlaneKind::Explicit) {
    if (!plane.relation().has_value())
      return Result<std::size_t>::failure(Error::validation(
          plane.id().value(), "relation-driven construction plane must carry a relation"));
    const ConstructionRelation& relation = plane.relation().value();
    const bool expected_relation =
        (plane.kind() == ConstructionPlaneKind::OffsetFromPlane &&
         relation.type() == ConstructionRelationType::PlaneOffsetFromPlane) ||
        (plane.kind() == ConstructionPlaneKind::ThroughThreePoints &&
         relation.type() == ConstructionRelationType::PlaneThroughThreePoints) ||
        (plane.kind() == ConstructionPlaneKind::ParallelToPlaneThroughPoint &&
         relation.type() == ConstructionRelationType::PlaneParallelToPlaneThroughPoint);
    if (!expected_relation)
      return Result<std::size_t>::failure(Error::validation(
          plane.id().value(),
          "relation-driven construction plane kind does not match relation type"));
    auto valid_relation = validate_relation_references(*this, relation, plane.id().value());
    if (valid_relation.has_error())
      return Result<std::size_t>::failure(valid_relation.error());
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(plane.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto parameter_dependencies =
      add_parameter_dependencies(graph, plane.parameter_dependencies(), plane.id().value());
  if (parameter_dependencies.has_error())
    return Result<std::size_t>::failure(parameter_dependencies.error());
  if (plane.relation().has_value()) {
    auto relation_dependencies =
        add_relation_dependencies(graph, plane.relation().value(), plane.id().value());
    if (relation_dependencies.has_error())
      return Result<std::size_t>::failure(relation_dependencies.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  construction_planes_.push_back(std::move(plane));
  return Result<std::size_t>::success(construction_planes_.size() - 1U);
}

Result<std::size_t> PartDocument::add_derived_workplane(DerivedWorkplane workplane) {
  if (has_workplane_id(workplane.id()))
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "workplane id must be unique within part document"));
  const Feature* source_feature = find_feature(workplane.face_reference().source_feature());
  if (source_feature == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "derived workplane source feature must exist in part document"));
  if (source_feature->type() != FeatureType::AdditiveExtrude)
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "derived workplane source feature must be an additive extrude"));
  if (!is_supported_derived_face(workplane.face_reference().face()))
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(),
        "only top, bottom, right, left, front, and back semantic faces are supported"));
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(workplane.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto source_dependency = add_dependency_if_missing(
      graph, workplane.face_reference().source_feature().value(), workplane.id().value());
  if (source_dependency.has_error())
    return Result<std::size_t>::failure(source_dependency.error());
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  derived_workplanes_.push_back(std::move(workplane));
  return Result<std::size_t>::success(derived_workplanes_.size() - 1U);
}

Result<std::size_t> PartDocument::add_sketch(Sketch sketch) {
  if (has_sketch_id(sketch.id()) || has_sketch_3d_id(Sketch3DId(sketch.id().value())))
    return Result<std::size_t>::failure(
        Error::validation(sketch.id().value(), "sketch id must be unique within part document"));
  if (!has_workplane_id(sketch.workplane()))
    return Result<std::size_t>::failure(
        Error::validation(sketch.id().value(), "sketch workplane must exist in part document"));
  auto valid_projected_references = validate_projected_sketch_references(*this, sketch);
  if (valid_projected_references.has_error())
    return Result<std::size_t>::failure(valid_projected_references.error());
  auto valid_reference_generated_lines = validate_reference_generated_lines(sketch);
  if (valid_reference_generated_lines.has_error())
    return Result<std::size_t>::failure(valid_reference_generated_lines.error());
  auto valid_dimensions = validate_driving_dimensions(*this, sketch);
  if (valid_dimensions.has_error())
    return Result<std::size_t>::failure(valid_dimensions.error());
  for (const auto& profile : sketch.rectangle_profiles()) {
    if (!has_parameter_id(profile.width_parameter()))
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "rectangle width parameter must exist in part document"));
    if (!has_parameter_id(profile.height_parameter()))
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "rectangle height parameter must exist in part document"));
  }
  for (const auto& profile : sketch.circle_profiles())
    if (!has_parameter_id(profile.diameter_parameter()))
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "circle diameter parameter must exist in part document"));
  for (const auto& pattern : sketch.circular_hole_patterns()) {
    const Parameter* radius = find_parameter(pattern.radius_parameter());
    if (radius == nullptr || radius->type() != ParameterType::Length)
      return Result<std::size_t>::failure(
          Error::validation(pattern.id().value(), "circular hole pattern radius parameter must be "
                                                  "a length parameter in part document"));
    const Parameter* count = find_parameter(pattern.count_parameter());
    if (count == nullptr || count->type() != ParameterType::Count)
      return Result<std::size_t>::failure(
          Error::validation(pattern.id().value(), "circular hole pattern count parameter must be "
                                                  "a count parameter in part document"));
    const Parameter* diameter = find_parameter(pattern.hole_diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length)
      return Result<std::size_t>::failure(Error::validation(
          pattern.id().value(), "circular hole pattern hole diameter parameter must be "
                                "a length parameter in part document"));
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(sketch.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  if (has_derived_workplane_id(sketch.workplane()) ||
      has_construction_plane_id(ConstructionPlaneId(sketch.workplane().value()))) {
    auto workplane_dependency =
        add_dependency_if_missing(graph, sketch.workplane().value(), sketch.id().value());
    if (workplane_dependency.has_error())
      return Result<std::size_t>::failure(workplane_dependency.error());
  }
  auto projected_reference_dependencies = add_projected_reference_dependencies(graph, sketch);
  if (projected_reference_dependencies.has_error())
    return Result<std::size_t>::failure(projected_reference_dependencies.error());
  auto dimension_dependencies = add_driving_dimension_dependencies(graph, sketch);
  if (dimension_dependencies.has_error())
    return Result<std::size_t>::failure(dimension_dependencies.error());
  for (const auto& profile : sketch.rectangle_profiles()) {
    auto width_dependency =
        add_dependency_if_missing(graph, profile.width_parameter().value(), sketch.id().value());
    if (width_dependency.has_error())
      return Result<std::size_t>::failure(width_dependency.error());
    auto height_dependency =
        add_dependency_if_missing(graph, profile.height_parameter().value(), sketch.id().value());
    if (height_dependency.has_error())
      return Result<std::size_t>::failure(height_dependency.error());
  }
  for (const auto& profile : sketch.circle_profiles()) {
    auto diameter_dependency =
        add_dependency_if_missing(graph, profile.diameter_parameter().value(), sketch.id().value());
    if (diameter_dependency.has_error())
      return Result<std::size_t>::failure(diameter_dependency.error());
  }
  for (const auto& pattern : sketch.circular_hole_patterns()) {
    for (const ParameterId* parameter_id : {&pattern.radius_parameter(), &pattern.count_parameter(),
                                            &pattern.hole_diameter_parameter()}) {
      auto pattern_dependency =
          add_dependency_if_missing(graph, parameter_id->value(), sketch.id().value());
      if (pattern_dependency.has_error())
        return Result<std::size_t>::failure(pattern_dependency.error());
    }
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  sketches_.push_back(std::move(sketch));
  return Result<std::size_t>::success(sketches_.size() - 1U);
}

Result<std::size_t> PartDocument::add_sketch_3d(Sketch3D sketch) {
  if (has_sketch_3d_id(sketch.id()) || has_sketch_id(SketchId(sketch.id().value())))
    return Result<std::size_t>::failure(Error::validation(
        sketch.id().value(), "sketch id must be unique across planar and 3D sketches"));

  for (const ParameterId& parameter_id : sketch.parameter_dependencies()) {
    const Parameter* parameter = find_parameter(parameter_id);
    if (parameter == nullptr || parameter->type() != ParameterType::Length)
      return Result<std::size_t>::failure(
          Error::validation(sketch.id().value(),
                            "3D sketch coordinate parameters must be existing Length parameters"));
  }

  auto graph = dependency_graph_;
  auto added_node = graph.add_node(sketch.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const ParameterId& parameter_id : sketch.parameter_dependencies()) {
    auto dependency = add_dependency_if_missing(graph, parameter_id.value(), sketch.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  sketches_3d_.push_back(std::move(sketch));
  return Result<std::size_t>::success(sketches_3d_.size() - 1U);
}

Result<std::size_t> PartDocument::remove_sketch_3d(Sketch3DId id) {
  if (id.empty())
    return Result<std::size_t>::failure(
        Error::validation("sketch_3d", "3D sketch id must not be empty"));
  const auto found = std::find_if(sketches_3d_.begin(), sketches_3d_.end(),
                                  [&id](const Sketch3D& sketch) { return sketch.id() == id; });
  if (found == sketches_3d_.end())
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "3D sketch must exist in part document"));
  if (!dependency_graph_.direct_dependents(id.value()).empty())
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch with dependent model intent cannot be removed"));
  auto graph = dependency_graph_;
  auto removed = graph.remove_node(id.value());
  if (removed.has_error())
    return Result<std::size_t>::failure(removed.error());
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  sketches_3d_.erase(found);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> PartDocument::add_feature(Feature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_sketch_id(feature.input_sketch()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "feature input sketch must exist in part document"));
  if (feature.type() == FeatureType::AdditiveExtrude &&
      feature.extrude_intent().is_historical_additive_default() &&
      !has_parameter_id(feature.length_parameter()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "additive extrude length parameter must exist in part document"));
  auto valid_extrude_intent = validate_extrude_feature_intent(*this, feature);
  if (valid_extrude_intent.has_error())
    return Result<std::size_t>::failure(valid_extrude_intent.error());
  if (feature.type() == FeatureType::SubtractiveExtrude &&
      !has_feature_id(feature.target_feature()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "subtractive extrude target feature must exist in part document"));
  if (feature.body_result_context().has_value()) {
    const auto& context = feature.body_result_context().value();
    if (context.target_body().has_value() && !has_body_id(context.target_body().value()))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "feature target body must exist in part document"));
    if (!has_body_id(context.effective_produced_body()))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "feature produced body must exist in part document"));
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto sketch_dependency =
      add_dependency_if_missing(graph, feature.input_sketch().value(), feature.id().value());
  if (sketch_dependency.has_error())
    return Result<std::size_t>::failure(sketch_dependency.error());
  auto extrude_dependencies = add_extrude_feature_dependencies(graph, feature);
  if (extrude_dependencies.has_error())
    return Result<std::size_t>::failure(extrude_dependencies.error());
  if (feature.type() == FeatureType::SubtractiveExtrude) {
    auto target_dependency =
        add_dependency_if_missing(graph, feature.target_feature().value(), feature.id().value());
    if (target_dependency.has_error())
      return Result<std::size_t>::failure(target_dependency.error());
  }
  if (feature.body_result_context().has_value()) {
    const auto& context = feature.body_result_context().value();
    const std::string result_node = body_dependency_node_id(context.effective_produced_body());
    const bool modifies_in_place =
        context.target_body().has_value() &&
        context.target_body().value() == context.effective_produced_body();
    if (modifies_in_place) {
      const auto previous_producers = dependency_sources_of(graph, result_node);
      graph.remove_dependencies_of_dependent(result_node);
      for (const auto& producer : previous_producers) {
        auto producer_dependency = add_dependency_if_missing(graph, producer, feature.id().value());
        if (producer_dependency.has_error())
          return Result<std::size_t>::failure(producer_dependency.error());
      }
    } else {
      if (!dependency_sources_of(graph, result_node).empty())
        return Result<std::size_t>::failure(Error::dependency(
            feature.id().value(), "feature produced body already has a producing feature"));
      if (context.target_body().has_value()) {
        auto body_dependency = add_dependency_if_missing(
            graph, body_dependency_node_id(context.target_body().value()), feature.id().value());
        if (body_dependency.has_error())
          return Result<std::size_t>::failure(body_dependency.error());
      }
    }
    auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
    if (result_dependency.has_error())
      return Result<std::size_t>::failure(result_dependency.error());
    if (graph.has_cycle())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "feature body operation must not create a dependency cycle"));
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  features_.push_back(std::move(feature));
  return Result<std::size_t>::success(features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_revolve_feature(RevolveFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  auto profile_valid = validate_part_feature_input_reference(*this, feature.profile());
  if (profile_valid.has_error())
    return Result<std::size_t>::failure(profile_valid.error());
  auto axis_valid = validate_part_feature_input_reference(*this, feature.axis());
  if (axis_valid.has_error())
    return Result<std::size_t>::failure(axis_valid.error());

  const auto& context = feature.body_result_context();
  if (context.target_body().has_value() && !has_body_id(context.target_body().value()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "revolve target body must exist in part document"));
  if (!has_body_id(context.effective_produced_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "revolve produced body must exist in part document"));

  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const std::string& source :
       {feature.profile().source_node_id(), feature.axis().source_node_id()}) {
    auto dependency = add_dependency_if_missing(graph, source, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }

  const std::string result_node = body_dependency_node_id(context.effective_produced_body());
  const bool modifies_in_place = context.target_body().has_value() &&
                                 context.target_body().value() == context.effective_produced_body();
  if (modifies_in_place) {
    const auto previous_producers = dependency_sources_of(graph, result_node);
    graph.remove_dependencies_of_dependent(result_node);
    for (const auto& producer : previous_producers) {
      auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  } else {
    if (!dependency_sources_of(graph, result_node).empty())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "revolve produced body already has a producing feature"));
    if (context.target_body().has_value()) {
      auto dependency = add_dependency_if_missing(
          graph, body_dependency_node_id(context.target_body().value()), feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        feature.id().value(), "revolve body operation must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  revolve_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(revolve_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_linear_pattern_feature(LinearPatternFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  const Parameter* count = find_parameter(feature.count_parameter());
  if (count == nullptr || count->type() != ParameterType::Count ||
      count->value().count_value() < 2U)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(),
        "linear pattern count must reference an existing count parameter of at least 2"));
  const Parameter* extent = find_parameter(feature.extent_parameter());
  if (extent == nullptr || extent->type() != ParameterType::Length)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "linear pattern extent must reference an existing length parameter"));
  auto direction_valid = validate_part_feature_input_reference(*this, feature.direction());
  if (direction_valid.has_error())
    return Result<std::size_t>::failure(direction_valid.error());
  for (const auto& source : feature.sources()) {
    if (source.kind() == PatternSourceKind::Feature &&
        !has_any_feature_id(std::get<FeatureId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "linear pattern source feature must exist in part document"));
    if (source.kind() == PatternSourceKind::Body && !has_body_id(std::get<BodyId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "linear pattern source body must exist in part document"));
  }
  const auto& context = feature.body_result_context();
  if (context.target_body().has_value() && !has_body_id(*context.target_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "linear pattern target body must exist in part document"));
  if (!has_body_id(context.effective_produced_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "linear pattern produced body must exist in part document"));

  const bool modifies_in_place = context.target_body().has_value() &&
                                 *context.target_body() == context.effective_produced_body();
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const auto& source : feature.sources()) {
    if (modifies_in_place && source.kind() == PatternSourceKind::Body &&
        std::get<BodyId>(source.source()) == context.effective_produced_body())
      continue;
    auto dependency =
        add_dependency_if_missing(graph, source.source_node_id(), feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  for (const std::string& source :
       {feature.direction().source_node_id(), feature.count_parameter().value(),
        feature.extent_parameter().value()}) {
    auto dependency = add_dependency_if_missing(graph, source, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  const std::string result_node = body_dependency_node_id(context.effective_produced_body());
  if (modifies_in_place) {
    const auto previous_producers = dependency_sources_of(graph, result_node);
    graph.remove_dependencies_of_dependent(result_node);
    for (const auto& producer : previous_producers) {
      auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  } else {
    if (!dependency_sources_of(graph, result_node).empty())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "linear pattern produced body already has a producing feature"));
    if (context.target_body().has_value()) {
      auto dependency = add_dependency_if_missing(
          graph, body_dependency_node_id(*context.target_body()), feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        feature.id().value(), "linear pattern must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  linear_pattern_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(linear_pattern_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_circular_pattern_feature(CircularPatternFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  const Parameter* count = find_parameter(feature.count_parameter());
  if (count == nullptr || count->type() != ParameterType::Count ||
      count->value().count_value() < 2U)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(),
        "circular pattern count must reference an existing count parameter of at least 2"));
  auto axis_valid = validate_part_feature_input_reference(*this, feature.axis());
  if (axis_valid.has_error())
    return Result<std::size_t>::failure(axis_valid.error());
  for (const auto& source : feature.sources()) {
    if (source.kind() == PatternSourceKind::Feature &&
        !has_any_feature_id(std::get<FeatureId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "circular pattern source feature must exist in part document"));
    if (source.kind() == PatternSourceKind::Body && !has_body_id(std::get<BodyId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "circular pattern source body must exist in part document"));
  }
  const auto& context = feature.body_result_context();
  if (context.target_body().has_value() && !has_body_id(*context.target_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "circular pattern target body must exist in part document"));
  if (!has_body_id(context.effective_produced_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "circular pattern produced body must exist in part document"));

  const bool modifies_in_place = context.target_body().has_value() &&
                                 *context.target_body() == context.effective_produced_body();
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const auto& source : feature.sources()) {
    if (modifies_in_place && source.kind() == PatternSourceKind::Body &&
        std::get<BodyId>(source.source()) == context.effective_produced_body())
      continue;
    auto dependency =
        add_dependency_if_missing(graph, source.source_node_id(), feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  for (const std::string& source :
       {feature.axis().source_node_id(), feature.count_parameter().value()}) {
    auto dependency = add_dependency_if_missing(graph, source, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  const std::string result_node = body_dependency_node_id(context.effective_produced_body());
  if (modifies_in_place) {
    const auto previous_producers = dependency_sources_of(graph, result_node);
    graph.remove_dependencies_of_dependent(result_node);
    for (const auto& producer : previous_producers) {
      auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  } else {
    if (!dependency_sources_of(graph, result_node).empty())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "circular pattern produced body already has a producing feature"));
    if (context.target_body().has_value()) {
      auto dependency = add_dependency_if_missing(
          graph, body_dependency_node_id(*context.target_body()), feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        feature.id().value(), "circular pattern must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  circular_pattern_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(circular_pattern_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_mirror_feature(MirrorFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  auto plane_valid = validate_part_feature_input_reference(*this, feature.mirror_plane());
  if (plane_valid.has_error())
    return Result<std::size_t>::failure(plane_valid.error());
  for (const auto& source : feature.sources()) {
    if (source.kind() == MirrorSourceKind::Feature &&
        !has_any_feature_id(std::get<FeatureId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "mirror source feature must exist in part document"));
    if (source.kind() == MirrorSourceKind::Body && !has_body_id(std::get<BodyId>(source.source())))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "mirror source body must exist in part document"));
  }
  const auto& context = feature.body_result_context();
  if (context.target_body().has_value() && !has_body_id(*context.target_body()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "mirror target body must exist in part document"));
  if (!has_body_id(context.effective_produced_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "mirror produced body must exist in part document"));

  const bool modifies_in_place = context.target_body().has_value() &&
                                 *context.target_body() == context.effective_produced_body();
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const auto& source : feature.sources()) {
    if (modifies_in_place && source.kind() == MirrorSourceKind::Body &&
        std::get<BodyId>(source.source()) == context.effective_produced_body())
      continue;
    auto dependency =
        add_dependency_if_missing(graph, source.source_node_id(), feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto plane_dependency = add_dependency_if_missing(graph, feature.mirror_plane().source_node_id(),
                                                    feature.id().value());
  if (plane_dependency.has_error())
    return Result<std::size_t>::failure(plane_dependency.error());

  const std::string result_node = body_dependency_node_id(context.effective_produced_body());
  if (modifies_in_place) {
    const auto previous_producers = dependency_sources_of(graph, result_node);
    graph.remove_dependencies_of_dependent(result_node);
    for (const auto& producer : previous_producers) {
      auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  } else {
    if (!dependency_sources_of(graph, result_node).empty())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "mirror produced body already has a producing feature"));
    if (context.target_body().has_value()) {
      auto dependency = add_dependency_if_missing(
          graph, body_dependency_node_id(*context.target_body()), feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(
        Error::dependency(feature.id().value(), "mirror must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  mirror_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(mirror_features_.size() - 1U);
}

Result<std::size_t>
PartDocument::add_edge_treatment_dependencies(const FeatureId& id, const BodyId& target_body,
                                              const std::vector<EdgeReference>& edges,
                                              const std::vector<ParameterId>& parameters) {
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(id.value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const auto& edge : edges) {
    auto dependency = add_dependency_if_missing(graph, edge.source_node_id(), id.value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  for (const auto& parameter : parameters) {
    auto dependency = add_dependency_if_missing(graph, parameter.value(), id.value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  const std::string body_node = body_dependency_node_id(target_body);
  const auto previous_producers = dependency_sources_of(graph, body_node);
  graph.remove_dependencies_of_dependent(body_node);
  for (const auto& producer : previous_producers) {
    auto dependency = add_dependency_if_missing(graph, producer, id.value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto result_dependency = add_dependency_if_missing(graph, id.value(), body_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "edge treatment must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> PartDocument::add_fillet_feature(FilletFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_body_id(feature.target_body()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "fillet target body must exist in part document"));
  for (const auto& edge : feature.edges()) {
    auto valid = validate_part_feature_input_reference(*this, edge);
    if (valid.has_error())
      return Result<std::size_t>::failure(valid.error());
  }
  const Parameter* radius = find_parameter(feature.radius_parameter());
  if (radius == nullptr || radius->type() != ParameterType::Length)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "fillet radius must reference an existing length parameter"));
  auto dependencies = add_edge_treatment_dependencies(
      feature.id(), feature.target_body(), feature.edges(), {feature.radius_parameter()});
  if (dependencies.has_error())
    return dependencies;
  fillet_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(fillet_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_chamfer_feature(ChamferFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_body_id(feature.target_body()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "chamfer target body must exist in part document"));
  for (const auto& edge : feature.edges()) {
    auto valid = validate_part_feature_input_reference(*this, edge);
    if (valid.has_error())
      return Result<std::size_t>::failure(valid.error());
  }
  const Parameter* first = find_parameter(feature.first_parameter());
  if (first == nullptr || first->type() != ParameterType::Length)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "chamfer distance must reference an existing length parameter"));
  std::vector<ParameterId> parameters{feature.first_parameter()};
  if (feature.second_parameter().has_value()) {
    const Parameter* second = find_parameter(*feature.second_parameter());
    const ParameterType required =
        feature.mode() == ChamferMode::DistanceAngle ? ParameterType::Angle : ParameterType::Length;
    if (second == nullptr || second->type() != required)
      return Result<std::size_t>::failure(
          Error::validation(feature.id().value(),
                            feature.mode() == ChamferMode::DistanceAngle
                                ? "chamfer angle must reference an existing angle parameter"
                                : "chamfer distance must reference an existing length parameter"));
    if (required == ParameterType::Angle &&
        (second->value().degrees() <= 0.0 || second->value().degrees() >= 90.0))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "chamfer angle must be greater than 0 and less than 90 degrees"));
    parameters.push_back(*feature.second_parameter());
  }
  auto dependencies = add_edge_treatment_dependencies(feature.id(), feature.target_body(),
                                                      feature.edges(), parameters);
  if (dependencies.has_error())
    return dependencies;
  chamfer_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(chamfer_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_shell_dependencies(const ShellFeature& feature) {
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const FaceReference& face : feature.removed_faces()) {
    auto dependency = add_dependency_if_missing(graph, face.source_node_id(), feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto thickness_dependency =
      add_dependency_if_missing(graph, feature.thickness_parameter().value(), feature.id().value());
  if (thickness_dependency.has_error())
    return Result<std::size_t>::failure(thickness_dependency.error());

  const std::string body_node = body_dependency_node_id(feature.target_body());
  const auto previous_producers = dependency_sources_of(graph, body_node);
  graph.remove_dependencies_of_dependent(body_node);
  for (const std::string& producer : previous_producers) {
    auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), body_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(
        Error::dependency(feature.id().value(), "shell must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> PartDocument::add_shell_feature(ShellFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_body_id(feature.target_body()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "shell target body must exist in part document"));
  for (const FaceReference& face : feature.removed_faces()) {
    auto valid = validate_part_feature_input_reference(*this, face);
    if (valid.has_error())
      return Result<std::size_t>::failure(valid.error());
  }
  const Parameter* thickness = find_parameter(feature.thickness_parameter());
  if (thickness == nullptr || thickness->type() != ParameterType::Length ||
      thickness->value().millimeters() <= 0.0)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "shell thickness must reference a positive length parameter"));
  auto dependencies = add_shell_dependencies(feature);
  if (dependencies.has_error())
    return dependencies;
  shell_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(shell_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_draft_dependencies(const DraftFeature& feature) {
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  for (const FaceReference& face : feature.faces()) {
    auto dependency = add_dependency_if_missing(graph, face.source_node_id(), feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  for (const std::string& source :
       {feature.pull_direction().source_node_id(), feature.neutral_plane().source_node_id(),
        feature.angle_parameter().value()}) {
    auto dependency = add_dependency_if_missing(graph, source, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }

  const std::string body_node = body_dependency_node_id(feature.target_body());
  const auto previous_producers = dependency_sources_of(graph, body_node);
  graph.remove_dependencies_of_dependent(body_node);
  for (const std::string& producer : previous_producers) {
    auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), body_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(
        Error::dependency(feature.id().value(), "draft must not create a dependency cycle"));
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> PartDocument::add_draft_feature(DraftFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_body_id(feature.target_body()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "draft target body must exist in part document"));
  for (const FaceReference& face : feature.faces()) {
    auto valid = validate_part_feature_input_reference(*this, face);
    if (valid.has_error())
      return Result<std::size_t>::failure(valid.error());
  }
  auto pull_valid = validate_part_feature_input_reference(*this, feature.pull_direction());
  if (pull_valid.has_error())
    return Result<std::size_t>::failure(pull_valid.error());
  auto plane_valid = validate_part_feature_input_reference(*this, feature.neutral_plane());
  if (plane_valid.has_error())
    return Result<std::size_t>::failure(plane_valid.error());
  const Parameter* angle = find_parameter(feature.angle_parameter());
  if (angle == nullptr || angle->type() != ParameterType::Angle ||
      std::abs(angle->value().degrees()) <= k_tolerance ||
      std::abs(angle->value().degrees()) >= 90.0)
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(),
        "draft angle must reference a non-zero Angle between -90 and 90 degrees"));
  auto dependencies = add_draft_dependencies(feature);
  if (dependencies.has_error())
    return dependencies;
  draft_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(draft_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_body_boolean_feature(BodyBooleanFeature feature) {
  if (has_any_feature_id(feature.id()))
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  if (!has_body_id(feature.target_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "body boolean target body must exist in part document"));
  for (const BodyId& tool : feature.tool_bodies())
    if (!has_body_id(tool))
      return Result<std::size_t>::failure(Error::validation(
          feature.id().value(), "body boolean tool bodies must exist in part document"));
  if (!has_body_id(feature.effective_result_body()))
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "body boolean result body must exist in part document"));

  auto graph = dependency_graph_;
  auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());

  const std::string result_node = body_dependency_node_id(feature.effective_result_body());
  if (feature.result_mode() == BodyBooleanResultMode::ModifyTarget) {
    const auto previous_producers = dependency_sources_of(graph, result_node);
    graph.remove_dependencies_of_dependent(result_node);
    for (const auto& producer : previous_producers) {
      auto dependency = add_dependency_if_missing(graph, producer, feature.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  } else {
    if (!dependency_sources_of(graph, result_node).empty())
      return Result<std::size_t>::failure(Error::dependency(
          feature.id().value(), "body boolean result body already has a producing feature"));
    auto target_dependency = add_dependency_if_missing(
        graph, body_dependency_node_id(feature.target_body()), feature.id().value());
    if (target_dependency.has_error())
      return Result<std::size_t>::failure(target_dependency.error());
  }

  for (const BodyId& tool : feature.tool_bodies()) {
    auto tool_dependency =
        add_dependency_if_missing(graph, body_dependency_node_id(tool), feature.id().value());
    if (tool_dependency.has_error())
      return Result<std::size_t>::failure(tool_dependency.error());
  }
  auto result_dependency = add_dependency_if_missing(graph, feature.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        feature.id().value(), "body boolean feature must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  body_boolean_features_.push_back(std::move(feature));
  return Result<std::size_t>::success(body_boolean_features_.size() - 1U);
}

Result<std::size_t> PartDocument::add_sketch_ownership(SketchOwnership ownership) {
  if (has_sketch_ownership_id(ownership.sketch()))
    return Result<std::size_t>::failure(Error::validation(
        ownership.sketch().value(), "sketch ownership must be unique per sketch"));
  if (!has_sketch_id(ownership.sketch()))
    return Result<std::size_t>::failure(
        Error::validation(ownership.sketch().value(), "owned sketch must exist in part document"));
  if (!has_body_id(ownership.owning_body()))
    return Result<std::size_t>::failure(Error::validation(
        ownership.sketch().value(), "sketch owning body must exist in part document"));

  auto graph = dependency_graph_;
  const std::string node_id = sketch_ownership_node_id(ownership.sketch());
  auto added_node = graph.add_node(node_id);
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto sketch_dependency = add_dependency_if_missing(graph, ownership.sketch().value(), node_id);
  if (sketch_dependency.has_error())
    return Result<std::size_t>::failure(sketch_dependency.error());
  for (const BodyTransform& transform : body_transforms_) {
    if (transform.body() != ownership.owning_body() || !transform.apply_to_owned_sketches())
      continue;
    auto dependency = add_dependency_if_missing(graph, node_id, transform.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        ownership.sketch().value(), "sketch ownership must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  const auto position =
      std::lower_bound(sketch_ownerships_.begin(), sketch_ownerships_.end(), ownership.sketch(),
                       [](const SketchOwnership& existing, const SketchId& id) {
                         return existing.sketch().value() < id.value();
                       });
  const auto index = static_cast<std::size_t>(std::distance(sketch_ownerships_.begin(), position));
  sketch_ownerships_.insert(position, std::move(ownership));
  return Result<std::size_t>::success(index);
}

Result<std::size_t> PartDocument::add_body_transform(BodyTransform transform) {
  if (has_body_transform_id(transform.id()) || dependency_graph_.has_node(transform.id().value()))
    return Result<std::size_t>::failure(Error::validation(
        transform.id().value(), "body transform id must be unique within part document"));
  if (!has_body_id(transform.body()))
    return Result<std::size_t>::failure(Error::validation(
        transform.id().value(), "body transform body must exist in part document"));

  std::optional<std::string> coordinate_dependency;
  if (transform.coordinate_space() == BodyTransformCoordinateSpace::SketchLocal) {
    const SketchId sketch(*transform.coordinate_reference());
    if (!has_sketch_id(sketch))
      return Result<std::size_t>::failure(Error::validation(
          transform.id().value(), "sketch-local coordinate reference must exist"));
    coordinate_dependency = sketch.value();
  } else if (transform.coordinate_space() == BodyTransformCoordinateSpace::ConstructionReference) {
    const std::string& reference = *transform.coordinate_reference();
    const bool exists = has_datum_plane_id(DatumPlaneId(reference)) ||
                        has_datum_axis_id(DatumAxisId(reference)) ||
                        has_construction_line_id(ConstructionLineId(reference)) ||
                        has_construction_plane_id(ConstructionPlaneId(reference));
    if (!exists)
      return Result<std::size_t>::failure(Error::validation(
          transform.id().value(), "construction coordinate reference must exist"));
    coordinate_dependency = reference;
  }

  std::optional<std::string> axis_dependency;
  if (transform.kind() == BodyTransformKind::Rotate) {
    const BodyTransformRotationAxis& axis = *transform.rotation_axis();
    if (axis.kind() == BodyTransformRotationAxisKind::DatumAxis) {
      if (!has_datum_axis_id(axis.datum_axis()))
        return Result<std::size_t>::failure(Error::validation(
            transform.id().value(), "rotation datum axis must exist in part document"));
      axis_dependency = axis.datum_axis().value();
    } else if (axis.kind() == BodyTransformRotationAxisKind::ConstructionLine) {
      if (!has_construction_line_id(axis.construction_line()))
        return Result<std::size_t>::failure(Error::validation(
            transform.id().value(), "rotation construction line must exist in part document"));
      axis_dependency = axis.construction_line().value();
    } else if (axis.kind() == BodyTransformRotationAxisKind::SemanticEdge) {
      auto valid =
          validate_generated_edge_reference(*this, *axis.semantic_edge(), transform.id().value());
      if (valid.has_error())
        return Result<std::size_t>::failure(valid.error());
      axis_dependency = axis.semantic_edge()->source_feature().value();
    }
  }

  auto graph = dependency_graph_;
  auto added_node = graph.add_node(transform.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  const std::string result_node = body_dependency_node_id(transform.body());
  const auto previous_producers = dependency_sources_of(graph, result_node);
  graph.remove_dependencies_of_dependent(result_node);
  for (const std::string& producer : previous_producers) {
    auto dependency = add_dependency_if_missing(graph, producer, transform.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  for (const auto& dependency_id : {coordinate_dependency, axis_dependency}) {
    if (!dependency_id.has_value())
      continue;
    auto dependency = add_dependency_if_missing(graph, *dependency_id, transform.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  if (transform.apply_to_owned_sketches()) {
    for (const SketchOwnership& ownership : sketch_ownerships_) {
      if (ownership.owning_body() != transform.body())
        continue;
      auto dependency = add_dependency_if_missing(
          graph, sketch_ownership_node_id(ownership.sketch()), transform.id().value());
      if (dependency.has_error())
        return Result<std::size_t>::failure(dependency.error());
    }
  }
  auto result_dependency = add_dependency_if_missing(graph, transform.id().value(), result_node);
  if (result_dependency.has_error())
    return Result<std::size_t>::failure(result_dependency.error());
  if (graph.has_cycle())
    return Result<std::size_t>::failure(Error::dependency(
        transform.id().value(), "body transform must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  body_transforms_.push_back(std::move(transform));
  return Result<std::size_t>::success(body_transforms_.size() - 1U);
}

Result<std::size_t> PartDocument::add_body(Body body) {
  if (has_body_id(body.id())) {
    return Result<std::size_t>::failure(
        Error::validation(body.id().value(), "body id must be unique within part document"));
  }
  auto graph = dependency_graph_;
  const std::string node_id = body_dependency_node_id(body.id());
  auto added_node = graph.add_node(node_id);
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  const auto position = std::lower_bound(
      bodies_.begin(), bodies_.end(), body.id().value(),
      [](const Body& existing, const std::string& id) { return existing.id().value() < id; });
  const auto index = static_cast<std::size_t>(std::distance(bodies_.begin(), position));
  bodies_.insert(position, std::move(body));
  return Result<std::size_t>::success(index);
}

Result<std::size_t> PartDocument::remove_body(BodyId id) {
  if (id.empty()) {
    return Result<std::size_t>::failure(Error::validation("body", "body id must not be empty"));
  }
  const auto found = std::find_if(bodies_.begin(), bodies_.end(),
                                  [&id](const Body& body) { return body.id() == id; });
  if (found == bodies_.end()) {
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "body must exist in part document"));
  }
  for (const auto& feature : features_) {
    if (!feature.body_result_context().has_value())
      continue;
    const auto& context = feature.body_result_context().value();
    if ((context.target_body().has_value() && context.target_body().value() == id) ||
        context.effective_produced_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& feature : revolve_features_) {
    const auto& context = feature.body_result_context();
    if ((context.target_body().has_value() && context.target_body().value() == id) ||
        context.effective_produced_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& feature : linear_pattern_features_) {
    const auto& context = feature.body_result_context();
    const bool references_body =
        std::any_of(feature.sources().begin(), feature.sources().end(), [&id](const auto& source) {
          return source.kind() == PatternSourceKind::Body &&
                 std::get<BodyId>(source.source()) == id;
        });
    if (references_body || (context.target_body().has_value() && *context.target_body() == id) ||
        context.effective_produced_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& feature : circular_pattern_features_) {
    const auto& context = feature.body_result_context();
    const bool references_body =
        std::any_of(feature.sources().begin(), feature.sources().end(), [&id](const auto& source) {
          return source.kind() == PatternSourceKind::Body &&
                 std::get<BodyId>(source.source()) == id;
        });
    if (references_body || (context.target_body().has_value() && *context.target_body() == id) ||
        context.effective_produced_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& feature : mirror_features_) {
    const auto& context = feature.body_result_context();
    const bool references_body =
        std::any_of(feature.sources().begin(), feature.sources().end(), [&id](const auto& source) {
          return source.kind() == MirrorSourceKind::Body && std::get<BodyId>(source.source()) == id;
        });
    if (references_body || (context.target_body().has_value() && *context.target_body() == id) ||
        context.effective_produced_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& feature : fillet_features_)
    if (feature.target_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  for (const auto& feature : chamfer_features_)
    if (feature.target_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  for (const auto& feature : shell_features_)
    if (feature.target_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  for (const auto& feature : draft_features_)
    if (feature.target_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  for (const auto& feature : body_boolean_features_) {
    if (feature.target_body() == id || feature.effective_result_body() == id ||
        std::find(feature.tool_bodies().begin(), feature.tool_bodies().end(), id) !=
            feature.tool_bodies().end())
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  for (const auto& ownership : sketch_ownerships_)
    if (ownership.owning_body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body is referenced by sketch ownership"));
  for (const auto& transform : body_transforms_)
    if (transform.body() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "body is referenced by body transform"));
  const auto dependents = dependency_graph_.direct_dependents(body_dependency_node_id(id));
  if (!dependents.empty()) {
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "body with dependent model intent cannot be removed"));
  }
  auto graph = dependency_graph_;
  auto removed_node = graph.remove_node(body_dependency_node_id(id));
  if (removed_node.has_error())
    return Result<std::size_t>::failure(removed_node.error());
  auto invalidation_state = invalidation_state_;
  invalidation_state.untrack_node(body_dependency_node_id(id));
  dependency_graph_ = std::move(graph);
  invalidation_state_ = std::move(invalidation_state);
  bodies_.erase(found);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> PartDocument::set_body_visibility(BodyId id, BodyVisibility visibility) {
  if (id.empty()) {
    return Result<std::size_t>::failure(Error::validation("body", "body id must not be empty"));
  }
  const auto found = std::find_if(bodies_.begin(), bodies_.end(),
                                  [&id](const Body& body) { return body.id() == id; });
  if (found == bodies_.end()) {
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "body must exist in part document"));
  }
  auto updated = found->with_visibility(visibility);
  if (updated.has_error()) {
    return Result<std::size_t>::failure(updated.error());
  }
  *found = std::move(updated.value());
  return Result<std::size_t>::success(
      static_cast<std::size_t>(std::distance(bodies_.begin(), found)));
}

Result<std::size_t> PartDocument::add_reference_status(ReferenceStatusRecord status) {
  if (has_reference_status_id(status.id()))
    return Result<std::size_t>::failure(Error::validation(
        status.id().value(), "reference status id must be unique within part document"));
  if (status.status() == ReferenceStatusKind::Resolved) {
    auto valid_target =
        validate_semantic_reference_target(*this, status.target(), status.id().value());
    if (valid_target.has_error())
      return Result<std::size_t>::failure(valid_target.error());
  }
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(status.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  if (find_feature(status.target().source_feature()) != nullptr) {
    auto dependency = add_dependency_if_missing(graph, status.target().source_feature().value(),
                                                status.id().value());
    if (dependency.has_error())
      return Result<std::size_t>::failure(dependency.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  reference_statuses_.push_back(std::move(status));
  return Result<std::size_t>::success(reference_statuses_.size() - 1U);
}

Result<std::size_t> PartDocument::add_reference_remap(ReferenceRemapRecord remap) {
  if (has_reference_remap_id(remap.id()))
    return Result<std::size_t>::failure(Error::validation(
        remap.id().value(), "reference remap id must be unique within part document"));
  auto valid_replacement =
      validate_semantic_reference_target(*this, remap.replacement(), remap.id().value());
  if (valid_replacement.has_error())
    return Result<std::size_t>::failure(valid_replacement.error());
  auto graph = dependency_graph_;
  auto added_node = graph.add_node(remap.id().value());
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto replacement_dependency = add_dependency_if_missing(
      graph, remap.replacement().source_feature().value(), remap.id().value());
  if (replacement_dependency.has_error())
    return Result<std::size_t>::failure(replacement_dependency.error());
  if (find_feature(remap.original().source_feature()) != nullptr) {
    auto original_dependency = add_dependency_if_missing(
        graph, remap.original().source_feature().value(), remap.id().value());
    if (original_dependency.has_error())
      return Result<std::size_t>::failure(original_dependency.error());
  }
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  reference_remaps_.push_back(std::move(remap));
  return Result<std::size_t>::success(reference_remaps_.size() - 1U);
}

Result<std::size_t>
PartDocument::add_sketch_origin_override(SketchOriginOverrideRecord origin_override) {
  if (has_sketch_origin_override_id(origin_override.sketch()))
    return Result<std::size_t>::failure(Error::validation(
        origin_override.sketch().value(), "sketch origin override must be unique per sketch"));
  if (find_sketch(origin_override.sketch()) == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(origin_override.sketch().value(),
                          "sketch origin override sketch must exist in part document"));
  auto graph = dependency_graph_;
  const std::string node_id = origin_override.sketch().value() + ".origin_override";
  auto added_node = graph.add_node(node_id);
  if (added_node.has_error())
    return Result<std::size_t>::failure(added_node.error());
  auto dependency = add_dependency_if_missing(graph, origin_override.sketch().value(), node_id);
  if (dependency.has_error())
    return Result<std::size_t>::failure(dependency.error());
  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::size_t>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);
  sketch_origin_overrides_.push_back(std::move(origin_override));
  return Result<std::size_t>::success(sketch_origin_overrides_.size() - 1U);
}

Result<std::vector<std::string>> PartDocument::mark_parameter_changed(ParameterId id) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  if (!has_parameter_id(id))
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "parameter must exist in part document"));
  return invalidation_state_.mark_changed(dependency_graph_, id.value());
}
Result<std::vector<std::string>> PartDocument::mark_feature_changed(FeatureId id) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("feature", "feature id must not be empty"));
  if (!has_any_feature_id(id))
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "feature must exist in part document"));
  return invalidation_state_.mark_changed(dependency_graph_, id.value());
}
Result<std::vector<std::string>> PartDocument::mark_body_changed(BodyId id) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("body", "body id must not be empty"));
  if (!has_body_id(id))
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "body must exist in part document"));
  return invalidation_state_.mark_changed(dependency_graph_, body_dependency_node_id(id));
}

Result<std::vector<std::string>> PartDocument::mark_body_transform_changed(BodyTransformId id) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("body_transform", "body transform id must not be empty"));
  if (!has_body_transform_id(id))
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "body transform must exist in part document"));
  return invalidation_state_.mark_changed(dependency_graph_, id.value());
}
Result<std::vector<std::string>> PartDocument::set_parameter_value(ParameterId id, Quantity value) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  const Parameter* existing = find_parameter(id);
  if (existing == nullptr)
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "parameter must exist in part document"));
  if (existing->is_expression())
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "expression-driven parameter value cannot be set directly"));
  auto updated = existing->with_value(value);
  if (updated.has_error())
    return Result<std::vector<std::string>>::failure(updated.error());
  for (auto& parameter : parameters_)
    if (parameter.id() == id) {
      parameter = std::move(updated.value());
      break;
    }
  auto affected = invalidation_state_.mark_changed(dependency_graph_, id.value());
  if (affected.has_error())
    return affected;
  auto reevaluated = reevaluate_affected_expressions(affected.value());
  if (reevaluated.has_error())
    return Result<std::vector<std::string>>::failure(reevaluated.error());
  return affected;
}

Result<std::vector<std::string>> PartDocument::set_parameter_formula(ParameterId id,
                                                                     std::string formula) {
  if (id.empty())
    return Result<std::vector<std::string>>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  const Parameter* existing = find_parameter(id);
  if (existing == nullptr)
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "parameter must exist in part document"));
  if (!existing->is_expression())
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "only expression parameters carry an editable formula"));

  const ParameterExpressionEvaluator evaluator;
  auto evaluation = evaluator.evaluate(*this, formula, existing->type(), id.value());
  if (evaluation.has_error())
    return Result<std::vector<std::string>>::failure(evaluation.error());

  auto updated =
      Parameter::create_expression(id, existing->name(), existing->type(), std::move(formula),
                                   evaluation.value().value, existing->scope());
  if (updated.has_error())
    return Result<std::vector<std::string>>::failure(updated.error());

  // Replace the parameter's input edges with the new formula's references and
  // reject the edit before any state changes if it would create a cycle.
  auto graph = dependency_graph_;
  graph.remove_dependencies_of_dependent(id.value());
  for (const ParameterId& referenced : evaluation.value().referenced_parameters) {
    auto edge = add_dependency_if_missing(graph, referenced.value(), id.value());
    if (edge.has_error())
      return Result<std::vector<std::string>>::failure(edge.error());
  }
  if (graph.has_cycle())
    return Result<std::vector<std::string>>::failure(Error::dependency(
        id.value(), "expression formula edit must not create a dependency cycle"));

  auto invalidation_state = invalidation_state_;
  auto synced = sync_graph(std::move(graph), invalidation_state, dependency_graph_);
  if (synced.has_error())
    return Result<std::vector<std::string>>::failure(synced.error());
  invalidation_state_ = std::move(invalidation_state);

  for (auto& parameter : parameters_)
    if (parameter.id() == id) {
      parameter = std::move(updated.value());
      break;
    }

  auto affected = invalidation_state_.mark_changed(dependency_graph_, id.value());
  if (affected.has_error())
    return affected;
  auto reevaluated = reevaluate_affected_expressions(affected.value());
  if (reevaluated.has_error())
    return Result<std::vector<std::string>>::failure(reevaluated.error());
  return affected;
}

Result<std::size_t>
PartDocument::reevaluate_affected_expressions(const std::vector<std::string>& affected_nodes) {
  // Re-evaluate affected expression parameters in topological order so chained
  // formulas read already-updated inputs.
  auto order = dependency_graph_.topological_order();
  if (order.has_error())
    return Result<std::size_t>::failure(order.error());
  const ParameterExpressionEvaluator evaluator;
  std::size_t reevaluated_count = 0U;
  for (const std::string& node : order.value()) {
    if (std::find(affected_nodes.begin(), affected_nodes.end(), node) == affected_nodes.end())
      continue;
    for (auto& parameter : parameters_) {
      if (parameter.id().value() != node || !parameter.is_expression())
        continue;
      auto evaluation = evaluator.evaluate(*this, parameter.formula().value(), parameter.type(),
                                           parameter.id().value());
      if (evaluation.has_error())
        return Result<std::size_t>::failure(evaluation.error());
      auto reevaluated = parameter.with_value(evaluation.value().value);
      if (reevaluated.has_error())
        return Result<std::size_t>::failure(reevaluated.error());
      parameter = std::move(reevaluated.value());
      ++reevaluated_count;
      break;
    }
  }
  return Result<std::size_t>::success(reevaluated_count);
}

Result<RecomputePlan> PartDocument::create_recompute_plan() const {
  return RecomputePlan::from_graph_and_invalidation_state(dependency_graph_, invalidation_state_);
}
void PartDocument::mark_all_clean() noexcept {
  invalidation_state_.mark_all_clean();
}
const DocumentId& PartDocument::id() const noexcept {
  return id_;
}
const std::string& PartDocument::name() const noexcept {
  return name_;
}
const std::vector<Parameter>& PartDocument::parameters() const noexcept {
  return parameters_;
}
const std::vector<DatumPlane>& PartDocument::datum_planes() const noexcept {
  return datum_planes_;
}
const std::vector<DatumAxis>& PartDocument::datum_axes() const noexcept {
  return datum_axes_;
}
const std::vector<ConstructionPoint>& PartDocument::construction_points() const noexcept {
  return construction_points_;
}
const std::vector<ConstructionLine>& PartDocument::construction_lines() const noexcept {
  return construction_lines_;
}
const std::vector<ConstructionPlane>& PartDocument::construction_planes() const noexcept {
  return construction_planes_;
}
const std::vector<DerivedWorkplane>& PartDocument::derived_workplanes() const noexcept {
  return derived_workplanes_;
}
const std::vector<Sketch>& PartDocument::sketches() const noexcept {
  return sketches_;
}
const std::vector<Sketch3D>& PartDocument::sketches_3d() const noexcept {
  return sketches_3d_;
}
const std::vector<Feature>& PartDocument::features() const noexcept {
  return features_;
}
const std::vector<RevolveFeature>& PartDocument::revolve_features() const noexcept {
  return revolve_features_;
}
const std::vector<LinearPatternFeature>& PartDocument::linear_pattern_features() const noexcept {
  return linear_pattern_features_;
}
const std::vector<CircularPatternFeature>&
PartDocument::circular_pattern_features() const noexcept {
  return circular_pattern_features_;
}
const std::vector<MirrorFeature>& PartDocument::mirror_features() const noexcept {
  return mirror_features_;
}
const std::vector<FilletFeature>& PartDocument::fillet_features() const noexcept {
  return fillet_features_;
}
const std::vector<ChamferFeature>& PartDocument::chamfer_features() const noexcept {
  return chamfer_features_;
}
const std::vector<ShellFeature>& PartDocument::shell_features() const noexcept {
  return shell_features_;
}
const std::vector<DraftFeature>& PartDocument::draft_features() const noexcept {
  return draft_features_;
}
const std::vector<BodyBooleanFeature>& PartDocument::body_boolean_features() const noexcept {
  return body_boolean_features_;
}

const std::vector<SketchOwnership>& PartDocument::sketch_ownerships() const noexcept {
  return sketch_ownerships_;
}

const std::vector<BodyTransform>& PartDocument::body_transforms() const noexcept {
  return body_transforms_;
}
const std::vector<Body>& PartDocument::bodies() const noexcept {
  return bodies_;
}
const std::vector<ReferenceStatusRecord>& PartDocument::reference_statuses() const noexcept {
  return reference_statuses_;
}
const std::vector<ReferenceRemapRecord>& PartDocument::reference_remaps() const noexcept {
  return reference_remaps_;
}
const std::vector<SketchOriginOverrideRecord>&
PartDocument::sketch_origin_overrides() const noexcept {
  return sketch_origin_overrides_;
}
const DependencyGraph& PartDocument::dependency_graph() const noexcept {
  return dependency_graph_;
}
const InvalidationState& PartDocument::invalidation_state() const noexcept {
  return invalidation_state_;
}
std::size_t PartDocument::parameter_count() const noexcept {
  return parameters_.size();
}
std::size_t PartDocument::datum_plane_count() const noexcept {
  return datum_planes_.size();
}
std::size_t PartDocument::datum_axis_count() const noexcept {
  return datum_axes_.size();
}
std::size_t PartDocument::construction_point_count() const noexcept {
  return construction_points_.size();
}
std::size_t PartDocument::construction_line_count() const noexcept {
  return construction_lines_.size();
}
std::size_t PartDocument::construction_plane_count() const noexcept {
  return construction_planes_.size();
}
std::size_t PartDocument::derived_workplane_count() const noexcept {
  return derived_workplanes_.size();
}
std::size_t PartDocument::sketch_count() const noexcept {
  return sketches_.size();
}
std::size_t PartDocument::sketch_3d_count() const noexcept {
  return sketches_3d_.size();
}
std::size_t PartDocument::feature_count() const noexcept {
  return features_.size();
}
std::size_t PartDocument::revolve_feature_count() const noexcept {
  return revolve_features_.size();
}
std::size_t PartDocument::linear_pattern_feature_count() const noexcept {
  return linear_pattern_features_.size();
}
std::size_t PartDocument::circular_pattern_feature_count() const noexcept {
  return circular_pattern_features_.size();
}
std::size_t PartDocument::mirror_feature_count() const noexcept {
  return mirror_features_.size();
}
std::size_t PartDocument::fillet_feature_count() const noexcept {
  return fillet_features_.size();
}
std::size_t PartDocument::chamfer_feature_count() const noexcept {
  return chamfer_features_.size();
}
std::size_t PartDocument::shell_feature_count() const noexcept {
  return shell_features_.size();
}
std::size_t PartDocument::draft_feature_count() const noexcept {
  return draft_features_.size();
}
std::size_t PartDocument::body_boolean_feature_count() const noexcept {
  return body_boolean_features_.size();
}

std::size_t PartDocument::sketch_ownership_count() const noexcept {
  return sketch_ownerships_.size();
}

std::size_t PartDocument::body_transform_count() const noexcept {
  return body_transforms_.size();
}
std::size_t PartDocument::body_count() const noexcept {
  return bodies_.size();
}
std::size_t PartDocument::reference_status_count() const noexcept {
  return reference_statuses_.size();
}
std::size_t PartDocument::reference_remap_count() const noexcept {
  return reference_remaps_.size();
}
std::size_t PartDocument::sketch_origin_override_count() const noexcept {
  return sketch_origin_overrides_.size();
}
const Parameter* PartDocument::find_parameter(ParameterId id) const noexcept {
  for (const auto& parameter : parameters_)
    if (parameter.id() == id)
      return &parameter;
  return nullptr;
}
const Parameter* PartDocument::find_parameter(std::string_view name) const noexcept {
  for (const auto& parameter : parameters_)
    if (parameter.name() == name)
      return &parameter;
  return nullptr;
}
const DatumPlane* PartDocument::find_datum_plane(DatumPlaneId id) const noexcept {
  for (const auto& datum_plane : datum_planes_)
    if (datum_plane.id() == id)
      return &datum_plane;
  return nullptr;
}
const DatumAxis* PartDocument::find_datum_axis(DatumAxisId id) const noexcept {
  for (const auto& datum_axis : datum_axes_)
    if (datum_axis.id() == id)
      return &datum_axis;
  return nullptr;
}
const ConstructionPoint*
PartDocument::find_construction_point(ConstructionPointId id) const noexcept {
  for (const auto& point : construction_points_)
    if (point.id() == id)
      return &point;
  return nullptr;
}
const ConstructionLine* PartDocument::find_construction_line(ConstructionLineId id) const noexcept {
  for (const auto& line : construction_lines_)
    if (line.id() == id)
      return &line;
  return nullptr;
}
const ConstructionPlane*
PartDocument::find_construction_plane(ConstructionPlaneId id) const noexcept {
  for (const auto& plane : construction_planes_)
    if (plane.id() == id)
      return &plane;
  return nullptr;
}
const DerivedWorkplane* PartDocument::find_derived_workplane(DatumPlaneId id) const noexcept {
  for (const auto& workplane : derived_workplanes_)
    if (workplane.id() == id)
      return &workplane;
  return nullptr;
}
const Sketch* PartDocument::find_sketch(SketchId id) const noexcept {
  for (const auto& sketch : sketches_)
    if (sketch.id() == id)
      return &sketch;
  return nullptr;
}
const Sketch3D* PartDocument::find_sketch_3d(Sketch3DId id) const noexcept {
  for (const auto& sketch : sketches_3d_)
    if (sketch.id() == id)
      return &sketch;
  return nullptr;
}
const Feature* PartDocument::find_feature(FeatureId id) const noexcept {
  for (const auto& feature : features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const RevolveFeature* PartDocument::find_revolve_feature(FeatureId id) const noexcept {
  for (const auto& feature : revolve_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const LinearPatternFeature* PartDocument::find_linear_pattern_feature(FeatureId id) const noexcept {
  for (const auto& feature : linear_pattern_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const CircularPatternFeature*
PartDocument::find_circular_pattern_feature(FeatureId id) const noexcept {
  for (const auto& feature : circular_pattern_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const MirrorFeature* PartDocument::find_mirror_feature(FeatureId id) const noexcept {
  for (const auto& feature : mirror_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const FilletFeature* PartDocument::find_fillet_feature(FeatureId id) const noexcept {
  for (const auto& feature : fillet_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const ChamferFeature* PartDocument::find_chamfer_feature(FeatureId id) const noexcept {
  for (const auto& feature : chamfer_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const ShellFeature* PartDocument::find_shell_feature(FeatureId id) const noexcept {
  for (const auto& feature : shell_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const DraftFeature* PartDocument::find_draft_feature(FeatureId id) const noexcept {
  for (const auto& feature : draft_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}
const BodyBooleanFeature* PartDocument::find_body_boolean_feature(FeatureId id) const noexcept {
  for (const auto& feature : body_boolean_features_)
    if (feature.id() == id)
      return &feature;
  return nullptr;
}

const SketchOwnership* PartDocument::find_sketch_ownership(SketchId id) const noexcept {
  const auto position =
      std::lower_bound(sketch_ownerships_.begin(), sketch_ownerships_.end(), id,
                       [](const SketchOwnership& ownership, const SketchId& candidate) {
                         return ownership.sketch().value() < candidate.value();
                       });
  return position != sketch_ownerships_.end() && position->sketch() == id ? &*position : nullptr;
}

const BodyTransform* PartDocument::find_body_transform(BodyTransformId id) const noexcept {
  for (const auto& transform : body_transforms_)
    if (transform.id() == id)
      return &transform;
  return nullptr;
}
const Body* PartDocument::find_body(BodyId id) const noexcept {
  const auto found = std::lower_bound(
      bodies_.begin(), bodies_.end(), id.value(),
      [](const Body& body, const std::string& value) { return body.id().value() < value; });
  return found != bodies_.end() && found->id() == id ? &*found : nullptr;
}
const ReferenceStatusRecord*
PartDocument::find_reference_status(ReferenceStatusId id) const noexcept {
  for (const auto& status : reference_statuses_)
    if (status.id() == id)
      return &status;
  return nullptr;
}
const ReferenceRemapRecord* PartDocument::find_reference_remap(ReferenceRemapId id) const noexcept {
  for (const auto& remap : reference_remaps_)
    if (remap.id() == id)
      return &remap;
  return nullptr;
}
const SketchOriginOverrideRecord*
PartDocument::find_sketch_origin_override(SketchId id) const noexcept {
  for (const auto& origin : sketch_origin_overrides_)
    if (origin.sketch() == id)
      return &origin;
  return nullptr;
}
bool PartDocument::has_workplane_id(const DatumPlaneId& id) const noexcept {
  return has_datum_plane_id(id) || has_derived_workplane_id(id) ||
         has_construction_plane_id(ConstructionPlaneId(id.value()));
}
PartDocument::PartDocument(DocumentId id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}
bool PartDocument::has_parameter_id(const ParameterId& id) const noexcept {
  return find_parameter(id) != nullptr;
}
bool PartDocument::has_parameter_name(std::string_view name) const noexcept {
  return find_parameter(name) != nullptr;
}
bool PartDocument::has_datum_plane_id(const DatumPlaneId& id) const noexcept {
  return find_datum_plane(id) != nullptr;
}
bool PartDocument::has_datum_axis_id(const DatumAxisId& id) const noexcept {
  return find_datum_axis(id) != nullptr;
}
bool PartDocument::has_construction_point_id(const ConstructionPointId& id) const noexcept {
  return find_construction_point(id) != nullptr;
}
bool PartDocument::has_construction_line_id(const ConstructionLineId& id) const noexcept {
  return find_construction_line(id) != nullptr;
}
bool PartDocument::has_construction_plane_id(const ConstructionPlaneId& id) const noexcept {
  return find_construction_plane(id) != nullptr;
}
bool PartDocument::has_derived_workplane_id(const DatumPlaneId& id) const noexcept {
  return find_derived_workplane(id) != nullptr;
}
bool PartDocument::has_sketch_id(const SketchId& id) const noexcept {
  return find_sketch(id) != nullptr;
}
bool PartDocument::has_sketch_3d_id(const Sketch3DId& id) const noexcept {
  return find_sketch_3d(id) != nullptr;
}
bool PartDocument::has_feature_id(const FeatureId& id) const noexcept {
  return find_feature(id) != nullptr;
}
bool PartDocument::has_revolve_feature_id(const FeatureId& id) const noexcept {
  return find_revolve_feature(id) != nullptr;
}
bool PartDocument::has_linear_pattern_feature_id(const FeatureId& id) const noexcept {
  return find_linear_pattern_feature(id) != nullptr;
}
bool PartDocument::has_circular_pattern_feature_id(const FeatureId& id) const noexcept {
  return find_circular_pattern_feature(id) != nullptr;
}
bool PartDocument::has_mirror_feature_id(const FeatureId& id) const noexcept {
  return find_mirror_feature(id) != nullptr;
}
bool PartDocument::has_fillet_feature_id(const FeatureId& id) const noexcept {
  return find_fillet_feature(id) != nullptr;
}
bool PartDocument::has_chamfer_feature_id(const FeatureId& id) const noexcept {
  return find_chamfer_feature(id) != nullptr;
}
bool PartDocument::has_shell_feature_id(const FeatureId& id) const noexcept {
  return find_shell_feature(id) != nullptr;
}
bool PartDocument::has_draft_feature_id(const FeatureId& id) const noexcept {
  return find_draft_feature(id) != nullptr;
}
bool PartDocument::has_body_boolean_feature_id(const FeatureId& id) const noexcept {
  return find_body_boolean_feature(id) != nullptr;
}

bool PartDocument::has_sketch_ownership_id(const SketchId& id) const noexcept {
  return find_sketch_ownership(id) != nullptr;
}

bool PartDocument::has_body_transform_id(const BodyTransformId& id) const noexcept {
  return find_body_transform(id) != nullptr;
}
bool PartDocument::has_any_feature_id(const FeatureId& id) const noexcept {
  return has_feature_id(id) || has_revolve_feature_id(id) || has_linear_pattern_feature_id(id) ||
         has_circular_pattern_feature_id(id) || has_mirror_feature_id(id) ||
         has_fillet_feature_id(id) || has_chamfer_feature_id(id) || has_shell_feature_id(id) ||
         has_draft_feature_id(id) || has_body_boolean_feature_id(id);
}
bool PartDocument::has_body_id(const BodyId& id) const noexcept {
  return find_body(id) != nullptr;
}
bool PartDocument::has_reference_status_id(const ReferenceStatusId& id) const noexcept {
  return find_reference_status(id) != nullptr;
}
bool PartDocument::has_reference_remap_id(const ReferenceRemapId& id) const noexcept {
  return find_reference_remap(id) != nullptr;
}
bool PartDocument::has_sketch_origin_override_id(const SketchId& id) const noexcept {
  return find_sketch_origin_override(id) != nullptr;
}

} // namespace blcad
