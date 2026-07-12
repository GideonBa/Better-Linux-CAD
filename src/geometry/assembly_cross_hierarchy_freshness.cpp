#include "assembly_cross_hierarchy_freshness.hpp"

#include "assembly_cross_hierarchy_numeric_system.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool same_boundary_identity(
    const AssemblyCrossHierarchyBoundaryInputSnapshot& lhs,
    const AssemblyCrossHierarchyBoundaryInputSnapshot& rhs) noexcept {
  return lhs.containing_assembly_document == rhs.containing_assembly_document &&
         lhs.subassembly_instance == rhs.subassembly_instance;
}

[[nodiscard]] Result<std::size_t> append_endpoint_boundary_snapshots(
    const Project& project, const AssemblyHierarchyConstraintEndpoint& endpoint,
    std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>& snapshots) {
  const AssemblyDocument* containing = &project.assembly();
  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    const SubassemblyInstance* instance = containing->find_subassembly_instance(occurrence);
    if (instance == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          occurrence.value(), "cross-hierarchy endpoint path boundary must exist"));
    }

    AssemblyCrossHierarchyBoundaryInputSnapshot snapshot{
        containing->id(), occurrence, instance->referenced_assembly_document(),
        instance->suppression_state(), instance->transform()};
    const auto existing = std::find_if(
        snapshots.begin(), snapshots.end(), [&snapshot](const auto& candidate) {
          return same_boundary_identity(candidate, snapshot);
        });
    if (existing == snapshots.end()) {
      snapshots.push_back(std::move(snapshot));
    } else if (*existing != snapshot) {
      return Result<std::size_t>::failure(validation_error(
          occurrence.value(), "cross-hierarchy endpoint boundary identity resolved inconsistently"));
    }

    containing = project.find_assembly_document(instance->referenced_assembly_document());
    if (containing == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          instance->referenced_assembly_document().value(),
          "cross-hierarchy endpoint child assembly must exist"));
    }
  }
  return Result<std::size_t>::success(endpoint.occurrence_path().size());
}

} // namespace

Result<std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>>
make_cross_hierarchy_authority_snapshots(
    const Project& project, const std::vector<ComponentTransformAuthority>& authorities) {
  std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot> snapshots;
  snapshots.reserve(authorities.size());
  for (const ComponentTransformAuthority& authority : authorities) {
    auto component = resolve_cross_hierarchy_authority_component(project, authority);
    if (component.has_error()) {
      return Result<std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>>::failure(
          component.error());
    }
    snapshots.push_back(AssemblyCrossHierarchyTransformAuthoritySnapshot{
        authority, component.value()->referenced_part_document(),
        component.value()->grounding_state(), component.value()->suppression_state(),
        component.value()->transform()});
  }
  return Result<std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>>::success(
      std::move(snapshots));
}

std::vector<DocumentId> semantic_part_documents_from_authority_snapshots(
    const std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>& authority_snapshots) {
  std::vector<DocumentId> part_documents;
  part_documents.reserve(authority_snapshots.size());
  for (const auto& snapshot : authority_snapshots) {
    part_documents.push_back(snapshot.referenced_part_document);
  }
  return part_documents;
}

Result<std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>>
make_cross_hierarchy_boundary_snapshots(
    const Project& project, const std::vector<AssemblyHierarchyConstraintEndpoint>& endpoints) {
  std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot> snapshots;
  for (const AssemblyHierarchyConstraintEndpoint& endpoint : endpoints) {
    auto appended = append_endpoint_boundary_snapshots(project, endpoint, snapshots);
    if (appended.has_error()) {
      return Result<std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>>::failure(
          appended.error());
    }
  }

  std::sort(snapshots.begin(), snapshots.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.containing_assembly_document.value() != rhs.containing_assembly_document.value()) {
      return lhs.containing_assembly_document.value() < rhs.containing_assembly_document.value();
    }
    return lhs.subassembly_instance.value() < rhs.subassembly_instance.value();
  });
  return Result<std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>>::success(
      std::move(snapshots));
}

Result<std::vector<ComponentTransformAuthority>>
validate_cross_hierarchy_authority_and_proposal_freshness(
    const Project& project, const std::vector<ComponentTransformAuthority>& fixed_authorities,
    const std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot>& authority_snapshots,
    const std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform>& proposed_transforms,
    std::string_view application_object_id, std::string_view result_kind) {
  std::vector<ComponentTransformAuthority> current_authorities;
  std::vector<ComponentTransformAuthority> expected_fixed;
  std::vector<ComponentTransformAuthority> expected_free;
  current_authorities.reserve(authority_snapshots.size());

  const std::string result_label(result_kind);
  for (const auto& snapshot : authority_snapshots) {
    if (std::find(current_authorities.begin(), current_authorities.end(), snapshot.authority) !=
        current_authorities.end()) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
          snapshot.authority.component_instance.value(),
          result_label + " contains duplicate authority snapshots"));
    }
    current_authorities.push_back(snapshot.authority);

    auto component = resolve_cross_hierarchy_authority_component(project, snapshot.authority);
    if (component.has_error()) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(component.error());
    }
    if (component.value()->referenced_part_document() != snapshot.referenced_part_document ||
        component.value()->grounding_state() != snapshot.grounding_state ||
        component.value()->suppression_state() != snapshot.suppression_state ||
        component.value()->transform() != snapshot.source_transform) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
          snapshot.authority.component_instance.value(),
          result_label + " is stale because transform authority input changed"));
    }
    if (snapshot.suppression_state != ComponentSuppressionState::Active) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
          snapshot.authority.component_instance.value(),
          result_label + " authority snapshot must be active"));
    }
    if (snapshot.grounding_state == ComponentGroundingState::Grounded) {
      expected_fixed.push_back(snapshot.authority);
    } else {
      expected_free.push_back(snapshot.authority);
    }
  }

  if (fixed_authorities != expected_fixed) {
    return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
        std::string(application_object_id),
        result_label + " fixed-authority order does not match authority snapshots"));
  }

  std::vector<ComponentTransformAuthority> proposal_authorities;
  proposal_authorities.reserve(proposed_transforms.size());
  for (const auto& proposal : proposed_transforms) {
    if (std::find(proposal_authorities.begin(), proposal_authorities.end(), proposal.authority) !=
        proposal_authorities.end()) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
          proposal.authority.component_instance.value(),
          result_label + " contains duplicate authority proposals"));
    }
    proposal_authorities.push_back(proposal.authority);

    const auto snapshot = std::find_if(
        authority_snapshots.begin(), authority_snapshots.end(),
        [&proposal](const AssemblyCrossHierarchyTransformAuthoritySnapshot& candidate) {
          return candidate.authority == proposal.authority;
        });
    if (snapshot == authority_snapshots.end() ||
        snapshot->grounding_state != ComponentGroundingState::Free ||
        snapshot->suppression_state != ComponentSuppressionState::Active ||
        snapshot->source_transform != proposal.source_transform) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
          proposal.authority.component_instance.value(),
          result_label + " proposal does not match a free active authority snapshot"));
    }

    auto component = resolve_cross_hierarchy_authority_component(project, proposal.authority);
    if (component.has_error()) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(component.error());
    }
    auto validated = component.value()->with_transform(proposal.proposed_transform);
    if (validated.has_error()) {
      return Result<std::vector<ComponentTransformAuthority>>::failure(validated.error());
    }
  }

  if (proposal_authorities != expected_free) {
    return Result<std::vector<ComponentTransformAuthority>>::failure(validation_error(
        std::string(application_object_id),
        result_label + " proposals must exactly cover free authority snapshots"));
  }

  return Result<std::vector<ComponentTransformAuthority>>::success(
      std::move(current_authorities));
}

Result<std::size_t> validate_cross_hierarchy_boundary_freshness(
    const Project& project, const std::vector<AssemblyHierarchyConstraintEndpoint>& endpoints,
    const std::vector<AssemblyCrossHierarchyBoundaryInputSnapshot>& boundary_snapshots,
    std::string_view application_object_id, std::string_view result_kind) {
  auto current = make_cross_hierarchy_boundary_snapshots(project, endpoints);
  if (current.has_error()) {
    return Result<std::size_t>::failure(current.error());
  }
  if (current.value() != boundary_snapshots) {
    return Result<std::size_t>::failure(validation_error(
        std::string(application_object_id),
        std::string(result_kind) + " is stale because hierarchy path boundary input changed"));
  }
  return Result<std::size_t>::success(current.value().size());
}

Result<std::size_t> apply_cross_hierarchy_authority_proposals(
    Project& project,
    const std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform>& proposed_transforms,
    std::string_view application_object_id) {
  for (const auto& proposal : proposed_transforms) {
    AssemblyDocument* assembly =
        project.find_assembly_document(proposal.authority.assembly_document);
    if (assembly == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          std::string(application_object_id),
          "cross-hierarchy proposal authority assembly document must remain in candidate project"));
    }
    auto updated = assembly->set_component_instance_transform(
        proposal.authority.component_instance, proposal.proposed_transform);
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }
  return Result<std::size_t>::success(proposed_transforms.size());
}

} // namespace blcad::geometry::detail
