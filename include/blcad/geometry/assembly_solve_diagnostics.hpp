#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace blcad::geometry {

enum class AssemblyDofClassification {
  NotEvaluated,
  NoVariableDof,
  Underconstrained,
  LocallyFullyConstrained,
};

[[nodiscard]] std::string_view to_string(AssemblyDofClassification classification) noexcept;

enum class AssemblyConstraintConsistencyClassification {
  LocallyConsistent,
  FixedGeometryInconsistent,
  SolverDidNotConverge,
};

[[nodiscard]] std::string_view
 to_string(AssemblyConstraintConsistencyClassification classification) noexcept;

enum class AssemblyResidualRankStructure {
  NotEvaluated,
  FullRowRank,
  RedundantResidualComponents,
};

[[nodiscard]] std::string_view to_string(AssemblyResidualRankStructure structure) noexcept;

struct AssemblySolveDiagnosticsOptions {
  AssemblyRigidBodySolverOptions solver_options;
  double rank_absolute_tolerance = 1.0e-12;
  double rank_relative_tolerance = 1.0e-8;

  friend bool operator==(const AssemblySolveDiagnosticsOptions&,
                         const AssemblySolveDiagnosticsOptions&) = default;
};

struct AssemblyJacobianRankSummary {
  bool rank_evaluated = false;
  std::size_t residual_component_count = 0U;
  std::size_t variable_count = 0U;
  std::size_t jacobian_rank = 0U;
  std::size_t constrained_dof = 0U;
  std::size_t remaining_dof = 0U;
  std::size_t residual_row_redundancy = 0U;
  double maximum_abs_jacobian_entry = 0.0;
  double pivot_threshold = 0.0;

  friend bool operator==(const AssemblyJacobianRankSummary&,
                         const AssemblyJacobianRankSummary&) = default;
};

struct AssemblySolveDiagnostics {
  AssemblySolveState solve_state = AssemblySolveState::NumericalFailure;
  AssemblyDofClassification dof_classification = AssemblyDofClassification::NotEvaluated;
  AssemblyConstraintConsistencyClassification consistency_classification =
      AssemblyConstraintConsistencyClassification::SolverDidNotConverge;
  AssemblyResidualRankStructure residual_rank_structure =
      AssemblyResidualRankStructure::NotEvaluated;
  std::vector<ComponentInstanceId> component_group;
  std::vector<ComponentInstanceId> fixed_components;
  std::vector<ComponentInstanceId> variable_components;
  std::vector<AssemblyConstraintId> constraint_order;
  AssemblyJacobianRankSummary rank_summary;
  AssemblySolveResidualSummary residual_summary;

  friend bool operator==(const AssemblySolveDiagnostics&,
                         const AssemblySolveDiagnostics&) = default;
};

// Read-only local Jacobian-rank and remaining-DOF analysis over the exact
// deterministic numeric system used by AssemblyRigidBodySolver.
class AssemblySolveDiagnosticsAnalyzer {
public:
  [[nodiscard]] Result<AssemblySolveDiagnostics> analyze(
      const Project& project,
      const std::vector<ComponentInstanceId>& connected_group,
      AssemblySolveDiagnosticsOptions options = {}) const;
};

struct AssemblyCrossHierarchySolveDiagnostics {
  AssemblySolveState solve_state = AssemblySolveState::NumericalFailure;
  AssemblyDofClassification dof_classification = AssemblyDofClassification::NotEvaluated;
  AssemblyConstraintConsistencyClassification consistency_classification =
      AssemblyConstraintConsistencyClassification::SolverDidNotConverge;
  AssemblyResidualRankStructure residual_rank_structure =
      AssemblyResidualRankStructure::NotEvaluated;
  std::vector<AssemblyRelationshipIdentity> relationship_order;
  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<ComponentTransformAuthority> variable_authorities;
  AssemblyJacobianRankSummary rank_summary;
  AssemblySolveResidualSummary residual_summary;

  friend bool operator==(const AssemblyCrossHierarchySolveDiagnostics&,
                         const AssemblyCrossHierarchySolveDiagnostics&) = default;
};

// Read-only Block-27 Jacobian-rank and remaining-DOF analysis over exactly the
// same free-authority ordering and mixed residual evaluator used by
// AssemblyCrossHierarchyConstraintSolver.
class AssemblyCrossHierarchySolveDiagnosticsAnalyzer {
public:
  [[nodiscard]] Result<AssemblyCrossHierarchySolveDiagnostics> analyze(
      const Project& project,
      const AssemblyCrossHierarchySolveGroup& solve_group,
      AssemblySolveDiagnosticsOptions options = {}) const;
};

} // namespace blcad::geometry
