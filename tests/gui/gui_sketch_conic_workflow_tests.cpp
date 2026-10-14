#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <algorithm>

using namespace blcad;
using namespace blcad::gui;

namespace {
void enter(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Conics")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
}
} // namespace

TEST_CASE("Shell conic and slot tools commit one complete persistent shape",
          "[gui][sketch-create-conics]") {
  ShellWindow window;
  enter(window);
  const auto sketch = *window.active_sketch();
  auto tool = GuiSketchCreateTool::Ellipse;
  std::vector<Point2> picks;
  std::size_t lines = 0, arcs = 0, splines = 0, circles = 0, profiles = 0;
  SECTION("radius circle") {
    tool = GuiSketchCreateTool::CenterRadiusCircle;
    picks = {{0, 0}, {20, 0}};
    circles = 1;
  }
  SECTION("diameter circle") {
    tool = GuiSketchCreateTool::CenterDiameterCircle;
    picks = {{0, 0}, {20, 0}};
    circles = 1;
  }
  SECTION("two point circle") {
    tool = GuiSketchCreateTool::TwoPointCircle;
    picks = {{0, 0}, {20, 0}};
    circles = 1;
  }
  SECTION("three point circle") {
    tool = GuiSketchCreateTool::ThreePointCircle;
    picks = {{20, 0}, {0, 20}, {-20, 0}};
    circles = 1;
  }
  SECTION("center arc") {
    tool = GuiSketchCreateTool::CenterStartEndArc;
    picks = {{0, 0}, {20, 0}, {0, 20}};
    arcs = 1;
  }
  SECTION("three point arc") {
    tool = GuiSketchCreateTool::ThreePointArc;
    picks = {{20, 0}, {14, 14}, {0, 20}};
    arcs = 1;
  }
  SECTION("ellipse") {
    tool = GuiSketchCreateTool::Ellipse;
    picks = {{0, 0}, {20, 0}, {0, 10}};
    splines = 4;
    profiles = 1;
  }
  SECTION("elliptical arc") {
    tool = GuiSketchCreateTool::EllipticalArc;
    picks = {{0, 0}, {20, 0}, {0, 10}, {20, 0}, {-10, 8}};
    splines = 2;
  }
  SECTION("center slot") {
    tool = GuiSketchCreateTool::CenterSlot;
    picks = {{0, 0}, {30, 0}, {0, 5}};
    lines = 2;
    arcs = 2;
    profiles = 1;
  }
  SECTION("overall slot") {
    tool = GuiSketchCreateTool::OverallSlot;
    picks = {{0, 0}, {30, 0}, {0, 5}};
    lines = 2;
    arcs = 2;
    profiles = 1;
  }
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(tool));
  REQUIRE(tools.add_pick(picks.front()));
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  tools.cancel_tool();
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(tools.begin_tool(tool));
  for (auto pick : picks)
    REQUIRE(tools.add_pick(pick));
  tools.cancel_tool();
  const auto* shape = window.session().part_document()->find_sketch(sketch);
  REQUIRE(shape);
  CHECK(shape->line_segments().size() == lines);
  CHECK(shape->arc_segments().size() == arcs);
  CHECK(shape->spline_segments().size() == splines);
  CHECK(shape->circle_profiles().size() == circles);
  CHECK(shape->arc_closed_profiles().size() == profiles);
  for (const auto& span : shape->spline_segments()) {
    REQUIRE(span.rational());
    for (double t : {.1, .25, .47, .76, .91}) {
      const auto p = span.curve().point(t);
      CHECK(p.x * p.x / 400 + p.y * p.y / 100 == Catch::Approx(1).margin(1e-12));
    }
  }
  const auto after = serialize_part_document_to_json(*window.session().part_document()).value();
  REQUIRE(window.session().undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.session().redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("conic.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE("Degenerate conic and slot creation leaves geometry and history unchanged",
          "[gui][sketch-create-conics]") {
  ShellWindow window;
  enter(window);
  auto tool = GuiSketchCreateTool::Ellipse;
  std::vector<Point2> picks;
  SECTION("zero radius") {
    tool = GuiSketchCreateTool::CenterRadiusCircle;
    picks = {{0, 0}, {0, 0}};
  }
  SECTION("collinear arc") {
    tool = GuiSketchCreateTool::ThreePointArc;
    picks = {{0, 0}, {10, 0}, {20, 0}};
  }
  SECTION("collapsed ellipse") {
    picks = {{0, 0}, {20, 0}, {10, 0}};
  }
  SECTION("zero ellipse sweep") {
    tool = GuiSketchCreateTool::EllipticalArc;
    picks = {{0, 0}, {20, 0}, {0, 10}, {20, 0}, {20, 0}};
  }
  SECTION("oversized slot") {
    tool = GuiSketchCreateTool::OverallSlot;
    picks = {{0, 0}, {10, 0}, {0, 10}};
  }
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  const auto history = window.session().undo_label();
  REQUIRE(window.sketch_tools().begin_tool(tool));
  for (auto pick : picks)
    (void)window.sketch_tools().add_pick(pick);
  window.sketch_tools().cancel_tool();
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  CHECK(window.session().undo_label() == history);
}

TEST_CASE("Tangent tools persist reference contact and tangency atomically",
          "[gui][sketch-create-conics][sketch-tangent-create]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  bool curved = false, circle = false;
  SECTION("line to circle") {
    circle = true;
  }
  SECTION("line to arc") {}
  SECTION("arc to circle") {
    curved = true;
    circle = true;
  }
  SECTION("arc to arc") {
    curved = true;
  }
  REQUIRE(window.session().commit_part_transaction(
      "Tangent source", [id, curved](PartDocument& part) -> Result<std::size_t> {
        auto sketch = *part.find_sketch(id);
        auto added =
            curved ? sketch.add_entity(ArcSegment::create_three_point(
                                           SketchEntityId("source"), {-10, -10},
                                           {-7.0710678118654755, -2.9289321881345245}, {0, 0})
                                           .value())
                   : sketch.add_entity(
                         LineSegment::create(SketchEntityId("source"), {-20, 0}, {0, 0}).value());
        if (!added)
          return added;
        return part.update_sketch(sketch);
      }));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(circle ? GuiSketchCreateTool::TangentCircle
                                  : GuiSketchCreateTool::TangentArc));
  window.refresh();
  tools.publish_scene();
  if (curved)
    REQUIRE(tools.add_pick({0, 0}, GuiSketchSnapKind::Endpoint, SketchEntityId("source")));
  else
    REQUIRE(tools.add_pick({0, 0}, GuiSketchSnapKind::Endpoint)); // actual viewport curve hit
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(tools.add_pick({5, 5}));
  tools.cancel_tool();
  auto catalog = window.session().sketch_constraint_catalog(id);
  REQUIRE(catalog);
  REQUIRE(catalog.value().constraints().size() == 2U);
  CHECK(std::count_if(
            catalog.value().constraints().begin(), catalog.value().constraints().end(),
            [](const auto& c) { return c.kind() == SketchSolverConstraintKind::Tangent; }) == 1);
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch);
  if (circle) {
    REQUIRE(sketch->circle_profiles().size() == 1U);
    const auto& result = sketch->circle_profiles().front();
    const auto* diameter =
        window.session().part_document()->find_parameter(result.diameter_parameter());
    REQUIRE(diameter);
    CHECK(result.center().x == Catch::Approx(0).margin(1e-8));
    CHECK(result.center().y == Catch::Approx(5).margin(1e-8));
    CHECK(diameter->value().millimeters() == Catch::Approx(10).margin(1e-8));
  } else {
    REQUIRE(sketch->arc_segments().size() == (curved ? 2U : 1U));
    const auto* result = sketch->find_arc_segment(SketchEntityId("arc.create.1"));
    REQUIRE(result);
    CHECK(result->start().x == Catch::Approx(0).margin(1e-8));
    CHECK(result->end().x == Catch::Approx(5).margin(1e-8));
    CHECK(result->end().y == Catch::Approx(5).margin(1e-8));
  }
  const auto after = serialize_part_document_to_json(*window.session().part_document()).value();
  REQUIRE(window.session().undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  CHECK(window.session().sketch_constraint_catalog(id).value().constraints().empty());
  REQUIRE(window.session().redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
  CHECK(window.session().sketch_constraint_catalog(id).value().constraints().size() == 2U);
  QTemporaryDir directory;
  const auto path = directory.filePath("tangent.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(reopened.sketch_constraint_catalog(id).value() == catalog.value());
  if (circle && !curved) {
    auto drag = GuiSketchDragController::create(window.session(), id);
    REQUIRE(drag);
    std::string handle;
    for (const auto& candidate : drag.value().handles())
      if (candidate.kind == GuiSketchDragHandleKind::Endpoint && candidate.position == Point2{-20, 0})
        handle = candidate.id;
    REQUIRE_FALSE(handle.empty());
    REQUIRE(drag.value().begin(handle));
    REQUIRE(drag.value().queue_pointer({-20, 5}));
    auto preview = drag.value().process_pending();
    REQUIRE(preview);
    const auto& moved = preview.value().preview_sketch();
    const auto& line = moved.line_segments().front();
    const auto center = moved.circle_profiles().front().center();
    const Point2 radial{line.end().x-center.x, line.end().y-center.y};
    const Point2 direction{line.end().x-line.start().x, line.end().y-line.start().y};
    CHECK(std::hypot(radial.x, radial.y) == Catch::Approx(5).margin(1e-5));
    CHECK((radial.x*direction.x+radial.y*direction.y)/std::hypot(direction.x,direction.y) ==
          Catch::Approx(0).margin(1e-5));
    CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
  }

}

TEST_CASE("Tangent creation requires a real reference and a nondegenerate second pick",
          "[gui][sketch-create-conics][sketch-tangent-create]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction("Source", [id](PartDocument& part) {
    auto sketch = *part.find_sketch(id);
    auto added =
        sketch.add_entity(LineSegment::create(SketchEntityId("source"), {-10, 0}, {0, 0}).value());
    if (!added)
      return added;
    return part.update_sketch(sketch);
  }));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::TangentArc));
  CHECK_FALSE(tools.add_pick({100, 100}));
  CHECK_FALSE(tools.add_pick({-5, 0}, GuiSketchSnapKind::Nearest, SketchEntityId("source")));
  REQUIRE(tools.add_pick({0, 0}, GuiSketchSnapKind::Endpoint, SketchEntityId("source")));
  (void)tools.add_pick({10, 0}); // infinite-radius solution rejected at completion
  tools.cancel_tool();
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  CHECK(window.session().sketch_constraint_catalog(id).value().constraints().empty());
}
