#include "assembly_cross_hierarchy_numeric_system.hpp"

#include "blcad/core/assembly_hierarchy.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error internal_error(std::string object_id, std::string message) {
  return Error::internal(std::move(object_id), std::move(message));
}

[[nodiscard]] RigidTransform transform_from_variables(const NumericVector& values,
                                                      std::size_t offset) noexcept {
  return RigidTransform{Vector3{values[offset], values[offset + 1U], values[offset + 2U]},
                        Vector3{values[offset + 3U], values[offset + 4U], values[offset + 5U]}};
}

} // namespace

Result<Project> make_cross_hierarchy_local_evaluation_view(
    const Project& project, const DocumentId& assembly_document,
    const std::vector<SubassemblyInstanceId>& occurrence_path) {
  const AssemblyDocument* assembly = project.find_assembly_document(assembly_document);
  if (assembly == nullptr) {
    return Result<Project>::failure(
        validation_error(assembly_document.value(),
                         "cross-hierarchy local relationship assembly document must exist"));
  }

  AssemblyDocument effective = *assembly;
  if (!occurrence_path.empty()) {
    auto hierarchy = AssemblyHierarchyTraversal::build(project);
    if (!hierarchy)
      return Result<Project>::failure(hierarchy.error());
    const auto found = std::find_if(
        hierarchy.value().occurrences().begin(), hierarchy.value().occurrences().end(),
        [&](const auto& occurrence) { return occurrence.occurrence_path == occurrence_path; });
    if (found == hierarchy.value().occurrences().end() || !found->active_path ||
        found->assembly_document != assembly_document)
      return Result<Project>::failure(
          validation_error(assembly_document.value(), "Occurrence evaluation scope is stale"));
    auto evaluated = assembly->evaluate_model_state(found->selected_model_state);
    if (!evaluated)
      return Result<Project>::failure(evaluated.error());
    effective = std::move(evaluated.value());
  }
  auto local_project = Project::create(project.id(), project.name(), effective);
  if (local_project.has_error()) {
    return local_project;
  }
  for (const PartDocument& part : project.part_documents()) {
    auto added = local_project.value().add_part_document(part);
    if (added.has_error()) {
      return Result<Project>::failure(added.error());
    }
  }
  // A child becomes this view's root, so retain its descendant definitions.
  // Snapshot/source validation must still see a structurally complete hierarchy.
  std::vector<DocumentId> visited{assembly->id()};
  std::vector<DocumentId> pending;
  for (const auto& occurrence : assembly->subassembly_instances())
    pending.push_back(occurrence.referenced_assembly_document());
  for (std::size_t index = 0; index < pending.size(); ++index) {
    if (std::find(visited.begin(), visited.end(), pending[index]) != visited.end())
      continue;
    const auto* descendant = project.find_assembly_document(pending[index]);
    if (descendant == nullptr)
      return Result<Project>::failure(validation_error(
          pending[index].value(), "local evaluation view descendant assembly must exist"));
    auto added = local_project.value().add_child_assembly_document(*descendant);
    if (!added)
      return Result<Project>::failure(added.error());
    visited.push_back(descendant->id());
    for (const auto& occurrence : descendant->subassembly_instances())
      pending.push_back(occurrence.referenced_assembly_document());
  }
  if (project.flexible_occurrence(occurrence_path)) {
    const auto* pose = project.find_occurrence_pose(occurrence_path);
    if (!pose || !project.flexible_occurrence(occurrence_path) ||
        pose->assembly_document() != assembly_document)
      return Result<Project>::failure(
          validation_error(assembly_document.value(), "Flexible evaluation scope is stale"));
    for (const auto& coordinate : pose->joints()) {
      auto applied = local_project.value().assembly().set_joint_coordinate_value(
          coordinate.joint, coordinate.role, coordinate.value);
      if (!applied)
        return Result<Project>::failure(applied.error());
    }
    for (const auto& component : assembly->component_instances()) {
      auto transform = project.effective_component_transform(occurrence_path, component.id());
      if (!transform)
        return Result<Project>::failure(transform.error());
      auto applied = local_project.value().assembly().set_component_instance_transform(
          component.id(), transform.value());
      if (!applied)
        return Result<Project>::failure(applied.error());
    }
  }
  auto valid = local_project.value().validate_assembly_structure();
  if (!valid)
    return Result<Project>::failure(valid.error());
  return local_project;
}

Result<std::size_t> append_cross_hierarchy_local_relationship_residuals(
    const Project& project, const AssemblyLocalRelationshipIdentity& identity,
    double length_residual_scale_mm, NumericVector& residuals,
    StepImportTopologyContext* import_context) {
  auto local_project = make_cross_hierarchy_local_evaluation_view(
      project, identity.assembly_document, identity.occurrence_path);
  if (local_project.has_error()) {
    return Result<std::size_t>::failure(local_project.error());
  }
  if (local_project.value().assembly().find_constraint(identity.constraint) == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        identity.constraint.value(),
        "cross-hierarchy solve-group local relationship must exist in containing assembly"));
  }

  auto local_residuals = evaluate_residuals(local_project.value(),
                                            std::vector<AssemblyConstraintId>{identity.constraint},
                                            length_residual_scale_mm, import_context);
  if (local_residuals.has_error()) {
    return Result<std::size_t>::failure(local_residuals.error());
  }
  const std::size_t appended_count = local_residuals.value().size();
  residuals.insert(residuals.end(), local_residuals.value().begin(), local_residuals.value().end());
  return Result<std::size_t>::success(appended_count);
}

Result<std::size_t> append_cross_hierarchy_project_relationship_residuals(
    const Project& project, const AssemblyProjectCrossHierarchyRelationshipIdentity& identity,
    double length_residual_scale_mm, NumericVector& residuals,
    StepImportTopologyContext* import_context) {
  const AssemblyHierarchyConstraint* constraint =
      project.find_cross_hierarchy_constraint(identity.constraint);
  if (constraint == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(identity.constraint.value(),
                         "cross-hierarchy solve-group relationship must exist in project"));
  }

  auto query = AssemblyHierarchyConstraintQuery::create(*constraint);
  if (query.has_error()) {
    return Result<std::size_t>::failure(query.error());
  }
  const AssemblyHierarchyConstraintEquationBuilder builder(import_context);
  auto equation = builder.build(project, query.value());
  if (equation.has_error()) {
    return Result<std::size_t>::failure(equation.error());
  }
  return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
}

Result<const ComponentInstance*>
resolve_cross_hierarchy_authority_component(const Project& project,
                                            const ComponentTransformAuthority& authority) {
  const AssemblyDocument* assembly = project.find_assembly_document(authority.assembly_document);
  if (assembly == nullptr) {
    return Result<const ComponentInstance*>::failure(
        validation_error(authority.assembly_document.value(),
                         "cross-hierarchy transform authority assembly document must exist"));
  }
  if (!authority.occurrence_path.empty()) {
    const auto* pose = project.find_occurrence_pose(authority.occurrence_path);
    if (!pose || !project.flexible_occurrence(authority.occurrence_path) ||
        pose->assembly_document() != authority.assembly_document)
      return Result<const ComponentInstance*>::failure(validation_error(
          authority.component_instance.value(), "Occurrence transform authority is stale"));
  }
  const ComponentInstance* component =
      assembly->find_component_instance(authority.component_instance);
  if (component == nullptr) {
    return Result<const ComponentInstance*>::failure(
        validation_error(authority.component_instance.value(),
                         "cross-hierarchy transform authority component instance must exist"));
  }
  return Result<const ComponentInstance*>::success(component);
}

Result<RigidTransform>
cross_hierarchy_authority_transform(const Project& project,
                                    const ComponentTransformAuthority& authority) {
  auto component = resolve_cross_hierarchy_authority_component(project, authority);
  if (!component)
    return Result<RigidTransform>::failure(component.error());
  if (authority.occurrence_path.empty())
    return Result<RigidTransform>::success(component.value()->transform());
  return project.effective_component_transform(authority.occurrence_path,
                                               authority.component_instance);
}
Result<std::size_t> set_cross_hierarchy_authority_transform(
    Project& project, const ComponentTransformAuthority& authority, RigidTransform transform) {
  auto component = resolve_cross_hierarchy_authority_component(project, authority);
  if (!component)
    return Result<std::size_t>::failure(component.error());
  if (!authority.occurrence_path.empty())
    return project.set_occurrence_component_transform(authority.occurrence_path,
                                                      authority.component_instance, transform);
  return project.find_assembly_document(authority.assembly_document)
      ->set_component_instance_transform(authority.component_instance, transform);
}

Result<NumericVector> read_cross_hierarchy_authority_variables(
    const Project& project, const std::vector<ComponentTransformAuthority>& variable_authorities) {
  NumericVector values;
  values.reserve(variable_authorities.size() * kAssemblyTransformVariableCount);
  for (const ComponentTransformAuthority& authority : variable_authorities) {
    auto component = resolve_cross_hierarchy_authority_component(project, authority);
    if (component.has_error()) {
      return Result<NumericVector>::failure(component.error());
    }
    auto effective = cross_hierarchy_authority_transform(project, authority);
    if (!effective)
      return Result<NumericVector>::failure(effective.error());
    const RigidTransform& transform = effective.value();
    values.push_back(transform.translation_mm.x);
    values.push_back(transform.translation_mm.y);
    values.push_back(transform.translation_mm.z);
    values.push_back(transform.rotation_deg.x);
    values.push_back(transform.rotation_deg.y);
    values.push_back(transform.rotation_deg.z);
  }
  return Result<NumericVector>::success(std::move(values));
}

Result<std::size_t> apply_cross_hierarchy_authority_variables(
    Project& project, const std::vector<ComponentTransformAuthority>& variable_authorities,
    const NumericVector& values) {
  if (values.size() != variable_authorities.size() * kAssemblyTransformVariableCount) {
    return Result<std::size_t>::failure(internal_error(
        "assembly.cross_hierarchy_solver",
        "cross-hierarchy numeric variable vector does not match transform-authority count"));
  }

  for (std::size_t index = 0U; index < variable_authorities.size(); ++index) {
    const ComponentTransformAuthority& authority = variable_authorities[index];
    AssemblyDocument* assembly = project.find_assembly_document(authority.assembly_document);
    if (assembly == nullptr) {
      return Result<std::size_t>::failure(
          validation_error(authority.assembly_document.value(),
                           "cross-hierarchy transform authority assembly document must exist"));
    }
    const std::size_t offset = index * kAssemblyTransformVariableCount;
    auto updated = set_cross_hierarchy_authority_transform(
        project, authority, transform_from_variables(values, offset));
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }
  return Result<std::size_t>::success(variable_authorities.size());
}

Result<NumericVector> evaluate_cross_hierarchy_group_residuals(
    const Project& project, const AssemblyCrossHierarchySolveGroup& solve_group,
    double length_residual_scale_mm, StepImportTopologyContext* import_context) {
  NumericVector residuals;
  residuals.reserve(solve_group.relationships.size() * 7U);

  for (const AssemblyRelationshipIdentity& relationship : solve_group.relationships) {
    Result<std::size_t> appended =
        std::holds_alternative<AssemblyLocalRelationshipIdentity>(relationship)
            ? append_cross_hierarchy_local_relationship_residuals(
                  project, std::get<AssemblyLocalRelationshipIdentity>(relationship),
                  length_residual_scale_mm, residuals, import_context)
            : append_cross_hierarchy_project_relationship_residuals(
                  project,
                  std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(relationship),
                  length_residual_scale_mm, residuals, import_context);
    if (appended.has_error()) {
      return Result<NumericVector>::failure(appended.error());
    }
  }

  return Result<NumericVector>::success(std::move(residuals));
}

} // namespace blcad::geometry::detail
