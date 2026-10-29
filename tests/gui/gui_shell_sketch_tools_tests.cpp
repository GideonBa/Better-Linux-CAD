#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"

#include "blcad/core/sketch.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <QScreen>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

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

void show_endpoint_window(ShellWindow& window) {
  window.resize(1280, 800);
  if (QApplication::platformName() == "xcb") {
    auto* screen = QGuiApplication::screenAt(QPoint(0, 0));
    REQUIRE(screen);
    const auto available = screen->availableGeometry().adjusted(12, 12, -12, -12);
    window.move(available.topLeft());
    window.resize(std::min(1280, available.width()), std::min(800, available.height()));
  }
  window.show();
  window.raise();
  window.activateWindow();
  QGuiApplication::sync();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}

GuiSketchScreenPoint visible_endpoint_screen(OcctViewport& viewport, Point2 point) {
  INFO("Endpoint input at model " << point.x << ',' << point.y);
  for (int attempt = 0; attempt < 12; ++attempt) {
    auto screen = viewport.sketch_plane_to_screen(point);
    REQUIRE(screen);
    if (viewport.rect()
            .adjusted(12, 12, -12, -12)
            .contains(QPoint(screen.value().x, screen.value().y)))
      return screen.value();
    // Extending a chain can move past the current native fit. Exercise the
    // ordinary wheel route and remap before clicking; never send off-canvas input.
    const QPoint center = viewport.rect().center();
    QWheelEvent wheel(QPointF(center), QPointF(viewport.mapToGlobal(center)), {}, {0, -120},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewport, &wheel);
    QGuiApplication::sync();
    qApp->processEvents();
  }
  FAIL("Endpoint input remains outside the visible viewport after bounded zoom");
  return {};
}

void capture_endpoint_window(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  QGuiApplication::sync();
  qApp->processEvents();
  REQUIRE(window.grab().save(directory + '/' + QString::fromLatin1(name) + ".png"));
}

void reenter_sketch(ShellWindow& window, const SketchId& sketch) {
  auto& tree = window.model_browser();
  for (QTreeWidgetItemIterator it(&tree); *it; ++it) {
    if ((*it)->data(0, Qt::UserRole).toString().toStdString() == sketch.value()) {
      tree.itemDoubleClicked(*it, 0);
      REQUIRE(window.active_sketch() == sketch);
      return;
    }
  }
  FAIL("Sketch browser node missing");
}

[[nodiscard]] SketchTopology topology_of(ShellWindow& window) {
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  // These legacy tool tests inspect authored entities; selectable immutable
  // origin references are verified by the dedicated reference workflow tests.
  auto points = topology.value().points();
  auto entities = topology.value().entities();
  std::erase_if(points,
                [](const auto& point) { return point.id().value().starts_with("reference/"); });
  std::erase_if(entities, [](const auto& entity) { return entity.id().starts_with("reference/"); });
  return SketchTopology::create(sketch->id(), std::move(points), std::move(entities),
                                topology.value().dependencies())
      .value();
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

std::size_t automatic_point_on_object_count(ShellWindow& window,
                                            std::string_view trimmed_entity, Point2 endpoint,
                                            std::string_view cutter_entity) {
  const SketchId sketch_id = *window.active_sketch();
  const Sketch* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const SketchTopologyEntity* entity = topology.value().find_entity(trimmed_entity);
  REQUIRE(entity != nullptr);
  std::vector<std::string> points;
  for (const auto& point_id : entity->points()) {
    const SketchTopologyPoint* point = topology.value().find_point(point_id);
    REQUIRE(point != nullptr);
    if (near(point->position(), endpoint))
      points.push_back(point_id.value());
  }
  REQUIRE(points.size() == 1U);

  const auto catalog = window.session().sketch_constraint_catalog(sketch_id);
  REQUIRE(catalog);
  return static_cast<std::size_t>(std::count_if(
      catalog.value().constraints().begin(), catalog.value().constraints().end(),
      [&](const auto& intent) {
        return intent.kind() == SketchSolverConstraintKind::PointOnObject &&
               intent.source() == SketchConstraintIntentSource::Automatic &&
               intent.targets().size() == 2U && intent.targets()[0].id() == points.front() &&
               intent.targets()[1].id() == cutter_entity;
      }));
}

// Circumradius of the arc's 3 defining points.
double arc_radius(Point2 a, Point2 b, Point2 c) {
  const double ab = std::hypot(a.x - b.x, a.y - b.y);
  const double bc = std::hypot(b.x - c.x, b.y - c.y);
  const double ca = std::hypot(c.x - a.x, c.y - a.y);
  const double area = std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5;
  return area <= 1.0e-12 ? 0.0 : (ab * bc * ca) / (4.0 * area);
}

void move_sketch_pointer(OcctViewport& viewport, GuiSketchScreenPoint point) {
  const QPointF local(point.x, point.y);
  const QPointF global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent move(QEvent::MouseMove, local, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  qApp->processEvents();
}

void click_sketch_pointer(OcctViewport& viewport, GuiSketchScreenPoint point) {
  const QPointF local(point.x, point.y);
  const QPointF global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();
}

void press_sketch_pointer(OcctViewport& viewport, GuiSketchScreenPoint point) {
  const QPointF local(point.x, point.y);
  const QPointF global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  qApp->processEvents();
}

void drag_sketch_pointer(OcctViewport& viewport, GuiSketchScreenPoint point,
                         Qt::MouseButtons buttons = Qt::LeftButton) {
  const QPointF local(point.x, point.y);
  const QPointF global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent move(QEvent::MouseMove, local, global, Qt::NoButton, buttons, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  qApp->processEvents();
}

void release_sketch_pointer(OcctViewport& viewport, GuiSketchScreenPoint point) {
  const QPointF local(point.x, point.y);
  const QPointF global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&viewport, &release);
  qApp->processEvents();
}

void press_escape(ShellWindow& window) {
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&window, &escape);
  qApp->processEvents();
}

struct SeedLine {
  std::string id;
  Point2 start;
  Point2 end;
};

void seed_lines(ShellWindow& window, std::vector<SeedLine> lines) {
  const SketchId id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed stroke lines",
      [id, lines = std::move(lines)](PartDocument& part) mutable -> Result<std::size_t> {
        const Sketch* source = part.find_sketch(id);
        if (source == nullptr)
          return Result<std::size_t>::failure(Error::validation(id.value(), "no sketch"));
        Sketch sketch = *source;
        for (const auto& line : lines) {
          auto entity = LineSegment::create(SketchEntityId(line.id), line.start, line.end);
          if (entity.has_error())
            return Result<std::size_t>::failure(entity.error());
          auto added = sketch.add_entity(std::move(entity.value()));
          if (added.has_error())
            return added;
        }
        return part.update_sketch(std::move(sketch));
      }));
  window.refresh();
  window.sketch_tools().publish_scene();
  qApp->processEvents();
}

void verify_pointer_drawn_line_region(double close_offset_dip) {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  enter_sketch(window);
  auto& viewport = window.viewport();
  auto& tools = window.sketch_tools();

  // Keep the contour well away from the origin axes so this regression
  // isolates endpoint-vs-curve snapping rather than axis magnetism.
  const std::array<Point2, 4U> corners{{{50.0, 50.0}, {100.0, 50.0}, {100.0, 90.0}, {50.0, 90.0}}};
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  // A committed segment republishes the scene and can change the view fit.
  // Resolve each subsequent model point through the current mapping, just as
  // a user follows the geometry at its freshly painted screen position.
  for (const auto corner : corners) {
    const auto mapped = viewport.sketch_plane_to_screen(corner);
    REQUIRE(mapped);
    click_sketch_pointer(viewport, mapped.value());
  }

  const auto first_screen = viewport.sketch_plane_to_screen(corners[0]);
  const auto second_screen = viewport.sketch_plane_to_screen(corners[1]);
  REQUIRE(first_screen);
  REQUIRE(second_screen);

  const double dx = second_screen.value().x - first_screen.value().x;
  const double dy = second_screen.value().y - first_screen.value().y;
  const double length = std::hypot(dx, dy);
  REQUIRE(length > close_offset_dip);
  const GuiSketchScreenPoint close{first_screen.value().x + close_offset_dip * dx / length,
                                   first_screen.value().y + close_offset_dip * dy / length};
  move_sketch_pointer(viewport, close);
  REQUIRE(viewport.sketch_snap_result().has_value());
  INFO("closure offset=" << close_offset_dip
                         << " snap=" << to_string(viewport.sketch_snap_result()->kind)
                         << " candidate=" << viewport.sketch_snap_result()->candidate_id
                         << " screen-distance="
                         << viewport.sketch_snap_result()->screen_distance_dip);
  CHECK(viewport.sketch_snap_result()->kind == GuiSketchSnapKind::Endpoint);
  click_sketch_pointer(viewport, close);

  const SketchId sketch_id = *window.active_sketch();
  const auto* raw = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(raw != nullptr);
  REQUIRE(raw->line_segments().size() == 4U);
  const auto constraints = window.session().sketch_constraint_catalog(sketch_id);
  REQUIRE(constraints);
  CHECK(constraints.value().constraints().size() == 4U);
  for (const auto& intent : constraints.value().constraints()) {
    CHECK(intent.kind() == SketchSolverConstraintKind::Coincident);
    CHECK(intent.source() == SketchConstraintIntentSource::Automatic);
  }
  const auto geometry_json =
      serialize_part_document_to_json(*window.session().part_document()).value();
  const auto constraint_json =
      serialize_sketch_constraint_catalog_to_json(constraints.value()).value();
  tools.cancel_tool();
  REQUIRE(window.undo());
  CHECK(window.session().part_document()->find_sketch(sketch_id)->line_segments().size() == 3U);
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() ==
        geometry_json);
  CHECK(serialize_sketch_constraint_catalog_to_json(
            window.session().sketch_constraint_catalog(sketch_id).value())
            .value() == constraint_json);
  if (!window.active_sketch())
    reenter_sketch(window, sketch_id);
  raw = window.session().part_document()->find_sketch(sketch_id);
  const auto analysis =
      geometry::SketchFinishService::analyze(*window.session().part_document(), *raw);
  REQUIRE(analysis);
  INFO("regions=" << analysis.value().regions.size()
                  << " diagnostics=" << analysis.value().diagnostics.size());
  REQUIRE(analysis.value().diagnostics.empty());
  REQUIRE(analysis.value().regions.size() == 1U);
  const ProfileId expected_profile = analysis.value().regions.front().id;

  REQUIRE(window.finish_active_sketch());
  const auto* finished = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(finished != nullptr);
  REQUIRE(finished->closed_profiles().size() == 1U);
  CHECK(finished->closed_profiles().front().id() == expected_profile);

  REQUIRE(window.session().part_shape_cache() != nullptr);
  const auto scene = geometry::ViewportSceneBuilder{}.build_part(
      *window.session().part_document(), *window.session().part_shape_cache());
  REQUIRE(scene);
  const std::string expected_semantic =
      "sketch/" + sketch_id.value() + "/profile/" + expected_profile.value();
  const auto profile_item = std::find_if(
      scene.value().begin(), scene.value().end(),
      [&expected_semantic](const auto& item) { return item.semantic_id == expected_semantic; });
  REQUIRE(profile_item != scene.value().end());
  CHECK(profile_item->kind == geometry::ViewportSceneKind::SketchEntity);
  REQUIRE(viewport.select_semantic(expected_semantic));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->semantic_id == expected_semantic);
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

TEST_CASE("Line pointer clicks publish a selectable profile face when the loop closes exactly",
          "[gui][shell-sketch][gui][line-region-regression][endpoint-coincidence]") {
  verify_pointer_drawn_line_region(0.0);
}

TEST_CASE("Line pointer endpoint snap closes a visually closed loop",
          "[gui][shell-sketch][gui][line-region-regression][snap][endpoint-coincidence]") {
  // Four DIPs is visibly on the endpoint marker and inside the ten-DIP snap
  // tolerance.  A curve-nearest candidate must not steal this closure.
  verify_pointer_drawn_line_region(4.0);
}

TEST_CASE("Finish Sketch leaves an open contour intact for later editing",
          "[gui][shell-sketch][gui][line-region-regression][sketch-finish]") {
  ShellWindow window;
  enter_sketch(window);
  const SketchId sketch_id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({50.0, 50.0}));
  REQUIRE(tools.add_pick({100.0, 50.0}));
  REQUIRE(tools.add_pick({100.0, 90.0}));
  REQUIRE(tools.add_pick({50.0, 90.0}));
  // Deliberately bypass pointer snapping to model a real remaining gap.
  REQUIRE(tools.add_pick({50.5, 50.0}));
  tools.cancel_tool();

  const auto before = serialize_part_document_to_json(*window.session().part_document());
  REQUIRE(before);
  const auto history = window.session().undo_label();
  REQUIRE(window.finish_active_sketch());
  CHECK_FALSE(window.active_sketch());
  CHECK_FALSE(window.viewport().sketch_interaction_active());
  const auto* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  CHECK(sketch->profile_count() == 0U);
  CHECK(window.statusBar()->currentMessage().contains(QStringLiteral("open contours")));
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before.value());
  CHECK(window.session().undo_label() == history);
  CHECK_FALSE(window.feature_tools().begin_extrude());
  reenter_sketch(window, sketch_id);
  CHECK(window.viewport().sketch_interaction_active());
}

TEST_CASE("Restarted Line preserves the picked identity at overlapping endpoints through history "
          "and drag",
          "[gui][shell-sketch][endpoint-coincidence][endpoint-native-workflow]") {
  ShellWindow window;
  show_endpoint_window(window);
  enter_sketch(window);
  seed_two_lines(window, {50, 50}, {90, 50}, {90, 50}, {110, 75});
  auto& tools = window.sketch_tools();
  tools.publish_scene();
  qApp->processEvents();
  auto& viewport = window.viewport();
  const auto sketch = *window.active_sketch();
  const auto original = topology_of(window);
  const auto chosen = original.find_entity("entity/line.h")->points()[1];
  const auto unrelated = original.find_entity("entity/line.v")->points()[0];
  REQUIRE(chosen != unrelated);
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  const auto start = visible_endpoint_screen(viewport, {90, 50});
  move_sketch_pointer(viewport, start);
  REQUIRE(viewport.sketch_snap_result());
  REQUIRE(viewport.sketch_snap_result()->kind == GuiSketchSnapKind::Endpoint);
  REQUIRE(viewport.sketch_snap_result()->candidate_id == "line.h:end");
  click_sketch_pointer(viewport, start);
  click_sketch_pointer(viewport, visible_endpoint_screen(viewport, {70, 100}));
  tools.cancel_tool();
  const auto topology = topology_of(window);
  REQUIRE(topology.entities().size() == 3);
  const auto created =
      std::find_if(topology.entities().begin(), topology.entities().end(), [](const auto& entity) {
        return entity.id() != "entity/line.h" && entity.id() != "entity/line.v";
      });
  REQUIRE(created != topology.entities().end());
  const auto new_start = created->points()[0];
  const auto catalog = window.session().sketch_constraint_catalog(sketch).value();
  REQUIRE(catalog.constraints().size() == 1);
  CHECK(catalog.constraints().front().source() == SketchConstraintIntentSource::Automatic);
  std::vector<std::string> targets;
  for (const auto& target : catalog.constraints().front().targets())
    targets.push_back(target.id());
  CHECK(std::find(targets.begin(), targets.end(), chosen.value()) != targets.end());
  CHECK(std::find(targets.begin(), targets.end(), new_start.value()) != targets.end());
  CHECK(std::find(targets.begin(), targets.end(), unrelated.value()) == targets.end());
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  const auto relations = serialize_sketch_constraint_catalog_to_json(catalog).value();
  REQUIRE(window.undo());
  CHECK(window.session().part_document()->find_sketch(sketch)->line_segments().size() == 2);
  CHECK(window.session().sketch_constraint_catalog(sketch).value().constraints().empty());
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("endpoint.blcad");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(serialize_sketch_constraint_catalog_to_json(
            window.session().sketch_constraint_catalog(sketch).value())
            .value() == relations);
  if (!window.active_sketch())
    reenter_sketch(window, sketch);
  // Translate the chosen line through its unambiguous midpoint. The initial
  // coincident location deliberately contains an unrelated endpoint as well.
  (void)visible_endpoint_screen(viewport, {80, 40});
  move_sketch_pointer(viewport, visible_endpoint_screen(viewport, {70, 50}));
  press_sketch_pointer(viewport, visible_endpoint_screen(viewport, {70, 50}));
  const auto moved = visible_endpoint_screen(viewport, {80, 40});
  drag_sketch_pointer(viewport, moved);
  release_sketch_pointer(viewport, moved);
  const auto dragged = topology_of(window);
  CHECK(dragged.find_point(chosen)->position() == dragged.find_point(new_start)->position());
  CHECK_FALSE(near(dragged.find_point(chosen)->position(), {90, 50}));
  CHECK(near(dragged.find_point(unrelated)->position(), {90, 50}));
  capture_endpoint_window(window, "endpoint-overlap-after-drag");
  REQUIRE(window.undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.redo());
  CHECK(window.session().sketch_constraint_catalog(sketch).value().constraints().size() == 1);
}

TEST_CASE(
    "Queued creation picks retain press-time targets and cannot leak into a restarted command",
    "[gui][shell-sketch][endpoint-coincidence][endpoint-input-snapshot]") {
  ShellWindow window;
  show_endpoint_window(window);
  enter_sketch(window);
  seed_two_lines(window, {50, 50}, {90, 50}, {120, 80}, {120, 110});
  auto& tools = window.sketch_tools();
  tools.publish_scene();
  qApp->processEvents();
  auto& viewport = window.viewport();
  const auto sketch = *window.active_sketch();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  SECTION("Movement after press does not retarget the accepted endpoint") {
    REQUIRE(tools.add_pick({70, 140}));
    const auto clicked = visible_endpoint_screen(viewport, {90, 50});
    const auto later = visible_endpoint_screen(viewport, {120, 80});
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(clicked.x, clicked.y),
                      viewport.mapToGlobal(QPoint(clicked.x, clicked.y)), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(later.x, later.y),
                     viewport.mapToGlobal(QPoint(later.x, later.y)), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&viewport, &move);
    qApp->processEvents();
    release_sketch_pointer(viewport, clicked);
    tools.cancel_tool();
    const auto* raw = window.session().part_document()->find_sketch(sketch);
    REQUIRE(raw->line_segments().size() == 3);
    const auto created = std::find_if(
        raw->line_segments().begin(), raw->line_segments().end(), [](const auto& line) {
          return line.id() != SketchEntityId("line.h") && line.id() != SketchEntityId("line.v");
        });
    REQUIRE(created != raw->line_segments().end());
    CHECK(near(created->end(), {90, 50}));
    const auto chosen = topology_of(window).find_entity("entity/line.h")->points()[1];
    const auto catalog = window.session().sketch_constraint_catalog(sketch).value();
    REQUIRE(catalog.constraints().size() == 1);
    CHECK(std::any_of(catalog.constraints().front().targets().begin(),
                      catalog.constraints().front().targets().end(),
                      [&](const auto& target) { return target.id() == chosen.value(); }));
  }
  SECTION("Cancellation invalidates a queued press before the same tool restarts") {
    const auto clicked = visible_endpoint_screen(viewport, {90, 50});
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(clicked.x, clicked.y),
                      viewport.mapToGlobal(QPoint(clicked.x, clicked.y)), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &press);
    tools.cancel_tool();
    REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
    qApp->processEvents();
    release_sketch_pointer(viewport, clicked);
    REQUIRE(tools.add_pick({70, 140}));
    CHECK(window.session().part_document()->find_sketch(sketch)->line_segments().size() == 2);
    tools.cancel_tool();
  }
  SECTION("A burst of presses continues one line command across automatic rearming") {
    const std::array<Point2, 3> points{{{60, 100}, {80, 100}, {100, 100}}};
    for (const auto point : points)
      (void)visible_endpoint_screen(viewport, point);
    for (const auto point : points) {
      const auto screen = visible_endpoint_screen(viewport, point);
      const QPointF local(screen.x, screen.y);
      const QPointF global(viewport.mapToGlobal(local.toPoint()));
      QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                        Qt::NoModifier);
      QApplication::sendEvent(&viewport, &press);
      QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                          Qt::NoModifier);
      QApplication::sendEvent(&viewport, &release);
    }
    qApp->processEvents();
    tools.cancel_tool();
    const auto* raw = window.session().part_document()->find_sketch(sketch);
    REQUIRE(raw->line_segments().size() == 4);
    const auto catalog = window.session().sketch_constraint_catalog(sketch).value();
    REQUIRE(catalog.constraints().size() == 1);
    CHECK(catalog.constraints().front().source() == SketchConstraintIntentSource::Automatic);
    std::vector<LineSegment> created;
    for (const auto& line : raw->line_segments())
      if (line.id() != SketchEntityId("line.h") && line.id() != SketchEntityId("line.v"))
        created.push_back(line);
    REQUIRE(created.size() == 2);
    CHECK(near(created[0].start(), points[0]));
    CHECK(near(created[0].end(), points[1]));
    CHECK(near(created[1].start(), points[1]));
    CHECK(near(created[1].end(), points[2]));
  }
}

TEST_CASE("Twelve-segment pointer loop keeps every automatic junction after reopening and drag",
          "[gui][shell-sketch][endpoint-coincidence][endpoint-native-workflow]") {
  ShellWindow window;
  show_endpoint_window(window);
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  auto& viewport = window.viewport();
  const auto sketch = *window.active_sketch();
  const std::vector<Point2> picks{{50, 50},  {80, 50},   {110, 50},  {140, 50}, {140, 70},
                                  {140, 90}, {140, 110}, {110, 110}, {80, 110}, {50, 110},
                                  {50, 90},  {50, 70},   {50, 50}};
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  for (const auto point : picks)
    click_sketch_pointer(viewport, visible_endpoint_screen(viewport, point));
  tools.cancel_tool();
  const auto catalog = window.session().sketch_constraint_catalog(sketch).value();
  REQUIRE(catalog.constraints().size() == 12);
  for (const auto& intent : catalog.constraints())
    CHECK(intent.source() == SketchConstraintIntentSource::Automatic);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("long-chain.blcad");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(serialize_sketch_constraint_catalog_to_json(
            window.session().sketch_constraint_catalog(sketch).value())
            .value() == serialize_sketch_constraint_catalog_to_json(catalog).value());
  if (!window.active_sketch())
    reenter_sketch(window, sketch);
  (void)visible_endpoint_screen(viewport, {40, 40});
  press_sketch_pointer(viewport, visible_endpoint_screen(viewport, {50, 50}));
  const auto destination = visible_endpoint_screen(viewport, {40, 40});
  drag_sketch_pointer(viewport, destination);
  release_sketch_pointer(viewport, destination);
  auto analysis = geometry::SketchFinishService::analyze(
      *window.session().part_document(), *window.session().part_document()->find_sketch(sketch));
  REQUIRE(analysis);
  CHECK(analysis.value().regions.size() == 1);
  CHECK(analysis.value().diagnostics.empty());
  capture_endpoint_window(window, "endpoint-twelve-segment-after-drag");
  const auto after = serialize_part_document_to_json(*window.session().part_document()).value();
  REQUIRE(window.undo());
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
}

TEST_CASE("Creation endpoint evidence rejects stale targets and commits a polyline atomically",
          "[gui][sketch-create-basic][endpoint-coincidence][endpoint-evidence]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {50, 50}, {90, 50}, {120, 80}, {120, 110});
  auto& session = window.session();
  const auto sketch = *window.active_sketch();
  GuiSketchWorkbench workbench;
  const auto plane = workbench.plane_view(session, sketch).value();
  const auto topology = topology_of(window);
  const auto target = topology.find_entity("entity/line.h")->points()[1];
  auto controller =
      GuiSketchCreateController::begin(GuiSketchCreateTool::Line, sketch, plane).value();
  bool committed_polyline = false;
  SECTION("A point from a different Sketch is not a matching identity") {
    REQUIRE_FALSE(
        controller.add_pick({{90, 50},
                             GuiSketchSnapKind::Endpoint,
                             GuiSketchCreatePointReference{SketchId("foreign.sketch"), target}}));
    CHECK(controller.picks().empty());
  }
  SECTION("A missing target never falls back to another point at the same position") {
    REQUIRE(controller.add_pick(
        {{90, 50},
         GuiSketchSnapKind::Endpoint,
         GuiSketchCreatePointReference{sketch, SketchPointId("entity/removed/end")}}));
    REQUIRE(controller.add_pick({{70, 100}, GuiSketchSnapKind::None}));
    const auto before = serialize_part_document_to_json(*session.part_document()).value();
    const std::string history(session.undo_label());
    REQUIRE_FALSE(controller.commit(session, workbench));
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
    CHECK(session.undo_label() == history);
  }
  SECTION("Moved source evidence cannot move the picked source back on commit") {
    REQUIRE(controller.add_pick(
        {{90, 50}, GuiSketchSnapKind::Endpoint, GuiSketchCreatePointReference{sketch, target}}));
    REQUIRE(controller.add_pick({{70, 100}, GuiSketchSnapKind::None}));
    REQUIRE(session.commit_part_transaction("Move picked source", [&](PartDocument& part) {
      const auto* source = part.find_sketch(sketch);
      auto replacement = Sketch::create(sketch, source->name(), DatumPlaneId("datum.xy")).value();
      for (const auto& line : source->line_segments()) {
        const auto updated = line.id() == SketchEntityId("line.h")
                                 ? LineSegment::create(line.id(), line.start(), {95, 50}).value()
                                 : line;
        auto added = replacement.add_entity(updated);
        if (!added)
          return added;
      }
      return part.update_sketch(std::move(replacement));
    }));
    const auto before = serialize_part_document_to_json(*session.part_document()).value();
    const std::string history(session.undo_label());
    REQUIRE_FALSE(controller.commit(session, workbench));
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
    CHECK(session.undo_label() == history);
  }
  SECTION("A repeated endpoint creates neither a zero-length line nor a relation") {
    REQUIRE(controller.add_pick(
        {{90, 50}, GuiSketchSnapKind::Endpoint, GuiSketchCreatePointReference{sketch, target}}));
    REQUIRE(controller.add_pick({{90, 50}, GuiSketchSnapKind::None}));
    const auto before = serialize_part_document_to_json(*session.part_document()).value();
    const std::string history(session.undo_label());
    REQUIRE_FALSE(controller.commit(session, workbench));
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
    CHECK(session.undo_label() == history);
  }
  SECTION("Polyline internal and accepted external junctions share one transaction") {
    auto polyline =
        GuiSketchCreateController::begin(GuiSketchCreateTool::Polyline, sketch, plane).value();
    const auto start = topology.find_entity("entity/line.h")->points()[0];
    REQUIRE(polyline.add_pick(
        {{90, 50}, GuiSketchSnapKind::Endpoint, GuiSketchCreatePointReference{sketch, target}}));
    REQUIRE(polyline.add_pick({{70, 100}, GuiSketchSnapKind::None}));
    REQUIRE(polyline.add_pick(
        {{50, 50}, GuiSketchSnapKind::Endpoint, GuiSketchCreatePointReference{sketch, start}}));
    REQUIRE(polyline.commit(session, workbench));
    committed_polyline = true;
    const auto catalog = session.sketch_constraint_catalog(sketch).value();
    REQUIRE(catalog.constraints().size() == 3);
    std::vector<std::pair<std::string, std::string>> pairs;
    for (const auto& intent : catalog.constraints()) {
      REQUIRE(intent.targets().size() == 2);
      CHECK(intent.source() == SketchConstraintIntentSource::Automatic);
      auto first = intent.targets()[0].id(), second = intent.targets()[1].id();
      REQUIRE(first != second);
      if (second < first)
        std::swap(first, second);
      CHECK(std::find(pairs.begin(), pairs.end(), std::pair{first, second}) == pairs.end());
      pairs.emplace_back(first, second);
    }
    const auto geometry = serialize_part_document_to_json(*session.part_document()).value();
    const auto relations = serialize_sketch_constraint_catalog_to_json(catalog).value();
    REQUIRE(session.undo());
    CHECK(session.part_document()->find_sketch(sketch)->line_segments().size() == 2);
    CHECK(session.sketch_constraint_catalog(sketch).value().constraints().empty());
    REQUIRE(session.redo());
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == geometry);
    CHECK(serialize_sketch_constraint_catalog_to_json(
              session.sketch_constraint_catalog(sketch).value())
              .value() == relations);
  }
  CHECK(session.part_document()->find_sketch(sketch)->line_segments().size() ==
        (committed_polyline ? 4 : 2));
  CHECK(session.sketch_constraint_catalog(sketch).value().constraints().size() ==
        (committed_polyline ? 3 : 0));
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
  REQUIRE(window.session().commit_part_transaction("Parameter anlegen", [](PartDocument& part) {
    auto quantity = Quantity::length_mm(25.0, "parameter.laenge");
    if (quantity.has_error())
      return Result<std::size_t>::failure(quantity.error());
    auto parameter =
        Parameter::create_length(ParameterId("parameter.laenge"), "laenge", quantity.value());
    if (parameter.has_error())
      return Result<std::size_t>::failure(parameter.error());
    return part.add_parameter(std::move(parameter.value()));
  }));
  select_first_entity(window);
  REQUIRE(
      tools.apply_dimension(SketchDimensionKind::Length, false, ParameterId("parameter.laenge")));
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
  INFO("selected: " << window.session().selection().entries().front().semantic_id << " candidate: "
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
  // Rectangles commit as four line entities and one selectable ClosedProfile.
  CHECK(sketch->line_segments().size() == 4U);
  REQUIRE(sketch->closed_profiles().size() == 1U);

  // The viewport scene renders both materialized profiles under profile ids.
  REQUIRE(window.session().part_shape_cache() != nullptr);
  auto scene =
      geometry::ViewportSceneBuilder{}.build_part(*part, *window.session().part_shape_cache());
  REQUIRE(scene);
  const std::string prefix = "sketch/" + window.active_sketch()->value() + "/profile/";
  CHECK(std::count_if(scene.value().begin(), scene.value().end(), [&prefix](const auto& item) {
          return item.semantic_id.rfind(prefix, 0U) == 0U;
        }) == 2);
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
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
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
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
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
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  const auto* trimmed = sketch->find_line_segment(SketchEntityId("line.h"));
  REQUIRE(trimmed != nullptr);
  CHECK(near(trimmed->end(), {4.0, 0.0}));
}

TEST_CASE("Ändern: trim accepts spline entities through the shell",
          "[gui][shell-sketch][gui][sketch-modify]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  const SketchId sketch_id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed spline", [sketch_id](PartDocument& part) -> Result<std::size_t> {
        const Sketch* source = part.find_sketch(sketch_id);
        if (source == nullptr)
          return Result<std::size_t>::failure(Error::validation(sketch_id.value(), "no sketch"));
        Sketch candidate = *source;
        auto spline = SplineSegment::create_cubic_bezier(
            SketchEntityId("spline.trim"), {0.0, 0.0}, {3.0, 0.0}, {7.0, 0.0}, {10.0, 0.0});
        if (spline.has_error()) return Result<std::size_t>::failure(spline.error());
        auto added = candidate.add_entity(std::move(spline.value()));
        if (added.has_error()) return added;
        return part.update_sketch(std::move(candidate));
      }));

  REQUIRE(tools.apply_modify_pick(SketchModifyKind::Trim, "spline.trim", {5.0, 0.0}));
  const Sketch* trimmed = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  CHECK(trimmed->spline_segments().empty());

  REQUIRE(window.undo());
  const Sketch* restored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(restored != nullptr);
  CHECK(restored->spline_segments().size() == 1U);
}

TEST_CASE("Ändern: trim removes a generated rectangle edge and stale intents atomically",
          "[gui][shell-sketch][gui][sketch-modify][profile-trim]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  const SketchId sketch_id = *window.active_sketch();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 20.0}));

  const Sketch* rectangle = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(rectangle != nullptr);
  REQUIRE(rectangle->line_segments().size() == 4U);
  REQUIRE(rectangle->closed_profiles().size() == 1U);
  const SketchEntityId removed_line = rectangle->closed_profiles().front().line_segments()[0];
  const SketchEntityId stable_line = rectangle->closed_profiles().front().line_segments()[2];
  const auto removed_target =
      SketchConstraintIntentTarget::entity("entity/" + removed_line.value());
  const auto stable_target = SketchConstraintIntentTarget::entity("entity/" + stable_line.value());
  REQUIRE(removed_target);
  REQUIRE(stable_target);
  auto removed_horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.rectangle.removed"), SketchSolverConstraintKind::Horizontal,
      {removed_target.value()});
  auto stable_horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.rectangle.stable"), SketchSolverConstraintKind::Horizontal,
      {stable_target.value()});
  auto removed_length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.rectangle.removed"), SketchDimensionKind::Length,
      SketchDimensionMode::Reference, {removed_target.value()});
  auto stable_length = SketchDimensionIntent::create(
      SketchDimensionId("dimension.rectangle.stable"), SketchDimensionKind::Length,
      SketchDimensionMode::Reference, {stable_target.value()});
  REQUIRE(removed_horizontal);
  REQUIRE(stable_horizontal);
  REQUIRE(removed_length);
  REQUIRE(stable_length);

  const std::size_t generated_constraint_count =
      window.session().sketch_constraint_catalog(sketch_id).value().constraints().size();
  REQUIRE(window.session().commit_part_sketch_intent_transaction(
      "Seed rectangle trim intents",
      [removed_horizontal = std::move(removed_horizontal.value()),
       stable_horizontal = std::move(stable_horizontal.value()),
       removed_length = std::move(removed_length.value()),
       stable_length = std::move(stable_length.value()), sketch_id](
          PartDocument& part, std::vector<SketchConstraintCatalog>& constraint_catalogs,
          std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable
          -> Result<std::size_t> {
        auto constraint_entry = std::find_if(
            constraint_catalogs.begin(), constraint_catalogs.end(),
            [&sketch_id](const auto& catalog) { return catalog.sketch() == sketch_id; });
        if (constraint_entry == constraint_catalogs.end()) {
          auto catalog = SketchConstraintCatalog::create(part.id(), sketch_id);
          if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
          constraint_catalogs.push_back(std::move(catalog.value()));
          constraint_entry = std::prev(constraint_catalogs.end());
        }
        auto added = constraint_entry->add(std::move(removed_horizontal));
        if (added.has_error()) return Result<std::size_t>::failure(added.error());
        added = constraint_entry->add(std::move(stable_horizontal));
        if (added.has_error()) return Result<std::size_t>::failure(added.error());

        auto dimension_entry = std::find_if(
            dimension_catalogs.begin(), dimension_catalogs.end(),
            [&sketch_id](const auto& catalog) { return catalog.sketch() == sketch_id; });
        if (dimension_entry == dimension_catalogs.end()) {
          auto catalog = SketchDimensionCatalog::create(part.id(), sketch_id);
          if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
          dimension_catalogs.push_back(std::move(catalog.value()));
          dimension_entry = std::prev(dimension_catalogs.end());
        }
        added = dimension_entry->add(std::move(removed_length));
        if (added.has_error()) return Result<std::size_t>::failure(added.error());
        added = dimension_entry->add(std::move(stable_length));
        if (added.has_error()) return Result<std::size_t>::failure(added.error());
        return Result<std::size_t>::success(4U);
      }));

  rectangle = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(rectangle != nullptr);
  const LineSegment* target = rectangle->find_line_segment(removed_line);
  REQUIRE(target != nullptr);
  const Point2 pick{(target->start().x + target->end().x) * 0.5,
                    (target->start().y + target->end().y) * 0.5};
  REQUIRE(tools.apply_modify_pick(SketchModifyKind::Trim, removed_line.value(), pick));

  const Sketch* trimmed = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  CHECK(trimmed->line_segments().size() == 3U);
  CHECK(trimmed->closed_profiles().empty());
  CHECK(trimmed->find_line_segment(stable_line) != nullptr);
  const auto constraints_after = window.session().sketch_constraint_catalog(sketch_id);
  const auto dimensions_after = window.session().sketch_dimension_catalog(sketch_id);
  REQUIRE(constraints_after);
  REQUIRE(dimensions_after);
  CHECK(constraints_after.value().find(SketchConstraintId("constraint.rectangle.removed")) ==
        nullptr);
  CHECK(constraints_after.value().find(SketchConstraintId("constraint.rectangle.stable")) !=
        nullptr);
  CHECK(dimensions_after.value().find(SketchDimensionId("dimension.rectangle.removed")) == nullptr);
  CHECK(dimensions_after.value().find(SketchDimensionId("dimension.rectangle.stable")) != nullptr);

  // Geometry, profile removal, and both intent catalogs are one undo record.
  REQUIRE(window.undo());
  const Sketch* restored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(restored != nullptr);
  CHECK(restored->line_segments().size() == 4U);
  CHECK(restored->closed_profiles().size() == 1U);
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value().constraints().size() ==
        generated_constraint_count + 2U);
  CHECK(window.session().sketch_dimension_catalog(sketch_id).value().dimensions().size() == 2U);
}

TEST_CASE("Ändern: trim removes one generated slot arc and its profile container",
          "[gui][shell-sketch][gui][sketch-modify][profile-trim]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  const SketchId sketch_id = *window.active_sketch();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterSlot));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  REQUIRE(tools.add_pick({0.0, 5.0}));

  const Sketch* slot = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(slot != nullptr);
  REQUIRE(slot->line_segments().size() == 2U);
  REQUIRE(slot->arc_segments().size() == 2U);
  REQUIRE(slot->arc_closed_profiles().size() == 1U);
  const SketchEntityId target_id = slot->arc_segments().front().id();
  const Point2 pick = slot->arc_segments().front().mid();
  REQUIRE(tools.apply_modify_pick(SketchModifyKind::Trim, target_id.value(), pick));

  const Sketch* trimmed = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  CHECK(trimmed->line_segments().size() == 2U);
  CHECK(trimmed->arc_segments().size() == 1U);
  CHECK(trimmed->arc_closed_profiles().empty());

  REQUIRE(window.undo());
  const Sketch* restored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(restored != nullptr);
  CHECK(restored->line_segments().size() == 2U);
  CHECK(restored->arc_segments().size() == 2U);
  CHECK(restored->arc_closed_profiles().size() == 1U);
}

TEST_CASE("Ändern: trim turns a picked circle profile into its surviving arc",
          "[gui][shell-sketch][gui][sketch-modify][profile-trim]") {
  ShellWindow window;
  enter_sketch(window);
  auto& tools = window.sketch_tools();
  const SketchId sketch_id = *window.active_sketch();

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({10.0, 0.0}));
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, -20.0}));
  REQUIRE(tools.add_pick({0.0, 20.0}));

  const Sketch* source = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(source != nullptr);
  REQUIRE(source->circle_profiles().size() == 1U);
  REQUIRE(source->line_segments().size() == 1U);

  // Exercise the modal viewport-hit path as a user would. Forty-five degrees
  // lies on an exact interaction-scene sample but not on a quadrant point, so
  // the hit resolves to the CircleProfile curve rather than a snap point.
  tools.begin_modify_tool(SketchModifyKind::Trim);
  const double diagonal = std::sqrt(50.0);
  const auto screen = window.viewport().sketch_plane_to_screen({diagonal, diagonal});
  REQUIRE(screen);
  click_sketch_pointer(window.viewport(), screen.value());
  REQUIRE(window.viewport().hovered_sketch_hit().has_value());
  INFO("hover semantic: " << window.viewport().hovered_sketch_hit()->semantic_id);
  INFO("hover kind: " << to_string(window.viewport().hovered_sketch_hit()->kind));

  const Sketch* trimmed = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(trimmed != nullptr);
  INFO("modify status: " << window.statusBar()->currentMessage().toStdString());
  CHECK(trimmed->circle_profiles().empty());
  CHECK(trimmed->line_segments().size() == 1U);
  REQUIRE(trimmed->arc_segments().size() == 1U);
  const ArcSegment& remainder = trimmed->arc_segments().front();
  const bool endpoints_ok =
      (near(remainder.start(), {0.0, 10.0}) && near(remainder.end(), {0.0, -10.0})) ||
      (near(remainder.start(), {0.0, -10.0}) && near(remainder.end(), {0.0, 10.0}));
  CHECK(endpoints_ok);
  CHECK(near(remainder.mid(), {-10.0, 0.0}));

  REQUIRE(window.undo());
  const Sketch* restored = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(restored != nullptr);
  CHECK(restored->circle_profiles().size() == 1U);
  CHECK(restored->arc_segments().empty());
  CHECK(restored->line_segments().size() == 1U);
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
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 3U); // line.h split in two + untouched second line
}

TEST_CASE("Ändern: trim accepts clicks and an LMB stroke without retrimming a crossing",
          "[gui][shell-sketch][sketch-modify][trim-stroke]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  enter_sketch(window);
  seed_lines(window, {{"stroke.low", {0.0, -10.0}, {16.0, -10.0}},
                      {"stroke.middle", {0.0, 0.0}, {16.0, 0.0}},
                      {"stroke.high", {0.0, 10.0}, {16.0, 10.0}},
                      {"stroke.boundary", {8.0, -16.0}, {8.0, 16.0}}});
  auto& tools = window.sketch_tools();
  auto& viewport = window.viewport();

  tools.begin_modify_tool(SketchModifyKind::Trim);
  REQUIRE(tools.modify_tool_active());
  CHECK_FALSE(viewport.sketch_selection_enabled());

  // The paint gesture may begin in empty space. One coalesced move then crosses
  // two thin curves; both must still be found along the travelled segment.
  auto screen = viewport.sketch_plane_to_screen({15.0, -20.0});
  REQUIRE(screen);
  press_sketch_pointer(viewport, screen.value());

  const SketchId sketch_id = *window.active_sketch();
  const Sketch* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  const LineSegment* low = sketch->find_line_segment(SketchEntityId("stroke.low"));
  REQUIRE(low != nullptr);
  CHECK(near(low->end(), {16.0, -10.0}));

  // Crossing both curves while LMB remains held trims them immediately. A
  // repeated move at the same place must not create another modification.
  screen = viewport.sketch_plane_to_screen({15.0, 0.0});
  REQUIRE(screen);
  drag_sketch_pointer(viewport, screen.value());
  drag_sketch_pointer(viewport, screen.value());

  sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  low = sketch->find_line_segment(SketchEntityId("stroke.low"));
  const LineSegment* middle = sketch->find_line_segment(SketchEntityId("stroke.middle"));
  REQUIRE(low != nullptr);
  REQUIRE(middle != nullptr);
  CHECK(near(low->end(), {8.0, -10.0}));
  CHECK(near(middle->end(), {8.0, 0.0}));
  CHECK(sketch->line_segments().size() == 4U);
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value().constraints().size() == 2U);
  CHECK(automatic_point_on_object_count(window, "entity/stroke.low", {8.0, -10.0},
                                        "entity/stroke.boundary") == 1U);
  CHECK(automatic_point_on_object_count(window, "entity/stroke.middle", {8.0, 0.0},
                                        "entity/stroke.boundary") == 1U);

  release_sketch_pointer(viewport, screen.value());
  sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  middle = sketch->find_line_segment(SketchEntityId("stroke.middle"));
  REQUIRE(middle != nullptr);
  CHECK(near(middle->end(), {8.0, 0.0}));
  CHECK(sketch->line_segments().size() == 4U);
  CHECK_FALSE(viewport.sketch_selection_enabled());

  // Release terminates only the stroke. Even a synthetic Move carrying the
  // LMB flag cannot continue it without a fresh press, while the modal Trim
  // tool stays armed and still accepts a normal click.
  screen = viewport.sketch_plane_to_screen({15.0, 10.0});
  REQUIRE(screen);
  drag_sketch_pointer(viewport, screen.value());
  sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  const LineSegment* high = sketch->find_line_segment(SketchEntityId("stroke.high"));
  REQUIRE(high != nullptr);
  CHECK(near(high->end(), {16.0, 10.0}));
  REQUIRE(tools.modify_tool_active());

  click_sketch_pointer(viewport, screen.value());
  sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  high = sketch->find_line_segment(SketchEntityId("stroke.high"));
  REQUIRE(high != nullptr);
  CHECK(near(high->end(), {8.0, 10.0}));
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value().constraints().size() == 3U);
  CHECK(automatic_point_on_object_count(window, "entity/stroke.high", {8.0, 10.0},
                                        "entity/stroke.boundary") == 1U);
  CHECK(tools.modify_tool_active());
}

TEST_CASE("Ändern: trim ignores hover moves and Escape cancels an active stroke",
          "[gui][shell-sketch][sketch-modify][trim-stroke]") {
  ShellWindow window;
  window.show();
  qApp->processEvents();
  enter_sketch(window);
  seed_lines(window, {{"escape.first", {0.0, -5.0}, {16.0, -5.0}},
                      {"escape.second", {0.0, 5.0}, {16.0, 5.0}},
                      {"escape.boundary", {8.0, -10.0}, {8.0, 10.0}}});
  auto& tools = window.sketch_tools();
  auto& viewport = window.viewport();
  const SketchId sketch_id = *window.active_sketch();

  REQUIRE(viewport.sketch_selection_enabled());
  tools.begin_modify_tool(SketchModifyKind::Trim);
  CHECK_FALSE(viewport.sketch_selection_enabled());
  auto second_screen = viewport.sketch_plane_to_screen({13.0, 5.0});
  REQUIRE(second_screen);
  move_sketch_pointer(viewport, second_screen.value());

  const Sketch* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  const LineSegment* second = sketch->find_line_segment(SketchEntityId("escape.second"));
  REQUIRE(second != nullptr);
  CHECK(near(second->end(), {16.0, 5.0}));

  auto first_screen = viewport.sketch_plane_to_screen({13.0, -5.0});
  REQUIRE(first_screen);
  press_sketch_pointer(viewport, first_screen.value());
  press_escape(window);
  CHECK_FALSE(tools.modify_tool_active());
  CHECK(viewport.sketch_selection_enabled());

  second_screen = viewport.sketch_plane_to_screen({13.0, 5.0});
  REQUIRE(second_screen);
  drag_sketch_pointer(viewport, second_screen.value());
  release_sketch_pointer(viewport, second_screen.value());

  sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch != nullptr);
  second = sketch->find_line_segment(SketchEntityId("escape.second"));
  REQUIRE(second != nullptr);
  CHECK(near(second->end(), {16.0, 5.0}));
}

TEST_CASE("Ändern: extend and split do not repeat while LMB is dragged",
          "[gui][shell-sketch][sketch-modify][trim-stroke]") {
  SECTION("Extend remains a single press operation") {
    ShellWindow window;
    window.show();
    qApp->processEvents();
    enter_sketch(window);
    seed_lines(window, {{"extend.first", {0.0, -40.0}, {40.0, -40.0}},
                        {"extend.second", {0.0, 40.0}, {40.0, 40.0}},
                        {"extend.boundary", {80.0, -80.0}, {80.0, 80.0}}});
    auto& tools = window.sketch_tools();
    auto& viewport = window.viewport();

    tools.begin_modify_tool(SketchModifyKind::Extend);
    auto screen = viewport.sketch_plane_to_screen({30.0, -40.0});
    REQUIRE(screen);
    press_sketch_pointer(viewport, screen.value());
    screen = viewport.sketch_plane_to_screen({30.0, 40.0});
    REQUIRE(screen);
    drag_sketch_pointer(viewport, screen.value());
    release_sketch_pointer(viewport, screen.value());

    const Sketch* sketch =
        window.session().part_document()->find_sketch(*window.active_sketch());
    REQUIRE(sketch != nullptr);
    const LineSegment* first = sketch->find_line_segment(SketchEntityId("extend.first"));
    const LineSegment* second = sketch->find_line_segment(SketchEntityId("extend.second"));
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    INFO("extended first=" << first->start().x << ',' << first->start().y << " -> "
                           << first->end().x << ',' << first->end().y);
    if (viewport.hovered_sketch_hit())
      INFO("extend hit=" << viewport.hovered_sketch_hit()->semantic_id);
    INFO("status=" << window.statusBar()->currentMessage().toStdString());
    CHECK(near(first->end(), {80.0, -40.0}));
    CHECK(near(second->end(), {40.0, 40.0}));
  }

  SECTION("Split remains a single press operation") {
    ShellWindow window;
    window.show();
    qApp->processEvents();
    enter_sketch(window);
    seed_lines(window, {{"split.first", {0.0, -40.0}, {120.0, -40.0}},
                        {"split.second", {0.0, 40.0}, {120.0, 40.0}}});
    auto& tools = window.sketch_tools();
    auto& viewport = window.viewport();

    tools.begin_modify_tool(SketchModifyKind::Split);
    auto screen = viewport.sketch_plane_to_screen({80.0, -40.0});
    REQUIRE(screen);
    press_sketch_pointer(viewport, screen.value());
    screen = viewport.sketch_plane_to_screen({80.0, 40.0});
    REQUIRE(screen);
    drag_sketch_pointer(viewport, screen.value());
    release_sketch_pointer(viewport, screen.value());

    const Sketch* sketch =
        window.session().part_document()->find_sketch(*window.active_sketch());
    REQUIRE(sketch != nullptr);
    if (viewport.hovered_sketch_hit())
      INFO("split hit=" << viewport.hovered_sketch_hit()->semantic_id);
    INFO("status=" << window.statusBar()->currentMessage().toStdString());
    CHECK(sketch->line_segments().size() == 3U);
    const LineSegment* second = sketch->find_line_segment(SketchEntityId("split.second"));
    REQUIRE(second != nullptr);
    CHECK(near(second->start(), {0.0, 40.0}));
    CHECK(near(second->end(), {120.0, 40.0}));
  }
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
      gap_start =
          std::min(gap_start, std::hypot(point.x - moved.start().x, point.y - moved.start().y));
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
        return std::hypot(segment.start().x - segment.end().x, segment.start().y - segment.end().y);
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

TEST_CASE("Finish Sketch preserves open construction work alongside a bounded profile",
          "[gui][shell-sketch][sketch-finish]") {
  ShellWindow window;
  enter_sketch(window);
  const SketchId sketch_id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(tools.add_pick({0, 0}));
  REQUIRE(tools.add_pick({20, 10}));
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({40, 0}));
  REQUIRE(tools.add_pick({50, 5}));
  REQUIRE(window.finish_active_sketch());
  const auto* sketch = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(sketch);
  CHECK(sketch->profile_count() == 1U);
  CHECK(sketch->line_segments().size() == 5U);
  REQUIRE(window.feature_tools().begin_extrude());
  window.feature_tools().cancel();
  reenter_sketch(window, sketch_id);
}

TEST_CASE("Finish Sketch still rejects branched and self-intersecting profile topology",
          "[gui][shell-sketch][sketch-finish]") {
  ShellWindow window;
  enter_sketch(window);
  const SketchId sketch_id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed invalid contours", [sketch_id](PartDocument& part) -> Result<std::size_t> {
        Sketch sketch = *part.find_sketch(sketch_id);
        const std::array<Point2, 4> corners{{{0, 0}, {20, 10}, {0, 10}, {20, 0}}};
        for (std::size_t i = 0; i < corners.size(); ++i) {
          auto line = LineSegment::create(SketchEntityId("cross." + std::to_string(i)),
                                          corners[i], corners[(i + 1) % corners.size()]);
          if (!line) return Result<std::size_t>::failure(line.error());
          auto added = sketch.add_entity(line.value());
          if (!added) return added;
        }
        return part.update_sketch(std::move(sketch));
      }));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  CHECK_FALSE(window.finish_active_sketch());
  CHECK(window.active_sketch() == sketch_id);
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
}

TEST_CASE("Finish Sketch materializes a mouse-ready curved region with dimensions and atomic history",
          "[gui][shell-sketch][sketch-finish][sketch-curved-regions]") {
  ShellWindow window;
  enter_sketch(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == QStringLiteral("xcb"))
    REQUIRE(window.viewport().native_viewer_available());
  const auto id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::ThreePointArc));
  REQUIRE(tools.add_pick({-10, 0}));
  REQUIRE(tools.add_pick({0, 10}));
  REQUIRE(tools.add_pick({10, 0}));
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({-10, 0}));
  REQUIRE(tools.add_pick({10, 0}));
  tools.cancel_tool();
  const auto arc_id = window.session().part_document()->find_sketch(id)->arc_segments().front().id();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/" + arc_id.value()}));
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Radius, true));
  const auto dimensions = window.session().sketch_dimension_catalog(id).value();
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  const auto* source = window.session().part_document()->find_sketch(id);
  auto analysis = geometry::SketchFinishService::analyze_unmaterialized(*window.session().part_document(), *source);
  REQUIRE(analysis);
  INFO((analysis.value().diagnostics.empty() ? "No diagnostics" : analysis.value().diagnostics.front().message));
  REQUIRE(analysis.value().regions.size() == 1);
  const auto profile = analysis.value().regions.front().id;
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*window.session().part_document(), *window.session().part_shape_cache());
  REQUIRE(scene);
  const auto semantic = "sketch/" + id.value() + "/profile/" + profile.value();
  REQUIRE(std::any_of(scene.value().begin(), scene.value().end(), [&](const auto& item) {
    return item.semantic_id == semantic;
  }));
  REQUIRE(window.finish_active_sketch());
  const auto* completed = window.session().part_document()->find_sketch(id);
  REQUIRE(completed->arc_closed_profiles().size() == 1);
  CHECK(completed->arc_closed_profiles().front().id() == profile);
  CHECK(completed->find_arc_segment(arc_id)->start().x == -10);
  CHECK(window.session().sketch_dimension_catalog(id).value() == dimensions);
  const auto after = serialize_part_document_to_json(*window.session().part_document()).value();
  REQUIRE(window.undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("curved.blcad.json").toStdString();
  REQUIRE(window.save_to(QString::fromStdString(path)));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
  CHECK(reopened.sketch_dimension_catalog(id).value() == dimensions);
  if (window.active_sketch())
    REQUIRE(window.finish_active_sketch());
  REQUIRE(window.viewport().select_semantic(semantic));
  const auto begun = window.feature_tools().begin_extrude();
  INFO((begun ? std::string("Extrude started") : begun.error().message()));
  REQUIRE(begun);
  window.feature_tools().cancel();
}

TEST_CASE("Finish Sketch rejects a hidden spline crossing while preserving all intent",
          "[gui][shell-sketch][sketch-finish][sketch-curved-regions]") {
  ShellWindow window;
  enter_sketch(window);
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction("Seed crossing spline", [id](PartDocument& part) {
    auto sketch = *part.find_sketch(id);
    for (const auto& line : {
           LineSegment::create(SketchEntityId("base"), {0, 0}, {10, 0}).value(),
           LineSegment::create(SketchEntityId("left"), {0, 10}, {0, 0}).value(),
           LineSegment::create(SketchEntityId("right"), {10, 0}, {10, 10}).value()}) {
      auto added = sketch.add_entity(line);
      if (!added) return added;
    }
    auto added = sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("spline"),
        {10, 10}, {10, -20}, {0, -20}, {0, 10}).value());
    if (!added) return added;
    return part.update_sketch(std::move(sketch));
  }));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  const auto history = std::string(window.session().undo_label());
  CHECK_FALSE(window.finish_active_sketch());
  CHECK(window.active_sketch() == id);
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  CHECK(window.session().undo_label() == history);
}

TEST_CASE("Corner geometry, relations and size share one undo transaction",
          "[gui][sketch-modify][sketch-modify-expanded]") {
  ShellWindow window;
  enter_sketch(window);
  seed_two_lines(window, {0, 0}, {20, 0}, {20, 0}, {20, 10});
  const auto id = *window.active_sketch();
  const auto before = serialize_part_document_to_json(*window.session().part_document());
  REQUIRE(before);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.h"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line.v"}));
  SECTION("fillet") { REQUIRE(window.sketch_tools().apply_fillet(2)); }
  SECTION("chamfer") { REQUIRE(window.sketch_tools().apply_chamfer(2)); }
  auto dimensions = window.session().sketch_dimension_catalog(id);
  REQUIRE(dimensions);
  REQUIRE(dimensions.value().dimensions().size() == 1U);
  const auto after = serialize_part_document_to_json(*window.session().part_document());
  REQUIRE(after);
  REQUIRE(window.session().undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before.value());
  CHECK(window.session().sketch_constraint_catalog(id).value().constraints().empty());
  CHECK(window.session().sketch_dimension_catalog(id).value().dimensions().empty());
  REQUIRE(window.session().redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after.value());
  CHECK(window.session().sketch_dimension_catalog(id).value().dimensions().size() == 1U);
}

TEST_CASE("Curved fillet commits persistent tangency and rolls back an oversized request",
          "[gui][sketch-modify][sketch-modify-expanded]") {
  ShellWindow window;
  enter_sketch(window);
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction("Seed curved corner",
      [id](PartDocument& part) -> Result<std::size_t> {
        Sketch sketch = *part.find_sketch(id);
        auto line = LineSegment::create(SketchEntityId("first"), {0, -10}, {0, 0});
        auto arc = ArcSegment::create_three_point(SketchEntityId("second"), {0, 0},
                                                   {7.0710678118654755, 2.9289321881345245}, {10, 10});
        if (!line) return Result<std::size_t>::failure(line.error());
        if (!arc) return Result<std::size_t>::failure(arc.error());
        auto added = sketch.add_entity(line.value());
        if (!added) return added;
        added = sketch.add_entity(arc.value());
        if (!added) return added;
        return part.update_sketch(sketch);
      }));
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/first"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/second"}));
  const auto before = serialize_part_document_to_json(*window.session().part_document()).value();
  CHECK_FALSE(window.sketch_tools().apply_chamfer(100));
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
  REQUIRE(window.sketch_tools().apply_fillet(1));
  const auto catalog = window.session().sketch_constraint_catalog(id).value();
  CHECK(std::count_if(catalog.constraints().begin(), catalog.constraints().end(), [](const auto& c) {
    return c.kind() == SketchSolverConstraintKind::Tangent;
  }) == 2);
  REQUIRE(window.session().undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before);
}
