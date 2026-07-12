#pragma once

#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace blcad::geometry {

struct AssemblyLocalJointMotionInputSnapshot {
  DocumentId assembly_document;
  AssemblyJointId joint;
  std::string name;
  AssemblyJointType type = AssemblyJointType::Revolute;
  AssemblyConstraintTarget target_a;
  AssemblyConstraintTarget target_b;
  AssemblyJointState state = AssemblyJointState::Inactive;
  AssemblyJointLimits limits;
  double coordinate_deg = 0.0;

  friend bool operator==(const AssemblyLocalJointMotionInputSnapshot&,
                         const AssemblyLocalJointMotionInputSnapshot&) = default;
};

struct AssemblyProjectCrossHierarchyJointMotionInputSnapshot {
  AssemblyJointId joint;
  std::string name;
  AssemblyJointType type = AssemblyJointType::Revolute;
  AssemblyHierarchyConstraintEndpoint target_a;
  AssemblyHierarchyConstraintEndpoint target_b;
  AssemblyJointState state = AssemblyJointState::Inactive;
  AssemblyJointLimits limits;
  double coordinate_deg = 0.0;

  friend bool operator==(const AssemblyProjectCrossHierarchyJointMotionInputSnapshot&,
                         const AssemblyProjectCrossHierarchyJointMotionInputSnapshot&) = default;
};

using AssemblyCrossHierarchyMotionRelationshipInputSnapshot =
    std::variant<AssemblyLocalRelationshipInputSnapshot,
                 AssemblyLocalJointMotionInputSnapshot,
                 AssemblyProjectCrossHierarchyRelationshipInputSnapshot,
                 AssemblyProjectCrossHierarchyJointMotionInputSnapshot>;

struct AssemblyCrossHierarchyJointMotionResult {
  AssemblyJointId joint;
  double source_coordinate_deg = 0.0;
  double requested_coordinate_deg = 0.0;
  AssemblySolveState state = AssemblySolveState::NumericalFailure;
  std::size_t iterations = 0U;
  std::vector<AssemblyMotionRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot> authority_snapshots;
  std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot> relationship_snapshots;
  std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot> hierarchy_boundary_snapshots;
  std::vector<AssemblySemanticTargetPartSnapshot> semantic_target_part_snapshots;
  std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform> proposed_transforms;
  AssemblySolveResidualSummary residual_summary;

  [[nodiscard]] bool converged() const noexcept {
    return state == AssemblySolveState::Converged;
  }

  friend bool operator==(const AssemblyCrossHierarchyJointMotionResult&,
                         const AssemblyCrossHierarchyJointMotionResult&) = default;
};

// Drives one active Project-level occurrence-qualified Revolute joint to a
// requested in-range coordinate. The complete combined motion component closes
// over local/cross geometry and local/cross joints. Every other active Revolute
// joint in the component is transiently held at its authored coordinate.
class AssemblyCrossHierarchyJointMotionSolver {
public:
  [[nodiscard]] Result<AssemblyCrossHierarchyJointMotionResult>
  move(const Project& project, AssemblyJointId joint, Quantity requested_coordinate,
       AssemblyRigidBodySolverOptions options = {}) const;
};

// Block-28 atomic application boundary. It revalidates complete transform,
// relationship, hierarchy-boundary, semantic PartDocument, motion-group, and
// selected-coordinate inputs before applying authority transforms and the selected
// Project-level joint coordinate on one Project copy.
class AssemblyCrossHierarchyJointMotionResultApplier {
public:
  [[nodiscard]] Result<std::size_t>
  apply(Project& project, const AssemblyCrossHierarchyJointMotionResult& result) const;
};

} // namespace blcad::geometry
