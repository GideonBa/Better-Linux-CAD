#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

AssemblyConstraintTarget make_target(const char* component) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), "feature.hole.seat");
  REQUIRE(target);
  return target.value();
}

Quantity angle(double degrees, const char* id) {
  auto quantity = Quantity::angle_deg(degrees, id);
  REQUIRE(quantity);
  return quantity.value();
}

Quantity length(double millimeters, const char* id) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  return quantity.value();
}

AssemblyJoint make_joint(const char* id = "joint.revolute",
                         AssemblyJointState state = AssemblyJointState::Active,
                         double coordinate_deg = 0.0) {
  auto joint = AssemblyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Revolute, make_target("component.a"),
      make_target("component.b"), state, angle(-90.0, id), angle(90.0, id),
      angle(coordinate_deg, id));
  REQUIRE(joint);
  return joint.value();
}

AssemblyDocument make_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.joints"), "JointAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  auto component_a = ComponentInstance::create(ComponentInstanceId("component.a"), "A",
                                                DocumentId("part.shared"));
  auto component_b = ComponentInstance::create(ComponentInstanceId("component.b"), "B",
                                                DocumentId("part.shared"));
  REQUIRE(component_a);
  REQUIRE(component_b);
  REQUIRE(assembly.value().add_component_instance(component_a.value()));
  REQUIRE(assembly.value().add_component_instance(component_b.value()));
  return assembly.value();
}

PartDocument make_part() {
  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);
  return part.value();
}

} // namespace

TEST_CASE("AssemblyJoint validates revolute limits and authored coordinates",
          "[core][assembly-joint]") {
  const auto target_a = make_target("component.a");
  const auto target_b = make_target("component.b");

  CHECK(AssemblyJoint::create(AssemblyJointId(""), "Joint", AssemblyJointType::Revolute,
                              target_a, target_b, AssemblyJointState::Active,
                              angle(-90.0, "lower"), angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.empty_name"), "",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, angle(-90.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.same"), "Same",
                              AssemblyJointType::Revolute, target_a, target_a,
                              AssemblyJointState::Active, angle(-90.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.kind"), "Kind",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, length(1.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.principal"), "Principal",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, angle(-181.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.order"), "Order",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, angle(20.0, "lower"),
                              angle(20.0, "upper"), angle(20.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.coordinate"), "Coordinate",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, angle(-30.0, "lower"),
                              angle(30.0, "upper"), angle(31.0, "coordinate"))
            .has_error());

  const AssemblyJoint joint = make_joint("joint.valid", AssemblyJointState::Inactive, 15.0);
  CHECK(joint.type() == AssemblyJointType::Revolute);
  CHECK(joint.state() == AssemblyJointState::Inactive);
  CHECK(joint.limits().lower_deg == -90.0);
  CHECK(joint.limits().upper_deg == 90.0);
  CHECK(joint.coordinate_deg() == 15.0);
  CHECK(to_string(joint.type()) == "revolute");
  CHECK(to_string(joint.state()) == "inactive");

  const auto updated = joint.with_coordinate(angle(-45.0, "joint.valid"));
  REQUIRE(updated);
  CHECK(updated.value().coordinate_deg() == -45.0);
  CHECK(joint.coordinate_deg() == 15.0);
  CHECK(joint.with_coordinate(angle(91.0, "joint.valid")).has_error());
}

TEST_CASE("AssemblyDocument stores joints without mutating component placement",
          "[core][assembly-joint]") {
  auto assembly = make_assembly();
  const RigidTransform transform_a =
      assembly.find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform transform_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();

  REQUIRE(assembly.add_joint(make_joint()));
  CHECK(assembly.joint_count() == 1U);
  CHECK(assembly.add_joint(make_joint()).has_error());
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        transform_a);
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        transform_b);

  const auto updated =
      assembly.set_joint_coordinate(AssemblyJointId("joint.revolute"), angle(30.0, "joint.revolute"));
  REQUIRE(updated);
  CHECK(assembly.find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() == 30.0);
  CHECK(assembly.set_joint_coordinate(AssemblyJointId("joint.revolute"),
                                      angle(120.0, "joint.revolute"))
            .has_error());

  auto missing_target = AssemblyJoint::create(
      AssemblyJointId("joint.missing"), "Missing", AssemblyJointType::Revolute,
      make_target("component.missing"), make_target("component.b"), AssemblyJointState::Active,
      angle(-90.0, "joint.missing"), angle(90.0, "joint.missing"),
      angle(0.0, "joint.missing"));
  REQUIRE(missing_target);
  CHECK(assembly.add_joint(missing_target.value()).has_error());
}

TEST_CASE("Revolute joint intent survives assembly and project JSON roundtrip",
          "[core][assembly-joint]") {
  auto assembly = make_assembly();
  REQUIRE(assembly.add_joint(make_joint("joint.revolute", AssemblyJointState::Active, 25.0)));

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("assembly_joints") != std::string::npos);
  CHECK(serialized.value().find("revolute") != std::string::npos);
  CHECK(serialized.value().find("coordinate") != std::string::npos);
  CHECK(serialized.value().find("\"unit\": \"deg\"") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().joint_count() == 1U);
  const AssemblyJoint* joint = restored.value().find_joint(AssemblyJointId("joint.revolute"));
  REQUIRE(joint != nullptr);
  CHECK(joint->target_a().semantic_reference() == "feature.hole.seat");
  CHECK(joint->target_b().component_instance().value() == "component.b");
  CHECK(joint->limits().lower_deg == -90.0);
  CHECK(joint->limits().upper_deg == 90.0);
  CHECK(joint->coordinate_deg() == 25.0);

  auto project = Project::create(DocumentId("project.joints"), "JointProject", assembly);
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_part()));
  REQUIRE(project.value().validate_assembly_structure());
  const auto project_json = serialize_project_to_json(project.value());
  REQUIRE(project_json);
  const auto restored_project = deserialize_project_from_json(project_json.value());
  REQUIRE(restored_project);
  REQUIRE(restored_project.value().validate_assembly_structure());
  CHECK(restored_project.value().assembly().joint_count() == 1U);
  CHECK(restored_project.value().assembly()
            .find_joint(AssemblyJointId("joint.revolute"))
            ->coordinate_deg() == 25.0);
}

TEST_CASE("Assembly JSON remains backward compatible without joint records",
          "[core][assembly-joint]") {
  const std::string content = R"({
    "schema": "blcad.assembly_document.mvp4",
    "version": 1,
    "document": {"id": "assembly.old", "name": "OldAssembly"},
    "parameters": [],
    "member_parts": [],
    "parameter_bindings": [],
    "component_instances": [],
    "assembly_constraints": []
  })";

  const auto restored = deserialize_assembly_document_from_json(content);
  REQUIRE(restored);
  CHECK(restored.value().joint_count() == 0U);
}
