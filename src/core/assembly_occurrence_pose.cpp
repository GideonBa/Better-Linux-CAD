#include "blcad/core/assembly_occurrence_pose.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace blcad {
namespace {
Error error(std::string message) {
  return Error::validation("assembly.occurrence_pose", std::move(message));
}
bool finite(const RigidTransform& t) {
  return std::isfinite(t.translation_mm.x) && std::isfinite(t.translation_mm.y) &&
         std::isfinite(t.translation_mm.z) && std::isfinite(t.rotation_deg.x) &&
         std::isfinite(t.rotation_deg.y) && std::isfinite(t.rotation_deg.z);
}
Result<const AssemblyDocument*> resolve(const Project& project,
                                        const std::vector<SubassemblyInstanceId>& path) {
  const AssemblyDocument* assembly = &project.assembly();
  for (const auto& segment : path) {
    const auto* instance = assembly->find_subassembly_instance(segment);
    if (!instance)
      return Result<const AssemblyDocument*>::failure(error("Occurrence path no longer resolves"));
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (!assembly)
      return Result<const AssemblyDocument*>::failure(
          error("Occurrence child definition is missing"));
  }
  return Result<const AssemblyDocument*>::success(assembly);
}
Result<std::size_t> validate(const Project& project, const AssemblyOccurrencePose& pose) {
  auto assembly = resolve(project, pose.path());
  if (!assembly)
    return Result<std::size_t>::failure(assembly.error());
  if (assembly.value()->id() != pose.assembly_document())
    return Result<std::size_t>::failure(
        error("Occurrence child identity changed; repair explicitly"));
  for (const auto& override : pose.components()) {
    const auto* component = assembly.value()->find_component_instance(override.component);
    if (!component)
      return Result<std::size_t>::failure(error("Occurrence component override is orphaned"));
    if (component->grounding_state() == ComponentGroundingState::Grounded &&
        component->transform() != override.transform)
      return Result<std::size_t>::failure(
          error("A grounded component retains its definition pose"));
  }
  for (const auto& coordinate : pose.joints()) {
    const auto* joint = assembly.value()->find_joint(coordinate.joint);
    if (!joint)
      return Result<std::size_t>::failure(error("Occurrence joint coordinate is orphaned"));
    auto updated = joint->with_coordinate_value(coordinate.role, coordinate.value);
    if (!updated)
      return Result<std::size_t>::failure(updated.error());
  }
  return Result<std::size_t>::success(pose.components().size() + pose.joints().size());
}
} // namespace

AssemblyOccurrencePose::AssemblyOccurrencePose(
    std::vector<SubassemblyInstanceId> path, DocumentId assembly_document,
    AssemblyOccurrenceRigidity rigidity, std::vector<AssemblyComponentPoseOverride> components,
    std::vector<AssemblyOccurrenceJointCoordinate> joints)
    : path_(std::move(path)), assembly_document_(std::move(assembly_document)), rigidity_(rigidity),
      components_(std::move(components)), joints_(std::move(joints)) {}

Result<AssemblyOccurrencePose>
AssemblyOccurrencePose::create(std::vector<SubassemblyInstanceId> path,
                               DocumentId assembly_document, AssemblyOccurrenceRigidity rigidity,
                               std::vector<AssemblyComponentPoseOverride> components,
                               std::vector<AssemblyOccurrenceJointCoordinate> joints) {
  using R = Result<AssemblyOccurrencePose>;
  if (path.empty() || assembly_document.value().empty() ||
      std::any_of(path.begin(), path.end(), [](const auto& id) { return id.value().empty(); }))
    return R::failure(error("Occurrence pose requires a non-root path and child identity"));
  if (rigidity != AssemblyOccurrenceRigidity::Rigid &&
      rigidity != AssemblyOccurrenceRigidity::Flexible)
    return R::failure(error("Unknown occurrence rigidity"));
  if (rigidity == AssemblyOccurrenceRigidity::Rigid && (!components.empty() || !joints.empty()))
    return R::failure(error("Rigid occurrences cannot own internal pose overrides"));
  std::sort(components.begin(), components.end(),
            [](const auto& a, const auto& b) { return a.component.value() < b.component.value(); });
  for (std::size_t i = 0; i < components.size(); ++i)
    if (components[i].component.value().empty() || !finite(components[i].transform) ||
        (i && components[i - 1].component == components[i].component))
      return R::failure(
          error("Component pose overrides require unique identities and finite values"));
  std::sort(joints.begin(), joints.end(), [](const auto& a, const auto& b) {
    if (a.joint != b.joint)
      return a.joint.value() < b.joint.value();
    return a.role < b.role;
  });
  for (std::size_t i = 0; i < joints.size(); ++i)
    if (joints[i].joint.value().empty() ||
        (i && joints[i - 1].joint == joints[i].joint && joints[i - 1].role == joints[i].role))
      return R::failure(error("Occurrence joint coordinates require unique identities and roles"));
  return R::success(AssemblyOccurrencePose(std::move(path), std::move(assembly_document), rigidity,
                                           std::move(components), std::move(joints)));
}

const AssemblyOccurrencePose*
Project::find_occurrence_pose(const std::vector<SubassemblyInstanceId>& path) const noexcept {
  const auto it = std::find_if(occurrence_poses_.begin(), occurrence_poses_.end(),
                               [&](const auto& pose) { return pose.path() == path; });
  return it == occurrence_poses_.end() ? nullptr : &*it;
}
bool Project::flexible_occurrence(const std::vector<SubassemblyInstanceId>& path) const noexcept {
  const auto* pose = find_occurrence_pose(path);
  return pose && pose->rigidity() == AssemblyOccurrenceRigidity::Flexible;
}
Result<std::size_t> Project::set_occurrence_pose(AssemblyOccurrencePose pose) {
  auto valid = validate(*this, pose);
  if (!valid)
    return valid;
  const auto it = std::find_if(occurrence_poses_.begin(), occurrence_poses_.end(),
                               [&](const auto& current) { return current.path() == pose.path(); });
  if (it == occurrence_poses_.end())
    occurrence_poses_.push_back(std::move(pose));
  else
    *it = std::move(pose);
  std::sort(occurrence_poses_.begin(), occurrence_poses_.end(), [](const auto& a, const auto& b) {
    return std::lexicographical_compare(
        a.path().begin(), a.path().end(), b.path().begin(), b.path().end(),
        [](const auto& x, const auto& y) { return x.value() < y.value(); });
  });
  return Result<std::size_t>::success(1);
}
Result<std::size_t>
Project::remove_occurrence_pose(const std::vector<SubassemblyInstanceId>& path) {
  const auto count =
      std::erase_if(occurrence_poses_, [&](const auto& pose) { return pose.path() == path; });
  if (!count)
    return Result<std::size_t>::failure(error("Occurrence pose does not exist"));
  return Result<std::size_t>::success(count);
}
Result<std::size_t> Project::validate_occurrence_poses() const {
  for (const auto& pose : occurrence_poses_) {
    auto valid = validate(*this, pose);
    if (!valid)
      return valid;
  }
  return Result<std::size_t>::success(occurrence_poses_.size());
}
Result<RigidTransform>
Project::effective_component_transform(const std::vector<SubassemblyInstanceId>& path,
                                       const ComponentInstanceId& component) const {
  auto assembly = resolve(*this, path);
  if (!assembly)
    return Result<RigidTransform>::failure(assembly.error());
  const auto* instance = assembly.value()->find_component_instance(component);
  if (!instance)
    return Result<RigidTransform>::failure(error("Occurrence component does not exist"));
  if (const auto* pose = find_occurrence_pose(path)) {
    auto valid = validate(*this, *pose);
    if (!valid)
      return Result<RigidTransform>::failure(valid.error());
    for (const auto& override : pose->components())
      if (override.component == component)
        return Result<RigidTransform>::success(override.transform);
  }
  return Result<RigidTransform>::success(instance->transform());
}
Result<std::size_t>
Project::set_occurrence_component_transform(const std::vector<SubassemblyInstanceId>& path,
                                            ComponentInstanceId component,
                                            RigidTransform transform) {
  const auto* current = find_occurrence_pose(path);
  if (!current || current->rigidity() != AssemblyOccurrenceRigidity::Flexible)
    return Result<std::size_t>::failure(error("An explicit flexible occurrence is required"));
  auto components = current->components();
  const auto it = std::find_if(components.begin(), components.end(),
                               [&](const auto& item) { return item.component == component; });
  if (it == components.end())
    components.push_back({std::move(component), transform});
  else
    it->transform = transform;
  auto pose =
      AssemblyOccurrencePose::create(path, current->assembly_document(), current->rigidity(),
                                     std::move(components), current->joints());
  if (!pose)
    return Result<std::size_t>::failure(pose.error());
  return set_occurrence_pose(std::move(pose.value()));
}

Result<std::size_t>
Project::set_occurrence_joint_coordinate(const std::vector<SubassemblyInstanceId>& path,
                                         AssemblyJointId joint, AssemblyJointCoordinateRole role,
                                         Quantity value) {
  const auto* current = find_occurrence_pose(path);
  if (!current || !flexible_occurrence(path))
    return Result<std::size_t>::failure(error("An explicit flexible occurrence is required"));
  auto joints = current->joints();
  const auto it = std::find_if(joints.begin(), joints.end(), [&](const auto& item) {
    return item.joint == joint && item.role == role;
  });
  if (it == joints.end())
    joints.push_back({std::move(joint), role, value});
  else
    it->value = value;
  auto pose =
      AssemblyOccurrencePose::create(path, current->assembly_document(), current->rigidity(),
                                     current->components(), std::move(joints));
  if (!pose)
    return Result<std::size_t>::failure(pose.error());
  return set_occurrence_pose(std::move(pose.value()));
}

std::vector<std::vector<SubassemblyInstanceId>>
assembly_transform_scopes(const Project& project, const DocumentId& document) {
  std::vector<std::vector<SubassemblyInstanceId>> result;
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  bool shared = false, seen = false, extended = false;
  if (hierarchy)
    for (const auto& occurrence : hierarchy.value().occurrences()) {
      if (occurrence.assembly_document != document)
        continue;
      seen = true;
      extended = extended || project.flexible_occurrence(occurrence.occurrence_path) ||
                 occurrence.selected_model_state != primary_model_state_id();
      if (!occurrence.active_path)
        continue;
      if (project.flexible_occurrence(occurrence.occurrence_path) ||
          occurrence.selected_model_state != primary_model_state_id())
      result.push_back(occurrence.occurrence_path);
      else
        shared = true;
    }
  if (!extended)
    return {std::vector<SubassemblyInstanceId>{}};
  if (shared || !seen)
    result.insert(result.begin(), std::vector<SubassemblyInstanceId>{});
  return result;
}
} // namespace blcad
