#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"

#include "assembly_constraint_numeric_system.hpp"
#include "assembly_numeric_solve_engine.hpp"

#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Error internal_error(std::string object_id, std::string message) {
  return Error::internal(std::move(object_id), std::move(message));
}

[[nodiscard]] RigidTransform transform_from_variables(const detail::NumericVector& values,
                                                      std::size_t offset) noexcept {
  return RigidTransform{Vector3{values[offset], values[offset + 1U], values[offset + 2U]},
                        Vector3{values[offset + 3U], values[offset + 4U], values[offset + 5U]}};
}

[[nodiscard]] Result<const ComponentInstance*> resolve_authority_component(
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

[[nodiscard]] Result<detail::NumericVector> read_authority_variables(
    const Project& project, const std::vector<ComponentTransformAuthority>& variable_authorities) {
  detail::NumericVector values;
  values.reserve(variable_authorities.size() * detail::kAssemblyTransformVariableCount);
  for (const ComponentTransformAuthority& authority : variable_authorities) {
    auto component = resolve_authority_component(project, authority);
    if (component.has_error()) {
      return Result<detail::NumericVector>::failure(component.error());
    }
    const RigidTransform& transform = component.value()->transform();
    values.push_back(transform.translation_mm.x);
    values.push_back(transform.translation_mm.y);
    values.push_back(transform.translation_mm.z);
    values.push_back(transform.rotation_deg.x);
    values.push_back(transform.rotation_deg.y);
    values.push_back(transform.rotation_deg.z);
  }
  return Result<detail::NumericVector>::success(std::move(values));
}

[[nodiscard]] Result<std::size_t> apply_authority_variables(
    Project& project, const std::vector<ComponentTransformAuthority>& variable_authorities,
    const detail::NumericVector& values) {
  if (values.size() != variable_authorities.size() * detail::kAssemblyTransformVariableCount) {
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
    const std::size_t offset = index * detail::kAssemblyTransformVariableCount;
    auto updated = assembly->set_component_instance_transform(
        authority.component_instance, transform_from_variables(values, offset));
    if (updated.has_error()) {
      return Result<std::size_t>::failure(updated.error());
    }
  }
  return Result<std::size_t>::success(variable_authorities.size());
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
    double length_residual_scale_mm, detail::NumericVector& residuals) {
  auto local_project = make_local_evaluation_view(project, identity.assembly_document);
  if (local_project.has_error()) {
    return Result<std::size_t>::failure(local_project.error());
  }
  if (local_project.value().assembly().find_constraint(identity.constraint) == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        identity.constraint.value(),
        "cross-hierarchy solve-group local relationship must exist in containing assembly"));
  }

  auto local_residuals = detail::evaluate_residuals(
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
    double length_residual_scale_mm, detail::NumericVector& residuals) {
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
  return detail::append_scaled_residuals(equation.value().residual,
                                         length_residual_scale_mm, residuals);
}

[[nodiscard]] Result<detail::NumericVector> evaluate_group_residuals(
    const Project& project, const AssemblyCrossHierarchySolveGroup& solve_group,
    double length_residual_scale_mm) {
  detail::NumericVector residuals;
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
      return Result<detail::NumericVector>::failure(appended.error());
    }
  }

  return Result<detail::NumericVector>::success(std::move(residuals));
}

[[nodiscard]] Result<AssemblyCrossHierarchySolveResult> make_solve_result(
    const Project& source_project, const Project& solved_project,
    const AssemblyCrossHierarchySolveGroup& solve_group,
    const std::vector<ComponentTransformAuthority>& fixed_authorities,
    const std::vector<ComponentTransformAuthority>& variable_authorities,
    const detail::AssemblyNumericSolveOutcome& outcome) {
  std::vector<AssemblyCrossHierarchyTransformAuthoritySnapshot> snapshots;
  snapshots.reserve(solve_group.authorities.size());
  for (const ComponentTransformAuthority& authority : solve_group.authorities) {
    auto component = resolve_authority_component(source_project, authority);
    if (component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(component.error());
    }
    snapshots.push_back(AssemblyCrossHierarchyTransformAuthoritySnapshot{
        authority, component.value()->grounding_state(), component.value()->suppression_state(),
        component.value()->transform()});
  }

  std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform> proposals;
  proposals.reserve(variable_authorities.size());
  for (const ComponentTransformAuthority& authority : variable_authorities) {
    auto source_component = resolve_authority_component(source_project, authority);
    if (source_component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(source_component.error());
    }
    auto solved_component = resolve_authority_component(solved_project, authority);
    if (solved_component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(solved_component.error());
    }
    proposals.push_back(ProposedAssemblyCrossHierarchyAuthorityTransform{
        authority, source_component.value()->transform(), solved_component.value()->transform()});
  }

  return Result<AssemblyCrossHierarchySolveResult>::success(
      AssemblyCrossHierarchySolveResult{
          outcome.state,
          outcome.iterations,
          solve_group.relationships,
          fixed_authorities,
          std::move(snapshots),
          std::move(proposals),
          AssemblySolveResidualSummary{outcome.final_residuals.size(),
                                       detail::residual_rms(outcome.initial_residuals),
                                       detail::residual_rms(outcome.final_residuals),
                                       detail::residual_max_abs(outcome.final_residuals)}});
}

} // namespace

Result<AssemblyCrossHierarchySolveResult> AssemblyCrossHierarchyConstraintSolver::solve(
    const Project& project, const AssemblyCrossHierarchySolveGroup& solve_group,
    AssemblyRigidBodySolverOptions options) const {
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  if (graph.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(graph.error());
  }
  if (std::find(graph.value().solve_groups().begin(), graph.value().solve_groups().end(),
                solve_group) == graph.value().solve_groups().end()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(validation_error(
        "assembly.cross_hierarchy_solver",
        "solver input must exactly match one current deterministic cross-hierarchy solve group"));
  }

  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<ComponentTransformAuthority> variable_authorities;
  for (const ComponentTransformAuthority& authority : solve_group.authorities) {
    auto component = resolve_authority_component(project, authority);
    if (component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(component.error());
    }
    if (component.value()->suppression_state() != ComponentSuppressionState::Active) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(validation_error(
          authority.component_instance.value(),
          "cross-hierarchy solve-group authority must remain active"));
    }
    if (component.value()->grounding_state() == ComponentGroundingState::Grounded) {
      fixed_authorities.push_back(authority);
    } else {
      variable_authorities.push_back(authority);
    }
  }

  auto initial_variables = read_authority_variables(project, variable_authorities);
  if (initial_variables.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(initial_variables.error());
  }

  const detail::AssemblyNumericResidualEvaluator evaluator =
      [&project, &solve_group, &variable_authorities,
       length_scale = options.length_residual_scale_mm](const detail::NumericVector& values) {
        Project candidate_project = project;
        auto applied = apply_authority_variables(candidate_project, variable_authorities, values);
        if (applied.has_error()) {
          return Result<detail::NumericVector>::failure(applied.error());
        }
        return evaluate_group_residuals(candidate_project, solve_group, length_scale);
      };

  auto outcome = detail::solve_numeric_variables(initial_variables.value(),
                                                 !fixed_authorities.empty(), evaluator, options);
  if (outcome.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(outcome.error());
  }

  Project solved_project = project;
  auto applied = apply_authority_variables(solved_project, variable_authorities,
                                           outcome.value().final_variables);
  if (applied.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(applied.error());
  }

  return make_solve_result(project, solved_project, solve_group, fixed_authorities,
                           variable_authorities, outcome.value());
}

} // namespace blcad::geometry
