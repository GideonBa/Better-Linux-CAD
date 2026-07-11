#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <vector>

namespace blcad::geometry {

struct AssemblyJointMotionInputSnapshot {
  AssemblyJointId joint;
  AssemblyJointType type = AssemblyJointType::Revolute;
  AssemblyJointState state = AssemblyJointState::Inactive;
  AssemblyConstraintTarget target_a;
  AssemblyConstraintTarget target_b;
  AssemblyJointLimits limits;
  double coordinate_deg = 0.0;

  friend bool operator==(const AssemblyJointMotionInputSnapshot&,
                         const AssemblyJointMotionInputSnapshot&) = default;
};

struct AssemblyJointMotionResult {
  AssemblyJointId joint;
  double source_coordinate_deg = 0.0;
  double requested_coordinate_deg = 0.0;
  std::vector<AssemblyJointMotionInputSnapshot> joint_snapshots;
  AssemblySolveResult solve_result;

  [[nodiscard]] bool converged() const noexcept { return solve_result.converged(); }

  friend bool operator==(const AssemblyJointMotionResult&,
                         const AssemblyJointMotionResult&) = default;
};

// Explicit read-only motion query. It drives one active Revolute joint to a
// requested in-range coordinate, holds every other active joint in the derived
// combined relationship group at its persisted coordinate, and returns ordinary
// rigid-transform proposals through the shared assembly numeric solve engine.
class AssemblyJointMotionSolver {
public:
  [[nodiscard]] Result<AssemblyJointMotionResult>
  move(const Project& project, AssemblyJointId joint, Quantity requested_coordinate,
       AssemblyRigidBodySolverOptions options = {}) const;
};

// Atomic motion application boundary. It validates the embedded component solve
// snapshots plus every driven joint-input snapshot before applying transforms
// and replacing the selected authored joint coordinate on a Project copy.
class AssemblyJointMotionResultApplier {
public:
  [[nodiscard]] Result<std::size_t> apply(Project& project,
                                          const AssemblyJointMotionResult& result) const;
};

} // namespace blcad::geometry
