#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

struct ResolvedOccurrenceContext {
  AssemblyHierarchyOccurrenceDescriptor occurrence;
  const AssemblyDocument* assembly = nullptr;
};

struct ResolvedHierarchyGeometricTarget {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  ComponentInstanceId component_instance;
  std::string semantic_reference;
  AssemblyResolvedGeometricTarget target;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string
occurrence_object_id(const std::vector<SubassemblyInstanceId>& occurrence_path) {
  if (occurrence_path.empty()) {
    return "assembly.root";
  }
  return occurrence_path.back().value();
}

[[nodiscard]] Result<ResolvedOccurrenceContext>
resolve_occurrence(const Project& project,
                   const std::vector<SubassemblyInstanceId>& occurrence_path) {
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (hierarchy.has_error()) {
    return Result<ResolvedOccurrenceContext>::failure(hierarchy.error());
  }

  const auto found =
      std::find_if(hierarchy.value().occurrences().begin(), hierarchy.value().occurrences().end(),
                   [&occurrence_path](const AssemblyHierarchyOccurrenceDescriptor& occurrence) {
                     return occurrence.occurrence_path == occurrence_path;
                   });
  if (found == hierarchy.value().occurrences().end()) {
    return Result<ResolvedOccurrenceContext>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "cross-hierarchy relationship target occurrence path must exist in the rooted hierarchy"));
  }

  const AssemblyDocument* assembly = project.find_assembly_document(found->assembly_document);
  if (assembly == nullptr) {
    return Result<ResolvedOccurrenceContext>::failure(
        validation_error(found->assembly_document.value(),
                         "cross-hierarchy relationship target assembly must resolve in project"));
  }

  return Result<ResolvedOccurrenceContext>::success(ResolvedOccurrenceContext{*found, assembly});
}

[[nodiscard]] Result<Project> make_local_target_view(const Project& project,
                                                     const AssemblyDocument& assembly) {
  auto local_project = Project::create(project.id(), project.name(), assembly);
  if (local_project.has_error()) {
    return local_project;
  }
  for (const PartDocument& part : project.part_documents()) {
    auto added = local_project.value().add_part_document(part);
    if (added.has_error()) {
      return Result<Project>::failure(added.error());
    }
  }
  return local_project;
}

[[nodiscard]] Result<AssemblyConstraintTarget>
make_local_target(const AssemblyHierarchyConstraintTarget& target) {
  return AssemblyConstraintTarget::create(target.component_instance(), target.semantic_reference());
}

[[nodiscard]] std::vector<RigidTransform>
target_transform_chain(const RigidTransform& component_transform,
                       const AssemblyHierarchyOccurrenceDescriptor& occurrence) {
  std::vector<RigidTransform> transforms{component_transform};
  transforms.insert(transforms.end(), occurrence.parent_transforms_inner_to_outer.begin(),
                    occurrence.parent_transforms_inner_to_outer.end());
  return transforms;
}

[[nodiscard]] AssemblyGeometricTargetDescriptor
evaluate_geometric_descriptor(const std::vector<RigidTransform>& transforms,
                              const AssemblyGeometricTargetDescriptor& descriptor) {
  const AssemblyHierarchyTransformEvaluator evaluator;
  return std::visit(
      [&](const auto& value) -> AssemblyGeometricTargetDescriptor {
        using Descriptor = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Descriptor, AssemblyPlanarTargetDescriptor>) {
          return AssemblyPlanarTargetDescriptor{
              evaluator.evaluate_point(transforms, value.origin),
              evaluator.evaluate_vector(transforms, value.x_axis),
              evaluator.evaluate_vector(transforms, value.y_axis),
              evaluator.evaluate_vector(transforms, value.normal)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyAxisTargetDescriptor>) {
          return AssemblyAxisTargetDescriptor{
              evaluator.evaluate_point(transforms, value.origin),
              evaluator.evaluate_vector(transforms, value.direction)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyLineTargetDescriptor>) {
          return AssemblyLineTargetDescriptor{
              evaluator.evaluate_point(transforms, value.origin),
              evaluator.evaluate_vector(transforms, value.direction)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyPointTargetDescriptor>) {
          return AssemblyPointTargetDescriptor{evaluator.evaluate_point(transforms, value.point)};
        } else if constexpr (std::is_same_v<Descriptor, AssemblyCircularEdgeTargetDescriptor>) {
          return AssemblyCircularEdgeTargetDescriptor{
              evaluator.evaluate_point(transforms, value.center),
              evaluator.evaluate_vector(transforms, value.x_axis),
              evaluator.evaluate_vector(transforms, value.y_axis),
              evaluator.evaluate_vector(transforms, value.normal), value.radius_mm};
        } else if constexpr (std::is_same_v<Descriptor,
                                            AssemblyCylindricalSurfaceTargetDescriptor>) {
          return AssemblyCylindricalSurfaceTargetDescriptor{
              evaluator.evaluate_point(transforms, value.axis_origin),
              evaluator.evaluate_vector(transforms, value.axis_direction), value.radius_mm};
        } else {
          return AssemblyFrameTargetDescriptor{evaluator.evaluate_point(transforms, value.origin),
                                               evaluator.evaluate_vector(transforms, value.x_axis),
                                               evaluator.evaluate_vector(transforms, value.y_axis),
                                               evaluator.evaluate_vector(transforms, value.z_axis)};
        }
      },
      descriptor);
}

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] Vector3 add(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] Result<ResolvedHierarchyGeometricTarget>
resolve_hierarchy_geometric_target(const Project& project,
                                   const AssemblyHierarchyConstraintTarget& target) {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<ResolvedHierarchyGeometricTarget>::failure(occurrence.error());
  }

  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto resolved = resolver.resolve_geometric(project, target);
  if (resolved.has_error()) {
    return Result<ResolvedHierarchyGeometricTarget>::failure(resolved.error());
  }

  return Result<ResolvedHierarchyGeometricTarget>::success(ResolvedHierarchyGeometricTarget{
      target.occurrence_path(), occurrence.value().occurrence.assembly_document,
      target.component_instance(), target.semantic_reference(), std::move(resolved.value())});
}

[[nodiscard]] Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>
project_hierarchy_planar_target(const ResolvedHierarchyGeometricTarget& resolved) {
  auto plane = project_plane(resolved.target);
  if (plane.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(plane.error());
  }
  const AssemblyGeometricTargetSourceMetadata& source = resolved.target.source_metadata;
  return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::success(
      AssemblyHierarchyPlanarConstraintTargetDescriptor{
          resolved.occurrence_path, resolved.assembly_document, resolved.component_instance,
          source.referenced_part_document, source.source_feature.value_or(FeatureId{}),
          source.semantic_face.value_or(SemanticFace::Top), resolved.semantic_reference,
          AssemblySpacePlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                        plane.value().y_axis, plane.value().normal}});
}

[[nodiscard]] Result<AssemblyHierarchyAxisConstraintTargetDescriptor>
project_hierarchy_axis_target(const ResolvedHierarchyGeometricTarget& resolved) {
  auto axis = project_axis(resolved.target);
  if (axis.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(axis.error());
  }
  const AssemblyGeometricTargetSourceMetadata& source = resolved.target.source_metadata;
  return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::success(
      AssemblyHierarchyAxisConstraintTargetDescriptor{
          resolved.occurrence_path, resolved.assembly_document, resolved.component_instance,
          source.referenced_part_document, source.source_feature.value_or(FeatureId{}),
          source.source_profile.value_or(ProfileId{}),
          source.semantic_axis.value_or(SemanticAxis::Primary), resolved.semantic_reference,
          AssemblySpaceAxisDescriptor{axis.value().origin, axis.value().direction}});
}

[[nodiscard]] Result<AssemblyHierarchyInsertConstraintTargetDescriptor>
project_hierarchy_insert_target(const ResolvedHierarchyGeometricTarget& resolved) {
  auto frame = project_frame(resolved.target);
  if (frame.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(frame.error());
  }
  auto axis = project_axis(resolved.target);
  if (axis.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(axis.error());
  }
  auto plane = project_plane(resolved.target);
  if (plane.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(plane.error());
  }
  const AssemblyGeometricTargetSourceMetadata& source = resolved.target.source_metadata;
  return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::success(
      AssemblyHierarchyInsertConstraintTargetDescriptor{
          resolved.occurrence_path, resolved.assembly_document, resolved.component_instance,
          source.referenced_part_document, source.source_feature.value_or(FeatureId{}),
          source.source_profile.value_or(ProfileId{}),
          source.semantic_axis.value_or(SemanticAxis::Primary),
          source.semantic_seating_plane.value_or(SemanticSeatingPlane::Primary),
          resolved.semantic_reference,
          AssemblySpaceAxisDescriptor{axis.value().origin, axis.value().direction},
          AssemblySpacePlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                        plane.value().y_axis, plane.value().normal}});
}

[[nodiscard]] bool generic_relationship_type(AssemblyConstraintType type) noexcept {
  return type == AssemblyConstraintType::Coincident || type == AssemblyConstraintType::Parallel ||
         type == AssemblyConstraintType::Perpendicular;
}

[[nodiscard]] AssemblyHierarchyGenericConstraintTargetDescriptor
project_hierarchy_generic_target(const ResolvedHierarchyGeometricTarget& resolved,
                                 const AssemblyGenericRelationshipTargetDescriptor& target) {
  return AssemblyHierarchyGenericConstraintTargetDescriptor{
      resolved.occurrence_path,      resolved.assembly_document,  resolved.component_instance,
      target.target.source_metadata, resolved.semantic_reference, target.selected_capability,
      target.target.descriptor};
}

[[nodiscard]] bool selected_plane_pair(const AssemblyTargetCompatibility& compatibility) noexcept {
  return compatibility.target_a_capability == AssemblyGeometricTargetCapability::Plane &&
         compatibility.target_b_capability == AssemblyGeometricTargetCapability::Plane;
}

} // namespace

Result<AssemblyHierarchyConstraintTarget>
AssemblyHierarchyConstraintTarget::create(std::vector<SubassemblyInstanceId> occurrence_path,
                                          ComponentInstanceId component_instance,
                                          std::string semantic_reference) {
  for (const SubassemblyInstanceId& occurrence : occurrence_path) {
    if (occurrence.empty()) {
      return Result<AssemblyHierarchyConstraintTarget>::failure(
          Error::validation("assembly_hierarchy_constraint_target",
                            "cross-hierarchy relationship occurrence path ids must not be empty"));
    }
  }

  auto local_target = AssemblyConstraintTarget::create(component_instance, semantic_reference);
  if (local_target.has_error()) {
    return Result<AssemblyHierarchyConstraintTarget>::failure(local_target.error());
  }

  return Result<AssemblyHierarchyConstraintTarget>::success(AssemblyHierarchyConstraintTarget(
      std::move(occurrence_path), std::move(component_instance), std::move(semantic_reference)));
}

AssemblyHierarchyConstraintTarget::AssemblyHierarchyConstraintTarget(
    std::vector<SubassemblyInstanceId> occurrence_path, ComponentInstanceId component_instance,
    std::string semantic_reference)
    : occurrence_path_(std::move(occurrence_path)),
      component_instance_(std::move(component_instance)),
      semantic_reference_(std::move(semantic_reference)) {}

const std::vector<SubassemblyInstanceId>&
AssemblyHierarchyConstraintTarget::occurrence_path() const noexcept {
  return occurrence_path_;
}

const ComponentInstanceId& AssemblyHierarchyConstraintTarget::component_instance() const noexcept {
  return component_instance_;
}

const std::string& AssemblyHierarchyConstraintTarget::semantic_reference() const noexcept {
  return semantic_reference_;
}

Result<AssemblyHierarchyConstraintQuery> AssemblyHierarchyConstraintQuery::create(
    AssemblyConstraintId id, AssemblyConstraintType type,
    AssemblyHierarchyConstraintTarget target_a, AssemblyHierarchyConstraintTarget target_b,
    std::optional<Quantity> distance, std::optional<Quantity> angle) {
  auto local_target_a = AssemblyConstraintTarget::create(target_a.component_instance(),
                                                         target_a.semantic_reference());
  if (local_target_a.has_error()) {
    return Result<AssemblyHierarchyConstraintQuery>::failure(local_target_a.error());
  }
  auto local_target_b = AssemblyConstraintTarget::create(target_b.component_instance(),
                                                         target_b.semantic_reference());
  if (local_target_b.has_error()) {
    return Result<AssemblyHierarchyConstraintQuery>::failure(local_target_b.error());
  }

  const std::string validation_name = id.empty() ? "cross-hierarchy relationship" : id.value();
  auto validated = AssemblyConstraint::create(
      id, validation_name, type, std::move(local_target_a.value()),
      std::move(local_target_b.value()), AssemblyConstraintState::Active, distance, angle);
  if (validated.has_error()) {
    return Result<AssemblyHierarchyConstraintQuery>::failure(validated.error());
  }

  return Result<AssemblyHierarchyConstraintQuery>::success(AssemblyHierarchyConstraintQuery(
      validated.value().id(), validated.value().type(), std::move(target_a), std::move(target_b),
      validated.value().distance(), validated.value().angle()));
}

AssemblyHierarchyConstraintQuery::AssemblyHierarchyConstraintQuery(
    AssemblyConstraintId id, AssemblyConstraintType type,
    AssemblyHierarchyConstraintTarget target_a, AssemblyHierarchyConstraintTarget target_b,
    std::optional<Quantity> distance, std::optional<Quantity> angle)
    : id_(std::move(id)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), distance_(std::move(distance)), angle_(std::move(angle)) {}

const AssemblyConstraintId& AssemblyHierarchyConstraintQuery::id() const noexcept {
  return id_;
}

AssemblyConstraintType AssemblyHierarchyConstraintQuery::type() const noexcept {
  return type_;
}

const AssemblyHierarchyConstraintTarget&
AssemblyHierarchyConstraintQuery::target_a() const noexcept {
  return target_a_;
}

const AssemblyHierarchyConstraintTarget&
AssemblyHierarchyConstraintQuery::target_b() const noexcept {
  return target_b_;
}

const std::optional<Quantity>& AssemblyHierarchyConstraintQuery::distance() const noexcept {
  return distance_;
}

const std::optional<Quantity>& AssemblyHierarchyConstraintQuery::angle() const noexcept {
  return angle_;
}

Result<AssemblyResolvedGeometricTarget>
AssemblyHierarchyConstraintTargetResolver::resolve_geometric(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(occurrence.error());
  }
  auto local_project = make_local_target_view(project, *occurrence.value().assembly);
  if (local_project.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(local_project.error());
  }
  auto local_target = make_local_target(target);
  if (local_target.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(local_target.error());
  }

  const AssemblyConstraintTargetResolver local_resolver;
  auto resolved = local_resolver.resolve_geometric(local_project.value(), local_target.value());
  if (resolved.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
  }
  if (resolved.value().transforms_inner_to_outer.size() != 1U) {
    return Result<AssemblyResolvedGeometricTarget>::failure(
        validation_error(target.semantic_reference(),
                         "local typed target must retain exactly one direct component transform"));
  }

  const auto transforms = target_transform_chain(resolved.value().transforms_inner_to_outer.front(),
                                                 occurrence.value().occurrence);
  AssemblyResolvedGeometricTarget hierarchy_target{
      AssemblyHierarchyGeometricTargetEndpointIdentity{
          target.occurrence_path(), target.component_instance(), target.semantic_reference()},
      resolved.value().source_kind,
      resolved.value().source_metadata,
      evaluate_geometric_descriptor(transforms, resolved.value().descriptor),
      resolved.value().capabilities,
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      transforms};
  auto valid = validate_resolved_geometric_target(hierarchy_target);
  if (valid.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(valid.error());
  }
  return Result<AssemblyResolvedGeometricTarget>::success(std::move(hierarchy_target));
}

Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>
AssemblyHierarchyConstraintTargetResolver::resolve_planar(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(occurrence.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(typed.error());
  }
  auto plane = project_plane(typed.value());
  if (plane.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(plane.error());
  }
  const auto& source = typed.value().source_metadata;
  if (!source.source_feature.has_value() || !source.semantic_face.has_value()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(validation_error(
        target.semantic_reference(), "hierarchy planar target source metadata is incomplete"));
  }
  return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::success(
      AssemblyHierarchyPlanarConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          target.component_instance(), source.referenced_part_document,
          source.source_feature.value(), source.semantic_face.value(), target.semantic_reference(),
          AssemblySpacePlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                        plane.value().y_axis, plane.value().normal}});
}

Result<AssemblyHierarchyAxisConstraintTargetDescriptor>
AssemblyHierarchyConstraintTargetResolver::resolve_axis(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(occurrence.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(typed.error());
  }
  auto axis = project_axis(typed.value());
  if (axis.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(axis.error());
  }
  const auto& source = typed.value().source_metadata;
  if (!source.source_feature.has_value() || !source.source_profile.has_value() ||
      !source.semantic_axis.has_value()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(validation_error(
        target.semantic_reference(), "hierarchy axis target source metadata is incomplete"));
  }
  return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::success(
      AssemblyHierarchyAxisConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          target.component_instance(), source.referenced_part_document,
          source.source_feature.value(), source.source_profile.value(),
          source.semantic_axis.value(), target.semantic_reference(),
          AssemblySpaceAxisDescriptor{axis.value().origin, axis.value().direction}});
}

Result<AssemblyHierarchyInsertConstraintTargetDescriptor>
AssemblyHierarchyConstraintTargetResolver::resolve_insert(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(occurrence.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(typed.error());
  }
  auto axis = project_axis(typed.value());
  if (axis.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(axis.error());
  }
  auto plane = project_plane(typed.value());
  if (plane.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(plane.error());
  }
  const auto& source = typed.value().source_metadata;
  if (!source.source_feature.has_value() || !source.source_profile.has_value() ||
      !source.semantic_axis.has_value() || !source.semantic_seating_plane.has_value()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(validation_error(
        target.semantic_reference(), "hierarchy insert target source metadata is incomplete"));
  }
  return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::success(
      AssemblyHierarchyInsertConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          target.component_instance(), source.referenced_part_document,
          source.source_feature.value(), source.source_profile.value(),
          source.semantic_axis.value(), source.semantic_seating_plane.value(),
          target.semantic_reference(),
          AssemblySpaceAxisDescriptor{axis.value().origin, axis.value().direction},
          AssemblySpacePlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                        plane.value().y_axis, plane.value().normal}});
}

Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  if (generic_relationship_type(query.type())) {
    auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
    if (geometric_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_a.error());
    }
    auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
    if (geometric_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_b.error());
    }

    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation =
        generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                              geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }

  if (query.type() == AssemblyConstraintType::Concentric) {
    auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
    if (geometric_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_a.error());
    }
    auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
    if (geometric_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_b.error());
    }

    const AssemblyTargetCompatibilityResolver compatibility_resolver;
    auto compatibility = compatibility_resolver.resolve(
        query.type(), geometric_target_a.value().target, geometric_target_b.value().target);
    if (compatibility.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(compatibility.error());
    }

    auto target_a = project_hierarchy_axis_target(geometric_target_a.value());
    if (target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
    }
    auto target_b = project_hierarchy_axis_target(geometric_target_b.value());
    if (target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_b.error());
    }
    const Vector3 origin_delta =
        difference(target_b.value().axis.origin, target_a.value().axis.origin);
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(), target_a.value(), target_b.value(),
            ConcentricResidualDescriptor{
                cross(target_a.value().axis.direction, target_b.value().axis.direction),
                cross(origin_delta, target_a.value().axis.direction)}});
  }

  if (query.type() == AssemblyConstraintType::Insert) {
    auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
    if (geometric_target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_a.error());
    }
    auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
    if (geometric_target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
          geometric_target_b.error());
    }

    const AssemblyTargetCompatibilityResolver compatibility_resolver;
    auto compatibility = compatibility_resolver.resolve(
        query.type(), geometric_target_a.value().target, geometric_target_b.value().target);
    if (compatibility.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(compatibility.error());
    }

    auto target_a = project_hierarchy_insert_target(geometric_target_a.value());
    if (target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
    }
    auto target_b = project_hierarchy_insert_target(geometric_target_b.value());
    if (target_b.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_b.error());
    }
    const Vector3 axis_origin_delta =
        difference(target_b.value().axis.origin, target_a.value().axis.origin);
    const Vector3 seat_origin_delta =
        difference(target_b.value().seating_plane.origin, target_a.value().seating_plane.origin);
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(), target_a.value(), target_b.value(),
            InsertResidualDescriptor{
                cross(target_a.value().axis.direction, target_b.value().axis.direction),
                cross(axis_origin_delta, target_a.value().axis.direction),
                dot(seat_origin_delta, target_a.value().seating_plane.normal)}});
  }

  auto geometric_target_a = resolve_hierarchy_geometric_target(project, query.target_a());
  if (geometric_target_a.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
        geometric_target_a.error());
  }
  auto geometric_target_b = resolve_hierarchy_geometric_target(project, query.target_b());
  if (geometric_target_b.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(
        geometric_target_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(
      query.type(), geometric_target_a.value().target, geometric_target_b.value().target);
  if (compatibility.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(compatibility.error());
  }
  if (query.type() == AssemblyConstraintType::Angle &&
      !selected_plane_pair(compatibility.value())) {
    const AssemblyGenericRelationshipEquationBuilder generic_builder;
    auto equation =
        generic_builder.build(query.id(), query.type(), geometric_target_a.value().target,
                              geometric_target_b.value().target, query.angle());
    if (equation.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(equation.error());
    }
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(),
            project_hierarchy_generic_target(geometric_target_a.value(), equation.value().target_a),
            project_hierarchy_generic_target(geometric_target_b.value(), equation.value().target_b),
            equation.value().residual});
  }
  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar equation currently requires Plane/Plane target compatibility"));
  }

  auto target_a = project_hierarchy_planar_target(geometric_target_a.value());
  if (target_a.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = project_hierarchy_planar_target(geometric_target_b.value());
  if (target_b.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_b.error());
  }

  const Vector3 origin_delta =
      difference(target_b.value().plane.origin, target_a.value().plane.origin);
  const double signed_separation_mm = dot(origin_delta, target_a.value().plane.normal);
  if (query.type() == AssemblyConstraintType::Mate) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(), target_a.value(), target_b.value(),
            PlanarMateResidualDescriptor{
                add(target_a.value().plane.normal, target_b.value().plane.normal),
                signed_separation_mm}});
  }

  if (query.type() == AssemblyConstraintType::Angle) {
    if (!query.angle().has_value() || query.angle()->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
          query.id().value(),
          "cross-hierarchy planar Angle equation requires an angle value in degrees"));
    }
    const double target_angle_deg = query.angle()->degrees();
    const double target_cosine = std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
    const double normal_dot = dot(target_a.value().plane.normal, target_b.value().plane.normal);
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
        AssemblyHierarchyConstraintEquationDescriptor{
            query.id(), query.type(), target_a.value(), target_b.value(),
            PlanarAngleResidualDescriptor{target_angle_deg, normal_dot,
                                          normal_dot - target_cosine}});
  }

  if (query.type() != AssemblyConstraintType::Distance || !query.distance().has_value() ||
      query.distance()->kind() != QuantityKind::LengthMm) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
        query.id().value(),
        "cross-hierarchy planar Distance equation requires a length distance value"));
  }
  const double target_distance_mm = query.distance()->millimeters();
  return Result<AssemblyHierarchyConstraintEquationDescriptor>::success(
      AssemblyHierarchyConstraintEquationDescriptor{
          query.id(), query.type(), target_a.value(), target_b.value(),
          PlanarDistanceResidualDescriptor{
              cross(target_a.value().plane.normal, target_b.value().plane.normal),
              target_distance_mm, signed_separation_mm,
              signed_separation_mm - target_distance_mm}});
}

} // namespace blcad::geometry
