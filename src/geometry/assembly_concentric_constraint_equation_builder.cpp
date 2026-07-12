#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace blcad::geometry {
namespace {

struct ResolvedAssemblySpaceGeometricTarget {
  ComponentInstanceId component_instance;
  std::string semantic_reference;
  AssemblyResolvedGeometricTarget target;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] bool legacy_feature_target(std::string_view semantic_reference) noexcept {
  return !semantic_reference.starts_with("ref:") && !semantic_reference.starts_with("topo:");
}

[[nodiscard]] Result<ResolvedAssemblySpaceGeometricTarget>
resolve_assembly_space_geometric_target(const Project& project,
                                        const AssemblyConstraintTarget& target) {
  const AssemblyConstraintTargetResolver target_resolver;
  auto resolved_target = target_resolver.resolve_geometric(project, target);
  if (resolved_target.has_error()) {
    if (legacy_feature_target(target.semantic_reference())) {
      auto legacy_error = target_resolver.resolve_axis(project, target);
      if (legacy_error.has_error()) {
        return Result<ResolvedAssemblySpaceGeometricTarget>::failure(legacy_error.error());
      }
    }
    return Result<ResolvedAssemblySpaceGeometricTarget>::failure(resolved_target.error());
  }
  return Result<ResolvedAssemblySpaceGeometricTarget>::success(
      ResolvedAssemblySpaceGeometricTarget{target.component_instance(), target.semantic_reference(),
                                           std::move(resolved_target.value())});
}

[[nodiscard]] Result<AssemblySpaceAxisConstraintTargetDescriptor>
project_assembly_space_axis_target(const ResolvedAssemblySpaceGeometricTarget& resolved) {
  auto axis = project_axis(resolved.target);
  if (axis.has_error()) {
    return Result<AssemblySpaceAxisConstraintTargetDescriptor>::failure(axis.error());
  }
  const AssemblyTransformEvaluator transform_evaluator;
  return Result<AssemblySpaceAxisConstraintTargetDescriptor>::success(
      AssemblySpaceAxisConstraintTargetDescriptor{
          resolved.component_instance, resolved.semantic_reference,
          transform_evaluator.evaluate_axis(
              resolved.target.transforms_inner_to_outer.front(),
              ComponentLocalAxisDescriptor{axis.value().origin, axis.value().direction})});
}

} // namespace

Result<AssemblyConcentricConstraintEquationDescriptor>
AssemblyConcentricConstraintEquationBuilder::build(const Project& project,
                                                   const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(), "Concentric equation construction requires an active constraint"));
  }

  if (constraint.type() != AssemblyConstraintType::Concentric) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(
        validation_error(constraint.id().value(),
                         "Concentric equation construction requires a Concentric constraint"));
  }

  auto geometric_target_a = resolve_assembly_space_geometric_target(project, constraint.target_a());
  if (geometric_target_a.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(
        geometric_target_a.error());
  }

  auto geometric_target_b = resolve_assembly_space_geometric_target(project, constraint.target_b());
  if (geometric_target_b.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(
        geometric_target_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(
      constraint.type(), geometric_target_a.value().target, geometric_target_b.value().target);
  if (compatibility.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(compatibility.error());
  }

  auto target_a = project_assembly_space_axis_target(geometric_target_a.value());
  if (target_a.has_error()) {
    return Result<AssemblyConcentricConstraintEquationDescriptor>::failure(target_a.error());
  }

  auto target_b = project_assembly_space_axis_target(geometric_target_b.value());
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
