#include "../support/adaptive_project_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include <nlohmann/json.hpp>

using namespace blcad;
using namespace blcad::test;
TEST_CASE("Directed design references retain one driver and strict necessary Project version",
          "[parity-b194][design-reference]") {
  auto project = adaptive_project();
  auto legacy = serialize_project_to_json(project);
  REQUIRE(legacy);
  REQUIRE(project.set_design_reference(adaptive_plane_reference()));
  auto json = serialize_project_to_json(project);
  REQUIRE(json);
  CHECK(nlohmann::json::parse(json.value()).at("version") == 4);
  auto reopened = deserialize_project_from_json(json.value());
  REQUIRE(reopened);
  CHECK(serialize_project_to_json(reopened.value()).value() == json.value());
  REQUIRE(project.detach_design_reference(ProjectDesignReferenceId("cover.support")));
  CHECK(serialize_project_to_json(project).value() == legacy.value());
}
TEST_CASE("Cross-Part reference cycles and competing target contexts fail atomically",
          "[parity-b194][design-reference]") {
  auto project = adaptive_project();
  REQUIRE(project.set_design_reference(adaptive_plane_reference()));
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  CHECK_FALSE(project.set_design_reference(adaptive_plane_reference("reverse", "cover", "source")));
  CHECK(serialize_project_to_json(project).value() == before.value());
  auto conflicting = ProjectDesignReference::create(
      ProjectDesignReferenceId("other"), "Other driver",
      {{}, ComponentInstanceId("source"), DocumentId("source")}, primary_model_state_id(),
      ProjectDesignSemanticSource{"base.top"},
      {{}, ComponentInstanceId("cover"), DocumentId("cover")}, ConstructionPlaneId("linked.plane"));
  REQUIRE(conflicting);
  CHECK_FALSE(project.set_design_reference(conflicting.value()));
  REQUIRE(project.set_design_reference(adaptive_plane_reference("chain", "cover", "irrelevant")));
  CHECK_FALSE(
      project.set_design_reference(adaptive_plane_reference("cycle", "irrelevant", "source")));
}
TEST_CASE("Design JSON rejects duplicate links unknown versions and accidental snapshots",
          "[parity-b194][design-reference]") {
  auto project = adaptive_project();
  REQUIRE(project.set_design_reference(adaptive_plane_reference()));
  auto source = serialize_project_to_json(project);
  REQUIRE(source);
  const auto original = nlohmann::json::parse(source.value());
  for (const auto mutation : {0, 1, 2, 3, 4, 5}) {
    auto json = original;
    if (mutation == 0)
      json["version"] = 3;
    if (mutation == 1)
      json["design_references"][0]["version"] = 2;
    if (mutation == 2)
      json["design_references"].push_back(json["design_references"][0]);
    if (mutation == 3)
      json["design_references"][0]["geometry"]["world_samples"] = {0, 0, 0};
    if (mutation == 4)
      json["design_references"][0]["source"]["part"] = "missing";
    if (mutation == 5)
      json["design_references"][0]["source_state"] = "missing";
    CAPTURE(mutation);
    CHECK_FALSE(deserialize_project_from_json(json.dump()));
  }
}
