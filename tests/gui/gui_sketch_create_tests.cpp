#include "blcad/gui/gui_sketch_create.hpp"
#include "blcad/gui/gui_sketch_interaction_binder.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/main_window.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QMouseEvent>
#include <QTreeWidget>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

GuiSketchPlaneView xy_plane() {
  return {DatumPlaneId("datum.xy"),
          {0.0, 0.0, 0.0},
          {1.0, 0.0, 0.0},
          {0.0, 1.0, 0.0},
          {0.0, 0.0, 1.0}};
}

void seed_sketch_part(GuiDocumentSession& session, GuiSketchWorkbench& workbench,
                      const char* part_id, const char* sketch_id) {
  REQUIRE(session.create_part(DocumentId(part_id), "Create Part"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(
      session,
      Sketch::create(SketchId(sketch_id), "Create Sketch", DatumPlaneId("datum.xy")).value()));
}

GuiSketchCreateController controller_for(GuiSketchCreateTool tool, const char* sketch_id,
                                         GuiSketchCreateOptions options = {}) {
  auto controller =
      GuiSketchCreateController::begin(tool, SketchId(sketch_id), xy_plane(), options);
  REQUIRE(controller);
  return std::move(controller.value());
}

GuiSketchCreatePick pick(double x, double y,
                         GuiSketchSnapKind snap = GuiSketchSnapKind::None) {
  return {{x, y}, snap};
}

QTreeWidgetItem* find_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item(item->child(index), id))
      return found;
  return nullptr;
}

} // namespace

TEST_CASE("Block 111 line tool expands two picks into one persistent line transaction",
          "[gui][sketch-create-basic]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_sketch_part(session, workbench, "part.create_line", "sketch.create");

  auto controller = controller_for(GuiSketchCreateTool::Line, "sketch.create");
  REQUIRE(controller.required_picks() == 2U);
  REQUIRE_FALSE(controller.complete_ready());
  REQUIRE(controller.add_pick(pick(0.0, 0.0)));
  REQUIRE(controller.add_pick(pick(20.0, 5.0)));
  REQUIRE(controller.complete_ready());
  REQUIRE_FALSE(controller.accepts_more_picks());

  auto expansion = controller.expansion(*session.part_document());
  REQUIRE(expansion);
  REQUIRE(expansion.value().lines.size() == 1U);
  CHECK(expansion.value().lines.front().id().value() == "line.create.1");
  CHECK_FALSE(expansion.value().profile.has_value());

  REQUIRE(controller.commit(session, workbench));
  const Sketch* sketch = session.part_document()->find_sketch(SketchId("sketch.create"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->line_segments().size() == 1U);
  CHECK(sketch->line_segments().front().start() == Point2{0.0, 0.0});
  CHECK(sketch->line_segments().front().end() == Point2{20.0, 5.0});
  CHECK(session.undo_label() == "Create sketch line");
  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.create"))->line_segments().empty());
}

TEST_CASE("Block 111 polyline expands ordered picks into chained distinct-endpoint lines",
          "[gui][sketch-create-basic]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_sketch_part(session, workbench, "part.create_polyline", "sketch.create");

  auto controller = controller_for(GuiSketchCreateTool::Polyline, "sketch.create");
  REQUIRE(controller.add_pick(pick(0.0, 0.0)));
  REQUIRE(controller.add_pick(pick(10.0, 0.0)));
  REQUIRE(controller.add_pick(pick(10.0, 8.0)));
  CHECK(controller.accepts_more_picks());
  REQUIRE(controller.complete_ready());

  REQUIRE(controller.commit(session, workbench));
  const Sketch* sketch = session.part_document()->find_sketch(SketchId("sketch.create"));
  REQUIRE(sketch->line_segments().size() == 2U);
  CHECK(sketch->line_segments()[0].end() == sketch->line_segments()[1].start());
  CHECK(sketch->line_segments()[0].id().value() != sketch->line_segments()[1].id().value());

  auto degenerate = controller_for(GuiSketchCreateTool::Polyline, "sketch.create");
  REQUIRE(degenerate.add_pick(pick(1.0, 1.0)));
  REQUIRE(degenerate.add_pick(pick(1.0, 1.0)));
  CHECK_FALSE(degenerate.expansion(*session.part_document()));
}

TEST_CASE("Block 111 rectangle families expand into four lines plus one closed profile",
          "[gui][sketch-create-basic]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_sketch_part(session, workbench, "part.create_rect", "sketch.create");

  SECTION("corner rectangle") {
    auto controller = controller_for(GuiSketchCreateTool::CornerRectangle, "sketch.create");
    REQUIRE(controller.add_pick(pick(0.0, 0.0)));
    REQUIRE(controller.add_pick(pick(30.0, 20.0)));
    auto expansion = controller.expansion(*session.part_document());
    REQUIRE(expansion);
    REQUIRE(expansion.value().lines.size() == 4U);
    REQUIRE(expansion.value().profile.has_value());
    CHECK(expansion.value().profile->line_segments().size() == 4U);
    REQUIRE(controller.commit(session, workbench));
    const Sketch* sketch = session.part_document()->find_sketch(SketchId("sketch.create"));
    CHECK(sketch->line_segments().size() == 4U);
    CHECK(sketch->closed_profiles().size() == 1U);
  }
  SECTION("degenerate corner rectangle fails closed") {
    auto controller = controller_for(GuiSketchCreateTool::CornerRectangle, "sketch.create");
    REQUIRE(controller.add_pick(pick(0.0, 0.0)));
    REQUIRE(controller.add_pick(pick(0.0, 20.0)));
    CHECK_FALSE(controller.expansion(*session.part_document()));
  }
  SECTION("center rectangle") {
    auto controller = controller_for(GuiSketchCreateTool::CenterRectangle, "sketch.create");
    REQUIRE(controller.add_pick(pick(5.0, 5.0)));
    REQUIRE(controller.add_pick(pick(15.0, 10.0)));
    auto expansion = controller.expansion(*session.part_document());
    REQUIRE(expansion);
    const auto& corner = expansion.value().lines.front().start();
    CHECK(corner.x == Catch::Approx(-5.0));
    CHECK(corner.y == Catch::Approx(0.0));
  }
  SECTION("three-point rectangle") {
    auto controller = controller_for(GuiSketchCreateTool::ThreePointRectangle, "sketch.create");
    REQUIRE(controller.add_pick(pick(0.0, 0.0)));
    REQUIRE(controller.add_pick(pick(10.0, 0.0)));
    REQUIRE(controller.add_pick(pick(3.0, 7.0)));
    auto expansion = controller.expansion(*session.part_document());
    REQUIRE(expansion);
    CHECK(expansion.value().lines[2].start() == Point2{10.0, 7.0});
    CHECK(expansion.value().lines[3].start() == Point2{0.0, 7.0});
  }
  SECTION("parallelogram") {
    auto controller = controller_for(GuiSketchCreateTool::Parallelogram, "sketch.create");
    REQUIRE(controller.add_pick(pick(0.0, 0.0)));
    REQUIRE(controller.add_pick(pick(10.0, 0.0)));
    REQUIRE(controller.add_pick(pick(14.0, 6.0)));
    auto expansion = controller.expansion(*session.part_document());
    REQUIRE(expansion);
    CHECK(expansion.value().lines[3].start() == Point2{4.0, 6.0});

    auto collinear = controller_for(GuiSketchCreateTool::Parallelogram, "sketch.create");
    REQUIRE(collinear.add_pick(pick(0.0, 0.0)));
    REQUIRE(collinear.add_pick(pick(5.0, 0.0)));
    REQUIRE(collinear.add_pick(pick(9.0, 0.0)));
    CHECK_FALSE(collinear.expansion(*session.part_document()));
  }
}

TEST_CASE("Block 111 regular polygon uses the numeric side count before the first pick",
          "[gui][sketch-create-basic]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_sketch_part(session, workbench, "part.create_polygon", "sketch.create");

  auto controller = controller_for(GuiSketchCreateTool::RegularPolygon, "sketch.create");
  auto sides_entry = controller.apply_numeric("8", std::nullopt);
  REQUIRE(sides_entry);
  CHECK_FALSE(sides_entry.value().has_value());
  REQUIRE(controller.set_polygon_sides(8U));
  REQUIRE(controller.add_pick(pick(0.0, 0.0)));
  REQUIRE(controller.add_pick(pick(10.0, 0.0)));

  auto expansion = controller.expansion(*session.part_document());
  REQUIRE(expansion);
  CHECK(expansion.value().lines.size() == 8U);
  REQUIRE(expansion.value().profile.has_value());
  REQUIRE(controller.commit(session, workbench));
  CHECK(session.part_document()->find_sketch(SketchId("sketch.create"))->line_segments().size() ==
        8U);

  auto degenerate = controller_for(GuiSketchCreateTool::RegularPolygon, "sketch.create");
  REQUIRE(degenerate.add_pick(pick(0.0, 0.0)));
  REQUIRE(degenerate.add_pick(pick(0.0, 0.0)));
  CHECK_FALSE(degenerate.expansion(*session.part_document()));
  CHECK_FALSE(GuiSketchCreateController::begin(GuiSketchCreateTool::RegularPolygon,
                                               SketchId("sketch.create"), xy_plane(),
                                               {.polygon_sides = 2U}));
}

TEST_CASE("Block 111 numeric input supports absolute, relative, and polar picks",
          "[gui][sketch-create-basic]") {
  auto controller = controller_for(GuiSketchCreateTool::Polyline, "sketch.create");

  auto absolute = controller.apply_numeric("5;7", std::nullopt);
  REQUIRE(absolute);
  REQUIRE(absolute.value().has_value());
  CHECK(absolute.value()->point == Point2{5.0, 7.0});
  REQUIRE(controller.add_pick(*absolute.value()));

  auto relative = controller.apply_numeric("10;-2", std::nullopt);
  REQUIRE(relative);
  CHECK(relative.value()->point == Point2{15.0, 5.0});
  REQUIRE(controller.add_pick(*relative.value()));

  auto polar = controller.apply_numeric("10<90", std::nullopt);
  REQUIRE(polar);
  CHECK(polar.value()->point.x == Catch::Approx(15.0).margin(1.0e-9));
  CHECK(polar.value()->point.y == Catch::Approx(15.0).margin(1.0e-9));

  auto toward_hover = controller.apply_numeric("5", Point2{25.0, 5.0});
  REQUIRE(toward_hover);
  CHECK(toward_hover.value()->point == Point2{20.0, 5.0});

  CHECK_FALSE(controller.apply_numeric("abc", std::nullopt));
  CHECK_FALSE(controller.apply_numeric("-4<10", std::nullopt));
  CHECK_FALSE(controller.apply_numeric("5", std::nullopt));
}

TEST_CASE("Block 111 preview publishes rubber band and automatic constraint previews",
          "[gui][sketch-create-basic]") {
  auto controller = controller_for(GuiSketchCreateTool::CornerRectangle, "sketch.create");
  REQUIRE(controller.add_pick(pick(0.0, 0.0, GuiSketchSnapKind::Endpoint)));

  const auto preview =
      controller.preview(pick(12.0, 8.0, GuiSketchSnapKind::HorizontalInference));
  REQUIRE(preview.rubber_band.size() == 4U);
  CHECK(preview.closed);
  REQUIRE(preview.constraints.size() == 2U);
  CHECK(preview.constraints[0].kind == "coincident");
  CHECK(preview.constraints[1].kind == "horizontal");

  auto line = controller_for(GuiSketchCreateTool::Line, "sketch.create");
  REQUIRE(line.add_pick(pick(0.0, 0.0)));
  const auto open_preview = line.preview(pick(4.0, 4.0, GuiSketchSnapKind::Midpoint));
  REQUIRE(open_preview.rubber_band.size() == 2U);
  CHECK_FALSE(open_preview.closed);
  REQUIRE(open_preview.constraints.size() == 1U);
  CHECK(open_preview.constraints[0].kind == "midpoint");

  REQUIRE(line.pop_pick());
  CHECK(line.picks().empty());
  CHECK_FALSE(line.pop_pick());
}

TEST_CASE("Block 111 point and centerline tools create construction geometry",
          "[gui][sketch-create-basic]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_sketch_part(session, workbench, "part.create_construction", "sketch.create");

  auto point_tool = controller_for(GuiSketchCreateTool::Point, "sketch.create");
  REQUIRE(point_tool.add_pick(pick(4.0, -3.0)));
  REQUIRE(point_tool.commit(session, workbench));
  REQUIRE(session.part_document()->construction_points().size() == 1U);
  const auto& created_point = session.part_document()->construction_points().front();
  CHECK(created_point.id().value() == "construction.point.create.1");
  CHECK(created_point.position() == Point3{4.0, -3.0, 0.0});

  auto centerline = controller_for(GuiSketchCreateTool::Centerline, "sketch.create");
  REQUIRE(centerline.add_pick(pick(0.0, 0.0)));
  REQUIRE(centerline.add_pick(pick(0.0, 25.0)));
  REQUIRE(centerline.commit(session, workbench));
  REQUIRE(session.part_document()->construction_lines().size() == 1U);
  const auto& created_line = session.part_document()->construction_lines().front();
  CHECK(created_line.point() == Point3{0.0, 0.0, 0.0});
  CHECK(created_line.direction().x == Catch::Approx(0.0));
  CHECK(created_line.direction().z == Catch::Approx(0.0));
}

TEST_CASE("Block 111 offscreen mouse creation commits a rectangle profile through the binder",
          "[gui][sketch-create-basic][integration][sketch-basic-profile]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  install_sketch_interaction_binder(window);
  seed_sketch_part(window.session(), window.sketch_workbench(), "part.mouse_create",
                   "sketch.mouse");
  window.refresh_command_state();
  window.show();
  qApp->processEvents();

  auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"));
  auto* edit = window.findChild<QAction*>(QStringLiteral("blcad.action.edit_sketch"));
  auto* rectangle_action = window.findChild<QAction*>(
      QStringLiteral("blcad.action.sketch_create_corner_rectangle"));
  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(tree != nullptr);
  REQUIRE(edit != nullptr);
  REQUIRE(rectangle_action != nullptr);
  REQUIRE(viewport != nullptr);

  QTreeWidgetItem* sketch_item = nullptr;
  for (int index = 0; index < tree->topLevelItemCount() && sketch_item == nullptr; ++index)
    sketch_item = find_item(tree->topLevelItem(index), u"sketch.mouse");
  REQUIRE(sketch_item != nullptr);
  tree->setCurrentItem(sketch_item);
  edit->trigger();
  qApp->processEvents();
  REQUIRE(window.active_sketch().has_value());
  REQUIRE(viewport->sketch_interaction_active());
  CHECK(rectangle_action->isEnabled());

  rectangle_action->trigger();
  qApp->processEvents();
  REQUIRE(window.sketch_workspace().stage() == GuiSketchInteractionStage::CollectingPicks);
  CHECK_FALSE(viewport->sketch_selection_enabled());

  const auto click_at = [&](Point2 plane_point) {
    auto screen = viewport->sketch_plane_to_screen(plane_point);
    REQUIRE(screen);
    const QPointF position(screen.value().x, screen.value().y);
    QMouseEvent move(QEvent::MouseMove, position, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &move);
    QMouseEvent press(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(viewport, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(viewport, &release);
    qApp->processEvents();
  };

  click_at({0.0, 0.0});
  CHECK(viewport->sketch_preview_point_count() > 0U);
  click_at({30.0, 20.0});

  const Sketch* sketch =
      window.session().part_document()->find_sketch(SketchId("sketch.mouse"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->line_segments().size() == 4U);
  REQUIRE(sketch->closed_profiles().size() == 1U);
  CHECK(window.session().undo_label() == "Create sketch corner_rectangle");
  CHECK((window.sketch_workspace().stage() == GuiSketchInteractionStage::Idle ||
         window.sketch_workspace().stage() == GuiSketchInteractionStage::Hover));
  CHECK(viewport->sketch_selection_enabled());
  CHECK(viewport->sketch_preview_point_count() == 0U);
  CHECK_FALSE(window.session().task().active());

  auto inspection = window.sketch_workbench().inspect(window.session(), SketchId("sketch.mouse"));
  REQUIRE(inspection);
  CHECK(inspection.value().profile_count == 1U);

  auto serialized = serialize_part_document_to_json(*window.session().part_document());
  REQUIRE(serialized);
  auto reloaded = deserialize_part_document_from_json(serialized.value());
  REQUIRE(reloaded);
  const Sketch* restored = reloaded.value().find_sketch(SketchId("sketch.mouse"));
  REQUIRE(restored != nullptr);
  CHECK(restored->line_segments().size() == 4U);
  CHECK(restored->closed_profiles().size() == 1U);

  REQUIRE(window.session().undo());
  CHECK(window.session()
            .part_document()
            ->find_sketch(SketchId("sketch.mouse"))
            ->line_segments()
            .empty());
  REQUIRE(window.session().redo());
  CHECK(window.session()
            .part_document()
            ->find_sketch(SketchId("sketch.mouse"))
            ->line_segments()
            .size() == 4U);
}
