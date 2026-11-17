#pragma once

#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include <string>

namespace blcad::geometry {

struct AssemblyOccurrenceSolveResult {
  std::vector<SubassemblyInstanceId> path;
  DocumentId assembly_document;
  std::string source_project_json;
  AssemblySolveResult local_result;
  [[nodiscard]] bool converged() const noexcept {
    return local_result.converged();
  }
};
struct AssemblyOccurrenceMotionResult {
  std::vector<SubassemblyInstanceId> path;
  DocumentId assembly_document;
  std::string source_project_json;
  AssemblyJointMotionResult local_result;
  [[nodiscard]] bool converged() const noexcept {
    return local_result.converged();
  }
};

// Explicit independent-occurrence APIs. Legacy child-document solving keeps
// its shared-definition contract and is intentionally a separate command.
class AssemblyOccurrenceSolver {
public:
  explicit AssemblyOccurrenceSolver(StepImportTopologyContext* imports = nullptr)
      : imports_(imports) {}
  [[nodiscard]] Result<AssemblyOccurrenceSolveResult>
  solve(const Project&, const std::vector<SubassemblyInstanceId>&,
        const std::vector<ComponentInstanceId>& connected_group,
        AssemblyRigidBodySolverOptions = {}) const;
  [[nodiscard]] Result<AssemblyOccurrenceMotionResult>
  move(const Project&, const std::vector<SubassemblyInstanceId>&, AssemblyJointDrive,
       AssemblyRigidBodySolverOptions = {}) const;
  [[nodiscard]] Result<std::size_t> apply(Project&, const AssemblyOccurrenceSolveResult&) const;
  [[nodiscard]] Result<std::size_t> apply(Project&, const AssemblyOccurrenceMotionResult&) const;

private:
  StepImportTopologyContext* imports_;
};
} // namespace blcad::geometry
