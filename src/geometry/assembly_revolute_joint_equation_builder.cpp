#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <cmath>
#include <numbers>
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

[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
  return degrees * (std::numbers::pi_v<double> / 180.0);
}

[[nodiscard]] Result<AssemblySpaceInsertConstraintTargetDescriptor>
resolve_assembly_space_joint_target(const Project& project, const AssemblyConstraintTarget& target) {
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

Result<AssemblyRevoluteJointEquationDescriptor>
AssemblyRevoluteJointEquationBuilder::build(const Project& project, const AssemblyJoint& joint,
                                             const Quantity& requested_coordinate) const {
  if (joint.state() != AssemblyJointState::Active) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "revolute joint equation construction requires an active joint"));
  }
  if (joint.type() != AssemblyJointType::Revolute) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "revolute joint equation construction requires a Revolute joint"));
  }
  if (requested_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "revolute joint drive coordinate must use degrees"));
  }

  auto target_a = resolve_assembly_space_joint_target(project, joint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolve_assembly_space_joint_target(project, joint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(target_b.error());
  }

  const AssemblySpaceAxisDescriptor& axis_a = target_a.value().axis;
  const AssemblySpaceAxisDescriptor& axis_b = target_b.value().axis;
  const AssemblySpacePlanarDescriptor& seat_a = target_a.value().seating_plane;
  const AssemblySpacePlanarDescriptor& seat_b = target_b.value().seating_plane;
  const Vector3 axis_origin_delta = difference(axis_b.origin, axis_a.origin);
  const Vector3 seat_origin_delta = difference(seat_b.origin, seat_a.origin);

  const double reference_cosine = dot(seat_a.x_axis, seat_b.x_axis);
  const double reference_sine = dot(axis_a.direction, cross(seat_a.x_axis, seat_b.x_axis));
  const double target_rad = degrees_to_radians(requested_coordinate.degrees());
  const double target_cosine = std::cos(target_rad);
  const double target_sine = std::sin(target_rad);

  return Result<AssemblyRevoluteJointEquationDescriptor>::success(
      AssemblyRevoluteJointEquationDescriptor{
          joint.id(), target_a.value(), target_b.value(), requested_coordinate.degrees(),
          RevoluteJointResidualDescriptor{
              cross(axis_a.direction, axis_b.direction),
              cross(axis_origin_delta, axis_a.direction),
              dot(seat_origin_delta, seat_a.normal),
              reference_sine * target_cosine - reference_cosine * target_sine,
              reference_cosine * target_cosine + reference_sine * target_sine - 1.0}});
}

} // namespace blcad::geometry
