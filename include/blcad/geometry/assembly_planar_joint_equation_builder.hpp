#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"

namespace blcad::geometry {

// Canonical row order: normal alignment (3), normal separation (1),
// U/V translation (2), then signed normal twist sine/cosine (2).
struct PlanarJointResidualDescriptor {
  Vector3 normal_alignment;
  double normal_separation_mm = 0.0;
  double translation_u_error_mm = 0.0;
  double translation_v_error_mm = 0.0;
  double rotation_normal_sine = 0.0;
  double rotation_normal_cosine = 0.0;

  friend bool operator==(const PlanarJointResidualDescriptor&,
                         const PlanarJointResidualDescriptor&) = default;
};

struct AssemblyPlanarJointTargetDescriptor {
  AssemblyResolvedGeometricTarget target;
  AssemblyGeometricTargetCapability selected_capability = AssemblyGeometricTargetCapability::Frame;
  AssemblyFrameTargetDescriptor equation_space_frame;

  friend bool operator==(const AssemblyPlanarJointTargetDescriptor&,
                         const AssemblyPlanarJointTargetDescriptor&) = default;
};

struct AssemblyPlanarJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblyJointTargetCompatibility compatibility;
  AssemblyPlanarJointTargetDescriptor target_a;
  AssemblyPlanarJointTargetDescriptor target_b;
  double requested_translation_u_mm = 0.0;
  double requested_translation_v_mm = 0.0;
  double requested_rotation_normal_deg = 0.0;
  PlanarJointResidualDescriptor residual;
};

class AssemblyPlanarJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyPlanarJointEquationDescriptor>
  build(const Project& project, const AssemblyJoint& joint, const Quantity& translation_u,
        const Quantity& translation_v, const Quantity& rotation_normal) const;

  [[nodiscard]] Result<AssemblyPlanarJointEquationDescriptor>
  build(AssemblyJointId joint, AssemblyJointType type,
        const AssemblyResolvedGeometricTarget& target_a,
        const AssemblyResolvedGeometricTarget& target_b, const Quantity& translation_u,
        const Quantity& translation_v, const Quantity& rotation_normal) const;
};

} // namespace blcad::geometry
