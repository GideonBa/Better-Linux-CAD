#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

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

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string occurrence_object_id(
    const std::vector<SubassemblyInstanceId>& occurrence_path) {
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

  const auto found = std::find_if(
      hierarchy.value().occurrences().begin(), hierarchy.value().occurrences().end(),
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
    return Result<ResolvedOccurrenceContext>::failure(validation_error(
        found->assembly_document.value(),
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

[[nodiscard]] AssemblySpacePlanarDescriptor
evaluate_plane(const std::vector<RigidTransform>& transforms,
               const ComponentLocalPlanarDescriptor& local) {
  const AssemblyHierarchyTransformEvaluator evaluator;
  return AssemblySpacePlanarDescriptor{evaluator.evaluate_point(transforms, local.origin),
                                       evaluator.evaluate_vector(transforms, local.x_axis),
                                       evaluator.evaluate_vector(transforms, local.y_axis),
                                       evaluator.evaluate_vector(transforms, local.normal)};
}

[[nodiscard]] AssemblySpaceAxisDescriptor
evaluate_axis(const std::vector<RigidTransform>& transforms,
              const ComponentLocalAxisDescriptor& local) {
  const AssemblyHierarchyTransformEvaluator evaluator;
  return AssemblySpaceAxisDescriptor{evaluator.evaluate_point(transforms, local.origin),
                                     evaluator.evaluate_vector(transforms, local.direction)};
}

[[nodiscard]] AssemblyGeometricTargetDescriptor evaluate_geometric_descriptor(
    const std::vector<RigidTransform>& transforms,
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
        } else if constexpr (
            std::is_same_v<Descriptor, AssemblyCylindricalSurfaceTargetDescriptor>) {
          return AssemblyCylindricalSurfaceTargetDescriptor{
              evaluator.evaluate_point(transforms, value.axis_origin),
              evaluator.evaluate_vector(transforms, value.axis_direction), value.radius_mm};
        } else {
          return AssemblyFrameTargetDescriptor{
              evaluator.evaluate_point(transforms, value.origin),
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

} // namespace

Result<AssemblyHierarchyConstraintTarget> AssemblyHierarchyConstraintTarget::create(
    std::vector<SubassemblyInstanceId> occurrence_path,
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
    std::vector<SubassemblyInstanceId> occurrence_path,
    ComponentInstanceId component_instance,
    std::string semantic_reference)
    : occurrence_path_(std::move(occurrence_path)),
      component_instance_(std::move(component_instance)),
      semantic_reference_(std::move(semantic_reference)) {}

const std::vector<SubassemblyInstanceId>&
AssemblyHierarchyConstraintTarget::occurrence_path() const noexcept {
  return occurrence_path_;
}

const ComponentInstanceId&
AssemblyHierarchyConstraintTarget::component_instance() const noexcept {
  return component_instance_;
}

const std::string& AssemblyHierarchyConstraintTarget::semantic_reference() const noexcept {
  return semantic_reference_;
}

Result<AssemblyHierarchyConstraintQuery> AssemblyHierarchyConstraintQuery::create(
    AssemblyConstraintId id,
    AssemblyConstraintType type,
    AssemblyHierarchyConstraintTarget target_a,
    AssemblyHierarchyConstraintTarget target_b,
    std::optional<Quantity> distance,
    std::optional<Quantity> angle) {
  auto local_target_a =
      AssemblyConstraintTarget::create(target_a.component_instance(), target_a.semantic_reference());
  if (local_target_a.has_error()) {
    return Result<AssemblyHierarchyConstraintQuery>::failure(local_target_a.error());
  }
  auto local_target_b =
      AssemblyConstraintTarget::create(target_b.component_instance(), target_b.semantic_reference());
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
    AssemblyConstraintId id,
    AssemblyConstraintType type,
    AssemblyHierarchyConstraintTarget target_a,
    AssemblyHierarchyConstraintTarget target_b,
    std::optional<Quantity> distance,
    std::optional<Quantity> angle)
    : id_(std::move(id)), type_(type), target_a_(std::move(target_a)),
      target_b_(std::move(target_b)), distance_(std::move(distance)), angle_(std::move(angle)) {}

const AssemblyConstraintId& AssemblyHierarchyConstraintQuery::id() const noexcept { return id_; }

AssemblyConstraintType AssemblyHierarchyConstraintQuery::type() const noexcept { return type_; }

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
    return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
        target.semantic_reference(),
        "local typed target must retain exactly one direct component transform"));
  }

  const auto transforms = target_transform_chain(
      resolved.value().transforms_inner_to_outer.front(), occurrence.value().occurrence);
  AssemblyResolvedGeometricTarget hierarchy_target{
      AssemblyHierarchyGeometricTargetEndpointIdentity{target.occurrence_path(),
                                                       target.component_instance(),
                                                       target.semantic_reference()},
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
  auto local_project = make_local_target_view(project, *occurrence.value().assembly);
  if (local_project.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(local_project.error());
  }
  auto local_target = make_local_target(target);
  if (local_target.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(local_target.error());
  }

  const AssemblyConstraintTargetResolver local_resolver;
  auto resolved = local_resolver.resolve(local_project.value(), local_target.value());
  if (resolved.has_error()) {
    return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::failure(resolved.error());
  }
  const auto transforms =
      target_transform_chain(resolved.value().component_transform, occurrence.value().occurrence);
  return Result<AssemblyHierarchyPlanarConstraintTargetDescriptor>::success(
      AssemblyHierarchyPlanarConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          resolved.value().component_instance, resolved.value().referenced_part_document,
          resolved.value().source_feature, resolved.value().face, target.semantic_reference(),
          evaluate_plane(transforms, resolved.value().local_plane)});
}

Result<AssemblyHierarchyAxisConstraintTargetDescriptor>
AssemblyHierarchyConstraintTargetResolver::resolve_axis(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(occurrence.error());
  }
  auto local_project = make_local_target_view(project, *occurrence.value().assembly);
  if (local_project.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(local_project.error());
  }
  auto local_target = make_local_target(target);
  if (local_target.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(local_target.error());
  }

  const AssemblyConstraintTargetResolver local_resolver;
  auto resolved = local_resolver.resolve_axis(local_project.value(), local_target.value());
  if (resolved.has_error()) {
    return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::failure(resolved.error());
  }
  const auto transforms =
      target_transform_chain(resolved.value().component_transform, occurrence.value().occurrence);
  return Result<AssemblyHierarchyAxisConstraintTargetDescriptor>::success(
      AssemblyHierarchyAxisConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          resolved.value().component_instance, resolved.value().referenced_part_document,
          resolved.value().source_feature, resolved.value().source_profile, resolved.value().axis,
          target.semantic_reference(), evaluate_axis(transforms, resolved.value().local_axis)});
}

Result<AssemblyHierarchyInsertConstraintTargetDescriptor>
AssemblyHierarchyConstraintTargetResolver::resolve_insert(
    const Project& project, const AssemblyHierarchyConstraintTarget& target) const {
  auto occurrence = resolve_occurrence(project, target.occurrence_path());
  if (occurrence.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(occurrence.error());
  }
  auto local_project = make_local_target_view(project, *occurrence.value().assembly);
  if (local_project.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(local_project.error());
  }
  auto local_target = make_local_target(target);
  if (local_target.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(local_target.error());
  }

  const AssemblyConstraintTargetResolver local_resolver;
  auto resolved = local_resolver.resolve_insert(local_project.value(), local_target.value());
  if (resolved.has_error()) {
    return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::failure(resolved.error());
  }
  const auto transforms =
      target_transform_chain(resolved.value().component_transform, occurrence.value().occurrence);
  return Result<AssemblyHierarchyInsertConstraintTargetDescriptor>::success(
      AssemblyHierarchyInsertConstraintTargetDescriptor{
          target.occurrence_path(), occurrence.value().occurrence.assembly_document,
          resolved.value().component_instance, resolved.value().referenced_part_document,
          resolved.value().source_feature, resolved.value().source_profile, resolved.value().axis,
          resolved.value().seating_plane, target.semantic_reference(),
          evaluate_axis(transforms, resolved.value().local_axis),
          evaluate_plane(transforms, resolved.value().local_seating_plane)});
}

Result<AssemblyHierarchyConstraintEquationDescriptor>
AssemblyHierarchyConstraintEquationBuilder::build(
    const Project& project, const AssemblyHierarchyConstraintQuery& query) const {
  const AssemblyHierarchyConstraintTargetResolver resolver;

  if (query.type() == AssemblyConstraintType::Concentric) {
    auto target_a = resolver.resolve_axis(project, query.target_a());
    if (target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
    }
    auto target_b = resolver.resolve_axis(project, query.target_b());
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
    auto target_a = resolver.resolve_insert(project, query.target_a());
    if (target_a.has_error()) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
    }
    auto target_b = resolver.resolve_insert(project, query.target_b());
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

  auto target_a = resolver.resolve_planar(project, query.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolver.resolve_planar(project, query.target_b());
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
            PlanarMateResidualDescriptor{add(target_a.value().plane.normal,
                                             target_b.value().plane.normal),
                                         signed_separation_mm}});
  }

  if (query.type() == AssemblyConstraintType::Angle) {
    if (!query.angle().has_value() || query.angle()->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyHierarchyConstraintEquationDescriptor>::failure(validation_error(
          query.id().value(),
          "cross-hierarchy planar Angle equation requires an angle value in degrees"));
    }
    const double target_angle_deg = query.angle()->degrees();
    const double target_cosine =
        std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
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
