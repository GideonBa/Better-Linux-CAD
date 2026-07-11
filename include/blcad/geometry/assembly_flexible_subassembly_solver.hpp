#pragma once

#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

// One child-assembly-local rigid-body solve selected through an exact rooted
// subassembly occurrence path. The local result uses the referenced child
// AssemblyDocument as its solver root; occurrence boundary transforms remain
// outside the numeric system.
struct AssemblyFlexibleSubassemblySolveResult {
  std::vector<SubassemblyInstanceId> occurrence_path;
  DocumentId assembly_document;
  AssemblySolveResult local_result;

  [[nodiscard]] bool converged() const noexcept { return local_result.converged(); }

  friend bool operator==(const AssemblyFlexibleSubassemblySolveResult&,
                         const AssemblyFlexibleSubassemblySolveResult&) = default;
};

// Exposes the direct ComponentInstance variables and local constraints of one
// active child AssemblyDocument to the existing rigid-body solver. The selected
// occurrence path is identity/staleness context only: repeated occurrences of
// the same child document share the same internal authored component pose.
class AssemblyFlexibleSubassemblySolver {
public:
  [[nodiscard]] Result<AssemblyFlexibleSubassemblySolveResult> solve(
      const Project& project,
      const std::vector<SubassemblyInstanceId>& occurrence_path,
      const std::vector<ComponentInstanceId>& connected_group,
      AssemblyRigidBodySolverOptions options = {}) const;
};

// Atomically applies a fresh converged child-local solve back to the referenced
// project-owned AssemblyDocument. Existing AssemblySolveResult snapshot rules
// are reused against a freshly reconstructed child-as-root solve view.
class AssemblyFlexibleSubassemblySolveResultApplier {
public:
  [[nodiscard]] Result<std::size_t>
  apply(Project& project, const AssemblyFlexibleSubassemblySolveResult& result) const;
};

} // namespace blcad::geometry
