#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <string>
#include <variant>

namespace blcad::geometry {

struct AssemblySpaceConstraintTargetDescriptor {
  ComponentInstanceId component_instance;
  std::string semantic_reference;
  AssemblySpacePlanarDescriptor plane;

  friend bool operator==(const AssemblySpaceConstraintTargetDescriptor&,
                         const AssemblySpaceConstraintTargetDescriptor&) = default;
};

// Canonical planar Mate residuals:
//   normal_opposition = nA + nB
//   signed_separation_mm = dot(oB - oA, nA)
// A satisfied Mate has both residuals equal to zero.
struct PlanarMateResidualDescriptor {
  Vector3 normal_opposition;
  double signed_separation_mm = 0.0;

  friend bool operator==(const PlanarMateResidualDescriptor&,
                         const PlanarMateResidualDescriptor&) = default;
};

// Canonical planar Distance residuals:
//   normal_parallelism = cross(nA, nB)
//   signed_separation_mm = dot(oB - oA, nA)
//   distance_residual_mm = signed_separation_mm - target_distance_mm
// The signed separation is target-order dependent and uses target A's normal.
struct PlanarDistanceResidualDescriptor {
  Vector3 normal_parallelism;
  double target_distance_mm = 0.0;
  double signed_separation_mm = 0.0;
  double distance_residual_mm = 0.0;

  friend bool operator==(const PlanarDistanceResidualDescriptor&,
                         const PlanarDistanceResidualDescriptor&) = default;
};

// Canonical planar Angle residual:
//   normal_dot = dot(nA, nB)
//   angle_alignment = normal_dot - cos(target_angle_deg)
// A satisfied Angle has angle_alignment equal to zero. The component is
// dimensionless, so no length scaling applies during flattening.
struct PlanarAngleResidualDescriptor {
  double target_angle_deg = 0.0;
  double normal_dot = 0.0;
  double angle_alignment = 0.0;

  friend bool operator==(const PlanarAngleResidualDescriptor&,
                         const PlanarAngleResidualDescriptor&) = default;
};

using PlanarConstraintResidualDescriptor =
    std::variant<PlanarMateResidualDescriptor, PlanarDistanceResidualDescriptor,
                 PlanarAngleResidualDescriptor>;

struct AssemblyConstraintEquationDescriptor {
  AssemblyConstraintId constraint;
  AssemblyConstraintType type = AssemblyConstraintType::Mate;
  AssemblySpaceConstraintTargetDescriptor target_a;
  AssemblySpaceConstraintTargetDescriptor target_b;
  PlanarConstraintResidualDescriptor residual;

  friend bool operator==(const AssemblyConstraintEquationDescriptor&,
                         const AssemblyConstraintEquationDescriptor&) = default;
};

// Builds deterministic derived residual data for active planar Mate and Distance records.
// No equation is solved and no component transform or project model intent is mutated.
class AssemblyConstraintEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyConstraintEquationDescriptor>
  build(const Project& project, const AssemblyConstraint& constraint) const;
};

} // namespace blcad::geometry
