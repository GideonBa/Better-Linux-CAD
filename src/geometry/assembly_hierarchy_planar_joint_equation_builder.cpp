#include "blcad/geometry/assembly_hierarchy_planar_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

namespace blcad::geometry {

Result<AssemblyHierarchyPlanarJointEquationDescriptor>
AssemblyHierarchyPlanarJointEquationBuilder::build(const Project& project,
                                                   const AssemblyHierarchyJoint& joint,
                                                   const Quantity& translation_u,
                                                   const Quantity& translation_v,
                                                   const Quantity& rotation_normal) const {
  if (joint.state() != AssemblyJointState::Active || joint.type() != AssemblyJointType::Planar) {
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(Error::validation(
        joint.id().value(), "hierarchy Planar equation requires an active Planar joint"));
  }
  auto query_a = AssemblyHierarchyConstraintTarget::create(joint.target_a().occurrence_path(),
                                                           joint.target_a().component_instance(),
                                                           joint.target_a().semantic_reference());
  if (query_a.has_error())
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(query_a.error());
  auto query_b = AssemblyHierarchyConstraintTarget::create(joint.target_b().occurrence_path(),
                                                           joint.target_b().component_instance(),
                                                           joint.target_b().semantic_reference());
  if (query_b.has_error())
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(query_b.error());
  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, query_a.value());
  if (a.has_error())
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, query_b.value());
  if (b.has_error())
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(b.error());
  const AssemblyPlanarJointEquationBuilder builder;
  auto equation = builder.build(joint.id(), joint.type(), a.value(), b.value(), translation_u,
                                translation_v, rotation_normal);
  if (equation.has_error())
    return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::failure(equation.error());
  const auto& value = equation.value();
  return Result<AssemblyHierarchyPlanarJointEquationDescriptor>::success(
      {value.joint, value.compatibility, value.target_a, value.target_b,
       value.requested_translation_u_mm, value.requested_translation_v_mm,
       value.requested_rotation_normal_deg, value.residual});
}

} // namespace blcad::geometry
