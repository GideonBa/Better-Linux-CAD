#pragma once

#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace blcad::geometry {

struct AssemblyCrossHierarchyTransformAuthoritySnapshot {
  ComponentTransformAuthority authority;
  DocumentId referenced_part_document;
  ComponentGroundingState grounding_state = ComponentGroundingState::Free;
  ComponentSuppressionState suppression_state = ComponentSuppressionState::Active;
  RigidTransform source_transform;

  friend bool operator==(const AssemblyCrossHierarchyTransformAuthoritySnapshot&,
                         const AssemblyCrossHierarchyTransformAuthoritySnapshot&) = default;
};

struct AssemblyLocalRelationshipInputSnapshot {
  DocumentId assembly_document;
  AssemblyConstraintId constraint;
  std::string name;
  AssemblyConstraintType type = AssemblyConstraintType::Mate;
  AssemblyConstraintTarget target_a;
  AssemblyConstraintTarget target_b;
  AssemblyConstraintState state = AssemblyConstraintState::Inactive;
  std::optional<double> distance_mm;
  std::optional<double> angle_deg;

  friend bool operator==(const AssemblyLocalRelationshipInputSnapshot&,
                         const AssemblyLocalRelationshipInputSnapshot&) = default;
};

struct AssemblyProjectCrossHierarchyRelationshipInputSnapshot {
  AssemblyConstraintId constraint;
  std::string name;
  AssemblyConstraintType type = AssemblyConstraintType::Mate;
  AssemblyHierarchyConstraintEndpoint target_a;
  AssemblyHierarchyConstraintEndpoint target_b;
  AssemblyConstraintState state = AssemblyConstraintState::Inactive;
  std::optional<double> distance_mm;
  std::optional<double> angle_deg;

  friend bool operator==(const AssemblyProjectCrossHierarchyRelationshipInputSnapshot&,
                         const AssemblyProjectCrossHierarchyRelationshipInputSnapshot&) = default;
};

using AssemblyCrossHierarchyRelationshipInputSnapshot =
    std::variant<AssemblyLocalRelationshipInputSnapshot,
                 AssemblyProjectCrossHierarchyRelationshipInputSnapshot>;

struct AssemblyCrossHierarchyBoundaryInputSnapshot {
  DocumentId containing_assembly_document;
  SubassemblyInstanceId subassembly_instance;
  DocumentId referenced_assembly_document;
  ComponentSuppressionState suppression_state = ComponentSuppressionState::Active;
  RigidTransform source_transform;

  friend bool operator==(const AssemblyCrossHierarchyBoundaryInputSnapshot&,
                         const AssemblyCrossHierarchyBoundaryInputSnapshot&) = default;
};

struct ProposedAssemblyCrossHierarchyAuthorityTransform {
  ComponentTransformAuthority authority;
  RigidTransform source_transform;
  RigidTransform proposed_transform;

  friend bool operator==(const ProposedAssemblyCrossHierarchyAuthorityTransform&,
                         const ProposedAssemblyCrossHierarchyAuthorityTransform&) = default;
};

// Derived unapplied result for one exact Block-25 cross-hierarchy solve group.
// Block-27 freshness snapshots preserve every modeled residual input needed for
// atomic authority-qualified application without persisting solver state.
struct AssemblyCrossHierarchySolveResult {
  AssemblySolveState state = AssemblySolveState::NumericalFailure;
  std::size_t iterations = 0U;
  std::vector<AssemblyRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot> authority_snapshots;
  std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot> relationship_snapshots;
  std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot> hierarchy_boundary_snapshots;
  std::vector<AssemblySemanticTargetPartSnapshot> semantic_target_part_snapshots;
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

// Explicit Block-27 atomic mutation boundary. A result is applied only when its
// authority, exact solve-group relationship records, hierarchy path boundaries,
// and conservative semantic target PartDocument snapshots all remain fresh.
class AssemblyCrossHierarchySolveResultApplier {
public:
  [[nodiscard]] Result<std::size_t> apply(
      Project& project,
      const AssemblyCrossHierarchySolveResult& result) const;
};

} // namespace blcad::geometry
