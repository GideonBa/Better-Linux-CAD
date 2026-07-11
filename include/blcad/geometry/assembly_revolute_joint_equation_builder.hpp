#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"

namespace blcad::geometry {

// Revolute-drive residuals reuse the semantic axis/seat endpoint family but
// orient the common axis explicitly because target A defines positive rotation:
//
//   direction_alignment          = dA - dB
//   axis_offset_mm               = cross(oB - oA, dA)
//   signed_seating_separation_mm = dot(sB - sA, nA)
//   twist_alignment_sine         = sin(phi - target)
//   twist_alignment_cosine       = cos(phi - target) - 1
//
// The sine/cosine pair avoids an atan2 branch cut while preserving signed,
// periodic revolute coordinates. At a satisfied state the sine row carries the
// regular twist rank and the cosine row is intentionally locally redundant.
struct RevoluteJointResidualDescriptor {
  Vector3 direction_alignment;
  Vector3 axis_offset_mm;
  double signed_seating_separation_mm = 0.0;
  double twist_alignment_sine = 0.0;
  double twist_alignment_cosine = 0.0;

  friend bool operator==(const RevoluteJointResidualDescriptor&,
                         const RevoluteJointResidualDescriptor&) = default;
};

struct AssemblyRevoluteJointEquationDescriptor {
  AssemblyJointId joint;
  AssemblySpaceInsertConstraintTargetDescriptor target_a;
  AssemblySpaceInsertConstraintTargetDescriptor target_b;
  double requested_coordinate_deg = 0.0;
  RevoluteJointResidualDescriptor residual;

  friend bool operator==(const AssemblyRevoluteJointEquationDescriptor&,
                         const AssemblyRevoluteJointEquationDescriptor&) = default;
};

class AssemblyRevoluteJointEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyRevoluteJointEquationDescriptor>
  build(const Project& project, const AssemblyJoint& joint,
        const Quantity& requested_coordinate) const;
};

} // namespace blcad::geometry
