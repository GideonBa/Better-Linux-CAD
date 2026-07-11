#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

AssemblyDocument make_empty_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.instances"), "InstanceAssembly");
  REQUIRE(assembly);
  return assembly.value();
}

PartDocument make_empty_part(const char* id, const char* name) {
  auto part = PartDocument::create(DocumentId(id), name);
  REQUIRE(part);
  return part.value();
}

ComponentInstance make_instance(const char* id,
                                const char* name,
                                const char* part,
                                ComponentGroundingState grounding = ComponentGroundingState::Free) {
  RigidTransform transform;
  transform.translation_mm = Vector3{10.0, 20.0, 30.0};
  transform.rotation_deg = Vector3{0.0, 0.0, 45.0};
  auto instance = ComponentInstance::create(ComponentInstanceId(id), name, DocumentId(part),
                                            ComponentVisibility::Visible,
                                            ComponentSuppressionState::Active, grounding,
                                            transform);
  REQUIRE(instance);
  return instance.value();
}

} // namespace

TEST_CASE("ComponentInstance validates required identity fields", "[core][component-instance]") {
  CHECK(ComponentInstance::create(ComponentInstanceId(""), "Base", DocumentId("part.base"))
            .has_error());
  CHECK(ComponentInstance::create(ComponentInstanceId("component.base"), "", DocumentId("part.base"))
            .has_error());
  CHECK(ComponentInstance::create(ComponentInstanceId("component.base"), "Base", DocumentId(""))
            .has_error());

  const auto instance = make_instance("component.base", "BasePlate", "part.base");
  CHECK(instance.id().value() == "component.base");
  CHECK(instance.name() == "BasePlate");
  CHECK(instance.referenced_part_document().value() == "part.base");
  CHECK(instance.transform().translation_mm == Vector3{10.0, 20.0, 30.0});
  CHECK(instance.transform().rotation_deg == Vector3{0.0, 0.0, 45.0});
}

TEST_CASE("AssemblyDocument owns component instances that reference member parts",
          "[core][component-instance]") {
  auto assembly = make_empty_assembly();
  CHECK(assembly.add_component_instance(make_instance("component.missing", "Missing", "part.missing"))
            .has_error());

  REQUIRE(assembly.add_member_part(DocumentId("part.base")));
  REQUIRE(assembly.add_component_instance(make_instance("component.base", "BasePlate", "part.base",
                                                        ComponentGroundingState::Grounded)));
  CHECK(assembly.component_instance_count() == 1U);

  const ComponentInstance* instance = assembly.find_component_instance(ComponentInstanceId("component.base"));
  REQUIRE(instance != nullptr);
  CHECK(instance->grounding_state() == ComponentGroundingState::Grounded);
  CHECK(instance->referenced_part_document().value() == "part.base");

  CHECK(assembly.add_component_instance(make_instance("component.base", "Duplicate", "part.base"))
            .has_error());
}

TEST_CASE("AssemblyDocument JSON roundtrip preserves component instances",
          "[core][component-instance]") {
  auto assembly = make_empty_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.base")));
  REQUIRE(assembly.add_component_instance(make_instance("component.base", "BasePlate", "part.base",
                                                        ComponentGroundingState::Grounded)));

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("component_instances") != std::string::npos);
  CHECK(serialized.value().find("component.base") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().component_instance_count() == 1U);
  const ComponentInstance* instance =
      restored.value().find_component_instance(ComponentInstanceId("component.base"));
  REQUIRE(instance != nullptr);
  CHECK(instance->name() == "BasePlate");
  CHECK(instance->visibility() == ComponentVisibility::Visible);
  CHECK(instance->suppression_state() == ComponentSuppressionState::Active);
  CHECK(instance->grounding_state() == ComponentGroundingState::Grounded);
  CHECK(instance->transform().translation_mm == Vector3{10.0, 20.0, 30.0});
}

TEST_CASE("Project validates component instances against owned member parts",
          "[core][component-instance]") {
  auto assembly = make_empty_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.base")));
  REQUIRE(assembly.add_component_instance(make_instance("component.base", "BasePlate", "part.base")));

  auto project = Project::create(DocumentId("project.instances"), "InstanceProject", std::move(assembly));
  REQUIRE(project);
  CHECK(project.value().validate_component_instances().has_error());

  REQUIRE(project.value().add_part_document(make_empty_part("part.base", "BasePlate")));
  CHECK(project.value().validate_component_instances());
  CHECK(project.value().validate_assembly_structure());
}

TEST_CASE("Project allows multiple component instances to reference one owned part document",
          "[core][component-instance]") {
  auto assembly = make_empty_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.bolt")));
  REQUIRE(assembly.add_component_instance(make_instance("component.bolt.1", "Bolt 1", "part.bolt")));
  REQUIRE(assembly.add_component_instance(make_instance("component.bolt.2", "Bolt 2", "part.bolt")));

  auto project = Project::create(DocumentId("project.fasteners"), "FastenerProject", std::move(assembly));
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_empty_part("part.bolt", "Bolt")));

  CHECK(project.value().part_document_count() == 1U);
  CHECK(project.value().assembly().component_instance_count() == 2U);
  CHECK(project.value().validate_assembly_structure());

  const auto serialized = serialize_project_to_json(project.value());
  REQUIRE(serialized);
  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().part_document_count() == 1U);
  CHECK(restored.value().assembly().component_instance_count() == 2U);
  CHECK(restored.value().validate_assembly_structure());
}
