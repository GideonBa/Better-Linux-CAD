#include "blcad/core/assembly_leaf_occurrence.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] bool is_visible_active(const ComponentInstance& component) noexcept {
  return component.visibility() == ComponentVisibility::Visible &&
         component.suppression_state() == ComponentSuppressionState::Active;
}

[[nodiscard]] std::vector<const ComponentInstance*>
ordered_components(const AssemblyDocument& assembly) {
  std::vector<const ComponentInstance*> components;
  components.reserve(assembly.component_instances().size());
  for (const ComponentInstance& component : assembly.component_instances()) {
    components.push_back(&component);
  }
  std::sort(components.begin(), components.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->id().value() < rhs->id().value();
  });
  return components;
}

} // namespace

Result<std::vector<AssemblyLeafOccurrenceDescriptor>>
AssemblyLeafOccurrenceResolver::resolve(const Project& project) const {
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (hierarchy.has_error()) {
    return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(hierarchy.error());
  }

  std::vector<AssemblyLeafOccurrenceDescriptor> leaves;
  for (const auto& assembly_occurrence : hierarchy.value().occurrences()) {
    if (!assembly_occurrence.visible_path || !assembly_occurrence.active_path) {
      continue;
    }

    const AssemblyDocument* assembly =
        project.find_assembly_document(assembly_occurrence.assembly_document);
    if (assembly == nullptr) {
      return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(Error::validation(
          assembly_occurrence.assembly_document.value(),
          "assembly leaf occurrence must resolve to a project assembly document"));
    }

    for (const ComponentInstance* component : ordered_components(*assembly)) {
      if (!is_visible_active(*component)) {
        continue;
      }

      std::vector<RigidTransform> transforms{component->transform()};
      transforms.insert(transforms.end(),
                        assembly_occurrence.parent_transforms_inner_to_outer.begin(),
                        assembly_occurrence.parent_transforms_inner_to_outer.end());
      leaves.push_back(AssemblyLeafOccurrenceDescriptor{
          assembly_occurrence.assembly_document, assembly_occurrence.occurrence_path,
          component->id(), component->referenced_part_document(), std::move(transforms)});
    }
  }

  return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::success(std::move(leaves));
}

} // namespace blcad
