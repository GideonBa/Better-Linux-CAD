#pragma once

#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"

namespace blcad::geometry {

struct AssemblyHierarchyPrismaticJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyPrismaticJointTargetDescriptor target_a;
  AssemblyPrismaticJointTargetDescriptor target_b;
  double requested_translation_mm = 0.0;
  PrismaticJointResidualDescriptor residual;
};

class AssemblyHierarchyPrismaticJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyPrismaticJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint,
        const Quantity& requested_translation) const;
};

} // namespace blcad::geometry
