#include "blcad/geometry/assembly_occurrence_solver.hpp"
#include "assembly_cross_hierarchy_numeric_system.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "effective_project_source.hpp"
#include "project_evaluation_internal.hpp"
#include <algorithm>

namespace blcad::geometry {
namespace {
Error error() {
  return Error::validation("assembly.occurrence_solver",
                           "Flexible occurrence or result snapshot is stale");
}
Result<Project> view(const Project& project, const std::vector<SubassemblyInstanceId>& path,
                     const DocumentId* expected = nullptr) {
  const auto* pose = project.find_occurrence_pose(path);
  if (!pose || !project.flexible_occurrence(path) ||
      (expected && pose->assembly_document() != *expected))
    return Result<Project>::failure(error());
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (!hierarchy)
    return Result<Project>::failure(hierarchy.error());
  const auto found =
      std::find_if(hierarchy.value().occurrences().begin(), hierarchy.value().occurrences().end(),
                   [&](const auto& occurrence) { return occurrence.occurrence_path == path; });
  if (found == hierarchy.value().occurrences().end() || !found->active_path)
    return Result<Project>::failure(error());
  return detail::make_cross_hierarchy_local_evaluation_view(project, pose->assembly_document(),
                                                            path);
}
Result<std::size_t> fresh(const Project& project, const std::string& snapshot) {
  auto current = detail::effective_project_source(project);
  if (!current)
    return Result<std::size_t>::failure(current.error());
  if (current.value() != snapshot)
    return Result<std::size_t>::failure(error());
  return Result<std::size_t>::success(1);
}
Result<std::size_t> publish(Project& candidate, const std::vector<SubassemblyInstanceId>& path,
                            const AssemblySolveResult& solved) {
  for (const auto& proposal : solved.proposed_transforms) {
    auto applied = candidate.set_occurrence_component_transform(path, proposal.component_instance,
                                                                proposal.proposed_transform);
    if (!applied)
      return applied;
  }
  return Result<std::size_t>::success(solved.proposed_transforms.size());
}
} // namespace

Result<AssemblyOccurrenceSolveResult> AssemblyOccurrenceSolver::solve(
    const Project& project, const std::vector<SubassemblyInstanceId>& path,
    const std::vector<ComponentInstanceId>& group, AssemblyRigidBodySolverOptions options) const {
  using R = Result<AssemblyOccurrenceSolveResult>;
  auto scope = detail::ProjectGeometryScope::create(project, imports_);
  if (!scope)
    return R::failure(scope.error());
  if (scope.value()->changed())
    return solve(scope.value()->project(), path, group, options);
  auto local = view(project, path);
  if (!local)
    return R::failure(local.error());
  auto snapshot = detail::effective_project_source(project);
  if (!snapshot)
    return R::failure(snapshot.error());
  auto result = AssemblyRigidBodySolver{imports_}.solve(local.value(), group, options);
  if (!result)
    return R::failure(result.error());
  return R::success({path, local.value().assembly().id(), std::move(snapshot.value()),
                     std::move(result.value())});
}
Result<AssemblyOccurrenceMotionResult> AssemblyOccurrenceSolver::move(
    const Project& project, const std::vector<SubassemblyInstanceId>& path,
    AssemblyJointDrive drive, AssemblyRigidBodySolverOptions options) const {
  using R = Result<AssemblyOccurrenceMotionResult>;
  auto scope = detail::ProjectGeometryScope::create(project, imports_);
  if (!scope)
    return R::failure(scope.error());
  if (scope.value()->changed())
    return move(scope.value()->project(), path, std::move(drive), options);
  auto local = view(project, path);
  if (!local)
    return R::failure(local.error());
  auto snapshot = detail::effective_project_source(project);
  if (!snapshot)
    return R::failure(snapshot.error());
  auto result = AssemblyJointMotionSolver{imports_}.move(local.value(), std::move(drive), options);
  if (!result)
    return R::failure(result.error());
  return R::success({path, local.value().assembly().id(), std::move(snapshot.value()),
                     std::move(result.value())});
}
Result<std::size_t>
AssemblyOccurrenceSolver::apply(Project& project,
                                const AssemblyOccurrenceSolveResult& result) const {
  auto scope = detail::ProjectGeometryScope::create(project, imports_);
  if (!scope)
    return Result<std::size_t>::failure(scope.error());
  if (scope.value()->outer()) {
    Project candidate = scope.value()->changed() ? scope.value()->project() : project;
    auto applied = apply(candidate, result);
    if (!applied)
      return applied;
    auto checked = evaluate_project_geometry(candidate, imports_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    project = std::move(checked.value());
    return applied;
  }
  auto valid = fresh(project, result.source_project_json);
  if (!valid)
    return valid;
  auto local = view(project, result.path, &result.assembly_document);
  if (!local)
    return Result<std::size_t>::failure(local.error());
  auto applied = AssemblySolveResultApplier{imports_}.apply(local.value(), result.local_result);
  if (!applied)
    return applied;
  Project candidate = project;
  auto changed = publish(candidate, result.path, result.local_result);
  if (!changed)
    return changed;
  project = std::move(candidate);
  return changed;
}
Result<std::size_t>
AssemblyOccurrenceSolver::apply(Project& project,
                                const AssemblyOccurrenceMotionResult& result) const {
  auto scope = detail::ProjectGeometryScope::create(project, imports_);
  if (!scope)
    return Result<std::size_t>::failure(scope.error());
  if (scope.value()->outer()) {
    Project candidate = scope.value()->changed() ? scope.value()->project() : project;
    auto applied = apply(candidate, result);
    if (!applied)
      return applied;
    auto checked = evaluate_project_geometry(candidate, imports_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    project = std::move(checked.value());
    return applied;
  }
  auto valid = fresh(project, result.source_project_json);
  if (!valid)
    return valid;
  auto local = view(project, result.path, &result.assembly_document);
  if (!local)
    return Result<std::size_t>::failure(local.error());
  auto applied =
      AssemblyJointMotionResultApplier{imports_}.apply(local.value(), result.local_result);
  if (!applied)
    return applied;
  Project candidate = project;
  auto changed = publish(candidate, result.path, result.local_result.solve_result);
  if (!changed)
    return changed;
  const auto* joint = local.value().assembly().find_joint(result.local_result.joint);
  if (!joint)
    return Result<std::size_t>::failure(error());
  for (const auto& slot : joint->coordinate_slots()) {
    auto coordinate = candidate.set_occurrence_joint_coordinate(result.path, joint->id(),
                                                                slot.role(), slot.value());
    if (!coordinate)
      return coordinate;
  }
  project = std::move(candidate);
  return changed;
}
} // namespace blcad::geometry
