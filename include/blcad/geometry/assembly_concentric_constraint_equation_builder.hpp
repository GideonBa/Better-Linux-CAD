#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <string>

namespace blcad::geometry {

struct AssemblySpaceAxisConstraintTargetDescriptor {
  ComponentInstanceId component_instance;
  std::string semantic_reference;
  AssemblySpaceAxisDescriptor axis;

  friend bool operator==(const AssemblySpaceAxisConstraintTargetDescriptor&,
                         const AssemblySpaceAxisConstraintTargetDescriptor&) = default;
};

// Canonical Concentric residuals for assembly-space axis lines A and B:
//   direction_parallelism = cross(dA, dB)
//   axis_offset_mm = cross(oB - oA, dA)
// A satisfied Concentric relationship has both residual vectors equal to zero.
// Equal and opposed axis directions are both accepted.
struct ConcentricResidualDescriptor {
  Vector3 direction_parallelism;
  Vector3 axis_offset_mm;

  friend bool operator==(const ConcentricResidualDescriptor&,
                         const ConcentricResidualDescriptor&) = default;
};

struct AssemblyConcentricConstraintEquationDescriptor {
  AssemblyConstraintId constraint;
  AssemblySpaceAxisConstraintTargetDescriptor target_a;
  AssemblySpaceAxisConstraintTargetDescriptor target_b;
  ConcentricResidualDescriptor residual;

  friend bool operator==(const AssemblyConcentricConstraintEquationDescriptor&,
                         const AssemblyConcentricConstraintEquationDescriptor&) = default;
};

// Builds deterministic derived residual data for active Concentric constraints.
// No equation is solved and no component transform or project model intent is mutated.
class AssemblyConcentricConstraintEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyConcentricConstraintEquationDescriptor>
  build(const Project& project, const AssemblyConstraint& constraint) const;
};

} // namespace blcad::geometry
