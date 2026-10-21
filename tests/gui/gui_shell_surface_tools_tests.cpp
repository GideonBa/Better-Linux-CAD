#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <array>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellFeatureTools::Command;
using Quad = std::array<Point3, 4>;
template <typename T> auto checked(T value) {
  INFO((value.has_error() ? value.error().message() : "ok"));
  REQUIRE(value);
  return value.value();
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
SketchCoordinate3D coordinate(double value) {
  return checked(
      SketchCoordinate3D::create_explicit(checked(Quantity::linear_displacement_mm(value))));
}
void quad(PartDocument& part, const std::string& name, const Quad& points, bool fill) {
  const Sketch3DId id("sketch3d." + name);
  auto sketch = checked(Sketch3D::create(id, name));
  std::vector<PathSegmentReference> segments;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto& p = points[i];
    REQUIRE(sketch.add_point(
        checked(SketchPoint3D::create(SketchEntityId("p" + std::to_string(i)), coordinate(p.x),
                                      coordinate(p.y), coordinate(p.z)))));
  }
  for (int i = 0; i < 4; ++i) {
    const SketchEntityId line("line." + std::to_string(i));
    REQUIRE(sketch.add_line(
        checked(SketchLine3D::create(line, SketchEntityId("p" + std::to_string(i)),
                                     SketchEntityId("p" + std::to_string((i + 1) % 4))))));
    segments.push_back(
        checked(PathSegmentReference::create_sketch_3d(id, line, Sketch3DPathCurveKind::Line)));
  }
  REQUIRE(part.add_sketch_3d(std::move(sketch)));
  REQUIRE(part.add_path_curve(
      checked(PathCurve::create(PathCurveId("path." + name), name + " boundary", segments,
                                PathClosure::Closed, PathOrientationRule::MinimumTwist))));
  REQUIRE(part.add_path_curve(
      checked(PathCurve::create(PathCurveId("edge." + name), name + " edge", {segments.front()},
                                PathClosure::Open, PathOrientationRule::MinimumTwist))));
  if (!fill)
    return;
  const BodyId body("body." + name);
  REQUIRE(part.add_body(checked(Body::create(body, name, BodyKind::Surface))));
  REQUIRE(part.add_surface_feature(checked(FillSurfaceFeature::create(
      FeatureId("feature." + name), name,
      {checked(BoundaryCurveReference::create_path_curve(PathCurveId("path." + name)))}, body))));
}
void fixture(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Surface inputs")));
  auto setup = window.session().commit_part_transaction("Surface inputs", [](PartDocument& part) {
    quad(part, "left", {{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}}}, true);
    quad(part, "right", {{{10, 0, 0}, {20, 0, 0}, {20, 10, 0}, {10, 10, 0}}}, true);
    quad(part, "solo", {{{0, 20, 0}, {10, 20, 0}, {10, 30, 0}, {0, 30, 0}}}, false);
    quad(part, "trim", {{{2, 2, 0}, {8, 2, 0}, {8, 8, 0}, {2, 8, 0}}}, false);
    const std::array<Quad, 6> cube{{{{{30, 0, 0}, {30, 10, 0}, {40, 10, 0}, {40, 0, 0}}},
                                    {{{30, 0, 10}, {40, 0, 10}, {40, 10, 10}, {30, 10, 10}}},
                                    {{{30, 0, 0}, {40, 0, 0}, {40, 0, 10}, {30, 0, 10}}},
                                    {{{40, 0, 0}, {40, 10, 0}, {40, 10, 10}, {40, 0, 10}}},
                                    {{{40, 10, 0}, {30, 10, 0}, {30, 10, 10}, {40, 10, 10}}},
                                    {{{30, 10, 0}, {30, 0, 0}, {30, 0, 10}, {30, 10, 10}}}}};
    for (std::size_t i = 0; i < cube.size(); ++i)
      quad(part, "cube." + std::to_string(i), cube[i], true);
    return Result<std::size_t>::success(1);
  });
  REQUIRE(setup);
  REQUIRE(window.session().recompute());
  window.refresh();
  window.resize(1280, 960);
  window.show();
  window.viewport().fit_all();
  qApp->processEvents();
}
QAction& action(ShellWindow& window, const char* id) {
  auto* found = window.feature_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(found);
  return *found;
}
void choose(QComboBox& combo, const QString& text) {
  const int index = combo.findText(text);
  REQUIRE(index > 0);
  combo.setCurrentIndex(index);
}
int choice(QComboBox& combo, const QString& text) {
  const int index = combo.findText(text);
  REQUIRE(index > 0);
  return combo.itemData(index).toInt();
}
geometry::ShapeSummary result(ShellWindow& window) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(
      surface_feature_result_body(window.session().part_document()->surface_features().back()));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape);
}
geometry::GeometryShapeBounds bounds(ShellWindow& window) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(
      surface_feature_result_body(window.session().part_document()->surface_features().back()));
  REQUIRE(shape);
  return checked(geometry::BodyTransformAdapter{}.bounds(*shape));
}
void history(ShellWindow& window, const std::string& before) {
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.save_to(dir.filePath("surface.blcad.json")));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(dir.filePath("surface.blcad.json").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*opened.part_document())) == after);
}
void capture(ShellWindow& window, const QString& name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(window.grab().save(QDir(dir).filePath(name + ".png")));
}
} // namespace

TEST_CASE("Surface Boundary and Fill publish geometry and commit one reversible feature",
          "[gui][block136-surface]") {
  bool fill = false;
  SECTION("Boundary") {}
  SECTION("Fill") {
    fill = true;
  }
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto bodies = window.session().part_document()->bodies().size();
  REQUIRE(action(window, fill ? "surface_fill" : "surface_boundary").isEnabled());
  action(window, fill ? "surface_fill" : "surface_boundary").trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.apply());
  const int source = choice(tools.surface_source_editor(), fill ? "solo boundary" : "left edge");
  REQUIRE(tools.append_surface_input(source));
  if (!fill)
    REQUIRE(tools.append_surface_input(choice(tools.surface_source_editor(), "solo edge")));
  CHECK_FALSE(tools.append_surface_input(source));
  REQUIRE(window.viewport().feature_preview_active());
  if (!fill) {
    tools.surface_order_editor().setCurrentRow(1);
    auto* up = window.findChild<QPushButton*>("blcad.surface.move_up");
    REQUIRE(up);
    up->click();
    CHECK(tools.surface_order_editor().item(0)->text() == "solo edge");
  }
  CHECK(window.viewport().feature_preview_count() == 1);
  CHECK(snapshot(window) == before);
  CHECK(window.session().part_document()->bodies().size() == bodies);
  capture(window, fill ? "surface-fill" : "surface-boundary");
  REQUIRE(tools.apply());
  CHECK(result(window).solid_count == 0);
  CHECK(bounds(window).minimum.y == Catch::Approx(fill ? 20 : 0).margin(1e-5));
  CHECK(bounds(window).maximum.y == Catch::Approx(fill ? 30 : 20).margin(1e-5));
  CHECK(window.session().part_document()->bodies().back().kind() == BodyKind::Surface);
  if (!fill) {
    const auto& feature = std::get<BoundarySurfaceFeature>(
        window.session().part_document()->surface_features().back());
    CHECK(std::get<PathCurveId>(feature.boundaries().front().source()) == PathCurveId("edge.solo"));
  }
  history(window, before);
}

TEST_CASE("Surface Trim and Extend keep all pending geometry out of the document until Apply",
          "[gui][block136-surface]") {
  bool extend = false;
  SECTION("Trim") {}
  SECTION("Extend") {
    extend = true;
  }
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_surface(extend ? Command::ExtendSurface : Command::TrimSurface));
  choose(tools.surface_target_editor(), "left");
  choose(tools.surface_source_editor(), extend ? "left edge" : "trim boundary");
  REQUIRE(window.viewport().feature_preview_active());
  if (extend) {
    REQUIRE(tools.manipulators().handles().size() == 1);
    const auto& handle = tools.manipulators().handles().front();
    CHECK(handle.origin.x == Catch::Approx(5));
    CHECK(handle.origin.y == Catch::Approx(0));
    CHECK(handle.axis.y == Catch::Approx(-1));
    REQUIRE(tools.set_numeric_text("7 mm"));
    CHECK_FALSE(tools.set_numeric_text("-2 mm"));
    CHECK(tools.coordinator().surface().distance() == 7);
    REQUIRE(window.viewport().feature_preview_active());
    choose(tools.surface_source_editor(), "right edge");
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(tools.apply());
    choose(tools.surface_source_editor(), "left edge");
    REQUIRE(window.viewport().feature_preview_active());
  }
  CHECK(snapshot(window) == before);
  capture(window, extend ? "surface-extend" : "surface-trim");
  REQUIRE(tools.apply());
  CHECK(result(window).solid_count == 0);
  CHECK(bounds(window).minimum.y == Catch::Approx(extend ? -7 : 2).margin(1e-5));
  CHECK(bounds(window).maximum.y == Catch::Approx(extend ? 10 : 8).margin(1e-5));
  if (extend)
    CHECK(window.session().part_document()->parameters().back().value().millimeters() == 7);
  history(window, before);
}

TEST_CASE("Surface Stitch and closed shell conversion preserve candidate and history boundaries",
          "[gui][block136-surface]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_surface(Command::SurfaceStitch));
  for (int i = 0; i < 6; ++i)
    REQUIRE(tools.append_surface_input(
        choice(tools.surface_target_editor(), QStringLiteral("cube.%1").arg(i))));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(tools.manipulators().handles().empty());
  REQUIRE(tools.set_numeric_text("0.01 mm"));
  CHECK_FALSE(tools.set_numeric_text("0 mm"));
  CHECK(tools.coordinator().surface().tolerance() == Catch::Approx(0.01));
  CHECK(snapshot(window) == before);
  capture(window, "surface-stitch");
  REQUIRE(tools.apply());
  CHECK(result(window).solid_count == 0);
  CHECK(bounds(window).maximum.z == Catch::Approx(10).margin(1e-5));
  const auto shell_name = window.session().part_document()->bodies().back().name();
  history(window, before);
  const auto stitched = snapshot(window);
  REQUIRE(tools.begin_surface(Command::ClosedShellToSolid));
  choose(tools.surface_target_editor(), "left");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(tools.apply());
  choose(tools.surface_target_editor(), QString::fromStdString(shell_name));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == stitched);
  capture(window, "surface-to-solid");
  REQUIRE(tools.apply());
  CHECK(result(window).solid_count == 1);
  CHECK(result(window).volume_mm3 == Catch::Approx(1000).margin(1e-5));
  CHECK(window.session().part_document()->bodies().back().kind() == BodyKind::Solid);
  history(window, stitched);
}

TEST_CASE("Surface selection roles and cancellation restore the previous viewport context",
          "[gui][block136-surface]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"datum-plane/datum.xy"});
  const auto scope = window.viewport().selection_semantic_scope();
  const auto mask = window.viewport().selection_filter_mask();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_surface(Command::ExtendSurface));
  CHECK_FALSE(window.viewport().select_semantic("path/edge.left"));
  REQUIRE(window.viewport().select_semantic("body/body.left"));
  CHECK(tools.surface_target_editor().currentText() == "left");
  tools.surface_pick_editor().setCurrentIndex(1);
  CHECK_FALSE(window.viewport().select_semantic("body/body.right"));
  REQUIRE(window.viewport().select_semantic("path/edge.left"));
  CHECK(tools.surface_source_editor().currentText() == "left edge");
  REQUIRE(window.viewport().feature_preview_active());
  tools.cancel();
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Surface commands reject missing sources and colliding pending records",
          "[gui][block136-surface]") {
  ShellWindow empty;
  REQUIRE(empty.new_part("Empty"));
  for (const char* id : {"surface_boundary", "surface_fill", "surface_trim", "surface_extend",
                         "surface_stitch", "surface_to_solid"})
    CHECK_FALSE(action(empty, id).isEnabled());
  CHECK_FALSE(empty.feature_tools().begin_surface(Command::FillSurface));
  CHECK_FALSE(empty.session().task().active());
  ShellWindow window;
  fixture(window);
  const auto before = snapshot(window);
  GuiInteractiveSurfaceController controller;
  const auto body = window.session().part_document()->bodies().front();
  CHECK_FALSE(controller.begin_fill(window.session(), FeatureId("collision"), "Collision",
                                    body.id(), body));
  auto duplicate_name = checked(Body::create(BodyId("new.body"), body.name(), BodyKind::Surface));
  CHECK_FALSE(controller.begin_fill(window.session(), FeatureId("collision"), "Collision",
                                    duplicate_name.id(), duplicate_name));
  auto pending_body = checked(Body::create(BodyId("pending.body"), "Pending", BodyKind::Surface));
  auto wrong = checked(Parameter::create_angle(ParameterId("pending.distance"), "Distance",
                                               checked(Quantity::angle_deg(5))));
  CHECK_FALSE(controller.begin_extend(
      window.session(), FeatureId("pending.feature"), "Pending",
      checked(SurfaceReference::create_body(body.id())),
      checked(BoundaryCurveReference::create_path_curve(PathCurveId("edge.left"))), wrong.id(),
      pending_body.id(), pending_body, {wrong}));
  CHECK_FALSE(controller.active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Native surface tools pick target and boundary then refine and apply a live drag",
          "[gui][block136-surface][native-shell-surface]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT surface picking");
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  auto& extend_action = action(window, "surface_extend");
  QToolButton* extend_button = nullptr;
  for (auto* button : window.findChildren<QToolButton*>())
    if (button->defaultAction() == &extend_action)
      extend_button = button;
  REQUIRE(extend_button);
  auto* parent = extend_button->parentWidget();
  while (parent && !qobject_cast<QScrollArea*>(parent))
    parent = parent->parentWidget();
  auto* ribbon_scroll = qobject_cast<QScrollArea*>(parent);
  REQUIRE(ribbon_scroll);
  ribbon_scroll->ensureWidgetVisible(extend_button);
  qApp->processEvents();
  REQUIRE(extend_button->isVisible());
  REQUIRE(window.childAt(extend_button->mapTo(&window, extend_button->rect().center())) ==
          extend_button);
  extend_button->click();
  REQUIRE(tools.command() == Command::ExtendSurface);
  choose(tools.surface_target_editor(), "right");
  QEventLoop frame;
  QTimer::singleShot(40, &frame, &QEventLoop::quit);
  frame.exec();
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point, Qt::MouseButtons buttons) {
    const QPointF local(point.x, point.y);
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  };
  const auto click = [&](Point3 point) {
    const auto screen = checked(window.viewport().model_to_screen(point));
    send(QEvent::MouseButtonPress, screen, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, screen, Qt::NoButton);
  };
  click({5, 5, 0});
  REQUIRE(tools.surface_target_editor().currentText() == "left");
  choose(tools.surface_source_editor(), "right edge");
  tools.surface_pick_editor().setCurrentIndex(1);
  click({5, 0, 0});
  REQUIRE(tools.surface_source_editor().currentText() == "left edge");
  REQUIRE(window.viewport().feature_preview_active());
  const auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1);
  const auto endpoint = presentations.front().endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  const GuiSketchScreenPoint farther{endpoint.x + (endpoint.x - presentations.front().anchor.x) / 2,
                                     endpoint.y +
                                         (endpoint.y - presentations.front().anchor.y) / 2};
  send(QEvent::MouseMove, farther, Qt::LeftButton);
  CHECK(tools.coordinator().surface().distance() > 5);
  REQUIRE(tools.set_numeric_text("8 mm"));
  send(QEvent::MouseButtonRelease, farther, Qt::NoButton);
  CHECK(tools.coordinator().surface().distance() == 8);
  const auto next_endpoint = tools.manipulators().presentations().front().endpoint;
  send(QEvent::MouseButtonPress, next_endpoint, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  REQUIRE(tools.set_numeric_text("12 mm"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &escape);
  REQUIRE(tools.manipulators().active_handle_id().empty());
  CHECK(tools.coordinator().surface().distance() == 8);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  capture(window, "native-surface-extend");
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  REQUIRE(apply->isVisible());
  REQUIRE(window.childAt(apply->mapTo(&window, apply->rect().center())) == apply);
  auto* inputs = window.findChild<QScrollArea*>("blcad.feature.inputs_scroll");
  REQUIRE(inputs);
  CHECK_FALSE(inputs->horizontalScrollBar()->isVisible());
  apply->click();
  REQUIRE_FALSE(tools.active());
  CHECK(bounds(window).minimum.y == Catch::Approx(-8).margin(1e-5));
  history(window, before);
}

TEST_CASE("Surface Trim resolves a solid semantic face and materialized Sketch profile",
          "[gui][block136-surface]") {
  ShellWindow window;
  REQUIRE(window.new_part("Semantic surface inputs"));
  auto setup =
      window.session().commit_part_transaction("Solid and trim profile", [](PartDocument& part) {
        for (const auto& [id, value] :
             std::array<std::pair<const char*, double>, 2>{{{"size", 10}, {"diameter", 4}}})
          REQUIRE(part.add_parameter(checked(
              Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
        auto base = checked(Sketch::create(SketchId("base"), "Base", DatumPlaneId("datum.xy")));
        REQUIRE(base.add_profile(checked(RectangleProfile::create(
            ProfileId("profile.base"), ParameterId("size"), ParameterId("size")))));
        REQUIRE(part.add_sketch(std::move(base)));
        REQUIRE(part.add_body(checked(Body::create(BodyId("box"), "Box", BodyKind::Solid))));
        auto feature = checked(Feature::create_additive_extrude(
            FeatureId("feature.box"), "Box", SketchId("base"), ParameterId("size")));
        auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                std::nullopt, BodyId("box")));
        REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
        REQUIRE(part.add_construction_plane(checked(
            ConstructionPlane::create_explicit(ConstructionPlaneId("plane.top"), "Top", {0, 0, 10},
                                               {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
        auto sketch = checked(Sketch::create(SketchId("trim"), "Trim", DatumPlaneId("plane.top")));
        REQUIRE(sketch.add_profile(
            checked(CircleProfile::create(ProfileId("profile.trim"), ParameterId("diameter")))));
        REQUIRE(part.add_sketch(std::move(sketch)));
        return Result<std::size_t>::success(1);
      });
  REQUIRE(setup);
  window.refresh();
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(action(window, "surface_trim").isEnabled());
  REQUIRE(tools.begin_surface(Command::TrimSurface));
  const int top = tools.surface_target_editor().findText("Box / top");
  REQUIRE(top > 0);
  const auto targets = tools.finishing_selection_items();
  const auto target = std::find_if(targets.begin(), targets.end(), [](const auto& item) {
    return item.semantic_id.ends_with("/top");
  });
  REQUIRE(target != targets.end());
  REQUIRE(window.viewport().select_semantic(target->semantic_id));
  CHECK(tools.surface_target_editor().currentIndex() == top);
  tools.surface_pick_editor().setCurrentIndex(1);
  REQUIRE(window.viewport().select_semantic("sketch/trim/profile/profile.trim"));
  CHECK(tools.surface_source_editor().currentText() == "Trim / profile.trim");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  CHECK(result(window).solid_count == 0);
  CHECK(bounds(window).minimum.z == Catch::Approx(10).margin(1e-5));
  CHECK(bounds(window).minimum.x == Catch::Approx(-2).margin(1e-5));
  CHECK(bounds(window).maximum.x == Catch::Approx(2).margin(1e-5));
  history(window, before);
}
