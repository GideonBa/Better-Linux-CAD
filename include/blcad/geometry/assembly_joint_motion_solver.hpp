#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

namespace blcad::geometry {

struct AssemblyJointMotionResult {
  AssemblyJointId joint;
  AssemblyJointType source_joint_type = AssemblyJointType::Revolute;
  AssemblyJointState source_joint_state = AssemblyJointState::Inactive;
  AssemblyConstraintTarget source_target_a;
  AssemblyConstraintTarget source_target_b;
  AssemblyJointLimits source_limits;
  double source_coordinate_deg = 0.0;
  double requested_coordinate_deg = 0.0;
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

// Atomic motion application boundary. It validates both the embedded solve
// snapshots and the source joint-intent snapshot before applying transforms and
// replacing the authored joint coordinate on a Project copy.
class AssemblyJointMotionResultApplier {
public:
  [[nodiscard]] Result<std::size_t> apply(Project& project,
                                          const AssemblyJointMotionResult& result) const;
};

} // namespace blcad::geometry
