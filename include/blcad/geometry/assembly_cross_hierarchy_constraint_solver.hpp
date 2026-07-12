#pragma once

#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

struct AssemblyCrossHierarchyTransformAuthoritySnapshot {
  ComponentTransformAuthority authority;
  ComponentGroundingState grounding_state = ComponentGroundingState::Free;
  ComponentSuppressionState suppression_state = ComponentSuppressionState::Active;
  RigidTransform source_transform;

  friend bool operator==(const AssemblyCrossHierarchyTransformAuthoritySnapshot&,
                         const AssemblyCrossHierarchyTransformAuthoritySnapshot&) = default;
};

struct ProposedAssemblyCrossHierarchyAuthorityTransform {
  ComponentTransformAuthority authority;
  RigidTransform source_transform;
  RigidTransform proposed_transform;

  friend bool operator==(const ProposedAssemblyCrossHierarchyAuthorityTransform&,
                         const ProposedAssemblyCrossHierarchyAuthorityTransform&) = default;
};

// Derived unapplied result for one exact Block-25 cross-hierarchy solve group.
// Authority snapshots and proposals are document/component qualified so repeated
// rooted occurrences of one child component share one numeric variable block and
// at most one direct-transform proposal.
struct AssemblyCrossHierarchySolveResult {
  AssemblySolveState state = AssemblySolveState::NumericalFailure;
  std::size_t iterations = 0U;
  std::vector<AssemblyRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot> authority_snapshots;
  std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform> proposed_transforms;
  AssemblySolveResidualSummary residual_summary;

  [[nodiscard]] bool converged() const noexcept {
    return state == AssemblySolveState::Converged;
  }

  friend bool operator==(const AssemblyCrossHierarchySolveResult&,
                         const AssemblyCrossHierarchySolveResult&) = default;
};

// Solves one exact deterministic AssemblyCrossHierarchySolveGroup on Project
// copies. Local relationships are evaluated once in their containing document's
// local assembly space; project-level relationships retain exact occurrence paths
// and are evaluated in root-assembly space. No result is applied here.
class AssemblyCrossHierarchyConstraintSolver {
public:
  [[nodiscard]] Result<AssemblyCrossHierarchySolveResult> solve(
      const Project& project,
      const AssemblyCrossHierarchySolveGroup& solve_group,
      AssemblyRigidBodySolverOptions options = {}) const;
};

} // namespace blcad::geometry
