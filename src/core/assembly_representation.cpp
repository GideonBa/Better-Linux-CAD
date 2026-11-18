#include "blcad/core/assembly_representation.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"

#include <algorithm>
#include <set>
#include <type_traits>

namespace blcad {
namespace {
Error error(std::string message) {
  return Error::validation("assembly.representation", std::move(message));
}
std::string path_key(const std::vector<SubassemblyInstanceId>& path) {
  std::string key = std::to_string(path.size()) + ":";
  for (const auto& id : path)
    key += std::to_string(id.value().size()) + ":" + id.value() + "/";
  return key;
}
Result<const AssemblyDocument*> resolve(const Project& project,
                                        const std::vector<SubassemblyInstanceId>& path) {
  const auto* assembly = &project.assembly();
  for (const auto& id : path) {
    const auto* instance = assembly->find_subassembly_instance(id);
    if (!instance)
      return Result<const AssemblyDocument*>::failure(
          error("Representation occurrence path is orphaned"));
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (!assembly)
      return Result<const AssemblyDocument*>::failure(
          error("Representation child definition is missing"));
  }
  return Result<const AssemblyDocument*>::success(assembly);
}
Result<Project> apply_position(const Project& source, const AssemblyPosition& position) {
  Project candidate = source;
  std::set<std::string> components, subassemblies, poses, coordinates;
  for (const auto& value : position.components) {
    const auto* component = candidate.assembly().find_component_instance(value.component);
    if (!component || component->referenced_part_document() != value.part ||
        !components.insert(value.component.value()).second)
      return Result<Project>::failure(
          error("Position component is duplicate, missing or retargeted"));
    auto applied =
        candidate.assembly().set_component_instance_transform(value.component, value.transform);
    if (!applied)
      return Result<Project>::failure(applied.error());
  }
  for (const auto& value : position.subassemblies) {
    const auto* instance = candidate.assembly().find_subassembly_instance(value.subassembly);
    if (!instance || instance->referenced_assembly_document() != value.assembly_document ||
        !subassemblies.insert(value.subassembly.value()).second)
      return Result<Project>::failure(
          error("Position subassembly is duplicate, missing or retargeted"));
    auto applied =
        candidate.assembly().set_subassembly_instance_transform(value.subassembly, value.transform);
    if (!applied)
      return Result<Project>::failure(applied.error());
  }
  const auto previous = candidate.occurrence_poses();
  for (const auto& pose : previous) {
    auto removed = candidate.remove_occurrence_pose(pose.path());
    if (!removed)
      return Result<Project>::failure(removed.error());
  }
  for (const auto& pose : position.poses) {
    if (!poses.insert(path_key(pose.path())).second)
      return Result<Project>::failure(error("Position has duplicate occurrence pose authorities"));
    auto applied = candidate.set_occurrence_pose(pose);
    if (!applied)
      return Result<Project>::failure(applied.error());
  }
  for (const auto& value : position.joints) {
    const auto key = value.joint.value() + "/" + std::string(to_string(value.role));
    if (!coordinates.insert(key).second)
      return Result<Project>::failure(error("Position has duplicate joint coordinates"));
    auto applied =
        candidate.assembly().set_joint_coordinate_value(value.joint, value.role, value.value);
    if (!applied)
      return Result<Project>::failure(applied.error());
  }
  coordinates.clear();
  for (const auto& value : position.cross_joints) {
    const auto key = value.joint.value() + "/" + std::string(to_string(value.role));
    if (!coordinates.insert(key).second)
      return Result<Project>::failure(error("Position has duplicate hierarchy joint coordinates"));
    auto applied =
        candidate.set_cross_hierarchy_joint_coordinate_value(value.joint, value.role, value.value);
    if (!applied)
      return Result<Project>::failure(applied.error());
  }
  return Result<Project>::success(std::move(candidate));
}
} // namespace
Result<AssemblyPosition> capture_assembly_position(const Project& project, AssemblyPositionId id,
                                                   std::string name) {
  if (id.empty() || id == master_position_id() || name.empty())
    return Result<AssemblyPosition>::failure(
        error("A named position requires a unique ID and name"));
  AssemblyPosition position{
      std::move(id), std::move(name), {}, {}, project.occurrence_poses(), {}, {}};
  for (const auto& value : project.assembly().component_instances())
    position.components.push_back(
        {value.id(), value.referenced_part_document(), value.transform()});
  for (const auto& value : project.assembly().subassembly_instances())
    position.subassemblies.push_back(
        {value.id(), value.referenced_assembly_document(), value.transform()});
  for (const auto& joint : project.assembly().joints())
    for (const auto& slot : joint.coordinate_slots())
      position.joints.push_back({joint.id(), slot.role(), slot.value()});
  for (const auto& joint : project.cross_hierarchy_joints())
    for (const auto& slot : joint.coordinate_slots())
      position.cross_joints.push_back({joint.id(), slot.role(), slot.value()});
  auto checked = apply_position(project, position);
  if (!checked)
    return Result<AssemblyPosition>::failure(checked.error());
  return Result<AssemblyPosition>::success(std::move(position));
}
Result<AssemblyDesignView> capture_assembly_design_view(const Project& project,
                                                        AssemblyDesignViewId id, std::string name) {
  if (id.empty() || id == default_design_view_id() || name.empty())
    return Result<AssemblyDesignView>::failure(
        error("A named design view requires a unique ID and name"));
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (!hierarchy)
    return Result<AssemblyDesignView>::failure(hierarchy.error());
  AssemblyDesignView view{std::move(id), std::move(name), {}, {}};
  for (const auto& occurrence : hierarchy.value().occurrences()) {
    const auto* assembly = project.find_assembly_document(occurrence.assembly_document);
    if (!occurrence.occurrence_path.empty()) {
      const auto* parent = project.find_assembly_document(*occurrence.parent_assembly_document);
      const auto* instance =
          parent->find_subassembly_instance(*occurrence.via_subassembly_instance);
      view.subassemblies.push_back({occurrence.occurrence_path, assembly->id(),
                                    project.design_subassembly_visible(
                                        occurrence.occurrence_path,
                                        instance->visibility() == ComponentVisibility::Visible)});
    }
    for (const auto& component : assembly->component_instances())
      view.components.push_back(
          {{occurrence.occurrence_path, component.id(), component.referenced_part_document()},
           project.design_component_visible(occurrence.occurrence_path, component.id(),
                                            component.visibility() ==
                                                ComponentVisibility::Visible)});
  }
  return Result<AssemblyDesignView>::success(std::move(view));
}
const AssemblyPosition* Project::find_assembly_position(AssemblyPositionId id) const noexcept {
  const auto it = std::find_if(assembly_positions_.begin(), assembly_positions_.end(),
                               [&](const auto& value) { return value.id == id; });
  return it == assembly_positions_.end() ? nullptr : &*it;
}
const AssemblyDesignView*
Project::find_assembly_design_view(AssemblyDesignViewId id) const noexcept {
  const auto it = std::find_if(assembly_design_views_.begin(), assembly_design_views_.end(),
                               [&](const auto& value) { return value.id == id; });
  return it == assembly_design_views_.end() ? nullptr : &*it;
}
Result<std::size_t> Project::replace_assembly_positions(std::vector<AssemblyPosition> values) {
  if (assembly().effective_model_state() != primary_model_state_id() ||
      effective_position_ != master_position_id() ||
      effective_design_view_ != default_design_view_id())
    return Result<std::size_t>::failure(
        error("Edit representation catalogs on the authoritative Primary / "
              "Master / Default Project"));
  Project candidate = *this;
  candidate.assembly_positions_ = std::move(values);
  if (candidate.effective_position_ != master_position_id() &&
      !candidate.find_assembly_position(candidate.effective_position_))
    candidate.effective_position_ = master_position_id();
  auto checked = candidate.validate_assembly_representations();
  if (!checked)
    return checked;
  *this = std::move(candidate);
  return Result<std::size_t>::success(assembly_positions_.size());
}
Result<std::size_t> Project::replace_assembly_design_views(std::vector<AssemblyDesignView> values) {
  if (assembly().effective_model_state() != primary_model_state_id() ||
      effective_position_ != master_position_id() ||
      effective_design_view_ != default_design_view_id())
    return Result<std::size_t>::failure(
        error("Edit representation catalogs on the authoritative Primary / "
              "Master / Default Project"));
  Project candidate = *this;
  candidate.assembly_design_views_ = std::move(values);
  if (candidate.effective_design_view_ != default_design_view_id() &&
      !candidate.find_assembly_design_view(candidate.effective_design_view_))
    candidate.effective_design_view_ = default_design_view_id();
  auto checked = candidate.validate_assembly_representations();
  if (!checked)
    return checked;
  *this = std::move(candidate);
  return Result<std::size_t>::success(assembly_design_views_.size());
}
Result<std::size_t> Project::validate_assembly_representations() const {
  std::set<std::string> ids, names;
  for (const auto& position : assembly_positions_) {
    if (position.id.empty() || position.id == master_position_id() || position.name.empty() ||
        !ids.insert(position.id.value()).second || !names.insert(position.name).second)
      return Result<std::size_t>::failure(
          error("Named position identities/names must be unique and cannot "
                "replace Master"));
    auto checked = apply_position(*this, position);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
  }
  ids.clear();
  names.clear();
  for (const auto& view : assembly_design_views_) {
    if (view.id.empty() || view.id == default_design_view_id() || view.name.empty() ||
        !ids.insert(view.id.value()).second || !names.insert(view.name).second)
      return Result<std::size_t>::failure(
          error("Design view identities/names must be unique and cannot "
                "replace Default"));
    std::set<std::string> components, subassemblies;
    for (const auto& value : view.components) {
      auto assembly = resolve(*this, value.occurrence.path);
      if (!assembly)
        return Result<std::size_t>::failure(assembly.error());
      const auto* component = assembly.value()->find_component_instance(value.occurrence.component);
      if (!component || component->referenced_part_document() != value.occurrence.part ||
          !components.insert(path_key(value.occurrence.path) + value.occurrence.component.value())
               .second)
        return Result<std::size_t>::failure(
            error("Design view component is duplicate, missing or retargeted"));
    }
    for (const auto& value : view.subassemblies) {
      auto assembly = resolve(*this, value.path);
      if (!assembly)
        return Result<std::size_t>::failure(assembly.error());
      if (value.path.empty() || assembly.value()->id() != value.assembly_document ||
          !subassemblies.insert(path_key(value.path)).second)
        return Result<std::size_t>::failure(
            error("Design view subassembly is duplicate, missing or retargeted"));
    }
  }
  return Result<std::size_t>::success(assembly_positions_.size() + assembly_design_views_.size());
}
Result<Project> Project::evaluate_assembly_position(AssemblyPositionId id) const {
  if (effective_position_ != master_position_id()) {
    if (id == effective_position_)
      return Result<Project>::success(*this);
    return Result<Project>::failure(
        error("Switch positions from the authoritative Master Project, not an "
              "evaluated position"));
  }
  if (id == master_position_id()) {
    auto evaluated = *this;
    evaluated.effective_position_ = id;
    return Result<Project>::success(std::move(evaluated));
  }
  const auto* position = find_assembly_position(id);
  if (!position)
    return Result<Project>::failure(error("Named position is missing"));
  auto evaluated = apply_position(*this, *position);
  if (evaluated)
    evaluated.value().effective_position_ = std::move(id);
  return evaluated;
}
Result<Project> Project::evaluate_assembly_design_view(AssemblyDesignViewId id) const {
  if (id != default_design_view_id() && !find_assembly_design_view(id))
    return Result<Project>::failure(error("Named design view is missing"));
  Project evaluated = *this;
  evaluated.effective_design_view_ = std::move(id);
  return Result<Project>::success(std::move(evaluated));
}
bool Project::design_component_visible(const std::vector<SubassemblyInstanceId>& path,
                                       ComponentInstanceId component,
                                       bool inherited) const noexcept {
  const auto* view = find_assembly_design_view(effective_design_view_);
  if (view)
    for (const auto& value : view->components)
      if (value.occurrence.path == path && value.occurrence.component == component)
        return value.visible;
  return inherited;
}
bool Project::design_subassembly_visible(const std::vector<SubassemblyInstanceId>& path,
                                         bool inherited) const noexcept {
  const auto* view = find_assembly_design_view(effective_design_view_);
  if (view)
    for (const auto& value : view->subassemblies)
      if (value.path == path)
        return value.visible;
  return inherited;
}
} // namespace blcad
