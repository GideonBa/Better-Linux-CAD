#include "blcad/core/assembly_leaf_occurrence.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

AssemblyDocument make_assembly(const char* id) {
  auto assembly = AssemblyDocument::create(DocumentId(id), id);
  REQUIRE(assembly);
  return assembly.value();
}

PartDocument make_part() {
  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);
  return part.value();
}

SubassemblyInstance make_subassembly(
    const char* id, RigidTransform transform = identity_rigid_transform(),
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(SubassemblyInstanceId(id), id,
                                              DocumentId("assembly.child"), visibility,
                                              suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

ComponentInstance make_component(
    const char* id, RigidTransform transform = identity_rigid_transform(),
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto component = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.shared"),
                                             visibility, suppression,
                                             ComponentGroundingState::Free, transform);
  REQUIRE(component);
  return component.value();
}

Project make_project(bool reverse_insertion) {
  AssemblyDocument root = make_assembly("assembly.root");
  const auto a = make_subassembly(
      "subassembly.a",
      RigidTransform{Vector3{10.0, 0.0, 0.0}, Vector3{0.0, 0.0, 10.0}});
  const auto z = make_subassembly(
      "subassembly.z",
      RigidTransform{Vector3{20.0, 0.0, 0.0}, Vector3{0.0, 0.0, 20.0}});
  const auto hidden = make_subassembly(
      "subassembly.hidden", RigidTransform{Vector3{30.0, 0.0, 0.0}, Vector3{}},
      ComponentVisibility::Hidden);
  const auto suppressed = make_subassembly(
      "subassembly.suppressed", RigidTransform{Vector3{40.0, 0.0, 0.0}, Vector3{}},
      ComponentVisibility::Visible, ComponentSuppressionState::Suppressed);

  if (reverse_insertion) {
    REQUIRE(root.add_subassembly_instance(z));
    REQUIRE(root.add_subassembly_instance(suppressed));
    REQUIRE(root.add_subassembly_instance(hidden));
    REQUIRE(root.add_subassembly_instance(a));
  } else {
    REQUIRE(root.add_subassembly_instance(a));
    REQUIRE(root.add_subassembly_instance(hidden));
    REQUIRE(root.add_subassembly_instance(suppressed));
    REQUIRE(root.add_subassembly_instance(z));
  }

  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_member_part(DocumentId("part.shared")));
  REQUIRE(child.add_component_instance(make_component(
      "component.z", RigidTransform{Vector3{0.0, 2.0, 0.0}, Vector3{}})));
  REQUIRE(child.add_component_instance(make_component(
      "component.hidden", identity_rigid_transform(), ComponentVisibility::Hidden)));
  REQUIRE(child.add_component_instance(make_component(
      "component.suppressed", identity_rigid_transform(), ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));
  REQUIRE(child.add_component_instance(make_component(
      "component.a", RigidTransform{Vector3{0.0, 1.0, 0.0}, Vector3{}})));

  auto project = Project::create(DocumentId("project.leaves"), "Leaves", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().add_part_document(make_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("Assembly leaf occurrence resolver is deterministic and preserves repeated paths",
          "[core][assembly-leaf-occurrence]") {
  const auto forward = AssemblyLeafOccurrenceResolver{}.resolve(make_project(false));
  const auto reverse = AssemblyLeafOccurrenceResolver{}.resolve(make_project(true));
  REQUIRE(forward);
  REQUIRE(reverse);
  CHECK(forward.value() == reverse.value());

  const auto& leaves = forward.value();
  REQUIRE(leaves.size() == 4U);

  CHECK(leaves[0].assembly_document.value() == "assembly.child");
  REQUIRE(leaves[0].subassembly_path.size() == 1U);
  CHECK(leaves[0].subassembly_path[0].value() == "subassembly.a");
  CHECK(leaves[0].component_instance.value() == "component.a");
  CHECK(leaves[0].referenced_part_document.value() == "part.shared");
  REQUIRE(leaves[0].transforms_inner_to_outer.size() == 2U);
  CHECK(leaves[0].transforms_inner_to_outer[0].translation_mm.y == 1.0);
  CHECK(leaves[0].transforms_inner_to_outer[1].translation_mm.x == 10.0);

  CHECK(leaves[1].subassembly_path[0].value() == "subassembly.a");
  CHECK(leaves[1].component_instance.value() == "component.z");
  CHECK(leaves[1].transforms_inner_to_outer[0].translation_mm.y == 2.0);
  CHECK(leaves[1].transforms_inner_to_outer[1].translation_mm.x == 10.0);

  CHECK(leaves[2].subassembly_path[0].value() == "subassembly.z");
  CHECK(leaves[2].component_instance.value() == "component.a");
  CHECK(leaves[2].transforms_inner_to_outer[1].translation_mm.x == 20.0);

  CHECK(leaves[3].subassembly_path[0].value() == "subassembly.z");
  CHECK(leaves[3].component_instance.value() == "component.z");
  CHECK(leaves[3].transforms_inner_to_outer[1].translation_mm.x == 20.0);
}

TEST_CASE("Assembly leaf occurrence resolver filters root and nested hidden or suppressed leaves",
          "[core][assembly-leaf-occurrence]") {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_member_part(DocumentId("part.shared")));
  REQUIRE(root.add_component_instance(make_component("component.root.visible")));
  REQUIRE(root.add_component_instance(make_component(
      "component.root.hidden", identity_rigid_transform(), ComponentVisibility::Hidden)));
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.hidden", identity_rigid_transform(), ComponentVisibility::Hidden)));
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.suppressed", identity_rigid_transform(), ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));

  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_member_part(DocumentId("part.shared")));
  REQUIRE(child.add_component_instance(make_component("component.child.visible")));

  auto project = Project::create(DocumentId("project.filtered_leaves"), "FilteredLeaves", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().add_part_document(make_part()));

  const auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(project.value());
  REQUIRE(leaves);
  REQUIRE(leaves.value().size() == 1U);
  CHECK(leaves.value().front().assembly_document.value() == "assembly.root");
  CHECK(leaves.value().front().subassembly_path.empty());
  CHECK(leaves.value().front().component_instance.value() == "component.root.visible");
  REQUIRE(leaves.value().front().transforms_inner_to_outer.size() == 1U);
}
