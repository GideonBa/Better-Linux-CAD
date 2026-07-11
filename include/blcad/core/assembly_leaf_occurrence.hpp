#pragma once

#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <vector>

namespace blcad {

// Derived visible-active leaf occurrence ready for geometry consumers. Identity,
// path, and authored transform levels are preserved; no composed matrix or shape
// is stored in Project or AssemblyDocument.
struct AssemblyLeafOccurrenceDescriptor {
  DocumentId assembly_document;
  std::vector<SubassemblyInstanceId> subassembly_path;
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  std::vector<RigidTransform> transforms_inner_to_outer;

  friend bool operator==(const AssemblyLeafOccurrenceDescriptor&,
                         const AssemblyLeafOccurrenceDescriptor&) = default;
};

// Flattens visible active component leaves from the deterministic root hierarchy.
// Hidden/suppressed subassembly paths and hidden/suppressed components are absent.
class AssemblyLeafOccurrenceResolver {
public:
  [[nodiscard]] Result<std::vector<AssemblyLeafOccurrenceDescriptor>>
  resolve(const Project& project) const;
};

} // namespace blcad
