#pragma once

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad::geometry {

enum class AssemblySolveState {
  Converged,
  MaximumIterationsReached,
  FixedGeometryInconsistent,
  NumericalFailure,
};

struct AssemblyRigidBodySolverOptions {
  double length_residual_scale_mm = 1.0;
  double convergence_rms = 1.0e-8;
  double finite_difference_translation_step_mm = 1.0e-4;
  double finite_difference_rotation_step_deg = 1.0e-4;
  double initial_damping = 1.0e-6;
  std::size_t maximum_iterations = 100U;
  std::size_t maximum_damping_attempts = 8U;
  std::size_t maximum_line_search_steps = 12U;

  friend bool operator==(const AssemblyRigidBodySolverOptions&,
                         const AssemblyRigidBodySolverOptions&) = default;
};

struct AssemblySolveResidualSummary {
  std::size_t residual_component_count = 0U;
  double initial_rms = 0.0;
  double final_rms = 0.0;
  double final_max_abs = 0.0;

  friend bool operator==(const AssemblySolveResidualSummary&,
                         const AssemblySolveResidualSummary&) = default;
};

struct AssemblySolveComponentSnapshot {
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  ComponentGroundingState grounding_state = ComponentGroundingState::Free;
  ComponentSuppressionState suppression_state = ComponentSuppressionState::Active;
  RigidTransform source_transform;

  friend bool operator==(const AssemblySolveComponentSnapshot&,
                         const AssemblySolveComponentSnapshot&) = default;
};

// Conservative exact model-intent snapshot for a PartDocument referenced by a
// participating component/authority. The canonical persisted PartDocument JSON
// payload is retained verbatim so stale-result detection has no hash collision
// boundary and observes every serialized semantic target-producing model edit.
struct AssemblySemanticTargetPartSnapshot {
  DocumentId part_document;
  std::string canonical_model_intent_json;

  friend bool operator==(const AssemblySemanticTargetPartSnapshot&,
                         const AssemblySemanticTargetPartSnapshot&) = default;
};

struct ProposedComponentTransform {
  ComponentInstanceId component_instance;
  RigidTransform source_transform;
  RigidTransform proposed_transform;

  friend bool operator==(const ProposedComponentTransform&,
                         const ProposedComponentTransform&) = default;
};

struct AssemblySolveResult {
  AssemblySolveState state = AssemblySolveState::NumericalFailure;
  std::size_t iterations = 0U;
  std::vector<ComponentInstanceId> component_group;
  std::vector<ComponentInstanceId> fixed_components;
  std::vector<AssemblySolveComponentSnapshot> component_snapshots;
  std::vector<AssemblySemanticTargetPartSnapshot> semantic_target_part_snapshots;
  std::vector<ProposedComponentTransform> proposed_transforms;
  AssemblySolveResidualSummary residual_summary;

  [[nodiscard]] bool converged() const noexcept {
    return state == AssemblySolveState::Converged;
  }

  friend bool operator==(const AssemblySolveResult&, const AssemblySolveResult&) = default;
};

// Deterministic first rigid-body assembly solve seed. The solver consumes one exact
// AssemblyConstraintGraph connected group, keeps grounded components fixed, optimizes
// free component RigidTransform values on a Project copy, and returns proposed updates.
class AssemblyRigidBodySolver {
public:
  [[nodiscard]] Result<AssemblySolveResult> solve(
      const Project& project,
      const std::vector<ComponentInstanceId>& connected_group,
      AssemblyRigidBodySolverOptions options = {}) const;
};

// Explicit atomic mutation boundary for successful solve results. The applier rejects
// non-converged and stale results before replacing any component transform.
class AssemblySolveResultApplier {
public:
  [[nodiscard]] Result<std::size_t> apply(Project& project,
                                          const AssemblySolveResult& result) const;
};

} // namespace blcad::geometry
