#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

AssemblyHierarchyConstraintEndpoint hierarchy_endpoint(
    std::initializer_list<const char*> path,
    const char* component,
    const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  occurrence_path.reserve(path.size());
  for (const char* occurrence : path) {
    occurrence_path.emplace_back(occurrence);
  }
  auto endpoint = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference);
  REQUIRE(endpoint);
  return endpoint.value();
}

AssemblyHierarchyConstraint hierarchy_constraint(
    const char* id,
    AssemblyConstraintType type,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state = AssemblyConstraintState::Active,
    std::optional<Quantity> distance = std::nullopt,
    std::optional<Quantity> angle = std::nullopt) {
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, type, std::move(target_a), std::move(target_b), state,
      std::move(distance), std::move(angle));
  REQUIRE(constraint);
  return constraint.value();
}

ComponentInstance hierarchy_instance(const char* id, double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 2.0, 3.0};
  transform.rotation_deg = Vector3{4.0, 5.0, 6.0};
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance hierarchy_subassembly(const char* id,
                                          const char* referenced_assembly,
                                          double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 7.0, 8.0};
  transform.rotation_deg = Vector3{9.0, 10.0, 11.0};
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(referenced_assembly),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, transform);
  REQUIRE(instance);
  return instance.value();
}

Project hierarchy_intent_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(hierarchy_instance("component.root", 10.0)));
  REQUIRE(root.value().add_subassembly_instance(
      hierarchy_subassembly("subassembly.child", "assembly.child", 100.0)));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(hierarchy_instance("component.child", 20.0)));

  auto project = Project::create(DocumentId("project.hierarchy.intent"), "HierarchyIntent",
                                 std::move(root.value()));
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(std::move(child.value())));
  return project.value();
}

Project hierarchy_json_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(hierarchy_instance("component.root", 10.0)));
  REQUIRE(root.value().add_subassembly_instance(
      hierarchy_subassembly("subassembly.outer", "assembly.child", 100.0)));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(hierarchy_instance("component.child", 20.0)));
  REQUIRE(child.value().add_subassembly_instance(
      hierarchy_subassembly("subassembly.inner", "assembly.grandchild", 200.0)));

  auto grandchild = AssemblyDocument::create(DocumentId("assembly.grandchild"), "Grandchild");
  REQUIRE(grandchild);
  REQUIRE(grandchild.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(grandchild.value().add_component_instance(hierarchy_instance("component.leaf", 30.0)));

  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);

  auto project = Project::create(DocumentId("project.hierarchy.json"), "HierarchyJson",
                                 std::move(root.value()));
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(std::move(child.value())));
  REQUIRE(project.value().add_child_assembly_document(std::move(grandchild.value())));
  REQUIRE(project.value().add_part_document(std::move(part.value())));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Quantity hierarchy_distance(double value_mm, const char* object_id) {
  auto quantity = Quantity::length_mm(value_mm, object_id);
  REQUIRE(quantity);
  return quantity.value();
}

Quantity hierarchy_angle(double value_deg, const char* object_id) {
  auto quantity = Quantity::angle_deg(value_deg, object_id);
  REQUIRE(quantity);
  return quantity.value();
}

void add_json_constraints(Project& project) {
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.mate", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "semantic.no_geometry.mate.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child",
                         "semantic.no_geometry.mate.b"))));

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.concentric", AssemblyConstraintType::Concentric,
      hierarchy_endpoint({"subassembly.outer"}, "component.child",
                         "semantic.no_geometry.concentric.a"),
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "semantic.no_geometry.concentric.b"))));

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.distance", AssemblyConstraintType::Distance,
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "semantic.no_geometry.distance.a"),
      hierarchy_endpoint({}, "component.root", "semantic.no_geometry.distance.b"),
      AssemblyConstraintState::Active,
      hierarchy_distance(12.5, "constraint.cross.distance"))));

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.insert", AssemblyConstraintType::Insert,
      hierarchy_endpoint({"subassembly.outer"}, "component.child",
                         "semantic.no_geometry.insert.a"),
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "semantic.no_geometry.insert.b"))));

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.angle", AssemblyConstraintType::Angle,
      hierarchy_endpoint({}, "component.root", "semantic.no_geometry.angle.a"),
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "semantic.no_geometry.angle.b"),
      AssemblyConstraintState::Inactive, std::nullopt,
      hierarchy_angle(35.0, "constraint.cross.angle"))));
}

} // namespace

TEST_CASE("Cross-hierarchy endpoint intent preserves exact rooted identity",
          "[core][assembly-cross-hierarchy-intent]") {
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {SubassemblyInstanceId("")}, ComponentInstanceId("component.child"),
            "feature.hole.axis")
            .has_error());
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {}, ComponentInstanceId(""), "feature.hole.axis")
            .has_error());
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {}, ComponentInstanceId("component.root"), "")
            .has_error());

  const auto root = hierarchy_endpoint({}, "component.root", "feature.base.top");
  CHECK(root.occurrence_path().empty());
  CHECK(root.component_instance().value() == "component.root");
  CHECK(root.semantic_reference() == "feature.base.top");

  const auto nested = hierarchy_endpoint(
      {"subassembly.outer", "subassembly.inner"}, "component.shaft", "feature.bore.axis");
  REQUIRE(nested.occurrence_path().size() == 2U);
  CHECK(nested.occurrence_path()[0].value() == "subassembly.outer");
  CHECK(nested.occurrence_path()[1].value() == "subassembly.inner");
  CHECK(nested.component_instance().value() == "component.shaft");
  CHECK(nested.semantic_reference() == "feature.bore.axis");
}

TEST_CASE("Cross-hierarchy constraint intent reuses all local value-family rules",
          "[core][assembly-cross-hierarchy-intent]") {
  const auto target_a = hierarchy_endpoint({}, "component.root", "feature.base.top");
  const auto target_b = hierarchy_endpoint(
      {"subassembly.child"}, "component.child", "feature.base.bottom");
  const Quantity distance = hierarchy_distance(12.5, "constraint.cross.distance");
  const Quantity angle = hierarchy_angle(35.0, "constraint.cross.angle");
  auto count = Quantity::count(4.0, "constraint.cross.count");
  REQUIRE(count);

  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId(""), "Mate", AssemblyConstraintType::Mate, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.mate"), "", AssemblyConstraintType::Mate,
            target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.distance"), "Distance",
            AssemblyConstraintType::Distance, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.distance"), "Distance",
            AssemblyConstraintType::Distance, target_a, target_b,
            AssemblyConstraintState::Active, count.value())
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.angle"), "Angle",
            AssemblyConstraintType::Angle, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.mate"), "Mate", AssemblyConstraintType::Mate,
            target_a, target_b, AssemblyConstraintState::Active, distance)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.insert"), "Insert",
            AssemblyConstraintType::Insert, target_a, target_b, AssemblyConstraintState::Active,
            std::nullopt, angle)
            .has_error());

  const auto mate = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.mate"), "Mate", AssemblyConstraintType::Mate,
      target_a, target_b);
  const auto concentric = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.concentric"), "Concentric",
      AssemblyConstraintType::Concentric, target_a, target_b, AssemblyConstraintState::Inactive);
  const auto distance_constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.distance"), "Distance",
      AssemblyConstraintType::Distance, target_a, target_b, AssemblyConstraintState::Active,
      distance);
  const auto insert = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.insert"), "Insert", AssemblyConstraintType::Insert,
      target_a, target_b);
  const auto angle_constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.angle"), "Angle", AssemblyConstraintType::Angle,
      target_a, target_b, AssemblyConstraintState::Active, std::nullopt, angle);

  REQUIRE(mate);
  REQUIRE(concentric);
  REQUIRE(distance_constraint);
  REQUIRE(insert);
  REQUIRE(angle_constraint);
  CHECK(concentric.value().state() == AssemblyConstraintState::Inactive);
  REQUIRE(distance_constraint.value().distance().has_value());
  CHECK(distance_constraint.value().distance()->millimeters() == 12.5);
  REQUIRE(angle_constraint.value().angle().has_value());
  CHECK(angle_constraint.value().angle()->degrees() == 35.0);
  CHECK(angle_constraint.value().target_a() == target_a);
  CHECK(angle_constraint.value().target_b() == target_b);
}

TEST_CASE("Project owns cross-hierarchy constraints in a separate project-wide id scope",
          "[core][assembly-cross-hierarchy-intent]") {
  auto project = hierarchy_intent_project();

  auto local_target_a = AssemblyConstraintTarget::create(
      ComponentInstanceId("component.root"), "feature.base.top");
  auto local_target_b = AssemblyConstraintTarget::create(
      ComponentInstanceId("component.root"), "feature.base.bottom");
  REQUIRE(local_target_a);
  REQUIRE(local_target_b);
  auto local = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.shared"), "Local", AssemblyConstraintType::Mate,
      std::move(local_target_a.value()), std::move(local_target_b.value()));
  REQUIRE(local);
  REQUIRE(project.assembly().add_constraint(std::move(local.value())));

  const auto cross = hierarchy_constraint(
      "constraint.shared", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.base.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.base.bottom"));
  REQUIRE(project.add_cross_hierarchy_constraint(cross));

  CHECK(project.cross_hierarchy_constraint_count() == 1U);
  CHECK(project.cross_hierarchy_constraints().size() == 1U);
  const AssemblyHierarchyConstraint* stored =
      project.find_cross_hierarchy_constraint(AssemblyConstraintId("constraint.shared"));
  REQUIRE(stored != nullptr);
  CHECK(stored->id().value() == "constraint.shared");
  CHECK(stored->target_b().occurrence_path().size() == 1U);
  CHECK(project.add_cross_hierarchy_constraint(cross).has_error());
  CHECK(project.cross_hierarchy_constraint_count() == 1U);
}

TEST_CASE("Project-level cross-hierarchy intent does not resolve paths components or geometry",
          "[core][assembly-cross-hierarchy-intent]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  auto project = Project::create(DocumentId("project.intent.only"), "IntentOnly",
                                 std::move(root.value()));
  REQUIRE(project);

  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.root_child", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.root.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.child.bottom"))));
  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.left_right", AssemblyConstraintType::Concentric,
      hierarchy_endpoint({"subassembly.left"}, "component.shaft", "feature.bore.axis"),
      hierarchy_endpoint({"subassembly.right"}, "component.shaft", "feature.bore.axis"))));
  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.nested_root", AssemblyConstraintType::Insert,
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.pin",
                         "feature.pin.seat"),
      hierarchy_endpoint({}, "component.root", "feature.root.seat"))));

  CHECK(project.value().cross_hierarchy_constraint_count() == 3U);
  CHECK(project.value().child_assembly_document_count() == 0U);
  CHECK(project.value().assembly().component_instance_count() == 0U);
}

TEST_CASE("Adding cross-hierarchy constraint intent never mutates authored transforms",
          "[core][assembly-cross-hierarchy-intent]") {
  auto project = hierarchy_intent_project();
  const RigidTransform root_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform();
  const RigidTransform child_transform = project
                                             .find_assembly_document(DocumentId("assembly.child"))
                                             ->find_component_instance(
                                                 ComponentInstanceId("component.child"))
                                             ->transform();
  const RigidTransform boundary_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
          ->transform();

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.mate", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.base.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.base.bottom"))));

  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.root"))
            ->transform() == root_transform);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == child_transform);
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
            ->transform() == boundary_transform);
}

TEST_CASE("Cross-hierarchy project JSON roundtrip preserves exact intent and authored transforms",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  const RigidTransform root_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform();
  const RigidTransform child_transform = project
                                             .find_assembly_document(DocumentId("assembly.child"))
                                             ->find_component_instance(
                                                 ComponentInstanceId("component.child"))
                                             ->transform();
  const RigidTransform leaf_transform = project
                                            .find_assembly_document(DocumentId("assembly.grandchild"))
                                            ->find_component_instance(ComponentInstanceId("component.leaf"))
                                            ->transform();
  const RigidTransform outer_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.outer"))
          ->transform();
  const RigidTransform inner_transform = project
                                             .find_assembly_document(DocumentId("assembly.child"))
                                             ->find_subassembly_instance(
                                                 SubassemblyInstanceId("subassembly.inner"))
                                             ->transform();

  add_json_constraints(project);
  REQUIRE(project.validate_assembly_structure());

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);
  const auto serialized_json = nlohmann::json::parse(serialized.value());
  REQUIRE(serialized_json.at("cross_hierarchy_constraints").size() == 5U);

  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().validate_assembly_structure());
  CHECK(restored.value().cross_hierarchy_constraint_count() == 5U);

  const AssemblyHierarchyConstraint* distance = restored.value().find_cross_hierarchy_constraint(
      AssemblyConstraintId("constraint.cross.distance"));
  REQUIRE(distance != nullptr);
  CHECK(distance->name() == "constraint.cross.distance");
  CHECK(distance->type() == AssemblyConstraintType::Distance);
  CHECK(distance->state() == AssemblyConstraintState::Active);
  REQUIRE(distance->distance().has_value());
  CHECK(distance->distance()->millimeters() == 12.5);
  REQUIRE(distance->target_a().occurrence_path().size() == 2U);
  CHECK(distance->target_a().occurrence_path()[0].value() == "subassembly.outer");
  CHECK(distance->target_a().occurrence_path()[1].value() == "subassembly.inner");
  CHECK(distance->target_a().component_instance().value() == "component.leaf");
  CHECK(distance->target_a().semantic_reference() == "semantic.no_geometry.distance.a");
  CHECK(distance->target_b().occurrence_path().empty());
  CHECK(distance->target_b().component_instance().value() == "component.root");
  CHECK(distance->target_b().semantic_reference() == "semantic.no_geometry.distance.b");

  const AssemblyHierarchyConstraint* angle = restored.value().find_cross_hierarchy_constraint(
      AssemblyConstraintId("constraint.cross.angle"));
  REQUIRE(angle != nullptr);
  CHECK(angle->type() == AssemblyConstraintType::Angle);
  CHECK(angle->state() == AssemblyConstraintState::Inactive);
  REQUIRE(angle->angle().has_value());
  CHECK(angle->angle()->degrees() == 35.0);

  CHECK(restored.value().assembly()
            .find_component_instance(ComponentInstanceId("component.root"))
            ->transform() == root_transform);
  CHECK(restored.value()
            .find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == child_transform);
  CHECK(restored.value()
            .find_assembly_document(DocumentId("assembly.grandchild"))
            ->find_component_instance(ComponentInstanceId("component.leaf"))
            ->transform() == leaf_transform);
  CHECK(restored.value().assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.outer"))
            ->transform() == outer_transform);
  CHECK(restored.value()
            .find_assembly_document(DocumentId("assembly.child"))
            ->find_subassembly_instance(SubassemblyInstanceId("subassembly.inner"))
            ->transform() == inner_transform);
}

TEST_CASE("Project JSON remains backward compatible without cross-hierarchy constraints",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  auto json = nlohmann::json::parse(serialized.value());
  json.erase("cross_hierarchy_constraints");

  auto restored = deserialize_project_from_json(json.dump());
  REQUIRE(restored);
  CHECK(restored.value().cross_hierarchy_constraint_count() == 0U);
  REQUIRE(restored.value().validate_assembly_structure());
}

TEST_CASE("Project JSON rejects duplicate cross-hierarchy relationship ids",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.duplicate", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "semantic.duplicate.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child", "semantic.duplicate.b"))));

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);
  auto json = nlohmann::json::parse(serialized.value());
  json["cross_hierarchy_constraints"].push_back(json["cross_hierarchy_constraints"][0]);

  auto restored = deserialize_project_from_json(json.dump());
  REQUIRE(restored.has_error());
  CHECK(restored.error().message() ==
        "cross-hierarchy assembly constraint id must be unique within project");
}

TEST_CASE("Cross-hierarchy structure validation rejects missing paths and reached components",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.structure", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "semantic.structure.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child", "semantic.structure.b"))));
  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  SECTION("missing exact occurrence path") {
    auto json = nlohmann::json::parse(serialized.value());
    json["cross_hierarchy_constraints"][0]["target_b"]["occurrence_path"][0] =
        "subassembly.missing";
    auto restored = deserialize_project_from_json(json.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "cross-hierarchy assembly constraint target B occurrence path must exist in rooted hierarchy");
  }

  SECTION("missing component in reached assembly") {
    auto json = nlohmann::json::parse(serialized.value());
    json["cross_hierarchy_constraints"][0]["target_b"]["component_instance"] =
        "component.missing";
    auto restored = deserialize_project_from_json(json.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "cross-hierarchy assembly constraint target B component instance must exist in reached assembly document");
  }
}

TEST_CASE("Project structure validates hierarchy before cross-hierarchy endpoint paths",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.validation-order", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "semantic.validation.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child", "semantic.validation.b"))));
  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  auto json = nlohmann::json::parse(serialized.value());
  json["assembly"]["subassembly_instances"][0]["referenced_assembly_document"] =
      "assembly.missing";
  json["cross_hierarchy_constraints"][0]["target_b"]["occurrence_path"][0] =
      "subassembly.also-missing";

  auto restored = deserialize_project_from_json(json.dump());
  REQUIRE(restored.has_error());
  CHECK(restored.error().message() ==
        "subassembly instance referenced assembly must resolve to a project-owned child assembly document");
}

TEST_CASE("Cross-hierarchy project JSON enforces millimeter and degree quantity spellings",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.distance", AssemblyConstraintType::Distance,
      hierarchy_endpoint({}, "component.root", "semantic.distance.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child", "semantic.distance.b"),
      AssemblyConstraintState::Active,
      hierarchy_distance(5.0, "constraint.cross.distance"))));
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.angle", AssemblyConstraintType::Angle,
      hierarchy_endpoint({}, "component.root", "semantic.angle.a"),
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "semantic.angle.b"),
      AssemblyConstraintState::Active, std::nullopt,
      hierarchy_angle(15.0, "constraint.cross.angle"))));
  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  SECTION("distance requires mm") {
    auto json = nlohmann::json::parse(serialized.value());
    json["cross_hierarchy_constraints"][0]["distance"]["unit"] = "cm";
    auto restored = deserialize_project_from_json(json.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "cross-hierarchy assembly constraint distance must use millimeters");
  }

  SECTION("angle requires deg") {
    auto json = nlohmann::json::parse(serialized.value());
    json["cross_hierarchy_constraints"][1]["angle"]["unit"] = "rad";
    auto restored = deserialize_project_from_json(json.dump());
    REQUIRE(restored.has_error());
    CHECK(restored.error().message() ==
          "cross-hierarchy assembly constraint angle must use degrees");
  }
}

TEST_CASE("Cross-hierarchy structure validation rejects duplicate ids introduced through mutation",
          "[core][assembly-cross-hierarchy-json]") {
  auto project = hierarchy_json_project();
  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.mutable", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "semantic.mutable.a"),
      hierarchy_endpoint({"subassembly.outer"}, "component.child", "semantic.mutable.b"))));
  project.cross_hierarchy_constraints().push_back(project.cross_hierarchy_constraints().front());

  auto validated = project.validate_assembly_structure();
  REQUIRE(validated.has_error());
  CHECK(validated.error().message() ==
        "cross-hierarchy assembly constraint id must be unique within project");
}
