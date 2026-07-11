#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/project.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace blcad;

namespace {

AssemblyDocument make_assembly(const char* id) {
  auto assembly = AssemblyDocument::create(DocumentId(id), id);
  REQUIRE(assembly);
  return assembly.value();
}

SubassemblyInstance make_instance(
    const char* id, const char* child, RigidTransform transform = identity_rigid_transform(),
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child),
                                              visibility, suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

Project make_nested_project(bool reverse_root_insertion = false) {
  AssemblyDocument root = make_assembly("assembly.root");
  const auto a = make_instance(
      "subassembly.a", "assembly.child",
      RigidTransform{Vector3{10.0, 0.0, 0.0}, Vector3{0.0, 0.0, 10.0}});
  const auto z = make_instance(
      "subassembly.z", "assembly.child",
      RigidTransform{Vector3{20.0, 0.0, 0.0}, Vector3{0.0, 0.0, 20.0}},
      ComponentVisibility::Hidden, ComponentSuppressionState::Active);
  if (reverse_root_insertion) {
    REQUIRE(root.add_subassembly_instance(z));
    REQUIRE(root.add_subassembly_instance(a));
  } else {
    REQUIRE(root.add_subassembly_instance(a));
    REQUIRE(root.add_subassembly_instance(z));
  }

  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_subassembly_instance(make_instance(
      "subassembly.middle", "assembly.grandchild",
      RigidTransform{Vector3{0.0, 5.0, 0.0}, Vector3{0.0, 0.0, 30.0}},
      ComponentVisibility::Visible, ComponentSuppressionState::Suppressed)));
  AssemblyDocument grandchild = make_assembly("assembly.grandchild");

  auto project = Project::create(DocumentId("project.hierarchy"), "Hierarchy", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(grandchild));
  REQUIRE(project.value().add_child_assembly_document(child));
  return project.value();
}

} // namespace

TEST_CASE("Assembly hierarchy traversal is deterministic and preserves repeated occurrences",
          "[core][assembly-hierarchy]") {
  const auto forward = AssemblyHierarchyTraversal::build(make_nested_project(false));
  const auto reverse = AssemblyHierarchyTraversal::build(make_nested_project(true));
  REQUIRE(forward);
  REQUIRE(reverse);
  CHECK(forward.value().occurrences() == reverse.value().occurrences());

  const auto& occurrences = forward.value().occurrences();
  REQUIRE(occurrences.size() == 5U);
  CHECK(occurrences[0].assembly_document.value() == "assembly.root");
  CHECK_FALSE(occurrences[0].parent_assembly_document.has_value());
  CHECK(occurrences[0].occurrence_path.empty());
  CHECK(occurrences[0].parent_transforms_inner_to_outer.empty());
  CHECK(occurrences[0].visible_path);
  CHECK(occurrences[0].active_path);

  CHECK(occurrences[1].assembly_document.value() == "assembly.child");
  REQUIRE(occurrences[1].via_subassembly_instance.has_value());
  CHECK(occurrences[1].via_subassembly_instance->value() == "subassembly.a");
  REQUIRE(occurrences[1].occurrence_path.size() == 1U);
  CHECK(occurrences[1].occurrence_path[0].value() == "subassembly.a");
  REQUIRE(occurrences[1].parent_transforms_inner_to_outer.size() == 1U);
  CHECK(occurrences[1].parent_transforms_inner_to_outer[0].translation_mm.x == 10.0);
  CHECK(occurrences[1].visible_path);
  CHECK(occurrences[1].active_path);

  CHECK(occurrences[2].assembly_document.value() == "assembly.grandchild");
  REQUIRE(occurrences[2].occurrence_path.size() == 2U);
  CHECK(occurrences[2].occurrence_path[0].value() == "subassembly.a");
  CHECK(occurrences[2].occurrence_path[1].value() == "subassembly.middle");
  REQUIRE(occurrences[2].parent_transforms_inner_to_outer.size() == 2U);
  CHECK(occurrences[2].parent_transforms_inner_to_outer[0].translation_mm.y == 5.0);
  CHECK(occurrences[2].parent_transforms_inner_to_outer[1].translation_mm.x == 10.0);
  CHECK(occurrences[2].visible_path);
  CHECK_FALSE(occurrences[2].active_path);

  CHECK(occurrences[3].assembly_document.value() == "assembly.child");
  REQUIRE(occurrences[3].via_subassembly_instance.has_value());
  CHECK(occurrences[3].via_subassembly_instance->value() == "subassembly.z");
  CHECK_FALSE(occurrences[3].visible_path);
  CHECK(occurrences[3].active_path);

  CHECK(occurrences[4].assembly_document.value() == "assembly.grandchild");
  REQUIRE(occurrences[4].parent_transforms_inner_to_outer.size() == 2U);
  CHECK(occurrences[4].parent_transforms_inner_to_outer[0].translation_mm.y == 5.0);
  CHECK(occurrences[4].parent_transforms_inner_to_outer[1].translation_mm.x == 20.0);
  CHECK_FALSE(occurrences[4].visible_path);
  CHECK_FALSE(occurrences[4].active_path);
}

TEST_CASE("Project hierarchy validation rejects missing and root assembly references",
          "[core][assembly-hierarchy]") {
  SECTION("missing child assembly") {
    AssemblyDocument root = make_assembly("assembly.root");
    REQUIRE(root.add_subassembly_instance(
        make_instance("subassembly.missing", "assembly.missing")));
    auto project = Project::create(DocumentId("project.missing"), "Missing", root);
    REQUIRE(project);
    const auto validated = project.value().validate_assembly_structure();
    REQUIRE(validated.has_error());
    CHECK(validated.error().object_id() == "subassembly.missing");
    CHECK(validated.error().message() ==
          "subassembly instance referenced assembly must resolve to a project-owned child assembly document");
  }

  SECTION("child cannot point to root assembly") {
    AssemblyDocument root = make_assembly("assembly.root");
    AssemblyDocument child = make_assembly("assembly.child");
    REQUIRE(child.add_subassembly_instance(
        make_instance("subassembly.root", "assembly.root")));
    auto project = Project::create(DocumentId("project.root_reference"), "RootReference", root);
    REQUIRE(project);
    REQUIRE(project.value().add_child_assembly_document(child));
    const auto validated = project.value().validate_assembly_structure();
    REQUIRE(validated.has_error());
    CHECK(validated.error().object_id() == "subassembly.root");
  }
}

TEST_CASE("Project hierarchy validation rejects indirect cycles in owned child assemblies",
          "[core][assembly-hierarchy]") {
  AssemblyDocument root = make_assembly("assembly.root");
  AssemblyDocument child_a = make_assembly("assembly.a");
  AssemblyDocument child_b = make_assembly("assembly.b");
  REQUIRE(child_a.add_subassembly_instance(
      make_instance("subassembly.a_to_b", "assembly.b")));
  REQUIRE(child_b.add_subassembly_instance(
      make_instance("subassembly.b_to_a", "assembly.a")));

  auto project = Project::create(DocumentId("project.cycle"), "Cycle", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child_b));
  REQUIRE(project.value().add_child_assembly_document(child_a));

  const auto validated = project.value().validate_assembly_hierarchy();
  REQUIRE(validated.has_error());
  CHECK(validated.error().object_id() == "subassembly.b_to_a");
  CHECK(validated.error().message() ==
        "assembly hierarchy must not contain direct or indirect cycles");
  CHECK(AssemblyHierarchyTraversal::build(project.value()).has_error());
}

TEST_CASE("Project child assembly identities are unique including the root assembly",
          "[core][assembly-hierarchy]") {
  AssemblyDocument root = make_assembly("assembly.root");
  auto project = Project::create(DocumentId("project.ids"), "Ids", root);
  REQUIRE(project);

  CHECK(project.value().add_child_assembly_document(make_assembly("assembly.root")).has_error());
  REQUIRE(project.value().add_child_assembly_document(make_assembly("assembly.child")));
  CHECK(project.value().add_child_assembly_document(make_assembly("assembly.child")).has_error());
}
