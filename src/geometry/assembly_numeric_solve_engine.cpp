#include "assembly_numeric_solve_engine.hpp"

#include "assembly_semantic_target_freshness.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {

constexpr double kPivotTolerance = 1.0e-14;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool positive_finite(double value) noexcept {
  return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] Result<std::size_t> validate_options(const AssemblyRigidBodySolverOptions& options) {
  if (!positive_finite(options.length_residual_scale_mm)) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "solver length residual scale must be finite and positive"));
  }
  if (!positive_finite(options.convergence_rms)) {
    return Result<std::size_t>::failure(
        validation_error("assembly.solver", "solver convergence RMS must be finite and positive"));
  }
  if (!positive_finite(options.finite_difference_translation_step_mm)) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "solver translation finite-difference step must be finite and positive"));
  }
  if (!positive_finite(options.finite_difference_rotation_step_deg)) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "solver rotation finite-difference step must be finite and positive"));
  }
  if (!positive_finite(options.initial_damping)) {
    return Result<std::size_t>::failure(
        validation_error("assembly.solver", "solver damping must be finite and positive"));
  }
  if (options.maximum_iterations == 0U || options.maximum_damping_attempts == 0U ||
      options.maximum_line_search_steps == 0U) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "solver iteration and search limits must be greater than zero"));
  }
  return Result<std::size_t>::success(0U);
}

[[nodiscard]] bool solve_linear_system(NumericMatrix matrix, NumericVector right_hand_side,
                                       NumericVector& solution) {
  const std::size_t dimension = right_hand_side.size();
  if (matrix.size() != dimension) {
    return false;
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      return false;
    }
  }

  for (std::size_t column = 0U; column < dimension; ++column) {
    std::size_t pivot_row = column;
    double pivot_magnitude = std::abs(matrix[column][column]);
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }

    if (!std::isfinite(pivot_magnitude) || pivot_magnitude <= kPivotTolerance) {
      return false;
    }
    if (pivot_row != column) {
      std::swap(matrix[pivot_row], matrix[column]);
      std::swap(right_hand_side[pivot_row], right_hand_side[column]);
    }

    const double pivot = matrix[column][column];
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double factor = matrix[row][column] / pivot;
      if (!std::isfinite(factor)) {
        return false;
      }
      for (std::size_t entry = column; entry < dimension; ++entry) {
        matrix[row][entry] -= factor * matrix[column][entry];
      }
      right_hand_side[row] -= factor * right_hand_side[column];
    }
  }

  solution.assign(dimension, 0.0);
  for (std::size_t reverse = 0U; reverse < dimension; ++reverse) {
    const std::size_t row = dimension - 1U - reverse;
    double value = right_hand_side[row];
    for (std::size_t column = row + 1U; column < dimension; ++column) {
      value -= matrix[row][column] * solution[column];
    }
    const double pivot = matrix[row][row];
    if (!std::isfinite(pivot) || std::abs(pivot) <= kPivotTolerance) {
      return false;
    }
    solution[row] = value / pivot;
    if (!std::isfinite(solution[row])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool gauss_newton_step(const NumericMatrix& jacobian,
                                     const NumericVector& residuals, double damping,
                                     NumericVector& step) {
  if (jacobian.size() != residuals.size()) {
    return false;
  }
  const std::size_t variable_count = jacobian.empty() ? 0U : jacobian.front().size();
  NumericMatrix normal_matrix(variable_count, NumericVector(variable_count, 0.0));
  NumericVector right_hand_side(variable_count, 0.0);

  for (std::size_t row = 0U; row < jacobian.size(); ++row) {
    if (jacobian[row].size() != variable_count) {
      return false;
    }
    for (std::size_t column = 0U; column < variable_count; ++column) {
      right_hand_side[column] -= jacobian[row][column] * residuals[row];
      for (std::size_t other = 0U; other < variable_count; ++other) {
        normal_matrix[column][other] += jacobian[row][column] * jacobian[row][other];
      }
    }
  }

  for (std::size_t index = 0U; index < variable_count; ++index) {
    normal_matrix[index][index] += damping;
  }
  return solve_linear_system(std::move(normal_matrix), std::move(right_hand_side), step);
}

[[nodiscard]] AssemblyNumericSolveOutcome make_numeric_outcome(
    AssemblySolveState state, std::size_t iterations,
    const NumericVector& initial_residuals, NumericVector final_residuals,
    NumericVector final_variables) {
  return AssemblyNumericSolveOutcome{state, iterations, initial_residuals,
                                     std::move(final_residuals),
                                     std::move(final_variables)};
}

[[nodiscard]] AssemblySolveResult make_solve_result(
    const AssemblyNumericSolveOutcome& outcome, const Project& source_project,
    const Project& solved_project, const std::vector<ComponentInstanceId>& connected_group,
    const std::vector<ComponentInstanceId>& fixed_components,
    const std::vector<ComponentInstanceId>& variable_components,
    std::vector<AssemblySemanticTargetPartSnapshot> semantic_target_part_snapshots) {
  std::vector<AssemblySolveComponentSnapshot> snapshots;
  snapshots.reserve(connected_group.size());
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component =
        source_project.assembly().find_component_instance(component_id);
    snapshots.push_back(AssemblySolveComponentSnapshot{
        component_id, component->referenced_part_document(), component->grounding_state(),
        component->suppression_state(), component->transform()});
  }

  std::vector<ProposedComponentTransform> proposals;
  proposals.reserve(variable_components.size());
  for (const auto& component_id : variable_components) {
    const RigidTransform& source_transform =
        source_project.assembly().find_component_instance(component_id)->transform();
    const RigidTransform& proposed_transform =
        solved_project.assembly().find_component_instance(component_id)->transform();
    proposals.push_back(
        ProposedComponentTransform{component_id, source_transform, proposed_transform});
  }

  return AssemblySolveResult{
      outcome.state,
      outcome.iterations,
      connected_group,
      fixed_components,
      std::move(snapshots),
      std::move(semantic_target_part_snapshots),
      std::move(proposals),
      AssemblySolveResidualSummary{outcome.final_residuals.size(),
                                   residual_rms(outcome.initial_residuals),
                                   residual_rms(outcome.final_residuals),
                                   residual_max_abs(outcome.final_residuals)}};
}

} // namespace

Result<AssemblyNumericSolveOutcome> solve_numeric_variables(
    const NumericVector& initial_variables, bool has_fixed_reference,
    const AssemblyNumericResidualEvaluator& evaluator, AssemblyRigidBodySolverOptions options) {
  auto options_validation = validate_options(options);
  if (options_validation.has_error()) {
    return Result<AssemblyNumericSolveOutcome>::failure(options_validation.error());
  }

  auto initial_residuals = evaluator(initial_variables);
  if (initial_residuals.has_error()) {
    return Result<AssemblyNumericSolveOutcome>::failure(initial_residuals.error());
  }

  NumericVector variables = initial_variables;
  NumericVector current_residuals = initial_residuals.value();

  if (current_residuals.empty()) {
    return Result<AssemblyNumericSolveOutcome>::success(make_numeric_outcome(
        AssemblySolveState::Converged, 0U, initial_residuals.value(),
        std::move(current_residuals), std::move(variables)));
  }

  if (!has_fixed_reference) {
    return Result<AssemblyNumericSolveOutcome>::failure(validation_error(
        "assembly.solver", "solver connected group requires at least one grounded component"));
  }

  if (variables.empty()) {
    const AssemblySolveState state = residual_rms(current_residuals) <= options.convergence_rms
                                         ? AssemblySolveState::Converged
                                         : AssemblySolveState::FixedGeometryInconsistent;
    return Result<AssemblyNumericSolveOutcome>::success(make_numeric_outcome(
        state, 0U, initial_residuals.value(), std::move(current_residuals),
        std::move(variables)));
  }

  const AssemblyNumericSystemOptions numeric_options{
      options.length_residual_scale_mm, options.finite_difference_translation_step_mm,
      options.finite_difference_rotation_step_deg};

  std::size_t completed_iterations = 0U;
  for (std::size_t iteration = 0U; iteration < options.maximum_iterations; ++iteration) {
    if (residual_rms(current_residuals) <= options.convergence_rms) {
      return Result<AssemblyNumericSolveOutcome>::success(make_numeric_outcome(
          AssemblySolveState::Converged, completed_iterations, initial_residuals.value(),
          std::move(current_residuals), std::move(variables)));
    }

    auto jacobian = build_central_difference_jacobian(
        variables, current_residuals, numeric_options, evaluator);
    if (jacobian.has_error()) {
      return Result<AssemblyNumericSolveOutcome>::failure(jacobian.error());
    }

    const double current_rms = residual_rms(current_residuals);
    bool accepted = false;
    for (std::size_t damping_attempt = 0U;
         damping_attempt < options.maximum_damping_attempts && !accepted; ++damping_attempt) {
      const double damping = options.initial_damping * std::pow(10.0, damping_attempt);
      NumericVector step;
      if (!gauss_newton_step(jacobian.value(), current_residuals, damping, step)) {
        continue;
      }

      for (std::size_t line_search = 0U; line_search < options.maximum_line_search_steps;
           ++line_search) {
        const double scale = std::ldexp(1.0, -static_cast<int>(line_search));
        NumericVector candidate_variables = variables;
        for (std::size_t index = 0U; index < candidate_variables.size(); ++index) {
          candidate_variables[index] += scale * step[index];
        }

        auto candidate_residuals = evaluator(candidate_variables);
        if (candidate_residuals.has_error()) {
          return Result<AssemblyNumericSolveOutcome>::failure(candidate_residuals.error());
        }
        if (candidate_residuals.value().size() != current_residuals.size()) {
          return Result<AssemblyNumericSolveOutcome>::failure(validation_error(
              "assembly.solver", "solver residual dimension changed during line search"));
        }

        if (residual_rms(candidate_residuals.value()) < current_rms) {
          variables = std::move(candidate_variables);
          current_residuals = std::move(candidate_residuals.value());
          accepted = true;
          ++completed_iterations;
          break;
        }
      }
    }

    if (!accepted) {
      return Result<AssemblyNumericSolveOutcome>::success(make_numeric_outcome(
          AssemblySolveState::NumericalFailure, completed_iterations, initial_residuals.value(),
          std::move(current_residuals), std::move(variables)));
    }
  }

  const AssemblySolveState final_state = residual_rms(current_residuals) <= options.convergence_rms
                                             ? AssemblySolveState::Converged
                                             : AssemblySolveState::MaximumIterationsReached;
  return Result<AssemblyNumericSolveOutcome>::success(make_numeric_outcome(
      final_state, completed_iterations, initial_residuals.value(),
      std::move(current_residuals), std::move(variables)));
}

Result<AssemblySolveResult> solve_numeric_relationships(
    const Project& project, const std::vector<ComponentInstanceId>& connected_group,
    const AssemblyNumericRelationshipSet& relationships, AssemblyRigidBodySolverOptions options) {
  auto options_validation = validate_options(options);
  if (options_validation.has_error()) {
    return Result<AssemblySolveResult>::failure(options_validation.error());
  }

  std::vector<ComponentInstanceId> fixed_components;
  std::vector<ComponentInstanceId> variable_components;
  std::vector<DocumentId> semantic_target_part_documents;
  semantic_target_part_documents.reserve(connected_group.size());
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component = project.assembly().find_component_instance(component_id);
    if (component == nullptr) {
      return Result<AssemblySolveResult>::failure(validation_error(
          component_id.value(), "solver connected-group component must exist in assembly"));
    }
    semantic_target_part_documents.push_back(component->referenced_part_document());
    if (component->suppression_state() == ComponentSuppressionState::Suppressed) {
      continue;
    }
    if (component->grounding_state() == ComponentGroundingState::Grounded) {
      fixed_components.push_back(component_id);
    } else {
      variable_components.push_back(component_id);
    }
  }

  auto semantic_target_part_snapshots =
      make_semantic_target_part_snapshots(project, std::move(semantic_target_part_documents));
  if (semantic_target_part_snapshots.has_error()) {
    return Result<AssemblySolveResult>::failure(semantic_target_part_snapshots.error());
  }

  const NumericVector initial_variables = read_variables(project, variable_components);
  const AssemblyNumericResidualEvaluator evaluator =
      [&project, &variable_components, &relationships,
       length_scale = options.length_residual_scale_mm](const NumericVector& candidate_values) {
        Project candidate_project = project;
        auto applied = apply_variables(candidate_project, variable_components, candidate_values);
        if (applied.has_error()) {
          return Result<NumericVector>::failure(applied.error());
        }
        return evaluate_residuals(candidate_project, relationships, length_scale);
      };

  auto outcome = solve_numeric_variables(initial_variables, !fixed_components.empty(), evaluator,
                                         options);
  if (outcome.has_error()) {
    return Result<AssemblySolveResult>::failure(outcome.error());
  }

  Project solved_project = project;
  auto applied = apply_variables(solved_project, variable_components,
                                 outcome.value().final_variables);
  if (applied.has_error()) {
    return Result<AssemblySolveResult>::failure(applied.error());
  }

  return Result<AssemblySolveResult>::success(make_solve_result(
      outcome.value(), project, solved_project, connected_group, fixed_components,
      variable_components, std::move(semantic_target_part_snapshots.value())));
}

} // namespace blcad::geometry::detail
