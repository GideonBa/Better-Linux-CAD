#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include "assembly_revolute_joint_residual.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] AssemblyFrameTargetDescriptor
evaluate_frame_to_equation_space(const AssemblyResolvedGeometricTarget& target,
                                 const AssemblyFrameTargetDescriptor& frame) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return frame;
  }
  const AssemblyHierarchyTransformEvaluator evaluator;
  return AssemblyFrameTargetDescriptor{
      evaluator.evaluate_point(target.transforms_inner_to_outer, frame.origin),
      evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.x_axis),
      evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.y_axis),
      evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.z_axis)};
}

[[nodiscard]] AssemblySpaceAxisDescriptor frame_axis(const AssemblyFrameTargetDescriptor& frame) {
  return AssemblySpaceAxisDescriptor{frame.origin, frame.z_axis};
}

[[nodiscard]] AssemblySpacePlanarDescriptor
frame_plane(const AssemblyFrameTargetDescriptor& frame) {
  return AssemblySpacePlanarDescriptor{frame.origin, frame.x_axis, frame.y_axis, frame.z_axis};
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
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(
        validation_error(joint.id().value(), "revolute joint drive coordinate must use degrees"));
  }

  const AssemblyConstraintTargetResolver resolver;
  auto target_a = resolver.resolve_geometric(project, joint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolver.resolve_geometric(project, joint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(target_b.error());
  }

  return build(joint.id(), joint.type(), target_a.value(), target_b.value(), requested_coordinate);
}

Result<AssemblyRevoluteJointEquationDescriptor> AssemblyRevoluteJointEquationBuilder::build(
    AssemblyJointId joint, AssemblyJointType type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b, const Quantity& requested_coordinate) const {
  if (type != AssemblyJointType::Revolute) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.value(), "revolute joint equation construction requires a Revolute joint"));
  }
  if (requested_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(
        validation_error(joint.value(), "revolute joint drive coordinate must use degrees"));
  }

  const AssemblyJointTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(type, target_a, target_b);
  if (compatibility.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(compatibility.error());
  }
  auto projected_a = project_frame(target_a);
  if (projected_a.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(projected_a.error());
  }
  auto projected_b = project_frame(target_b);
  if (projected_b.has_error()) {
    return Result<AssemblyRevoluteJointEquationDescriptor>::failure(projected_b.error());
  }

  const AssemblyFrameTargetDescriptor frame_a =
      evaluate_frame_to_equation_space(target_a, projected_a.value());
  const AssemblyFrameTargetDescriptor frame_b =
      evaluate_frame_to_equation_space(target_b, projected_b.value());
  return Result<AssemblyRevoluteJointEquationDescriptor>::success(
      AssemblyRevoluteJointEquationDescriptor{
          joint, compatibility.value(),
          AssemblyRevoluteJointTargetDescriptor{target_a, compatibility.value().target_a_capability,
                                                frame_a},
          AssemblyRevoluteJointTargetDescriptor{target_b, compatibility.value().target_b_capability,
                                                frame_b},
          requested_coordinate.degrees(),
          detail::build_revolute_joint_residual(frame_axis(frame_a), frame_plane(frame_a),
                                                frame_axis(frame_b), frame_plane(frame_b),
                                                requested_coordinate.degrees())});
}

} // namespace blcad::geometry
