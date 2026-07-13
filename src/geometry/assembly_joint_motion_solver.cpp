#include "blcad/geometry/assembly_joint_motion_solver.hpp"

#include "assembly_constraint_numeric_system.hpp"
#include "assembly_joint_drive_validation.hpp"
#include "assembly_numeric_solve_engine.hpp"

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/assembly_joint_graph.hpp"

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

[[nodiscard]] bool contains_joint(const std::vector<AssemblyJointId>& joints,
                                  const AssemblyJointId& joint) {
  return std::find(joints.begin(), joints.end(), joint) != joints.end();
}

void add_component(std::vector<ComponentInstanceId>& components,
                   const ComponentInstanceId& component) {
  if (!contains_component(components, component)) {
    components.push_back(component);
  }
}

[[nodiscard]] std::vector<ComponentInstanceId>
collect_combined_relationship_group(const AssemblyConstraintGraph& constraint_graph,
                                    const AssemblyJointGraph& joint_graph,
                                    const AssemblyJoint& selected_joint) {
  std::vector<ComponentInstanceId> group{selected_joint.target_a().component_instance(),
                                         selected_joint.target_b().component_instance()};

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& edge : constraint_graph.edges()) {
      if (!contains_component(group, edge.component_a()) &&
          !contains_component(group, edge.component_b())) {
        continue;
      }
      const std::size_t before = group.size();
      add_component(group, edge.component_a());
      add_component(group, edge.component_b());
      changed = changed || group.size() != before;
    }
    for (const auto& edge : joint_graph.edges()) {
      if (!contains_component(group, edge.component_a()) &&
          !contains_component(group, edge.component_b())) {
        continue;
      }
      const std::size_t before = group.size();
      add_component(group, edge.component_a());
      add_component(group, edge.component_b());
      changed = changed || group.size() != before;
    }
  }

  std::sort(group.begin(), group.end(),
            [](const ComponentInstanceId& lhs, const ComponentInstanceId& rhs) {
              return lhs.value() < rhs.value();
            });
  return group;
}

[[nodiscard]] AssemblyJointMotionInputSnapshot make_joint_snapshot(const AssemblyJoint& joint) {
  return AssemblyJointMotionInputSnapshot{
      joint.id(),       joint.type(),   joint.state(),          joint.target_a(),
      joint.target_b(), joint.limits(), joint.coordinate_deg(), joint.coordinate_slots()};
}

[[nodiscard]] bool
joint_snapshot_matches(const AssemblyJoint& joint,
                       const AssemblyJointMotionInputSnapshot& snapshot) noexcept {
  return joint.id() == snapshot.joint && joint.type() == snapshot.type &&
         joint.state() == snapshot.state && joint.target_a() == snapshot.target_a &&
         joint.target_b() == snapshot.target_b && joint.limits() == snapshot.limits &&
         joint.coordinate_deg() == snapshot.coordinate_deg &&
         joint.coordinate_slots() == snapshot.coordinate_slots;
}

} // namespace

Result<AssemblyJointMotionResult>
AssemblyJointMotionSolver::move(const Project& project, AssemblyJointId joint_id,
                                Quantity requested_coordinate,
                                AssemblyRigidBodySolverOptions options) const {
  if (requested_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(), "revolute joint motion coordinate must use degrees"));
  }
  const AssemblyJoint* joint = project.assembly().find_joint(joint_id);
  if (joint != nullptr && (requested_coordinate.degrees() < joint->limits().lower_deg ||
                           requested_coordinate.degrees() > joint->limits().upper_deg)) {
    return Result<AssemblyJointMotionResult>::failure(validation_error(
        joint_id.value(), "requested revolute joint coordinate must lie within joint limits"));
  }
  std::vector<AssemblyJointCoordinateDrive> requested;
  requested.push_back({AssemblyJointCoordinateRole::Rotation, std::move(requested_coordinate)});
  return move(project, AssemblyJointDrive{std::move(joint_id), std::move(requested)}, options);
}

Result<AssemblyJointMotionResult>
AssemblyJointMotionSolver::move(const Project& project, AssemblyJointDrive drive,
                                AssemblyRigidBodySolverOptions options) const {
  const AssemblyJointId& joint_id = drive.joint;
  auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<AssemblyJointMotionResult>::failure(valid_structure.error());
  }
  if (joint_id.empty()) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error("assembly_joint", "assembly joint id must not be empty"));
  }
  const AssemblyJoint* joint = project.assembly().find_joint(joint_id);
  if (joint == nullptr) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(), "assembly joint must exist in project assembly"));
  }
  if (joint->state() != AssemblyJointState::Active) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(), "revolute joint motion requires an active joint"));
  }
  if (joint->type() != AssemblyJointType::Revolute) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(), "joint motion seed supports only Revolute joints"));
  }

  auto selected_drive = detail::resolve_joint_drive(*joint, drive);
  if (selected_drive.has_error())
    return Result<AssemblyJointMotionResult>::failure(selected_drive.error());
  auto requested_rotation =
      detail::revolute_rotation_degrees(selected_drive.value().complete_coordinates, joint_id);
  if (requested_rotation.has_error())
    return Result<AssemblyJointMotionResult>::failure(requested_rotation.error());
  const double requested_deg = requested_rotation.value();

  const ComponentInstance* target_a =
      project.assembly().find_component_instance(joint->target_a().component_instance());
  const ComponentInstance* target_b =
      project.assembly().find_component_instance(joint->target_b().component_instance());
  if (target_a == nullptr || target_b == nullptr) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(), "revolute joint motion target components must exist"));
  }
  if (target_a->suppression_state() == ComponentSuppressionState::Suppressed ||
      target_b->suppression_state() == ComponentSuppressionState::Suppressed) {
    return Result<AssemblyJointMotionResult>::failure(
        validation_error(joint_id.value(),
                         "revolute joint motion requires active non-suppressed target components"));
  }

  auto constraint_graph = AssemblyConstraintGraph::build(project.assembly());
  if (constraint_graph.has_error()) {
    return Result<AssemblyJointMotionResult>::failure(constraint_graph.error());
  }
  auto joint_graph = AssemblyJointGraph::build(project.assembly());
  if (joint_graph.has_error()) {
    return Result<AssemblyJointMotionResult>::failure(joint_graph.error());
  }

  const std::vector<ComponentInstanceId> combined_group =
      collect_combined_relationship_group(constraint_graph.value(), joint_graph.value(), *joint);
  std::vector<ComponentInstanceId> active_subgroup;
  active_subgroup.reserve(combined_group.size());
  for (const auto& component_id : combined_group) {
    const ComponentInstance* component = project.assembly().find_component_instance(component_id);
    if (component->suppression_state() == ComponentSuppressionState::Active) {
      active_subgroup.push_back(component_id);
    }
  }

  auto constraint_ids = detail::collect_constraint_ids(constraint_graph.value(), active_subgroup);
  if (constraint_ids.has_error()) {
    return Result<AssemblyJointMotionResult>::failure(constraint_ids.error());
  }

  std::vector<detail::AssemblyRevoluteJointDrive> drives;
  std::vector<AssemblyJointMotionInputSnapshot> joint_snapshots;
  for (const auto& edge : joint_graph.value().edges()) {
    if (!contains_component(active_subgroup, edge.component_a()) ||
        !contains_component(active_subgroup, edge.component_b())) {
      continue;
    }
    const AssemblyJoint* active_joint = project.assembly().find_joint(edge.joint());
    if (active_joint == nullptr) {
      return Result<AssemblyJointMotionResult>::failure(validation_error(
          edge.joint().value(), "active joint graph edge must resolve to assembly joint intent"));
    }
    if (active_joint->type() != AssemblyJointType::Revolute) {
      return Result<AssemblyJointMotionResult>::failure(validation_error(
          active_joint->id().value(), "joint motion seed supports only Revolute joints"));
    }
    drives.push_back(detail::AssemblyRevoluteJointDrive{
        active_joint->id(),
        active_joint->id() == joint_id ? requested_deg : active_joint->coordinate_deg()});
    joint_snapshots.push_back(make_joint_snapshot(*active_joint));
  }

  auto solved = detail::solve_numeric_relationships(
      project, combined_group,
      detail::AssemblyNumericRelationshipSet{std::move(constraint_ids.value()), std::move(drives)},
      options);
  if (solved.has_error()) {
    return Result<AssemblyJointMotionResult>::failure(solved.error());
  }

  return Result<AssemblyJointMotionResult>::success(AssemblyJointMotionResult{
      joint->id(), joint->coordinate_deg(), requested_deg, std::move(joint_snapshots),
      std::move(solved.value()), std::move(selected_drive.value().requested_coordinates)});
}

Result<std::size_t>
AssemblyJointMotionResultApplier::apply(Project& project,
                                        const AssemblyJointMotionResult& result) const {
  if (!result.converged()) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "only a converged joint motion result can be applied"));
  }

  bool selected_joint_snapshot_found = false;
  std::vector<AssemblyJointId> seen_joints;
  seen_joints.reserve(result.joint_snapshots.size());
  for (const auto& snapshot : result.joint_snapshots) {
    if (contains_joint(seen_joints, snapshot.joint)) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.joint.value(), "joint motion result contains duplicate joint snapshots"));
    }
    seen_joints.push_back(snapshot.joint);

    const AssemblyJoint* joint = project.assembly().find_joint(snapshot.joint);
    if (joint == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.joint.value(), "joint motion result joint input no longer exists"));
    }
    if (!joint_snapshot_matches(*joint, snapshot)) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.joint.value(), "joint motion result is stale because joint input changed"));
    }

    if (snapshot.joint == result.joint) {
      selected_joint_snapshot_found = true;
      if (snapshot.coordinate_deg != result.source_coordinate_deg) {
        return Result<std::size_t>::failure(validation_error(
            result.joint.value(), "joint motion result selected joint snapshot is inconsistent"));
      }
    }
  }
  if (!selected_joint_snapshot_found) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "joint motion result must snapshot the selected joint"));
  }

  const AssemblyJoint* selected_joint = project.assembly().find_joint(result.joint);
  const bool has_vector_drive = !result.requested_coordinates.empty();
  std::vector<AssemblyJointCoordinateDrive> requested_coordinates = result.requested_coordinates;
  if (requested_coordinates.empty()) {
    auto legacy = Quantity::angle_deg(result.requested_coordinate_deg, result.joint.value());
    if (legacy.has_error())
      return Result<std::size_t>::failure(legacy.error());
    requested_coordinates.push_back({AssemblyJointCoordinateRole::Rotation, legacy.value()});
  }
  auto selected_drive = detail::resolve_joint_drive(
      *selected_joint, AssemblyJointDrive{result.joint, requested_coordinates});
  if (selected_drive.has_error())
    return Result<std::size_t>::failure(selected_drive.error());
  if (has_vector_drive &&
      result.requested_coordinates != selected_drive.value().requested_coordinates) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "joint motion result selected drive order is not canonical"));
  }
  auto requested_rotation =
      detail::revolute_rotation_degrees(selected_drive.value().complete_coordinates, result.joint);
  if (requested_rotation.has_error() ||
      requested_rotation.value() != result.requested_coordinate_deg) {
    return Result<std::size_t>::failure(validation_error(
        result.joint.value(), "joint motion result selected drive snapshot is inconsistent"));
  }

  Project candidate_project = project;
  const AssemblySolveResultApplier solve_applier;
  auto applied = solve_applier.apply(candidate_project, result.solve_result);
  if (applied.has_error()) {
    return Result<std::size_t>::failure(applied.error());
  }

  for (const auto& requested : selected_drive.value().requested_coordinates) {
    auto coordinate_updated = candidate_project.assembly().set_joint_coordinate_value(
        result.joint, requested.role, requested.requested_value);
    if (coordinate_updated.has_error())
      return Result<std::size_t>::failure(coordinate_updated.error());
  }

  project = std::move(candidate_project);
  return Result<std::size_t>::success(applied.value());
}

} // namespace blcad::geometry
