#include "blcad/core/project.hpp"

#include <algorithm>
#include <utility>

namespace blcad {

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
