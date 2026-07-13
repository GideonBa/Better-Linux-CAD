#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"

namespace blcad::geometry {

struct SphericalJointResidualDescriptor {
  Vector3 center_offset_mm;

  friend bool operator==(const SphericalJointResidualDescriptor&,
                         const SphericalJointResidualDescriptor&) = default;
};

struct AssemblySphericalJointTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Point;
  Point3 equation_space_point;

  friend bool operator==(const AssemblySphericalJointTargetDescriptor&,
                         const AssemblySphericalJointTargetDescriptor&) = default;
};

struct AssemblySphericalJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblySphericalJointTargetDescriptor target_a;
  AssemblySphericalJointTargetDescriptor target_b;
  SphericalJointResidualDescriptor residual;
};

class AssemblySphericalJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblySphericalJointEquationDescriptor>
  build(const Project& project, const AssemblyJoint& joint) const;

  [[nodiscard]] Result<AssemblySphericalJointEquationDescriptor>
  build(AssemblyJointId joint, AssemblyJointType type,
        const AssemblyResolvedGeometricTarget& target_a,
        const AssemblyResolvedGeometricTarget& target_b) const;
};

} // namespace blcad::geometry
