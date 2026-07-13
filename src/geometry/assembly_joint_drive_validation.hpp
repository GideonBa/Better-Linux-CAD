#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_joint_drive.hpp"

#include <vector>

namespace blcad::geometry::detail {

struct ResolvedAssemblyJointDrive {
  std::vector<AssemblyJointCoordinateDrive> requested_coordinates;
  std::vector<AssemblyJointCoordinateDrive> complete_coordinates;
};

[[nodiscard]] Result<ResolvedAssemblyJointDrive>
resolve_joint_drive(const AssemblyJoint& joint, const AssemblyJointDrive& drive);

[[nodiscard]] Result<ResolvedAssemblyJointDrive>
resolve_joint_drive(const AssemblyHierarchyJoint& joint, const AssemblyJointDrive& drive);

[[nodiscard]] Result<double>
revolute_rotation_degrees(const std::vector<AssemblyJointCoordinateDrive>& complete_coordinates,
                          const AssemblyJointId& joint);

} // namespace blcad::geometry::detail
