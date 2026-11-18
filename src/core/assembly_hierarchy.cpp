#include "blcad/core/assembly_hierarchy.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] std::vector<const SubassemblyInstance*>
ordered_subassembly_instances(const AssemblyDocument& assembly) {
  std::vector<const SubassemblyInstance*> instances;
  instances.reserve(assembly.subassembly_instances().size());
  for (const auto& instance : assembly.subassembly_instances()) {
    instances.push_back(&instance);
  }
  std::sort(instances.begin(), instances.end(),
            [](const auto* lhs, const auto* rhs) { return lhs->id().value() < rhs->id().value(); });
  return instances;
}

[[nodiscard]] Result<std::size_t> append_occurrence_tree(
    const Project& project, const AssemblyDocument& assembly,
    std::optional<DocumentId> parent_assembly, std::optional<SubassemblyInstanceId> via_subassembly,
    const std::vector<SubassemblyInstanceId>& path,
    const std::vector<RigidTransform>& parent_transforms_inner_to_outer, bool visible_path,
    bool active_path, std::vector<AssemblyHierarchyOccurrenceDescriptor>& occurrences,
    ModelStateId selected_state) {
  auto effective = assembly.evaluate_model_state(selected_state);
  if (!effective)
    return Result<std::size_t>::failure(effective.error());
  occurrences.push_back(AssemblyHierarchyOccurrenceDescriptor{
      assembly.id(), std::move(parent_assembly), std::move(via_subassembly), path,
      parent_transforms_inner_to_outer, visible_path, active_path, selected_state});

  for (const SubassemblyInstance* instance : ordered_subassembly_instances(effective.value())) {
    const AssemblyDocument* child =
        project.find_assembly_document(instance->referenced_assembly_document());
    if (child == nullptr) {
      return Result<std::size_t>::failure(Error::validation(
          instance->id().value(),
          "assembly hierarchy occurrence must resolve to a project-owned assembly document"));
    }

    std::vector<SubassemblyInstanceId> child_path = path;
    child_path.push_back(instance->id());

    std::vector<RigidTransform> child_parent_transforms = parent_transforms_inner_to_outer;
    child_parent_transforms.insert(child_parent_transforms.begin(), instance->transform());

    auto appended = append_occurrence_tree(
        project, *child, assembly.id(), instance->id(), child_path, child_parent_transforms,
        visible_path && project.design_subassembly_visible(
                            child_path, instance->visibility() == ComponentVisibility::Visible),
        active_path && instance->suppression_state() == ComponentSuppressionState::Active,
        occurrences, instance->selected_model_state());
    if (appended.has_error()) {
      return appended;
    }
  }

  return Result<std::size_t>::success(occurrences.size());
}

} // namespace

Result<AssemblyHierarchyTraversal> AssemblyHierarchyTraversal::build(const Project& project,
                                                                     ModelStateId root_state) {
  auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<AssemblyHierarchyTraversal>::failure(valid_structure.error());
  }

  if (root_state == primary_model_state_id() &&
      project.assembly().effective_model_state() != primary_model_state_id())
    root_state = project.assembly().effective_model_state();
  std::vector<AssemblyHierarchyOccurrenceDescriptor> occurrences;
  auto appended = append_occurrence_tree(project, project.assembly(), std::nullopt, std::nullopt,
                                         {}, {}, true, true, occurrences, std::move(root_state));
  if (appended.has_error()) {
    return Result<AssemblyHierarchyTraversal>::failure(appended.error());
  }

  return Result<AssemblyHierarchyTraversal>::success(
      AssemblyHierarchyTraversal(std::move(occurrences)));
}

AssemblyHierarchyTraversal::AssemblyHierarchyTraversal(
    std::vector<AssemblyHierarchyOccurrenceDescriptor> occurrences)
    : occurrences_(std::move(occurrences)) {}

const std::vector<AssemblyHierarchyOccurrenceDescriptor>&
AssemblyHierarchyTraversal::occurrences() const noexcept {
  return occurrences_;
}

std::size_t AssemblyHierarchyTraversal::occurrence_count() const noexcept {
  return occurrences_.size();
}

} // namespace blcad
