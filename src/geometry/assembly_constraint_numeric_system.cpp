#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_constraint_equation_builder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error internal_error(std::string object_id, std::string message) {
  return Error::internal(std::move(object_id), std::move(message));
}

[[nodiscard]] bool contains_component(const std::vector<ComponentInstanceId>& components,
                                      const ComponentInstanceId& component) {
  return std::find(components.begin(), components.end(), component) != components.end();
}

[[nodiscard]] RigidTransform transform_from_variables(const NumericVector& values,
                                                      std::size_t offset) noexcept {
  return RigidTransform{Vector3{values[offset], values[offset + 1U], values[offset + 2U]},
                        Vector3{values[offset + 3U], values[offset + 4U],
                                values[offset + 5U]}};
}

[[nodiscard]] double variable_step(const AssemblyNumericSystemOptions& options,
                                   std::size_t variable_index) noexcept {
  return variable_index % kAssemblyTransformVariableCount < 3U
             ? options.finite_difference_translation_step_mm
             : options.finite_difference_rotation_step_deg;
}

} // namespace

Result<std::vector<AssemblyConstraintId>> collect_constraint_ids(
    const AssemblyConstraintGraph& graph,
    const std::vector<ComponentInstanceId>& connected_group) {
  std::vector<AssemblyConstraintId> constraint_ids;
  for (const auto& edge : graph.edges()) {
    if (contains_component(connected_group, edge.component_a()) &&
        contains_component(connected_group, edge.component_b())) {
      constraint_ids.push_back(edge.constraint());
    }
  }
  return Result<std::vector<AssemblyConstraintId>>::success(std::move(constraint_ids));
}

Result<NumericVector> evaluate_residuals(
    const Project& project,
    const std::vector<AssemblyConstraintId>& constraint_ids,
    double length_residual_scale_mm) {
  const AssemblyConstraintEquationBuilder builder;
  NumericVector residuals;
  residuals.reserve(constraint_ids.size() * 4U);

  for (const auto& constraint_id : constraint_ids) {
    const AssemblyConstraint* constraint = project.assembly().find_constraint(constraint_id);
    if (constraint == nullptr) {
      return Result<NumericVector>::failure(internal_error(
          constraint_id.value(), "assembly numeric system graph constraint must exist in assembly"));
    }

    auto equation = builder.build(project, *constraint);
    if (equation.has_error()) {
      return Result<NumericVector>::failure(equation.error());
    }

    if (const auto* mate =
            std::get_if<PlanarMateResidualDescriptor>(&equation.value().residual)) {
      residuals.push_back(mate->normal_opposition.x);
      residuals.push_back(mate->normal_opposition.y);
      residuals.push_back(mate->normal_opposition.z);
      residuals.push_back(mate->signed_separation_mm / length_residual_scale_mm);
      continue;
    }

    const auto* distance =
        std::get_if<PlanarDistanceResidualDescriptor>(&equation.value().residual);
    if (distance == nullptr) {
      return Result<NumericVector>::failure(internal_error(
          constraint_id.value(), "assembly numeric system received an unknown residual descriptor"));
    }
    residuals.push_back(distance->normal_parallelism.x);
    residuals.push_back(distance->normal_parallelism.y);
    residuals.push_back(distance->normal_parallelism.z);
    residuals.push_back(distance->distance_residual_mm / length_residual_scale_mm);
  }

  return Result<NumericVector>::success(std::move(residuals));
}

double residual_rms(const NumericVector& residuals) noexcept {
  if (residuals.empty()) {
    return 0.0;
  }
  double sum_squares = 0.0;
  for (double residual : residuals) {
    sum_squares += residual * residual;
  }
  return std::sqrt(sum_squares / static_cast<double>(residuals.size()));
}

double residual_max_abs(const NumericVector& residuals) noexcept {
  double maximum = 0.0;
  for (double residual : residuals) {
    maximum = std::max(maximum, std::abs(residual));
  }
  return maximum;
}

NumericVector read_variables(
    const Project& project,
    const std::vector<ComponentInstanceId>& variable_components) {
  NumericVector values;
  values.reserve(variable_components.size() * kAssemblyTransformVariableCount);
  for (const auto& component_id : variable_components) {
    const RigidTransform& transform =
        project.assembly().find_component_instance(component_id)->transform();
    values.push_back(transform.translation_mm.x);
    values.push_back(transform.translation_mm.y);
    values.push_back(transform.translation_mm.z);
    values.push_back(transform.rotation_deg.x);
    values.push_back(transform.rotation_deg.y);
    values.push_back(transform.rotation_deg.z);
  }
  return values;
}

Result<std::size_t> apply_variables(
    Project& project,
    const std::vector<ComponentInstanceId>& variable_components,
    const NumericVector& values) {
  if (values.size() != variable_components.size() * kAssemblyTransformVariableCount) {
    return Result<std::size_t>::failure(internal_error(
        "assembly.numeric_system",
        "assembly numeric variable vector does not match component variable count"));
  }

  for (std::size_t index = 0U; index < variable_components.size(); ++index) {
    const std::size_t offset = index * kAssemblyTransformVariableCount;
    auto updated = project.assembly().set_component_instance_transform(
        variable_components[index], transform_from_variables(values, offset));
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }
  return Result<std::size_t>::success(variable_components.size());
}

Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project,
    const std::vector<ComponentInstanceId>& variable_components,
    const std::vector<AssemblyConstraintId>& constraint_ids,
    const NumericVector& variables,
    const NumericVector& baseline_residuals,
    const AssemblyNumericSystemOptions& options) {
  NumericMatrix jacobian(baseline_residuals.size(), NumericVector(variables.size(), 0.0));

  for (std::size_t column = 0U; column < variables.size(); ++column) {
    const double step = variable_step(options, column);
    NumericVector plus_variables = variables;
    NumericVector minus_variables = variables;
    plus_variables[column] += step;
    minus_variables[column] -= step;

    Project plus_project = project;
    auto plus_applied = apply_variables(plus_project, variable_components, plus_variables);
    if (plus_applied.has_error()) {
      return Result<NumericMatrix>::failure(plus_applied.error());
    }
    auto plus_residuals =
        evaluate_residuals(plus_project, constraint_ids, options.length_residual_scale_mm);
    if (plus_residuals.has_error()) {
      return Result<NumericMatrix>::failure(plus_residuals.error());
    }

    Project minus_project = project;
    auto minus_applied = apply_variables(minus_project, variable_components, minus_variables);
    if (minus_applied.has_error()) {
      return Result<NumericMatrix>::failure(minus_applied.error());
    }
    auto minus_residuals =
        evaluate_residuals(minus_project, constraint_ids, options.length_residual_scale_mm);
    if (minus_residuals.has_error()) {
      return Result<NumericMatrix>::failure(minus_residuals.error());
    }

    if (plus_residuals.value().size() != baseline_residuals.size() ||
        minus_residuals.value().size() != baseline_residuals.size()) {
      return Result<NumericMatrix>::failure(internal_error(
          "assembly.numeric_system",
          "assembly numeric residual dimension changed during finite differences"));
    }

    const double denominator = 2.0 * step;
    for (std::size_t row = 0U; row < baseline_residuals.size(); ++row) {
      jacobian[row][column] =
          (plus_residuals.value()[row] - minus_residuals.value()[row]) / denominator;
    }
  }

  return Result<NumericMatrix>::success(std::move(jacobian));
}

} // namespace blcad::geometry::detail
