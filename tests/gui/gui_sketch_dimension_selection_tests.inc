#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/gui_selection_model.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;

TEST_CASE("Block 115 Sketch selection preserves two point roles on one entity",
          "[gui][sketch-dimensions][gui][sketch-hit-test]") {
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.roles");
  scene.curves.push_back({"sketch/sketch.roles/entity/line.a", "line.a:curve",
                          GuiSketchCurveKind::Line, {{-10.0, 0.0}, {10.0, 0.0}}, false});
  scene.points.push_back({"sketch/sketch.roles/entity/line.a", "line.a:start",
                          {-10.0, 0.0}, GuiSketchSnapKind::Endpoint});
  scene.points.push_back({"sketch/sketch.roles/entity/line.a", "line.a:end",
                          {10.0, 0.0}, GuiSketchSnapKind::Endpoint});

  auto mapping = GuiSketchPlaneMapping::create_orthographic(
      {DatumPlaneId("datum.xy"), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
       {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
      200.0, 200.0, {0.0, 0.0}, 1.0);
  REQUIRE(mapping);
  auto controller = GuiSketchInteractionController::create(
      std::move(mapping.value()), std::move(scene));
  REQUIRE(controller);

  const auto start_hits = controller.value().hits_at({90.0, 100.0});
  const auto end_hits = controller.value().hits_at({110.0, 100.0});
  REQUIRE(start_hits);
  REQUIRE(end_hits);
  REQUIRE_FALSE(start_hits.value().empty());
  REQUIRE_FALSE(end_hits.value().empty());

  GuiSelectionModel selections;
  REQUIRE(selections.add({GuiSelectionKind::SketchEntity,
                          start_hits.value().front().semantic_id}));
  REQUIRE(selections.add({GuiSelectionKind::SketchEntity,
                          end_hits.value().front().semantic_id}));
  REQUIRE(selections.size() == 2U);
  CHECK(selections.entries()[0].semantic_id ==
        "sketch/sketch.roles/entity/line.a");
  CHECK(selections.entries()[1].semantic_id ==
        "sketch/sketch.roles/entity/line.a");
  CHECK(selections.entries()[0].candidate_id == "line.a:start");
  CHECK(selections.entries()[1].candidate_id == "line.a:end");
  CHECK_FALSE(selections.add({GuiSelectionKind::SketchEntity,
                              start_hits.value().front().semantic_id}));
}
