#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>

using namespace blcad;

namespace {

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

ComponentInstance make_instance(const char* id, double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 2.0, 3.0};
  transform.rotation_deg = Vector3{4.0, 5.0, 6.0};
  auto instance = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.shared"),
                                            ComponentVisibility::Visible,
                                            ComponentSuppressionState::Active,
                                            ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

AssemblyDocument make_constraint_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.constraints"), "ConstraintAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.a", 10.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.b", 20.0)));
  return assembly.value();
}

AssemblyConstraint make_constraint(const char* id,
                                   AssemblyConstraintType type,
                                   AssemblyConstraintState state = AssemblyConstraintState::Active,
                                   std::optional<Quantity> distance = std::nullopt) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type, make_target("component.a", "feature.base.top"),
      make_target("component.b", "feature.base.bottom"), state, std::move(distance));
  REQUIRE(constraint);
  return constraint.value();
}

PartDocument make_empty_part() {
  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);
  return part.value();
}

} // namespace

TEST_CASE("AssemblyConstraintTarget validates semantic component targets",
          "[core][assembly-constraint]") {
  CHECK(AssemblyConstraintTarget::create(ComponentInstanceId(""), "feature.base.top").has_error());
  CHECK(AssemblyConstraintTarget::create(ComponentInstanceId("component.a"), "").has_error());

  const auto target = make_target("component.a", "feature.base.top");
  CHECK(target.component_instance().value() == "component.a");
  CHECK(target.semantic_reference() == "feature.base.top");
}

TEST_CASE("AssemblyConstraint validates identity and type-specific distance intent",
          "[core][assembly-constraint]") {
  const auto target_a = make_target("component.a", "feature.base.top");
  const auto target_b = make_target("component.b", "feature.base.bottom");
  auto distance = Quantity::length_mm(12.5, "constraint.distance");
  REQUIRE(distance);
  auto count = Quantity::count(4.0, "constraint.count");
  REQUIRE(count);

  CHECK(AssemblyConstraint::create(AssemblyConstraintId(""), "Mate", AssemblyConstraintType::Mate,
                                   target_a, target_b)
            .has_error());
  CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.mate"), "",
                                   AssemblyConstraintType::Mate, target_a, target_b)
            .has_error());
  CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.mate"), "Mate",
                                   AssemblyConstraintType::Mate, target_a, target_b,
                                   AssemblyConstraintState::Active, distance.value())
            .has_error());
  CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.concentric"), "Concentric",
                                   AssemblyConstraintType::Concentric, target_a, target_b,
                                   AssemblyConstraintState::Active, distance.value())
            .has_error());
  CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.distance"), "Distance",
                                   AssemblyConstraintType::Distance, target_a, target_b)
            .has_error());
  CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.distance"), "Distance",
                                   AssemblyConstraintType::Distance, target_a, target_b,
                                   AssemblyConstraintState::Active, count.value())
            .has_error());

  const auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.distance"), "Distance", AssemblyConstraintType::Distance,
      target_a, target_b, AssemblyConstraintState::Inactive, distance.value());
  REQUIRE(constraint);
  CHECK(constraint.value().type() == AssemblyConstraintType::Distance);
  CHECK(constraint.value().state() == AssemblyConstraintState::Inactive);
  REQUIRE(constraint.value().distance().has_value());
  CHECK(constraint.value().distance()->millimeters() == 12.5);
}

TEST_CASE("AssemblyDocument validates assembly constraint ids and component targets",
          "[core][assembly-constraint]") {
  auto assembly = make_constraint_assembly();
  REQUIRE(assembly.add_constraint(make_constraint("constraint.mate", AssemblyConstraintType::Mate)));
  CHECK(assembly.constraint_count() == 1U);
  CHECK(assembly.add_constraint(make_constraint("constraint.mate", AssemblyConstraintType::Mate))
            .has_error());

  auto unknown_constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.unknown"), "Unknown", AssemblyConstraintType::Mate,
      make_target("component.missing", "feature.base.top"),
      make_target("component.b", "feature.base.bottom"));
  REQUIRE(unknown_constraint);
  CHECK(assembly.add_constraint(std::move(unknown_constraint.value())).has_error());

  const AssemblyConstraint* stored =
      assembly.find_constraint(AssemblyConstraintId("constraint.mate"));
  REQUIRE(stored != nullptr);
  CHECK(stored->target_a().component_instance().value() == "component.a");
  CHECK(stored->target_b().component_instance().value() == "component.b");
}

TEST_CASE("Adding assembly constraints does not mutate free-placement transforms",
          "[core][assembly-constraint]") {
  auto assembly = make_constraint_assembly();
  const auto transform_a =
      assembly.find_component_instance(ComponentInstanceId("component.a"))->transform();
  const auto transform_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();

  REQUIRE(assembly.add_constraint(make_constraint("constraint.mate", AssemblyConstraintType::Mate)));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.concentric", AssemblyConstraintType::Concentric)));
  auto distance = Quantity::length_mm(40.0, "constraint.distance");
  REQUIRE(distance);
  REQUIRE(assembly.add_constraint(make_constraint("constraint.distance", AssemblyConstraintType::Distance,
                                                  AssemblyConstraintState::Active,
                                                  distance.value())));

  CHECK(assembly.find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        transform_a);
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        transform_b);
}

TEST_CASE("Mate Concentric and Distance assembly constraints survive JSON roundtrip",
          "[core][assembly-constraint]") {
  auto assembly = make_constraint_assembly();
  REQUIRE(assembly.add_constraint(make_constraint("constraint.mate", AssemblyConstraintType::Mate)));
  REQUIRE(assembly.add_constraint(make_constraint("constraint.concentric",
                                                  AssemblyConstraintType::Concentric,
                                                  AssemblyConstraintState::Inactive)));
  auto distance = Quantity::length_mm(40.0, "constraint.distance");
  REQUIRE(distance);
  REQUIRE(assembly.add_constraint(make_constraint("constraint.distance", AssemblyConstraintType::Distance,
                                                  AssemblyConstraintState::Active,
                                                  distance.value())));

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("assembly_constraints") != std::string::npos);
  CHECK(serialized.value().find("semantic_reference") != std::string::npos);
  CHECK(serialized.value().find("concentric") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().constraint_count() == 3U);

  const AssemblyConstraint* mate =
      restored.value().find_constraint(AssemblyConstraintId("constraint.mate"));
  REQUIRE(mate != nullptr);
  CHECK(mate->type() == AssemblyConstraintType::Mate);
  CHECK_FALSE(mate->distance().has_value());

  const AssemblyConstraint* concentric =
      restored.value().find_constraint(AssemblyConstraintId("constraint.concentric"));
  REQUIRE(concentric != nullptr);
  CHECK(concentric->type() == AssemblyConstraintType::Concentric);
  CHECK(concentric->state() == AssemblyConstraintState::Inactive);

  const AssemblyConstraint* restored_distance =
      restored.value().find_constraint(AssemblyConstraintId("constraint.distance"));
  REQUIRE(restored_distance != nullptr);
  CHECK(restored_distance->type() == AssemblyConstraintType::Distance);
  REQUIRE(restored_distance->distance().has_value());
  CHECK(restored_distance->distance()->millimeters() == 40.0);
  CHECK(restored_distance->target_a().semantic_reference() == "feature.base.top");
  CHECK(restored_distance->target_b().semantic_reference() == "feature.base.bottom");
}

TEST_CASE("Assembly JSON remains backward compatible without assembly constraints",
          "[core][assembly-constraint]") {
  const std::string content = R"({
    "schema": "blcad.assembly_document.mvp4",
    "version": 1,
    "document": {"id": "assembly.old", "name": "OldAssembly"},
    "parameters": [],
    "member_parts": [],
    "parameter_bindings": [],
    "component_instances": []
  })";

  const auto restored = deserialize_assembly_document_from_json(content);
  REQUIRE(restored);
  CHECK(restored.value().constraint_count() == 0U);
}

TEST_CASE("Assembly JSON rejects constraints with missing component targets",
          "[core][assembly-constraint]") {
  const std::string content = R"({
    "schema": "blcad.assembly_document.mvp4",
    "version": 1,
    "document": {"id": "assembly.invalid", "name": "InvalidAssembly"},
    "parameters": [],
    "member_parts": [],
    "parameter_bindings": [],
    "component_instances": [],
    "assembly_constraints": [
      {
        "id": "constraint.invalid",
        "name": "Invalid",
        "type": "mate",
        "target_a": {"component_instance": "component.missing.a", "semantic_reference": "face.top"},
        "target_b": {"component_instance": "component.missing.b", "semantic_reference": "face.bottom"},
        "state": "active"
      }
    ]
  })";

  const auto restored = deserialize_assembly_document_from_json(content);
  REQUIRE(restored.has_error());
  CHECK(restored.error().message() == "assembly constraint target A component instance must exist");
}

TEST_CASE("Project constraint roundtrip preserves shared part ownership and valid structure",
          "[core][assembly-constraint]") {
  auto assembly = make_constraint_assembly();
  REQUIRE(assembly.add_constraint(make_constraint("constraint.mate", AssemblyConstraintType::Mate)));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.concentric", AssemblyConstraintType::Concentric)));
  auto distance = Quantity::length_mm(40.0, "constraint.distance");
  REQUIRE(distance);
  REQUIRE(assembly.add_constraint(make_constraint("constraint.distance", AssemblyConstraintType::Distance,
                                                  AssemblyConstraintState::Active,
                                                  distance.value())));

  auto project = Project::create(DocumentId("project.constraints"), "ConstraintProject",
                                 std::move(assembly));
  REQUIRE(project);
  CHECK(project.value().validate_assembly_structure().has_error());
  REQUIRE(project.value().add_part_document(make_empty_part()));
  REQUIRE(project.value().validate_assembly_constraints());
  REQUIRE(project.value().validate_assembly_structure());
  CHECK(project.value().part_document_count() == 1U);
  CHECK(project.value().assembly().component_instance_count() == 2U);
  CHECK(project.value().assembly().constraint_count() == 3U);

  const auto serialized = serialize_project_to_json(project.value());
  REQUIRE(serialized);
  const auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().validate_assembly_structure());
  CHECK(restored.value().part_document_count() == 1U);
  CHECK(restored.value().assembly().component_instance_count() == 2U);
  CHECK(restored.value().assembly().constraint_count() == 3U);
  CHECK(restored.value().assembly()
            .find_component_instance(ComponentInstanceId("component.a"))
            ->referenced_part_document()
            .value() == "part.shared");
  CHECK(restored.value().assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->referenced_part_document()
            .value() == "part.shared");
}
