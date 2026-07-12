#include "blcad/core/project.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Result<const AssemblyDocument*> resolve_joint_endpoint_assembly(
    const Project& project, const AssemblyHierarchyConstraintEndpoint& endpoint,
    const AssemblyJointId& joint_id, std::string_view target_label) {
  const AssemblyDocument* assembly = &project.assembly();
  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    const SubassemblyInstance* instance = assembly->find_subassembly_instance(occurrence);
    if (instance == nullptr) {
      return Result<const AssemblyDocument*>::failure(Error::validation(
          joint_id.value(), "cross-hierarchy assembly joint target " + std::string(target_label) +
                                " occurrence path must exist in rooted hierarchy"));
    }
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (assembly == nullptr) {
      return Result<const AssemblyDocument*>::failure(Error::validation(
          joint_id.value(), "cross-hierarchy assembly joint target " + std::string(target_label) +
                                " occurrence path must exist in rooted hierarchy"));
    }
  }
  return Result<const AssemblyDocument*>::success(assembly);
}

[[nodiscard]] Result<std::size_t> validate_joint_endpoint(
    const Project& project, const AssemblyHierarchyConstraintEndpoint& endpoint,
    const AssemblyJointId& joint_id, std::string_view target_label) {
  auto assembly = resolve_joint_endpoint_assembly(project, endpoint, joint_id, target_label);
  if (assembly.has_error()) {
    return Result<std::size_t>::failure(assembly.error());
  }
  if (assembly.value()->find_component_instance(endpoint.component_instance()) == nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        joint_id.value(), "cross-hierarchy assembly joint target " + std::string(target_label) +
                              " component instance must exist in reached assembly document"));
  }
  return Result<std::size_t>::success(1U);
}

} // namespace

Result<std::size_t> Project::add_cross_hierarchy_joint(AssemblyHierarchyJoint joint) {
  if (find_cross_hierarchy_joint(joint.id()) != nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        joint.id().value(), "cross-hierarchy assembly joint id must be unique within project"));
  }
  cross_hierarchy_joints_.push_back(std::move(joint));
  return Result<std::size_t>::success(cross_hierarchy_joints_.size() - 1U);
}

Result<std::size_t> Project::validate_cross_hierarchy_joints() const {
  std::vector<AssemblyJointId> seen_ids;
  seen_ids.reserve(cross_hierarchy_joints_.size());

  std::size_t count = 0U;
  for (const AssemblyHierarchyJoint& joint : cross_hierarchy_joints_) {
    if (std::find(seen_ids.begin(), seen_ids.end(), joint.id()) != seen_ids.end()) {
      return Result<std::size_t>::failure(Error::validation(
          joint.id().value(), "cross-hierarchy assembly joint id must be unique within project"));
    }
    seen_ids.push_back(joint.id());

    auto target_a = validate_joint_endpoint(*this, joint.target_a(), joint.id(), "A");
    if (target_a.has_error()) {
      return Result<std::size_t>::failure(target_a.error());
    }
    auto target_b = validate_joint_endpoint(*this, joint.target_b(), joint.id(), "B");
    if (target_b.has_error()) {
      return Result<std::size_t>::failure(target_b.error());
    }
    ++count;
  }
  return Result<std::size_t>::success(count);
}

Result<std::size_t> Project::set_cross_hierarchy_joint_coordinate(AssemblyJointId id,
                                                                  Quantity coordinate) {
  AssemblyHierarchyJoint* joint = find_cross_hierarchy_joint(id);
  if (joint == nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        id.empty() ? std::string("assembly_hierarchy_joint") : id.value(),
        "cross-hierarchy assembly joint must exist in project"));
  }
  auto updated = joint->with_coordinate(coordinate);
  if (updated.has_error()) {
    return Result<std::size_t>::failure(updated.error());
  }
  *joint = std::move(updated.value());
  return Result<std::size_t>::success(1U);
}

const std::vector<AssemblyHierarchyJoint>& Project::cross_hierarchy_joints() const noexcept {
  return cross_hierarchy_joints_;
}

std::vector<AssemblyHierarchyJoint>& Project::cross_hierarchy_joints() noexcept {
  return cross_hierarchy_joints_;
}

std::size_t Project::cross_hierarchy_joint_count() const noexcept {
  return cross_hierarchy_joints_.size();
}

const AssemblyHierarchyJoint* Project::find_cross_hierarchy_joint(AssemblyJointId id) const noexcept {
  const auto found = std::find_if(
      cross_hierarchy_joints_.begin(), cross_hierarchy_joints_.end(),
      [&id](const AssemblyHierarchyJoint& joint) { return joint.id() == id; });
  return found == cross_hierarchy_joints_.end() ? nullptr : &*found;
}

AssemblyHierarchyJoint* Project::find_cross_hierarchy_joint(AssemblyJointId id) noexcept {
  const auto found = std::find_if(
      cross_hierarchy_joints_.begin(), cross_hierarchy_joints_.end(),
      [&id](const AssemblyHierarchyJoint& joint) { return joint.id() == id; });
  return found == cross_hierarchy_joints_.end() ? nullptr : &*found;
}

} // namespace blcad
