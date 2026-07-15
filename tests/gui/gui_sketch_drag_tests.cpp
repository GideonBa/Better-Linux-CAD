#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_interaction_binder.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/main_window.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include "blcad/core/parameter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QMouseEvent>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length_parameter(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

SketchReferenceTarget line_target(const char* id) {
  return SketchReferenceTarget::create_line_segment(SketchEntityId(id)).value();
}

SketchReferenceTarget line_start(const char* id) {
  return SketchReferenceTarget::create_line_segment_start(SketchEntityId(id)).value();
}

SketchReferenceTarget line_end(const char* id) {
  return SketchReferenceTarget::create_line_segment_end(SketchEntityId(id)).value();
}

QTreeWidgetItem* find_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item(item->child(index), id))
      return found;
  return nullptr;
}

const GuiSketchDragHandle* find_handle(const GuiSketchDragController& controller,
                                       GuiSketchDragHandleKind kind,
                                       std::string_view entity = {}) {
  const auto found = std::find_if(controller.handles().begin(), controller.handles().end(),
                                  [kind, entity](const auto& handle) {
                                    return handle.kind == kind &&
                                           (entity.empty() || handle.entity_id == entity);
                                  });
  return found == controller.handles().end() ? nullptr : &*found;
}

struct ArcGeometry {
  Point2 center;
  double radius;
};

ArcGeometry arc_geometry(const SketchTopology& topology, std::string_view entity_id) {
  const auto* entity = topology.find_entity(entity_id);
  REQUIRE(entity != nullptr);
  REQUIRE(entity->points().size() == 3U);
  const Point2 a = topology.find_point(entity->points()[0])->position();
  const Point2 b = topology.find_point(entity->points()[1])->position();
  const Point2 c = topology.find_point(entity->points()[2])->position();
  const double denominator =
      2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  REQUIRE(std::abs(denominator) > 1.0e-12);
  const double aa = a.x * a.x + a.y * a.y;
  const double bb = b.x * b.x + b.y * b.y;
  const double cc = c.x * c.x + c.y * c.y;
  const Point2 center{(aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) /
                          denominator,
                      (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) /
                          denominator};
  return {center, std::hypot(a.x - center.x, a.y - center.y)};
}

void seed_drag_line(GuiDocumentSession& session, GuiSketchWorkbench& workbench,
                    const char* part_id, const char* sketch_id, bool fully_constrained) {
  REQUIRE(session.create_part(DocumentId(part_id), "Drag Part"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  if (fully_constrained)
    REQUIRE(session.commit_part_transaction("Seed width", [](PartDocument& part) {
      return part.add_parameter(length_parameter("drag.width", 10.0));
    }));
  REQUIRE(workbench.create_sketch(
      session, Sketch::create(SketchId(sketch_id), "Drag Sketch", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_line(
      session, SketchId(sketch_id),
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {8.0, 2.0}).value()));
  REQUIRE(workbench.add_constraint(
      session, SketchId(sketch_id),
      SketchGeometricConstraint::create_fixed(SketchConstraintId("fixed.start"),
                                               line_start("line.a")).value()));
  REQUIRE(workbench.add_constraint(
      session, SketchId(sketch_id),
      SketchGeometricConstraint::create_horizontal(SketchConstraintId("horizontal.line"),
                                                    line_target("line.a")).value()));
  if (fully_constrained)
    REQUIRE(workbench.add_dimension(
        session, SketchId(sketch_id),
        SketchDrivingDimension::create_horizontal_distance(
            SketchDimensionId("width"), line_start("line.a"), line_end("line.a"),
            ParameterId("drag.width")).value()));
}

} // namespace

TEST_CASE("Block 110 exposes stable semantic handles without duplicating shared endpoints",
          "[gui][sketch-drag]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.handle_catalog"), "Handle Catalog"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(session.commit_part_transaction("Seed dimension", [](PartDocument& part) {
    auto width = part.add_parameter(length_parameter("catalog.width", 10.0));
    if (width.has_error()) return width;
    auto diameter = part.add_parameter(length_parameter("catalog.diameter", 8.0));
    if (diameter.has_error()) return diameter;
    return part.add_parameter(length_parameter("catalog.height", 6.0));
  }));

  auto sketch = Sketch::create(SketchId("sketch.catalog"), "Catalog", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(LineSegment::create(
      SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(LineSegment::create(
      SketchEntityId("line.b"), {10.0, 0.0}, {5.0, 8.0}).value()));
  REQUIRE(sketch.value().add_entity(LineSegment::create(
      SketchEntityId("line.c"), {5.0, 8.0}, {0.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(ArcSegment::create_three_point(
      SketchEntityId("arc.a"), {20.0, 0.0}, {25.0, 5.0}, {30.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {40.0, 0.0}, {42.0, 6.0}, {48.0, 6.0}, {50.0, 0.0}).value()));
  REQUIRE(sketch.value().add_profile(
      ClosedProfile::create(ProfileId("profile.triangle"),
                            {SketchEntityId("line.a"), SketchEntityId("line.b"),
                             SketchEntityId("line.c")}).value()));
  REQUIRE(sketch.value().add_profile(RectangleProfile::create(
      ProfileId("profile.rectangle"), ParameterId("catalog.width"),
      ParameterId("catalog.height"), {60.0, 0.0}).value()));
  REQUIRE(sketch.value().add_profile(CircleProfile::create(
      ProfileId("profile.circle"), ParameterId("catalog.diameter"), {70.0, 0.0}).value()));
  REQUIRE(sketch.value().add_dimension(SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("line.width"), line_start("line.a"), line_end("line.a"),
      ParameterId("catalog.width")).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  auto controller = GuiSketchDragController::create(*session.part_document(), SketchId("sketch.catalog"));
  REQUIRE(controller);
  CHECK(std::is_sorted(controller.value().handles().begin(), controller.value().handles().end(),
                       [](const auto& first, const auto& second) { return first.id < second.id; }));

  const auto count_kind = [&](GuiSketchDragHandleKind kind) {
    return std::count_if(controller.value().handles().begin(), controller.value().handles().end(),
                         [kind](const auto& handle) { return handle.kind == kind; });
  };
  CHECK(count_kind(GuiSketchDragHandleKind::Endpoint) == 7);
  CHECK(count_kind(GuiSketchDragHandleKind::Midpoint) == 3);
  CHECK(count_kind(GuiSketchDragHandleKind::Center) == 3);
  CHECK(count_kind(GuiSketchDragHandleKind::Radius) == 1);
  CHECK(count_kind(GuiSketchDragHandleKind::Arc) == 1);
  CHECK(count_kind(GuiSketchDragHandleKind::Spline) == 2);
  CHECK(count_kind(GuiSketchDragHandleKind::Dimension) == 1);

  const auto* line_a = controller.value().source_topology().find_entity("entity/line.a");
  const auto* line_b = controller.value().source_topology().find_entity("entity/line.b");
  REQUIRE(line_a != nullptr);
  REQUIRE(line_b != nullptr);
  REQUIRE(line_a->points()[1] == line_b->points()[0]);
  const auto shared_id = line_a->points()[1];
  CHECK(std::count_if(controller.value().handles().begin(), controller.value().handles().end(),
                      [&shared_id](const auto& handle) {
                        return handle.kind == GuiSketchDragHandleKind::Endpoint &&
                               handle.point_id == shared_id;
                      }) == 1);
}

TEST_CASE("Block 110 coalesces pointer samples and commits the exact final sample once",
          "[gui][sketch-drag][integration][sketch-live-solve]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_drag_line(session, workbench, "part.coalesced_drag", "sketch.drag", false);
  const Point2 original_end =
      session.part_document()->find_sketch(SketchId("sketch.drag"))
          ->find_line_segment(SketchEntityId("line.a"))->end();

  auto controller = GuiSketchDragController::create(*session.part_document(), SketchId("sketch.drag"));
  REQUIRE(controller);
  CHECK(controller.value().baseline_solve().status == SketchSolveStatus::UnderConstrained);
  CHECK(controller.value().baseline_solve().remaining_dof == 1U);
  const auto* endpoint = std::find_if(
      controller.value().handles().begin(), controller.value().handles().end(), [](const auto& handle) {
        return handle.kind == GuiSketchDragHandleKind::Endpoint && handle.point_id &&
               handle.position == Point2{8.0, 2.0};
      });
  REQUIRE(endpoint != controller.value().handles().end());
  REQUIRE(controller.value().begin(endpoint->id));

  REQUIRE(controller.value().queue_pointer({10.0, 0.0}));
  REQUIRE(controller.value().queue_pointer({12.0, 0.0}));
  REQUIRE(controller.value().queue_pointer({15.0, 0.0}));
  CHECK(controller.value().solve_count() == 0U);
  CHECK(session.part_document()->find_sketch(SketchId("sketch.drag"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);

  auto preview = controller.value().process_pending();
  REQUIRE(preview);
  REQUIRE(controller.value().processed_pointer().has_value());
  CHECK(*controller.value().processed_pointer() == Point2{15.0, 0.0});
  CHECK(controller.value().solve_count() == 1U);
  const auto* preview_line = preview.value().topology().find_entity("entity/line.a");
  REQUIRE(preview_line != nullptr);
  const Point2 preview_end = preview.value().topology().find_point(preview_line->points()[1])->position();
  CHECK(preview_end.x == Catch::Approx(15.0).margin(1.0e-6));
  CHECK(preview_end.y == Catch::Approx(0.0).margin(1.0e-6));
  CHECK(session.part_document()->find_sketch(SketchId("sketch.drag"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);

  auto final_preview = controller.value().flush({20.0, 0.0});
  REQUIRE(final_preview);
  CHECK(*controller.value().processed_pointer() == Point2{20.0, 0.0});
  CHECK(controller.value().solve_count() == 2U);
  REQUIRE(controller.value().commit(session));
  const Point2 committed = session.part_document()->find_sketch(SketchId("sketch.drag"))
                               ->find_line_segment(SketchEntityId("line.a"))->end();
  CHECK(committed.x == Catch::Approx(20.0).margin(1.0e-6));
  CHECK(committed.y == Catch::Approx(0.0).margin(1.0e-6));
  CHECK(session.undo_label() == "Drag sketch handle");

  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.drag"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);
  REQUIRE(session.redo());
  const Point2 redone = session.part_document()->find_sketch(SketchId("sketch.drag"))
                            ->find_line_segment(SketchEntityId("line.a"))->end();
  CHECK(redone.x == Catch::Approx(20.0).margin(1.0e-6));
  CHECK(redone.y == Catch::Approx(0.0).margin(1.0e-6));
}

TEST_CASE("Block 110 cancels previews and refuses incompatible fully constrained drags",
          "[gui][sketch-drag][integration][sketch-live-solve]") {
  SECTION("cancel restores the pre-drag document") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    seed_drag_line(session, workbench, "part.cancel_drag", "sketch.cancel", false);
    const Point2 before = session.part_document()->find_sketch(SketchId("sketch.cancel"))
                              ->find_line_segment(SketchEntityId("line.a"))->end();
    auto controller =
        GuiSketchDragController::create(*session.part_document(), SketchId("sketch.cancel"));
    REQUIRE(controller);
    const auto endpoint = std::find_if(
        controller.value().handles().begin(), controller.value().handles().end(), [](const auto& handle) {
          return handle.kind == GuiSketchDragHandleKind::Endpoint &&
                 handle.position == Point2{8.0, 2.0};
        });
    REQUIRE(endpoint != controller.value().handles().end());
    REQUIRE(controller.value().begin(endpoint->id));
    REQUIRE(controller.value().flush({16.0, 0.0}));
    controller.value().cancel();
    CHECK_FALSE(controller.value().active());
    CHECK(session.part_document()->find_sketch(SketchId("sketch.cancel"))
              ->find_line_segment(SketchEntityId("line.a"))->end() == before);
  }

  SECTION("fixed and dimensioned geometry refuses an incompatible pointer") {
    GuiDocumentSession session;
    GuiSketchWorkbench workbench;
    seed_drag_line(session, workbench, "part.fixed_drag", "sketch.fixed", true);
    const Point2 before = session.part_document()->find_sketch(SketchId("sketch.fixed"))
                              ->find_line_segment(SketchEntityId("line.a"))->end();
    auto controller =
        GuiSketchDragController::create(*session.part_document(), SketchId("sketch.fixed"));
    REQUIRE(controller);
    CHECK(controller.value().baseline_solve().status == SketchSolveStatus::FullyConstrained);
    const auto endpoint = std::find_if(
        controller.value().handles().begin(), controller.value().handles().end(), [](const auto& handle) {
          return handle.kind == GuiSketchDragHandleKind::Endpoint &&
                 handle.position == Point2{8.0, 2.0};
        });
    REQUIRE(endpoint != controller.value().handles().end());
    REQUIRE(controller.value().begin(endpoint->id));
    auto refused = controller.value().flush({20.0, 0.0});
    CHECK_FALSE(refused);
    CHECK_FALSE(controller.value().latest_preview().has_value());
    CHECK_FALSE(controller.value().commit(session));
    CHECK(session.part_document()->find_sketch(SketchId("sketch.fixed"))
              ->find_line_segment(SketchEntityId("line.a"))->end() == before);
  }
}

TEST_CASE("Block 110 arc center and radius handles use Block 109 solver target families",
          "[gui][sketch-drag][integration][sketch-live-solve]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.arc_drag"), "Arc Drag"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(
      session, Sketch::create(SketchId("sketch.arc"), "Arc", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_arc(
      session, SketchId("sketch.arc"),
      ArcSegment::create_three_point(SketchEntityId("arc.a"), {5.0, 0.0}, {0.0, 5.0}, {-5.0, 0.0})
          .value()));

  auto radius_controller =
      GuiSketchDragController::create(*session.part_document(), SketchId("sketch.arc"));
  REQUIRE(radius_controller);
  const auto* radius = find_handle(radius_controller.value(), GuiSketchDragHandleKind::Radius,
                                   "entity/arc.a");
  REQUIRE(radius != nullptr);
  REQUIRE(radius_controller.value().begin(radius->id));
  auto radius_preview = radius_controller.value().flush({7.0, 0.0});
  REQUIRE(radius_preview);
  const auto resized = arc_geometry(radius_preview.value().topology(), "entity/arc.a");
  CHECK(resized.radius == Catch::Approx(7.0).margin(1.0e-5));
  radius_controller.value().cancel();

  auto center_controller =
      GuiSketchDragController::create(*session.part_document(), SketchId("sketch.arc"));
  REQUIRE(center_controller);
  const auto* center = find_handle(center_controller.value(), GuiSketchDragHandleKind::Center,
                                   "entity/arc.a");
  REQUIRE(center != nullptr);
  REQUIRE(center_controller.value().begin(center->id));
  auto center_preview = center_controller.value().flush({2.0, 3.0});
  REQUIRE(center_preview);
  const auto moved = arc_geometry(center_preview.value().topology(), "entity/arc.a");
  CHECK(moved.center.x == Catch::Approx(2.0).margin(1.0e-5));
  CHECK(moved.center.y == Catch::Approx(3.0).margin(1.0e-5));
}

TEST_CASE("Block 110 offscreen mouse drag publishes live solve and one release transaction",
          "[gui][sketch-drag][integration][sketch-live-solve]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  install_sketch_interaction_binder(window);
  seed_drag_line(window.session(), window.sketch_workbench(), "part.mouse_drag", "sketch.mouse", false);
  window.refresh_command_state();
  window.show();
  qApp->processEvents();

  auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"));
  auto* edit = window.findChild<QAction*>(QStringLiteral("blcad.action.edit_sketch"));
  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(tree != nullptr);
  REQUIRE(edit != nullptr);
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
  REQUIRE(viewport->sketch_drag_handle_count() > 0U);

  auto start_screen = viewport->sketch_plane_to_screen({8.0, 2.0});
  auto end_screen = viewport->sketch_plane_to_screen({20.0, 0.0});
  REQUIRE(start_screen);
  REQUIRE(end_screen);

  QMouseEvent press(QEvent::MouseButtonPress,
                    QPointF(start_screen.value().x, start_screen.value().y), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(viewport, &press);
  REQUIRE(window.sketch_workspace().stage() == GuiSketchInteractionStage::SelectedHandle);

  QMouseEvent move(QEvent::MouseMove, QPointF(end_screen.value().x, end_screen.value().y),
                   Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(viewport, &move);
  qApp->processEvents();
  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::DragCandidate);
  CHECK(window.sketch_workspace().status().remaining_dof == 0U);
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.mouse"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == Point2{8.0, 2.0});

  QMouseEvent release(QEvent::MouseButtonRelease,
                      QPointF(end_screen.value().x, end_screen.value().y), Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(viewport, &release);
  qApp->processEvents();

  CHECK(window.sketch_workspace().stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(window.session().task().active());
  const Point2 committed = window.session().part_document()->find_sketch(SketchId("sketch.mouse"))
                               ->find_line_segment(SketchEntityId("line.a"))->end();
  CHECK(committed.x == Catch::Approx(20.0).margin(1.0e-5));
  CHECK(committed.y == Catch::Approx(0.0).margin(1.0e-5));
  CHECK(window.session().undo_label() == "Drag sketch handle");
}
