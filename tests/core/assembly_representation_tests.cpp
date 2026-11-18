#include "../support/adaptive_project_fixture.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project_json.hpp"
#include <nlohmann/json.hpp>

using namespace blcad;
using namespace blcad::test;
TEST_CASE("Named positions retain Master and reject relabeling an evaluated "
          "projection",
          "[parity-b198][assembly-representation]") {
  auto project = adaptive_project();
  const auto master = serialize_project_to_json(project);
  REQUIRE(master);
  auto position = capture_assembly_position(project, AssemblyPositionId("extended"), "Extended");
  REQUIRE(position);
  position.value().components[0].transform.translation_mm.z = 25;
  REQUIRE(project.replace_assembly_positions({position.value()}));
  const auto authored = serialize_project_to_json(project);
  REQUIRE(authored);
  auto evaluated = project.evaluate_assembly_position(position.value().id);
  REQUIRE(evaluated);
  CHECK(evaluated.value().effective_position() == position.value().id);
  CHECK(evaluated.value()
            .assembly()
            .find_component_instance(position.value().components[0].component)
            ->transform()
            .translation_mm.z == 25);
  CHECK(serialize_project_to_json(project).value() == authored.value());
  CHECK_FALSE(evaluated.value().evaluate_assembly_position(master_position_id()));
  CHECK(evaluated.value().evaluate_assembly_position(position.value().id));
  auto json = nlohmann::json::parse(authored.value());
  CHECK(json.at("version") == 6);
  CHECK_FALSE(json.contains("active_position"));
  CHECK_FALSE(json.contains("effective_position"));
  auto reopened = deserialize_project_from_json(authored.value());
  REQUIRE(reopened);
  CHECK(reopened.value().effective_position() == master_position_id());
  CHECK(serialize_project_to_json(reopened.value()).value() == authored.value());
  REQUIRE(project.replace_assembly_positions({}));
  CHECK(serialize_project_to_json(project).value() == master.value());
}
TEST_CASE("Design views change rooted display and retain every physical active leaf",
          "[parity-b198][assembly-representation]") {
  auto project = adaptive_project();
  auto view =
      capture_assembly_design_view(project, AssemblyDesignViewId("cover-only"), "Cover only");
  REQUIRE(view);
  for (auto& value : view.value().components)
    value.visible = value.occurrence.part == DocumentId("cover");
  REQUIRE(project.replace_assembly_design_views({view.value()}));
  auto evaluated = project.evaluate_assembly_design_view(view.value().id);
  REQUIRE(evaluated);
  auto visible = AssemblyLeafOccurrenceResolver{}.resolve(evaluated.value());
  REQUIRE(visible);
  CHECK(visible.value().size() == 1);
  auto physical = AssemblyLeafOccurrenceResolver{}.resolve(
      evaluated.value(), AssemblyLeafParticipation::PhysicalActive);
  REQUIRE(physical);
  CHECK(physical.value().size() == 4);
  CHECK(project.effective_design_view() == default_design_view_id());
  CHECK(evaluated.value().effective_design_view() == view.value().id);
  auto restored = evaluated.value().evaluate_assembly_design_view(default_design_view_id());
  REQUIRE(restored);
  CHECK(AssemblyLeafOccurrenceResolver{}.resolve(restored.value()).value().size() == 4);
  const auto saved = serialize_project_to_json(project);
  REQUIRE(saved);
  const auto original = nlohmann::json::parse(saved.value());
  for (int mutation = 0; mutation < 5; ++mutation) {
    auto json = original;
    if (mutation == 0)
      json["version"] = 5;
    if (mutation == 1)
      json["representations"]["design_views"][0]["version"] = 2;
    if (mutation == 2)
      json["representations"]["design_views"][0]["components"][0]["visible"] = 1;
    if (mutation == 3)
      json["representations"]["design_views"][0]["components"][0]["manufacturing_state"] = "wrong";
    if (mutation == 4)
      json["representations"]["design_views"][0]["components"][0]["occurrence"]["component"] =
          "missing";
    CAPTURE(mutation);
    CHECK_FALSE(deserialize_project_from_json(json.dump()));
  }
}

TEST_CASE("Representation catalog edits require an authored Primary Master "
          "Default context",
          "[parity-b198][assembly-representation]") {
  auto project = adaptive_project();
  auto position = capture_assembly_position(project, AssemblyPositionId("moved"), "Moved");
  REQUIRE(position);
  position.value().components.front().transform.translation_mm.z += 25;
  REQUIRE(project.replace_assembly_positions({position.value()}));
  auto view = capture_assembly_design_view(project, AssemblyDesignViewId("display"), "Display");
  REQUIRE(view);
  REQUIRE(project.replace_assembly_design_views({view.value()}));
  ModelState state{ModelStateId("manufacturing"), "Manufacturing", {}, {}, {}, {}, {}};
  REQUIRE(project.assembly().replace_model_states({state}));
  auto state_project = project;
  auto state_assembly = project.assembly().evaluate_model_state(state.id);
  REQUIRE(state_assembly);
  state_project.assembly() = state_assembly.value();
  auto moved = project.evaluate_assembly_position(position.value().id);
  REQUIRE(moved);
  auto displayed = project.evaluate_assembly_design_view(view.value().id);
  REQUIRE(displayed);
  for (auto* projection : {&moved.value(), &displayed.value(), &state_project}) {
    const auto before = serialize_project_to_json(*projection);
    REQUIRE(before);
    const auto old_position = projection->effective_position();
    const auto old_view = projection->effective_design_view();
    const auto old_state = projection->assembly().effective_model_state();
    CHECK_FALSE(projection->replace_assembly_positions({}));
    CHECK_FALSE(projection->replace_assembly_design_views({}));
    CHECK(projection->effective_position() == old_position);
    CHECK(projection->effective_design_view() == old_view);
    CHECK(projection->assembly().effective_model_state() == old_state);
    CHECK(serialize_project_to_json(*projection).value() == before.value());
  }
}
