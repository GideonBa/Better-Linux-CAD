#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

struct ResolvedFlexibleSubassemblyOccurrence {
  DocumentId assembly_document;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string occurrence_object_id(
    const std::vector<SubassemblyInstanceId>& occurrence_path) {
  if (occurrence_path.empty()) {
    return "assembly.flexible_subassembly";
  }
  return occurrence_path.back().value();
}

[[nodiscard]] Result<ResolvedFlexibleSubassemblyOccurrence> resolve_occurrence(
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

Result<AssemblyFlexibleSubassemblySolveResult> AssemblyFlexibleSubassemblySolver::solve(
    const Project& project, const std::vector<SubassemblyInstanceId>& occurrence_path,
    const std::vector<ComponentInstanceId>& connected_group,
    AssemblyRigidBodySolverOptions options) const {
  auto occurrence = resolve_occurrence(project, occurrence_path);
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
  auto occurrence = resolve_occurrence(project, result.occurrence_path);
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
