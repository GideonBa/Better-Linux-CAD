#include "assembly_cross_hierarchy_numeric_system.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

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

[[nodiscard]] Result<Project> make_local_evaluation_view(const Project& project,
                                                         const DocumentId& assembly_document) {
  const AssemblyDocument* assembly = project.find_assembly_document(assembly_document);
  if (assembly == nullptr) {
    return Result<Project>::failure(validation_error(
        assembly_document.value(),
        "cross-hierarchy local relationship assembly document must exist"));
  }

  auto local_project = Project::create(project.id(), project.name(), *assembly);
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

[[nodiscard]] Result<std::size_t> append_local_relationship_residuals(
    const Project& project, const AssemblyLocalRelationshipIdentity& identity,
    double length_residual_scale_mm, NumericVector& residuals) {
  auto local_project = make_local_evaluation_view(project, identity.assembly_document);
  if (local_project.has_error()) {
    return Result<std::size_t>::failure(local_project.error());
  }
  if (local_project.value().assembly().find_constraint(identity.constraint) == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        identity.constraint.value(),
        "cross-hierarchy solve-group local relationship must exist in containing assembly"));
  }

  auto local_residuals = evaluate_residuals(
      local_project.value(), std::vector<AssemblyConstraintId>{identity.constraint},
      length_residual_scale_mm);
  if (local_residuals.has_error()) {
    return Result<std::size_t>::failure(local_residuals.error());
  }
  const std::size_t appended_count = local_residuals.value().size();
  residuals.insert(residuals.end(), local_residuals.value().begin(),
                   local_residuals.value().end());
  return Result<std::size_t>::success(appended_count);
}

[[nodiscard]] Result<std::size_t> append_cross_hierarchy_relationship_residuals(
    const Project& project,
    const AssemblyProjectCrossHierarchyRelationshipIdentity& identity,
    double length_residual_scale_mm, NumericVector& residuals) {
  const AssemblyHierarchyConstraint* constraint =
      project.find_cross_hierarchy_constraint(identity.constraint);
  if (constraint == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        identity.constraint.value(),
        "cross-hierarchy solve-group relationship must exist in project"));
  }

  auto query = AssemblyHierarchyConstraintQuery::create(*constraint);
  if (query.has_error()) {
    return Result<std::size_t>::failure(query.error());
  }
  const AssemblyHierarchyConstraintEquationBuilder builder;
  auto equation = builder.build(project, query.value());
  if (equation.has_error()) {
    return Result<std::size_t>::failure(equation.error());
  }
  return append_scaled_residuals(equation.value().residual,
                                 length_residual_scale_mm, residuals);
}

} // namespace

Result<const ComponentInstance*> resolve_cross_hierarchy_authority_component(
    const Project& project, const ComponentTransformAuthority& authority) {
  const AssemblyDocument* assembly = project.find_assembly_document(authority.assembly_document);
  if (assembly == nullptr) {
    return Result<const ComponentInstance*>::failure(validation_error(
        authority.assembly_document.value(),
        "cross-hierarchy transform authority assembly document must exist"));
  }
  const ComponentInstance* component =
      assembly->find_component_instance(authority.component_instance);
  if (component == nullptr) {
    return Result<const ComponentInstance*>::failure(validation_error(
        authority.component_instance.value(),
        "cross-hierarchy transform authority component instance must exist"));
  }
  return Result<const ComponentInstance*>::success(component);
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
    const RigidTransform& transform = component.value()->transform();
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
      return Result<std::size_t>::failure(validation_error(
          authority.assembly_document.value(),
          "cross-hierarchy transform authority assembly document must exist"));
    }
    const std::size_t offset = index * kAssemblyTransformVariableCount;
    auto updated = assembly->set_component_instance_transform(
        authority.component_instance, transform_from_variables(values, offset));
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }
  return Result<std::size_t>::success(variable_authorities.size());
}

Result<NumericVector> evaluate_cross_hierarchy_group_residuals(
    const Project& project, const AssemblyCrossHierarchySolveGroup& solve_group,
    double length_residual_scale_mm) {
  NumericVector residuals;
  residuals.reserve(solve_group.relationships.size() * 7U);

  for (const AssemblyRelationshipIdentity& relationship : solve_group.relationships) {
    Result<std::size_t> appended =
        std::holds_alternative<AssemblyLocalRelationshipIdentity>(relationship)
            ? append_local_relationship_residuals(
                  project, std::get<AssemblyLocalRelationshipIdentity>(relationship),
                  length_residual_scale_mm, residuals)
            : append_cross_hierarchy_relationship_residuals(
                  project,
                  std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(relationship),
                  length_residual_scale_mm, residuals);
    if (appended.has_error()) {
      return Result<NumericVector>::failure(appended.error());
    }
  }

  return Result<NumericVector>::success(std::move(residuals));
}

} // namespace blcad::geometry::detail
