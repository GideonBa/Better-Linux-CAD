#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"

#include "assembly_constraint_numeric_system.hpp"
#include "assembly_numeric_solve_engine.hpp"
#include "assembly_semantic_target_freshness.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<std::size_t> validate_application_snapshot(const Project& project,
                                                                const AssemblySolveResult& result) {
  if (result.component_snapshots.size() != result.component_group.size()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver",
        "assembly solve result component snapshot set is incomplete or contains extras"));
  }

  std::vector<ComponentInstanceId> expected_fixed_components;
  std::vector<ComponentInstanceId> expected_variable_components;
  std::vector<DocumentId> semantic_target_part_documents;
  expected_fixed_components.reserve(result.component_snapshots.size());
  expected_variable_components.reserve(result.component_snapshots.size());
  semantic_target_part_documents.reserve(result.component_snapshots.size());

  for (std::size_t index = 0U; index < result.component_snapshots.size(); ++index) {
    const AssemblySolveComponentSnapshot& snapshot = result.component_snapshots[index];
    if (snapshot.component_instance != result.component_group[index]) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.component_instance.value(),
          "assembly solve result component snapshots do not match exact component-group order"));
    }

    const ComponentInstance* component =
        project.assembly().find_component_instance(snapshot.component_instance);
    if (component == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.component_instance.value(), "assembly solve result component no longer exists"));
    }
    if (component->referenced_part_document() != snapshot.referenced_part_document ||
        component->transform() != snapshot.source_transform ||
        component->grounding_state() != snapshot.grounding_state ||
        component->suppression_state() != snapshot.suppression_state) {
      return Result<std::size_t>::failure(
          validation_error(snapshot.component_instance.value(),
                           "assembly solve result is stale because component solve input changed"));
    }

    semantic_target_part_documents.push_back(snapshot.referenced_part_document);
    if (snapshot.suppression_state != ComponentSuppressionState::Active) {
      continue;
    }
    if (snapshot.grounding_state == ComponentGroundingState::Grounded) {
      expected_fixed_components.push_back(snapshot.component_instance);
    } else {
      expected_variable_components.push_back(snapshot.component_instance);
    }
  }

  if (result.fixed_components != expected_fixed_components) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver",
        "assembly solve result fixed-component order does not match complete component snapshots"));
  }

  auto semantic_freshness = detail::validate_semantic_target_part_snapshots(
      project, semantic_target_part_documents, result.semantic_target_part_snapshots);
  if (semantic_freshness.has_error()) {
    return Result<std::size_t>::failure(semantic_freshness.error());
  }

  if (result.proposed_transforms.size() != expected_variable_components.size()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.solver",
        "assembly solve result proposals must exactly cover active free component snapshots"));
  }

  for (std::size_t index = 0U; index < result.proposed_transforms.size(); ++index) {
    const ProposedComponentTransform& proposal = result.proposed_transforms[index];
    if (proposal.component_instance != expected_variable_components[index]) {
      return Result<std::size_t>::failure(validation_error(
          proposal.component_instance.value(),
          "assembly solve result proposal order does not match active free component snapshots"));
    }

    const AssemblySolveComponentSnapshot& snapshot = *std::find_if(
        result.component_snapshots.begin(), result.component_snapshots.end(),
        [&proposal](const AssemblySolveComponentSnapshot& candidate) {
          return candidate.component_instance == proposal.component_instance;
        });
    if (snapshot.source_transform != proposal.source_transform) {
      return Result<std::size_t>::failure(validation_error(
          proposal.component_instance.value(),
          "assembly solve proposal source transform does not match component snapshot"));
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

struct ResolvedFlexibleSubassemblyOccurrence {
  DocumentId assembly_document;
};

[[nodiscard]] std::string occurrence_object_id(
    const std::vector<SubassemblyInstanceId>& occurrence_path) {
  if (occurrence_path.empty()) {
    return "assembly.flexible_subassembly";
  }
  return occurrence_path.back().value();
}

[[nodiscard]] Result<ResolvedFlexibleSubassemblyOccurrence> resolve_flexible_occurrence(
    const Project& project, const std::vector<SubassemblyInstanceId>& occurrence_path) {
  if (occurrence_path.empty()) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        "assembly.flexible_subassembly",
        "flexible subassembly solving requires a non-root occurrence path"));
  }

  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (hierarchy.has_error()) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(hierarchy.error());
  }

  const auto found = std::find_if(
      hierarchy.value().occurrences().begin(), hierarchy.value().occurrences().end(),
      [&occurrence_path](const AssemblyHierarchyOccurrenceDescriptor& occurrence) {
        return occurrence.occurrence_path == occurrence_path;
      });
  if (found == hierarchy.value().occurrences().end()) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "flexible subassembly occurrence path must exist in the rooted assembly hierarchy"));
  }
  if (!found->active_path) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "flexible subassembly occurrence path must be active"));
  }
  if (!found->parent_assembly_document.has_value() ||
      !found->via_subassembly_instance.has_value()) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "flexible subassembly occurrence must have a parent occurrence boundary"));
  }

  const AssemblyDocument* parent =
      project.find_assembly_document(found->parent_assembly_document.value());
  if (parent == nullptr) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "flexible subassembly parent assembly must resolve in project"));
  }
  const SubassemblyInstance* instance =
      parent->find_subassembly_instance(found->via_subassembly_instance.value());
  if (instance == nullptr || instance->referenced_assembly_document() != found->assembly_document) {
    return Result<ResolvedFlexibleSubassemblyOccurrence>::failure(validation_error(
        occurrence_object_id(occurrence_path),
        "flexible subassembly occurrence boundary no longer matches hierarchy identity"));
  }

  return Result<ResolvedFlexibleSubassemblyOccurrence>::success(
      ResolvedFlexibleSubassemblyOccurrence{found->assembly_document});
}

[[nodiscard]] Result<Project> make_child_solve_view(
    const Project& project, const ResolvedFlexibleSubassemblyOccurrence& occurrence) {
  const AssemblyDocument* child = project.find_assembly_document(occurrence.assembly_document);
  if (child == nullptr || child->id() == project.assembly().id()) {
    return Result<Project>::failure(validation_error(
        occurrence.assembly_document.value(),
        "flexible subassembly solve target must resolve to a project-owned child assembly"));
  }

  auto local_project = Project::create(project.id(), project.name(), *child);
  if (local_project.has_error()) {
    return local_project;
  }

  for (const PartDocument& part : project.part_documents()) {
    auto added = local_project.value().add_part_document(part);
    if (added.has_error()) {
      return Result<Project>::failure(added.error());
    }
  }

  return local_project;
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

Result<AssemblyFlexibleSubassemblySolveResult> AssemblyFlexibleSubassemblySolver::solve(
    const Project& project, const std::vector<SubassemblyInstanceId>& occurrence_path,
    const std::vector<ComponentInstanceId>& connected_group,
    AssemblyRigidBodySolverOptions options) const {
  auto occurrence = resolve_flexible_occurrence(project, occurrence_path);
  if (occurrence.has_error()) {
    return Result<AssemblyFlexibleSubassemblySolveResult>::failure(occurrence.error());
  }

  auto local_project = make_child_solve_view(project, occurrence.value());
  if (local_project.has_error()) {
    return Result<AssemblyFlexibleSubassemblySolveResult>::failure(local_project.error());
  }

  const AssemblyRigidBodySolver solver;
  auto local_result = solver.solve(local_project.value(), connected_group, options);
  if (local_result.has_error()) {
    return Result<AssemblyFlexibleSubassemblySolveResult>::failure(local_result.error());
  }

  return Result<AssemblyFlexibleSubassemblySolveResult>::success(
      AssemblyFlexibleSubassemblySolveResult{occurrence_path, occurrence.value().assembly_document,
                                             std::move(local_result.value())});
}

Result<std::size_t> AssemblyFlexibleSubassemblySolveResultApplier::apply(
    Project& project, const AssemblyFlexibleSubassemblySolveResult& result) const {
  auto occurrence = resolve_flexible_occurrence(project, result.occurrence_path);
  if (occurrence.has_error()) {
    return Result<std::size_t>::failure(occurrence.error());
  }
  if (occurrence.value().assembly_document != result.assembly_document) {
    return Result<std::size_t>::failure(validation_error(
        occurrence_object_id(result.occurrence_path),
        "flexible subassembly solve result is stale because occurrence target changed"));
  }

  auto local_project = make_child_solve_view(project, occurrence.value());
  if (local_project.has_error()) {
    return Result<std::size_t>::failure(local_project.error());
  }

  const AssemblySolveResultApplier local_applier;
  auto local_applied = local_applier.apply(local_project.value(), result.local_result);
  if (local_applied.has_error()) {
    return Result<std::size_t>::failure(local_applied.error());
  }

  Project candidate_project = project;
  AssemblyDocument* child = candidate_project.find_assembly_document(result.assembly_document);
  if (child == nullptr || child->id() == candidate_project.assembly().id()) {
    return Result<std::size_t>::failure(validation_error(
        result.assembly_document.value(),
        "flexible subassembly solve target must remain a project-owned child assembly"));
  }

  for (const ProposedComponentTransform& proposal : result.local_result.proposed_transforms) {
    const ComponentInstance* solved_component =
        local_project.value().assembly().find_component_instance(proposal.component_instance);
    if (solved_component == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          proposal.component_instance.value(),
          "flexible subassembly solved component must remain in local solve view"));
    }
    auto updated = child->set_component_instance_transform(proposal.component_instance,
                                                           solved_component->transform());
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }

  project = std::move(candidate_project);
  return Result<std::size_t>::success(local_applied.value());
}

} // namespace blcad::geometry
