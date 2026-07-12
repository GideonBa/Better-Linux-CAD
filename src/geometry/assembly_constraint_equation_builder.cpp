#include "blcad/geometry/assembly_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <cmath>
#include <numbers>
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
      auto legacy_error = target_resolver.resolve(project, target);
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

[[nodiscard]] Result<AssemblySpaceConstraintTargetDescriptor>
project_assembly_space_planar_target(const ResolvedAssemblySpaceGeometricTarget& resolved) {
  auto plane = project_plane(resolved.target);
  if (plane.has_error()) {
    return Result<AssemblySpaceConstraintTargetDescriptor>::failure(plane.error());
  }
  const AssemblyTransformEvaluator transform_evaluator;
  return Result<AssemblySpaceConstraintTargetDescriptor>::success(
      AssemblySpaceConstraintTargetDescriptor{
          resolved.component_instance, resolved.semantic_reference,
          transform_evaluator.evaluate_plane(
              resolved.target.transforms_inner_to_outer.front(),
              ComponentLocalPlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                             plane.value().y_axis, plane.value().normal})});
}

[[nodiscard]] bool selected_plane_pair(const AssemblyTargetCompatibility& compatibility) noexcept {
  return compatibility.target_a_capability == AssemblyGeometricTargetCapability::Plane &&
         compatibility.target_b_capability == AssemblyGeometricTargetCapability::Plane;
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
    return Result<AssemblyConstraintEquationDescriptor>::failure(
        validation_error(constraint.id().value(),
                         "concentric equation construction requires semantic axis target support"));
  }
  if (constraint.type() == AssemblyConstraintType::Insert) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "Insert equation construction requires dedicated composite target support"));
  }

  auto geometric_target_a = resolve_assembly_space_geometric_target(project, constraint.target_a());
  if (geometric_target_a.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(geometric_target_a.error());
  }

  auto geometric_target_b = resolve_assembly_space_geometric_target(project, constraint.target_b());
  if (geometric_target_b.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(geometric_target_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(
      constraint.type(), geometric_target_a.value().target, geometric_target_b.value().target);
  if (compatibility.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(compatibility.error());
  }
  if (!selected_plane_pair(compatibility.value())) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "planar equation construction currently requires Plane/Plane target compatibility"));
  }

  auto target_a = project_assembly_space_planar_target(geometric_target_a.value());
  if (target_a.has_error()) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(target_a.error());
  }

  auto target_b = project_assembly_space_planar_target(geometric_target_b.value());
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

  if (constraint.type() == AssemblyConstraintType::Angle) {
    if (!constraint.angle().has_value() || constraint.angle()->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyConstraintEquationDescriptor>::failure(validation_error(
          constraint.id().value(),
          "planar Angle equation construction requires an angle value in degrees"));
    }

    const double target_angle_deg = constraint.angle()->degrees();
    const double target_cosine = std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
    const double normal_dot = dot(plane_a.normal, plane_b.normal);
    return Result<AssemblyConstraintEquationDescriptor>::success(
        AssemblyConstraintEquationDescriptor{
            constraint.id(), constraint.type(), target_a.value(), target_b.value(),
            PlanarAngleResidualDescriptor{target_angle_deg, normal_dot,
                                          normal_dot - target_cosine}});
  }

  if (!constraint.distance().has_value() ||
      constraint.distance()->kind() != QuantityKind::LengthMm) {
    return Result<AssemblyConstraintEquationDescriptor>::failure(
        validation_error(constraint.id().value(),
                         "planar Distance equation construction requires a length distance value"));
  }

  const double target_distance_mm = constraint.distance()->millimeters();
  return Result<AssemblyConstraintEquationDescriptor>::success(AssemblyConstraintEquationDescriptor{
      constraint.id(), constraint.type(), target_a.value(), target_b.value(),
      PlanarDistanceResidualDescriptor{cross(plane_a.normal, plane_b.normal), target_distance_mm,
                                       signed_separation_mm,
                                       signed_separation_mm - target_distance_mm}});
}

} // namespace blcad::geometry
