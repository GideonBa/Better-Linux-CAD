#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>

using namespace blcad;

namespace {

AssemblyConstraintTarget make_target(const char* component) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), "feature.hole.seat");
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

Quantity displacement(double millimeters, const char* id) {
  auto quantity = Quantity::linear_displacement_mm(millimeters, id);
  REQUIRE(quantity);
  return quantity.value();
}

AssemblyJoint make_joint(const char* id = "joint.revolute",
                         AssemblyJointState state = AssemblyJointState::Active,
                         double coordinate_deg = 0.0) {
  auto joint = AssemblyJoint::create(AssemblyJointId(id), id, AssemblyJointType::Revolute,
                                     make_target("component.a"), make_target("component.b"), state,
                                     angle(-90.0, id), angle(90.0, id), angle(coordinate_deg, id));
  REQUIRE(joint);
  return joint.value();
}

AssemblyDocument make_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.joints"), "JointAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  auto component_a =
      ComponentInstance::create(ComponentInstanceId("component.a"), "A", DocumentId("part.shared"));
  auto component_b =
      ComponentInstance::create(ComponentInstanceId("component.b"), "B", DocumentId("part.shared"));
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

  CHECK(AssemblyJoint::create(AssemblyJointId(""), "Joint", AssemblyJointType::Revolute, target_a,
                              target_b, AssemblyJointState::Active, angle(-90.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.empty_name"), "", AssemblyJointType::Revolute,
                              target_a, target_b, AssemblyJointState::Active, angle(-90.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.same"), "Same", AssemblyJointType::Revolute,
                              target_a, target_a, AssemblyJointState::Active, angle(-90.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.kind"), "Kind", AssemblyJointType::Revolute,
                              target_a, target_b, AssemblyJointState::Active, length(1.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.principal"), "Principal",
                              AssemblyJointType::Revolute, target_a, target_b,
                              AssemblyJointState::Active, angle(-181.0, "lower"),
                              angle(90.0, "upper"), angle(0.0, "coordinate"))
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.order"), "Order", AssemblyJointType::Revolute,
                              target_a, target_b, AssemblyJointState::Active, angle(20.0, "lower"),
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

TEST_CASE("Assembly joint coordinate slots preserve family roles and physical kinds",
          "[core][assembly-joint-coordinate-model]") {
  CHECK(to_string(AssemblyJointCoordinateKind::Angular) == "angular");
  CHECK(to_string(AssemblyJointCoordinateKind::Linear) == "linear");
  CHECK(to_string(AssemblyJointCoordinateRole::Rotation) == "rotation");
  CHECK(to_string(AssemblyJointCoordinateRole::Translation) == "translation");
  CHECK(to_string(AssemblyJointCoordinateRole::TranslationU) == "translation_u");
  CHECK(to_string(AssemblyJointCoordinateRole::TranslationV) == "translation_v");
  CHECK(to_string(AssemblyJointCoordinateRole::RotationNormal) == "rotation_normal");

  auto rotation = AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateKind::Angular,
      angle(15.0, "rotation"), angle(-45.0, "rotation"), angle(60.0, "rotation"));
  REQUIRE(rotation);
  CHECK(rotation.value().value().degrees() == 15.0);
  REQUIRE(rotation.value().lower_limit());
  REQUIRE(rotation.value().upper_limit());
  CHECK(rotation.value().lower_limit()->degrees() == -45.0);
  CHECK(rotation.value().upper_limit()->degrees() == 60.0);

  auto translation = AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
      displacement(4.0, "translation"), displacement(-2.0, "translation"));
  REQUIRE(translation);
  CHECK(translation.value().value().millimeters() == 4.0);
  CHECK_FALSE(translation.value().upper_limit().has_value());

  CHECK(AssemblyJointCoordinateSlot::create(AssemblyJointCoordinateRole::Rotation,
                                            AssemblyJointCoordinateKind::Linear,
                                            displacement(1.0, "wrong-role-kind"))
            .has_error());
  CHECK(AssemblyJointCoordinateSlot::create(static_cast<AssemblyJointCoordinateRole>(99),
                                            AssemblyJointCoordinateKind::Angular,
                                            angle(1.0, "unknown-role"))
            .has_error());
  CHECK(AssemblyJointCoordinateSlot::create(AssemblyJointCoordinateRole::Translation,
                                            AssemblyJointCoordinateKind::Linear,
                                            angle(1.0, "wrong-quantity"))
            .has_error());
  CHECK(AssemblyJointCoordinateSlot::create(
            AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
            displacement(-1.0, "outside"), displacement(0.0, "outside"),
            displacement(2.0, "outside"))
            .has_error());

  auto joint = AssemblyJoint::create(AssemblyJointId("joint.slots"), "Slots",
                                     AssemblyJointType::Revolute, make_target("component.a"),
                                     make_target("component.b"), AssemblyJointState::Active,
                                     std::vector<AssemblyJointCoordinateSlot>{rotation.value()});
  REQUIRE(joint);
  REQUIRE(joint.value().coordinate_slots().size() == 1U);
  const auto* stored = joint.value().find_coordinate_slot(AssemblyJointCoordinateRole::Rotation);
  REQUIRE(stored != nullptr);
  CHECK(stored->kind() == AssemblyJointCoordinateKind::Angular);
  CHECK((joint.value().limits() == AssemblyJointLimits{-45.0, 60.0}));
  CHECK(joint.value().coordinate_deg() == 15.0);

  auto updated = joint.value().with_coordinate_value(AssemblyJointCoordinateRole::Rotation,
                                                     angle(-20.0, "joint.slots"));
  REQUIRE(updated);
  CHECK(updated.value().coordinate_slots().front().value().degrees() == -20.0);
  CHECK(joint.value().coordinate_slots().front().value().degrees() == 15.0);
  CHECK(joint.value()
            .with_coordinate_value(AssemblyJointCoordinateRole::Translation,
                                   displacement(1.0, "joint.slots"))
            .has_error());

  CHECK(AssemblyJoint::create(AssemblyJointId("joint.missing-slot"), "Missing",
                              AssemblyJointType::Revolute, make_target("component.a"),
                              make_target("component.b"), AssemblyJointState::Active,
                              std::vector<AssemblyJointCoordinateSlot>{})
            .has_error());
  CHECK(AssemblyJoint::create(AssemblyJointId("joint.wrong-slot"), "Wrong",
                              AssemblyJointType::Revolute, make_target("component.a"),
                              make_target("component.b"), AssemblyJointState::Active,
                              std::vector<AssemblyJointCoordinateSlot>{translation.value()})
            .has_error());
  CHECK(AssemblyJoint::create(
            AssemblyJointId("joint.duplicate-slot"), "Duplicate", AssemblyJointType::Revolute,
            make_target("component.a"), make_target("component.b"), AssemblyJointState::Active,
            std::vector<AssemblyJointCoordinateSlot>{rotation.value(), rotation.value()})
            .has_error());
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

  const auto updated = assembly.set_joint_coordinate(AssemblyJointId("joint.revolute"),
                                                     angle(30.0, "joint.revolute"));
  REQUIRE(updated);
  CHECK(assembly.find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() == 30.0);
  CHECK(assembly
            .set_joint_coordinate(AssemblyJointId("joint.revolute"), angle(120.0, "joint.revolute"))
            .has_error());

  auto missing_target = AssemblyJoint::create(
      AssemblyJointId("joint.missing"), "Missing", AssemblyJointType::Revolute,
      make_target("component.missing"), make_target("component.b"), AssemblyJointState::Active,
      angle(-90.0, "joint.missing"), angle(90.0, "joint.missing"), angle(0.0, "joint.missing"));
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
  CHECK(restored_project.value()
            .assembly()
            .find_joint(AssemblyJointId("joint.revolute"))
            ->coordinate_deg() == 25.0);
}

TEST_CASE("Local joint coordinate JSON is additive deterministic and backward compatible",
          "[core][assembly-joint-coordinate-json]") {
  auto assembly = make_assembly();
  REQUIRE(
      assembly.add_joint(make_joint("joint.coordinate.json", AssemblyJointState::Active, 25.0)));
  auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  nlohmann::json authored = nlohmann::json::parse(serialized.value());
  auto& joint_json = authored["assembly_joints"][0];

  REQUIRE(joint_json["coordinates"].size() == 1U);
  const auto& slot = joint_json["coordinates"][0];
  CHECK(slot["role"] == "rotation");
  CHECK(slot["kind"] == "angular");
  CHECK(slot["value"]["unit"] == "deg");
  CHECK(slot["value"]["value"] == 25.0);
  CHECK(slot["lower_limit"]["value"] == -90.0);
  CHECK(slot["upper_limit"]["value"] == 90.0);
  CHECK(joint_json.contains("limits"));
  CHECK(joint_json.contains("coordinate"));

  auto generalized_only = authored;
  generalized_only["assembly_joints"][0].erase("limits");
  generalized_only["assembly_joints"][0].erase("coordinate");
  auto generalized_loaded = deserialize_assembly_document_from_json(generalized_only.dump());
  REQUIRE(generalized_loaded);
  CHECK(generalized_loaded.value()
            .find_joint(AssemblyJointId("joint.coordinate.json"))
            ->coordinate_deg() == 25.0);
  auto canonical_again = serialize_assembly_document_to_json(generalized_loaded.value());
  REQUIRE(canonical_again);
  CHECK(canonical_again.value() == serialized.value());

  auto legacy_only = authored;
  legacy_only["assembly_joints"][0].erase("coordinates");
  auto legacy_loaded = deserialize_assembly_document_from_json(legacy_only.dump());
  REQUIRE(legacy_loaded);
  CHECK(legacy_loaded.value()
            .find_joint(AssemblyJointId("joint.coordinate.json"))
            ->coordinate_deg() == 25.0);

  auto duplicate = generalized_only;
  duplicate["assembly_joints"][0]["coordinates"].push_back(
      duplicate["assembly_joints"][0]["coordinates"][0]);
  CHECK(deserialize_assembly_document_from_json(duplicate.dump()).has_error());

  auto unknown_role = generalized_only;
  unknown_role["assembly_joints"][0]["coordinates"][0]["role"] = "spin";
  CHECK(deserialize_assembly_document_from_json(unknown_role.dump()).has_error());

  auto wrong_unit = generalized_only;
  wrong_unit["assembly_joints"][0]["coordinates"][0]["value"]["unit"] = "mm";
  CHECK(deserialize_assembly_document_from_json(wrong_unit.dump()).has_error());

  auto missing_role = generalized_only;
  missing_role["assembly_joints"][0]["coordinates"] = nlohmann::json::array();
  CHECK(deserialize_assembly_document_from_json(missing_role.dump()).has_error());

  auto conflicting = authored;
  conflicting["assembly_joints"][0]["coordinate"]["value"] = 20.0;
  CHECK(deserialize_assembly_document_from_json(conflicting.dump()).has_error());

  auto partial_legacy = authored;
  partial_legacy["assembly_joints"][0].erase("limits");
  CHECK(deserialize_assembly_document_from_json(partial_legacy.dump()).has_error());
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
