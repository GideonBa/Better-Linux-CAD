#pragma once

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_cylindrical_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_joint_drive.hpp"
#include "blcad/geometry/assembly_planar_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include <cstddef>
#include <functional>
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

struct AssemblyNumericJointDrive {
  AssemblyJointId joint;
  std::vector<AssemblyJointCoordinateDrive> coordinates;

  friend bool operator==(const AssemblyNumericJointDrive&,
                         const AssemblyNumericJointDrive&) = default;
};

struct AssemblyNumericRelationshipSet {
  std::vector<AssemblyConstraintId> constraint_ids;
  std::vector<AssemblyNumericJointDrive> joint_drives;

  [[nodiscard]] bool empty() const noexcept {
    return constraint_ids.empty() && joint_drives.empty();
  }
};

using AssemblyNumericResidualEvaluator = std::function<Result<NumericVector>(const NumericVector&)>;

[[nodiscard]] Result<std::vector<AssemblyConstraintId>>
collect_constraint_ids(const AssemblyConstraintGraph& graph,
                       const std::vector<ComponentInstanceId>& connected_group);

[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const PlanarConstraintResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const ConcentricResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,
                                                          double length_residual_scale_mm,
                                                          NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const AssemblyGenericRelationshipResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const AssemblyHierarchyConstraintResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const RevoluteJointResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const PrismaticJointResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const CylindricalJointResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);
[[nodiscard]] Result<std::size_t>
append_scaled_residuals(const PlanarJointResidualDescriptor& residual,
                        double length_residual_scale_mm, NumericVector& residuals);

[[nodiscard]] Result<NumericVector>
evaluate_residuals(const Project& project, const std::vector<AssemblyConstraintId>& constraint_ids,
                   double length_residual_scale_mm);

[[nodiscard]] Result<NumericVector>
evaluate_residuals(const Project& project, const AssemblyNumericRelationshipSet& relationships,
                   double length_residual_scale_mm);

[[nodiscard]] double residual_rms(const NumericVector& residuals) noexcept;
[[nodiscard]] double residual_max_abs(const NumericVector& residuals) noexcept;

[[nodiscard]] NumericVector
read_variables(const Project& project, const std::vector<ComponentInstanceId>& variable_components);

[[nodiscard]] Result<std::size_t>
apply_variables(Project& project, const std::vector<ComponentInstanceId>& variable_components,
                const NumericVector& values);

[[nodiscard]] Result<NumericMatrix> build_central_difference_jacobian(
    const NumericVector& variables, const NumericVector& baseline_residuals,
    const AssemblyNumericSystemOptions& options, const AssemblyNumericResidualEvaluator& evaluator);

[[nodiscard]] Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project, const std::vector<ComponentInstanceId>& variable_components,
    const std::vector<AssemblyConstraintId>& constraint_ids, const NumericVector& variables,
    const NumericVector& baseline_residuals, const AssemblyNumericSystemOptions& options);

[[nodiscard]] Result<NumericMatrix> build_central_difference_jacobian(
    const Project& project, const std::vector<ComponentInstanceId>& variable_components,
    const AssemblyNumericRelationshipSet& relationships, const NumericVector& variables,
    const NumericVector& baseline_residuals, const AssemblyNumericSystemOptions& options);

} // namespace blcad::geometry::detail
