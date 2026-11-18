#include "blcad/core/assembly_leaf_occurrence.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] std::vector<const ComponentInstance*>
ordered_components(const AssemblyDocument& assembly) {
  std::vector<const ComponentInstance*> components;
  components.reserve(assembly.component_instances().size());
  for (const ComponentInstance& component : assembly.component_instances()) {
    components.push_back(&component);
  }
  std::sort(components.begin(), components.end(),
            [](const auto* lhs, const auto* rhs) { return lhs->id().value() < rhs->id().value(); });
  return components;
}

} // namespace

Result<std::vector<AssemblyLeafOccurrenceDescriptor>>
AssemblyLeafOccurrenceResolver::resolve(const Project& project,
                                        AssemblyLeafParticipation participation,
                                        ModelStateId root_state) const {
  if (participation != AssemblyLeafParticipation::VisibleActive &&
      participation != AssemblyLeafParticipation::PhysicalActive)
    return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(
        Error::validation(project.id().value(), "Unknown assembly leaf participation policy"));
  const bool physical = participation == AssemblyLeafParticipation::PhysicalActive;
  auto hierarchy = AssemblyHierarchyTraversal::build(project, std::move(root_state));
  if (hierarchy.has_error()) {
    return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(hierarchy.error());
  }

  std::vector<AssemblyLeafOccurrenceDescriptor> leaves;
  for (const auto& assembly_occurrence : hierarchy.value().occurrences()) {
    if (!assembly_occurrence.active_path || (!physical && !assembly_occurrence.visible_path)) {
      continue;
    }

    const AssemblyDocument* assembly =
        project.find_assembly_document(assembly_occurrence.assembly_document);
    if (assembly == nullptr) {
      return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(Error::validation(
          assembly_occurrence.assembly_document.value(),
          "assembly leaf occurrence must resolve to a project assembly document"));
    }

    auto effective = assembly->evaluate_model_state(assembly_occurrence.selected_model_state);
    if (!effective)
      return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(effective.error());
    for (const ComponentInstance* component : ordered_components(effective.value())) {
      if (component->suppression_state() != ComponentSuppressionState::Active ||
          (!physical && !project.design_component_visible(
                            assembly_occurrence.occurrence_path, component->id(),
                            component->visibility() == ComponentVisibility::Visible))) {
        continue;
      }

      auto effective = project.effective_component_transform(assembly_occurrence.occurrence_path,
                                                             component->id());
      if (!effective)
        return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::failure(effective.error());
      std::vector<RigidTransform> transforms{effective.value()};
      transforms.insert(transforms.end(),
                        assembly_occurrence.parent_transforms_inner_to_outer.begin(),
                        assembly_occurrence.parent_transforms_inner_to_outer.end());
      leaves.push_back(AssemblyLeafOccurrenceDescriptor{
          assembly_occurrence.assembly_document, assembly_occurrence.occurrence_path,
          component->id(), component->referenced_part_document(), std::move(transforms),
          component->selected_model_state()});
    }
  }

  return Result<std::vector<AssemblyLeafOccurrenceDescriptor>>::success(std::move(leaves));
}

} // namespace blcad
