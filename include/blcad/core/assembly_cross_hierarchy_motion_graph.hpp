#pragma once

#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace blcad {

class Project;

struct AssemblyLocalJointIdentity {
  DocumentId assembly_document;
  AssemblyJointId joint;

  friend bool operator==(const AssemblyLocalJointIdentity&, const AssemblyLocalJointIdentity&) =
      default;
};

struct AssemblyProjectCrossHierarchyJointIdentity {
  AssemblyJointId joint;

  friend bool operator==(const AssemblyProjectCrossHierarchyJointIdentity&,
                         const AssemblyProjectCrossHierarchyJointIdentity&) = default;
};

// Canonical Block-28 combined motion relationship order is represented by the
// variant alternative order: local geometry, local joint, project cross geometry,
// project cross joint.
using AssemblyMotionRelationshipIdentity =
    std::variant<AssemblyLocalRelationshipIdentity,
                 AssemblyLocalJointIdentity,
                 AssemblyProjectCrossHierarchyRelationshipIdentity,
                 AssemblyProjectCrossHierarchyJointIdentity>;

struct AssemblyMotionRelationshipAuthorityIncidence {
  AssemblyMotionRelationshipIdentity relationship;
  ComponentTransformAuthority authority;

  friend bool operator==(const AssemblyMotionRelationshipAuthorityIncidence&,
                         const AssemblyMotionRelationshipAuthorityIncidence&) = default;
};

enum class AssemblyHierarchyJointEndpointRole { TargetA, TargetB };

struct AssemblyCrossHierarchyJointEndpointAuthorityMapping {
  AssemblyJointId joint;
  AssemblyHierarchyJointEndpointRole role;
  AssemblyHierarchyConstraintEndpoint endpoint;
  ComponentTransformAuthority authority;

  friend bool operator==(const AssemblyCrossHierarchyJointEndpointAuthorityMapping&,
                         const AssemblyCrossHierarchyJointEndpointAuthorityMapping&) = default;
};

struct AssemblyCrossHierarchyMotionGroup {
  std::vector<AssemblyMotionRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> authorities;

  friend bool operator==(const AssemblyCrossHierarchyMotionGroup&,
                         const AssemblyCrossHierarchyMotionGroup&) = default;
};

// Derived Core-only combined geometric/joint connectivity for Project-level
// occurrence-qualified motion. Visibility is ignored; inactive relationships and
// suppressed endpoint components/path boundaries do not participate.
class AssemblyCrossHierarchyMotionGraph {
public:
  [[nodiscard]] static Result<AssemblyCrossHierarchyMotionGraph> build(const Project& project);

  [[nodiscard]] const std::vector<AssemblyMotionRelationshipIdentity>& relationships() const
      noexcept;
  [[nodiscard]] const std::vector<ComponentTransformAuthority>& authorities() const noexcept;
  [[nodiscard]] const std::vector<AssemblyMotionRelationshipAuthorityIncidence>& incidences() const
      noexcept;
  [[nodiscard]] const std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping>&
  joint_endpoint_mappings() const noexcept;
  [[nodiscard]] const std::vector<AssemblyCrossHierarchyMotionGroup>& motion_groups() const noexcept;

  [[nodiscard]] std::size_t relationship_count() const noexcept;
  [[nodiscard]] std::size_t authority_count() const noexcept;
  [[nodiscard]] std::size_t incidence_count() const noexcept;
  [[nodiscard]] std::size_t joint_endpoint_mapping_count() const noexcept;
  [[nodiscard]] std::size_t motion_group_count() const noexcept;

private:
  AssemblyCrossHierarchyMotionGraph(
      std::vector<AssemblyMotionRelationshipIdentity> relationships,
      std::vector<ComponentTransformAuthority> authorities,
      std::vector<AssemblyMotionRelationshipAuthorityIncidence> incidences,
      std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping> joint_endpoint_mappings,
      std::vector<AssemblyCrossHierarchyMotionGroup> motion_groups);

  std::vector<AssemblyMotionRelationshipIdentity> relationships_;
  std::vector<ComponentTransformAuthority> authorities_;
  std::vector<AssemblyMotionRelationshipAuthorityIncidence> incidences_;
  std::vector<AssemblyCrossHierarchyJointEndpointAuthorityMapping> joint_endpoint_mappings_;
  std::vector<AssemblyCrossHierarchyMotionGroup> motion_groups_;
};

} // namespace blcad
