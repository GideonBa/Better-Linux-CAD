#pragma once

#include "blcad/geometry/assembly_spherical_joint_equation_builder.hpp"

namespace blcad::geometry {

struct AssemblyHierarchySphericalJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblySphericalJointTargetDescriptor target_a;
  AssemblySphericalJointTargetDescriptor target_b;
  SphericalJointResidualDescriptor residual;
};

class AssemblyHierarchySphericalJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchySphericalJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint) const;
};

} // namespace blcad::geometry
