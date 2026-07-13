#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"

namespace blcad::geometry {

// Canonical row order: direction alignment (3), transverse offset (3),
// translation (1), then signed periodic twist sine/cosine (2).
struct CylindricalJointResidualDescriptor {
  Vector3 direction_alignment;
  Vector3 transverse_offset_mm;
  double translation_error_mm = 0.0;
  double twist_alignment_sine = 0.0;
  double twist_alignment_cosine = 0.0;

  friend bool operator==(const CylindricalJointResidualDescriptor&,
                         const CylindricalJointResidualDescriptor&) = default;
};

struct AssemblyCylindricalJointTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Frame;
  AssemblyFrameTargetDescriptor equation_space_frame;

  friend bool operator==(const AssemblyCylindricalJointTargetDescriptor&,
                         const AssemblyCylindricalJointTargetDescriptor&) = default;
};

struct AssemblyCylindricalJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyCylindricalJointTargetDescriptor target_a;
  AssemblyCylindricalJointTargetDescriptor target_b;
  double requested_translation_mm = 0.0;
  double requested_rotation_deg = 0.0;
  CylindricalJointResidualDescriptor residual;
};

class AssemblyCylindricalJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyCylindricalJointEquationDescriptor>
  build(const Project& project, const AssemblyJoint& joint, const Quantity& requested_translation,
        const Quantity& requested_rotation) const;

  [[nodiscard]] Result<AssemblyCylindricalJointEquationDescriptor>
  build(AssemblyJointId joint, AssemblyJointType type,
        const AssemblyResolvedGeometricTarget& target_a,
        const AssemblyResolvedGeometricTarget& target_b, const Quantity& requested_translation,
        const Quantity& requested_rotation) const;
};

} // namespace blcad::geometry
