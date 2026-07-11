#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

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
                        Vector3{values[offset + 3U], values[offset + 4U], values[offset + 5U]}};
}

[[nodiscard]] double variable_step(const AssemblyNumericSystemOptions& options,
                                   std::size_t variable_index) noexcept {
  return variable_index % kAssemblyTransformVariableCount < 3U
             ? options.finite_difference_translation_step_mm
             : options.finite_difference_rotation_step_deg;
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PlanarMateResidualDescriptor& mate,
    double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(mate.normal_opposition.x);
  residuals.push_back(mate.normal_opposition.y);
  residuals.push_back(mate.normal_opposition.z);
  residuals.push_back(mate.signed_separation_mm / length_residual_scale_mm);
  return Result<std::size_t>::success(4U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PlanarDistanceResidualDescriptor& distance,
    double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(distance.normal_parallelism.x);
  residuals.push_back(distance.normal_parallelism.y);
  residuals.push_back(distance.normal_parallelism.z);
  residuals.push_back(distance.distance_residual_mm / length_residual_scale_mm);
  return Result<std::size_t>::success(4U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const PlanarAngleResidualDescriptor& angle,
    double,
    NumericVector& residuals) {
  residuals.push_back(angle.angle_alignment);
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const ConcentricResidualDescriptor& concentric,
    double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(concentric.direction_parallelism.x);
  residuals.push_back(concentric.direction_parallelism.y);
  residuals.push_back(concentric.direction_parallelism.z);
  residuals.push_back(concentric.axis_offset_mm.x / length_residual_scale_mm);
  residuals.push_back(concentric.axis_offset_mm.y / length_residual_scale_mm);
  residuals.push_back(concentric.axis_offset_mm.z / length_residual_scale_mm);
  return Result<std::size_t>::success(6U);
}

[[nodiscard]] Result<std::size_t> append_scaled_residual(
    const InsertResidualDescriptor& insert,
    double length_residual_scale_mm,
    NumericVector& residuals) {
  residuals.push_back(insert.direction_parallelism.x);
  residuals.push_back(insert.direction_parallelism.y);
  residuals.push_back(insert.direction_parallelism.z);
  residuals.push_back(insert.axis_offset_mm.x / length_residual_scale_mm);
  residuals.push_back(insert.axis_offset_mm.y / length_residual_scale_mm);
  residuals.push_back(insert.axis_offset_mm.z / length_residual_scale_mm);
  residuals.push_back(insert.signed_seating_separation_mm / length_residual_scale_mm);
  return Result<std::size_t>::success(7U);
}

[[nodiscard]] Result<std::size_t>
append_constraint_residuals(const Project& project, const AssemblyConstraintId& constraint_id,
                            double length_residual_scale_mm, NumericVector& residuals) {
  const AssemblyConstraint* constraint = project.assembly().find_constraint(constraint_id);
  if (constraint == nullptr) {
    return Result<std::size_t>::failure(
        internal_error(constraint_id.value(),
                       "assembly numeric system graph constraint must exist in assembly"));
  }

  if (constraint->type() == AssemblyConstraintType::Concentric) {
    const AssemblyConcentricConstraintEquationBuilder builder;
    auto equation = builder.build(project, *constraint);
    if (equation.has_error()) return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm,
                                   residuals);
  }

  if (constraint->type() == AssemblyConstraintType::Insert) {
    const AssemblyInsertConstraintEquationBuilder builder;
    auto equation = builder.build(project, *constraint);
    if (equation.has_error()) return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm,
                                   residuals);
  }

  const AssemblyConstraintEquationBuilder builder;
  auto equation = builder.build(project, *constraint);
  if (equation.has_error()) return Result<std::size_t>::failure(equation.error());
  return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
}

[[nodiscard]] Result<std::size_t>
append_revolute_drive_residuals(const Project& project, const AssemblyRevoluteJointDrive& drive,
                                double length_residual_scale_mm, NumericVector& residuals) {
  const AssemblyJoint* joint = project.assembly().find_joint(drive.joint);
  if (joint == nullptr) {
    return Result<std::size_t>::failure(
        internal_error(drive.joint.value(), "assembly numeric revolute drive joint must exist"));
  }
  auto requested = Quantity::angle_deg(drive.requested_coordinate_deg, drive.joint.value());
  if (requested.has_error()) return Result<std::size_t>::failure(requested.error());

  const AssemblyRevoluteJointEquationBuilder builder;
  auto equation = builder.build(project, *joint, requested.value());
  if (equation.has_error()) return Result<std::size_t>::failure(equation.error());
  const RevoluteJointResidualDescriptor& revolute = equation.value().residual;
  residuals.push_back(revolute.direction_alignment.x);
  residuals.push_back(revolute.direction_alignment.y);
  residuals.push_back(revolute.direction_alignment.z);
  residuals.push_back(revolute.axis_offset_mm.x / length_residual_scale_mm);
  residuals.push_back(revolute.axis_offset_mm.y / length_residual_scale_mm);
  residuals.push_back(revolute.axis_offset_mm.z / length_residual_scale_mm);
  residuals.push_back(revolute.signed_seating_separation_mm / length_residual_scale_mm);
  residuals.push_back(revolute.twist_alignment_sine);
  residuals.push_back(revolute.twist_alignment_cosine);
  return Result<std::size_t>::success(9U);
}

} // namespace

Result<std::vector<AssemblyConstraintId>>
collect_constraint_ids(const AssemblyConstraintGraph& graph,
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

Result<std::size_t> append_scaled_residuals(const PlanarConstraintResidualDescriptor& residual,
                                            double length_residual_scale_mm,
                                            NumericVector& residuals) {
  return std::visit(
      [&](const auto& value) {
        return append_scaled_residual(value, length_residual_scale_mm, residuals);
      },
      residual);
}

Result<std::size_t> append_scaled_residuals(const ConcentricResidualDescriptor& residual,
                                            double length_residual_scale_mm,
                                            NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}

Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,
                                            double length_residual_scale_mm,
                                            NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}

Result<std::size_t> append_scaled_residuals(
    const AssemblyHierarchyConstraintResidualDescriptor& residual,
    double length_residual_scale_mm,
    NumericVector& residuals) {
  return std::visit(
      [&](const auto& value) {
        return append_scaled_residual(value, length_residual_scale_mm, residuals);
      },
      residual);
}

Result<NumericVector> evaluate_residuals(const Project& project,
                                         const std::vector<AssemblyConstraintId>& constraint_ids,
                                         double length_residual_scale_mm) {
  return evaluate_residuals(project, AssemblyNumericRelationshipSet{constraint_ids, {}},
                            length_residual_scale_mm);
}

Result<NumericVector> evaluate_residuals(const Project& project,
                                         const AssemblyNumericRelationshipSet& relationships,
                                         double length_residual_scale_mm) {
  NumericVector residuals;
  residuals.reserve(relationships.constraint_ids.size() * 7U +
                    relationships.revolute_drives.size() * 9U);

  for (const auto& constraint_id : relationships.constraint_ids) {
    auto appended =
        append_constraint_residuals(project, constraint_id, length_residual_scale_mm, residuals);
    if (appended.has_error()) return Result<NumericVector>::failure(appended.error());
  }

  for (const auto& drive : relationships.revolute_drives) {
    auto appended =
        append_revolute_drive_residuals(project, drive, length_residual_scale_mm, residuals);
    if (appended.has_error()) return Result<NumericVector>::failure(appended.error());
  }

  return Result<NumericVector>::success(std::move(residuals));
}

double residual_rms(const NumericVector& residuals) noexcept {
  if (residuals.empty()) return 0.0;
  double sum_squares = 0.0;
  for (double residual : residuals) sum_squares += residual * residual;
  return std::sqrt(sum_squares / static_cast<double>(residuals.size()));
}

double residual_max_abs(const NumericVector& residuals) noexcept {
  double maximum = 0.0;
  for (double residual : residuals) maximum = std::max(maximum, std::abs(residual));
  return maximum;
}

NumericVector read_variables(const Project& project,
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

Result<std::size_t> apply_variables(Project& project,
                                    const std::vector<ComponentInstanceId>& variable_components,
                                    const NumericVector& values) {
  if (values.size() != variable_components.size() * kAssemblyTransformVariableCount) {
    return Result<std::size_t>::failure(
        internal_error("assembly.numeric_system",
                       "assembly numeric variable vector does not match component variable count"));
  }

  for (std::size_t index = 0U; index < variable_components.size(); ++index) {
    const std::size_t offset = index * kAssemblyTransformVariableCount;
    auto updated = project.assembly().set_component_instance_transform(
        variable_components[index], transform_from_variables(values, offset));
    if (updated.has_error()) return Result<std::size_t>::failure(updated.error());
  }
  return Result<std::size_t>::success(variable_components.size());
}

Result<NumericMatrix> build_central_difference_jacobian(
    const NumericVector& variables,
    const NumericVector& baseline_residuals,
    const AssemblyNumericSystemOptions& options,
    const AssemblyNumericResidualEvaluator& evaluator) {
  NumericMatrix jacobian(baseline_residuals.size(), NumericVector(variables.size(), 0.0));

  for (std::size_t column = 0U; column < variables.size(); ++column) {
    const double step = variable_step(options, column);
    NumericVector plus_variables = variables;
    NumericVector minus_variables = variables;
    plus_variables[column] += step;
    minus_variables[column] -= step;

    auto plus_residuals = evaluator(plus_variables);
    if (plus_residuals.has_error()) return Result<NumericMatrix>::failure(plus_residuals.error());
    auto minus_residuals = evaluator(minus_variables);
    if (minus_residuals.has_error()) return Result<NumericMatrix>::failure(minus_residuals.error());

    if (plus_residuals.value().size() != baseline_residuals.size() ||
        minus_residuals.value().size() != baseline_residuals.size()) {
      return Result<NumericMatrix>::failure(
          internal_error("assembly.numeric_system",
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

Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project, const std::vector<ComponentInstanceId>& variable_components,
    const std::vector<AssemblyConstraintId>& constraint_ids, const NumericVector& variables,
    const NumericVector& baseline_residuals, const AssemblyNumericSystemOptions& options) {
  return build_central_difference_jacobian(
      project, variable_components, AssemblyNumericRelationshipSet{constraint_ids, {}}, variables,
      baseline_residuals, options);
}

Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project, const std::vector<ComponentInstanceId>& variable_components,
    const AssemblyNumericRelationshipSet& relationships, const NumericVector& variables,
    const NumericVector& baseline_residuals, const AssemblyNumericSystemOptions& options) {
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
  return build_central_difference_jacobian(variables, baseline_residuals, options, evaluator);
}

} // namespace blcad::geometry::detail
