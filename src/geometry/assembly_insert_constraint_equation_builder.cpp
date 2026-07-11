#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y,
                 lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Result<AssemblySpaceInsertConstraintTargetDescriptor>
resolve_assembly_space_insert_target(const Project& project,
                                     const AssemblyConstraintTarget& target) {
  const AssemblyConstraintTargetResolver target_resolver;
  auto resolved_target = target_resolver.resolve_insert(project, target);
  if (resolved_target.has_error()) {
    return Result<AssemblySpaceInsertConstraintTargetDescriptor>::failure(resolved_target.error());
  }

  const AssemblyTransformEvaluator transform_evaluator;
  const ResolvedAssemblyInsertConstraintTarget& resolved = resolved_target.value();
  return Result<AssemblySpaceInsertConstraintTargetDescriptor>::success(
      AssemblySpaceInsertConstraintTargetDescriptor{
          resolved.component_instance,
          target.semantic_reference(),
          resolved.source_feature,
          resolved.source_profile,
          transform_evaluator.evaluate_axis(resolved.component_transform, resolved.local_axis),
          transform_evaluator.evaluate_plane(resolved.component_transform,
                                             resolved.local_seating_plane)});
}

} // namespace

Result<AssemblyInsertConstraintEquationDescriptor>
AssemblyInsertConstraintEquationBuilder::build(const Project& project,
                                                const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyInsertConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(), "Insert equation construction requires an active constraint"));
  }

  if (constraint.type() != AssemblyConstraintType::Insert) {
    return Result<AssemblyInsertConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(), "Insert equation construction requires an Insert constraint"));
  }

  auto target_a = resolve_assembly_space_insert_target(project, constraint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyInsertConstraintEquationDescriptor>::failure(target_a.error());
  }

  auto target_b = resolve_assembly_space_insert_target(project, constraint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyInsertConstraintEquationDescriptor>::failure(target_b.error());
  }

  const AssemblySpaceAxisDescriptor& axis_a = target_a.value().axis;
  const AssemblySpaceAxisDescriptor& axis_b = target_b.value().axis;
  const AssemblySpacePlanarDescriptor& seat_a = target_a.value().seating_plane;
  const AssemblySpacePlanarDescriptor& seat_b = target_b.value().seating_plane;
  const Vector3 axis_origin_delta = difference(axis_b.origin, axis_a.origin);
  const Vector3 seat_origin_delta = difference(seat_b.origin, seat_a.origin);

  return Result<AssemblyInsertConstraintEquationDescriptor>::success(
      AssemblyInsertConstraintEquationDescriptor{
          constraint.id(), target_a.value(), target_b.value(),
          InsertResidualDescriptor{cross(axis_a.direction, axis_b.direction),
                                   cross(axis_origin_delta, axis_a.direction),
                                   dot(seat_origin_delta, seat_a.normal)}});
}

} // namespace blcad::geometry
