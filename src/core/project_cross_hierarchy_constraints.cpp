#include "blcad/core/project.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Result<const AssemblyDocument*> resolve_endpoint_assembly(
    const Project& project,
    const AssemblyHierarchyConstraintEndpoint& endpoint,
    const AssemblyConstraintId& constraint_id,
    std::string_view target_label) {
  const AssemblyDocument* assembly = &project.assembly();
  for (const SubassemblyInstanceId& occurrence : endpoint.occurrence_path()) {
    const SubassemblyInstance* instance = assembly->find_subassembly_instance(occurrence);
    if (instance == nullptr) {
      return Result<const AssemblyDocument*>::failure(Error::validation(
          constraint_id.value(),
          "cross-hierarchy assembly constraint target " + std::string(target_label) +
              " occurrence path must exist in rooted hierarchy"));
    }

    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (assembly == nullptr) {
      return Result<const AssemblyDocument*>::failure(Error::validation(
          constraint_id.value(),
          "cross-hierarchy assembly constraint target " + std::string(target_label) +
              " occurrence path must exist in rooted hierarchy"));
    }
  }
  return Result<const AssemblyDocument*>::success(assembly);
}

[[nodiscard]] Result<std::size_t> validate_endpoint(
    const Project& project,
    const AssemblyHierarchyConstraintEndpoint& endpoint,
    const AssemblyConstraintId& constraint_id,
    std::string_view target_label) {
  auto assembly = resolve_endpoint_assembly(project, endpoint, constraint_id, target_label);
  if (assembly.has_error()) {
    return Result<std::size_t>::failure(assembly.error());
  }
  if (assembly.value()->find_component_instance(endpoint.component_instance()) == nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        constraint_id.value(),
        "cross-hierarchy assembly constraint target " + std::string(target_label) +
            " component instance must exist in reached assembly document"));
  }
  return Result<std::size_t>::success(1U);
}

} // namespace

Result<std::size_t>
Project::add_cross_hierarchy_constraint(AssemblyHierarchyConstraint constraint) {
  if (find_cross_hierarchy_constraint(constraint.id()) != nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        constraint.id().value(),
        "cross-hierarchy assembly constraint id must be unique within project"));
  }
  cross_hierarchy_constraints_.push_back(std::move(constraint));
  return Result<std::size_t>::success(cross_hierarchy_constraints_.size() - 1U);
}

Result<std::size_t> Project::validate_cross_hierarchy_constraints() const {
  std::vector<AssemblyConstraintId> seen_ids;
  seen_ids.reserve(cross_hierarchy_constraints_.size());

  std::size_t count = 0U;
  for (const AssemblyHierarchyConstraint& constraint : cross_hierarchy_constraints_) {
    if (std::find(seen_ids.begin(), seen_ids.end(), constraint.id()) != seen_ids.end()) {
      return Result<std::size_t>::failure(Error::validation(
          constraint.id().value(),
          "cross-hierarchy assembly constraint id must be unique within project"));
    }
    seen_ids.push_back(constraint.id());

    auto target_a = validate_endpoint(*this, constraint.target_a(), constraint.id(), "A");
    if (target_a.has_error()) {
      return Result<std::size_t>::failure(target_a.error());
    }
    auto target_b = validate_endpoint(*this, constraint.target_b(), constraint.id(), "B");
    if (target_b.has_error()) {
      return Result<std::size_t>::failure(target_b.error());
    }
    ++count;
  }
  return Result<std::size_t>::success(count);
}

const std::vector<AssemblyHierarchyConstraint>&
Project::cross_hierarchy_constraints() const noexcept {
  return cross_hierarchy_constraints_;
}

std::vector<AssemblyHierarchyConstraint>& Project::cross_hierarchy_constraints() noexcept {
  return cross_hierarchy_constraints_;
}

std::size_t Project::cross_hierarchy_constraint_count() const noexcept {
  return cross_hierarchy_constraints_.size();
}

const AssemblyHierarchyConstraint*
Project::find_cross_hierarchy_constraint(AssemblyConstraintId id) const noexcept {
  const auto found = std::find_if(
      cross_hierarchy_constraints_.begin(), cross_hierarchy_constraints_.end(),
      [&id](const AssemblyHierarchyConstraint& constraint) { return constraint.id() == id; });
  return found == cross_hierarchy_constraints_.end() ? nullptr : &*found;
}

AssemblyHierarchyConstraint*
Project::find_cross_hierarchy_constraint(AssemblyConstraintId id) noexcept {
  const auto found = std::find_if(
      cross_hierarchy_constraints_.begin(), cross_hierarchy_constraints_.end(),
      [&id](const AssemblyHierarchyConstraint& constraint) { return constraint.id() == id; });
  return found == cross_hierarchy_constraints_.end() ? nullptr : &*found;
}

} // namespace blcad
