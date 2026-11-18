#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "effective_project_source.hpp"
#include "project_evaluation_internal.hpp"
#include <algorithm>

namespace blcad::geometry {
namespace detail {
Result<std::unique_ptr<ProjectGeometryScope>>
ProjectGeometryScope::create(const Project& project, StepImportTopologyContext* imports) {
  const bool outer = project_evaluation_depth == 0;
  auto scope = std::unique_ptr<ProjectGeometryScope>(new ProjectGeometryScope);
  scope->outer_ = outer;
  if (outer &&
      (!project.design_references().empty() || !project.assembly_hole_features().empty())) {
    auto evaluated = evaluate_project_geometry(project, imports);
    if (!evaluated)
      return Result<std::unique_ptr<ProjectGeometryScope>>::failure(evaluated.error());
    auto before = effective_project_source(project),
         after = effective_project_source(evaluated.value());
    if (!before)
      return Result<std::unique_ptr<ProjectGeometryScope>>::failure(before.error());
    if (!after)
      return Result<std::unique_ptr<ProjectGeometryScope>>::failure(after.error());
    if (before.value() != after.value())
      scope->evaluated_ = std::move(evaluated.value());
  }
  return Result<std::unique_ptr<ProjectGeometryScope>>::success(std::move(scope));
}
} // namespace detail
Result<Project> evaluate_project_geometry(const Project& project,
                                          StepImportTopologyContext* imports) {
  detail::ProjectEvaluationGuard guard;
  auto valid = project.validate_assembly_structure();
  if (!valid)
    return Result<Project>::failure(valid.error());
  if (project.design_references().empty() && project.assembly_hole_features().empty())
    return Result<Project>::success(project);
  auto feedback = detail::validate_project_feedback(project);
  if (!feedback)
    return Result<Project>::failure(feedback.error());
  std::vector<std::pair<DocumentId, DocumentId>> machining_dependencies;
  for (const auto& feature : project.assembly_hole_features())
    for (const auto& participant : feature.participants())
      if (feature.layout().source.part != participant.occurrence.part)
        machining_dependencies.emplace_back(feature.layout().source.part,
                                            participant.occurrence.part);
  auto order = project_design_dependency_order(project, machining_dependencies);
  if (!order)
    return Result<Project>::failure(order.error());
  Project candidate = project;
  // The combined graph is acyclic. Each definition consumes only completed
  // upstream outputs, and each local machining slice remains inside this one
  // disposable root candidate. This is a directed pass, without feedback loops.
  for (const auto& part : order.value()) {
    if (std::any_of(candidate.design_references().begin(), candidate.design_references().end(),
                    [&](const auto& ref) { return ref.driver().part == part; })) {
      auto updated = detail::stage_adaptive_part(candidate, part, imports);
      if (!updated)
        return updated;
      candidate = std::move(updated.value());
    }
    if (std::any_of(candidate.assembly_hole_features().begin(),
                    candidate.assembly_hole_features().end(), [&](const auto& feature) {
                      return std::any_of(feature.participants().begin(),
                                         feature.participants().end(),
                                         [&](const auto& participant) {
                                           return participant.occurrence.part == part;
                                         });
                    })) {
      auto updated = detail::stage_assembly_holes(candidate, part, imports);
      if (!updated)
        return updated;
      candidate = std::move(updated.value());
    }
  }
  // Recheck all downstream histories, relationships and current source frames.
  // Any solve that invalidates a consumed axis rejects the candidate below.
  auto poses = detail::settle_project_pose(candidate, imports);
  if (!poses)
    return Result<Project>::failure(poses.error());
  if (!candidate.design_references().empty()) {
    auto adaptive = AdaptiveProjectEvaluator{imports}.evaluate(candidate);
    if (!adaptive)
      return Result<Project>::failure(adaptive.error());
    candidate = std::move(adaptive.value().project);
  }
  for (const auto& part : candidate.part_documents()) {
    auto cache = ShapeCache::create(ShapeCacheId("project.checked." + part.id().value()));
    if (!cache)
      return Result<Project>::failure(cache.error());
    auto computed = GeometryRecomputeExecutor{imports}.execute_document(part, cache.value());
    if (!computed)
      return Result<Project>::failure(computed.error());
  }
  auto physical = BodyResultInspector{}.inspect_physical_properties(candidate, imports);
  if (!physical)
    return Result<Project>::failure(physical.error());
  auto posed = AssemblyStepExporter{imports}.build_posed_shape(candidate);
  if (!posed)
    return Result<Project>::failure(posed.error());
  auto checked = detail::verify_assembly_holes(candidate, imports);
  if (!checked)
    return Result<Project>::failure(checked.error());
  if (imports) {
    auto fresh = imports->validate_sources();
    if (!fresh)
      return Result<Project>::failure(fresh.error());
  }
  return Result<Project>::success(std::move(candidate));
}
} // namespace blcad::geometry
