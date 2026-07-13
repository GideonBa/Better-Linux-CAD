#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"

namespace blcad::geometry {

// Canonical row order: z-direction alignment (3), transverse offset (3),
// reference-frame sine/cosine alignment (2), and signed translation (1).
struct PrismaticJointResidualDescriptor {
  Vector3 direction_alignment;
  Vector3 transverse_offset_mm;
  double orientation_alignment_sine = 0.0;
  double orientation_alignment_cosine = 0.0;
  double translation_error_mm = 0.0;

  friend bool operator==(const PrismaticJointResidualDescriptor&,
                         const PrismaticJointResidualDescriptor&) = default;
};

struct AssemblyPrismaticJointTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Frame;
  AssemblyFrameTargetDescriptor equation_space_frame;

  friend bool operator==(const AssemblyPrismaticJointTargetDescriptor&,
                         const AssemblyPrismaticJointTargetDescriptor&) = default;
};

struct AssemblyPrismaticJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyPrismaticJointTargetDescriptor target_a;
  AssemblyPrismaticJointTargetDescriptor target_b;
  double requested_translation_mm = 0.0;
  PrismaticJointResidualDescriptor residual;

  friend bool operator==(const AssemblyPrismaticJointEquationDescriptor&,
                         const AssemblyPrismaticJointEquationDescriptor&) = default;
};

class AssemblyPrismaticJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyPrismaticJointEquationDescriptor>
  build(const Project& project, const AssemblyJoint& joint,
        const Quantity& requested_translation) const;

  [[nodiscard]] Result<AssemblyPrismaticJointEquationDescriptor>
  build(AssemblyJointId joint, AssemblyJointType type,
        const AssemblyResolvedGeometricTarget& target_a,
        const AssemblyResolvedGeometricTarget& target_b,
        const Quantity& requested_translation) const;
};

} // namespace blcad::geometry
