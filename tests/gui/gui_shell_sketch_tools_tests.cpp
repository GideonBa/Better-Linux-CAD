#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"

#include "blcad/core/sketch.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {

// New part + sketch on the XY origin plane, ready for tools.
void enter_sketch(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Skizzentest")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.active_sketch().has_value());
}

[[nodiscard]] SketchTopology topology_of(ShellWindow& window) {
  const auto* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  return std::move(topology.value());
}

void select_first_entity(ShellWindow& window) {
  const auto topology = topology_of(window);
  REQUIRE_FALSE(topology.entities().empty());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities().front().id()}));
}

// Seeds two known-id line segments directly into the active sketch (bypasses the
// create tools so the modify tests address stable ids "line.h"/"line.v").
void seed_two_lines(ShellWindow& window, Point2 a0, Point2 a1, Point2 b0, Point2 b1) {
  const SketchId id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed lines", [id, a0, a1, b0, b1](PartDocument& part) -> Result<std::size_t> {
        const Sketch* source = part.find_sketch(id);
        if (source == nullptr)
          return Result<std::size_t>::failure(Error::validation(id.value(), "no sketch"));
        Sketch sketch = *source;
        if (auto added =
                sketch.add_entity(LineSegment::create(SketchEntityId("line.h"), a0, a1).value());
            added.has_error())
          return added;
        if (auto added =
                sketch.add_entity(LineSegment::create(SketchEntityId("line.v"), b0, b1).value());
            added.has_error())
          return added;
        return part.update_sketch(std::move(sketch));
      }));
  window.refresh();
}

bool near(Point2 point, Point2 expected) {
  return std::hypot(point.x - expected.x, point.y - expected.y) <= 1.0e-6;
}

// Circumradius of the arc's 3 defining points.
double arc_radius(Point2 a, Point2 b, Point2 c) {
  const double ab = std::hypot(a.x - b.x, a.y - b.y);
  const double bc = std::hypot(b.x - c.x, b.y - c.y);
  const double ca = std::hypot(c.x - a.x, c.y - a.y);
  const double area = std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5;
  return area <= 1.0e-12 ? 0.0 : (ab * bc * ca) / (4.0 * area);
}

} // namespace

TEST_CASE("Block 134 line tool commits an entity through the shell tool surface",
          "[gui][shell-sketch][gui][sketch-create]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  CHECK(tools.tool_active());
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0})); // second pick commits the segment …
  CHECK(tools.tool_active());           // … and the chain stays armed (Inventor-style)

  CHECK(topology_of(window).entities().size() == 1U);
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(window.viewport().sketch_drag_handle_count() > 0U); // drag re-armed

  // One more click continues the chain from the committed end and couples the
  // shared corner coincident.
  REQUIRE(tools.add_pick({30.0, 20.0}));
  CHECK(topology_of(window).entities().size() == 2U);
  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  CHECK(catalog.value().constraints().size() == 1U); // chained corner coincident

  tools.cancel_tool(); // Escape ends the chain
  CHECK_FALSE(tools.tool_active());
  CHECK(topology_of(window).entities().size() == 2U);
}

TEST_CASE("Block 134 constraints apply and remove end-to-end from the tool surface",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 4.0})); // slightly off-horizontal
  tools.cancel_tool();                  // end the chain

  select_first_entity(window);
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Horizontal);
  REQUIRE(applied);
  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  CHECK(catalog.value().constraints().size() == 1U);

  // The solver made the line horizontal.
  const auto topology = topology_of(window);
  const auto& entity = topology.entities().front();
  const auto* start = topology.find_point(entity.points()[0]);
  const auto* end = topology.find_point(entity.points()[1]);
  REQUIRE(start != nullptr);
  REQUIRE(end != nullptr);
  CHECK(std::abs(start->position().y - end->position().y) < 1.0e-9);

  // Incompatible request fails closed.
  select_first_entity(window);
  CHECK(tools.apply_constraint(SketchSolverConstraintKind::Coincident).has_error());

  // Removal via the selected constraint glyph.
  const auto constraint_id = catalog.value().constraints().front().id();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity,
       "sketch/" + window.active_sketch()->value() + "/constraint/" + constraint_id.value()}));
  REQUIRE(tools.remove_selected_constraint());
  auto after = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(after);
  CHECK(after.value().constraints().empty());
}

TEST_CASE("Block 134 dimensions apply reference and driving with edit",
          "[gui][shell-sketch][gui][sketch-dimensions]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({25.0, 0.0}));
  tools.cancel_tool(); // end the chain

  // Reference dimension needs no parameter.
  select_first_entity(window);
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, true));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  REQUIRE(catalog);
  CHECK(catalog.value().dimensions().size() == 1U);

  // Driving dimension binds a Length parameter and edits to a new literal.
  REQUIRE(window.session().commit_part_transaction(
      "Parameter anlegen", [](PartDocument& part) {
        auto quantity = Quantity::length_mm(25.0, "parameter.laenge");
        if (quantity.has_error())
          return Result<std::size_t>::failure(quantity.error());
        auto parameter = Parameter::create_length(ParameterId("parameter.laenge"), "laenge",
                                                  quantity.value());
        if (parameter.has_error())
          return Result<std::size_t>::failure(parameter.error());
        return part.add_parameter(std::move(parameter.value()));
      }));
  select_first_entity(window);
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, false,
                                ParameterId("parameter.laenge")));
  catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  REQUIRE(catalog);
  REQUIRE(catalog.value().dimensions().size() == 2U);

  const auto driving =
      std::find_if(catalog.value().dimensions().begin(), catalog.value().dimensions().end(),
                   [](const auto& dimension) { return dimension.driving(); });
  REQUIRE(driving != catalog.value().dimensions().end());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity,
       "sketch/" + window.active_sketch()->value() + "/dimension/" + driving->id().value()}));
  REQUIRE(tools.edit_selected_dimension("40"));
  CHECK(window.session()
            .part_document()
            ->find_parameter(ParameterId("parameter.laenge"))
            ->value()
            .millimeters() == 40.0);

  // Driving without explicit parameter auto-creates dN with the measured value
  // (Inventor-style) and does not move geometry.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({5.0, 10.0}));
  REQUIRE(tools.add_pick({20.0, 10.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities().back().id()}));
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, false));
  const auto* automatic = window.session().part_document()->find_parameter(ParameterId("d1"));
  REQUIRE(automatic != nullptr);
  CHECK(automatic->value().millimeters() == 15.0);
}

TEST_CASE("Block 134 dimensions work through the real viewport click path",
          "[gui][shell-sketch][gui][sketch-dimensions]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();

  // Click the line the way a user does: mouse press + release over its middle.
  auto& viewport = window.viewport();
  const auto screen = viewport.sketch_plane_to_screen({15.0, 0.0});
  REQUIRE(screen);
  const QPointF position(screen.value().x, screen.value().y);
  QMouseEvent press(QEvent::MouseButtonPress, position, viewport.mapToGlobal(position.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, position,
                      viewport.mapToGlobal(position.toPoint()), Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();

  INFO("selection count: " << window.session().selection().entries().size());
  REQUIRE_FALSE(window.session().selection().entries().empty());
  INFO("selected: " << window.session().selection().entries().front().semantic_id
                    << " candidate: "
                    << window.session().selection().entries().front().candidate_id);
  const auto applied = tools.apply_dimension(SketchDimensionKind::Length, false);
  if (applied.has_error())
    INFO("apply error: " << applied.error().object_id() << ": " << applied.error().message());
  REQUIRE(applied);
  const auto* automatic = window.session().part_document()->find_parameter(ParameterId("d1"));
  REQUIRE(automatic != nullptr);
  CHECK(automatic->value().millimeters() == 30.0);
}

TEST_CASE("Block 134 circle and rectangle tools commit visible scene items",
          "[gui][shell-sketch][gui][sketch-create]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({10.0, 0.0}));
  CHECK_FALSE(tools.tool_active());
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(tools.add_pick({20.0, 5.0}));
  REQUIRE(tools.add_pick({40.0, 15.0}));
  CHECK_FALSE(tools.tool_active());

  const auto* part = window.session().part_document();
  const auto* sketch = part->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->circle_profiles().size() == 1U);
  // Rectangles commit as four line entities (already visible).
  CHECK(sketch->line_segments().size() == 4U);

  // The viewport scene renders the circle profile (regression: circles used
  // to commit invisibly).
  REQUIRE(window.session().part_shape_cache() != nullptr);
  auto scene = geometry::ViewportSceneBuilder{}.build_part(
      *part, *window.session().part_shape_cache());
  REQUIRE(scene);
  const std::string prefix = "sketch/" + window.active_sketch()->value() + "/profile/";
  CHECK(std::count_if(scene.value().begin(), scene.value().end(), [&prefix](const auto& item) {
          return item.semantic_id.rfind(prefix, 0U) == 0U;
        }) == 1);
}

TEST_CASE("Block 134 sketch ribbon exposes draw constraint dimension and grid groups",
          "[gui][shell-sketch][gui][ribbon]") {
  ShellWindow window;
  enter_sketch(window);
  CHECK(window.ribbon().tab_enabled(1));
  CHECK(window.ribbon().current_tab() == 1);
  // Finish restores the model tab and clears the tool surface.
  REQUIRE(window.finish_active_sketch());
  CHECK_FALSE(window.sketch_tools().tool_active());
  CHECK(window.viewport().sketch_drag_handle_count() == 0U);
}

TEST_CASE("Ändern: fillet on two selected curves inserts a tangent arc",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0});
  auto& tools = window.sketch_tools();

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));

  const auto applied = tools.apply_fillet(2.0);
  REQUIRE(applied);
  const auto* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  CHECK(sketch->arc_segments().size() == 1U);
  const auto* trimmed = sketch->find_line_segment(SketchEntityId("line.h"));
  REQUIRE(trimmed != nullptr);
  CHECK(near(trimmed->start(), {2.0, 0.0}));
}

TEST_CASE("Ändern: chamfer on two selected curves inserts a connecting line",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0});
  auto& tools = window.sketch_tools();

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));

  const auto applied = tools.apply_chamfer(3.0);
  REQUIRE(applied);
  const auto* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 3U); // two trimmed lines + the chamfer

  // Fewer than two curves selected fails closed.
  window.session().selection().clear();
  CHECK(tools.apply_chamfer(3.0).has_error());
}

TEST_CASE("Ändern: trim shortens a line at the picked intersection",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {4.0, -5.0}, {4.0, 5.0});
  auto& tools = window.sketch_tools();

  const auto applied = tools.apply_modify_pick(SketchModifyKind::Trim, "line.h", {8.0, 0.0});
  REQUIRE(applied);
  const auto* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  const auto* trimmed = sketch->find_line_segment(SketchEntityId("line.h"));
  REQUIRE(trimmed != nullptr);
  CHECK(near(trimmed->end(), {4.0, 0.0}));
}

TEST_CASE("Ändern: split divides a line and the modal tool arms then disarms",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {20.0, 0.0}, {20.0, 5.0});
  auto& tools = window.sketch_tools();

  tools.begin_modify_tool(SketchModifyKind::Split);
  CHECK(tools.modify_tool_active());
  tools.cancel_tool(); // switching away / Escape ends the modal tool
  CHECK_FALSE(tools.modify_tool_active());

  const auto applied = tools.apply_modify_pick(SketchModifyKind::Split, "line.h", {6.0, 0.0});
  REQUIRE(applied);
  const auto* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 3U); // line.h split in two + untouched second line
}


TEST_CASE("Ändern: fillet auto-places coincidence and tangent constraints",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0});
  auto& tools = window.sketch_tools();

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));
  REQUIRE(tools.apply_fillet(2.0));

  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  std::size_t coincident = 0;
  std::size_t tangent = 0;
  for (const auto& constraint : catalog.value().constraints()) {
    if (constraint.kind() == SketchSolverConstraintKind::Coincident)
      ++coincident;
    if (constraint.kind() == SketchSolverConstraintKind::Tangent)
      ++tangent;
  }
  CHECK(coincident == 2U); // one per corner touch point
  CHECK(tangent == 2U);    // arc tangent to each trimmed line
}

TEST_CASE("Ändern: chamfer auto-places coincidence constraints",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0});
  auto& tools = window.sketch_tools();

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));
  REQUIRE(tools.apply_chamfer(3.0));

  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  std::size_t coincident = 0;
  for (const auto& constraint : catalog.value().constraints())
    if (constraint.kind() == SketchSolverConstraintKind::Coincident)
      ++coincident;
  CHECK(coincident == 2U); // one per corner touch point (no tangent for a chamfer)
}

namespace {
// Draws two chained lines meeting at (10,0) (the chain adds a corner Coincident)
// and returns the two line topology ids, selected and ready to fillet/chamfer.
std::vector<std::string> chained_corner(ShellWindow& window) {
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({10.0, 0.0}));
  REQUIRE(tools.add_pick({10.0, 10.0}));
  tools.cancel_tool();
  std::vector<std::string> line_ids;
  const auto topology = topology_of(window); // keep the topology alive while iterating
  for (const auto& entity : topology.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      line_ids.push_back(entity.id());
  REQUIRE(line_ids.size() == 2U);
  window.session().selection().clear();
  for (const auto& id : line_ids)
    REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, id}));
  return line_ids;
}
} // namespace

TEST_CASE("Ändern: fillet on a chained corner stays attached when a line is dragged",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  (void)chained_corner(window);
  const auto sketch_id = *window.active_sketch();
  REQUIRE(window.sketch_tools().apply_fillet(2.0));

  // The stale chained-corner coincidence was pruned, so the fillet is the real
  // geometry (arc between the two tangent points), not a collapsed corner.
  const auto* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->arc_segments().size() == 1U);
  const auto& arc = sketch->arc_segments().front();
  const bool endpoints_ok = (near(arc.start(), {8.0, 0.0}) && near(arc.end(), {10.0, 2.0})) ||
                            (near(arc.start(), {10.0, 2.0}) && near(arc.end(), {8.0, 0.0}));
  CHECK(endpoints_ok);
  const double radius_before = arc_radius(arc.start(), arc.mid(), arc.end());
  CHECK(radius_before == Catch::Approx(2.0).margin(1.0e-6));

  // A driving Radius dimension was auto-placed to pin the size.
  auto dims = window.session().sketch_dimension_catalog(sketch_id);
  REQUIRE(dims);
  CHECK(dims.value().dimensions().size() == 1U);

  // Drag line1's far endpoint (0,0): the arc must follow, still meeting both lines.
  auto drag = GuiSketchDragController::create(window.session(), sketch_id);
  REQUIRE(drag);
  std::string handle_id;
  for (const auto& handle : drag.value().handles())
    if (handle.kind == GuiSketchDragHandleKind::Endpoint && near(handle.position, {0.0, 0.0}))
      handle_id = handle.id;
  REQUIRE_FALSE(handle_id.empty());
  REQUIRE(drag.value().begin(handle_id));
  REQUIRE(drag.value().queue_pointer({-6.0, -4.0}));
  auto preview = drag.value().process_pending();
  REQUIRE(preview);
  const Sketch& dragged = preview.value().preview_sketch();
  REQUIRE(dragged.arc_segments().size() == 1U);
  const auto& moved = dragged.arc_segments().front();
  double gap_start = 1.0e9;
  double gap_end = 1.0e9;
  for (const auto& segment : dragged.line_segments())
    for (const Point2 point : {segment.start(), segment.end()}) {
      gap_start = std::min(gap_start, std::hypot(point.x - moved.start().x, point.y - moved.start().y));
      gap_end = std::min(gap_end, std::hypot(point.x - moved.end().x, point.y - moved.end().y));
    }
  CHECK(gap_start <= 1.0e-6); // arc start still glued to a line endpoint
  CHECK(gap_end <= 1.0e-6);   // arc end still glued to a line endpoint

  // The radius stays fixed under drag (the driving Radius dimension holds it).
  const double radius_after = arc_radius(moved.start(), moved.mid(), moved.end());
  CHECK(radius_after == Catch::Approx(radius_before).margin(1.0e-4));
}

TEST_CASE("Ändern: chamfer on a chained corner produces clean geometry",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  (void)chained_corner(window);
  const auto sketch_id = *window.active_sketch();
  REQUIRE(window.sketch_tools().apply_chamfer(3.0));

  const auto* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 3U); // two trimmed lines + the chamfer line
  // No collapse: the two far endpoints are preserved (not pulled together).
  bool has_far_a = false;
  bool has_far_b = false;
  for (const auto& segment : sketch->line_segments())
    for (const Point2 point : {segment.start(), segment.end()}) {
      has_far_a = has_far_a || near(point, {0.0, 0.0});
      has_far_b = has_far_b || near(point, {10.0, 10.0});
    }
  CHECK(has_far_a);
  CHECK(has_far_b);

  // A driving Length dimension was auto-placed to pin the chamfer size.
  auto dims = window.session().sketch_dimension_catalog(sketch_id);
  REQUIRE(dims);
  CHECK(dims.value().dimensions().size() == 1U);

  // The chamfer line is the segment touching neither far endpoint.
  const auto chamfer_length = [](const Sketch& source) -> double {
    for (const auto& segment : source.line_segments())
      if (!near(segment.start(), {0.0, 0.0}) && !near(segment.end(), {0.0, 0.0}) &&
          !near(segment.start(), {10.0, 10.0}) && !near(segment.end(), {10.0, 10.0}))
        return std::hypot(segment.start().x - segment.end().x,
                          segment.start().y - segment.end().y);
    return -1.0;
  };
  const double length_before = chamfer_length(*sketch);
  CHECK(length_before > 0.0);

  // Drag line1's far endpoint (0,0): the chamfer keeps its size.
  auto drag = GuiSketchDragController::create(window.session(), sketch_id);
  REQUIRE(drag);
  std::string handle_id;
  for (const auto& handle : drag.value().handles())
    if (handle.kind == GuiSketchDragHandleKind::Endpoint && near(handle.position, {0.0, 0.0}))
      handle_id = handle.id;
  REQUIRE_FALSE(handle_id.empty());
  REQUIRE(drag.value().begin(handle_id));
  REQUIRE(drag.value().queue_pointer({-6.0, -4.0}));
  auto preview = drag.value().process_pending();
  REQUIRE(preview);
  const double length_after = chamfer_length(preview.value().preview_sketch());
  CHECK(length_after == Catch::Approx(length_before).margin(1.0e-4));
}

TEST_CASE("Ändern/Zeichnen: point tool stays armed and drops multiple points",
          "[gui][shell-sketch][gui][sketch-create]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Point));
  REQUIRE(tools.tool_active());
  REQUIRE(tools.add_pick({5.0, 5.0}));
  // Re-arms after each placement (Inventor-style) instead of deactivating.
  CHECK(tools.tool_active());
  REQUIRE(tools.add_pick({8.0, 2.0}));
  CHECK(tools.tool_active());
  const auto* part = window.session().part_document();
  // Free sketch points (not part-level construction points): they live in the
  // sketch as ordinary, editable geometry.
  const auto* sketch = part->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  CHECK(sketch->points().size() == 2U);
  CHECK(part->construction_points().empty());
  tools.cancel_tool();
  CHECK_FALSE(tools.tool_active());

  // They appear as clickable point primitives in the interaction scene.
  auto scene = GuiSketchInteractionSceneBuilder{}.build(*part, *sketch);
  REQUIRE(scene);
  std::size_t scene_points = 0;
  for (const auto& primitive : scene.value().points)
    if (primitive.semantic_id.find("sketch.point.") != std::string::npos)
      ++scene_points;
  CHECK(scene_points == 2U);
}

TEST_CASE("Zeichnen: a free sketch point is draggable and constrainable",
          "[gui][shell-sketch][gui][sketch-create]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  const auto sketch_id = *window.active_sketch();

  // A free point and a line.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Point));
  REQUIRE(tools.add_pick({5.0, 5.0}));
  tools.cancel_tool();
  seed_two_lines(window, {0.0, 0.0}, {20.0, 0.0}, {30.0, 0.0}, {40.0, 0.0});

  const auto* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->points().size() == 1U);
  const SketchEntityId point_id = sketch->points().front().id();

  // Draggable: the point has a (non-reference) drag handle at its position.
  auto drag = GuiSketchDragController::create(window.session(), sketch_id);
  REQUIRE(drag);
  std::string handle_id;
  for (const auto& handle : drag.value().handles())
    if (handle.kind == GuiSketchDragHandleKind::Endpoint && near(handle.position, {5.0, 5.0}))
      handle_id = handle.id;
  REQUIRE_FALSE(handle_id.empty());
  REQUIRE(drag.value().begin(handle_id));
  REQUIRE(drag.value().queue_pointer({7.0, 9.0}));
  auto preview = drag.value().process_pending();
  REQUIRE(preview);
  const auto* moved = preview.value().preview_sketch().find_point(point_id);
  REQUIRE(moved != nullptr);
  CHECK(near(moved->position(), {7.0, 9.0}));

  // Constrainable: point coincident with a line -> point moves onto the line.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "entity/" + point_id.value()}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Coincident)); // point-on-line
  const auto* solved = window.session().part_document()->find_sketch(sketch_id);
  const auto* constrained = solved->find_point(point_id);
  const auto* line = solved->find_line_segment(SketchEntityId("line.h"));
  REQUIRE(constrained != nullptr);
  REQUIRE(line != nullptr);
  // The line is free, so the point settles onto it at a compromise (not y=0):
  // assert it actually lies on the line's infinite extension.
  const Point2 p = constrained->position();
  const Point2 a = line->start();
  const Point2 b = line->end();
  const double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  CHECK(std::abs(cross) <= 1.0e-6);
}

TEST_CASE("Abhängigkeiten: two lines can be made collinear",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  seed_two_lines(window, {0.0, 0.0}, {10.0, 1.0}, {12.0, 5.0}, {20.0, 6.0});
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Collinear));

  // Both lines now lie on one infinite line: all four endpoints are collinear.
  const auto topology = topology_of(window);
  std::vector<Point2> ends;
  for (const auto& entity : topology.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      for (const auto& id : entity.points())
        ends.push_back(topology.find_point(id)->position());
  REQUIRE(ends.size() == 4U);
  for (std::size_t index = 2U; index < ends.size(); ++index) {
    const double cross = (ends[1].x - ends[0].x) * (ends[index].y - ends[0].y) -
                         (ends[1].y - ends[0].y) * (ends[index].x - ends[0].x);
    CHECK(std::abs(cross) <= 1.0e-6);
  }
}
