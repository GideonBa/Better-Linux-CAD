#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"

#include "assembly_constraint_graph_participation.hpp"

#include "blcad/core/project.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {
namespace {

struct ResolvedHierarchyEndpoint {
  const AssemblyDocument* assembly = nullptr;
  const ComponentInstance* component = nullptr;
  bool active = false;
};

[[nodiscard]] bool authority_less(const ComponentTransformAuthority& lhs,
                                  const ComponentTransformAuthority& rhs) {
  if (lhs.assembly_document.value() != rhs.assembly_document.value()) {
    return lhs.assembly_document.value() < rhs.assembly_document.value();
  }
  return lhs.component_instance.value() < rhs.component_instance.value();
}

[[nodiscard]] bool relationship_less(const AssemblyRelationshipIdentity& lhs,
                                     const AssemblyRelationshipIdentity& rhs) {
  if (lhs.index() != rhs.index()) {
    return lhs.index() < rhs.index();
  }

  if (std::holds_alternative<AssemblyLocalRelationshipIdentity>(lhs)) {
    const auto& lhs_local = std::get<AssemblyLocalRelationshipIdentity>(lhs);
    const auto& rhs_local = std::get<AssemblyLocalRelationshipIdentity>(rhs);
    if (lhs_local.assembly_document.value() != rhs_local.assembly_document.value()) {
      return lhs_local.assembly_document.value() < rhs_local.assembly_document.value();
    }
    return lhs_local.constraint.value() < rhs_local.constraint.value();
  }

  return std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(lhs).constraint.value() <
         std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(rhs).constraint.value();
}

[[nodiscard]] bool incidence_less(const AssemblyRelationshipAuthorityIncidence& lhs,
                                  const AssemblyRelationshipAuthorityIncidence& rhs) {
  if (relationship_less(lhs.relationship, rhs.relationship)) {
    return true;
  }
  if (relationship_less(rhs.relationship, lhs.relationship)) {
    return false;
  }
  return authority_less(lhs.authority, rhs.authority);
}

[[nodiscard]] bool
endpoint_mapping_less(const AssemblyCrossHierarchyEndpointAuthorityMapping& lhs,
                      const AssemblyCrossHierarchyEndpointAuthorityMapping& rhs) {
  if (lhs.constraint.value() != rhs.constraint.value()) {
    return lhs.constraint.value() < rhs.constraint.value();
  }
  return static_cast<int>(lhs.role) < static_cast<int>(rhs.role);
}

[[nodiscard]] std::vector<const AssemblyDocument*>
ordered_assembly_documents(const Project& project) {
  std::vector<const AssemblyDocument*> assemblies{&project.assembly()};
  for (const AssemblyDocument& assembly : project.child_assembly_documents()) {
    assemblies.push_back(&assembly);
  }
  std::sort(assemblies.begin(), assemblies.end(),
            [](const auto* lhs, const auto* rhs) { return lhs->id().value() < rhs->id().value(); });
  return assemblies;
}

[[nodiscard]] Result<ResolvedHierarchyEndpoint>
resolve_endpoint(const Project& project, const AssemblyHierarchyConstraintEndpoint& endpoint,
                 const AssemblyConstraintId& constraint_id) {
  const AssemblyDocument* assembly = &project.assembly();
  bool active = true;

  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    const SubassemblyInstance* instance = assembly->find_subassembly_instance(occurrence);
    if (instance == nullptr) {
      return Result<ResolvedHierarchyEndpoint>::failure(
          Error::validation(constraint_id.value(),
                            "cross-hierarchy incidence endpoint occurrence path must resolve"));
    }
    active = active && instance->suppression_state() == ComponentSuppressionState::Active;

    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (assembly == nullptr) {
      return Result<ResolvedHierarchyEndpoint>::failure(
          Error::validation(constraint_id.value(),
                            "cross-hierarchy incidence endpoint assembly document must resolve"));
    }
  }

  const ComponentInstance* component =
      assembly->find_component_instance(endpoint.component_instance());
  if (component == nullptr) {
    return Result<ResolvedHierarchyEndpoint>::failure(
        Error::validation(constraint_id.value(),
                          "cross-hierarchy incidence endpoint component instance must resolve"));
  }
  active = active && component->suppression_state() == ComponentSuppressionState::Active;

  return Result<ResolvedHierarchyEndpoint>::success(
      ResolvedHierarchyEndpoint{assembly, component, active});
}

void add_relationship(std::vector<AssemblyRelationshipIdentity>& relationships,
                      const AssemblyRelationshipIdentity& relationship) {
  if (std::find(relationships.begin(), relationships.end(), relationship) == relationships.end()) {
    relationships.push_back(relationship);
  }
}

void add_authority(std::vector<ComponentTransformAuthority>& authorities,
                   const ComponentTransformAuthority& authority) {
  if (std::find(authorities.begin(), authorities.end(), authority) == authorities.end()) {
    authorities.push_back(authority);
  }
}

void add_incidence(std::vector<AssemblyRelationshipAuthorityIncidence>& incidences,
                   const AssemblyRelationshipIdentity& relationship,
                   const ComponentTransformAuthority& authority) {
  const AssemblyRelationshipAuthorityIncidence incidence{relationship, authority};
  if (std::find(incidences.begin(), incidences.end(), incidence) == incidences.end()) {
    incidences.push_back(incidence);
  }
}

[[nodiscard]] std::size_t
relationship_index(const std::vector<AssemblyRelationshipIdentity>& relationships,
                   const AssemblyRelationshipIdentity& relationship) {
  const auto found = std::find(relationships.begin(), relationships.end(), relationship);
  return static_cast<std::size_t>(found - relationships.begin());
}

[[nodiscard]] std::size_t
authority_index(const std::vector<ComponentTransformAuthority>& authorities,
                const ComponentTransformAuthority& authority) {
  const auto found = std::find(authorities.begin(), authorities.end(), authority);
  return static_cast<std::size_t>(found - authorities.begin());
}

[[nodiscard]] std::vector<AssemblyCrossHierarchySolveGroup>
derive_solve_groups(const std::vector<AssemblyRelationshipIdentity>& relationships,
                    const std::vector<ComponentTransformAuthority>& authorities,
                    const std::vector<AssemblyRelationshipAuthorityIncidence>& incidences) {
  struct PendingNode {
    bool relationship = true;
    std::size_t index = 0U;
  };

  std::vector<bool> visited_relationships(relationships.size(), false);
  std::vector<bool> visited_authorities(authorities.size(), false);
  std::vector<AssemblyCrossHierarchySolveGroup> groups;

  for (std::size_t start = 0U; start < relationships.size(); ++start) {
    if (visited_relationships[start]) {
      continue;
    }

    std::vector<PendingNode> pending{{true, start}};
    visited_relationships[start] = true;
    AssemblyCrossHierarchySolveGroup group;
    bool contains_cross_hierarchy_relationship = false;

    while (!pending.empty()) {
      const PendingNode current = pending.back();
      pending.pop_back();

      if (current.relationship) {
        const AssemblyRelationshipIdentity& relationship = relationships[current.index];
        group.relationships.push_back(relationship);
        contains_cross_hierarchy_relationship =
            contains_cross_hierarchy_relationship ||
            std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(relationship);

        for (const AssemblyRelationshipAuthorityIncidence& incidence : incidences) {
          if (incidence.relationship != relationship) {
            continue;
          }
          const std::size_t index = authority_index(authorities, incidence.authority);
          if (index < authorities.size() && !visited_authorities[index]) {
            visited_authorities[index] = true;
            pending.push_back(PendingNode{false, index});
          }
        }
        continue;
      }

      const ComponentTransformAuthority& authority = authorities[current.index];
      group.authorities.push_back(authority);
      for (const AssemblyRelationshipAuthorityIncidence& incidence : incidences) {
        if (incidence.authority != authority) {
          continue;
        }
        const std::size_t index = relationship_index(relationships, incidence.relationship);
        if (index < relationships.size() && !visited_relationships[index]) {
          visited_relationships[index] = true;
          pending.push_back(PendingNode{true, index});
        }
      }
    }

    std::sort(group.relationships.begin(), group.relationships.end(), relationship_less);
    std::sort(group.authorities.begin(), group.authorities.end(), authority_less);
    if (contains_cross_hierarchy_relationship) {
      groups.push_back(std::move(group));
    }
  }

  std::sort(
      groups.begin(), groups.end(),
      [](const AssemblyCrossHierarchySolveGroup& lhs, const AssemblyCrossHierarchySolveGroup& rhs) {
        if (!lhs.relationships.empty() && !rhs.relationships.empty()) {
          if (relationship_less(lhs.relationships.front(), rhs.relationships.front())) {
            return true;
          }
          if (relationship_less(rhs.relationships.front(), lhs.relationships.front())) {
            return false;
          }
        }
        if (lhs.authorities.empty()) {
          return !rhs.authorities.empty();
        }
        if (rhs.authorities.empty()) {
          return false;
        }
        return authority_less(lhs.authorities.front(), rhs.authorities.front());
      });

  return groups;
}

} // namespace

Result<AssemblyCrossHierarchyConstraintGraph>
AssemblyCrossHierarchyConstraintGraph::build(const Project& project) {
  auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<AssemblyCrossHierarchyConstraintGraph>::failure(valid_structure.error());
  }

  std::vector<AssemblyRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> authorities;
  std::vector<AssemblyRelationshipAuthorityIncidence> incidences;
  std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping> endpoint_mappings;

  for (const AssemblyDocument* assembly : ordered_assembly_documents(project)) {
    for (const AssemblyConstraint& constraint : assembly->constraints()) {
      if (constraint.state() != AssemblyConstraintState::Active) {
        continue;
      }
      if (!participates_in_current_assembly_constraint_graph(constraint.type())) {
        continue;
      }

      const ComponentInstance* component_a =
          assembly->find_component_instance(constraint.target_a().component_instance());
      const ComponentInstance* component_b =
          assembly->find_component_instance(constraint.target_b().component_instance());
      if (component_a == nullptr || component_b == nullptr) {
        return Result<AssemblyCrossHierarchyConstraintGraph>::failure(Error::validation(
            constraint.id().value(),
            "local relationship component instance must resolve for cross-hierarchy incidence"));
      }
      if (component_a->suppression_state() != ComponentSuppressionState::Active ||
          component_b->suppression_state() != ComponentSuppressionState::Active) {
        continue;
      }

      const AssemblyRelationshipIdentity relationship =
          AssemblyLocalRelationshipIdentity{assembly->id(), constraint.id()};
      const ComponentTransformAuthority authority_a{assembly->id(), component_a->id()};
      const ComponentTransformAuthority authority_b{assembly->id(), component_b->id()};

      add_relationship(relationships, relationship);
      add_authority(authorities, authority_a);
      add_authority(authorities, authority_b);
      add_incidence(incidences, relationship, authority_a);
      add_incidence(incidences, relationship, authority_b);
    }
  }

  for (const AssemblyHierarchyConstraint& constraint : project.cross_hierarchy_constraints()) {
    if (constraint.state() != AssemblyConstraintState::Active) {
      continue;
    }
    if (!participates_in_current_assembly_constraint_graph(constraint.type())) {
      continue;
    }

    auto target_a = resolve_endpoint(project, constraint.target_a(), constraint.id());
    if (target_a.has_error()) {
      return Result<AssemblyCrossHierarchyConstraintGraph>::failure(target_a.error());
    }
    auto target_b = resolve_endpoint(project, constraint.target_b(), constraint.id());
    if (target_b.has_error()) {
      return Result<AssemblyCrossHierarchyConstraintGraph>::failure(target_b.error());
    }
    if (!target_a.value().active || !target_b.value().active) {
      continue;
    }

    const AssemblyRelationshipIdentity relationship =
        AssemblyProjectCrossHierarchyRelationshipIdentity{constraint.id()};
    const ComponentTransformAuthority authority_a{target_a.value().assembly->id(),
                                                  target_a.value().component->id()};
    const ComponentTransformAuthority authority_b{target_b.value().assembly->id(),
                                                  target_b.value().component->id()};

    add_relationship(relationships, relationship);
    add_authority(authorities, authority_a);
    add_authority(authorities, authority_b);
    add_incidence(incidences, relationship, authority_a);
    add_incidence(incidences, relationship, authority_b);

    endpoint_mappings.push_back(AssemblyCrossHierarchyEndpointAuthorityMapping{
        constraint.id(), AssemblyHierarchyConstraintEndpointRole::TargetA, constraint.target_a(),
        authority_a});
    endpoint_mappings.push_back(AssemblyCrossHierarchyEndpointAuthorityMapping{
        constraint.id(), AssemblyHierarchyConstraintEndpointRole::TargetB, constraint.target_b(),
        authority_b});
  }

  std::sort(relationships.begin(), relationships.end(), relationship_less);
  std::sort(authorities.begin(), authorities.end(), authority_less);
  std::sort(incidences.begin(), incidences.end(), incidence_less);
  std::sort(endpoint_mappings.begin(), endpoint_mappings.end(), endpoint_mapping_less);

  auto solve_groups = derive_solve_groups(relationships, authorities, incidences);
  return Result<AssemblyCrossHierarchyConstraintGraph>::success(
      AssemblyCrossHierarchyConstraintGraph(std::move(relationships), std::move(authorities),
                                            std::move(incidences), std::move(endpoint_mappings),
                                            std::move(solve_groups)));
}

AssemblyCrossHierarchyConstraintGraph::AssemblyCrossHierarchyConstraintGraph(
    std::vector<AssemblyRelationshipIdentity> relationships,
    std::vector<ComponentTransformAuthority> authorities,
    std::vector<AssemblyRelationshipAuthorityIncidence> incidences,
    std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping> endpoint_mappings,
    std::vector<AssemblyCrossHierarchySolveGroup> solve_groups)
    : relationships_(std::move(relationships)), authorities_(std::move(authorities)),
      incidences_(std::move(incidences)), endpoint_mappings_(std::move(endpoint_mappings)),
      solve_groups_(std::move(solve_groups)) {}

const std::vector<AssemblyRelationshipIdentity>&
AssemblyCrossHierarchyConstraintGraph::relationships() const noexcept {
  return relationships_;
}

const std::vector<ComponentTransformAuthority>&
AssemblyCrossHierarchyConstraintGraph::authorities() const noexcept {
  return authorities_;
}

const std::vector<AssemblyRelationshipAuthorityIncidence>&
AssemblyCrossHierarchyConstraintGraph::incidences() const noexcept {
  return incidences_;
}

const std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping>&
AssemblyCrossHierarchyConstraintGraph::endpoint_mappings() const noexcept {
  return endpoint_mappings_;
}

const std::vector<AssemblyCrossHierarchySolveGroup>&
AssemblyCrossHierarchyConstraintGraph::solve_groups() const noexcept {
  return solve_groups_;
}

std::size_t AssemblyCrossHierarchyConstraintGraph::relationship_count() const noexcept {
  return relationships_.size();
}

std::size_t AssemblyCrossHierarchyConstraintGraph::authority_count() const noexcept {
  return authorities_.size();
}

std::size_t AssemblyCrossHierarchyConstraintGraph::incidence_count() const noexcept {
  return incidences_.size();
}

std::size_t AssemblyCrossHierarchyConstraintGraph::endpoint_mapping_count() const noexcept {
  return endpoint_mappings_.size();
}

std::size_t AssemblyCrossHierarchyConstraintGraph::solve_group_count() const noexcept {
  return solve_groups_.size();
}

} // namespace blcad
