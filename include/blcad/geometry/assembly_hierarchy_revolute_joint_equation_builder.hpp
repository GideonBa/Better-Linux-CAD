#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

namespace blcad::geometry {

struct AssemblyHierarchyRevoluteJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyHierarchyInsertConstraintTargetDescriptor target_a;
  AssemblyHierarchyInsertConstraintTargetDescriptor target_b;
  double requested_coordinate_deg = 0.0;
  RevoluteJointResidualDescriptor residual;

  friend bool operator==(const AssemblyHierarchyRevoluteJointEquationDescriptor&,
                         const AssemblyHierarchyRevoluteJointEquationDescriptor&) = default;
};

// Resolves exact occurrence-qualified `.seat` endpoints into root-assembly space
// and applies the same directed axis/seating/signed-twist Revolute residual
// mathematics as the local joint seed.
class AssemblyHierarchyRevoluteJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyHierarchyRevoluteJointEquationDescriptor>
  build(const Project& project, const AssemblyHierarchyJoint& joint,
        const Quantity& requested_coordinate) const;
};

} // namespace blcad::geometry
