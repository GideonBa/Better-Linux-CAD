#pragma once

#include "blcad/geometry/assembly_planar_joint_equation_builder.hpp"

namespace blcad::geometry {

struct AssemblyHierarchyPlanarJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyPlanarJointTargetDescriptor target_a;
  AssemblyPlanarJointTargetDescriptor target_b;
  double requested_translation_u_mm = 0.0;
  double requested_translation_v_mm = 0.0;
  double requested_rotation_normal_deg = 0.0;
  PlanarJointResidualDescriptor residual;
};

class AssemblyHierarchyPlanarJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyPlanarJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint, const Quantity& translation_u,
        const Quantity& translation_v, const Quantity& rotation_normal) const;
};

} // namespace blcad::geometry
