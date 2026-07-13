#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"

namespace blcad::geometry {

struct AssemblyJointTargetCompatibility {
  AssemblyJointType joint_type = AssemblyJointType::Revolute;
  AssemblyGeometricTargetCapability target_a_capability = AssemblyGeometricTargetCapability::Frame;
  AssemblyGeometricTargetCapability target_b_capability = AssemblyGeometricTargetCapability::Frame;

  friend bool operator==(const AssemblyJointTargetCompatibility&,
                         const AssemblyJointTargetCompatibility&) = default;
};

// Resolves joint intent plus two typed target capability vectors to one
// deterministic ordered projection contract. Compatibility is transient query
// state. Block 40 intentionally supports only the oriented Frame/Frame contract
// required by the existing Revolute residual.
class AssemblyJointTargetCompatibilityResolver {
public:
  [[nodiscard]] Result<AssemblyJointTargetCompatibility>
  resolve(AssemblyJointType joint_type, const AssemblyResolvedGeometricTarget& target_a,
          const AssemblyResolvedGeometricTarget& target_b) const;
};

} // namespace blcad::geometry
