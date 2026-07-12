#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry::detail {

[[nodiscard]] Result<std::vector<AssemblySemanticTargetPartSnapshot>>
make_semantic_target_part_snapshots(const Project& project,
                                    std::vector<DocumentId> part_documents);

[[nodiscard]] Result<std::size_t> validate_semantic_target_part_snapshots(
    const Project& project,
    const std::vector<DocumentId>& expected_part_documents,
    const std::vector<AssemblySemanticTargetPartSnapshot>& snapshots);

} // namespace blcad::geometry::detail
