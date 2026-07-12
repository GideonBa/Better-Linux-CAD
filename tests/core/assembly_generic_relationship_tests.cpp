#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

using json = nlohmann::json;

constexpr std::array kGenericRelationshipTypes{
    AssemblyConstraintType::Coincident,
    AssemblyConstraintType::Parallel,
    AssemblyConstraintType::Perpendicular,
};

AssemblyConstraintTarget local_target(const char* component, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyHierarchyConstraintEndpoint hierarchy_target(std::initializer_list<const char*> path,
                                                     const char* component,
                                                     const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  occurrence_path.reserve(path.size());
  for (const char* occurrence : path) {
    occurrence_path.emplace_back(occurrence);
  }
  auto target = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

ComponentInstance component(const char* id, double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 2.0, 3.0};
  transform.rotation_deg = Vector3{4.0, 5.0, 6.0};
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance subassembly(const char* id, const char* referenced_assembly) {
  RigidTransform transform;
  transform.translation_mm = Vector3{100.0, 20.0, 30.0};
  transform.rotation_deg = Vector3{10.0, 20.0, 30.0};
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(referenced_assembly), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, transform);
  REQUIRE(instance);
  return instance.value();
}

AssemblyDocument local_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.generic"), "GenericAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(component("component.a", 10.0)));
  REQUIRE(assembly.value().add_component_instance(component("component.b", 20.0)));
  return assembly.value();
}

AssemblyConstraint
local_relationship(const char* id, AssemblyConstraintType type,
                   AssemblyConstraintState state = AssemblyConstraintState::Active,
                   std::optional<Quantity> distance = std::nullopt,
                   std::optional<Quantity> angle = std::nullopt) {
  auto relationship = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type, local_target("component.a", "semantic.generic.target.a"),
      local_target("component.b", "semantic.generic.target.b"), state, std::move(distance),
      std::move(angle));
  REQUIRE(relationship);
  return relationship.value();
}

AssemblyHierarchyConstraint cross_relationship(
    const char* id, AssemblyConstraintType type, AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state = AssemblyConstraintState::Active,
    std::optional<Quantity> distance = std::nullopt, std::optional<Quantity> angle = std::nullopt) {
  auto relationship = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, type, std::move(target_a), std::move(target_b), state,
      std::move(distance), std::move(angle));
  REQUIRE(relationship);
  return relationship.value();
}

Project generic_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(component("component.root", 10.0)));
  REQUIRE(
      root.value().add_subassembly_instance(subassembly("subassembly.child", "assembly.child")));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(component("component.child", 20.0)));

  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);

  auto project =
      Project::create(DocumentId("project.generic"), "GenericProject", std::move(root.value()));
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(std::move(child.value())));
  REQUIRE(project.value().add_part_document(std::move(part.value())));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Quantity distance_quantity(double value, const char* id) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  return quantity.value();
}

Quantity angle_quantity(double value, const char* id) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  return quantity.value();
}

std::vector<std::string> relationship_type_spellings(const json& constraints) {
  std::vector<std::string> types;
  for (const auto& constraint : constraints) {
    types.push_back(constraint.at("type").get<std::string>());
  }
  return types;
}

} // namespace

TEST_CASE("Generic assembly relationship intent is persistent-only before Block 39",
          "[core][assembly-generic-relationship-intent]") {
  CHECK(to_string(AssemblyConstraintType::Coincident) == "coincident");
  CHECK(to_string(AssemblyConstraintType::Parallel) == "parallel");
  CHECK(to_string(AssemblyConstraintType::Perpendicular) == "perpendicular");

  const Quantity distance = distance_quantity(12.5, "generic.distance");
  const Quantity angle = angle_quantity(35.0, "generic.angle");
  const auto target_a = local_target("component.a", "semantic.generic.a");
  const auto target_b = local_target("component.b", "semantic.generic.b");

  for (const AssemblyConstraintType type : kGenericRelationshipTypes) {
    auto relationship =
        AssemblyConstraint::create(AssemblyConstraintId("constraint.generic"), "Generic", type,
                                   target_a, target_b, AssemblyConstraintState::Inactive);
    REQUIRE(relationship);
    CHECK(relationship.value().type() == type);
    CHECK(relationship.value().state() == AssemblyConstraintState::Inactive);
    CHECK(relationship.value().target_a() == target_a);
    CHECK(relationship.value().target_b() == target_b);
    CHECK_FALSE(relationship.value().distance().has_value());
    CHECK_FALSE(relationship.value().angle().has_value());

    CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.generic.distance"),
                                     "Generic distance", type, target_a, target_b,
                                     AssemblyConstraintState::Active, distance)
              .has_error());
    CHECK(AssemblyConstraint::create(AssemblyConstraintId("constraint.generic.angle"),
                                     "Generic angle", type, target_a, target_b,
                                     AssemblyConstraintState::Active, std::nullopt, angle)
              .has_error());
  }

  auto assembly = local_assembly();
  const RigidTransform transform_a =
      assembly.find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform transform_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();

  REQUIRE(assembly.add_constraint(
      local_relationship("constraint.coincident", AssemblyConstraintType::Coincident)));
  REQUIRE(assembly.add_constraint(local_relationship(
      "constraint.parallel", AssemblyConstraintType::Parallel, AssemblyConstraintState::Inactive)));
  REQUIRE(assembly.add_constraint(
      local_relationship("constraint.perpendicular", AssemblyConstraintType::Perpendicular)));
  REQUIRE(
      assembly.add_constraint(local_relationship("constraint.mate", AssemblyConstraintType::Mate)));

  CHECK(assembly.find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        transform_a);
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        transform_b);

  auto graph = AssemblyConstraintGraph::build(assembly);
  REQUIRE(graph);
  CHECK(graph.value().edge_count() == 1U);
  REQUIRE(graph.value().edges().size() == 1U);
  CHECK(graph.value().edges().front().constraint() == AssemblyConstraintId("constraint.mate"));
}

TEST_CASE("Generic Project relationships preserve rooted identity and stay out of current graphs",
          "[core][assembly-generic-relationship-intent]") {
  auto project = generic_project();

  const RigidTransform root_transform =
      project.assembly()
          .find_component_instance(ComponentInstanceId("component.root"))
          ->transform();
  const RigidTransform boundary_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
          ->transform();
  const AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  const RigidTransform child_transform =
      child->find_component_instance(ComponentInstanceId("component.child"))->transform();

  auto local_a = AssemblyConstraintTarget::create(ComponentInstanceId("component.root"),
                                                  "semantic.generic.local.a");
  auto local_b = AssemblyConstraintTarget::create(ComponentInstanceId("component.root"),
                                                  "semantic.generic.local.b");
  REQUIRE(local_a);
  REQUIRE(local_b);
  auto local = AssemblyConstraint::create(AssemblyConstraintId("constraint.shared"), "Local shared",
                                          AssemblyConstraintType::Coincident,
                                          std::move(local_a.value()), std::move(local_b.value()));
  REQUIRE(local);
  REQUIRE(project.assembly().add_constraint(std::move(local.value())));

  const auto root_target = hierarchy_target({}, "component.root", "semantic.generic.cross.root");
  const auto child_target =
      hierarchy_target({"subassembly.child"}, "component.child", "semantic.generic.cross.child");

  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_relationship("constraint.shared", AssemblyConstraintType::Coincident, root_target,
                         child_target, AssemblyConstraintState::Inactive)));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.parallel", AssemblyConstraintType::Parallel, child_target, root_target)));
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_relationship("constraint.cross.perpendicular", AssemblyConstraintType::Perpendicular,
                         root_target, child_target)));

  CHECK(project.cross_hierarchy_constraint_count() == 3U);
  const AssemblyHierarchyConstraint* shared =
      project.find_cross_hierarchy_constraint(AssemblyConstraintId("constraint.shared"));
  REQUIRE(shared != nullptr);
  CHECK(shared->type() == AssemblyConstraintType::Coincident);
  CHECK(shared->state() == AssemblyConstraintState::Inactive);
  CHECK(shared->target_a() == root_target);
  CHECK(shared->target_b() == child_target);

  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.root"))
            ->transform() == root_transform);
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
            ->transform() == boundary_transform);
  child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  CHECK(child->find_component_instance(ComponentInstanceId("component.child"))->transform() ==
        child_transform);

  auto solve_graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(solve_graph);
  CHECK(solve_graph.value().relationship_count() == 0U);
  CHECK(solve_graph.value().authority_count() == 0U);
  CHECK(solve_graph.value().incidence_count() == 0U);
  CHECK(solve_graph.value().solve_group_count() == 0U);

  auto motion_graph = AssemblyCrossHierarchyMotionGraph::build(project);
  REQUIRE(motion_graph);
  CHECK(motion_graph.value().relationship_count() == 0U);
  CHECK(motion_graph.value().authority_count() == 0U);
  CHECK(motion_graph.value().incidence_count() == 0U);
  CHECK(motion_graph.value().motion_group_count() == 0U);
}

TEST_CASE("Generic relationship JSON roundtrips local and occurrence-qualified intent",
          "[core][assembly-generic-relationship-json]") {
  auto assembly = local_assembly();
  REQUIRE(assembly.add_constraint(
      local_relationship("constraint.coincident", AssemblyConstraintType::Coincident)));
  REQUIRE(assembly.add_constraint(local_relationship(
      "constraint.parallel", AssemblyConstraintType::Parallel, AssemblyConstraintState::Inactive)));
  REQUIRE(assembly.add_constraint(
      local_relationship("constraint.perpendicular", AssemblyConstraintType::Perpendicular)));

  auto serialized_assembly = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized_assembly);
  const json assembly_json = json::parse(serialized_assembly.value());
  const std::vector<std::string> expected_types{"coincident", "parallel", "perpendicular"};
  CHECK(relationship_type_spellings(assembly_json.at("assembly_constraints")) == expected_types);
  for (const auto& relationship : assembly_json.at("assembly_constraints")) {
    CHECK_FALSE(relationship.contains("distance"));
    CHECK_FALSE(relationship.contains("angle"));
  }
  CHECK(assembly_json.at("assembly_constraints").at(0).at("target_a").at("semantic_reference") ==
        "semantic.generic.target.a");
  CHECK(assembly_json.at("assembly_constraints").at(0).at("target_b").at("semantic_reference") ==
        "semantic.generic.target.b");
  CHECK(assembly_json.at("assembly_constraints").at(1).at("state") == "inactive");

  auto restored_assembly = deserialize_assembly_document_from_json(serialized_assembly.value());
  REQUIRE(restored_assembly);
  REQUIRE(restored_assembly.value().constraint_count() == 3U);
  CHECK(restored_assembly.value()
            .find_constraint(AssemblyConstraintId("constraint.coincident"))
            ->type() == AssemblyConstraintType::Coincident);
  CHECK(restored_assembly.value()
            .find_constraint(AssemblyConstraintId("constraint.parallel"))
            ->state() == AssemblyConstraintState::Inactive);
  CHECK(restored_assembly.value()
            .find_constraint(AssemblyConstraintId("constraint.perpendicular"))
            ->target_a()
            .semantic_reference() == "semantic.generic.target.a");

  auto project = generic_project();
  const auto root_target = hierarchy_target({}, "component.root", "semantic.cross.root");
  const auto child_target =
      hierarchy_target({"subassembly.child"}, "component.child", "semantic.cross.child");
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_relationship("constraint.cross.coincident", AssemblyConstraintType::Coincident,
                         root_target, child_target)));
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_relationship("constraint.cross.parallel", AssemblyConstraintType::Parallel,
                         child_target, root_target, AssemblyConstraintState::Inactive)));
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_relationship("constraint.cross.perpendicular", AssemblyConstraintType::Perpendicular,
                         root_target, child_target)));

  auto serialized_project = serialize_project_to_json(project);
  REQUIRE(serialized_project);
  const json project_json = json::parse(serialized_project.value());
  CHECK(relationship_type_spellings(project_json.at("cross_hierarchy_constraints")) ==
        expected_types);
  CHECK(project_json.at("cross_hierarchy_constraints").at(0).at("target_a").at("occurrence_path") ==
        json::array());
  CHECK(project_json.at("cross_hierarchy_constraints").at(0).at("target_b").at("occurrence_path") ==
        json::array({"subassembly.child"}));
  CHECK(project_json.at("cross_hierarchy_constraints").at(1).at("state") == "inactive");
  for (const auto& relationship : project_json.at("cross_hierarchy_constraints")) {
    CHECK_FALSE(relationship.contains("distance"));
    CHECK_FALSE(relationship.contains("angle"));
  }

  auto restored_project = deserialize_project_from_json(serialized_project.value());
  REQUIRE(restored_project);
  REQUIRE(restored_project.value().validate_assembly_structure());
  CHECK(restored_project.value().cross_hierarchy_constraint_count() == 3U);
  const AssemblyHierarchyConstraint* restored =
      restored_project.value().find_cross_hierarchy_constraint(
          AssemblyConstraintId("constraint.cross.coincident"));
  REQUIRE(restored != nullptr);
  CHECK(restored->type() == AssemblyConstraintType::Coincident);
  CHECK(restored->target_a() == root_target);
  CHECK(restored->target_b() == child_target);
}

TEST_CASE("Generic relationship JSON rejects scalar values and preserves historical five families",
          "[core][assembly-generic-relationship-json]") {
  auto assembly = local_assembly();
  REQUIRE(assembly.add_constraint(
      local_relationship("constraint.coincident", AssemblyConstraintType::Coincident)));
  auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);

  json invalid_distance = json::parse(serialized.value());
  invalid_distance["assembly_constraints"][0]["distance"] = json{{"unit", "mm"}, {"value", 1.0}};
  CHECK(deserialize_assembly_document_from_json(invalid_distance.dump()).has_error());

  json invalid_angle = json::parse(serialized.value());
  invalid_angle["assembly_constraints"][0]["angle"] = json{{"unit", "deg"}, {"value", 90.0}};
  CHECK(deserialize_assembly_document_from_json(invalid_angle.dump()).has_error());

  auto project = generic_project();
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.coincident", AssemblyConstraintType::Coincident,
      hierarchy_target({}, "component.root", "semantic.cross.root"),
      hierarchy_target({"subassembly.child"}, "component.child", "semantic.cross.child"))));
  auto serialized_project = serialize_project_to_json(project);
  REQUIRE(serialized_project);
  json invalid_cross = json::parse(serialized_project.value());
  invalid_cross["cross_hierarchy_constraints"][0]["distance"] =
      json{{"unit", "mm"}, {"value", 2.0}};
  CHECK(deserialize_project_from_json(invalid_cross.dump()).has_error());

  auto historical = local_assembly();
  REQUIRE(historical.add_constraint(
      local_relationship("constraint.mate", AssemblyConstraintType::Mate)));
  REQUIRE(historical.add_constraint(
      local_relationship("constraint.concentric", AssemblyConstraintType::Concentric)));
  REQUIRE(historical.add_constraint(local_relationship(
      "constraint.distance", AssemblyConstraintType::Distance, AssemblyConstraintState::Active,
      distance_quantity(40.0, "constraint.distance"))));
  REQUIRE(historical.add_constraint(
      local_relationship("constraint.insert", AssemblyConstraintType::Insert)));
  REQUIRE(historical.add_constraint(local_relationship(
      "constraint.angle", AssemblyConstraintType::Angle, AssemblyConstraintState::Active,
      std::nullopt, angle_quantity(35.0, "constraint.angle"))));

  auto historical_json_text = serialize_assembly_document_to_json(historical);
  REQUIRE(historical_json_text);
  const json historical_json = json::parse(historical_json_text.value());
  const std::vector<std::string> historical_types{"mate", "concentric", "distance", "insert",
                                                  "angle"};
  CHECK(relationship_type_spellings(historical_json.at("assembly_constraints")) ==
        historical_types);
  CHECK(historical_json_text.value().find("coincident") == std::string::npos);
  CHECK(historical_json_text.value().find("parallel") == std::string::npos);
  CHECK(historical_json_text.value().find("perpendicular") == std::string::npos);

  auto restored_historical = deserialize_assembly_document_from_json(historical_json_text.value());
  REQUIRE(restored_historical);
  CHECK(restored_historical.value().constraint_count() == 5U);
}
