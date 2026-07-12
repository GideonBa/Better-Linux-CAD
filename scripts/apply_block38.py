from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    content = read(path)
    actual = content.count(old)
    if actual != expected:
        raise RuntimeError(
            f"{path}: expected {expected} occurrence(s) of replacement seed, found {actual}"
        )
    write(path, content.replace(old, new))


def create_exact(path: str, content: str) -> None:
    target = ROOT / path
    if target.exists():
        raise RuntimeError(f"{path}: file already exists")
    write(path, content)


replace_exact(
    "include/blcad/core/assembly_constraint.hpp",
    "enum class AssemblyConstraintType { Mate, Concentric, Distance, Insert, Angle };",
    "enum class AssemblyConstraintType {\n"
    "  Mate,\n"
    "  Concentric,\n"
    "  Distance,\n"
    "  Insert,\n"
    "  Angle,\n"
    "  Coincident,\n"
    "  Parallel,\n"
    "  Perpendicular\n"
    "};",
)
replace_exact(
    "include/blcad/core/assembly_constraint.hpp",
    "// Solver-independent local assembly relationship intent. Distance constraints\n"
    "// carry one length quantity and Angle constraints carry one angle quantity;\n"
    "// Mate, Concentric, and Insert intentionally carry no value.",
    "// Solver-independent local assembly relationship intent. Distance constraints\n"
    "// carry one length quantity and Angle constraints carry one angle quantity;\n"
    "// Mate, Concentric, Insert, Coincident, Parallel, and Perpendicular intentionally\n"
    "// carry no scalar value. Generic Block-38 families remain persistent-only until\n"
    "// Block 39 adds equation and graph participation semantics.",
)

replace_exact(
    "src/core/assembly_constraint.cpp",
    "  case AssemblyConstraintType::Angle:\n"
    "    return \"angle\";\n"
    "  }",
    "  case AssemblyConstraintType::Angle:\n"
    "    return \"angle\";\n"
    "  case AssemblyConstraintType::Coincident:\n"
    "    return \"coincident\";\n"
    "  case AssemblyConstraintType::Parallel:\n"
    "    return \"parallel\";\n"
    "  case AssemblyConstraintType::Perpendicular:\n"
    "    return \"perpendicular\";\n"
    "  }",
)

for path, error_text in (
    ("src/core/assembly_document_json.cpp", "unsupported assembly constraint type"),
    ("src/core/project_json.cpp", "unsupported cross-hierarchy assembly constraint type"),
):
    replace_exact(
        path,
        "  if (text == \"angle\")\n"
        "    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Angle);\n"
        f"  return Result<AssemblyConstraintType>::failure(\n"
        + (
            f"      json_error(\"{error_text}\"));"
            if path.endswith("project_json.cpp")
            else f"      json_error(\"{error_text}\"));"
        ),
        "  if (text == \"angle\")\n"
        "    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Angle);\n"
        "  if (text == \"coincident\")\n"
        "    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Coincident);\n"
        "  if (text == \"parallel\")\n"
        "    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Parallel);\n"
        "  if (text == \"perpendicular\")\n"
        "    return Result<AssemblyConstraintType>::success(AssemblyConstraintType::Perpendicular);\n"
        f"  return Result<AssemblyConstraintType>::failure(\n"
        f"      json_error(\"{error_text}\"));",
    )

create_exact(
    "src/core/assembly_constraint_graph_participation.hpp",
    '''#pragma once

#include "blcad/core/assembly_constraint.hpp"

namespace blcad {

// Block 38 adds persistent generic relationship intent only. Keep the existing
// five equation-enabled families in solve/motion connectivity until Block 39
// implements generic relationship equations and shared graph integration.
[[nodiscard]] inline bool
participates_in_current_assembly_constraint_graph(AssemblyConstraintType type) noexcept {
  switch (type) {
  case AssemblyConstraintType::Mate:
  case AssemblyConstraintType::Concentric:
  case AssemblyConstraintType::Distance:
  case AssemblyConstraintType::Insert:
  case AssemblyConstraintType::Angle:
    return true;
  case AssemblyConstraintType::Coincident:
  case AssemblyConstraintType::Parallel:
  case AssemblyConstraintType::Perpendicular:
    return false;
  }
  return false;
}

} // namespace blcad
''',
)

replace_exact(
    "src/core/assembly_constraint_graph.cpp",
    '#include "blcad/core/assembly_constraint_graph.hpp"\n',
    '#include "blcad/core/assembly_constraint_graph.hpp"\n\n'
    '#include "assembly_constraint_graph_participation.hpp"\n',
)
replace_exact(
    "src/core/assembly_constraint_graph.cpp",
    "    if (constraint.state() == AssemblyConstraintState::Inactive) {\n"
    "      continue;\n"
    "    }\n\n"
    "    const ComponentInstanceId& component_a",
    "    if (constraint.state() == AssemblyConstraintState::Inactive) {\n"
    "      continue;\n"
    "    }\n"
    "    if (!participates_in_current_assembly_constraint_graph(constraint.type())) {\n"
    "      continue;\n"
    "    }\n\n"
    "    const ComponentInstanceId& component_a",
)

for path in (
    "src/core/assembly_cross_hierarchy_constraint_graph.cpp",
    "src/core/assembly_cross_hierarchy_motion_graph.cpp",
):
    first_include = (
        '#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"\n'
        if "constraint_graph" in path
        else '#include "blcad/core/assembly_cross_hierarchy_motion_graph.hpp"\n'
    )
    replace_exact(
        path,
        first_include,
        first_include + '\n#include "assembly_constraint_graph_participation.hpp"\n',
    )
    replace_exact(
        path,
        "    if (constraint.state() != AssemblyConstraintState::Active) {\n"
        "      continue;\n"
        "    }",
        "    if (constraint.state() != AssemblyConstraintState::Active) {\n"
        "      continue;\n"
        "    }\n"
        "    if (!participates_in_current_assembly_constraint_graph(constraint.type())) {\n"
        "      continue;\n"
        "    }",
        expected=2,
    )

replace_exact(
    "src/geometry/assembly_target_compatibility.cpp",
    "  case AssemblyConstraintType::Insert:\n"
    "    return {{Capability::Frame, Capability::Frame}};\n"
    "  }",
    "  case AssemblyConstraintType::Insert:\n"
    "    return {{Capability::Frame, Capability::Frame}};\n"
    "  case AssemblyConstraintType::Coincident:\n"
    "  case AssemblyConstraintType::Parallel:\n"
    "  case AssemblyConstraintType::Perpendicular:\n"
    "    return {};\n"
    "  }",
)

replace_exact(
    "CMakeLists.txt",
    "    tests/core/assembly_constraint_graph_tests.cpp\n"
    "    tests/core/assembly_constraint_tests.cpp\n",
    "    tests/core/assembly_constraint_graph_tests.cpp\n"
    "    tests/core/assembly_constraint_tests.cpp\n"
    "    tests/core/assembly_generic_relationship_tests.cpp\n",
)

create_exact(
    "tests/core/assembly_generic_relationship_tests.cpp",
    r'''#include "blcad/core/assembly_constraint_graph.hpp"
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
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
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

AssemblyConstraint local_relationship(
    const char* id, AssemblyConstraintType type,
    AssemblyConstraintState state = AssemblyConstraintState::Active,
    std::optional<Quantity> distance = std::nullopt,
    std::optional<Quantity> angle = std::nullopt) {
  auto relationship = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type,
      local_target("component.a", "semantic.generic.target.a"),
      local_target("component.b", "semantic.generic.target.b"), state, std::move(distance),
      std::move(angle));
  REQUIRE(relationship);
  return relationship.value();
}

AssemblyHierarchyConstraint cross_relationship(
    const char* id, AssemblyConstraintType type,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state = AssemblyConstraintState::Active,
    std::optional<Quantity> distance = std::nullopt,
    std::optional<Quantity> angle = std::nullopt) {
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
  REQUIRE(root.value().add_subassembly_instance(
      subassembly("subassembly.child", "assembly.child")));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(component("component.child", 20.0)));

  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);

  auto project = Project::create(DocumentId("project.generic"), "GenericProject",
                                 std::move(root.value()));
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
    auto relationship = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.generic"), "Generic", type, target_a, target_b,
        AssemblyConstraintState::Inactive);
    REQUIRE(relationship);
    CHECK(relationship.value().type() == type);
    CHECK(relationship.value().state() == AssemblyConstraintState::Inactive);
    CHECK(relationship.value().target_a() == target_a);
    CHECK(relationship.value().target_b() == target_b);
    CHECK_FALSE(relationship.value().distance().has_value());
    CHECK_FALSE(relationship.value().angle().has_value());

    CHECK(AssemblyConstraint::create(
              AssemblyConstraintId("constraint.generic.distance"), "Generic distance", type,
              target_a, target_b, AssemblyConstraintState::Active, distance)
              .has_error());
    CHECK(AssemblyConstraint::create(
              AssemblyConstraintId("constraint.generic.angle"), "Generic angle", type, target_a,
              target_b, AssemblyConstraintState::Active, std::nullopt, angle)
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
  REQUIRE(assembly.add_constraint(local_relationship("constraint.mate", AssemblyConstraintType::Mate)));

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

  const RigidTransform root_transform = project.assembly()
                                            .find_component_instance(
                                                ComponentInstanceId("component.root"))
                                            ->transform();
  const RigidTransform boundary_transform = project.assembly()
                                                .find_subassembly_instance(
                                                    SubassemblyInstanceId("subassembly.child"))
                                                ->transform();
  const AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  const RigidTransform child_transform = child->find_component_instance(
                                                   ComponentInstanceId("component.child"))
                                             ->transform();

  auto local_a = AssemblyConstraintTarget::create(ComponentInstanceId("component.root"),
                                                   "semantic.generic.local.a");
  auto local_b = AssemblyConstraintTarget::create(ComponentInstanceId("component.root"),
                                                   "semantic.generic.local.b");
  REQUIRE(local_a);
  REQUIRE(local_b);
  auto local = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.shared"), "Local shared", AssemblyConstraintType::Coincident,
      std::move(local_a.value()), std::move(local_b.value()));
  REQUIRE(local);
  REQUIRE(project.assembly().add_constraint(std::move(local.value())));

  const auto root_target =
      hierarchy_target({}, "component.root", "semantic.generic.cross.root");
  const auto child_target = hierarchy_target(
      {"subassembly.child"}, "component.child", "semantic.generic.cross.child");

  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.shared", AssemblyConstraintType::Coincident, root_target, child_target,
      AssemblyConstraintState::Inactive)));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.parallel", AssemblyConstraintType::Parallel, child_target, root_target)));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.perpendicular", AssemblyConstraintType::Perpendicular, root_target,
      child_target)));

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
  const auto child_target = hierarchy_target(
      {"subassembly.child"}, "component.child", "semantic.cross.child");
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.coincident", AssemblyConstraintType::Coincident, root_target, child_target)));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.parallel", AssemblyConstraintType::Parallel, child_target, root_target,
      AssemblyConstraintState::Inactive)));
  REQUIRE(project.add_cross_hierarchy_constraint(cross_relationship(
      "constraint.cross.perpendicular", AssemblyConstraintType::Perpendicular, root_target,
      child_target)));

  auto serialized_project = serialize_project_to_json(project);
  REQUIRE(serialized_project);
  const json project_json = json::parse(serialized_project.value());
  CHECK(relationship_type_spellings(project_json.at("cross_hierarchy_constraints")) ==
        expected_types);
  CHECK(project_json.at("cross_hierarchy_constraints")
            .at(0)
            .at("target_a")
            .at("occurrence_path") == json::array());
  CHECK(project_json.at("cross_hierarchy_constraints")
            .at(0)
            .at("target_b")
            .at("occurrence_path") == json::array({"subassembly.child"}));
  CHECK(project_json.at("cross_hierarchy_constraints").at(1).at("state") == "inactive");
  for (const auto& relationship : project_json.at("cross_hierarchy_constraints")) {
    CHECK_FALSE(relationship.contains("distance"));
    CHECK_FALSE(relationship.contains("angle"));
  }

  auto restored_project = deserialize_project_from_json(serialized_project.value());
  REQUIRE(restored_project);
  REQUIRE(restored_project.value().validate_assembly_structure());
  CHECK(restored_project.value().cross_hierarchy_constraint_count() == 3U);
  const AssemblyHierarchyConstraint* restored = restored_project.value().find_cross_hierarchy_constraint(
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
  invalid_distance["assembly_constraints"][0]["distance"] =
      json{{"unit", "mm"}, {"value", 1.0}};
  CHECK(deserialize_assembly_document_from_json(invalid_distance.dump()).has_error());

  json invalid_angle = json::parse(serialized.value());
  invalid_angle["assembly_constraints"][0]["angle"] =
      json{{"unit", "deg"}, {"value", 90.0}};
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
  const std::vector<std::string> historical_types{
      "mate", "concentric", "distance", "insert", "angle"};
  CHECK(relationship_type_spellings(historical_json.at("assembly_constraints")) ==
        historical_types);
  CHECK(historical_json_text.value().find("coincident") == std::string::npos);
  CHECK(historical_json_text.value().find("parallel") == std::string::npos);
  CHECK(historical_json_text.value().find("perpendicular") == std::string::npos);

  auto restored_historical =
      deserialize_assembly_document_from_json(historical_json_text.value());
  REQUIRE(restored_historical);
  CHECK(restored_historical.value().constraint_count() == 5U);
}
''',
)

create_exact(
    "docs/assembly-generic-relationship-intent-mvp5.md",
    '''# Assembly Generic Geometric Relationship Intent and JSON MVP-5

Status: implemented as Block 38 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for persistent local and Project-level `Coincident`, `Parallel`, and `Perpendicular` relationship intent, their JSON spellings, value-family validation, graph-participation boundary, and Block-39 handoff.

## Scope

Implemented:

- `AssemblyConstraintType::Coincident`;
- `AssemblyConstraintType::Parallel`;
- `AssemblyConstraintType::Perpendicular`;
- shared local `AssemblyConstraint` intent for all three families;
- shared occurrence-qualified `AssemblyHierarchyConstraint` intent for all three families;
- existing `AssemblyConstraintTarget` and `AssemblyHierarchyConstraintEndpoint` identity shapes without modification;
- active/inactive state reuse;
- exact target A/B order preservation;
- local and Project-level id-scope independence;
- canonical lowercase JSON spellings `coincident`, `parallel`, and `perpendicular`;
- fail-closed scalar-value validation;
- historical five-family JSON compatibility;
- explicit exclusion from current solve and motion graphs until Block 39.

Not implemented:

- Coincident, Parallel, or Perpendicular target compatibility entries;
- residual or equation descriptors for the new families;
- local or cross-hierarchy numeric execution for the new families;
- graph incidence or solve-group participation for the new families;
- freshness/application/diagnostic integration for the new families;
- any new JSON field or schema/version marker.

## Persistent relationship family expansion

The shared Core relationship type is now:

```text
Mate
Concentric
Distance
Insert
Angle
Coincident
Parallel
Perpendicular
```

The three Block-38 families reuse the same relationship records already used by the historical five families.

Local:

```text
AssemblyConstraint
  id
  name
  type
  target_a
  target_b
  state
  optional distance
  optional angle
```

Project-level:

```text
AssemblyHierarchyConstraint
  id
  name
  type
  target_a
  target_b
  state
  optional distance
  optional angle
```

No parallel relationship hierarchy, generic relationship base class, or second endpoint model is introduced.

## Endpoint identity remains unchanged

Local endpoint identity remains:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Project-level endpoint identity remains:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

`occurrence_path` remains the exact ordered root-to-current `SubassemblyInstanceId` sequence. The empty path addresses the explicit root occurrence.

The semantic-reference string remains opaque Core model intent. Constructing, adding, serializing, or loading a Block-38 relationship does not resolve Plane/Axis/Line/Point/Circle/Cylinder/Frame geometry.

## Target order and state

Target A/B order is persistent for every relationship family. Block 38 does not canonicalize, sort, or swap endpoints.

The existing state values are reused unchanged:

```text
active
inactive
```

An inactive generic relationship is still persistent model intent. State does not authorize Geometry execution in Block 38.

## Value-family rules

The existing scalar-value authority remains:

```text
Distance
  distance = required LengthMm
  angle    = absent

Angle
  distance = absent
  angle    = required AngleDeg

Mate
Concentric
Insert
Coincident
Parallel
Perpendicular
  distance = absent
  angle    = absent
```

Therefore `Coincident`, `Parallel`, and `Perpendicular` fail closed if either `distance` or `angle` is supplied.

The typed `AssemblyConstraint::create` validation remains the single value-family authority and is reused by `AssemblyHierarchyConstraint::create`. JSON parsing does not bypass it.

## Local and Project-level id scopes

Local relationship ids remain unique inside one `AssemblyDocument`.

Project-level cross-hierarchy relationship ids remain unique inside the Project-owned `cross_hierarchy_constraints[]` collection.

These scopes remain independent. One local relationship and one Project-level relationship may intentionally share the same `AssemblyConstraintId` text.

## JSON

Block 38 adds accepted type spellings only:

```text
coincident
parallel
perpendicular
```

Local JSON shape remains:

```json
{
  "id": "constraint.coincident",
  "name": "Coincident",
  "type": "coincident",
  "target_a": {
    "component_instance": "component.a",
    "semantic_reference": "topo:vertex:feature%2Ebox:top_front_right"
  },
  "target_b": {
    "component_instance": "component.b",
    "semantic_reference": "ref:construction_point:point_b"
  },
  "state": "active"
}
```

Project-level JSON shape remains:

```json
{
  "id": "constraint.cross.parallel",
  "name": "Parallel",
  "type": "parallel",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root",
    "semantic_reference": "ref:datum_axis:axis_root"
  },
  "target_b": {
    "occurrence_path": ["subassembly.child"],
    "component_instance": "component.child",
    "semantic_reference": "topo:linear_edge:feature%2Ebox:top_front"
  },
  "state": "inactive"
}
```

Neither shape gains a new field. Existing schema markers remain:

```text
blcad.assembly_document.mvp4 / version 1
blcad.project.mvp4 / version 1
```

Historical Mate/Concentric/Distance/Insert/Angle files retain the same accepted spellings and serialized shapes.

## Persistent-only execution boundary

Block 38 intentionally does not activate the three new families in existing solve or motion connectivity.

The private Core graph-participation boundary classifies:

```text
Mate / Concentric / Distance / Insert / Angle
  -> current graph participation = yes

Coincident / Parallel / Perpendicular
  -> current graph participation = no
```

This guard is required because the pre-Block-38 graph builders iterate generic `AssemblyConstraint` collections. Without an explicit boundary, merely extending the enum would accidentally place unsupported relationships into solve and motion groups before equations exist.

`AssemblyConstraintGraph`, `AssemblyCrossHierarchyConstraintGraph`, and `AssemblyCrossHierarchyMotionGraph` therefore preserve their historical equation-enabled participation set. Existing graph behavior is unchanged for the original five families.

The Block-37 `AssemblyTargetCompatibilityResolver` likewise has no compatibility entries for the Block-38 types. Direct Geometry compatibility/equation requests fail closed until Block 39.

## Mutation boundary

Adding Block-38 relationship intent may mutate only the owning relationship collection.

It does not mutate:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
PartDocument intent
semantic-reference strings
hierarchy structure
joint coordinates
```

Loading performs Core structure/value validation only and resolves no Geometry.

## Focused acceptance coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
```

`tests/core/assembly_generic_relationship_tests.cpp` proves:

- all three enum values and canonical lowercase spellings;
- local intent creation and active/inactive state retention;
- target A/B order retention;
- no distance/angle values on generic families;
- scalar injection fails closed through Core/JSON validation;
- Project-level occurrence-qualified intent;
- independent local and Project-level id scopes;
- local and cross-hierarchy transform immutability;
- exact occurrence-path JSON roundtrip;
- all three new type spellings roundtrip locally and Project-level;
- historical five-family spellings remain unchanged;
- generic relationships remain absent from current local solve, cross-hierarchy solve, and cross-hierarchy motion graphs.

## Handoff

The next technical step is Block 39: generic relationship equations and shared solve integration.

Block 39 must extend Block-37 compatibility and Geometry execution for the documented initial capability pairs, then integrate `Coincident`, `Parallel`, and `Perpendicular` through the existing local/cross-hierarchy graphs, residual/Jacobian engine, freshness, atomic application, and diagnostics boundaries.

Block 39 must not create a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model.
''',
)

# Documentation status and handoff updates.
replace_exact(
    "docs/mvp-plan.md",
    "implemented_through: Block 37\ncurrent_block: 38\ncurrent_boundary: Generic geometric relationship Core intent and JSON\ncurrent_tag: \"(assigned when Block 38 starts)\"",
    "implemented_through: Block 38\ncurrent_block: 39\ncurrent_boundary: Generic relationship equations and shared solve integration\ncurrent_tag: \"(assigned when Block 39 starts)\"",
)
replace_exact(
    "docs/mvp-plan.md",
    '  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–37 implemented, Blocks 38–47 planned"',
    '  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–38 implemented, Blocks 39–47 planned"',
)
replace_exact(
    "docs/mvp-plan.md",
    '''## Blocks 38–47 — Planned general relationship and joint continuation

**Status:** Planned
**Canonical:** roadmap `docs/assembly-general-geometric-target-roadmap.md`

Mandatory order:

```text
38 generic geometric relationship Core intent + JSON
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

**Notes:** Do not merge these blocks.
''',
    '''### Block 38 — Generic geometric relationship Core intent and JSON

**Status:** Implemented
**Canonical:** `docs/assembly-generic-relationship-intent-mvp5.md`

Implemented shared local and Project-level persistent `Coincident`, `Parallel`, and `Perpendicular` relationship intent. The three families reuse existing local and occurrence-qualified endpoint shapes, target A/B order, active/inactive state semantics, and independent local/Project id scopes.

Canonical JSON spellings are:

```text
coincident
parallel
perpendicular
```

All three families carry neither `distance` nor `angle`; typed Core construction and JSON loading fail closed on scalar injection. Existing five-family spellings and JSON shapes remain unchanged.

Until Block 39 adds equations, the three generic families remain persistent-only and are explicitly excluded from current local solve, cross-hierarchy solve, and cross-hierarchy motion graph participation.

**Focused tags:**

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

## Blocks 39–47 — Planned general relationship and joint continuation

**Status:** Planned
**Canonical:** roadmap `docs/assembly-general-geometric-target-roadmap.md`

Mandatory order:

```text
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

**Notes:** Do not merge these blocks.
''',
)

replace_exact(
    "docs/architecture-summary.md",
    '''Persistent geometric relationship families remain:

```text
Mate
Distance
Angle
Concentric
Insert
```
''',
    '''Persistent geometric relationship families are:

```text
Mate
Distance
Angle
Concentric
Insert
Coincident
Parallel
Perpendicular
```

Current equation-enabled relationship families remain the historical five: Mate, Distance, Angle, Concentric, and Insert. Block-38 Coincident/Parallel/Perpendicular intent remains persistent-only until Block 39.
''',
)
replace_exact(
    "docs/architecture-summary.md",
    "Local and cross-hierarchy graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values.",
    "Current local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values for equation-enabled relationship families. Block-38 generic relationship intent is explicitly excluded from graph participation until Block 39 adds equations and shared integration.",
)
replace_exact(
    "docs/architecture-summary.md",
    "Block 37 adds deterministic target compatibility selection through `AssemblyTargetCompatibilityResolver`: relationship type plus target A/B capability vectors produce one ordered capability pair or an explicit incompatibility. Existing local and cross-hierarchy Mate/Distance/Angle/Concentric/Insert equation builders consume that compatibility result before projection, without adding new equations or JSON.\n",
    "Block 37 adds deterministic target compatibility selection through `AssemblyTargetCompatibilityResolver`: relationship type plus target A/B capability vectors produce one ordered capability pair or an explicit incompatibility. Existing local and cross-hierarchy Mate/Distance/Angle/Concentric/Insert equation builders consume that compatibility result before projection, without adding new equations or JSON.\n\nBlock 38 extends shared Core relationship intent and existing local/Project JSON type spellings with `Coincident`, `Parallel`, and `Perpendicular`. They reuse endpoint/order/state/id-scope semantics, carry no scalar quantity, resolve no Geometry during Core construction/loading, and remain outside current solve/motion graphs until Block 39.\n",
)
replace_exact(
    "docs/architecture-summary.md",
    "Block 33 adds `datum_axes` PartDocument persistence. Block 35 adds canonical semantic endpoint strings but no JSON field or generated-topology cache. `docs/file-format.md` remains save-format authority.",
    "Block 33 adds `datum_axes` PartDocument persistence. Block 35 adds canonical semantic endpoint strings but no JSON field or generated-topology cache. Block 38 adds accepted relationship type spellings without changing local or Project-level relationship record shapes. `docs/file-format.md` remains save-format authority.",
)
replace_exact(
    "docs/architecture-summary.md",
    "Blocks 23–37 of the current assembly sequence are implemented.",
    "Blocks 23–38 of the current assembly sequence are implemented.",
)
replace_exact(
    "docs/architecture-summary.md",
    "- `docs/assembly-generated-topology-reference-mvp5.md`\n- `docs/assembly-general-geometric-target-roadmap.md`",
    "- `docs/assembly-generated-topology-reference-mvp5.md`\n- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/assembly-general-geometric-target-roadmap.md`",
)
replace_exact(
    "docs/architecture-summary.md",
    "The next technical step is Block 38 only: generic geometric relationship Core intent and JSON for `Coincident`, `Parallel`, and `Perpendicular`.\n\nBlock 37 target compatibility is implemented. Generic relationship equations, occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain deferred according to their roadmap blocks.",
    "The next technical step is Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.\n\nBlock 38 persistent intent/JSON is implemented. Generic relationship equations and graph participation, occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain deferred according to their roadmap blocks.",
)

replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Status: Blocks 31–37 are implemented. Block 38 is the current next technical step. Blocks 38–47 remain planned headless architecture.",
    "Status: Blocks 31–38 are implemented. Block 39 is the current next technical step. Blocks 39–47 remain planned headless architecture.",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "- `docs/assembly-generated-topology-reference-mvp5.md`",
    "- `docs/assembly-generated-topology-reference-mvp5.md`\n- `docs/assembly-generic-relationship-intent-mvp5.md`",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Blocks 32 through 37 are implemented. The remaining order is unchanged.",
    "Blocks 32 through 38 are implemented. The remaining order is unchanged.",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "  -> 37 explicit target compatibility matrix (implemented)\n  -> 38 generic geometric relationship Core intent + JSON\n  -> 39 generic relationship equations + shared solve integration",
    "  -> 37 explicit target compatibility matrix (implemented)\n  -> 38 generic geometric relationship Core intent + JSON (implemented)\n  -> 39 generic relationship equations + shared solve integration",
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    '''## Block 38 — Generic geometric relationship Core intent and JSON

Primary boundary: persistent Core relationship model and serialization.

Add local and Project-level:

```text
Coincident
Parallel
Perpendicular
```

Preserve endpoint shapes, target A/B order, active/inactive state semantics, local versus Project-level id scopes, and historical five-family compatibility. No equation or graph participation changes belong in Block 38.
''',
    '''## Block 38 — Generic geometric relationship Core intent and JSON — Implemented

Canonical document: `docs/assembly-generic-relationship-intent-mvp5.md`.

Primary boundary: persistent Core relationship model and serialization.

Implemented local and Project-level:

```text
Coincident
Parallel
Perpendicular
```

The shared `AssemblyConstraintType` enum now carries all three families. Local `AssemblyConstraint` and Project-level `AssemblyHierarchyConstraint` reuse unchanged endpoint records, preserve target A/B order, reuse active/inactive state semantics, and retain independent local versus Project-level id scopes.

Canonical JSON spellings are:

```text
coincident
parallel
perpendicular
```

All three new families carry neither `distance` nor `angle`; typed Core construction remains value-family authority and JSON loading reuses it fail closed. Existing Mate/Concentric/Distance/Insert/Angle JSON spellings and shapes remain unchanged.

Because existing graph builders are generic over `AssemblyConstraint`, Block 38 adds an explicit private participation guard preserving the historical five equation-enabled graph families. Coincident/Parallel/Perpendicular remain absent from current local solve, cross-hierarchy solve, and cross-hierarchy motion graphs until Block 39.

Focused tags:

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

`tests/core/assembly_generic_relationship_tests.cpp` proves local and occurrence-qualified intent, independent id scopes, target order, state, scalar rejection, lowercase JSON roundtrips, historical five-family compatibility, transform immutability, and persistent-only nonparticipation in current solve/motion graphs.

Block 38 adds no residual/equation, no target compatibility entry for the new families, no JSON field/schema bump, and no Geometry resolution during Core construction or loading.
''',
)
replace_exact(
    "docs/assembly-general-geometric-target-roadmap.md",
    "Block 38 is the current next technical step.\n\nImplement Block 38 only: generic geometric relationship Core intent and JSON for `Coincident`, `Parallel`, and `Perpendicular`, while preserving existing local and Project-level id scopes, endpoint shapes, target order, state/value validation, and historical five-family compatibility.\n\nDo not add generic relationship equations in Block 38; Geometry execution begins at Block 39.",
    "Block 39 is the current next technical step.\n\nImplement Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular` through Block-37 capability compatibility, existing local/cross-hierarchy graphs, the shared residual/Jacobian engine, freshness, atomic application, and diagnostics.\n\nDo not introduce a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model in Block 39.",
)

replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "Status: Blocks 23–37 are implemented. Block 38 is the current next technical step. Blocks 38–47 remain the explicit general assembly relationship and richer-joint continuation.",
    "Status: Blocks 23–38 are implemented. Block 39 is the current next technical step. Blocks 39–47 remain the explicit general assembly relationship and richer-joint continuation.",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "- `docs/assembly-generated-topology-reference-mvp5.md`\n- `docs/file-format.md`",
    "- `docs/assembly-generated-topology-reference-mvp5.md`\n- `docs/assembly-generic-relationship-intent-mvp5.md`\n- `docs/file-format.md`",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "37 explicit target compatibility matrix (implemented)\n38 generic geometric relationship Core intent + JSON\n39 generic relationship equations + shared solve integration",
    "37 explicit target compatibility matrix (implemented)\n38 generic geometric relationship Core intent + JSON (implemented)\n39 generic relationship equations + shared solve integration",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    '''## Blocks 38–40 — Generic relationship and joint target semantics

Block 38 adds persistent local/Project-level `Coincident`, `Parallel`, and `Perpendicular` intent plus JSON.

Block 39 adds their equations through existing graph, authority, residual/Jacobian, freshness, atomic application, and diagnostics paths.

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.
''',
    '''## Block 38 — Generic relationship Core intent and JSON — Implemented

Canonical: `docs/assembly-generic-relationship-intent-mvp5.md`.

The shared Core relationship enum now includes `Coincident`, `Parallel`, and `Perpendicular` for both local and Project-level records. Existing endpoint shapes, exact target A/B order, active/inactive state, and independent local/Project id scopes remain unchanged.

The three new families carry no `distance` or `angle`. JSON accepts only lowercase `coincident`, `parallel`, and `perpendicular`; local `assembly_constraints[]` and Project `cross_hierarchy_constraints[]` shapes are unchanged and historical five-family files remain compatible.

The new families remain persistent-only until Block 39. A private graph-participation guard keeps them out of current local solve, cross-hierarchy solve, and cross-hierarchy motion connectivity, preserving the historical five equation-enabled graph families.

Focused tags:

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

## Blocks 39–40 — Generic relationship execution and joint target semantics

Block 39 adds Coincident/Parallel/Perpendicular equations through existing graph, authority, residual/Jacobian, freshness, atomic application, and diagnostics paths.

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.
''',
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "local geometric relationships\nlocal Revolute joints/limits/coordinate",
    "local geometric relationships including persistent Coincident/Parallel/Perpendicular intent\nlocal Revolute joints/limits/coordinate",
)
replace_exact(
    "docs/assembly-cross-hierarchy-solver-sequence-mvp5.md",
    "Implement Block 38 only: generic geometric relationship Core intent and JSON for `Coincident`, `Parallel`, and `Perpendicular`.\n\nDo not add generic relationship equations in Block 38; those begin at Block 39.",
    "Implement Block 39 only: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.\n\nReuse Block-37 compatibility, existing local/cross-hierarchy graphs and authority mapping, the shared residual/Jacobian engine, freshness, atomic application, and diagnostics. Do not introduce parallel execution infrastructure.",
)

replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    "Blocks 32–37 have since extended semantic source identity, generated-topology resolution, and explicit target compatibility without changing persistent endpoint authority.",
    "Blocks 32–38 have since extended semantic source identity, generated-topology resolution, explicit target compatibility, and generic relationship intent without changing persistent endpoint authority.",
)
replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    "Geometry resolution of those Block-35 `topo:` identities through this taxonomy is implemented as Block 36; explicit relationship target compatibility is implemented as Block 37. Their canonical contracts live in `docs/assembly-general-geometric-target-roadmap.md`.",
    "Geometry resolution of those Block-35 `topo:` identities through this taxonomy is implemented as Block 36; explicit relationship target compatibility is implemented as Block 37. Block 38 adds persistent generic relationship intent without extending the compatibility matrix. Their canonical contracts live in `docs/assembly-general-geometric-target-roadmap.md` and `docs/assembly-generic-relationship-intent-mvp5.md`.",
)
replace_exact(
    "docs/assembly-geometric-target-taxonomy-mvp5.md",
    '''## Next technical step — Block 38

The next technical step is Block 38: generic geometric relationship Core intent and JSON for `Coincident`, `Parallel`, and `Perpendicular`.
''',
    '''## Block 38 — generic relationship intent — Implemented

Block 38 adds persistent `Coincident`, `Parallel`, and `Perpendicular` relationship types and JSON spellings at Core level. It does not change this target taxonomy or the Block-37 compatibility matrix.

`AssemblyTargetCompatibilityResolver` therefore has no compatibility entries for the three generic families yet. Direct compatibility/equation requests remain explicitly incompatible until Block 39 integrates their documented capability pairs and equations.

Canonical Block-38 contract: `docs/assembly-generic-relationship-intent-mvp5.md`.

## Next technical step — Block 39

The next technical step is Block 39: generic relationship equations and shared solve integration for `Coincident`, `Parallel`, and `Perpendicular`.
''',
)

replace_exact(
    "docs/assembly-generated-topology-reference-mvp5.md",
    "Block 37 explicit target compatibility is now implemented. Its focused tags are `[geometry][assembly-target-compatibility]` and `[geometry][assembly-cross-hierarchy-target-compatibility]`; it adds no generated-topology JSON field or new equation.\n\nThe next technical step is Block 38: generic geometric relationship Core intent and JSON.",
    "Block 37 explicit target compatibility is implemented. Block 38 generic relationship Core intent and JSON is also implemented in `docs/assembly-generic-relationship-intent-mvp5.md`; neither block adds a generated-topology JSON field.\n\nThe next technical step is Block 39: generic relationship equations and shared solve integration.",
)

replace_exact(
    "docs/file-format.md",
    '''```text
mate
concentric
distance
insert
angle
```''',
    '''```text
mate
concentric
distance
insert
angle
coincident
parallel
perpendicular
```''',
)
replace_exact(
    "docs/file-format.md",
    '''```text
Mate        -> omit distance and angle
Concentric  -> omit distance and angle
Insert      -> omit distance and angle
Distance    -> require distance, omit angle
Angle       -> require angle, omit distance
```''',
    '''```text
Mate           -> omit distance and angle
Concentric     -> omit distance and angle
Insert         -> omit distance and angle
Coincident     -> omit distance and angle
Parallel       -> omit distance and angle
Perpendicular  -> omit distance and angle
Distance       -> require distance, omit angle
Angle          -> require angle, omit distance
```''',
)
replace_exact(
    "docs/file-format.md",
    "Typed Core constructors remain authoritative after JSON parsing.",
    "Typed Core constructors remain authoritative after JSON parsing. Block 38 adds only the three accepted lowercase type spellings; local and Project-level relationship record shapes, endpoint shapes, target order, and state fields are unchanged. Historical five-family files remain compatible.",
)
replace_exact(
    "docs/file-format.md",
    "Block 35 does not persist generated-topology role matrices, producer classification, validation results, recovery results, or OCCT topology. Block 36 Geometry resolution of `topo:` identities into typed descriptors/capabilities is derived query state and likewise adds no JSON field. Block 37 target compatibility selection is also derived query state and adds no JSON field.",
    "Block 35 does not persist generated-topology role matrices, producer classification, validation results, recovery results, or OCCT topology. Block 36 Geometry resolution of `topo:` identities into typed descriptors/capabilities is derived query state and likewise adds no JSON field. Block 37 target compatibility selection is also derived query state and adds no JSON field. Block 38 extends accepted relationship `type` values with `coincident`, `parallel`, and `perpendicular` without adding a field or changing schema/version markers.",
)
replace_exact(
    "docs/file-format.md",
    "Local numeric solving derives geometric relationship ids and optional transient local Revolute drives.",
    "Local numeric solving currently derives equation-enabled geometric relationship ids and optional transient local Revolute drives. Block-38 Coincident/Parallel/Perpendicular records remain persisted but excluded from solve/motion graph participation until Block 39.",
)

replace_exact(
    "docs/development-setup.md",
    './build/dev/blcad_core_tests "[core][assembly-constraint]"\n',
    './build/dev/blcad_core_tests "[core][assembly-constraint]"\n'
    './build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"\n'
    './build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"\n',
)
replace_exact(
    "docs/development-setup.md",
    "- local Mate/Distance/Angle/Concentric/Insert intent and solving;",
    "- local Mate/Distance/Angle/Concentric/Insert intent and solving;\n- persistent local and Project-level Coincident/Parallel/Perpendicular intent and lowercase JSON roundtrips;\n- generic relationship target order/state/id-scope preservation and scalar rejection;\n- Block-38 generic relationships excluded from current solve/motion graph participation until Block 39;",
)
replace_exact(
    "docs/development-setup.md",
    "- `docs/assembly-generated-topology-reference-mvp5.md`: Block-35 generated-topology identity/recovery contract\n- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–37 and planned Blocks 38–47",
    "- `docs/assembly-generated-topology-reference-mvp5.md`: Block-35 generated-topology identity/recovery contract\n- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract\n- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–38 and planned Blocks 39–47",
)
replace_exact(
    "docs/development-setup.md",
    '''For Block 35 production/test files:

```bash
clang-format -i \\
  include/blcad/core/generated_topology_reference.hpp \\
  include/blcad/core/reference_recovery.hpp \\
  src/core/generated_topology_reference.cpp \\
  src/core/reference_recovery.cpp \\
  tests/core/generated_topology_reference_tests.cpp
```''',
    '''For Block 38 production/test files:

```bash
clang-format -i \\
  include/blcad/core/assembly_constraint.hpp \\
  src/core/assembly_constraint.cpp \\
  src/core/assembly_constraint_graph.cpp \\
  src/core/assembly_cross_hierarchy_constraint_graph.cpp \\
  src/core/assembly_cross_hierarchy_motion_graph.cpp \\
  src/core/assembly_constraint_graph_participation.hpp \\
  src/core/assembly_document_json.cpp \\
  src/core/project_json.cpp \\
  src/geometry/assembly_target_compatibility.cpp \\
  tests/core/assembly_generic_relationship_tests.cpp
```''',
)
replace_exact(
    "docs/development-setup.md",
    "Blocks 23–37 are implemented.",
    "Blocks 23–38 are implemented.",
)
replace_exact(
    "docs/development-setup.md",
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target compatibility selection for existing relationship types.",
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target compatibility selection for existing relationship types. Block 38 adds persistent local/Project-level Coincident, Parallel, and Perpendicular intent plus lowercase JSON spellings while keeping the three families out of current solve/motion graphs until Block 39.",
)
replace_exact(
    "docs/development-setup.md",
    '''Focused Block-36 and Block-37 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
```

The immediate next step is Block 38: generic geometric relationship Core intent and JSON. Block 37 explicit target compatibility is implemented without adding new equations or JSON fields. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
''',
    '''Focused Blocks 36–38 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
```

The immediate next step is Block 39: generic relationship equations and shared solve integration. Block 38 persistent intent/JSON is implemented without activating the new families in current graphs or equations. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
''',
)

replace_exact(
    "docs/project-goal.md",
    "- local geometric assembly constraints;",
    "- local geometric assembly constraints, including persistent Coincident/Parallel/Perpendicular intent;",
)
replace_exact(
    "docs/project-goal.md",
    "- Project-level occurrence-qualified geometric constraints;",
    "- Project-level occurrence-qualified geometric constraints, including persistent Coincident/Parallel/Perpendicular intent;",
)
replace_exact(
    "docs/project-goal.md",
    "stable producer-driven generated topology identity and read-only recovery\n```",
    "stable producer-driven generated topology identity and read-only recovery\ngenerated topology target resolution and deterministic target compatibility\npersistent local/Project-level Coincident, Parallel, and Perpendicular intent + JSON\n```",
)
replace_exact(
    "docs/project-goal.md",
    "Block 36 resolves the supported Block-35 semantic producers into Cylinder/Axis, Line, Circle/Axis/center Point, and Point capabilities, computed analytically from validated model intent for both component-local and exact rooted transform semantics. Block 37 adds deterministic relationship/target compatibility selection for existing relationship types. The next authority step is Block 38: generic geometric relationship Core intent and JSON.",
    "Block 36 resolves the supported Block-35 semantic producers into Cylinder/Axis, Line, Circle/Axis/center Point, and Point capabilities, computed analytically from validated model intent for both component-local and exact rooted transform semantics. Block 37 adds deterministic relationship/target compatibility selection for existing relationship types. Block 38 adds persistent local and Project-level Coincident/Parallel/Perpendicular intent plus lowercase JSON spellings while deliberately deferring equations and graph participation. The next authority step is Block 39: generic relationship equations and shared solve integration.",
)
replace_exact(
    "docs/project-goal.md",
    "- generic relationship families before target capability compatibility exists;",
    "- generic relationship equations before persistent generic relationship intent and target capability compatibility exist;",
)
replace_exact(
    "docs/project-goal.md",
    "`docs/assembly-generated-topology-reference-mvp5.md` is canonical for Block-35 producer-driven generated topology identity and recovery.\n\n`docs/assembly-general-geometric-target-roadmap.md` is canonical for implemented Blocks 31–37 and planned Blocks 38–47.",
    "`docs/assembly-generated-topology-reference-mvp5.md` is canonical for Block-35 producer-driven generated topology identity and recovery.\n\n`docs/assembly-generic-relationship-intent-mvp5.md` is canonical for Block-38 persistent Coincident/Parallel/Perpendicular intent and JSON.\n\n`docs/assembly-general-geometric-target-roadmap.md` is canonical for implemented Blocks 31–38 and planned Blocks 39–47.",
)

replace_exact(
    "docs/part-construction-sequence-mvp6.md",
    "Current repository work remains on Block 38 of the assembly target/relationship/joint sequence. Blocks 38-47 remain mandatory before this sequence starts.",
    "Current repository work remains on Block 39 of the assembly target/relationship/joint sequence. Blocks 39-47 remain mandatory before this sequence starts.",
)

# Remove the temporary patch machinery from the final repository state.
(ROOT / "scripts/apply_block38.py").unlink()
workflow = ROOT / ".github/workflows/block38-patch.yml"
if workflow.exists():
    workflow.unlink()

print("Block 38 patch applied successfully")
