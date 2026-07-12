#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"

namespace blcad::geometry {

struct AssemblyTargetCompatibility {
  AssemblyConstraintType relationship_type = AssemblyConstraintType::Mate;
  AssemblyGeometricTargetCapability target_a_capability = AssemblyGeometricTargetCapability::Plane;
  AssemblyGeometricTargetCapability target_b_capability = AssemblyGeometricTargetCapability::Plane;

  friend bool operator==(const AssemblyTargetCompatibility&,
                         const AssemblyTargetCompatibility&) = default;
};

// Resolves relationship intent plus two typed target capability vectors to one
// deterministic ordered capability pair. It owns compatibility only; equation
// construction remains in the relationship-specific builders.
class AssemblyTargetCompatibilityResolver {
public:
  [[nodiscard]] Result<AssemblyTargetCompatibility>
  resolve(AssemblyConstraintType relationship_type, const AssemblyResolvedGeometricTarget& target_a,
          const AssemblyResolvedGeometricTarget& target_b) const;
};

} // namespace blcad::geometry
