#include "blcad/geometry/assembly_hierarchy_cylindrical_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

namespace blcad::geometry {

Result<AssemblyHierarchyCylindricalJointEquationDescriptor>
AssemblyHierarchyCylindricalJointEquationBuilder::build(const Project& project,
                                                        const AssemblyHierarchyJoint& joint,
                                                        const Quantity& translation,
                                                        const Quantity& rotation) const {
  if (joint.state() != AssemblyJointState::Active ||
      joint.type() != AssemblyJointType::Cylindrical) {
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(Error::validation(
        joint.id().value(), "hierarchy Cylindrical equation requires an active Cylindrical joint"));
  }
  auto query_a = AssemblyHierarchyConstraintTarget::create(joint.target_a().occurrence_path(),
                                                           joint.target_a().component_instance(),
                                                           joint.target_a().semantic_reference());
  if (query_a.has_error())
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(query_a.error());
  auto query_b = AssemblyHierarchyConstraintTarget::create(joint.target_b().occurrence_path(),
                                                           joint.target_b().component_instance(),
                                                           joint.target_b().semantic_reference());
  if (query_b.has_error())
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(query_b.error());
  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, query_a.value());
  if (a.has_error())
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, query_b.value());
  if (b.has_error())
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(b.error());
  const AssemblyCylindricalJointEquationBuilder builder;
  auto equation =
      builder.build(joint.id(), joint.type(), a.value(), b.value(), translation, rotation);
  if (equation.has_error())
    return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::failure(equation.error());
  const auto& value = equation.value();
  return Result<AssemblyHierarchyCylindricalJointEquationDescriptor>::success(
      {value.joint, value.compatibility, value.target_a, value.target_b,
       value.requested_translation_mm, value.requested_rotation_deg, value.residual});
}

} // namespace blcad::geometry
