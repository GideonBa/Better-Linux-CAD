#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QMouseEvent>

#include <cmath>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {

void enter_sketch(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Interaktion")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.active_sketch().has_value());
}

[[nodiscard]] SketchTopology topology_of(ShellWindow& window) {
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  return std::move(topology.value());
}

// Click a plane point through the real viewport hit path.
void click_plane(ShellWindow& window, Point2 point,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  auto& viewport = window.viewport();
  const auto screen = viewport.sketch_plane_to_screen(point);
  REQUIRE(screen);
  const QPointF position(screen.value().x, screen.value().y);
  QMouseEvent press(QEvent::MouseButtonPress, position, viewport.mapToGlobal(position.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, modifiers);
  QMouseEvent release(QEvent::MouseButtonRelease, position,
                      viewport.mapToGlobal(position.toPoint()), Qt::LeftButton, Qt::NoButton,
                      modifiers);
  QApplication::sendEvent(&viewport, &press);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();
}

} // namespace

TEST_CASE("Block 134 clicking endpoints selects the point role for coincident",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  // Two disjoint lines whose near ends meet at (20,0) but are NOT coincident.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 25.0}));
  tools.cancel_tool();
  REQUIRE(topology_of(window).entities().size() == 2U);

  // Click the two coincident-looking endpoints; the selection must carry the
  // endpoint role (candidate_id), not just the entity.
  window.session().selection().clear();
  click_plane(window, {20.0, 0.0}); // endpoint of line 1 or 2
  REQUIRE_FALSE(window.session().selection().entries().empty());
  CHECK_FALSE(window.session().selection().entries().front().candidate_id.empty());

  // Programmatically select both shared endpoints and apply Coincident.
  const auto topology = topology_of(window);
  window.session().selection().clear();
  for (const auto& entity : topology.entities()) {
    // Pick the endpoint at (20,0) of each line.
    for (const auto& point_id : entity.points()) {
      const auto* point = topology.find_point(point_id);
      if (point != nullptr &&
          std::hypot(point->position().x - 20.0, point->position().y) < 1.0e-6) {
        REQUIRE(window.session().selection().add(
            {GuiSelectionKind::SketchEntity, entity.id(), point_id.value()}));
        break;
      }
    }
  }
  REQUIRE(window.session().selection().entries().size() == 2U);
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  REQUIRE(applied);
  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  CHECK(catalog.value().constraints().size() == 1U);
}

TEST_CASE("Block 134 constraints still work when the sketch contains a circle profile",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  // A circle first (the user's failing scenario), then two chained lines.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({-30.0, 20.0}));
  REQUIRE(tools.add_pick({-20.0, 20.0}));
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 30.0}));
  REQUIRE(tools.add_pick({8.0, 0.0}));
  REQUIRE(tools.add_pick({40.0, 0.0})); // chained second segment
  tools.cancel_tool();

  const auto topology = topology_of(window);
  REQUIRE(topology.entities().size() >= 3U); // circle profile + two lines
  // The chained corner must be coupled coincident even with the profile there.
  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  REQUIRE(catalog);
  INFO("constraints after chain: " << catalog.value().constraints().size());
  CHECK(catalog.value().constraints().size() == 1U);

  // Coincident between two clicked endpoints of the two lines.
  std::vector<const SketchTopologyEntity*> lines;
  for (const auto& entity : topology.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      lines.push_back(&entity);
  REQUIRE(lines.size() == 2U);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, lines[0]->id(), lines[0]->points()[0].value()}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, lines[1]->id(), lines[1]->points()[1].value()}));
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (applied.has_error())
    WARN("coincident with circle present: " << applied.error().object_id() << ": "
                                             << applied.error().message());
  REQUIRE(applied);
}

TEST_CASE("Block 134 click plus Ctrl-click on two endpoints then Koinzident joins them",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  // Two lines whose near endpoints are close but distinct.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({26.0, 3.0}));
  REQUIRE(tools.add_pick({40.0, 20.0}));
  tools.cancel_tool();

  window.session().selection().clear();
  click_plane(window, {20.0, 0.0}); // endpoint of line 1
  CHECK(window.session().selection().entries().size() == 1U);
  click_plane(window, {26.0, 3.0}, Qt::ControlModifier); // + endpoint of line 2
  {
    const auto& entries = window.session().selection().entries();
    INFO("selection: " << entries.size()
                       << (entries.empty() ? "" : " first=" + entries.front().semantic_id + "|" +
                                                      entries.front().candidate_id));
    REQUIRE(entries.size() == 2U);
  }
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (applied.has_error())
    WARN("ctrl-click coincident: " << applied.error().object_id() << ": "
                                    << applied.error().message());
  REQUIRE(applied);
  // The two endpoints now coincide.
  const auto after = topology_of(window);
  std::vector<const SketchTopologyEntity*> lines;
  for (const auto& entity : after.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      lines.push_back(&entity);
  REQUIRE(lines.size() == 2U);
  const auto* first_end = after.find_point(lines[0]->points()[1]);
  const auto* second_start = after.find_point(lines[1]->points()[0]);
  REQUIRE(first_end != nullptr);
  REQUIRE(second_start != nullptr);
  CHECK(std::hypot(first_end->position().x - second_start->position().x,
                   first_end->position().y - second_start->position().y) < 1.0e-6);
}

TEST_CASE("Block 134 Koinzident works with circles: center point and rim",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({0.0, 20.0}));
  REQUIRE(tools.add_pick({10.0, 20.0})); // radius 10
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  REQUIRE(tools.add_pick({50.0, 0.0}));
  tools.cancel_tool();

  const auto topology = topology_of(window);
  const SketchTopologyEntity* line = nullptr;
  const SketchTopologyEntity* circle = nullptr;
  for (const auto& entity : topology.entities()) {
    if (entity.kind() == SketchTopologyEntityKind::Line)
      line = &entity;
    if (entity.kind() == SketchTopologyEntityKind::CircleProfile)
      circle = &entity;
  }
  REQUIRE(line != nullptr);
  REQUIRE(circle != nullptr);

  // Point ↔ circle CENTER: the clicked center resolves to the center point.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, line->id(), line->points()[0].value()}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, circle->id(),
       circle->id().substr(std::string_view("profile/").size()) + ":center"}));
  const auto center_joined = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (center_joined.has_error())
    WARN("center coincident: " << center_joined.error().message());
  REQUIRE(center_joined);
  REQUIRE(window.undo()); // restore for the rim case

  // Point ↔ circle RIM: Koinzident maps to point-on-object with baked radius.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, line->id(), line->points()[0].value()}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, circle->id()}));
  const auto rim_joined = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (rim_joined.has_error())
    WARN("rim coincident: " << rim_joined.error().message());
  REQUIRE(rim_joined);
  const auto after = topology_of(window);
  const auto* moved = after.find_point(line->points()[0]);
  const auto* center = after.find_point(circle->points()[0]);
  REQUIRE(moved != nullptr);
  REQUIRE(center != nullptr);
  const double distance = std::hypot(moved->position().x - center->position().x,
                                     moved->position().y - center->position().y);
  CHECK(std::abs(distance - 10.0) < 1.0e-4);
}

TEST_CASE("Block 134 rectangle lines delete together in one multi-selection",
          "[gui][shell-sketch][gui][sketch-edit]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 20.0}));
  CHECK_FALSE(tools.tool_active());
  const auto topology = topology_of(window);
  std::vector<std::string> lines;
  for (const auto& entity : topology.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      lines.push_back(entity.id());
  REQUIRE(lines.size() == 4U);

  // Deleting one member removes the whole closed-profile group (the legacy
  // Sketch JSON can only express the shared corners THROUGH the profile).
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, lines[0]}));
  {
    const auto deleted = tools.delete_selection();
    if (deleted.has_error())
      WARN("rectangle delete: " << deleted.error().object_id() << ": "
                                 << deleted.error().message());
    REQUIRE(deleted);
  }
  CHECK(topology_of(window).entities().empty());
  REQUIRE(window.undo());
  CHECK(topology_of(window).entities().size() == 5U); // 4 lines + profile back

  // Multi-delete of several INDEPENDENT lines in one go.
  REQUIRE(window.redo());
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({50.0, 0.0}));
  REQUIRE(tools.add_pick({70.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({50.0, 10.0}));
  REQUIRE(tools.add_pick({70.0, 10.0}));
  tools.cancel_tool();
  const auto later = topology_of(window);
  window.session().selection().clear();
  for (const auto& entity : later.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity.id()}));
  {
    const auto deleted = tools.delete_selection();
    if (deleted.has_error())
      WARN("multi delete: " << deleted.error().object_id() << ": "
                             << deleted.error().message());
    REQUIRE(deleted);
  }
  CHECK(topology_of(window).entities().empty());
}

TEST_CASE("Block 134 Koinzident with a line midpoint maps to the midpoint constraint",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({40.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({10.0, 15.0}));
  REQUIRE(tools.add_pick({30.0, 30.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  std::vector<const SketchTopologyEntity*> lines;
  for (const auto& entity : topology.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line)
      lines.push_back(&entity);
  REQUIRE(lines.size() == 2U);

  // Select line 1's MIDPOINT role + line 2's start point, apply Koinzident.
  const std::string raw_line_id =
      lines[0]->id().substr(std::string_view("entity/").size());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, lines[0]->id(), raw_line_id + ":midpoint"}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, lines[1]->id(), lines[1]->points()[0].value()}));
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (applied.has_error())
    WARN("midpoint coincident: " << applied.error().object_id() << ": "
                                  << applied.error().message());
  REQUIRE(applied);
  // The second line's start now sits exactly at line 1's middle.
  const auto after = topology_of(window);
  const auto* moved = after.find_point(lines[1]->points()[0]);
  const auto* start = after.find_point(lines[0]->points()[0]);
  const auto* end = after.find_point(lines[0]->points()[1]);
  REQUIRE(moved != nullptr);
  const Point2 middle{(start->position().x + end->position().x) * 0.5,
                      (start->position().y + end->position().y) * 0.5};
  CHECK(std::hypot(moved->position().x - middle.x, moved->position().y - middle.y) < 1.0e-6);
}

TEST_CASE("Block 134 tangent works after joining line and arc coincident first",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  // Line and arc that do NOT touch initially; join them coincident first
  // (user flow), then apply tangent.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::ThreePointArc));
  REQUIRE(tools.add_pick({22.0, 1.0}));
  REQUIRE(tools.add_pick({30.0, 8.0}));
  REQUIRE(tools.add_pick({32.0, 18.0}));

  const auto topology = topology_of(window);
  const SketchTopologyEntity* line = nullptr;
  const SketchTopologyEntity* arc = nullptr;
  for (const auto& entity : topology.entities()) {
    if (entity.kind() == SketchTopologyEntityKind::Line)
      line = &entity;
    if (entity.kind() == SketchTopologyEntityKind::Arc)
      arc = &entity;
  }
  REQUIRE(line != nullptr);
  REQUIRE(arc != nullptr);

  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, line->id(), line->points()[1].value()}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, arc->id(), arc->points()[0].value()}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Coincident));

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, line->id()}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, arc->id()}));
  const auto tangent = tools.apply_constraint(SketchSolverConstraintKind::Tangent);
  if (tangent.has_error())
    WARN("tangent after coincident: " << tangent.error().object_id() << ": "
                                       << tangent.error().message());
  REQUIRE(tangent);

  // Two straight lines must NOT offer tangent.
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({50.0, 0.0}));
  REQUIRE(tools.add_pick({70.0, 0.0}));
  REQUIRE(tools.add_pick({70.0, 20.0}));
  tools.cancel_tool();
  const auto after = topology_of(window);
  std::vector<const SketchTopologyEntity*> new_lines;
  for (const auto& entity : after.entities())
    if (entity.kind() == SketchTopologyEntityKind::Line && entity.id() != line->id())
      new_lines.push_back(&entity);
  REQUIRE(new_lines.size() >= 2U);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, new_lines[0]->id()}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, new_lines[1]->id()}));
  CHECK(tools.apply_constraint(SketchSolverConstraintKind::Tangent).has_error());
}

TEST_CASE("Block 134 tangent works between a line and a circle",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({0.0, 20.0}));
  REQUIRE(tools.add_pick({10.0, 20.0})); // radius 10
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({-30.0, 5.0}));
  REQUIRE(tools.add_pick({30.0, 5.0})); // 5 mm below the circle rim
  tools.cancel_tool();

  const auto topology = topology_of(window);
  const SketchTopologyEntity* line = nullptr;
  const SketchTopologyEntity* circle = nullptr;
  for (const auto& entity : topology.entities()) {
    if (entity.kind() == SketchTopologyEntityKind::Line)
      line = &entity;
    if (entity.kind() == SketchTopologyEntityKind::CircleProfile)
      circle = &entity;
  }
  REQUIRE(line != nullptr);
  REQUIRE(circle != nullptr);

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, line->id()}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, circle->id()}));
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Tangent);
  if (applied.has_error())
    WARN("line-circle tangent: " << applied.error().object_id() << ": "
                                  << applied.error().message());
  REQUIRE(applied);

  // Afterwards the center-to-line distance equals the radius (10 mm).
  const auto after = topology_of(window);
  const auto* center = after.find_point(circle->points()[0]);
  const auto* start = after.find_point(line->points()[0]);
  const auto* end = after.find_point(line->points()[1]);
  REQUIRE(center != nullptr);
  REQUIRE(start != nullptr);
  REQUIRE(end != nullptr);
  const double dx = end->position().x - start->position().x;
  const double dy = end->position().y - start->position().y;
  const double px = center->position().x - start->position().x;
  const double py = center->position().y - start->position().y;
  const double distance = std::abs(dx * py - dy * px) / std::hypot(dx, dy);
  CHECK(std::abs(distance - 10.0) < 1.0e-4);
}

TEST_CASE("Block 134 deleting a constrained entity cascades its constraints and dimensions",
          "[gui][shell-sketch][gui][sketch-edit]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 2.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  const std::string entity_id = topology.entities().front().id();

  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity_id}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Horizontal));
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity_id}));
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, false));

  // Deleting the constrained + dimensioned line removes it together with the
  // referencing intents.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity_id}));
  {
    const auto deleted = tools.delete_selection();
    if (deleted.has_error())
      WARN("cascade delete: " << deleted.error().object_id() << ": "
                               << deleted.error().message());
    REQUIRE(deleted);
  }
  CHECK(topology_of(window).entities().empty());
  CHECK(window.session().sketch_constraint_catalog(*window.active_sketch()).value()
            .constraints().empty());
  CHECK(window.session().sketch_dimension_catalog(*window.active_sketch()).value()
            .dimensions().empty());
}

TEST_CASE("Block 134 tangent works for positionally connected line and arc",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  tools.cancel_tool();
  // Three-point arc starting exactly at the line end (separate entity, no
  // shared topology point).
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::ThreePointArc));
  REQUIRE(tools.add_pick({20.0, 0.0}));
  REQUIRE(tools.add_pick({28.0, 6.0}));
  REQUIRE(tools.add_pick({30.0, 15.0}));
  const auto topology = topology_of(window);
  REQUIRE(topology.entities().size() == 2U);

  window.session().selection().clear();
  for (const auto& entity : topology.entities())
    REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity.id()}));
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Tangent);
  if (applied.has_error())
    WARN("tangent error: " << applied.error().object_id() << ": " << applied.error().message());
  REQUIRE(applied);
  CHECK(window.session().sketch_constraint_catalog(*window.active_sketch()).value()
            .constraints().size() >= 1U);
}

TEST_CASE("Block 134 Koinzident maps point plus curve to point-on-object in any order",
          "[gui][shell-sketch][gui][sketch-constraints]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({10.0, 5.0}));
  REQUIRE(tools.add_pick({10.0, 25.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  REQUIRE(topology.entities().size() == 2U);

  // Entity FIRST, then point (order-insensitive): select line 1 as entity and
  // line 2's start point.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities()[0].id()}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities()[1].id(),
       topology.entities()[1].points()[0].value()}));
  const auto applied = tools.apply_constraint(SketchSolverConstraintKind::Coincident);
  if (applied.has_error())
    WARN("koinzident error: " << applied.error().message());
  REQUIRE(applied);
  // The point lies ON line 1 afterwards (either side may have moved).
  const auto after = topology_of(window);
  const auto* moved = after.find_point(topology.entities()[1].points()[0]);
  const auto* line_start = after.find_point(topology.entities()[0].points()[0]);
  const auto* line_end = after.find_point(topology.entities()[0].points()[1]);
  REQUIRE(moved != nullptr);
  REQUIRE(line_start != nullptr);
  REQUIRE(line_end != nullptr);
  const double dx = line_end->position().x - line_start->position().x;
  const double dy = line_end->position().y - line_start->position().y;
  const double px = moved->position().x - line_start->position().x;
  const double py = moved->position().y - line_start->position().y;
  CHECK(std::abs(dx * py - dy * px) / std::hypot(dx, dy) < 1.0e-6); // distance to line
}

TEST_CASE("Block 134 delete removes constraint dimension and entity; undo restores",
          "[gui][shell-sketch][gui][sketch-edit]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();
  REQUIRE(topology_of(window).entities().size() == 1U);
  const std::string entity_id = topology_of(window).entities().front().id();

  // Dimension the line (auto-parameter), then delete it.
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity_id}));
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, false));
  REQUIRE(window.session().sketch_dimension_catalog(*window.active_sketch()).value()
              .dimensions().size() == 1U);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity,
       "sketch/" + window.active_sketch()->value() + "/dimension/" +
           window.session().sketch_dimension_catalog(*window.active_sketch())
               .value().dimensions().front().id().value()}));
  {
    const auto deleted = tools.delete_selection();
    if (deleted.has_error())
      WARN("delete dimension error: " << deleted.error().object_id() << ": "
                                       << deleted.error().message());
    REQUIRE(deleted);
  }
  CHECK(window.session().sketch_dimension_catalog(*window.active_sketch()).value()
            .dimensions().empty());

  // Undo restores the dimension, redo removes it again.
  REQUIRE(window.undo());
  CHECK(window.session().sketch_dimension_catalog(*window.active_sketch()).value()
            .dimensions().size() == 1U);
  REQUIRE(window.redo());
  CHECK(window.session().sketch_dimension_catalog(*window.active_sketch()).value()
            .dimensions().empty());

  // Delete the entity itself (now free of dependents).
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity_id}));
  {
    const auto deleted = tools.delete_selection();
    if (deleted.has_error())
      WARN("delete entity error: " << deleted.error().object_id() << ": "
                                    << deleted.error().message());
    REQUIRE(deleted);
  }
  CHECK(topology_of(window).entities().empty());
  REQUIRE(window.undo());
  CHECK(topology_of(window).entities().size() == 1U);
}

TEST_CASE("Block 134 a horizontally constrained line still drags along its free axis",
          "[gui][shell-sketch][gui][sketch-drag]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities().front().id()}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Horizontal));

  // Controller-level drag of the line end after the constraint.
  auto drag = GuiSketchDragController::create(window.session(), *window.active_sketch());
  if (drag.has_error())
    WARN("drag create error: " << drag.error().message());
  REQUIRE(drag);
  REQUIRE_FALSE(drag.value().handles().empty());
  // Pick the handle at the line end (30,0).
  std::string handle_id;
  for (const auto& handle : drag.value().handles())
    if (std::hypot(handle.position.x - 30.0, handle.position.y) < 1.0e-6)
      handle_id = handle.id;
  REQUIRE_FALSE(handle_id.empty());
  {
    const auto begun = drag.value().begin(handle_id);
    if (begun.has_error())
      WARN("drag begin error: " << begun.error().message());
    REQUIRE(begun);
  }
  auto preview = drag.value().flush({45.0, 6.0});
  if (preview.has_error())
    WARN("drag flush error: " << preview.error().message());
  REQUIRE(preview);
  // The end moved in x; the horizontal constraint keeps y level.
  const auto* moved = preview.value().topology().find_point(
      topology.entities().front().points()[1]);
  REQUIRE(moved != nullptr);
  CHECK(std::abs(moved->position().x - 45.0) < 1.0);
  CHECK(std::abs(moved->position().y - preview.value()
                                            .topology()
                                            .find_point(topology.entities().front().points()[0])
                                            ->position()
                                            .y) < 1.0e-6);
}

TEST_CASE("Block 134 mouse drag still works after applying a constraint via the ribbon path",
          "[gui][shell-sketch][gui][sketch-drag]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  auto& viewport = window.viewport();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.entities().front().id()}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Horizontal));
  window.refresh(); // exactly what the ribbon slot does afterwards
  qApp->processEvents();

  // The armed drag controller must know the new constraint (DOF 4 → 3).
  REQUIRE(tools.drag_controller().has_value());
  INFO("baseline dof: " << tools.drag_controller()->baseline_solve().remaining_dof);
  CHECK(tools.drag_controller()->baseline_solve().remaining_dof == 3U);

  // Drag the line end (30,0) to (45,6) with real mouse events.
  const auto start_screen = viewport.sketch_plane_to_screen({30.0, 0.0});
  const auto end_screen = viewport.sketch_plane_to_screen({45.0, 6.0});
  REQUIRE(start_screen);
  REQUIRE(end_screen);
  const QPointF from(start_screen.value().x, start_screen.value().y);
  const QPointF to(end_screen.value().x, end_screen.value().y);
  QMouseEvent press(QEvent::MouseButtonPress, from, viewport.mapToGlobal(from.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  qApp->processEvents();
  QMouseEvent move(QEvent::MouseMove, to, viewport.mapToGlobal(to.toPoint()), Qt::NoButton,
                   Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  qApp->processEvents();
  QMouseEvent release(QEvent::MouseButtonRelease, to, viewport.mapToGlobal(to.toPoint()),
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();

  const auto after = topology_of(window);
  const auto* moved = after.find_point(topology.entities().front().points()[1]);
  const auto* anchor = after.find_point(topology.entities().front().points()[0]);
  REQUIRE(moved != nullptr);
  REQUIRE(anchor != nullptr);
  INFO("moved to " << moved->position().x << ", " << moved->position().y);
  CHECK(std::abs(moved->position().x - 45.0) < 2.0); // x follows the pointer
  // Horizontal is held: both endpoints share one y (the free line may float).
  CHECK(std::abs(moved->position().y - anchor->position().y) < 1.0e-6);
}

TEST_CASE("Block 134 drag handles survive a parallel constraint",
          "[gui][shell-sketch][gui][sketch-drag]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({20.0, 2.0}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 20.0}));
  REQUIRE(tools.add_pick({20.0, 20.0}));
  tools.cancel_tool();

  const auto before = window.viewport().sketch_drag_handle_count();
  CHECK(before > 0U);

  const auto topology = topology_of(window);
  window.session().selection().clear();
  for (const auto& entity : topology.entities())
    REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity.id()}));
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Parallel));

  // Drag must still be armed afterwards (regression: parallel killed drag).
  CHECK(window.viewport().sketch_drag_handle_count() > 0U);
}
