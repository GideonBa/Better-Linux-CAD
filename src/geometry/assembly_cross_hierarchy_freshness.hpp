#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"

#include <string_view>
#include <vector>

namespace blcad::geometry::detail {

// Legacy documents retain focused historical freshness. Extended state/pose
// projects conservatively require the entire canonical context to be unchanged.
[[nodiscard]] Result<std::string> cross_hierarchy_context_snapshot(const Project&);
[[nodiscard]] Result<std::size_t> validate_cross_hierarchy_context_snapshot(const Project&,
                                                                            const std::string&);

[[nodiscard]] Result<std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>>
make_cross_hierarchy_authority_snapshots(
    const Project& project,
    const std::vector<ComponentTransformAuthority>& authorities);

[[nodiscard]] std::vector<DocumentId> semantic_part_documents_from_authority_snapshots(
    const std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>& authority_snapshots);

[[nodiscard]] Result<std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>>
make_cross_hierarchy_boundary_snapshots(
    const Project& project,
    const std::vector<AssemblyHierarchyConstraintEndpoint>& endpoints);

[[nodiscard]] Result<std::vector<ComponentTransformAuthority>>
validate_cross_hierarchy_authority_and_proposal_freshness(
    const Project& project,
    const std::vector<ComponentTransformAuthority>& fixed_authorities,
    const std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>& authority_snapshots,
    const std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform>& proposed_transforms,
    std::string_view application_object_id,
    std::string_view result_kind);

[[nodiscard]] Result<std::size_t> validate_cross_hierarchy_boundary_freshness(
    const Project& project,
    const std::vector<AssemblyHierarchyConstraintEndpoint>& endpoints,
    const std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>& boundary_snapshots,
    std::string_view application_object_id,
    std::string_view result_kind);

[[nodiscard]] Result<std::size_t> apply_cross_hierarchy_authority_proposals(
    Project& project,
    const std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform>& proposed_transforms,
    std::string_view application_object_id);

} // namespace blcad::geometry::detail
