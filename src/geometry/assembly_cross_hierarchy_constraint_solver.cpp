#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"

#include "assembly_cross_hierarchy_freshness.hpp"
#include "assembly_cross_hierarchy_numeric_system.hpp"
#include "assembly_numeric_solve_engine.hpp"
#include "assembly_semantic_target_freshness.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::optional<double> distance_mm(const std::optional<Quantity>& quantity) {
  return quantity.has_value() ? std::optional<double>(quantity->millimeters()) : std::nullopt;
}

[[nodiscard]] std::optional<double> angle_deg(const std::optional<Quantity>& quantity) {
  return quantity.has_value() ? std::optional<double>(quantity->degrees()) : std::nullopt;
}

[[nodiscard]] AssemblyLocalRelationshipInputSnapshot make_local_relationship_snapshot(
    const DocumentId& assembly_document, const AssemblyConstraint& constraint) {
  return AssemblyLocalRelationshipInputSnapshot{
      assembly_document, constraint.id(), constraint.name(), constraint.type(),
      constraint.target_a(), constraint.target_b(), constraint.state(),
      distance_mm(constraint.distance()), angle_deg(constraint.angle())};
}

[[nodiscard]] AssemblyProjectCrossHierarchyRelationshipInputSnapshot
make_cross_hierarchy_relationship_snapshot(const AssemblyHierarchyConstraint& constraint) {
  return AssemblyProjectCrossHierarchyRelationshipInputSnapshot{
      constraint.id(), constraint.name(), constraint.type(), constraint.target_a(),
      constraint.target_b(), constraint.state(), distance_mm(constraint.distance()),
      angle_deg(constraint.angle())};
}

[[nodiscard]] Result<AssemblyCrossHierarchyRelationshipInputSnapshot>
make_relationship_snapshot(const Project& project, const AssemblyRelationshipIdentity& identity) {
  if (const auto* local = std::get_if<AssemblyLocalRelationshipIdentity>(&identity)) {
    const AssemblyDocument* assembly = project.find_assembly_document(local->assembly_document);
    if (assembly == nullptr) {
      return Result<AssemblyCrossHierarchyRelationshipInputSnapshot>::failure(validation_error(
          local->assembly_document.value(),
          "cross-hierarchy relationship snapshot assembly document must exist"));
    }
    const AssemblyConstraint* constraint = assembly->find_constraint(local->constraint);
    if (constraint == nullptr) {
      return Result<AssemblyCrossHierarchyRelationshipInputSnapshot>::failure(validation_error(
          local->constraint.value(),
          "cross-hierarchy relationship snapshot local constraint must exist"));
    }
    return Result<AssemblyCrossHierarchyRelationshipInputSnapshot>::success(
        make_local_relationship_snapshot(local->assembly_document, *constraint));
  }

  const auto& cross =
      std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(identity);
  const AssemblyHierarchyConstraint* constraint =
      project.find_cross_hierarchy_constraint(cross.constraint);
  if (constraint == nullptr) {
    return Result<AssemblyCrossHierarchyRelationshipInputSnapshot>::failure(validation_error(
        cross.constraint.value(),
        "cross-hierarchy relationship snapshot project constraint must exist"));
  }
  return Result<AssemblyCrossHierarchyRelationshipInputSnapshot>::success(
      make_cross_hierarchy_relationship_snapshot(*constraint));
}

[[nodiscard]] AssemblyRelationshipIdentity relationship_identity(
    const AssemblyCrossHierarchyRelationshipInputSnapshot& snapshot) {
  if (const auto* local = std::get_if<AssemblyLocalRelationshipInputSnapshot>(&snapshot)) {
    return AssemblyLocalRelationshipIdentity{local->assembly_document, local->constraint};
  }
  return AssemblyProjectCrossHierarchyRelationshipIdentity{
      std::get<AssemblyProjectCrossHierarchyRelationshipInputSnapshot>(snapshot).constraint};
}

[[nodiscard]] Result<std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot>>
make_relationship_snapshots(const Project& project,
                            const std::vector<AssemblyRelationshipIdentity>& relationships) {
  std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot> snapshots;
  snapshots.reserve(relationships.size());
  for (const AssemblyRelationshipIdentity& relationship : relationships) {
    auto snapshot = make_relationship_snapshot(project, relationship);
    if (snapshot.has_error()) {
      return Result<std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot>>::failure(
          snapshot.error());
    }
    snapshots.push_back(std::move(snapshot.value()));
  }
  return Result<std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot>>::success(
      std::move(snapshots));
}

[[nodiscard]] std::vector<AssemblyHierarchyConstraintEndpoint> hierarchy_endpoints_from(
    const std::vector<AssemblyCrossHierarchyRelationshipInputSnapshot>& snapshots) {
  std::vector<AssemblyHierarchyConstraintEndpoint> endpoints;
  for (const auto& snapshot : snapshots) {
    if (const auto* cross =
            std::get_if<AssemblyProjectCrossHierarchyRelationshipInputSnapshot>(&snapshot)) {
      endpoints.push_back(cross->target_a);
      endpoints.push_back(cross->target_b);
    }
  }
  return endpoints;
}

[[nodiscard]] Result<std::size_t> validate_relationship_freshness(
    const Project& project, const AssemblyCrossHierarchySolveResult& result) {
  if (result.relationship_snapshots.size() != result.relationships.size()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.cross_hierarchy_application",
        "cross-hierarchy solve result relationship snapshot set is incomplete or contains extras"));
  }

  std::vector<AssemblyRelationshipIdentity> seen_relationships;
  seen_relationships.reserve(result.relationships.size());
  for (std::size_t index = 0U; index < result.relationships.size(); ++index) {
    const AssemblyRelationshipIdentity& identity = result.relationships[index];
    if (std::find(seen_relationships.begin(), seen_relationships.end(), identity) !=
        seen_relationships.end()) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_application",
          "cross-hierarchy solve result contains duplicate relationships"));
    }
    seen_relationships.push_back(identity);
    if (relationship_identity(result.relationship_snapshots[index]) != identity) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_application",
          "cross-hierarchy relationship snapshot identity/order does not match solve result"));
    }
    auto current = make_relationship_snapshot(project, identity);
    if (current.has_error()) {
      return Result<std::size_t>::failure(current.error());
    }
    if (current.value() != result.relationship_snapshots[index]) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_application",
          "cross-hierarchy solve result is stale because relationship input changed"));
    }
  }
  return Result<std::size_t>::success(result.relationships.size());
}

[[nodiscard]] Result<std::size_t> validate_group_freshness(
    const Project& project, const AssemblyCrossHierarchySolveResult& result,
    const std::vector<ComponentTransformAuthority>& current_authorities) {
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  if (graph.has_error()) {
    return Result<std::size_t>::failure(graph.error());
  }
  const AssemblyCrossHierarchySolveGroup expected_group{result.relationships, current_authorities};
  if (std::find(graph.value().solve_groups().begin(), graph.value().solve_groups().end(),
                expected_group) == graph.value().solve_groups().end()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.cross_hierarchy_application",
        "cross-hierarchy solve result is stale because active solve-group participation changed"));
  }
  return Result<std::size_t>::success(expected_group.relationships.size());
}

[[nodiscard]] Result<AssemblyCrossHierarchySolveResult> make_solve_result(
    const Project& source_project, const Project& solved_project,
    const AssemblyCrossHierarchySolveGroup& solve_group,
    const std::vector<ComponentTransformAuthority>& fixed_authorities,
    const std::vector<ComponentTransformAuthority>& variable_authorities,
    const detail::AssemblyNumericSolveOutcome& outcome) {
  auto authority_snapshots =
      detail::make_cross_hierarchy_authority_snapshots(source_project, solve_group.authorities);
  if (authority_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(authority_snapshots.error());
  }
  auto relationship_snapshots =
      make_relationship_snapshots(source_project, solve_group.relationships);
  if (relationship_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(relationship_snapshots.error());
  }
  auto hierarchy_boundary_snapshots = detail::make_cross_hierarchy_boundary_snapshots(
      source_project, hierarchy_endpoints_from(relationship_snapshots.value()));
  if (hierarchy_boundary_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(
        hierarchy_boundary_snapshots.error());
  }
  auto semantic_target_part_snapshots = detail::make_semantic_target_part_snapshots(
      source_project,
      detail::semantic_part_documents_from_authority_snapshots(authority_snapshots.value()));
  if (semantic_target_part_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(
        semantic_target_part_snapshots.error());
  }

  std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform> proposals;
  proposals.reserve(variable_authorities.size());
  for (const ComponentTransformAuthority& authority : variable_authorities) {
    auto source_component =
        detail::resolve_cross_hierarchy_authority_component(source_project, authority);
    if (source_component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(source_component.error());
    }
    auto solved_component =
        detail::resolve_cross_hierarchy_authority_component(solved_project, authority);
    if (solved_component.has_error()) {
      return Result<AssemblyCrossHierarchySolveResult>::failure(solved_component.error());
    }
    proposals.push_back(ProposedAssemblyCrossHierarchyAuthorityTransform{
        authority, source_component.value()->transform(), solved_component.value()->transform()});
  }

  return Result<AssemblyCrossHierarchySolveResult>::success(AssemblyCrossHierarchySolveResult{
      outcome.state,
      outcome.iterations,
      solve_group.relationships,
      fixed_authorities,
      std::move(authority_snapshots.value()),
      std::move(relationship_snapshots.value()),
      std::move(hierarchy_boundary_snapshots.value()),
      std::move(semantic_target_part_snapshots.value()),
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
    auto component = detail::resolve_cross_hierarchy_authority_component(project, authority);
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

  auto initial_variables =
      detail::read_cross_hierarchy_authority_variables(project, variable_authorities);
  if (initial_variables.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(initial_variables.error());
  }

  const detail::AssemblyNumericResidualEvaluator evaluator =
      [&project, &solve_group, &variable_authorities,
       length_scale = options.length_residual_scale_mm](const detail::NumericVector& values) {
        Project candidate_project = project;
        auto applied = detail::apply_cross_hierarchy_authority_variables(
            candidate_project, variable_authorities, values);
        if (applied.has_error()) {
          return Result<detail::NumericVector>::failure(applied.error());
        }
        return detail::evaluate_cross_hierarchy_group_residuals(
            candidate_project, solve_group, length_scale);
      };

  auto outcome = detail::solve_numeric_variables(initial_variables.value(),
                                                 !fixed_authorities.empty(), evaluator, options);
  if (outcome.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(outcome.error());
  }

  Project solved_project = project;
  auto applied = detail::apply_cross_hierarchy_authority_variables(
      solved_project, variable_authorities, outcome.value().final_variables);
  if (applied.has_error()) {
    return Result<AssemblyCrossHierarchySolveResult>::failure(applied.error());
  }

  return make_solve_result(project, solved_project, solve_group, fixed_authorities,
                           variable_authorities, outcome.value());
}

Result<std::size_t> AssemblyCrossHierarchySolveResultApplier::apply(
    Project& project, const AssemblyCrossHierarchySolveResult& result) const {
  if (!result.converged()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.cross_hierarchy_application",
        "only a converged cross-hierarchy solve result can be applied"));
  }

  auto structure = project.validate_assembly_structure();
  if (structure.has_error()) {
    return Result<std::size_t>::failure(structure.error());
  }

  auto current_authorities = detail::validate_cross_hierarchy_authority_and_proposal_freshness(
      project, result.fixed_authorities, result.authority_snapshots, result.proposed_transforms,
      "assembly.cross_hierarchy_application", "cross-hierarchy solve result");
  if (current_authorities.has_error()) {
    return Result<std::size_t>::failure(current_authorities.error());
  }
  auto relationships = validate_relationship_freshness(project, result);
  if (relationships.has_error()) {
    return Result<std::size_t>::failure(relationships.error());
  }
  auto group = validate_group_freshness(project, result, current_authorities.value());
  if (group.has_error()) {
    return Result<std::size_t>::failure(group.error());
  }
  auto boundaries = detail::validate_cross_hierarchy_boundary_freshness(
      project, hierarchy_endpoints_from(result.relationship_snapshots),
      result.hierarchy_boundary_snapshots, "assembly.cross_hierarchy_application",
      "cross-hierarchy solve result");
  if (boundaries.has_error()) {
    return Result<std::size_t>::failure(boundaries.error());
  }
  auto semantic_freshness = detail::validate_semantic_target_part_snapshots(
      project,
      detail::semantic_part_documents_from_authority_snapshots(result.authority_snapshots),
      result.semantic_target_part_snapshots);
  if (semantic_freshness.has_error()) {
    return Result<std::size_t>::failure(semantic_freshness.error());
  }

  Project candidate_project = project;
  auto applied = detail::apply_cross_hierarchy_authority_proposals(
      candidate_project, result.proposed_transforms, "assembly.cross_hierarchy_application");
  if (applied.has_error()) {
    return Result<std::size_t>::failure(applied.error());
  }

  project = std::move(candidate_project);
  return applied;
}

} // namespace blcad::geometry
