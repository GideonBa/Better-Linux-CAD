#pragma once

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace blcad::geometry::detail {

using NumericVector = std::vector<double>;
using NumericMatrix = std::vector<NumericVector>;

inline constexpr std::size_t kAssemblyTransformVariableCount = 6U;

struct AssemblyNumericSystemOptions {
  double length_residual_scale_mm = 1.0;
  double finite_difference_translation_step_mm = 1.0e-4;
  double finite_difference_rotation_step_deg = 1.0e-4;
};

struct AssemblyRevoluteJointDrive {
  AssemblyJointId joint;
  double requested_coordinate_deg = 0.0;

  friend bool operator==(const AssemblyRevoluteJointDrive&,
                         const AssemblyRevoluteJointDrive&) = default;
};

// Derived numeric relationship selection. Persistent assembly constraints remain
// distinct from persistent joint intent; a motion query may append one transient
// revolute drive to the same residual/Jacobian system.
struct AssemblyNumericRelationshipSet {
  std::vector<AssemblyConstraintId> constraint_ids;
  std::optional<AssemblyRevoluteJointDrive> revolute_drive;

  [[nodiscard]] bool empty() const noexcept {
    return constraint_ids.empty() && !revolute_drive.has_value();
  }
};

[[nodiscard]] Result<std::vector<AssemblyConstraintId>> collect_constraint_ids(
    const AssemblyConstraintGraph& graph,
    const std::vector<ComponentInstanceId>& connected_group);

[[nodiscard]] Result<NumericVector> evaluate_residuals(
    const Project& project,
    const std::vector<AssemblyConstraintId>& constraint_ids,
    double length_residual_scale_mm);

[[nodiscard]] Result<NumericVector> evaluate_residuals(
    const Project& project,
    const AssemblyNumericRelationshipSet& relationships,
    double length_residual_scale_mm);

[[nodiscard]] double residual_rms(const NumericVector& residuals) noexcept;
[[nodiscard]] double residual_max_abs(const NumericVector& residuals) noexcept;

[[nodiscard]] NumericVector read_variables(
    const Project& project,
    const std::vector<ComponentInstanceId>& variable_components);

[[nodiscard]] Result<std::size_t> apply_variables(
    Project& project,
    const std::vector<ComponentInstanceId>& variable_components,
    const NumericVector& values);

[[nodiscard]] Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project,
    const std::vector<ComponentInstanceId>& variable_components,
    const std::vector<AssemblyConstraintId>& constraint_ids,
    const NumericVector& variables,
    const NumericVector& baseline_residuals,
    const AssemblyNumericSystemOptions& options);

[[nodiscard]] Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project,
    const std::vector<ComponentInstanceId>& variable_components,
    const AssemblyNumericRelationshipSet& relationships,
    const NumericVector& variables,
    const NumericVector& baseline_residuals,
    const AssemblyNumericSystemOptions& options);

} // namespace blcad::geometry::detail
