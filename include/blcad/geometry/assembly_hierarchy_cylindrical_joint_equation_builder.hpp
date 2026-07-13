#pragma once

#include "blcad/geometry/assembly_cylindrical_joint_equation_builder.hpp"

namespace blcad::geometry {

struct AssemblyHierarchyCylindricalJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyCylindricalJointTargetDescriptor target_a;
  AssemblyCylindricalJointTargetDescriptor target_b;
  double requested_translation_mm = 0.0;
  double requested_rotation_deg = 0.0;
  CylindricalJointResidualDescriptor residual;
};

class AssemblyHierarchyCylindricalJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyCylindricalJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint,
        const Quantity& requested_translation, const Quantity& requested_rotation) const;
};

} // namespace blcad::geometry
