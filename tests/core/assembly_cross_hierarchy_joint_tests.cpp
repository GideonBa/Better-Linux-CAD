#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;

namespace {

PartDocument part() {
  auto value = PartDocument::create(DocumentId("part.motion"), "MotionPart");
  REQUIRE(value);
  return value.value();
}

ComponentInstance component(const char* id,
                            ComponentGroundingState grounding = ComponentGroundingState::Free,
                            ComponentVisibility visibility = ComponentVisibility::Visible,
                            ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.motion"), visibility, suppression, grounding,
      identity_rigid_transform());
  REQUIRE(value);
  return value.value();
}

SubassemblyInstance occurrence(
    const char* id, ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto value = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId("assembly.child"), visibility, suppression,
      identity_rigid_transform());
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraintEndpoint endpoint(std::initializer_list<const char*> path,
                                             const char* component_id,
                                             const char* reference = "feature.hole.seat") {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) occurrence_path.emplace_back(id);
  auto value = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), reference);
  REQUIRE(value);
  return value.value();
}

AssemblyConstraintTarget local_target(const char* component_id,
                                      const char* reference = "feature.hole.seat") {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component_id), reference);
  REQUIRE(value);
  return value.value();
}

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyJoint cross_joint(
    const char* id = "joint.cross.revolute",
    AssemblyHierarchyConstraintEndpoint target_a = endpoint({}, "component.root"),
    AssemblyHierarchyConstraintEndpoint target_b =
        endpoint({"subassembly.left"}, "component.child"),
    AssemblyJointState state = AssemblyJointState::Active, double coordinate = 0.0) {
  auto value = AssemblyHierarchyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Revolute, std::move(target_a),
      std::move(target_b), state, angle(-90.0, id), angle(90.0, id), angle(coordinate, id));
  REQUIRE(value);
  return value.value();
}

AssemblyJoint local_joint(const char* id) {
  auto value = AssemblyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Revolute,
      local_target("component.child"), local_target("component.anchor"),
      AssemblyJointState::Active, angle(-90.0, id), angle(90.0, id), angle(0.0, id));
  REQUIRE(value);
  return value.value();
}

AssemblyConstraint local_constraint(const char* id) {
  auto value = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Concentric,
      local_target("component.child", "feature.hole.axis"),
      local_target("component.anchor", "feature.hole.axis"));
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraint cross_constraint(const char* id) {
  auto value = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Concentric,
      endpoint({}, "component.root", "feature.hole.axis"),
      endpoint({"subassembly.left"}, "component.child", "feature.hole.axis"));
  REQUIRE(value);
  return value.value();
}

Project base_project(bool repeated = false) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.motion")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(occurrence("subassembly.left")));
  if (repeated) REQUIRE(root.value().add_subassembly_instance(occurrence("subassembly.right")));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.motion")));
  REQUIRE(child.value().add_component_instance(component("component.child")));
  REQUIRE(child.value().add_component_instance(
      component("component.anchor", ComponentGroundingState::Grounded)));

  auto project = Project::create(DocumentId("project.motion"), "Motion", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("Project-level occurrence-qualified Revolute joint intent preserves hierarchy identity",
          "[core][assembly-cross-hierarchy-joint]") {
  const auto root = endpoint({}, "component.root");
  const auto nested = endpoint({"subassembly.left"}, "component.child");
  auto joint = AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.cross"), "Cross", AssemblyJointType::Revolute, root, nested,
      AssemblyJointState::Inactive, angle(-45.0, "joint.cross"), angle(80.0, "joint.cross"),
      angle(12.0, "joint.cross"));
  REQUIRE(joint);
  CHECK(joint.value().target_a() == root);
  CHECK(joint.value().target_b() == nested);
  CHECK(joint.value().state() == AssemblyJointState::Inactive);
  CHECK(joint.value().limits() == AssemblyJointLimits{-45.0, 80.0});
  CHECK(joint.value().coordinate_deg() == 12.0);

  CHECK(AssemblyHierarchyJoint::create(
            AssemblyJointId("joint.same"), "Same", AssemblyJointType::Revolute, root, root,
            AssemblyJointState::Active, angle(-45.0, "joint.same"), angle(45.0, "joint.same"),
            angle(0.0, "joint.same"))
            .has_error());

  const auto left = endpoint({"subassembly.left"}, "component.child");
  const auto right = endpoint({"subassembly.right"}, "component.child");
  CHECK(AssemblyHierarchyJoint::create(
            AssemblyJointId("joint.shared"), "Shared", AssemblyJointType::Revolute, left, right,
            AssemblyJointState::Active, angle(-90.0, "joint.shared"),
            angle(90.0, "joint.shared"), angle(0.0, "joint.shared"))
            .has_value());

  CHECK(AssemblyHierarchyJoint::create(
            AssemblyJointId("joint.bad_limits"), "Bad", AssemblyJointType::Revolute, root, nested,
            AssemblyJointState::Active, angle(20.0, "joint.bad_limits"),
            angle(10.0, "joint.bad_limits"), angle(15.0, "joint.bad_limits"))
            .has_error());
}

TEST_CASE("Project owns cross-hierarchy joint ids independently from local joint ids",
          "[core][assembly-cross-hierarchy-joint]") {
  Project project = base_project();
  AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  REQUIRE(child->add_joint(local_joint("joint.shared_id")));
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint("joint.shared_id")));
  CHECK(project.cross_hierarchy_joint_count() == 1U);
  CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.shared_id")) != nullptr);
  CHECK(project.add_cross_hierarchy_joint(cross_joint("joint.shared_id")).has_error());

  const RigidTransform root_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform();
  const RigidTransform child_transform =
      child->find_component_instance(ComponentInstanceId("component.child"))->transform();
  const RigidTransform boundary =
      project.assembly().find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))->transform();
  REQUIRE(project.set_cross_hierarchy_joint_coordinate(AssemblyJointId("joint.shared_id"),
                                                       angle(30.0, "joint.shared_id")));
  CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.shared_id"))->coordinate_deg() ==
        30.0);
  CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform() ==
        root_transform);
  CHECK(child->find_component_instance(ComponentInstanceId("component.child"))->transform() ==
        child_transform);
  CHECK(project.assembly().find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))->transform() ==
        boundary);
}

TEST_CASE("Cross-hierarchy Revolute Project JSON is additive and exact",
          "[core][assembly-cross-hierarchy-joint-json]") {
  Project project = base_project(true);
  const auto target_a = endpoint({}, "component.root", "feature.root.seat");
  const auto target_b = endpoint({"subassembly.right"}, "component.child", "feature.child.seat");
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint(
      "joint.cross.json", target_a, target_b, AssemblyJointState::Inactive, 25.0)));

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);
  CHECK(serialized.value().find("cross_hierarchy_joints") != std::string::npos);
  auto loaded = deserialize_project_from_json(serialized.value());
  REQUIRE(loaded);
  REQUIRE(loaded.value().cross_hierarchy_joint_count() == 1U);
  const AssemblyHierarchyJoint* joint =
      loaded.value().find_cross_hierarchy_joint(AssemblyJointId("joint.cross.json"));
  REQUIRE(joint != nullptr);
  CHECK(joint->target_a() == target_a);
  CHECK(joint->target_b() == target_b);
  CHECK(joint->state() == AssemblyJointState::Inactive);
  CHECK(joint->limits() == AssemblyJointLimits{-90.0, 90.0});
  CHECK(joint->coordinate_deg() == 25.0);

  std::string legacy = serialized.value();
  const auto field = legacy.find("\"cross_hierarchy_joints\"");
  REQUIRE(field != std::string::npos);
  const auto array_begin = legacy.find('[', field);
  const auto array_end = legacy.find(']', array_begin);
  REQUIRE(array_begin != std::string::npos);
  REQUIRE(array_end != std::string::npos);
  const auto comma = legacy.find(',', array_end);
  REQUIRE(comma != std::string::npos);
  legacy.erase(field, comma - field + 1U);
  auto legacy_loaded = deserialize_project_from_json(legacy);
  REQUIRE(legacy_loaded);
  CHECK(legacy_loaded.value().cross_hierarchy_joint_count() == 0U);
}

TEST_CASE("Cross-hierarchy joint structure validation follows exact rooted paths",
          "[core][assembly-cross-hierarchy-joint][core][assembly-cross-hierarchy-joint-json]") {
  Project project = base_project();
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint()));
  REQUIRE(project.validate_cross_hierarchy_joints());

  auto missing_path = AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.missing_path"), "MissingPath", AssemblyJointType::Revolute,
      endpoint({}, "component.root"), endpoint({"subassembly.missing"}, "component.child"),
      AssemblyJointState::Active, angle(-90.0, "joint.missing_path"),
      angle(90.0, "joint.missing_path"), angle(0.0, "joint.missing_path"));
  REQUIRE(missing_path);
  REQUIRE(project.add_cross_hierarchy_joint(missing_path.value()));
  CHECK(project.validate_assembly_structure().has_error());
}

TEST_CASE("Combined motion graph closes over local and cross geometry and joints deterministically",
          "[core][assembly-cross-hierarchy-motion-graph]") {
  Project project = base_project();
  AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  REQUIRE(child->add_constraint(local_constraint("constraint.local")));
  REQUIRE(child->add_joint(local_joint("joint.local")));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_constraint("constraint.cross")));
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint("joint.cross")));
  REQUIRE(project.validate_assembly_structure());

  auto graph = AssemblyCrossHierarchyMotionGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().motion_group_count() == 1U);
  const auto& group = graph.value().motion_groups().front();
  REQUIRE(group.relationships.size() == 4U);
  CHECK(std::holds_alternative<AssemblyLocalRelationshipIdentity>(group.relationships[0]));
  CHECK(std::holds_alternative<AssemblyLocalJointIdentity>(group.relationships[1]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(
      group.relationships[2]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyJointIdentity>(group.relationships[3]));
  CHECK(group.authorities.size() == 3U);

  Project reverse = base_project();
  AssemblyDocument* reverse_child = reverse.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(reverse_child != nullptr);
  REQUIRE(reverse.add_cross_hierarchy_joint(cross_joint("joint.cross")));
  REQUIRE(reverse.add_cross_hierarchy_constraint(cross_constraint("constraint.cross")));
  REQUIRE(reverse_child->add_joint(local_joint("joint.local")));
  REQUIRE(reverse_child->add_constraint(local_constraint("constraint.local")));
  auto reverse_graph = AssemblyCrossHierarchyMotionGraph::build(reverse);
  REQUIRE(reverse_graph);
  CHECK(reverse_graph.value().motion_groups() == graph.value().motion_groups());
}

TEST_CASE("Repeated cross-hierarchy joint endpoints retain two paths and one shared authority incidence",
          "[core][assembly-cross-hierarchy-motion-graph]") {
  Project project = base_project(true);
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint(
      "joint.same_authority", endpoint({"subassembly.left"}, "component.child"),
      endpoint({"subassembly.right"}, "component.child"))));
  auto graph = AssemblyCrossHierarchyMotionGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().motion_group_count() == 1U);
  REQUIRE(graph.value().joint_endpoint_mapping_count() == 2U);
  CHECK(graph.value().joint_endpoint_mappings()[0].endpoint.occurrence_path() ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.left")});
  CHECK(graph.value().joint_endpoint_mappings()[1].endpoint.occurrence_path() ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.right")});
  CHECK(graph.value().joint_endpoint_mappings()[0].authority ==
        graph.value().joint_endpoint_mappings()[1].authority);
  CHECK(graph.value().incidence_count() == 1U);
  CHECK(graph.value().motion_groups().front().authorities.size() == 1U);
}

TEST_CASE("Motion graph participation uses suppression but ignores visibility",
          "[core][assembly-cross-hierarchy-motion-graph]") {
  Project visible = base_project();
  REQUIRE(visible.add_cross_hierarchy_joint(cross_joint()));
  auto visible_graph = AssemblyCrossHierarchyMotionGraph::build(visible);
  REQUIRE(visible_graph);
  CHECK(visible_graph.value().motion_group_count() == 1U);

  Project hidden = visible;
  REQUIRE(hidden.assembly().set_subassembly_instance_visibility(
      SubassemblyInstanceId("subassembly.left"), ComponentVisibility::Hidden));
  REQUIRE(hidden.find_assembly_document(DocumentId("assembly.child"))
              ->set_component_instance_visibility(ComponentInstanceId("component.child"),
                                                  ComponentVisibility::Hidden));
  auto hidden_graph = AssemblyCrossHierarchyMotionGraph::build(hidden);
  REQUIRE(hidden_graph);
  CHECK(hidden_graph.value().motion_groups() == visible_graph.value().motion_groups());

  Project suppressed = visible;
  REQUIRE(suppressed.assembly().set_subassembly_instance_suppression_state(
      SubassemblyInstanceId("subassembly.left"), ComponentSuppressionState::Suppressed));
  auto suppressed_graph = AssemblyCrossHierarchyMotionGraph::build(suppressed);
  REQUIRE(suppressed_graph);
  CHECK(suppressed_graph.value().motion_group_count() == 0U);
}
