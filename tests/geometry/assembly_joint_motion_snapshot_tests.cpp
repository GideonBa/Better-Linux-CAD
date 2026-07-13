#include "blcad/geometry/assembly_joint_motion_solver.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

AssemblyConstraintTarget target(const char* component) {
  auto value =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), "feature.hole.seat");
  REQUIRE(value);
  return value.value();
}

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyJoint joint(const char* id, const char* component_a, const char* component_b) {
  auto value =
      AssemblyJoint::create(AssemblyJointId(id), id, AssemblyJointType::Revolute,
                            target(component_a), target(component_b), AssemblyJointState::Active,
                            angle(-90.0, id), angle(90.0, id), angle(0.0, id));
  REQUIRE(value);
  return value.value();
}

AssemblyJointMotionInputSnapshot snapshot(const AssemblyJoint& value) {
  return AssemblyJointMotionInputSnapshot{
      value.id(),       value.type(),   value.state(),          value.target_a(),
      value.target_b(), value.limits(), value.coordinate_deg(), value.coordinate_slots()};
}

Project make_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.snapshot"), "SnapshotAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  for (const char* id : {"component.a", "component.b", "component.c"}) {
    auto component =
        ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.shared"));
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }
  REQUIRE(assembly.value().add_joint(joint("joint.a", "component.a", "component.b")));
  REQUIRE(assembly.value().add_joint(joint("joint.z", "component.b", "component.c")));

  auto project =
      Project::create(DocumentId("project.snapshot"), "SnapshotProject", assembly.value());
  REQUIRE(project);
  return project.value();
}

} // namespace

TEST_CASE("Joint motion application rejects a changed non-selected driven joint",
          "[geometry][assembly-revolute-joint]") {
  Project project = make_project();
  const AssemblyJoint* joint_a = project.assembly().find_joint(AssemblyJointId("joint.a"));
  const AssemblyJoint* joint_z = project.assembly().find_joint(AssemblyJointId("joint.z"));
  REQUIRE(joint_a != nullptr);
  REQUIRE(joint_z != nullptr);

  const AssemblyJointMotionResult result{
      AssemblyJointId("joint.a"),
      0.0,
      10.0,
      {snapshot(*joint_a), snapshot(*joint_z)},
      AssemblySolveResult{AssemblySolveState::Converged, 0U, {}, {}, {}, {}, {}, {}},
      {}};

  REQUIRE(
      project.assembly().set_joint_coordinate(AssemblyJointId("joint.z"), angle(20.0, "joint.z")));
  const RigidTransform component_b_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

  const auto applied = AssemblyJointMotionResultApplier{}.apply(project, result);
  REQUIRE(applied.has_error());
  CHECK(applied.error().object_id() == "joint.z");
  CHECK(applied.error().message() == "joint motion result is stale because joint input changed");
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      component_b_before);
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.a"))->coordinate_deg() == 0.0);
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.z"))->coordinate_deg() == 20.0);
}
