#pragma once

#include "assembly_constraint_numeric_system.hpp"

#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry::detail {

[[nodiscard]] Result<const ComponentInstance*> resolve_cross_hierarchy_authority_component(
    const Project& project,
    const ComponentTransformAuthority& authority);

[[nodiscard]] Result<NumericVector> read_cross_hierarchy_authority_variables(
    const Project& project,
    const std::vector<ComponentTransformAuthority>& variable_authorities);

[[nodiscard]] Result<std::size_t> apply_cross_hierarchy_authority_variables(
    Project& project,
    const std::vector<ComponentTransformAuthority>& variable_authorities,
    const NumericVector& values);

[[nodiscard]] Result<NumericVector> evaluate_cross_hierarchy_group_residuals(
    const Project& project,
    const AssemblyCrossHierarchySolveGroup& solve_group,
    double length_residual_scale_mm);

} // namespace blcad::geometry::detail
