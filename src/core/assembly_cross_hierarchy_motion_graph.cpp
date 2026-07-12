#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"

#include "assembly_constraint_graph_participation.hpp"

#include "blcad/core/project.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {
namespace {

struct ResolvedMotionEndpoint {
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

[[nodiscard]] bool motion_relationship_less(const AssemblyMotionRelationshipIdentity& lhs,
                                            const AssemblyMotionRelationshipIdentity& rhs) {
  if (lhs.index() != rhs.index()) {
    return lhs.index() < rhs.index();
  }

  if (const auto* lhs_local = std::get_if<AssemblyLocalRelationshipIdentity>(&lhs)) {
    const auto& rhs_local = std::get<AssemblyLocalRelationshipIdentity>(rhs);
    if (lhs_local->assembly_document.value() != rhs_local.assembly_document.value()) {
      return lhs_local->assembly_document.value() < rhs_local.assembly_document.value();
    }
    return lhs_local->constraint.value() < rhs_local.constraint.value();
  }
  if (const auto* lhs_joint = std::get_if<AssemblyLocalJointIdentity>(&lhs)) {
    const auto& rhs_joint = std::get<AssemblyLocalJointIdentity>(rhs);
    if (lhs_joint->assembly_document.value() != rhs_joint.assembly_document.value()) {
      return lhs_joint->assembly_document.value() < rhs_joint.assembly_document.value();
    }
    return lhs_joint->joint.value() < rhs_joint.joint.value();
  }
  if (const auto* lhs_cross =
          std::get_if<AssemblyProjectCrossHierarchyRelationshipIdentity>(&lhs)) {
    return lhs_cross->constraint.value() <
           std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(rhs).constraint.value();
  }
  return std::get<AssemblyProjectCrossHierarchyJointIdentity>(lhs).joint.value() <
         std::get<AssemblyProjectCrossHierarchyJointIdentity>(rhs).joint.value();
}

[[nodiscard]] bool incidence_less(const AssemblyMotionRelationshipAuthorityIncidence& lhs,
                                  const AssemblyMotionRelationshipAuthorityIncidence& rhs) {
  if (motion_relationship_less(lhs.relationship, rhs.relationship)) {
    return true;
  }
  if (motion_relationship_less(rhs.relationship, lhs.relationship)) {
    return false;
  }
  return authority_less(lhs.authority, rhs.authority);
}

[[nodiscard]] bool
joint_mapping_less(const AssemblyCrossHierarchyJointEndpointAuthorityMapping& lhs,
                   const AssemblyCrossHierarchyJointEndpointAuthorityMapping& rhs) {
  if (lhs.joint.value() != rhs.joint.value()) {
    return lhs.joint.value() < rhs.joint.value();
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

[[nodiscard]] Result<ResolvedMotionEndpoint>
resolve_endpoint(const Project& project, const AssemblyHierarchyConstraintEndpoint& endpoint,
                 const std::string& relationship_id) {
  const AssemblyDocument* assembly = &project.assembly();
  bool active = true;

  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    const SubassemblyInstance* instance = assembly->find_subassembly_instance(occurrence);
    if (instance == nullptr) {
      return Result<ResolvedMotionEndpoint>::failure(Error::validation(
          relationship_id, "cross-hierarchy motion endpoint occurrence path must resolve"));
    }
    active = active && instance->suppression_state() == ComponentSuppressionState::Active;
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (assembly == nullptr) {
      return Result<ResolvedMotionEndpoint>::failure(Error::validation(
          relationship_id, "cross-hierarchy motion endpoint assembly document must resolve"));
    }
  }

  const ComponentInstance* component =
      assembly->find_component_instance(endpoint.component_instance());
  if (component == nullptr) {
    return Result<ResolvedMotionEndpoint>::failure(Error::validation(
        relationship_id, "cross-hierarchy motion endpoint component instance must resolve"));
  }
  active = active && component->suppression_state() == ComponentSuppressionState::Active;
  return Result<ResolvedMotionEndpoint>::success(
      ResolvedMotionEndpoint{assembly, component, active});
}

void add_relationship(std::vector<AssemblyMotionRelationshipIdentity>& relationships,
                      const AssemblyMotionRelationshipIdentity& relationship) {
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

void add_incidence(std::vector<AssemblyMotionRelationshipAuthorityIncidence>& incidences,
                   const AssemblyMotionRelationshipIdentity& relationship,
                   const ComponentTransformAuthority& authority) {
  const AssemblyMotionRelationshipAuthorityIncidence incidence{relationship, authority};
  if (std::find(incidences.begin(), incidences.end(), incidence) == incidences.end()) {
    incidences.push_back(incidence);
  }
}

void add_local_dependency(std::vector<AssemblyMotionRelationshipIdentity>& relationships,
                          std::vector<ComponentTransformAuthority>& authorities,
                          std::vector<AssemblyMotionRelationshipAuthorityIncidence>& incidences,
                          const AssemblyMotionRelationshipIdentity& relationship,
                          const DocumentId& assembly_document, const ComponentInstance& component_a,
                          const ComponentInstance& component_b) {
  const ComponentTransformAuthority authority_a{assembly_document, component_a.id()};
  const ComponentTransformAuthority authority_b{assembly_document, component_b.id()};
  add_relationship(relationships, relationship);
  add_authority(authorities, authority_a);
  add_authority(authorities, authority_b);
  add_incidence(incidences, relationship, authority_a);
  add_incidence(incidences, relationship, authority_b);
}

void add_cross_dependency(std::vector<AssemblyMotionRelationshipIdentity>& relationships,
                          std::vector<ComponentTransformAuthority>& authorities,
                          std::vector<AssemblyMotionRelationshipAuthorityIncidence>& incidences,
                          const AssemblyMotionRelationshipIdentity& relationship,
                          const ResolvedMotionEndpoint& endpoint_a,
                          const ResolvedMotionEndpoint& endpoint_b) {
  const ComponentTransformAuthority authority_a{endpoint_a.assembly->id(),
                                                endpoint_a.component->id()};
  const ComponentTransformAuthority authority_b{endpoint_b.assembly->id(),
                                                endpoint_b.component->id()};
  add_relationship(relationships, relationship);
  add_authority(authorities, authority_a);
  add_authority(authorities, authority_b);
  add_incidence(incidences, relationship, authority_a);
  add_incidence(incidences, relationship, authority_b);
}

[[nodiscard]] std::size_t
relationship_index(const std::vector<AssemblyMotionRelationshipIdentity>& relationships,
                   const AssemblyMotionRelationshipIdentity& relationship) {
  const auto found = std::find(relationships.begin(), relationships.end(), relationship);
  return static_cast<std::size_t>(found - relationships.begin());
}

[[nodiscard]] std::size_t
authority_index(const std::vector<ComponentTransformAuthority>& authorities,
                const ComponentTransformAuthority& authority) {
  const auto found = std::find(authorities.begin(), authorities.end(), authority);
  return static_cast<std::size_t>(found - authorities.begin());
}

[[nodiscard]] std::vector<AssemblyCrossHierarchyMotionGroup>
derive_motion_groups(const std::vector<AssemblyMotionRelationshipIdentity>& relationships,
                     const std::vector<ComponentTransformAuthority>& authorities,
                     const std::vector<AssemblyMotionRelationshipAuthorityIncidence>& incidences) {
  struct PendingNode {
    bool relationship = true;
    std::size_t index = 0U;
  };

  std::vector<bool> visited_relationships(relationships.size(), false);
  std::vector<bool> visited_authorities(authorities.size(), false);
  std::vector<AssemblyCrossHierarchyMotionGroup> groups;

  for (std::size_t start = 0U; start < relationships.size(); ++start) {
    if (visited_relationships[start]) {
      continue;
    }

    std::vector<PendingNode> pending{{true, start}};
    visited_relationships[start] = true;
    AssemblyCrossHierarchyMotionGroup group;
    bool contains_project_cross_joint = false;

    while (!pending.empty()) {
      const PendingNode current = pending.back();
      pending.pop_back();

      if (current.relationship) {
        const AssemblyMotionRelationshipIdentity& relationship = relationships[current.index];
        group.relationships.push_back(relationship);
        contains_project_cross_joint =
            contains_project_cross_joint ||
            std::holds_alternative<AssemblyProjectCrossHierarchyJointIdentity>(relationship);

        for (const AssemblyMotionRelationshipAuthorityIncidence& incidence : incidences) {
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
      for (const AssemblyMotionRelationshipAuthorityIncidence& incidence : incidences) {
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

    std::sort(group.relationships.begin(), group.relationships.end(), motion_relationship_less);
    std::sort(group.authorities.begin(), group.authorities.end(), authority_less);
    if (contains_project_cross_joint) {
      groups.push_back(std::move(group));
    }
  }

  std::sort(groups.begin(), groups.end(), [](const auto& lhs, const auto& rhs) {
    if (!lhs.relationships.empty() && !rhs.relationships.empty()) {
      if (motion_relationship_less(lhs.relationships.front(), rhs.relationships.front())) {
        return true;
      }
      if (motion_relationship_less(rhs.relationships.front(), lhs.relationships.front())) {
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

Result<AssemblyCrossHierarchyMotionGraph>
AssemblyCrossHierarchyMotionGraph::build(const Project& project) {
  auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<AssemblyCrossHierarchyMotionGraph>::failure(valid_structure.error());
  }

  std::vector<AssemblyMotionRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> authorities;
  std::vector<AssemblyMotionRelationshipAuthorityIncidence> incidences;
  std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping> joint_endpoint_mappings;

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
        return Result<AssemblyCrossHierarchyMotionGraph>::failure(
            Error::validation(constraint.id().value(),
                              "local geometric motion relationship components must resolve"));
      }
      if (component_a->suppression_state() != ComponentSuppressionState::Active ||
          component_b->suppression_state() != ComponentSuppressionState::Active) {
        continue;
      }
      add_local_dependency(relationships, authorities, incidences,
                           AssemblyLocalRelationshipIdentity{assembly->id(), constraint.id()},
                           assembly->id(), *component_a, *component_b);
    }

    for (const AssemblyJoint& joint : assembly->joints()) {
      if (joint.state() != AssemblyJointState::Active) {
        continue;
      }
      const ComponentInstance* component_a =
          assembly->find_component_instance(joint.target_a().component_instance());
      const ComponentInstance* component_b =
          assembly->find_component_instance(joint.target_b().component_instance());
      if (component_a == nullptr || component_b == nullptr) {
        return Result<AssemblyCrossHierarchyMotionGraph>::failure(
            Error::validation(joint.id().value(), "local motion joint components must resolve"));
      }
      if (component_a->suppression_state() != ComponentSuppressionState::Active ||
          component_b->suppression_state() != ComponentSuppressionState::Active) {
        continue;
      }
      add_local_dependency(relationships, authorities, incidences,
                           AssemblyLocalJointIdentity{assembly->id(), joint.id()}, assembly->id(),
                           *component_a, *component_b);
    }
  }

  for (const AssemblyHierarchyConstraint& constraint : project.cross_hierarchy_constraints()) {
    if (constraint.state() != AssemblyConstraintState::Active) {
      continue;
    }
    if (!participates_in_current_assembly_constraint_graph(constraint.type())) {
      continue;
    }
    auto target_a = resolve_endpoint(project, constraint.target_a(), constraint.id().value());
    if (target_a.has_error()) {
      return Result<AssemblyCrossHierarchyMotionGraph>::failure(target_a.error());
    }
    auto target_b = resolve_endpoint(project, constraint.target_b(), constraint.id().value());
    if (target_b.has_error()) {
      return Result<AssemblyCrossHierarchyMotionGraph>::failure(target_b.error());
    }
    if (!target_a.value().active || !target_b.value().active) {
      continue;
    }
    add_cross_dependency(relationships, authorities, incidences,
                         AssemblyProjectCrossHierarchyRelationshipIdentity{constraint.id()},
                         target_a.value(), target_b.value());
  }

  for (const AssemblyHierarchyJoint& joint : project.cross_hierarchy_joints()) {
    if (joint.state() != AssemblyJointState::Active) {
      continue;
    }
    auto target_a = resolve_endpoint(project, joint.target_a(), joint.id().value());
    if (target_a.has_error()) {
      return Result<AssemblyCrossHierarchyMotionGraph>::failure(target_a.error());
    }
    auto target_b = resolve_endpoint(project, joint.target_b(), joint.id().value());
    if (target_b.has_error()) {
      return Result<AssemblyCrossHierarchyMotionGraph>::failure(target_b.error());
    }
    if (!target_a.value().active || !target_b.value().active) {
      continue;
    }

    const AssemblyMotionRelationshipIdentity relationship =
        AssemblyProjectCrossHierarchyJointIdentity{joint.id()};
    add_cross_dependency(relationships, authorities, incidences, relationship, target_a.value(),
                         target_b.value());

    const ComponentTransformAuthority authority_a{target_a.value().assembly->id(),
                                                  target_a.value().component->id()};
    const ComponentTransformAuthority authority_b{target_b.value().assembly->id(),
                                                  target_b.value().component->id()};
    joint_endpoint_mappings.push_back(AssemblyCrossHierarchyJointEndpointAuthorityMapping{
        joint.id(), AssemblyHierarchyJointEndpointRole::TargetA, joint.target_a(), authority_a});
    joint_endpoint_mappings.push_back(AssemblyCrossHierarchyJointEndpointAuthorityMapping{
        joint.id(), AssemblyHierarchyJointEndpointRole::TargetB, joint.target_b(), authority_b});
  }

  std::sort(relationships.begin(), relationships.end(), motion_relationship_less);
  std::sort(authorities.begin(), authorities.end(), authority_less);
  std::sort(incidences.begin(), incidences.end(), incidence_less);
  std::sort(joint_endpoint_mappings.begin(), joint_endpoint_mappings.end(), joint_mapping_less);

  auto motion_groups = derive_motion_groups(relationships, authorities, incidences);
  return Result<AssemblyCrossHierarchyMotionGraph>::success(AssemblyCrossHierarchyMotionGraph(
      std::move(relationships), std::move(authorities), std::move(incidences),
      std::move(joint_endpoint_mappings), std::move(motion_groups)));
}

AssemblyCrossHierarchyMotionGraph::AssemblyCrossHierarchyMotionGraph(
    std::vector<AssemblyMotionRelationshipIdentity> relationships,
    std::vector<ComponentTransformAuthority> authorities,
    std::vector<AssemblyMotionRelationshipAuthorityIncidence> incidences,
    std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping> joint_endpoint_mappings,
    std::vector<AssemblyCrossHierarchyMotionGroup> motion_groups)
    : relationships_(std::move(relationships)), authorities_(std::move(authorities)),
      incidences_(std::move(incidences)),
      joint_endpoint_mappings_(std::move(joint_endpoint_mappings)),
      motion_groups_(std::move(motion_groups)) {}

const std::vector<AssemblyMotionRelationshipIdentity>&
AssemblyCrossHierarchyMotionGraph::relationships() const noexcept {
  return relationships_;
}

const std::vector<ComponentTransformAuthority>&
AssemblyCrossHierarchyMotionGraph::authorities() const noexcept {
  return authorities_;
}

const std::vector<AssemblyMotionRelationshipAuthorityIncidence>&
AssemblyCrossHierarchyMotionGraph::incidences() const noexcept {
  return incidences_;
}

const std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping>&
AssemblyCrossHierarchyMotionGraph::joint_endpoint_mappings() const noexcept {
  return joint_endpoint_mappings_;
}

const std::vector<AssemblyCrossHierarchyMotionGroup>&
AssemblyCrossHierarchyMotionGraph::motion_groups() const noexcept {
  return motion_groups_;
}

std::size_t AssemblyCrossHierarchyMotionGraph::relationship_count() const noexcept {
  return relationships_.size();
}
std::size_t AssemblyCrossHierarchyMotionGraph::authority_count() const noexcept {
  return authorities_.size();
}
std::size_t AssemblyCrossHierarchyMotionGraph::incidence_count() const noexcept {
  return incidences_.size();
}
std::size_t AssemblyCrossHierarchyMotionGraph::joint_endpoint_mapping_count() const noexcept {
  return joint_endpoint_mappings_.size();
}
std::size_t AssemblyCrossHierarchyMotionGraph::motion_group_count() const noexcept {
  return motion_groups_.size();
}

} // namespace blcad
