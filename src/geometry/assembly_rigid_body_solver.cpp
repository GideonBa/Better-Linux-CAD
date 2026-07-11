#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include "assembly_constraint_numeric_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
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
    return Result<std::size_t>::failure(
        validation_error("assembly.solver",
                         "solver translation finite-difference step must be finite and positive"));
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

[[nodiscard]] bool contains_component(const std::vector<ComponentInstanceId>& components,
                                      const ComponentInstanceId& component) {
  return std::find(components.begin(), components.end(), component) != components.end();
}

[[nodiscard]] bool solve_linear_system(detail::NumericMatrix matrix,
                                       detail::NumericVector right_hand_side,
                                       detail::NumericVector& solution) {
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

[[nodiscard]] bool gauss_newton_step(const detail::NumericMatrix& jacobian,
                                     const detail::NumericVector& residuals, double damping,
                                     detail::NumericVector& step) {
  if (jacobian.size() != residuals.size()) {
    return false;
  }
  const std::size_t variable_count = jacobian.empty() ? 0U : jacobian.front().size();
  detail::NumericMatrix normal_matrix(variable_count, detail::NumericVector(variable_count, 0.0));
  detail::NumericVector right_hand_side(variable_count, 0.0);

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

[[nodiscard]] AssemblySolveResult make_solve_result(
    AssemblySolveState state, std::size_t iterations, const Project& source_project,
    const Project& solved_project, const std::vector<ComponentInstanceId>& connected_group,
    const std::vector<ComponentInstanceId>& fixed_components,
    const std::vector<ComponentInstanceId>& variable_components,
    const detail::NumericVector& initial_residuals, const detail::NumericVector& final_residuals) {
  std::vector<AssemblySolveComponentSnapshot> snapshots;
  snapshots.reserve(connected_group.size());
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component =
        source_project.assembly().find_component_instance(component_id);
    snapshots.push_back(AssemblySolveComponentSnapshot{component_id, component->grounding_state(),
                                                       component->suppression_state(),
                                                       component->transform()});
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
      state,
      iterations,
      connected_group,
      fixed_components,
      std::move(snapshots),
      std::move(proposals),
      AssemblySolveResidualSummary{final_residuals.size(), detail::residual_rms(initial_residuals),
                                   detail::residual_rms(final_residuals),
                                   detail::residual_max_abs(final_residuals)}};
}

[[nodiscard]] Result<std::size_t> validate_application_snapshot(const Project& project,
                                                                const AssemblySolveResult& result) {
  std::vector<ComponentInstanceId> seen_snapshots;
  seen_snapshots.reserve(result.component_snapshots.size());
  for (const auto& snapshot : result.component_snapshots) {
    if (contains_component(seen_snapshots, snapshot.component_instance)) {
      return Result<std::size_t>::failure(
          validation_error(snapshot.component_instance.value(),
                           "assembly solve result contains duplicate snapshots"));
    }
    seen_snapshots.push_back(snapshot.component_instance);

    const ComponentInstance* component =
        project.assembly().find_component_instance(snapshot.component_instance);
    if (component == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.component_instance.value(), "assembly solve result component no longer exists"));
    }
    if (component->transform() != snapshot.source_transform ||
        component->grounding_state() != snapshot.grounding_state ||
        component->suppression_state() != snapshot.suppression_state) {
      return Result<std::size_t>::failure(
          validation_error(snapshot.component_instance.value(),
                           "assembly solve result is stale because component solve input changed"));
    }
  }

  std::vector<ComponentInstanceId> seen_proposals;
  seen_proposals.reserve(result.proposed_transforms.size());
  for (const auto& proposal : result.proposed_transforms) {
    if (contains_component(seen_proposals, proposal.component_instance)) {
      return Result<std::size_t>::failure(
          validation_error(proposal.component_instance.value(),
                           "assembly solve result contains duplicate proposals"));
    }
    seen_proposals.push_back(proposal.component_instance);

    const auto snapshot =
        std::find_if(result.component_snapshots.begin(), result.component_snapshots.end(),
                     [&proposal](const AssemblySolveComponentSnapshot& candidate) {
                       return candidate.component_instance == proposal.component_instance;
                     });
    if (snapshot == result.component_snapshots.end() ||
        snapshot->grounding_state != ComponentGroundingState::Free ||
        snapshot->suppression_state != ComponentSuppressionState::Active ||
        snapshot->source_transform != proposal.source_transform) {
      return Result<std::size_t>::failure(validation_error(
          proposal.component_instance.value(),
          "assembly solve proposal does not match a free active component snapshot"));
    }

    const ComponentInstance* component =
        project.assembly().find_component_instance(proposal.component_instance);
    auto validated = component->with_transform(proposal.proposed_transform);
    if (validated.has_error()) {
      return Result<std::size_t>::failure(validated.error());
    }
  }

  return Result<std::size_t>::success(result.proposed_transforms.size());
}

} // namespace

Result<AssemblySolveResult>
AssemblyRigidBodySolver::solve(const Project& project,
                               const std::vector<ComponentInstanceId>& connected_group,
                               AssemblyRigidBodySolverOptions options) const {
  auto options_validation = validate_options(options);
  if (options_validation.has_error()) {
    return Result<AssemblySolveResult>::failure(options_validation.error());
  }

  auto graph = AssemblyConstraintGraph::build(project.assembly());
  if (graph.has_error()) {
    return Result<AssemblySolveResult>::failure(graph.error());
  }
  const auto groups = graph.value().connected_components();
  if (std::find(groups.begin(), groups.end(), connected_group) == groups.end()) {
    return Result<AssemblySolveResult>::failure(
        validation_error("assembly.solver",
                         "solver input must exactly match one deterministic connected component"));
  }

  // Suppressed components stay in the group snapshots for stale-result
  // detection, but they contribute no variables, and every constraint that
  // touches them vanishes from the numeric system.
  std::vector<ComponentInstanceId> active_subgroup;
  std::vector<ComponentInstanceId> fixed_components;
  std::vector<ComponentInstanceId> variable_components;
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component = project.assembly().find_component_instance(component_id);
    if (component == nullptr) {
      return Result<AssemblySolveResult>::failure(validation_error(
          component_id.value(), "solver connected-group component must exist in assembly"));
    }
    if (component->suppression_state() == ComponentSuppressionState::Suppressed) {
      continue;
    }
    active_subgroup.push_back(component_id);
    if (component->grounding_state() == ComponentGroundingState::Grounded) {
      fixed_components.push_back(component_id);
    } else {
      variable_components.push_back(component_id);
    }
  }

  auto constraint_ids = detail::collect_constraint_ids(graph.value(), active_subgroup);
  if (constraint_ids.has_error()) {
    return Result<AssemblySolveResult>::failure(constraint_ids.error());
  }

  if (constraint_ids.value().empty()) {
    // Every constraint in the group vanished through suppression: nothing is
    // left to solve, so the remaining components stay where they are.
    const Project& unchanged_project = project;
    return Result<AssemblySolveResult>::success(make_solve_result(
        AssemblySolveState::Converged, 0U, project, unchanged_project, connected_group,
        fixed_components, variable_components, detail::NumericVector{}, detail::NumericVector{}));
  }

  if (fixed_components.empty()) {
    return Result<AssemblySolveResult>::failure(validation_error(
        "assembly.solver", "solver connected group requires at least one grounded component"));
  }

  const detail::AssemblyNumericSystemOptions numeric_options{
      options.length_residual_scale_mm, options.finite_difference_translation_step_mm,
      options.finite_difference_rotation_step_deg};

  Project working_project = project;
  auto initial_residuals = detail::evaluate_residuals(working_project, constraint_ids.value(),
                                                      options.length_residual_scale_mm);
  if (initial_residuals.has_error()) {
    return Result<AssemblySolveResult>::failure(initial_residuals.error());
  }
  detail::NumericVector current_residuals = initial_residuals.value();

  if (variable_components.empty()) {
    const AssemblySolveState state =
        detail::residual_rms(current_residuals) <= options.convergence_rms
            ? AssemblySolveState::Converged
            : AssemblySolveState::FixedGeometryInconsistent;
    return Result<AssemblySolveResult>::success(
        make_solve_result(state, 0U, project, working_project, connected_group, fixed_components,
                          variable_components, initial_residuals.value(), current_residuals));
  }

  detail::NumericVector variables = detail::read_variables(working_project, variable_components);
  std::size_t completed_iterations = 0U;

  for (std::size_t iteration = 0U; iteration < options.maximum_iterations; ++iteration) {
    if (detail::residual_rms(current_residuals) <= options.convergence_rms) {
      return Result<AssemblySolveResult>::success(
          make_solve_result(AssemblySolveState::Converged, completed_iterations, project,
                            working_project, connected_group, fixed_components, variable_components,
                            initial_residuals.value(), current_residuals));
    }

    auto jacobian = detail::build_central_difference_jacobian(working_project, variable_components,
                                                              constraint_ids.value(), variables,
                                                              current_residuals, numeric_options);
    if (jacobian.has_error()) {
      return Result<AssemblySolveResult>::failure(jacobian.error());
    }

    const double current_rms = detail::residual_rms(current_residuals);
    bool accepted = false;
    for (std::size_t damping_attempt = 0U;
         damping_attempt < options.maximum_damping_attempts && !accepted; ++damping_attempt) {
      const double damping = options.initial_damping * std::pow(10.0, damping_attempt);
      detail::NumericVector step;
      if (!gauss_newton_step(jacobian.value(), current_residuals, damping, step)) {
        continue;
      }

      for (std::size_t line_search = 0U; line_search < options.maximum_line_search_steps;
           ++line_search) {
        const double scale = std::ldexp(1.0, -static_cast<int>(line_search));
        detail::NumericVector candidate_variables = variables;
        for (std::size_t index = 0U; index < candidate_variables.size(); ++index) {
          candidate_variables[index] += scale * step[index];
        }

        Project candidate_project = working_project;
        auto candidate_applied =
            detail::apply_variables(candidate_project, variable_components, candidate_variables);
        if (candidate_applied.has_error()) {
          return Result<AssemblySolveResult>::failure(candidate_applied.error());
        }
        auto candidate_residuals = detail::evaluate_residuals(
            candidate_project, constraint_ids.value(), options.length_residual_scale_mm);
        if (candidate_residuals.has_error()) {
          return Result<AssemblySolveResult>::failure(candidate_residuals.error());
        }

        if (detail::residual_rms(candidate_residuals.value()) < current_rms) {
          variables = std::move(candidate_variables);
          working_project = std::move(candidate_project);
          current_residuals = std::move(candidate_residuals.value());
          accepted = true;
          ++completed_iterations;
          break;
        }
      }
    }

    if (!accepted) {
      return Result<AssemblySolveResult>::success(
          make_solve_result(AssemblySolveState::NumericalFailure, completed_iterations, project,
                            working_project, connected_group, fixed_components, variable_components,
                            initial_residuals.value(), current_residuals));
    }
  }

  const AssemblySolveState final_state =
      detail::residual_rms(current_residuals) <= options.convergence_rms
          ? AssemblySolveState::Converged
          : AssemblySolveState::MaximumIterationsReached;
  return Result<AssemblySolveResult>::success(make_solve_result(
      final_state, completed_iterations, project, working_project, connected_group,
      fixed_components, variable_components, initial_residuals.value(), current_residuals));
}

Result<std::size_t> AssemblySolveResultApplier::apply(Project& project,
                                                      const AssemblySolveResult& result) const {
  if (!result.converged()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "only a converged assembly solve result can be applied"));
  }

  auto validated = validate_application_snapshot(project, result);
  if (validated.has_error()) {
    return Result<std::size_t>::failure(validated.error());
  }

  Project candidate_project = project;
  for (const auto& proposal : result.proposed_transforms) {
    auto updated = candidate_project.assembly().set_component_instance_transform(
        proposal.component_instance, proposal.proposed_transform);
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }

  project = std::move(candidate_project);
  return Result<std::size_t>::success(result.proposed_transforms.size());
}

} // namespace blcad::geometry
