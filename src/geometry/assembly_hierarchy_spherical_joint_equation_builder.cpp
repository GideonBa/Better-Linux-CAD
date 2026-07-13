#include "blcad/geometry/assembly_hierarchy_spherical_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

namespace blcad::geometry {

Result<AssemblyHierarchySphericalJointEquationDescriptor>
AssemblyHierarchySphericalJointEquationBuilder::build(const Project& project,
                                                      const AssemblyHierarchyJoint& joint) const {
  if (joint.state() != AssemblyJointState::Active || joint.type() != AssemblyJointType::Spherical) {
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(Error::validation(
        joint.id().value(), "hierarchy Spherical equation requires an active Spherical joint"));
  }
  auto query_a = AssemblyHierarchyConstraintTarget::create(joint.target_a().occurrence_path(),
                                                           joint.target_a().component_instance(),
                                                           joint.target_a().semantic_reference());
  if (query_a.has_error())
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(query_a.error());
  auto query_b = AssemblyHierarchyConstraintTarget::create(joint.target_b().occurrence_path(),
                                                           joint.target_b().component_instance(),
                                                           joint.target_b().semantic_reference());
  if (query_b.has_error())
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(query_b.error());
  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, query_a.value());
  if (a.has_error())
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, query_b.value());
  if (b.has_error())
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(b.error());
  const AssemblySphericalJointEquationBuilder builder;
  auto equation = builder.build(joint.id(), joint.type(), a.value(), b.value());
  if (equation.has_error())
    return Result<AssemblyHierarchySphericalJointEquationDescriptor>::failure(equation.error());
  const auto& value = equation.value();
  return Result<AssemblyHierarchySphericalJointEquationDescriptor>::success(
      {value.joint, value.compatibility, value.target_a, value.target_b, value.residual});
}

} // namespace blcad::geometry
