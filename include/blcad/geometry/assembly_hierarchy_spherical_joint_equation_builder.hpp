#pragma once

#include "blcad/geometry/assembly_spherical_joint_equation_builder.hpp"

namespace blcad::geometry {

class StepImportTopologyContext;

struct AssemblyHierarchySphericalJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblySphericalJointTargetDescriptor target_a;
  AssemblySphericalJointTargetDescriptor target_b;
  SphericalJointResidualDescriptor residual;
};

class AssemblyHierarchySphericalJointEquationBuilder {
public:
  AssemblyHierarchySphericalJointEquationBuilder() = default;
  explicit AssemblyHierarchySphericalJointEquationBuilder(StepImportTopologyContext* import_context)
      : import_context_(import_context) {}
  [[nodiscard]] Result<AssemblyHierarchySphericalJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint) const;

private:
  StepImportTopologyContext* import_context_{};
};

} // namespace blcad::geometry
