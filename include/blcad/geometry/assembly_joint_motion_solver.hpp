#pragma once

#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_joint_drive.hpp"
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
  std::vector<AssemblyJointCoordinateSlot> coordinate_slots;

  friend bool operator==(const AssemblyJointMotionInputSnapshot&,
                         const AssemblyJointMotionInputSnapshot&) = default;
};

struct AssemblyJointMotionResult {
  AssemblyJointId joint;
  double source_coordinate_deg = 0.0;
  double requested_coordinate_deg = 0.0;
  std::vector<AssemblyJointMotionInputSnapshot> joint_snapshots;
  AssemblySolveResult solve_result;
  std::vector<AssemblyJointCoordinateDrive> requested_coordinates;

  [[nodiscard]] bool converged() const noexcept {
    return solve_result.converged();
  }

  friend bool operator==(const AssemblyJointMotionResult&,
                         const AssemblyJointMotionResult&) = default;
};

// Explicit read-only motion query. It drives typed roles on one active joint,
// holds omitted selected roles and every role of other active joints at authored
// values, and returns ordinary rigid-transform proposals through the shared
// assembly numeric solve engine. The scalar overload is the Revolute adapter.
class AssemblyJointMotionSolver {
public:
  [[nodiscard]] Result<AssemblyJointMotionResult>
  move(const Project& project, AssemblyJointDrive drive,
       AssemblyRigidBodySolverOptions options = {}) const;

  [[nodiscard]] Result<AssemblyJointMotionResult>
  move(const Project& project, AssemblyJointId joint, Quantity requested_coordinate,
       AssemblyRigidBodySolverOptions options = {}) const;
};

// Atomic motion application boundary. It validates the embedded component solve
// snapshots plus every driven joint-input snapshot before applying transforms
// and replacing exactly the explicitly driven selected-joint coordinates on a
// Project copy.
class AssemblyJointMotionResultApplier {
public:
  [[nodiscard]] Result<std::size_t> apply(Project& project,
                                          const AssemblyJointMotionResult& result) const;
};

} // namespace blcad::geometry
