#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace blcad {

// One derived occurrence of an AssemblyDocument in the rooted project hierarchy.
// The root occurrence has no parent/via occurrence and an empty transform chain.
// Child paths are root-to-current ids; parent transforms are inner-to-outer so a
// leaf component transform can be prepended and evaluated sequentially.
struct AssemblyHierarchyOccurrenceDescriptor {
  DocumentId assembly_document;
  std::optional<DocumentId> parent_assembly_document;
  std::optional<SubassemblyInstanceId> via_subassembly_instance;
  std::vector<SubassemblyInstanceId> occurrence_path;
  std::vector<RigidTransform> parent_transforms_inner_to_outer;
  bool visible_path = true;
  bool active_path = true;

  friend bool operator==(const AssemblyHierarchyOccurrenceDescriptor&,
                         const AssemblyHierarchyOccurrenceDescriptor&) = default;
};

// Read-only deterministic pre-order DFS of the rigid assembly occurrence tree.
// Child occurrences are visited lexicographically by SubassemblyInstanceId.
// Repeated occurrences of one child AssemblyDocument remain distinct entries.
class AssemblyHierarchyTraversal {
public:
  [[nodiscard]] static Result<AssemblyHierarchyTraversal> build(const Project& project);

  [[nodiscard]] const std::vector<AssemblyHierarchyOccurrenceDescriptor>& occurrences() const
      noexcept;
  [[nodiscard]] std::size_t occurrence_count() const noexcept;

private:
  explicit AssemblyHierarchyTraversal(
      std::vector<AssemblyHierarchyOccurrenceDescriptor> occurrences);

  std::vector<AssemblyHierarchyOccurrenceDescriptor> occurrences_;
};

} // namespace blcad
