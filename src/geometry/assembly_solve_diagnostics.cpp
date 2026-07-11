#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include "assembly_constraint_numeric_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

struct MatrixRankResult {
  std::size_t rank = 0U;
  double maximum_abs_entry = 0.0;
  double pivot_threshold = 0.0;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error internal_error(std::string object_id, std::string message) {
  return Error::internal(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<std::size_t> validate_options(const AssemblySolveDiagnosticsOptions& options) {
  const bool absolute_valid =
      std::isfinite(options.rank_absolute_tolerance) && options.rank_absolute_tolerance >= 0.0;
  const bool relative_valid =
      std::isfinite(options.rank_relative_tolerance) && options.rank_relative_tolerance >= 0.0;
  if (!absolute_valid || !relative_valid ||
      (options.rank_absolute_tolerance == 0.0 && options.rank_relative_tolerance == 0.0)) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solve_diagnostics",
        "assembly diagnostics rank tolerances must be finite, non-negative, and not both zero"));
  }
  return Result<std::size_t>::success(0U);
}

[[nodiscard]] std::vector<ComponentInstanceId>
variable_components_from(const AssemblySolveResult& solve_result) {
  std::vector<ComponentInstanceId> variable_components;
  variable_components.reserve(solve_result.proposed_transforms.size());
  for (const auto& proposal : solve_result.proposed_transforms) {
    variable_components.push_back(proposal.component_instance);
  }
  return variable_components;
}

[[nodiscard]] Result<MatrixRankResult> compute_matrix_rank(detail::NumericMatrix matrix,
                                                           std::size_t expected_column_count,
                                                           double absolute_tolerance,
                                                           double relative_tolerance) {
  double maximum_abs_entry = 0.0;
  for (const auto& row : matrix) {
    if (row.size() != expected_column_count) {
      return Result<MatrixRankResult>::failure(
          internal_error("assembly.solve_diagnostics",
                         "assembly diagnostics Jacobian row width does not match variable count"));
    }
    for (double entry : row) {
      if (!std::isfinite(entry)) {
        return Result<MatrixRankResult>::failure(internal_error(
            "assembly.solve_diagnostics", "assembly diagnostics Jacobian must be finite"));
      }
      maximum_abs_entry = std::max(maximum_abs_entry, std::abs(entry));
    }
  }

  const double pivot_threshold =
      std::max(absolute_tolerance, relative_tolerance * maximum_abs_entry);
  std::size_t rank = 0U;
  std::size_t column = 0U;

  while (rank < matrix.size() && column < expected_column_count) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[rank][column]);
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }

    if (pivot_magnitude <= pivot_threshold) {
      ++column;
      continue;
    }

    if (pivot_row != rank) {
      std::swap(matrix[pivot_row], matrix[rank]);
    }

    const double pivot = matrix[rank][column];
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double factor = matrix[row][column] / pivot;
      matrix[row][column] = 0.0;
      for (std::size_t entry = column + 1U; entry < expected_column_count; ++entry) {
        matrix[row][entry] -= factor * matrix[rank][entry];
      }
    }

    ++rank;
    ++column;
  }

  return Result<MatrixRankResult>::success(
      MatrixRankResult{rank, maximum_abs_entry, pivot_threshold});
}

[[nodiscard]] AssemblyJacobianRankSummary
unevaluated_rank_summary(const AssemblySolveResult& solve_result, std::size_t variable_count) {
  return AssemblyJacobianRankSummary{false,
                                     solve_result.residual_summary.residual_component_count,
                                     variable_count,
                                     0U,
                                     0U,
                                     0U,
                                     0U,
                                     0.0,
                                     0.0};
}

} // namespace

std::string_view to_string(AssemblyDofClassification classification) noexcept {
  switch (classification) {
  case AssemblyDofClassification::NotEvaluated:
    return "not_evaluated";
  case AssemblyDofClassification::NoVariableDof:
    return "no_variable_dof";
  case AssemblyDofClassification::Underconstrained:
    return "underconstrained";
  case AssemblyDofClassification::LocallyFullyConstrained:
    return "locally_fully_constrained";
  }
  return "not_evaluated";
}

std::string_view to_string(AssemblyConstraintConsistencyClassification classification) noexcept {
  switch (classification) {
  case AssemblyConstraintConsistencyClassification::LocallyConsistent:
    return "locally_consistent";
  case AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent:
    return "fixed_geometry_inconsistent";
  case AssemblyConstraintConsistencyClassification::SolverDidNotConverge:
    return "solver_did_not_converge";
  }
  return "solver_did_not_converge";
}

std::string_view to_string(AssemblyResidualRankStructure structure) noexcept {
  switch (structure) {
  case AssemblyResidualRankStructure::NotEvaluated:
    return "not_evaluated";
  case AssemblyResidualRankStructure::FullRowRank:
    return "full_row_rank";
  case AssemblyResidualRankStructure::RedundantResidualComponents:
    return "redundant_residual_components";
  }
  return "not_evaluated";
}

Result<AssemblySolveDiagnostics>
AssemblySolveDiagnosticsAnalyzer::analyze(const Project& project,
                                          const std::vector<ComponentInstanceId>& connected_group,
                                          AssemblySolveDiagnosticsOptions options) const {
  auto options_validation = validate_options(options);
  if (options_validation.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(options_validation.error());
  }

  const AssemblyRigidBodySolver solver;
  auto solved = solver.solve(project, connected_group, options.solver_options);
  if (solved.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(solved.error());
  }

  const AssemblySolveResult& solve_result = solved.value();
  std::vector<ComponentInstanceId> variable_components = variable_components_from(solve_result);

  auto graph = AssemblyConstraintGraph::build(project.assembly());
  if (graph.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(graph.error());
  }
  // Match the solver's suppression policy: constraints touching suppressed
  // components vanish, and rank/DOF cover only non-suppressed components.
  std::vector<ComponentInstanceId> active_subgroup;
  active_subgroup.reserve(connected_group.size());
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component = project.assembly().find_component_instance(component_id);
    if (component != nullptr &&
        component->suppression_state() == ComponentSuppressionState::Active) {
      active_subgroup.push_back(component_id);
    }
  }
  auto constraint_ids = detail::collect_constraint_ids(graph.value(), active_subgroup);
  if (constraint_ids.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(constraint_ids.error());
  }

  if (solve_result.state == AssemblySolveState::FixedGeometryInconsistent) {
    return Result<AssemblySolveDiagnostics>::success(AssemblySolveDiagnostics{
        solve_result.state, AssemblyDofClassification::NotEvaluated,
        AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent,
        AssemblyResidualRankStructure::NotEvaluated, solve_result.component_group,
        solve_result.fixed_components, std::move(variable_components),
        std::move(constraint_ids.value()), unevaluated_rank_summary(solve_result, 0U),
        solve_result.residual_summary});
  }

  if (!solve_result.converged()) {
    const std::size_t variable_count =
        variable_components.size() * detail::kAssemblyTransformVariableCount;
    return Result<AssemblySolveDiagnostics>::success(AssemblySolveDiagnostics{
        solve_result.state, AssemblyDofClassification::NotEvaluated,
        AssemblyConstraintConsistencyClassification::SolverDidNotConverge,
        AssemblyResidualRankStructure::NotEvaluated, solve_result.component_group,
        solve_result.fixed_components, std::move(variable_components),
        std::move(constraint_ids.value()), unevaluated_rank_summary(solve_result, variable_count),
        solve_result.residual_summary});
  }

  Project evaluation_project = project;
  const AssemblySolveResultApplier applier;
  auto applied = applier.apply(evaluation_project, solve_result);
  if (applied.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(applied.error());
  }

  detail::NumericVector variables = detail::read_variables(evaluation_project, variable_components);
  auto residuals = detail::evaluate_residuals(evaluation_project, constraint_ids.value(),
                                              options.solver_options.length_residual_scale_mm);
  if (residuals.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(residuals.error());
  }

  const detail::AssemblyNumericSystemOptions numeric_options{
      options.solver_options.length_residual_scale_mm,
      options.solver_options.finite_difference_translation_step_mm,
      options.solver_options.finite_difference_rotation_step_deg};
  auto jacobian = detail::build_central_difference_jacobian(evaluation_project, variable_components,
                                                            constraint_ids.value(), variables,
                                                            residuals.value(), numeric_options);
  if (jacobian.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(jacobian.error());
  }

  auto rank = compute_matrix_rank(jacobian.value(), variables.size(),
                                  options.rank_absolute_tolerance, options.rank_relative_tolerance);
  if (rank.has_error()) {
    return Result<AssemblySolveDiagnostics>::failure(rank.error());
  }

  const std::size_t constrained_dof = rank.value().rank;
  const std::size_t remaining_dof = variables.size() - constrained_dof;
  const std::size_t residual_row_redundancy = residuals.value().size() - constrained_dof;
  const AssemblyDofClassification dof_classification =
      variables.empty()    ? AssemblyDofClassification::NoVariableDof
      : remaining_dof > 0U ? AssemblyDofClassification::Underconstrained
                           : AssemblyDofClassification::LocallyFullyConstrained;
  const AssemblyResidualRankStructure rank_structure =
      residual_row_redundancy == 0U ? AssemblyResidualRankStructure::FullRowRank
                                    : AssemblyResidualRankStructure::RedundantResidualComponents;

  return Result<AssemblySolveDiagnostics>::success(AssemblySolveDiagnostics{
      solve_result.state, dof_classification,
      AssemblyConstraintConsistencyClassification::LocallyConsistent, rank_structure,
      solve_result.component_group, solve_result.fixed_components, std::move(variable_components),
      std::move(constraint_ids.value()),
      AssemblyJacobianRankSummary{true, residuals.value().size(), variables.size(),
                                  rank.value().rank, constrained_dof, remaining_dof,
                                  residual_row_redundancy, rank.value().maximum_abs_entry,
                                  rank.value().pivot_threshold},
      solve_result.residual_summary});
}

} // namespace blcad::geometry
