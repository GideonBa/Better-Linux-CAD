#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <string>

namespace blcad::geometry {

struct AssemblySpaceInsertConstraintTargetDescriptor {
  ComponentInstanceId component_instance;
  std::string semantic_reference;
  FeatureId source_feature;
  ProfileId source_profile;
  AssemblySpaceAxisDescriptor axis;
  AssemblySpacePlanarDescriptor seating_plane;

  friend bool operator==(const AssemblySpaceInsertConstraintTargetDescriptor&,
                         const AssemblySpaceInsertConstraintTargetDescriptor&) = default;
};

// Canonical Insert residuals for assembly-space axis/seat pairs A and B:
//   direction_parallelism       = cross(dA, dB)
//   axis_offset_mm               = cross(oB - oA, dA)
//   signed_seating_separation_mm = dot(sB - sA, nA)
// The seat normal is canonically aligned with the endpoint's semantic axis.
// Equal and opposed axis directions are accepted; target A defines the signed seating direction.
struct InsertResidualDescriptor {
  Vector3 direction_parallelism;
  Vector3 axis_offset_mm;
  double signed_seating_separation_mm = 0.0;

  friend bool operator==(const InsertResidualDescriptor&, const InsertResidualDescriptor&) = default;
};

struct AssemblyInsertConstraintEquationDescriptor {
  AssemblyConstraintId constraint;
  AssemblySpaceInsertConstraintTargetDescriptor target_a;
  AssemblySpaceInsertConstraintTargetDescriptor target_b;
  InsertResidualDescriptor residual;

  friend bool operator==(const AssemblyInsertConstraintEquationDescriptor&,
                         const AssemblyInsertConstraintEquationDescriptor&) = default;
};

// Builds deterministic derived residual data for active Insert constraints.
// No equation is solved and no component transform or project model intent is mutated.
class AssemblyInsertConstraintEquationBuilder {
public:
  [[nodiscard]] Result<AssemblyInsertConstraintEquationDescriptor>
  build(const Project& project, const AssemblyConstraint& constraint) const;
};

} // namespace blcad::geometry
