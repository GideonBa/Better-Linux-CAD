#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>

using namespace blcad;

namespace {

SubassemblyInstance make_instance(
    const char* id, const char* child,
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active,
    RigidTransform transform = identity_rigid_transform()) {
  auto instance = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child),
                                              visibility, suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

AssemblyDocument make_assembly(const char* id) {
  auto assembly = AssemblyDocument::create(DocumentId(id), id);
  REQUIRE(assembly);
  return assembly.value();
}

} // namespace

TEST_CASE("SubassemblyInstance validates rigid occurrence intent",
          "[core][subassembly-instance]") {
  CHECK(SubassemblyInstance::create(SubassemblyInstanceId(""), "Child", DocumentId("assembly.child"))
            .has_error());
  CHECK(SubassemblyInstance::create(SubassemblyInstanceId("subassembly.child"), "",
                                    DocumentId("assembly.child"))
            .has_error());
  CHECK(SubassemblyInstance::create(SubassemblyInstanceId("subassembly.child"), "Child",
                                    DocumentId(""))
            .has_error());

  RigidTransform invalid;
  invalid.translation_mm.x = std::numeric_limits<double>::infinity();
  CHECK(SubassemblyInstance::create(SubassemblyInstanceId("subassembly.child"), "Child",
                                    DocumentId("assembly.child"), ComponentVisibility::Visible,
                                    ComponentSuppressionState::Active, invalid)
            .has_error());

  const RigidTransform transform{Vector3{1.0, 2.0, 3.0}, Vector3{4.0, 5.0, 6.0}};
  const auto instance = make_instance("subassembly.child", "assembly.child",
                                      ComponentVisibility::Hidden,
                                      ComponentSuppressionState::Suppressed, transform);
  CHECK(instance.id().value() == "subassembly.child");
  CHECK(instance.name() == "subassembly.child");
  CHECK(instance.referenced_assembly_document().value() == "assembly.child");
  CHECK(instance.visibility() == ComponentVisibility::Hidden);
  CHECK(instance.suppression_state() == ComponentSuppressionState::Suppressed);
  CHECK(instance.transform() == transform);
}

TEST_CASE("AssemblyDocument stores and explicitly edits rigid subassembly occurrences",
          "[core][subassembly-instance]") {
  AssemblyDocument assembly = make_assembly("assembly.root");
  REQUIRE(assembly.add_subassembly_instance(
      make_instance("subassembly.child", "assembly.child")));
  CHECK(assembly.subassembly_instance_count() == 1U);
  CHECK(assembly.add_subassembly_instance(
                    make_instance("subassembly.child", "assembly.other"))
            .has_error());
  CHECK(assembly.add_subassembly_instance(
                    make_instance("subassembly.self", "assembly.root"))
            .has_error());

  const RigidTransform transform{Vector3{10.0, 20.0, 30.0}, Vector3{0.0, 0.0, 90.0}};
  REQUIRE(assembly.set_subassembly_instance_transform(SubassemblyInstanceId("subassembly.child"),
                                                      transform));
  REQUIRE(assembly.set_subassembly_instance_visibility(SubassemblyInstanceId("subassembly.child"),
                                                       ComponentVisibility::Hidden));
  REQUIRE(assembly.set_subassembly_instance_suppression_state(
      SubassemblyInstanceId("subassembly.child"), ComponentSuppressionState::Suppressed));

  const SubassemblyInstance* stored =
      assembly.find_subassembly_instance(SubassemblyInstanceId("subassembly.child"));
  REQUIRE(stored != nullptr);
  CHECK(stored->transform() == transform);
  CHECK(stored->visibility() == ComponentVisibility::Hidden);
  CHECK(stored->suppression_state() == ComponentSuppressionState::Suppressed);
}

TEST_CASE("Rigid subassembly occurrences survive additive assembly JSON roundtrip",
          "[core][subassembly-instance]") {
  AssemblyDocument assembly = make_assembly("assembly.root");
  const RigidTransform transform{Vector3{5.0, -2.0, 8.0}, Vector3{10.0, 20.0, 30.0}};
  REQUIRE(assembly.add_subassembly_instance(make_instance(
      "subassembly.child", "assembly.child", ComponentVisibility::Hidden,
      ComponentSuppressionState::Suppressed, transform)));

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("subassembly_instances") != std::string::npos);
  CHECK(serialized.value().find("referenced_assembly_document") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().subassembly_instance_count() == 1U);
  const SubassemblyInstance* instance =
      restored.value().find_subassembly_instance(SubassemblyInstanceId("subassembly.child"));
  REQUIRE(instance != nullptr);
  CHECK(instance->referenced_assembly_document().value() == "assembly.child");
  CHECK(instance->visibility() == ComponentVisibility::Hidden);
  CHECK(instance->suppression_state() == ComponentSuppressionState::Suppressed);
  CHECK(instance->transform() == transform);
}

TEST_CASE("Assembly JSON remains backward compatible without subassembly occurrences",
          "[core][subassembly-instance]") {
  const std::string content = R"({
    "schema": "blcad.assembly_document.mvp4",
    "version": 1,
    "document": {"id": "assembly.old", "name": "OldAssembly"},
    "parameters": [],
    "member_parts": [],
    "parameter_bindings": [],
    "component_instances": [],
    "assembly_constraints": [],
    "assembly_joints": []
  })";

  const auto restored = deserialize_assembly_document_from_json(content);
  REQUIRE(restored);
  CHECK(restored.value().subassembly_instance_count() == 0U);
}

TEST_CASE("Project JSON preserves explicit root and project-owned child assemblies",
          "[core][subassembly-instance]") {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(make_instance("subassembly.child", "assembly.child")));
  AssemblyDocument child = make_assembly("assembly.child");

  auto project = Project::create(DocumentId("project.hierarchy"), "HierarchyProject", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().validate_assembly_structure());

  const auto serialized = serialize_project_to_json(project.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"assemblies\"") != std::string::npos);

  const auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().assembly().id().value() == "assembly.root");
  CHECK(restored.value().child_assembly_document_count() == 1U);
  REQUIRE(restored.value().find_assembly_document(DocumentId("assembly.child")) != nullptr);
  REQUIRE(restored.value().validate_assembly_structure());
}

TEST_CASE("Project JSON remains backward compatible without child assembly collection",
          "[core][subassembly-instance]") {
  const std::string content = R"({
    "schema": "blcad.project.mvp4",
    "version": 1,
    "project": {"id": "project.old", "name": "OldProject"},
    "assembly": {
      "schema": "blcad.assembly_document.mvp4",
      "version": 1,
      "document": {"id": "assembly.root", "name": "Root"},
      "parameters": [],
      "member_parts": [],
      "parameter_bindings": [],
      "component_instances": [],
      "assembly_constraints": [],
      "assembly_joints": []
    },
    "parts": []
  })";

  const auto restored = deserialize_project_from_json(content);
  REQUIRE(restored);
  CHECK(restored.value().child_assembly_document_count() == 0U);
}
