#pragma once

#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry::detail {

struct AssemblyNumericSolveOutcome {
  AssemblySolveState state = AssemblySolveState::NumericalFailure;
  std::size_t iterations = 0U;
  NumericVector initial_residuals;
  NumericVector final_residuals;
  NumericVector final_variables;
};

// Shared absolute-variable Gauss-Newton engine. Model-specific solvers adapt
// their candidate transform ownership and relationship evaluation to the residual
// evaluator rather than copying optimizer or finite-difference logic.
[[nodiscard]] Result<AssemblyNumericSolveOutcome> solve_numeric_variables(
    const NumericVector& initial_variables,
    bool has_fixed_reference,
    const AssemblyNumericResidualEvaluator& evaluator,
    AssemblyRigidBodySolverOptions options);

// Ordinary local assembly adapter retained for rigid-body and joint-motion users.
[[nodiscard]] Result<AssemblySolveResult> solve_numeric_relationships(
    const Project& project,
    const std::vector<ComponentInstanceId>& connected_group,
    const AssemblyNumericRelationshipSet& relationships,
    AssemblyRigidBodySolverOptions options);

} // namespace blcad::geometry::detail
