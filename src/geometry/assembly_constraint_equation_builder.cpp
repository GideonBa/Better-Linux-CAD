#include "blcad/geometry/assembly_constraint_equation_builder.hpp"

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

[[nodiscard]] Vector3 add(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y,
                 lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] Result<AssemblySpaceConstraintTargetDescriptor>
resolve_assembly_space_target(const Project& project, const AssemblyConstraintTarget& target) {
  const AssemblyConstraintTargetResolver target_resolver;
  auto resolved_target = target_resolver.resolve(project, target);
  if (resolved_target.has_error()) {
    return Result<AssemblySpaceConstraintTargetDescriptor>::failure(resolved_target.error());
  }

  const AssemblyTransformEvaluator transform_evaluator;
  const ResolvedAssemblyConstraintTarget& resolved = resolved_target.value();
  return Result<AssemblySpaceConstraintTargetDescriptor>::success(
      AssemblySpaceConstraintTargetDescriptor{
          resolved.component_instance, target.semantic_reference(),
          transform_evaluator.evaluate_plane(resolved.component_transform, resolved.local_plane)});
}

} // namespace

Result<AssemblyConstraintEquationDescriptor>
AssemblyConstraintEquationBuilder::build(const Project& project,
                                          const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "assembly constraint equation construction requires an active constraint"));
  }

  if (constraint.type() == AssemblyConstraintType::Concentric) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "planar assembly constraint equation builder does not support Concentric constraints"));
  }

  auto target_a = resolve_assembly_space_target(project, constraint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(target_a.error());
  }

  auto target_b = resolve_assembly_space_target(project, constraint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(target_b.error());
  }

  const AssemblySpacePlanarDescriptor& plane_a = target_a.value().plane;
  const AssemblySpacePlanarDescriptor& plane_b = target_b.value().plane;
  const Vector3 origin_delta = difference(plane_b.origin, plane_a.origin);
  const double signed_separation_mm = dot(origin_delta, plane_a.normal);

  if (constraint.type() == AssemblyConstraintType::Mate) {
    return Result<AssemblyConstraintEquationDescriptor>::success(
        AssemblyConstraintEquationDescriptor{
            constraint.id(), constraint.type(), target_a.value(), target_b.value(),
            PlanarMateResidualDescriptor{add(plane_a.normal, plane_b.normal),
                                         signed_separation_mm}});
  }

  if (!constraint.distance().has_value() ||
      constraint.distance()->kind() != QuantityKind::LengthMm) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "planar Distance equation construction requires a length distance value"));
  }

  const double target_distance_mm = constraint.distance()->millimeters();
  return Result<AssemblyConstraintEquationDescriptor>::success(
      AssemblyConstraintEquationDescriptor{
          constraint.id(), constraint.type(), target_a.value(), target_b.value(),
          PlanarDistanceResidualDescriptor{cross(plane_a.normal, plane_b.normal),
                                           target_distance_mm, signed_separation_mm,
                                           signed_separation_mm - target_distance_mm}});
}

} // namespace blcad::geometry
