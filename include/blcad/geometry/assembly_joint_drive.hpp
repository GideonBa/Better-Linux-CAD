#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"

#include <vector>

namespace blcad::geometry {

struct AssemblyJointCoordinateDrive {
  AssemblyJointCoordinateRole role;
  Quantity requested_value;

  friend bool operator==(const AssemblyJointCoordinateDrive&,
                         const AssemblyJointCoordinateDrive&) = default;
};

// Transient role-addressed request for one selected joint. Validation restores
// family order; omitted selected roles hold their authored values.
struct AssemblyJointDrive {
  AssemblyJointId joint;
  std::vector<AssemblyJointCoordinateDrive> requested_coordinates;

  friend bool operator==(const AssemblyJointDrive&, const AssemblyJointDrive&) = default;
};

} // namespace blcad::geometry
