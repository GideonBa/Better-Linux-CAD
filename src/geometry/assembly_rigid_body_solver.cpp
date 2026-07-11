#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include "assembly_constraint_numeric_system.hpp"
#include "assembly_numeric_solve_engine.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool contains_component(const std::vector<ComponentInstanceId>& components,
                                      const ComponentInstanceId& component) {
  return std::find(components.begin(), components.end(), component) != components.end();
}

[[nodiscard]] Result<std::size_t> validate_application_snapshot(const Project& project,
                                                                const AssemblySolveResult& result) {
  std::vector<ComponentInstanceId> seen_snapshots;
  seen_snapshots.reserve(result.component_snapshots.size());
  for (const auto& snapshot : result.component_snapshots) {
    if (contains_component(seen_snapshots, snapshot.component_instance)) {
      return Result<std::size_t>::failure(
          validation_error(snapshot.component_instance.value(),
                           "assembly solve result contains duplicate snapshots"));
    }
    seen_snapshots.push_back(snapshot.component_instance);

    const ComponentInstance* component =
        project.assembly().find_component_instance(snapshot.component_instance);
    if (component == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.component_instance.value(), "assembly solve result component no longer exists"));
    }
    if (component->transform() != snapshot.source_transform ||
        component->grounding_state() != snapshot.grounding_state ||
        component->suppression_state() != snapshot.suppression_state) {
      return Result<std::size_t>::failure(
          validation_error(snapshot.component_instance.value(),
                           "assembly solve result is stale because component solve input changed"));
    }
  }

  std::vector<ComponentInstanceId> seen_proposals;
  seen_proposals.reserve(result.proposed_transforms.size());
  for (const auto& proposal : result.proposed_transforms) {
    if (contains_component(seen_proposals, proposal.component_instance)) {
      return Result<std::size_t>::failure(
          validation_error(proposal.component_instance.value(),
                           "assembly solve result contains duplicate proposals"));
    }
    seen_proposals.push_back(proposal.component_instance);

    const auto snapshot =
        std::find_if(result.component_snapshots.begin(), result.component_snapshots.end(),
                     [&proposal](const AssemblySolveComponentSnapshot& candidate) {
                       return candidate.component_instance == proposal.component_instance;
                     });
    if (snapshot == result.component_snapshots.end() ||
        snapshot->grounding_state != ComponentGroundingState::Free ||
        snapshot->suppression_state != ComponentSuppressionState::Active ||
        snapshot->source_transform != proposal.source_transform) {
      return Result<std::size_t>::failure(validation_error(
          proposal.component_instance.value(),
          "assembly solve proposal does not match a free active component snapshot"));
    }

    const ComponentInstance* component =
        project.assembly().find_component_instance(proposal.component_instance);
    auto validated = component->with_transform(proposal.proposed_transform);
    if (validated.has_error()) {
      return Result<std::size_t>::failure(validated.error());
    }
  }

  return Result<std::size_t>::success(result.proposed_transforms.size());
}

} // namespace

Result<AssemblySolveResult>
AssemblyRigidBodySolver::solve(const Project& project,
                               const std::vector<ComponentInstanceId>& connected_group,
                               AssemblyRigidBodySolverOptions options) const {
  auto graph = AssemblyConstraintGraph::build(project.assembly());
  if (graph.has_error()) {
    return Result<AssemblySolveResult>::failure(graph.error());
  }
  const auto groups = graph.value().connected_components();
  if (std::find(groups.begin(), groups.end(), connected_group) == groups.end()) {
    return Result<AssemblySolveResult>::failure(
        validation_error("assembly.solver",
                         "solver input must exactly match one deterministic connected component"));
  }

  std::vector<ComponentInstanceId> active_subgroup;
  for (const auto& component_id : connected_group) {
    const ComponentInstance* component = project.assembly().find_component_instance(component_id);
    if (component == nullptr) {
      return Result<AssemblySolveResult>::failure(validation_error(
          component_id.value(), "solver connected-group component must exist in assembly"));
    }
    if (component->suppression_state() == ComponentSuppressionState::Active) {
      active_subgroup.push_back(component_id);
    }
  }

  auto constraint_ids = detail::collect_constraint_ids(graph.value(), active_subgroup);
  if (constraint_ids.has_error()) {
    return Result<AssemblySolveResult>::failure(constraint_ids.error());
  }

  return detail::solve_numeric_relationships(
      project, connected_group,
      detail::AssemblyNumericRelationshipSet{std::move(constraint_ids.value()), {}}, options);
}

Result<std::size_t> AssemblySolveResultApplier::apply(Project& project,
                                                      const AssemblySolveResult& result) const {
  if (!result.converged()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver", "only a converged assembly solve result can be applied"));
  }

  auto validated = validate_application_snapshot(project, result);
  if (validated.has_error()) {
    return Result<std::size_t>::failure(validated.error());
  }

  Project candidate_project = project;
  for (const auto& proposal : result.proposed_transforms) {
    auto updated = candidate_project.assembly().set_component_instance_transform(
        proposal.component_instance, proposal.proposed_transform);
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }

  project = std::move(candidate_project);
  return Result<std::size_t>::success(result.proposed_transforms.size());
}

} // namespace blcad::geometry
