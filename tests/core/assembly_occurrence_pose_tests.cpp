#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/assembly_occurrence_pose.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace {
Project fixture() {
  auto root = AssemblyDocument::create(DocumentId("root"), "Root");
  auto child = AssemblyDocument::create(DocumentId("child"), "Child");
  auto part = PartDocument::create(DocumentId("part"), "Part");
  REQUIRE(root);
  REQUIRE(child);
  REQUIRE(part);
  REQUIRE(child.value().add_member_part(part.value().id()));
  for (const char* name : {"fixed", "moving"}) {
    auto component =
        ComponentInstance::create(ComponentInstanceId(name), name, part.value().id(),
                                  ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                  std::string(name) == "fixed" ? ComponentGroundingState::Grounded
                                                               : ComponentGroundingState::Free);
    REQUIRE(component);
    REQUIRE(child.value().add_component_instance(component.value()));
  }
  for (const char* name : {"left", "right"}) {
    auto occurrence =
        SubassemblyInstance::create(SubassemblyInstanceId(name), name, child.value().id());
    REQUIRE(occurrence);
    REQUIRE(root.value().add_subassembly_instance(occurrence.value()));
  }
  auto project = Project::create(DocumentId("project"), "Project", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(part.value()));
  return project.value();
}
AssemblyOccurrencePose pose(const char* path, double extension) {
  auto value = AssemblyOccurrencePose::create(
      {SubassemblyInstanceId(path)}, DocumentId("child"), AssemblyOccurrenceRigidity::Flexible,
      {{ComponentInstanceId("moving"), {{0, 0, extension}, {}}}});
  REQUIRE(value);
  return value.value();
}
} // namespace

TEST_CASE("Occurrence poses isolate repeated child definitions and preserve legacy JSON",
          "[parity-b192][occurrence-pose]") {
  auto project = fixture();
  const auto original_child =
      serialize_assembly_document_to_json(*project.find_assembly_document(DocumentId("child")));
  REQUIRE(original_child);
  auto legacy = serialize_project_to_json(project);
  REQUIRE(legacy);
  CHECK(nlohmann::json::parse(legacy.value()).at("version") == 1);
  REQUIRE(project.set_occurrence_pose(pose("left", 10)));
  REQUIRE(project.set_occurrence_pose(pose("right", 30)));
  auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(project);
  REQUIRE(leaves);
  REQUIRE(leaves.value().size() == 4);
  CHECK(project
            .effective_component_transform({SubassemblyInstanceId("left")},
                                           ComponentInstanceId("moving"))
            .value()
            .translation_mm.z == 10);
  CHECK(project
            .effective_component_transform({SubassemblyInstanceId("right")},
                                           ComponentInstanceId("moving"))
            .value()
            .translation_mm.z == 30);
  CHECK(serialize_assembly_document_to_json(*project.find_assembly_document(DocumentId("child")))
            .value() == original_child.value());
  auto json = serialize_project_to_json(project);
  REQUIRE(json);
  CHECK(nlohmann::json::parse(json.value()).at("version") == 3);
  auto reopened = deserialize_project_from_json(json.value());
  REQUIRE(reopened);
  CHECK(serialize_project_to_json(reopened.value()).value() == json.value());
  REQUIRE(project.remove_occurrence_pose({SubassemblyInstanceId("left")}));
  CHECK(project
            .effective_component_transform({SubassemblyInstanceId("left")},
                                           ComponentInstanceId("moving"))
            .value() == identity_rigid_transform());
  REQUIRE(project.remove_occurrence_pose({SubassemblyInstanceId("right")}));
  CHECK(serialize_project_to_json(project).value() == legacy.value());
}

TEST_CASE("Occurrence pose policy rejects orphaned identities and conflicting authority",
          "[parity-b192][occurrence-pose]") {
  auto project = fixture();
  CHECK_FALSE(AssemblyOccurrencePose::create({}, DocumentId("child"),
                                             AssemblyOccurrenceRigidity::Flexible));
  CHECK_FALSE(AssemblyOccurrencePose::create(
      {SubassemblyInstanceId("left")}, DocumentId("child"), AssemblyOccurrenceRigidity::Rigid,
      {{ComponentInstanceId("moving"), identity_rigid_transform()}}));
  CHECK_FALSE(AssemblyOccurrencePose::create(
      {SubassemblyInstanceId("left")}, DocumentId("child"), AssemblyOccurrenceRigidity::Flexible,
      {{ComponentInstanceId("moving"), identity_rigid_transform()},
       {ComponentInstanceId("moving"), identity_rigid_transform()}}));
  CHECK_FALSE(AssemblyOccurrencePose::create(
      {SubassemblyInstanceId("left")}, DocumentId("child"), AssemblyOccurrenceRigidity::Flexible,
      {{ComponentInstanceId("moving"), {{std::numeric_limits<double>::quiet_NaN(), 0, 0}, {}}}}));
  CHECK_FALSE(project.set_occurrence_pose(pose("missing", 5)));
  auto wrong = AssemblyOccurrencePose::create({SubassemblyInstanceId("left")}, DocumentId("other"),
                                              AssemblyOccurrenceRigidity::Flexible);
  REQUIRE(wrong);
  CHECK_FALSE(project.set_occurrence_pose(wrong.value()));
  auto grounded = AssemblyOccurrencePose::create(
      {SubassemblyInstanceId("left")}, DocumentId("child"), AssemblyOccurrenceRigidity::Flexible,
      {{ComponentInstanceId("fixed"), {{0, 0, 4}, {}}}});
  REQUIRE(grounded);
  CHECK_FALSE(project.set_occurrence_pose(grounded.value()));
  auto missing = AssemblyOccurrencePose::create(
      {SubassemblyInstanceId("left")}, DocumentId("child"), AssemblyOccurrenceRigidity::Flexible,
      {{ComponentInstanceId("missing"), identity_rigid_transform()}});
  REQUIRE(missing);
  CHECK_FALSE(project.set_occurrence_pose(missing.value()));
  CHECK_FALSE(project.set_occurrence_component_transform(
      {SubassemblyInstanceId("left")}, ComponentInstanceId("moving"), identity_rigid_transform()));
}

TEST_CASE("Occurrence pose JSON dispatch rejects downgrade duplicates and unknown policy",
          "[parity-b192][occurrence-pose]") {
  auto project = fixture();
  REQUIRE(project.set_occurrence_pose(pose("left", 10)));
  const auto source = serialize_project_to_json(project);
  REQUIRE(source);
  const auto original = nlohmann::json::parse(source.value());
  for (const auto mutation : {0, 1, 2, 3, 4, 5, 6}) {
    auto json = original;
    if (mutation == 0)
      json["version"] = 2;
    if (mutation == 1)
      json["occurrence_poses"][0]["version"] = 2;
    if (mutation == 2)
      json["occurrence_poses"][0]["rigidity"] = "automatic";
    if (mutation == 3)
      json["occurrence_poses"].push_back(json["occurrence_poses"][0]);
    if (mutation == 4)
      json["occurrence_poses"][0]["components"][0]["translation_mm"][0] = true;
    if (mutation == 5)
      json["occurrence_poses"][0]["implicit_policy"] = true;
    if (mutation == 6)
      json["occurrence_poses"] = nlohmann::json::array();
    CAPTURE(mutation);
    CHECK_FALSE(deserialize_project_from_json(json.dump()));
  }
}
