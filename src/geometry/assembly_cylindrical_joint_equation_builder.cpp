#include "blcad/geometry/assembly_cylindrical_joint_equation_builder.hpp"

#include "assembly_cylindrical_joint_residual.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include <utility>

namespace blcad::geometry {
namespace {
Error validation_error(std::string id, std::string message) {
  return Error::validation(std::move(id), std::move(message));
}
AssemblyFrameTargetDescriptor equation_frame(const AssemblyResolvedGeometricTarget& target,
                                             const AssemblyFrameTargetDescriptor& frame) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly)
    return frame;
  const AssemblyHierarchyTransformEvaluator evaluator;
  return {evaluator.evaluate_point(target.transforms_inner_to_outer, frame.origin),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.x_axis),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.y_axis),
          evaluator.evaluate_vector(target.transforms_inner_to_outer, frame.z_axis)};
}
} // namespace

Result<AssemblyCylindricalJointEquationDescriptor>
AssemblyCylindricalJointEquationBuilder::build(const Project& project, const AssemblyJoint& joint,
                                               const Quantity& translation,
                                               const Quantity& rotation) const {
  if (joint.state() != AssemblyJointState::Active)
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "cylindrical equation construction requires an active joint"));
  const AssemblyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, joint.target_a());
  if (a.has_error())
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, joint.target_b());
  if (b.has_error())
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(b.error());
  return build(joint.id(), joint.type(), a.value(), b.value(), translation, rotation);
}

Result<AssemblyCylindricalJointEquationDescriptor> AssemblyCylindricalJointEquationBuilder::build(
    AssemblyJointId joint, AssemblyJointType type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b, const Quantity& translation,
    const Quantity& rotation) const {
  if (type != AssemblyJointType::Cylindrical ||
      translation.kind() != QuantityKind::LinearDisplacementMm ||
      rotation.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(validation_error(
        joint.value(), "cylindrical equation requires linear translation and angular rotation"));
  }
  const AssemblyJointTargetCompatibilityResolver resolver;
  auto compatibility = resolver.resolve(type, target_a, target_b);
  if (compatibility.has_error())
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(compatibility.error());
  auto projected_a = project_frame(target_a);
  if (projected_a.has_error())
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(projected_a.error());
  auto projected_b = project_frame(target_b);
  if (projected_b.has_error())
    return Result<AssemblyCylindricalJointEquationDescriptor>::failure(projected_b.error());
  const auto frame_a = equation_frame(target_a, projected_a.value());
  const auto frame_b = equation_frame(target_b, projected_b.value());
  return Result<AssemblyCylindricalJointEquationDescriptor>::success(
      {joint,
       compatibility.value(),
       {target_a, compatibility.value().target_a_capability, frame_a},
       {target_b, compatibility.value().target_b_capability, frame_b},
       translation.millimeters(),
       rotation.degrees(),
       detail::build_cylindrical_joint_residual(frame_a, frame_b, translation.millimeters(),
                                                rotation.degrees())});
}

} // namespace blcad::geometry
