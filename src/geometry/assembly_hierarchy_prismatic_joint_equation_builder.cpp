#include "blcad/geometry/assembly_hierarchy_prismatic_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

namespace blcad::geometry {

Result<AssemblyHierarchyPrismaticJointEquationDescriptor>
AssemblyHierarchyPrismaticJointEquationBuilder::build(const Project& project,
                                                      const AssemblyHierarchyJoint& joint,
                                                      const Quantity& requested) const {
  if (joint.state() != AssemblyJointState::Active || joint.type() != AssemblyJointType::Prismatic ||
      requested.kind() != QuantityKind::LinearDisplacementMm) {
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(Error::validation(
        joint.id().value(),
        "hierarchy Prismatic equation requires an active Prismatic joint and linear millimeters"));
  }
  auto query_a = AssemblyHierarchyConstraintTarget::create(joint.target_a().occurrence_path(),
                                                           joint.target_a().component_instance(),
                                                           joint.target_a().semantic_reference());
  if (query_a.has_error())
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(query_a.error());
  auto query_b = AssemblyHierarchyConstraintTarget::create(joint.target_b().occurrence_path(),
                                                           joint.target_b().component_instance(),
                                                           joint.target_b().semantic_reference());
  if (query_b.has_error())
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(query_b.error());
  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, query_a.value());
  if (a.has_error())
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, query_b.value());
  if (b.has_error())
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(b.error());
  const AssemblyPrismaticJointEquationBuilder builder;
  auto equation = builder.build(joint.id(), joint.type(), a.value(), b.value(), requested);
  if (equation.has_error())
    return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::failure(equation.error());
  const auto& value = equation.value();
  return Result<AssemblyHierarchyPrismaticJointEquationDescriptor>::success(
      {value.joint, value.compatibility, value.target_a, value.target_b,
       value.requested_translation_mm, value.residual});
}

} // namespace blcad::geometry
