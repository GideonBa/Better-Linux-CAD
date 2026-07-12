#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include "assembly_revolute_joint_residual.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
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

  return Result<AssemblyRevoluteJointEquationDescriptor>::success(
      AssemblyRevoluteJointEquationDescriptor{
          joint.id(), target_a.value(), target_b.value(), requested_coordinate.degrees(),
          detail::build_revolute_joint_residual(
              target_a.value().axis, target_a.value().seating_plane, target_b.value().axis,
              target_b.value().seating_plane, requested_coordinate.degrees())});
}

} // namespace blcad::geometry
