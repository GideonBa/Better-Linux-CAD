#include "blcad/core/assembly_exchange_graph.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

PartDocument part(const char* id) {
  auto value = PartDocument::create(DocumentId(id), id);
  REQUIRE(value);
  return value.value();
}

ComponentInstance component(
    const char* id, const char* part_id,
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active,
    RigidTransform transform = identity_rigid_transform()) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId(part_id), visibility, suppression,
      ComponentGroundingState::Free, transform);
  REQUIRE(value);
  return value.value();
}

SubassemblyInstance occurrence(
    const char* id, const char* child_id,
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active,
    RigidTransform transform = identity_rigid_transform()) {
  auto value = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child_id),
                                           visibility, suppression, transform);
  REQUIRE(value);
  return value.value();
}

Project repeated_root_part_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(component(
      "component.b", "part.shared", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{20.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));
  REQUIRE(root.value().add_component_instance(component(
      "component.a", "part.shared", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{-20.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));

  auto project = Project::create(DocumentId("project.exchange.root"), "ExchangeRoot", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(part("part.shared")));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Project repeated_nested_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.right", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.left", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{-100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(component(
      "component.child", "part.shared", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{4.0, -6.0, 0.0}, Vector3{0.0, 0.0, 10.0}})));
  REQUIRE(child.value().add_subassembly_instance(occurrence(
      "subassembly.inner", "assembly.grandchild", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{0.0, 30.0, 0.0}, Vector3{0.0, 20.0, 0.0}})));

  auto grandchild = AssemblyDocument::create(DocumentId("assembly.grandchild"), "Grandchild");
  REQUIRE(grandchild);
  REQUIRE(grandchild.value().add_member_part(DocumentId("part.other")));
  REQUIRE(grandchild.value().add_component_instance(component(
      "component.grand", "part.other", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{0.0, 0.0, 5.0}, Vector3{30.0, 0.0, 0.0}})));

  auto project = Project::create(DocumentId("project.exchange.nested"), "ExchangeNested",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_child_assembly_document(grandchild.value()));
  REQUIRE(project.value().add_part_document(part("part.shared")));
  REQUIRE(project.value().add_part_document(part("part.other")));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("Assembly exchange graph freezes rooted component and shared part definition identities",
          "[core][assembly-exchange-graph]") {
  Project project = repeated_root_part_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  auto graph = AssemblyExchangeGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().assembly_occurrence_count() == 1U);
  REQUIRE(graph.value().component_occurrence_count() == 2U);
  REQUIRE(graph.value().part_product_definition_count() == 1U);

  const auto& root = graph.value().assembly_occurrences().front();
  CHECK(root.identity.occurrence_path.empty());
  CHECK_FALSE(root.parent_identity.has_value());
  CHECK_FALSE(root.via_subassembly_instance.has_value());
  CHECK(root.transform_from_parent == identity_rigid_transform());
  CHECK(root.product_name == "blcad:assembly-occurrence:root");

  const auto& definitions = graph.value().part_product_definitions();
  CHECK(definitions.front().identity.part_document == DocumentId("part.shared"));
  CHECK(definitions.front().product_name == "blcad:part-definition:part.shared");

  const auto& components = graph.value().component_occurrences();
  CHECK(components[0].identity.component_instance == ComponentInstanceId("component.a"));
  CHECK(components[1].identity.component_instance == ComponentInstanceId("component.b"));
  CHECK(components[0].part_product == components[1].part_product);
  CHECK(components[0].identity.containing_assembly_occurrence.occurrence_path.empty());
  REQUIRE(components[0].transforms_inner_to_outer.size() == 1U);
  CHECK(components[0].transforms_inner_to_outer.front().translation_mm.x == -20.0);
  CHECK(components[0].occurrence_name ==
        "blcad:component-occurrence:root/component.a");

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Assembly exchange graph keeps repeated child and nested occurrence paths distinct",
          "[core][assembly-exchange-graph]") {
  Project project = repeated_nested_project();
  auto graph = AssemblyExchangeGraph::build(project);
  REQUIRE(graph);

  REQUIRE(graph.value().assembly_occurrence_count() == 5U);
  const auto& assemblies = graph.value().assembly_occurrences();
  CHECK(assemblies[0].identity.occurrence_path.empty());
  CHECK(assemblies[1].identity.occurrence_path ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.left")});
  CHECK(assemblies[2].identity.occurrence_path ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.left"),
                                           SubassemblyInstanceId("subassembly.inner")});
  CHECK(assemblies[3].identity.occurrence_path ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.right")});
  CHECK(assemblies[4].identity.occurrence_path ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.right"),
                                           SubassemblyInstanceId("subassembly.inner")});
  CHECK(assemblies[1].assembly_document == DocumentId("assembly.child"));
  CHECK(assemblies[3].assembly_document == DocumentId("assembly.child"));
  REQUIRE(assemblies[2].parent_identity.has_value());
  CHECK(assemblies[2].parent_identity->occurrence_path == assemblies[1].identity.occurrence_path);
  CHECK(assemblies[2].transform_from_parent.translation_mm.y == 30.0);

  REQUIRE(graph.value().component_occurrence_count() == 4U);
  REQUIRE(graph.value().part_product_definition_count() == 2U);
  const auto& definitions = graph.value().part_product_definitions();
  CHECK(definitions[0].identity.part_document == DocumentId("part.other"));
  CHECK(definitions[1].identity.part_document == DocumentId("part.shared"));

  const auto& components = graph.value().component_occurrences();
  CHECK(components[0].identity.containing_assembly_occurrence.occurrence_path ==
        assemblies[1].identity.occurrence_path);
  CHECK(components[1].identity.containing_assembly_occurrence.occurrence_path ==
        assemblies[2].identity.occurrence_path);
  CHECK(components[2].identity.containing_assembly_occurrence.occurrence_path ==
        assemblies[3].identity.occurrence_path);
  CHECK(components[3].identity.containing_assembly_occurrence.occurrence_path ==
        assemblies[4].identity.occurrence_path);
  CHECK(components[0].part_product.part_document == DocumentId("part.shared"));
  CHECK(components[2].part_product == components[0].part_product);
  CHECK(components[1].part_product.part_document == DocumentId("part.other"));
  CHECK(components[3].part_product == components[1].part_product);

  REQUIRE(components[3].transforms_inner_to_outer.size() == 3U);
  CHECK(components[3].transforms_inner_to_outer[0].translation_mm.z == 5.0);
  CHECK(components[3].transforms_inner_to_outer[1].translation_mm.y == 30.0);
  CHECK(components[3].transforms_inner_to_outer[2].translation_mm.x == 100.0);
}

TEST_CASE("Assembly exchange graph mirrors visible-active leaf export filtering",
          "[core][assembly-exchange-graph]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(component("component.visible", "part.shared")));
  REQUIRE(root.value().add_component_instance(component(
      "component.hidden", "part.shared", ComponentVisibility::Hidden)));
  REQUIRE(root.value().add_component_instance(component(
      "component.suppressed", "part.shared", ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.hidden", "assembly.child", ComponentVisibility::Hidden)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.suppressed", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.visible", "assembly.child")));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(component("component.child", "part.shared")));

  auto project = Project::create(DocumentId("project.exchange.filter"), "ExchangeFilter",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(part("part.shared")));

  auto graph = AssemblyExchangeGraph::build(project.value());
  REQUIRE(graph);
  REQUIRE(graph.value().assembly_occurrence_count() == 2U);
  CHECK(graph.value().assembly_occurrences()[1].identity.occurrence_path ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("subassembly.visible")});
  REQUIRE(graph.value().component_occurrence_count() == 2U);
  CHECK(graph.value().component_occurrences()[0].identity.component_instance ==
        ComponentInstanceId("component.visible"));
  CHECK(graph.value().component_occurrences()[1].identity.component_instance ==
        ComponentInstanceId("component.child"));
}
