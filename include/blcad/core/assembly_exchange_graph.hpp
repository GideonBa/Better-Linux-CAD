#pragma once

#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace blcad {

// Derived rooted assembly occurrence identity for presentation/exchange consumers.
// The explicit root occurrence is the empty path. The value is not persistent
// model authority and is regenerated from the validated Project hierarchy.
struct AssemblyExchangeAssemblyOccurrenceIdentity {
  std::vector<SubassemblyInstanceId> occurrence_path;

  friend bool operator==(const AssemblyExchangeAssemblyOccurrenceIdentity&,
                         const AssemblyExchangeAssemblyOccurrenceIdentity&) = default;
};

// Derived rooted component occurrence identity. ComponentInstanceId remains local
// to the AssemblyDocument reached by containing_assembly_occurrence.
struct AssemblyExchangeComponentOccurrenceIdentity {
  AssemblyExchangeAssemblyOccurrenceIdentity containing_assembly_occurrence;
  ComponentInstanceId component_instance;

  friend bool operator==(const AssemblyExchangeComponentOccurrenceIdentity&,
                         const AssemblyExchangeComponentOccurrenceIdentity&) = default;
};

// Shared part-product definition identity. Repeated component occurrences that
// reference one PartDocument map to this same derived exchange definition.
struct AssemblyExchangePartProductDefinitionIdentity {
  DocumentId part_document;

  friend bool operator==(const AssemblyExchangePartProductDefinitionIdentity&,
                         const AssemblyExchangePartProductDefinitionIdentity&) = default;
};

struct AssemblyExchangeAssemblyOccurrence {
  AssemblyExchangeAssemblyOccurrenceIdentity identity;
  DocumentId assembly_document;
  std::optional<AssemblyExchangeAssemblyOccurrenceIdentity> parent_identity;
  std::optional<SubassemblyInstanceId> via_subassembly_instance;
  RigidTransform transform_from_parent;
  std::string product_name;

  friend bool operator==(const AssemblyExchangeAssemblyOccurrence&,
                         const AssemblyExchangeAssemblyOccurrence&) = default;
};

struct AssemblyExchangeComponentOccurrence {
  AssemblyExchangeComponentOccurrenceIdentity identity;
  DocumentId assembly_document;
  AssemblyExchangePartProductDefinitionIdentity part_product;
  std::vector<RigidTransform> transforms_inner_to_outer;
  std::string occurrence_name;

  friend bool operator==(const AssemblyExchangeComponentOccurrence&,
                         const AssemblyExchangeComponentOccurrence&) = default;
};

struct AssemblyExchangePartProductDefinition {
  AssemblyExchangePartProductDefinitionIdentity identity;
  std::string product_name;

  friend bool operator==(const AssemblyExchangePartProductDefinition&,
                         const AssemblyExchangePartProductDefinition&) = default;
};

// Read-only deterministic exchange graph. Export inclusion is defined by the
// canonical visible-active AssemblyLeafOccurrenceResolver result: the root plus
// every assembly path prefix required by an exported leaf is retained. Component
// occurrence transform chains are copied verbatim from the leaf descriptors.
class AssemblyExchangeGraph {
public:
  [[nodiscard]] static Result<AssemblyExchangeGraph> build(const Project& project);

  [[nodiscard]] const std::vector<AssemblyExchangeAssemblyOccurrence>&
  assembly_occurrences() const noexcept;
  [[nodiscard]] const std::vector<AssemblyExchangeComponentOccurrence>&
  component_occurrences() const noexcept;
  [[nodiscard]] const std::vector<AssemblyExchangePartProductDefinition>&
  part_product_definitions() const noexcept;

  [[nodiscard]] std::size_t assembly_occurrence_count() const noexcept;
  [[nodiscard]] std::size_t component_occurrence_count() const noexcept;
  [[nodiscard]] std::size_t part_product_definition_count() const noexcept;

private:
  AssemblyExchangeGraph(
      std::vector<AssemblyExchangeAssemblyOccurrence> assembly_occurrences,
      std::vector<AssemblyExchangeComponentOccurrence> component_occurrences,
      std::vector<AssemblyExchangePartProductDefinition> part_product_definitions);

  std::vector<AssemblyExchangeAssemblyOccurrence> assembly_occurrences_;
  std::vector<AssemblyExchangeComponentOccurrence> component_occurrences_;
  std::vector<AssemblyExchangePartProductDefinition> part_product_definitions_;
};

} // namespace blcad
