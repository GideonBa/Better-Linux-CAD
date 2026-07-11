#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace blcad {

class Project;

// Derived identity of one persistent direct component-transform authority.
// Repeated rooted occurrences of one child AssemblyDocument can map to the same
// authority; the identity itself is not an additional persisted model record.
struct ComponentTransformAuthority {
  DocumentId assembly_document;
  ComponentInstanceId component_instance;

  friend bool operator==(const ComponentTransformAuthority&,
                         const ComponentTransformAuthority&) = default;
};

// One active local relationship, identified in the scope that owns its intent.
struct AssemblyLocalRelationshipIdentity {
  DocumentId assembly_document;
  AssemblyConstraintId constraint;

  friend bool operator==(const AssemblyLocalRelationshipIdentity&,
                         const AssemblyLocalRelationshipIdentity&) = default;
};

// One active project-level occurrence-qualified geometric relationship.
struct AssemblyProjectCrossHierarchyRelationshipIdentity {
  AssemblyConstraintId constraint;

  friend bool operator==(const AssemblyProjectCrossHierarchyRelationshipIdentity&,
                         const AssemblyProjectCrossHierarchyRelationshipIdentity&) = default;
};

using AssemblyRelationshipIdentity =
    std::variant<AssemblyLocalRelationshipIdentity,
                 AssemblyProjectCrossHierarchyRelationshipIdentity>;

enum class AssemblyHierarchyConstraintEndpointRole { TargetA, TargetB };

// Unique bipartite incidence edge. A relationship has at most one edge to one
// transform authority even when both of its geometric endpoints map there.
struct AssemblyRelationshipAuthorityIncidence {
  AssemblyRelationshipIdentity relationship;
  ComponentTransformAuthority authority;

  friend bool operator==(const AssemblyRelationshipAuthorityIncidence&,
                         const AssemblyRelationshipAuthorityIncidence&) = default;
};

// Occurrence-specific endpoint context retained separately from unique authority
// incidence. Target A and B therefore remain distinct even when they share one
// ComponentTransformAuthority.
struct AssemblyCrossHierarchyEndpointAuthorityMapping {
  AssemblyConstraintId constraint;
  AssemblyHierarchyConstraintEndpointRole role;
  AssemblyHierarchyConstraintEndpoint endpoint;
  ComponentTransformAuthority authority;

  friend bool operator==(const AssemblyCrossHierarchyEndpointAuthorityMapping&,
                         const AssemblyCrossHierarchyEndpointAuthorityMapping&) = default;
};

// One connected active incidence component containing at least one project-level
// cross-hierarchy relationship. Pure-local components are intentionally omitted.
struct AssemblyCrossHierarchySolveGroup {
  std::vector<AssemblyRelationshipIdentity> relationships;
  std::vector<ComponentTransformAuthority> authorities;

  friend bool operator==(const AssemblyCrossHierarchySolveGroup&,
                         const AssemblyCrossHierarchySolveGroup&) = default;
};

// Derived Core-only relationship-to-transform-authority connectivity. No target
// geometry, residual, Jacobian, solve state, or graph cache is persisted.
class AssemblyCrossHierarchyConstraintGraph {
public:
  [[nodiscard]] static Result<AssemblyCrossHierarchyConstraintGraph>
  build(const Project& project);

  [[nodiscard]] const std::vector<AssemblyRelationshipIdentity>& relationships() const noexcept;
  [[nodiscard]] const std::vector<ComponentTransformAuthority>& authorities() const noexcept;
  [[nodiscard]] const std::vector<AssemblyRelationshipAuthorityIncidence>& incidences() const
      noexcept;
  [[nodiscard]] const std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping>&
  endpoint_mappings() const noexcept;
  [[nodiscard]] const std::vector<AssemblyCrossHierarchySolveGroup>& solve_groups() const noexcept;

  [[nodiscard]] std::size_t relationship_count() const noexcept;
  [[nodiscard]] std::size_t authority_count() const noexcept;
  [[nodiscard]] std::size_t incidence_count() const noexcept;
  [[nodiscard]] std::size_t endpoint_mapping_count() const noexcept;
  [[nodiscard]] std::size_t solve_group_count() const noexcept;

private:
  AssemblyCrossHierarchyConstraintGraph(
      std::vector<AssemblyRelationshipIdentity> relationships,
      std::vector<ComponentTransformAuthority> authorities,
      std::vector<AssemblyRelationshipAuthorityIncidence> incidences,
      std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping> endpoint_mappings,
      std::vector<AssemblyCrossHierarchySolveGroup> solve_groups);

  std::vector<AssemblyRelationshipIdentity> relationships_;
  std::vector<ComponentTransformAuthority> authorities_;
  std::vector<AssemblyRelationshipAuthorityIncidence> incidences_;
  std::vector<AssemblyCrossHierarchyEndpointAuthorityMapping> endpoint_mappings_;
  std::vector<AssemblyCrossHierarchySolveGroup> solve_groups_;
};

} // namespace blcad
