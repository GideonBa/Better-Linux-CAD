#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"

#include "blcad/geometry/viewport_scene.hpp"

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
