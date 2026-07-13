#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"

#include "assembly_prismatic_joint_residual.hpp"
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

Result<AssemblyPrismaticJointEquationDescriptor>
AssemblyPrismaticJointEquationBuilder::build(const Project& project, const AssemblyJoint& joint,
                                             const Quantity& requested) const {
  if (joint.state() != AssemblyJointState::Active)
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "prismatic equation construction requires an active joint"));
  const AssemblyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, joint.target_a());
  if (a.has_error())
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, joint.target_b());
  if (b.has_error())
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(b.error());
  return build(joint.id(), joint.type(), a.value(), b.value(), requested);
}

Result<AssemblyPrismaticJointEquationDescriptor> AssemblyPrismaticJointEquationBuilder::build(
    AssemblyJointId joint, AssemblyJointType type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b, const Quantity& requested) const {
  if (type != AssemblyJointType::Prismatic)
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(validation_error(
        joint.value(), "prismatic equation construction requires a Prismatic joint"));
  if (requested.kind() != QuantityKind::LinearDisplacementMm)
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(
        validation_error(joint.value(), "prismatic drive coordinate must use linear millimeters"));
  const AssemblyJointTargetCompatibilityResolver resolver;
  auto compatibility = resolver.resolve(type, target_a, target_b);
  if (compatibility.has_error())
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(compatibility.error());
  auto projected_a = project_frame(target_a);
  if (projected_a.has_error())
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(projected_a.error());
  auto projected_b = project_frame(target_b);
  if (projected_b.has_error())
    return Result<AssemblyPrismaticJointEquationDescriptor>::failure(projected_b.error());
  const auto frame_a = equation_frame(target_a, projected_a.value());
  const auto frame_b = equation_frame(target_b, projected_b.value());
  return Result<AssemblyPrismaticJointEquationDescriptor>::success(
      {joint,
       compatibility.value(),
       {target_a, compatibility.value().target_a_capability, frame_a},
       {target_b, compatibility.value().target_b_capability, frame_b},
       requested.millimeters(),
       detail::build_prismatic_joint_residual(frame_a, frame_b, requested.millimeters())});
}

} // namespace blcad::geometry
