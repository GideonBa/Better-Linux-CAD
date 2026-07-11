#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"

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

[[nodiscard]] Result<AssemblySpaceAxisConstraintTargetDescriptor>
resolve_assembly_space_axis_target(const Project& project,
                                   const AssemblyConstraintTarget& target) {
  const AssemblyConstraintTargetResolver target_resolver;
  auto resolved_target = target_resolver.resolve_axis(project, target);
  if (resolved_target.has_error()) {
    return Result<AssemblySpaceAxisConstraintTargetDescriptor>::failure(resolved_target.error());
  }

  const AssemblyTransformEvaluator transform_evaluator;
  const ResolvedAssemblyAxisConstraintTarget& resolved = resolved_target.value();
  return Result<AssemblySpaceAxisConstraintTargetDescriptor>::success(
      AssemblySpaceAxisConstraintTargetDescriptor{
          resolved.component_instance, target.semantic_reference(),
          transform_evaluator.evaluate_axis(resolved.component_transform, resolved.local_axis)});
}

} // namespace

Result<AssemblyConcentricConstraintEquationDescriptor>
AssemblyConcentricConstraintEquationBuilder::build(const Project& project,
                                                    const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "Concentric equation construction requires an active constraint"));
  }

  if (constraint.type() != AssemblyConstraintType::Concentric) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "Concentric equation construction requires a Concentric constraint"));
  }

  auto target_a = resolve_assembly_space_axis_target(project, constraint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(target_a.error());
  }

  auto target_b = resolve_assembly_space_axis_target(project, constraint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(target_b.error());
  }

  const AssemblySpaceAxisDescriptor& axis_a = target_a.value().axis;
  const AssemblySpaceAxisDescriptor& axis_b = target_b.value().axis;
  const Vector3 origin_delta = difference(axis_b.origin, axis_a.origin);

  return Result<AssemblyConcentricConstraintEquationDescriptor>::success(
      AssemblyConcentricConstraintEquationDescriptor{
          constraint.id(), target_a.value(), target_b.value(),
          ConcentricResidualDescriptor{cross(axis_a.direction, axis_b.direction),
                                       cross(origin_delta, axis_a.direction)}});
}

} // namespace blcad::geometry
