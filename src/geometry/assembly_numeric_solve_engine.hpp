#pragma once

#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <vector>

namespace blcad::geometry::detail {

// One deterministic Gauss-Newton solve engine shared by ordinary geometric
// constraint solves and transient joint-motion drives.
[[nodiscard]] Result<AssemblySolveResult> solve_numeric_relationships(
    const Project& project,
    const std::vector<ComponentInstanceId>& connected_group,
    const AssemblyNumericRelationshipSet& relationships,
    AssemblyRigidBodySolverOptions options);

} // namespace blcad::geometry::detail
