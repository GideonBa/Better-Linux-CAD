#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"

#include "assembly_cross_hierarchy_freshness.hpp"
#include "assembly_cross_hierarchy_motion_numeric_system.hpp"
#include "assembly_cross_hierarchy_numeric_system.hpp"
#include "assembly_numeric_solve_engine.hpp"
#include "assembly_semantic_target_freshness.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
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

[[nodiscard]] AssemblyLocalRelationshipInputSnapshot make_local_constraint_snapshot(
    const DocumentId& assembly_document, const AssemblyConstraint& constraint) {
  return AssemblyLocalRelationshipInputSnapshot{
      assembly_document, constraint.id(), constraint.name(), constraint.type(),
      constraint.target_a(), constraint.target_b(), constraint.state(),
      distance_mm(constraint.distance()), angle_deg(constraint.angle())};
}

[[nodiscard]] AssemblyProjectCrossHierarchyRelationshipInputSnapshot
make_cross_constraint_snapshot(const AssemblyHierarchyConstraint& constraint) {
  return AssemblyProjectCrossHierarchyRelationshipInputSnapshot{
      constraint.id(), constraint.name(), constraint.type(), constraint.target_a(),
      constraint.target_b(), constraint.state(), distance_mm(constraint.distance()),
      angle_deg(constraint.angle())};
}

[[nodiscard]] AssemblyLocalJointMotionInputSnapshot make_local_joint_snapshot(
    const DocumentId& assembly_document, const AssemblyJoint& joint) {
  return AssemblyLocalJointMotionInputSnapshot{
      assembly_document, joint.id(), joint.name(), joint.type(), joint.target_a(), joint.target_b(),
      joint.state(), joint.limits(), joint.coordinate_deg()};
}

[[nodiscard]] AssemblyProjectCrossHierarchyJointMotionInputSnapshot make_cross_joint_snapshot(
    const AssemblyHierarchyJoint& joint) {
  return AssemblyProjectCrossHierarchyJointMotionInputSnapshot{
      joint.id(), joint.name(), joint.type(), joint.target_a(), joint.target_b(), joint.state(),
      joint.limits(), joint.coordinate_deg()};
}

[[nodiscard]] Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>
make_motion_relationship_snapshot(const Project& project,
                                  const AssemblyMotionRelationshipIdentity& identity) {
  return std::visit(
      [&](const auto& value) -> Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot> {
        using Identity = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Identity, AssemblyLocalRelationshipIdentity>) {
          const AssemblyDocument* assembly = project.find_assembly_document(value.assembly_document);
          if (assembly == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.assembly_document.value(),
                                 "cross-hierarchy motion local constraint assembly must exist"));
          }
          const AssemblyConstraint* constraint = assembly->find_constraint(value.constraint);
          if (constraint == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.constraint.value(),
                                 "cross-hierarchy motion local constraint must exist"));
          }
          return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::success(
              make_local_constraint_snapshot(value.assembly_document, *constraint));
        } else if constexpr (std::is_same_v<Identity, AssemblyLocalJointIdentity>) {
          const AssemblyDocument* assembly = project.find_assembly_document(value.assembly_document);
          if (assembly == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.assembly_document.value(),
                                 "cross-hierarchy motion local joint assembly must exist"));
          }
          const AssemblyJoint* joint = assembly->find_joint(value.joint);
          if (joint == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.joint.value(), "cross-hierarchy motion local joint must exist"));
          }
          return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::success(
              make_local_joint_snapshot(value.assembly_document, *joint));
        } else if constexpr (
            std::is_same_v<Identity, AssemblyProjectCrossHierarchyRelationshipIdentity>) {
          const AssemblyHierarchyConstraint* constraint =
              project.find_cross_hierarchy_constraint(value.constraint);
          if (constraint == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.constraint.value(),
                                 "cross-hierarchy motion project constraint must exist"));
          }
          return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::success(
              make_cross_constraint_snapshot(*constraint));
        } else {
          const AssemblyHierarchyJoint* joint = project.find_cross_hierarchy_joint(value.joint);
          if (joint == nullptr) {
            return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::failure(
                validation_error(value.joint.value(),
                                 "cross-hierarchy motion project joint must exist"));
          }
          return Result<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>::success(
              make_cross_joint_snapshot(*joint));
        }
      },
      identity);
}

[[nodiscard]] AssemblyMotionRelationshipIdentity motion_relationship_identity(
    const AssemblyCrossHierarchyMotionRelationshipInputSnapshot& snapshot) {
  return std::visit(
      [](const auto& value) -> AssemblyMotionRelationshipIdentity {
        using Snapshot = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Snapshot, AssemblyLocalRelationshipInputSnapshot>) {
          return AssemblyLocalRelationshipIdentity{value.assembly_document, value.constraint};
        } else if constexpr (std::is_same_v<Snapshot, AssemblyLocalJointMotionInputSnapshot>) {
          return AssemblyLocalJointIdentity{value.assembly_document, value.joint};
        } else if constexpr (
            std::is_same_v<Snapshot, AssemblyProjectCrossHierarchyRelationshipInputSnapshot>) {
          return AssemblyProjectCrossHierarchyRelationshipIdentity{value.constraint};
        } else {
          return AssemblyProjectCrossHierarchyJointIdentity{value.joint};
        }
      },
      snapshot);
}

[[nodiscard]] Result<std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>>
make_motion_relationship_snapshots(
    const Project& project, const std::vector<AssemblyMotionRelationshipIdentity>& relationships) {
  std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot> snapshots;
  snapshots.reserve(relationships.size());
  for (const AssemblyMotionRelationshipIdentity& relationship : relationships) {
    auto snapshot = make_motion_relationship_snapshot(project, relationship);
    if (snapshot.has_error()) {
      return Result<std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>>::failure(
          snapshot.error());
    }
    snapshots.push_back(std::move(snapshot.value()));
  }
  return Result<std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>>::success(
      std::move(snapshots));
}

[[nodiscard]] std::vector<AssemblyHierarchyConstraintEndpoint> hierarchy_endpoints_from(
    const std::vector<AssemblyCrossHierarchyMotionRelationshipInputSnapshot>& snapshots) {
  std::vector<AssemblyHierarchyConstraintEndpoint> endpoints;
  for (const auto& snapshot : snapshots) {
    if (const auto* constraint =
            std::get_if<AssemblyProjectCrossHierarchyRelationshipInputSnapshot>(&snapshot)) {
      endpoints.push_back(constraint->target_a);
      endpoints.push_back(constraint->target_b);
    } else if (const auto* joint =
                   std::get_if<AssemblyProjectCrossHierarchyJointMotionInputSnapshot>(&snapshot)) {
      endpoints.push_back(joint->target_a);
      endpoints.push_back(joint->target_b);
    }
  }
  return endpoints;
}

[[nodiscard]] Result<std::size_t> validate_motion_relationship_freshness(
    const Project& project, const AssemblyCrossHierarchyJointMotionResult& result) {
  if (result.relationship_snapshots.size() != result.relationships.size()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.cross_hierarchy_motion_application",
        "cross-hierarchy motion result relationship snapshot set is incomplete or contains extras"));
  }

  std::vector<AssemblyMotionRelationshipIdentity> seen;
  seen.reserve(result.relationships.size());
  for (std::size_t index = 0U; index < result.relationships.size(); ++index) {
    const AssemblyMotionRelationshipIdentity& identity = result.relationships[index];
    if (std::find(seen.begin(), seen.end(), identity) != seen.end()) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_motion_application",
          "cross-hierarchy motion result contains duplicate relationships"));
    }
    seen.push_back(identity);
    if (motion_relationship_identity(result.relationship_snapshots[index]) != identity) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_motion_application",
          "cross-hierarchy motion relationship snapshot identity/order does not match result"));
    }
    auto current = make_motion_relationship_snapshot(project, identity);
    if (current.has_error()) {
      return Result<std::size_t>::failure(current.error());
    }
    if (current.value() != result.relationship_snapshots[index]) {
      return Result<std::size_t>::failure(validation_error(
          "assembly.cross_hierarchy_motion_application",
          "cross-hierarchy motion result is stale because relationship input changed"));
    }
  }
  return Result<std::size_t>::success(result.relationships.size());
}

[[nodiscard]] Result<std::size_t> validate_motion_group_freshness(
    const Project& project, const AssemblyCrossHierarchyJointMotionResult& result,
    const std::vector<ComponentTransformAuthority>& current_authorities) {
  auto graph = AssemblyCrossHierarchyMotionGraph::build(project);
  if (graph.has_error()) {
    return Result<std::size_t>::failure(graph.error());
  }
  const AssemblyCrossHierarchyMotionGroup expected_group{result.relationships, current_authorities};
  if (std::find(graph.value().motion_groups().begin(), graph.value().motion_groups().end(),
                expected_group) == graph.value().motion_groups().end()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.cross_hierarchy_motion_application",
        "cross-hierarchy motion result is stale because active motion-group participation changed"));
  }
  return Result<std::size_t>::success(expected_group.relationships.size());
}

[[nodiscard]] Result<std::size_t> validate_selected_joint_snapshot(
    const AssemblyCrossHierarchyJointMotionResult& result) {
  const AssemblyProjectCrossHierarchyJointMotionInputSnapshot* selected = nullptr;
  for (const auto& snapshot : result.relationship_snapshots) {
    const auto* joint =
        std::get_if<AssemblyProjectCrossHierarchyJointMotionInputSnapshot>(&snapshot);
    if (joint == nullptr || joint->joint != result.joint) {
      continue;
    }
    if (selected != nullptr) {
      return Result<std::size_t>::failure(validation_error(
          result.joint.value(), "cross-hierarchy motion result duplicates selected joint snapshot"));
    }
    selected = joint;
  }

  if (selected == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "cross-hierarchy motion result must snapshot the selected joint"));
  }
  if (selected->state != AssemblyJointState::Active ||
      selected->type != AssemblyJointType::Revolute ||
      selected->coordinate_deg != result.source_coordinate_deg ||
      result.requested_coordinate_deg < selected->limits.lower_deg ||
      result.requested_coordinate_deg > selected->limits.upper_deg) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "cross-hierarchy motion selected joint snapshot is inconsistent"));
  }
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<AssemblyCrossHierarchyJointMotionResult> make_motion_result(
    const Project& source_project, const Project& solved_project,
    const AssemblyCrossHierarchyMotionGroup& motion_group, const AssemblyHierarchyJoint& selected_joint,
    double requested_coordinate_deg,
    const std::vector<ComponentTransformAuthority>& fixed_authorities,
    const std::vector<ComponentTransformAuthority>& variable_authorities,
    const detail::AssemblyNumericSolveOutcome& outcome) {
  auto authority_snapshots =
      detail::make_cross_hierarchy_authority_snapshots(source_project, motion_group.authorities);
  if (authority_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(authority_snapshots.error());
  }
  auto relationship_snapshots =
      make_motion_relationship_snapshots(source_project, motion_group.relationships);
  if (relationship_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(relationship_snapshots.error());
  }
  auto boundary_snapshots = detail::make_cross_hierarchy_boundary_snapshots(
      source_project, hierarchy_endpoints_from(relationship_snapshots.value()));
  if (boundary_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(boundary_snapshots.error());
  }
  auto semantic_snapshots = detail::make_semantic_target_part_snapshots(
      source_project,
      detail::semantic_part_documents_from_authority_snapshots(authority_snapshots.value()));
  if (semantic_snapshots.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(semantic_snapshots.error());
  }

  std::vector<ProposedAssemblyCrossHierarchyAuthorityTransform> proposals;
  proposals.reserve(variable_authorities.size());
  for (const ComponentTransformAuthority& authority : variable_authorities) {
    auto source_component =
        detail::resolve_cross_hierarchy_authority_component(source_project, authority);
    if (source_component.has_error()) {
      return Result<AssemblyCrossHierarchyJointMotionResult>::failure(source_component.error());
    }
    auto solved_component =
        detail::resolve_cross_hierarchy_authority_component(solved_project, authority);
    if (solved_component.has_error()) {
      return Result<AssemblyCrossHierarchyJointMotionResult>::failure(solved_component.error());
    }
    proposals.push_back(ProposedAssemblyCrossHierarchyAuthorityTransform{
        authority, source_component.value()->transform(), solved_component.value()->transform()});
  }

  return Result<AssemblyCrossHierarchyJointMotionResult>::success(
      AssemblyCrossHierarchyJointMotionResult{
          selected_joint.id(), selected_joint.coordinate_deg(), requested_coordinate_deg,
          outcome.state, outcome.iterations, motion_group.relationships, fixed_authorities,
          std::move(authority_snapshots.value()), std::move(relationship_snapshots.value()),
          std::move(boundary_snapshots.value()), std::move(semantic_snapshots.value()),
          std::move(proposals),
          AssemblySolveResidualSummary{outcome.final_residuals.size(),
                                       detail::residual_rms(outcome.initial_residuals),
                                       detail::residual_rms(outcome.final_residuals),
                                       detail::residual_max_abs(outcome.final_residuals)}});
}

} // namespace

Result<AssemblyCrossHierarchyJointMotionResult> AssemblyCrossHierarchyJointMotionSolver::move(
    const Project& project, AssemblyJointId joint_id, Quantity requested_coordinate,
    AssemblyRigidBodySolverOptions options) const {
  auto structure = project.validate_assembly_structure();
  if (structure.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(structure.error());
  }
  if (joint_id.empty()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        "assembly_hierarchy_joint", "cross-hierarchy motion joint id must not be empty"));
  }
  if (requested_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "cross-hierarchy Revolute motion coordinate must use degrees"));
  }

  const AssemblyHierarchyJoint* selected_joint = project.find_cross_hierarchy_joint(joint_id);
  if (selected_joint == nullptr) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "cross-hierarchy motion joint must exist in project"));
  }
  if (selected_joint->state() != AssemblyJointState::Active) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "cross-hierarchy Revolute motion requires an active joint"));
  }
  if (selected_joint->type() != AssemblyJointType::Revolute) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "cross-hierarchy motion supports only Revolute joints"));
  }

  const double requested_deg = requested_coordinate.degrees();
  if (requested_deg < selected_joint->limits().lower_deg ||
      requested_deg > selected_joint->limits().upper_deg) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "requested cross-hierarchy Revolute coordinate must lie within limits"));
  }

  auto graph = AssemblyCrossHierarchyMotionGraph::build(project);
  if (graph.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(graph.error());
  }
  const AssemblyMotionRelationshipIdentity selected_identity =
      AssemblyProjectCrossHierarchyJointIdentity{joint_id};
  const AssemblyCrossHierarchyMotionGroup* motion_group = nullptr;
  for (const AssemblyCrossHierarchyMotionGroup& candidate : graph.value().motion_groups()) {
    if (std::find(candidate.relationships.begin(), candidate.relationships.end(), selected_identity) ==
        candidate.relationships.end()) {
      continue;
    }
    if (motion_group != nullptr) {
      return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
          joint_id.value(), "selected cross-hierarchy joint must belong to exactly one motion group"));
    }
    motion_group = &candidate;
  }
  if (motion_group == nullptr) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
        joint_id.value(), "selected cross-hierarchy joint must participate in an active motion group"));
  }

  std::vector<ComponentTransformAuthority> fixed_authorities;
  std::vector<ComponentTransformAuthority> variable_authorities;
  for (const ComponentTransformAuthority& authority : motion_group->authorities) {
    auto component = detail::resolve_cross_hierarchy_authority_component(project, authority);
    if (component.has_error()) {
      return Result<AssemblyCrossHierarchyJointMotionResult>::failure(component.error());
    }
    if (component.value()->suppression_state() != ComponentSuppressionState::Active) {
      return Result<AssemblyCrossHierarchyJointMotionResult>::failure(validation_error(
          authority.component_instance.value(), "cross-hierarchy motion authority must remain active"));
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
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(initial_variables.error());
  }

  const detail::AssemblyNumericResidualEvaluator evaluator =
      [&project, motion_group, &variable_authorities, joint_id, requested_deg,
       length_scale = options.length_residual_scale_mm](const detail::NumericVector& values) {
        Project candidate_project = project;
        auto applied = detail::apply_cross_hierarchy_authority_variables(
            candidate_project, variable_authorities, values);
        if (applied.has_error()) {
          return Result<detail::NumericVector>::failure(applied.error());
        }
        return detail::evaluate_cross_hierarchy_motion_group_residuals(
            candidate_project, *motion_group, joint_id, requested_deg, length_scale);
      };

  auto outcome = detail::solve_numeric_variables(initial_variables.value(),
                                                 !fixed_authorities.empty(), evaluator, options);
  if (outcome.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(outcome.error());
  }

  Project solved_project = project;
  auto applied = detail::apply_cross_hierarchy_authority_variables(
      solved_project, variable_authorities, outcome.value().final_variables);
  if (applied.has_error()) {
    return Result<AssemblyCrossHierarchyJointMotionResult>::failure(applied.error());
  }

  return make_motion_result(project, solved_project, *motion_group, *selected_joint, requested_deg,
                            fixed_authorities, variable_authorities, outcome.value());
}

Result<std::size_t> AssemblyCrossHierarchyJointMotionResultApplier::apply(
    Project& project, const AssemblyCrossHierarchyJointMotionResult& result) const {
  if (!result.converged()) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "only a converged cross-hierarchy motion result can be applied"));
  }

  auto structure = project.validate_assembly_structure();
  if (structure.has_error()) {
    return Result<std::size_t>::failure(structure.error());
  }

  auto current_authorities = detail::validate_cross_hierarchy_authority_and_proposal_freshness(
      project, result.fixed_authorities, result.authority_snapshots, result.proposed_transforms,
      "assembly.cross_hierarchy_motion_application", "cross-hierarchy motion result");
  if (current_authorities.has_error()) {
    return Result<std::size_t>::failure(current_authorities.error());
  }
  auto relationships = validate_motion_relationship_freshness(project, result);
  if (relationships.has_error()) {
    return Result<std::size_t>::failure(relationships.error());
  }
  auto selected = validate_selected_joint_snapshot(result);
  if (selected.has_error()) {
    return Result<std::size_t>::failure(selected.error());
  }
  auto group = validate_motion_group_freshness(project, result, current_authorities.value());
  if (group.has_error()) {
    return Result<std::size_t>::failure(group.error());
  }
  auto boundaries = detail::validate_cross_hierarchy_boundary_freshness(
      project, hierarchy_endpoints_from(result.relationship_snapshots),
      result.hierarchy_boundary_snapshots, "assembly.cross_hierarchy_motion_application",
      "cross-hierarchy motion result");
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
      candidate_project, result.proposed_transforms,
      "assembly.cross_hierarchy_motion_application");
  if (applied.has_error()) {
    return Result<std::size_t>::failure(applied.error());
  }
  auto requested = Quantity::angle_deg(result.requested_coordinate_deg, result.joint.value());
  if (requested.has_error()) {
    return Result<std::size_t>::failure(requested.error());
  }
  auto coordinate_updated =
      candidate_project.set_cross_hierarchy_joint_coordinate(result.joint, requested.value());
  if (coordinate_updated.has_error()) {
    return Result<std::size_t>::failure(coordinate_updated.error());
  }

  project = std::move(candidate_project);
  return applied;
}

} // namespace blcad::geometry
