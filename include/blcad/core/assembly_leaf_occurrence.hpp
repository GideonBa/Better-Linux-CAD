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

enum class AssemblyLeafParticipation { VisibleActive, PhysicalActive };

// The default preserves visible-active presentation/export. PhysicalActive also
// includes hidden active paths/components; suppression always excludes a leaf.
class AssemblyLeafOccurrenceResolver {
public:
  [[nodiscard]] Result<std::vector<AssemblyLeafOccurrenceDescriptor>>
  resolve(const Project& project,
          AssemblyLeafParticipation participation = AssemblyLeafParticipation::VisibleActive) const;
};

} // namespace blcad
