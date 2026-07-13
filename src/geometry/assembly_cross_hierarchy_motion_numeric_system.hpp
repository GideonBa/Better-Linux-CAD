#pragma once

#include "assembly_constraint_numeric_system.hpp"

#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_joint_drive.hpp"

namespace blcad::geometry::detail {

[[nodiscard]] Result<NumericVector> evaluate_cross_hierarchy_motion_group_residuals(
    const Project& project, const AssemblyCrossHierarchyMotionGroup& motion_group,
    AssemblyJointId selected_cross_hierarchy_joint,
    std::vector<AssemblyJointCoordinateDrive> requested_coordinates,
    double length_residual_scale_mm);

} // namespace blcad::geometry::detail
