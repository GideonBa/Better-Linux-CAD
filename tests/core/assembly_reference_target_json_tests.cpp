#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;

namespace {

std::string reference_spelling(const AssemblyReferenceTargetIdentity& identity) {
  auto spelling = make_assembly_reference_target_spelling(identity);
  REQUIRE(spelling);
  return spelling.value();
}

ComponentInstance reference_instance(const char* id) {
  RigidTransform transform;
  transform.translation_mm = Vector3{1.0, 2.0, 3.0};
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

Project reference_json_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(reference_instance("component.root")));

  RigidTransform subassembly_transform;
  subassembly_transform.translation_mm = Vector3{100.0, 0.0, 0.0};
  auto subassembly = SubassemblyInstance::create(
      SubassemblyInstanceId("subassembly.child"), "subassembly.child", DocumentId("assembly.child"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, subassembly_transform);
  REQUIRE(subassembly);
  REQUIRE(root.value().add_subassembly_instance(std::move(subassembly.value())));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(reference_instance("component.child")));

  auto part = PartDocument::create(DocumentId("part.shared"), "SharedPart");
  REQUIRE(part);

  auto project = Project::create(DocumentId("project.reference.json"), "ReferenceJson",
                                 std::move(root.value()));
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(std::move(child.value())));
  REQUIRE(project.value().add_part_document(std::move(part.value())));
  return project.value();
}

} // namespace

TEST_CASE("Local constraint endpoints roundtrip reference spellings byte-for-byte",
          "[core][assembly-reference-target-json]") {
  Project project = reference_json_project();
  const std::string spelling_a = reference_spelling(DatumAxisId("datum_axis.spindle"));
  const std::string spelling_b = reference_spelling(DatumPlaneId("datum.xy"));

  auto target_a =
      AssemblyConstraintTarget::create(ComponentInstanceId("component.root"), spelling_a);
  REQUIRE(target_a);
  auto target_b =
      AssemblyConstraintTarget::create(ComponentInstanceId("component.root"), spelling_b);
  REQUIRE(target_b);
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.local.reference"), "constraint.local.reference",
      AssemblyConstraintType::Mate, std::move(target_a.value()), std::move(target_b.value()));
  REQUIRE(constraint);
  REQUIRE(project.assembly().add_constraint(std::move(constraint.value())));

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);
  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);

  const AssemblyConstraint* roundtripped = restored.value().assembly().find_constraint(
      AssemblyConstraintId("constraint.local.reference"));
  REQUIRE(roundtripped != nullptr);
  CHECK(roundtripped->target_a().semantic_reference() == spelling_a);
  CHECK(roundtripped->target_b().semantic_reference() == spelling_b);

  auto parsed =
      parse_assembly_reference_target_spelling(roundtripped->target_a().semantic_reference());
  REQUIRE(parsed);
  CHECK(std::get<DatumAxisId>(parsed.value()).value() == "datum_axis.spindle");
}

TEST_CASE("Cross-hierarchy endpoints roundtrip adversarial reference spellings byte-for-byte",
          "[core][assembly-reference-target-json]") {
  Project project = reference_json_project();
  const std::string spelling_a = reference_spelling(ConstructionLineId("a.b/c%d:e"));
  const std::string spelling_b =
      reference_spelling(ConstructionPointId("ref:construction_point:x"));

  auto endpoint_a = AssemblyHierarchyConstraintEndpoint::create(
      {SubassemblyInstanceId("subassembly.child")}, ComponentInstanceId("component.child"),
      spelling_a);
  REQUIRE(endpoint_a);
  auto endpoint_b = AssemblyHierarchyConstraintEndpoint::create(
      {}, ComponentInstanceId("component.root"), spelling_b);
  REQUIRE(endpoint_b);
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.reference"), "constraint.cross.reference",
      AssemblyConstraintType::Mate, std::move(endpoint_a.value()), std::move(endpoint_b.value()));
  REQUIRE(constraint);
  REQUIRE(project.add_cross_hierarchy_constraint(std::move(constraint.value())));
  REQUIRE(project.validate_assembly_structure());

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);
  const auto serialized_json = nlohmann::json::parse(serialized.value());
  CHECK(serialized_json.at("cross_hierarchy_constraints")
            .at(0)
            .at("target_a")
            .at("semantic_reference") == spelling_a);

  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().validate_assembly_structure());

  const AssemblyHierarchyConstraint* roundtripped =
      restored.value().find_cross_hierarchy_constraint(
          AssemblyConstraintId("constraint.cross.reference"));
  REQUIRE(roundtripped != nullptr);
  CHECK(roundtripped->target_a().semantic_reference() == spelling_a);
  CHECK(roundtripped->target_b().semantic_reference() == spelling_b);

  auto parsed_a =
      parse_assembly_reference_target_spelling(roundtripped->target_a().semantic_reference());
  REQUIRE(parsed_a);
  CHECK(std::get<ConstructionLineId>(parsed_a.value()).value() == "a.b/c%d:e");
  auto parsed_b =
      parse_assembly_reference_target_spelling(roundtripped->target_b().semantic_reference());
  REQUIRE(parsed_b);
  CHECK(std::get<ConstructionPointId>(parsed_b.value()).value() == "ref:construction_point:x");
}

TEST_CASE("Loading reference endpoint spellings resolves no geometry and keeps shapes unchanged",
          "[core][assembly-reference-target-json]") {
  Project project = reference_json_project();
  auto endpoint_a = AssemblyHierarchyConstraintEndpoint::create(
      {}, ComponentInstanceId("component.root"),
      reference_spelling(DatumAxisId("datum_axis.spindle")));
  REQUIRE(endpoint_a);
  auto endpoint_b = AssemblyHierarchyConstraintEndpoint::create(
      {SubassemblyInstanceId("subassembly.child")}, ComponentInstanceId("component.child"),
      reference_spelling(DatumPlaneId("datum.xy")));
  REQUIRE(endpoint_b);
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.reference"), "constraint.cross.reference",
      AssemblyConstraintType::Mate, std::move(endpoint_a.value()), std::move(endpoint_b.value()));
  REQUIRE(constraint);
  REQUIRE(project.add_cross_hierarchy_constraint(std::move(constraint.value())));

  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  // The referenced DatumAxis/DatumPlane do not exist in part.shared. Load and
  // structure validation must still succeed because endpoints persist identity
  // strings only and never resolve reference geometry.
  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().validate_assembly_structure());

  // Endpoint JSON shape stays the existing occurrence/component/semantic trio.
  const auto root = nlohmann::json::parse(serialized.value());
  const auto& endpoint_json = root.at("cross_hierarchy_constraints").at(0).at("target_a");
  CHECK(endpoint_json.size() == 3U);
  CHECK(endpoint_json.contains("occurrence_path"));
  CHECK(endpoint_json.contains("component_instance"));
  CHECK(endpoint_json.contains("semantic_reference"));
}
