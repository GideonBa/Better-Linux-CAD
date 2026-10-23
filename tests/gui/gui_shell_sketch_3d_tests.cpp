#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <tuple>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* found = window.findChild<T*>(QStringLiteral("blcad.sketch3d.") + id);
  REQUIRE(found);
  return *found;
}
std::string snapshot(const ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
void button(ShellWindow& window, const char* id) {
  control<QPushButton>(window, id).click();
}
void coordinates(ShellWindow& window, double x, double y, double z) {
  control<QDoubleSpinBox>(window, "x").setValue(x);
  control<QDoubleSpinBox>(window, "y").setValue(y);
  control<QDoubleSpinBox>(window, "z").setValue(z);
}
void select(ShellWindow& window, const char* field, const char* id) {
  auto& combo = control<QComboBox>(window, field);
  const int index = combo.findData(QString::fromUtf8(id));
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void point(ShellWindow& window, double x, double y, double z) {
  select(window, "point", "");
  coordinates(window, x, y, z);
  button(window, "stage_point");
}
void line(ShellWindow& window, const char* start, const char* end) {
  select(window, "start", start);
  select(window, "end", end);
  button(window, "stage_line");
}
void seed(ShellWindow& window) {
  REQUIRE(window.new_part("Spatial editing"));
  REQUIRE(window.session().commit_part_transaction("Existing spatial curves", [](PartDocument&
                                                                                     part) {
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId("px"), "X", checked(Quantity::length_mm(5))))));
    auto sketch = checked(Sketch3D::create(Sketch3DId("spatial"), "Spatial"));
    sketch = checked(Sketch3DInteractionService::add_point(sketch, SketchEntityId("a"), {5, 0, 0}))
                 .sketch;
    sketch = checked(Sketch3DInteractionService::add_point(sketch, SketchEntityId("b"), {5, 0, 10}))
                 .sketch;
    sketch = checked(Sketch3DInteractionService::add_point(sketch, SketchEntityId("c"), {5, 0, 20}))
                 .sketch;
    sketch =
        checked(Sketch3DInteractionService::add_point(sketch, SketchEntityId("d"), {15, 0, 30}))
            .sketch;
    const auto explicit_coordinate = [](double value) {
      return checked(
          SketchCoordinate3D::create_explicit(checked(Quantity::linear_displacement_mm(value))));
    };
    auto driven = checked(SketchPoint3D::create(
        SketchEntityId("driven"), checked(SketchCoordinate3D::create_parameter(ParameterId("px"))),
        explicit_coordinate(4.123456789123), explicit_coordinate(0)));
    REQUIRE(sketch.add_point(driven));
    for (const auto& [id, start, end] : {std::tuple{"line.1", "a", "b"}, {"line.2", "b", "c"}}) {
      auto segment = checked(
          SketchLine3D::create(SketchEntityId(id), SketchEntityId(start), SketchEntityId(end)));
      REQUIRE(sketch.add_line(segment));
    }
    const auto ref = [](const char* id) {
      return checked(SketchPointReference3D::create_local_point(SketchEntityId(id)));
    };
    REQUIRE(sketch.add_arc(checked(
        SketchArc3D::create_three_point(SketchEntityId("arc"), ref("b"), ref("c"), ref("d")))));
    REQUIRE(sketch.add_spline(checked(SketchSpline3D::create(
        SketchEntityId("spline"), SketchSpline3DRepresentation::ControlPoints,
        {ref("a"), ref("b"), ref("c"), ref("d")}, 3, SketchSpline3DContinuity::Positional))));
    REQUIRE(sketch.add_polyline(checked(SketchPolyline3D::create(
        SketchEntityId("polyline"), {SketchEntityId("a"), SketchEntityId("d")}))));
    REQUIRE(sketch.add_guide_curve(
        checked(SketchGuideCurve3D::create(SketchEntityId("guide"), SketchEntityId("spline"),
                                           SketchGuideCurve3DRole::LoftGuide, "Retain guide"))));
    REQUIRE(part.add_sketch_3d(sketch));
    std::vector<PathSegmentReference> segments;
    for (const char* id : {"line.1", "line.2"})
      segments.push_back(checked(PathSegmentReference::create_sketch_3d(
          sketch.id(), SketchEntityId(id), Sketch3DPathCurveKind::Line)));
    REQUIRE(part.add_path_curve(
        checked(PathCurve::create(PathCurveId("path"), "Path", segments, PathClosure::Open,
                                  PathOrientationRule::MinimumTwist))));
    return Result<std::size_t>::success(1);
  }));
  window.refresh();
}
void edit(ShellWindow& window) {
  REQUIRE(window.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
}
void move(ShellWindow& window, const char* id, double x, double y, double z) {
  select(window, "point", id);
  coordinates(window, x, y, z);
  button(window, "stage_point");
}
} // namespace

TEST_CASE("Sketch3D creates shared-point line chains with disposable preview and atomic history",
          "[gui][block136-sketch3d]") {
  ShellWindow window;
  REQUIRE(window.new_part("Spatial"));
  const auto before = snapshot(window);
  const bool dirty = window.session().dirty();
  REQUIRE(window.sketch_3d_tools().begin());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK_FALSE(window.sketch_3d_tools().preview());
  point(window, -5, 2, 0);
  point(window, 3, 7, 10);
  point(window, 6, 4, 20);
  line(window, "point.1", "point.2");
  line(window, "point.2", "point.3");
  REQUIRE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window) == before);
  CHECK(window.session().dirty() == dirty);
  CHECK(window.viewport().feature_preview_active());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto after = snapshot(window);
  const auto& sketch = window.session().part_document()->sketches_3d().front();
  REQUIRE(sketch.points().size() == 3);
  REQUIRE(sketch.lines().size() == 2);
  CHECK(sketch.lines()[0].end_point() == sketch.lines()[1].start_point());
  CHECK_FALSE(window.sketch_3d_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  const auto path = directory.filePath("spatial.blcad");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == after);
}

TEST_CASE("Sketch3D point edits preserve all curve kinds bindings and path references",
          "[gui][block136-sketch3d]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  edit(window);
  move(window, "b", 5, 0, 12);
  REQUIRE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().feature_preview_replaces("path/path"));
  CHECK(window.viewport().feature_preview_replaces("sketch3d/spatial/entity/arc"));
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* sketch = window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  REQUIRE(sketch);
  CHECK(sketch->find_point(SketchEntityId("b"))->z().explicit_coordinate()->millimeters() == 12);
  CHECK(sketch->find_point(SketchEntityId("driven"))->x().parameter() == ParameterId("px"));
  CHECK(sketch->find_point(SketchEntityId("driven"))->y().explicit_coordinate()->millimeters() ==
        4.123456789123);
  CHECK(sketch->arcs().size() == 1);
  CHECK(sketch->splines().size() == 1);
  CHECK(sketch->polylines().size() == 1);
  CHECK(sketch->guide_curves().front().label() == "Retain guide");
  CHECK(sketch->find_arc(SketchEntityId("arc"))->start().local_point() == SketchEntityId("b"));
  CHECK(window.session().part_document()->dependency_graph().has_dependency("spatial", "path"));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D explicitly confirms parameter detachment and rejects unaccepted point input",
          "[gui][block136-sketch3d]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  edit(window);
  move(window, "driven", 6, 4, 2);
  CHECK(control<QLabel>(window, "summary").text().contains("Zustimmung"));
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window) == before);
  control<QCheckBox>(window, "detach").setChecked(true);
  button(window, "stage_point");
  REQUIRE(window.sketch_3d_tools().preview());
  control<QDoubleSpinBox>(window, "z").setValue(4);
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  button(window, "stage_point");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.session()
            .part_document()
            ->find_sketch_3d(Sketch3DId("spatial"))
            ->find_point(SketchEntityId("driven"))
            ->parameter_dependencies()
            .empty());
  CHECK_FALSE(window.session().part_document()->dependency_graph().has_dependency("px", "spatial"));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE(
    "Sketch3D invalid geometry and duplicate endpoints reject without publishing partial intent",
    "[gui][block136-sketch3d]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  const auto revision = window.session().presentation_revision();
  edit(window);
  line(window, "a", "a");
  CHECK_FALSE(window.sketch_3d_tools().preview());
  move(window, "b", 5, 0, 0);
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK(snapshot(window) == before);
  CHECK(window.session().presentation_revision() == revision);
  move(window, "b", 5, 0, 11);
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.session().derived_results_fresh());
}

TEST_CASE("Sketch3D tasks expire on cancellation saving history context and revision changes",
          "[gui][block136-sketch3d]") {
  ShellWindow window;
  seed(window);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("spatial.blcad")));
  REQUIRE(window.open_path(directory.filePath("spatial.blcad")));
  const auto before = snapshot(window);
  edit(window);
  move(window, "b", 5, 0, 12);
  REQUIRE(window.sketch_3d_tools().preview());
  SECTION("Escape from nested numeric editor") {
    auto& spin = control<QDoubleSpinBox>(window, "z");
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&spin, &event);
  }
  SECTION("dock close") {
    window.findChild<QDockWidget*>("blcad.shell.sketch3d_task")->close();
  }
  SECTION("save retains committed data only") {
    REQUIRE(window.save_to({}));
  }
  SECTION("undo cancels before history") {
    CHECK_FALSE(window.undo());
  }
  SECTION("revision mutation") {
    REQUIRE(window.session().commit_part_transaction("Parameter edit", [](PartDocument& part) {
      auto changed = part.set_parameter_value(ParameterId("px"), checked(Quantity::length_mm(6)));
      return changed ? Result<std::size_t>::success(1)
                     : Result<std::size_t>::failure(changed.error());
    }));
    window.refresh();
    REQUIRE(window.undo());
  }
  SECTION("foreign task") {
    (void)window.session().task().cancel();
    REQUIRE(window.session().task().begin("foreign"));
    window.refresh();
    CHECK(window.session().task().command_id() == "foreign");
    (void)window.session().task().cancel();
  }
  SECTION("shell context replacement") {
    REQUIRE(window.new_project("Project"));
    window.refresh();
    CHECK_FALSE(window.sketch_3d_tools().active());
    CHECK_FALSE(window.sketch_3d_tools().begin());
    CHECK_FALSE(window.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
    return;
  }
  CHECK_FALSE(window.sketch_3d_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D entry points reject unavailable contexts and allocate across all entity kinds",
          "[gui][block136-sketch3d]") {
  ShellWindow window;
  CHECK_FALSE(window.sketch_3d_tools().begin());
  REQUIRE(window.new_project("Project"));
  CHECK_FALSE(window.sketch_3d_tools().begin());
  REQUIRE(window.session().close(true));
  window.refresh();
  REQUIRE(window.new_part("Identity"));
  REQUIRE(window.session().task().begin("foreign"));
  CHECK_FALSE(window.sketch_3d_tools().begin());
  (void)window.session().task().cancel();
  window.session().set_derived_results_fresh(false);
  CHECK_FALSE(window.sketch_3d_tools().begin());
  window.session().set_derived_results_fresh(true);
  CHECK_FALSE(window.sketch_3d_tools().begin_edit(Sketch3DId("missing")));
  REQUIRE(window.session().commit_part_transaction(
      "Existing entity identities", [](PartDocument& part) {
        auto sketch = checked(Sketch3D::create(Sketch3DId("spatial"), "Spatial"));
        for (const auto& [id, value] : std::vector<std::pair<const char*, SketchPoint3DValue>>{
                 {"a", {0, 0, 0}}, {"b", {10, 0, 0}}, {"c", {10, 10, 0}}})
          sketch = checked(Sketch3DInteractionService::add_point(sketch, SketchEntityId(id), value))
                       .sketch;
        const auto ref = [](const char* id) {
          return checked(SketchPointReference3D::create_local_point(SketchEntityId(id)));
        };
        REQUIRE(sketch.add_arc(checked(SketchArc3D::create_three_point(
            SketchEntityId("point.1"), ref("a"), ref("b"), ref("c")))));
        REQUIRE(sketch.add_line(checked(SketchLine3D::create(
            SketchEntityId("line.1"), SketchEntityId("a"), SketchEntityId("b")))));
        return part.add_sketch_3d(sketch);
      }));
  window.refresh();
  edit(window);
  CHECK_FALSE(window.sketch_3d_tools().begin());
  point(window, 3, 4, 20);
  line(window, "c", "point.2");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto& sketch = window.session().part_document()->sketches_3d().front();
  CHECK(sketch.find_arc(SketchEntityId("point.1")));
  CHECK(sketch.find_point(SketchEntityId("point.2")));
  CHECK(sketch.find_line(SketchEntityId("line.2")));
}

namespace {
void prepare_handles(ShellWindow& window) {
  seed(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  window.viewport().set_projection(GuiViewportProjection::Orthographic);
  window.viewport().set_standard_view(GuiStandardView::Isometric);
  window.viewport().fit_all();
  auto camera = window.viewport().camera_bookmark();
  camera.eye = {150, 150, 160};
  camera.target = {0, 0, 10};
  if (QApplication::platformName() != "xcb")
    camera.scale = 120;
  REQUIRE(window.viewport().restore_camera_bookmark(camera));
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  edit(window);
  if (QApplication::platformName() == "xcb") {
    window.raise();
    window.activateWindow();
    QGuiApplication::sync();
    qApp->processEvents();
    if (!window.isActiveWindow()) {
      QEventLoop activation;
      QTimer poll, deadline;
      deadline.setSingleShot(true);
      QObject::connect(&poll, &QTimer::timeout, &activation, [&] {
        if (window.isActiveWindow())
          activation.quit();
      });
      QObject::connect(&deadline, &QTimer::timeout, &activation, &QEventLoop::quit);
      poll.start(5);
      deadline.start(2000);
      activation.exec();
    }
    REQUIRE(window.isActiveWindow());
  }
  control<QCheckBox>(window, "grips").setChecked(true);
  window.sketch_3d_tools().refresh_presentation();
  qApp->processEvents();
}
void pointer(ShellWindow& window, QEvent::Type type, GuiSketchScreenPoint p) {
  auto& viewport = window.viewport();
  const QPointF local(p.x, p.y);
  QMouseEvent event(type, local, viewport.mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                    type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&viewport, &event);
  qApp->processEvents();
}
GuiSketchScreenPoint press(ShellWindow& window, const std::string& axis) {
  window.sketch_3d_tools().refresh_presentation();
  const auto presentations = window.sketch_3d_tools().manipulators().presentations();
  const auto found = std::find_if(presentations.begin(), presentations.end(), [&](const auto& h) {
    return h.handle_id == "sketch3d.position." + axis;
  });
  REQUIRE(found != presentations.end());
  REQUIRE(window.viewport().rect().contains(
      QPoint(qRound(found->endpoint.x), qRound(found->endpoint.y))));
  pointer(window, QEvent::MouseButtonPress, found->endpoint);
  REQUIRE(window.sketch_3d_tools().manipulators().active_handle_id() ==
          "sketch3d.position." + axis);
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  return found->endpoint;
}
void numeric(ShellWindow& window, const QString& value) {
  auto& editor = control<QLineEdit>(window, "grip_numeric");
  REQUIRE_FALSE(editor.isReadOnly());
  editor.selectAll();
  QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, value);
  QApplication::sendEvent(&editor, &event);
  qApp->processEvents();
}
void drag_to(ShellWindow& window, const std::string& axis, const QString& value) {
  const auto p = press(window, axis);
  numeric(window, value);
  REQUIRE(window.sketch_3d_tools().manipulators().numeric_override_active());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  pointer(window, QEvent::MouseMove, {p.x + 11, p.y + 7});
  pointer(window, QEvent::MouseButtonRelease, {p.x + 19, p.y + 13});
  CHECK(window.sketch_3d_tools().manipulators().active_handle_id().empty());
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
}
void pick_point(ShellWindow& window, int role, Point3 point) {
  control<QComboBox>(window, "pick").setCurrentIndex(role);
  const auto screen = checked(window.viewport().model_to_screen(point));
  REQUIRE(window.viewport().rect().contains(QPoint(qRound(screen.x), qRound(screen.y))));
  pointer(window, QEvent::MouseButtonPress, screen);
  pointer(window, QEvent::MouseButtonRelease, screen);
}
void escape_handle(ShellWindow& window) {
  QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&control<QLineEdit>(window, "grip_numeric"), &event);
}
void placement_hover(ShellWindow& window, GuiSketchScreenPoint p) {
  auto& viewport = window.viewport();
  const QPointF local(p.x, p.y);
  QMouseEvent event(QEvent::MouseMove, local, viewport.mapToGlobal(local.toPoint()), Qt::NoButton,
                    Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &event);
  qApp->processEvents();
}
void placement_mode(ShellWindow& window, const char* anchor = "b", int lock = 4) {
  select(window, "placement_anchor", anchor);
  control<QComboBox>(window, "lock").setCurrentIndex(lock);
  control<QComboBox>(window, "pick").setCurrentIndex(5);
}
GuiSketchScreenPoint placement_screen(ShellWindow& window, Point3 p) {
  const auto screen = checked(window.viewport().model_to_screen(p));
  REQUIRE(window.viewport().rect().contains(QPoint(qRound(screen.x), qRound(screen.y))));
  return screen;
}
double placement_tolerance(ShellWindow& window) {
  if (QApplication::platformName() != "xcb")
    return 1e-6;
  // Native projection rounds to physical pixels. Bound the model-coordinate
  // error using the visible projection of a ten-millimeter axis, not a fixed
  // model tolerance that depends on desktop resolution or fitted OCCT zoom.
  const auto origin = placement_screen(window, {5, 0, 10});
  double pixels = 1e9;
  for (const auto p : std::array<Point3, 3>{{{15, 0, 10}, {5, 10, 10}, {5, 0, 20}}}) {
    const auto screen = placement_screen(window, p);
    pixels = std::min(pixels, manipulator_detail::screen_distance(screen, origin));
  }
  REQUIRE(pixels > 1);
  return 40 / pixels;
}
} // namespace

TEST_CASE("Sketch3D viewport collection uses candidate point and line identities without changing "
          "filters",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  window.viewport().set_selection_filter_mask(17);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"custom"});
  pick_point(window, 1, {5, 0, 20});
  CHECK(control<QComboBox>(window, "point").currentData().toString() == "c");
  CHECK(window.sketch_3d_tools().manipulators().handles().empty());
  pick_point(window, 2, {5, 0, 0});
  pick_point(window, 3, {15, 0, 30});
  CHECK(control<QComboBox>(window, "start").currentData().toString() == "a");
  CHECK(control<QComboBox>(window, "end").currentData().toString() == "d");
  pick_point(window, 4, {5, 0, 15});
  CHECK(control<QComboBox>(window, "start").currentData().toString() == "b");
  CHECK(control<QComboBox>(window, "end").currentData().toString() == "c");
  // New locally staged points are collected from the same model IDs as committed points.
  point(window, -20, 10, 15);
  REQUIRE(window.sketch_3d_tools().preview());
  pick_point(window, 2, {-20, 10, 15});
  CHECK(control<QComboBox>(window, "start").currentData().toString() == "point.1");
  pointer(window, QEvent::MouseButtonPress, {2, 2});
  pointer(window, QEvent::MouseButtonRelease, {2, 2});
  CHECK(control<QLabel>(window, "summary").text().contains("Kein passender"));
  CHECK(snapshot(window) == before);
  window.sketch_3d_tools().cancel();
  CHECK(window.viewport().selection_filter_mask() == 17);
  CHECK(window.viewport().selection_semantic_scope() ==
        std::optional(std::vector<std::string>{"custom"}));
}

TEST_CASE("Sketch3D locked grips preserve bindings precision and all downstream curve references",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  select(window, "point", "driven");
  CHECK(window.sketch_3d_tools().manipulators().handles().empty());
  control<QCheckBox>(window, "detach").setChecked(true);
  const std::array<std::size_t, 7> counts{3, 1, 1, 1, 2, 2, 2};
  for (int mode = 0; mode < 7; ++mode) {
    control<QComboBox>(window, "lock").setCurrentIndex(mode);
    const auto& handles = window.sketch_3d_tools().manipulators().handles();
    REQUIRE(handles.size() == counts[mode]);
    for (const auto& handle : handles) {
      if (mode >= 1 && mode <= 3)
        CHECK(handle.id.back() == "xyz"[mode - 1]);
      if (mode >= 4)
        CHECK(handle.id.back() != "zxy"[mode - 4]);
    }
  }
  control<QComboBox>(window, "lock").setCurrentIndex(1);
  drag_to(window, "x", "-8.123456789123 mm");
  CHECK(control<QCheckBox>(window, "detach").isHidden());
  CHECK(snapshot(window) == before);
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* source = window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  const auto* point = source->find_point(SketchEntityId("driven"));
  CHECK(point->x().explicit_coordinate()->millimeters() == -8.123456789123);
  CHECK(point->y().explicit_coordinate()->millimeters() == 4.123456789123);
  CHECK(point->parameter_dependencies().empty());
  CHECK(source->arcs().size() == 1);
  CHECK(source->splines().size() == 1);
  CHECK(source->polylines().size() == 1);
  CHECK(source->guide_curves().size() == 1);
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("grips.blcad")));
  REQUIRE(window.open_path(directory.filePath("grips.blcad")));
  CHECK(snapshot(window) == after);
}

TEST_CASE("Sketch3D drag uses the final release and Escape restores the complete local candidate",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  select(window, "point", "d");
  const auto before = snapshot(window);
  const auto p = press(window, "x");
  pointer(window, QEvent::MouseMove, {p.x + 8, p.y + 3});
  const auto during = control<QDoubleSpinBox>(window, "x").value();
  pointer(window, QEvent::MouseButtonRelease, {p.x + 28, p.y + 15});
  const auto released = control<QDoubleSpinBox>(window, "x").value();
  CHECK(released != Catch::Approx(during));
  REQUIRE(window.viewport().feature_preview_active());
  press(window, "z");
  numeric(window, "55 mm");
  escape_handle(window);
  CHECK(window.sketch_3d_tools().active());
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 30);
  CHECK(control<QDoubleSpinBox>(window, "x").value() == released);
  REQUIRE(window.viewport().feature_preview_active());
  control<QComboBox>(window, "lock").setCurrentIndex(4);
  drag_to(window, "y", "8.75 mm");
  CHECK(control<QDoubleSpinBox>(window, "x").value() == released);
  CHECK(control<QDoubleSpinBox>(window, "y").value() == 8.75);
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 30);
  REQUIRE(window.sketch_3d_tools().apply());
  const auto& edited = *window.session()
                            .part_document()
                            ->find_sketch_3d(Sketch3DId("spatial"))
                            ->find_point(SketchEntityId("d"));
  CHECK(edited.y().explicit_coordinate()->millimeters() == 8.75);
  CHECK(edited.z().explicit_coordinate()->millimeters() == 30);
  CHECK(snapshot(window) != before);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D invalid numeric and degenerate downstream drag candidates remain correctable",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  select(window, "point", "b");
  const auto before = snapshot(window);
  const auto p = press(window, "z");
  numeric(window, "0 mm");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  numeric(window, "not a number");
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  pointer(window, QEvent::MouseButtonRelease, p);
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 10);
  CHECK(snapshot(window) == before);
  drag_to(window, "z", "12 mm");
  CHECK(window.viewport().feature_preview_replaces("path/path"));
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.session()
            .part_document()
            ->find_sketch_3d(Sketch3DId("spatial"))
            ->find_point(SketchEntityId("b"))
            ->z()
            .explicit_coordinate()
            ->millimeters() == 12);
}

TEST_CASE("Sketch3D grips roll back on camera focus resize toggles and stale document boundaries",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  select(window, "point", "d");
  const auto before = snapshot(window);
  press(window, "x");
  numeric(window, "25 mm");
  SECTION("camera") {
    auto camera = window.viewport().camera_bookmark();
    camera.scale *= 1.2;
    REQUIRE(window.viewport().restore_camera_bookmark(camera));
  }
  SECTION("resize") {
    window.resize(1400, 900);
    qApp->processEvents();
  }
  SECTION("focus") {
    QEvent event(QEvent::WindowDeactivate);
    QApplication::sendEvent(&window, &event);
  }
  SECTION("lock") {
    control<QComboBox>(window, "lock").setCurrentIndex(4);
  }
  SECTION("point choice") {
    select(window, "point", "c");
  }
  SECTION("grip toggle") {
    control<QCheckBox>(window, "grips").setChecked(false);
  }
  SECTION("revision") {
    window.session().report_error(Error::validation("test", "Revision changed"));
    window.refresh();
    CHECK_FALSE(window.sketch_3d_tools().active());
  }
  SECTION("document replacement") {
    (void)window.session().task().cancel();
    REQUIRE(window.session().close(true));
    window.refresh();
    CHECK_FALSE(window.sketch_3d_tools().active());
    CHECK(window.sketch_3d_tools().manipulators().handles().empty());
    CHECK(window.viewport().mouseGrabber() != &window.viewport());
    return;
  }
  CHECK(window.sketch_3d_tools().manipulators().active_handle_id().empty());
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  CHECK(snapshot(window) == before);
  if (window.sketch_3d_tools().active() &&
      control<QComboBox>(window, "point").currentData().toString() == "d")
    CHECK(control<QDoubleSpinBox>(window, "x").value() == 15);
  window.sketch_3d_tools().cancel();
  CHECK(window.sketch_3d_tools().manipulators().handles().empty());
}

TEST_CASE("Sketch3D typed point input replaces an active gesture without retaining its other axes",
          "[gui][block136-sketch3d-grips]") {
  ShellWindow window;
  prepare_handles(window);
  select(window, "point", "d");
  const auto before = snapshot(window);
  press(window, "x");
  numeric(window, "25 mm");
  control<QDoubleSpinBox>(window, "z").setValue(42);
  CHECK(window.sketch_3d_tools().manipulators().active_handle_id().empty());
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  CHECK(control<QDoubleSpinBox>(window, "x").value() == 15);
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 42);
  CHECK(window.sketch_3d_tools().manipulators().handles().empty());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK(snapshot(window) == before);
  button(window, "stage_point");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* edited = window.session()
                           .part_document()
                           ->find_sketch_3d(Sketch3DId("spatial"))
                           ->find_point(SketchEntityId("d"));
  CHECK(edited->x().explicit_coordinate()->millimeters() == 15);
  CHECK(edited->z().explicit_coordinate()->millimeters() == 42);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D pointer placement resolves all seven anchor locks with atomic history",
          "[gui][block136-sketch3d-placement]") {
  bool perspective = false;
  SECTION("orthographic") {}
  SECTION("perspective") {
    perspective = true;
  }
  ShellWindow window;
  prepare_handles(window);
  if (perspective) {
    auto camera = window.viewport().camera_bookmark();
    camera.projection = GuiViewportProjection::Perspective;
    REQUIRE(window.viewport().restore_camera_bookmark(camera));
  }
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  const double tolerance = placement_tolerance(window);
  window.viewport().set_selection_filter_mask(17);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"custom"});
  const std::array<Point3, 7> expected{
      {{11, -6, 10}, {13, 0, 10}, {5, 8, 10}, {5, 0, 18}, {11, 7, 10}, {5, 7, 18}, {11, 0, 18}}};
  for (int mode = 0; mode < 7; ++mode) {
    CAPTURE(mode, perspective);
    placement_mode(window, "b", mode);
    const auto screen = placement_screen(window, expected[mode]);
    const int count = control<QComboBox>(window, "point").count();
    placement_hover(window, screen);
    CHECK(control<QWidget>(window, "placement_overlay").isVisible());
    CHECK(control<QComboBox>(window, "point").count() == count);
    CHECK(snapshot(window) == before);
    pointer(window, QEvent::MouseButtonPress, {screen.x + 9, screen.y + 7});
    CHECK(control<QComboBox>(window, "point").count() == count);
    CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
    CHECK_FALSE(window.sketch_3d_tools().preview());
    pointer(window, QEvent::MouseButtonRelease, screen);
    CHECK(control<QComboBox>(window, "point").count() == count + 1);
    CHECK(control<QComboBox>(window, "point").currentData().toString() ==
          "point." + QString::number(mode + 1));
    CHECK(control<QComboBox>(window, "placement_anchor").currentData().toString() == "b");
    CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
    CHECK(window.viewport().mouseGrabber() != &window.viewport());
    CHECK(snapshot(window) == before);
    CHECK(window.session().undo_label() == undo);
  }
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.viewport().selection_filter_mask() == 17);
  CHECK(window.viewport().selection_semantic_scope() ==
        std::optional(std::vector<std::string>{"custom"}));
  const auto* sketch = window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  REQUIRE(sketch);
  CHECK(sketch->points().size() == 12);
  CHECK(sketch->arcs().size() == 1);
  CHECK(sketch->splines().size() == 1);
  CHECK(sketch->guide_curves().size() == 1);
  for (int mode = 0; mode < 7; ++mode) {
    CAPTURE(mode);
    const auto* point = sketch->find_point(SketchEntityId("point." + std::to_string(mode + 1)));
    REQUIRE(point);
    CHECK(point->x().explicit_coordinate()->millimeters() ==
          Catch::Approx(expected[mode].x).margin(tolerance));
    CHECK(point->y().explicit_coordinate()->millimeters() ==
          Catch::Approx(expected[mode].y).margin(tolerance));
    CHECK(point->z().explicit_coordinate()->millimeters() ==
          Catch::Approx(expected[mode].z).margin(tolerance));
    if (mode == 1 || mode == 3 || mode == 6)
      CHECK(point->y().explicit_coordinate()->millimeters() == 0);
    if (mode == 2 || mode == 3 || mode == 5)
      CHECK(point->x().explicit_coordinate()->millimeters() == 5);
    if (mode == 1 || mode == 2 || mode == 4)
      CHECK(point->z().explicit_coordinate()->millimeters() == 10);
  }
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("placed.blcad")));
  REQUIRE(window.open_path(directory.filePath("placed.blcad")));
  CHECK(snapshot(window) == after);
}

TEST_CASE(
    "Sketch3D placement copies bound anchors and supports newly staged anchors without detaching",
    "[gui][block136-sketch3d-placement]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  placement_mode(window, "driven", 3);
  const auto first = placement_screen(window, {5, 4.123456789123, 12});
  pointer(window, QEvent::MouseButtonPress, first);
  pointer(window, QEvent::MouseButtonRelease, first);
  CHECK_FALSE(control<QCheckBox>(window, "detach").isChecked());
  placement_mode(window, "point.1", 1);
  const auto second =
      placement_screen(window, {14, 4.123456789123, control<QDoubleSpinBox>(window, "z").value()});
  pointer(window, QEvent::MouseButtonPress, second);
  pointer(window, QEvent::MouseButtonRelease, second);
  REQUIRE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window) == before);
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* sketch = window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  REQUIRE(sketch);
  const auto* anchor = sketch->find_point(SketchEntityId("driven"));
  REQUIRE(anchor);
  CHECK(anchor->x().parameter() == ParameterId("px"));
  CHECK(anchor->y().explicit_coordinate()->millimeters() == 4.123456789123);
  for (const char* id : {"point.1", "point.2"}) {
    const auto* placed = sketch->find_point(SketchEntityId(id));
    REQUIRE(placed);
    CHECK(placed->parameter_dependencies().empty());
    CHECK(placed->y().explicit_coordinate()->millimeters() == 4.123456789123);
  }
  CHECK(sketch->find_point(SketchEntityId("point.1"))->z().explicit_coordinate()->millimeters() ==
        sketch->find_point(SketchEntityId("point.2"))->z().explicit_coordinate()->millimeters());
}

TEST_CASE("Sketch3D placement interruptions retain earlier local previews and release capture",
          "[gui][block136-sketch3d-placement]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  point(window, -10, 5, 15);
  REQUIRE(window.sketch_3d_tools().preview());
  const int count = control<QComboBox>(window, "point").count();
  placement_mode(window, "point.1", 4);
  const auto screen = placement_screen(window, {0, 10, 15});
  pointer(window, QEvent::MouseButtonPress, screen);
  REQUIRE(window.viewport().mouseGrabber() == &window.viewport());
  SECTION("Escape exits placement first") {
    escape_handle(window);
    CHECK(control<QComboBox>(window, "pick").currentIndex() == 0);
  }
  SECTION("release outside viewport") {
    pointer(window, QEvent::MouseButtonRelease,
            {window.viewport().width() + 10.0, window.viewport().height() + 10.0});
  }
  SECTION("camera") {
    auto camera = window.viewport().camera_bookmark();
    camera.scale *= 1.1;
    REQUIRE(window.viewport().restore_camera_bookmark(camera));
  }
  SECTION("resize") {
    window.resize(1380, 880);
    qApp->processEvents();
  }
  SECTION("deactivate") {
    QEvent event(QEvent::WindowDeactivate);
    QApplication::sendEvent(&window, &event);
  }
  SECTION("capture lost") {
    QEvent event(QEvent::UngrabMouse);
    QApplication::sendEvent(&window.viewport(), &event);
  }
  SECTION("anchor") {
    select(window, "placement_anchor", "b");
  }
  SECTION("lock") {
    control<QComboBox>(window, "lock").setCurrentIndex(2);
  }
  SECTION("collection mode") {
    control<QComboBox>(window, "pick").setCurrentIndex(1);
  }
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
  CHECK(control<QComboBox>(window, "point").count() == count);
  CHECK(window.sketch_3d_tools().active());
  CHECK(window.viewport().feature_preview_active());
  CHECK(control<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"))->points().size() ==
        6);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D placement refuses pending typed input and unmatched releases",
          "[gui][block136-sketch3d-placement]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  const int count = control<QComboBox>(window, "point").count();
  select(window, "point", "b");
  auto* editor = control<QDoubleSpinBox>(window, "z").findChild<QLineEdit*>();
  REQUIRE(editor);
  editor->setText("14 mm"); // Pending spin text has not emitted valueChanged.
  placement_mode(window);
  const auto screen = placement_screen(window, {12, 6, 10});
  pointer(window, QEvent::MouseButtonRelease, screen);
  CHECK(control<QComboBox>(window, "point").count() == count);
  pointer(window, QEvent::MouseButtonPress, screen);
  pointer(window, QEvent::MouseButtonRelease, screen);
  CHECK(control<QComboBox>(window, "point").count() == count);
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 14);
  CHECK_FALSE(window.sketch_3d_tools().preview());
  button(window, "stage_point");
  REQUIRE(window.sketch_3d_tools().preview());
  pointer(window, QEvent::MouseButtonPress, screen);
  control<QDoubleSpinBox>(window, "x").setValue(7);
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  pointer(window, QEvent::MouseButtonRelease, screen);
  CHECK(control<QComboBox>(window, "point").count() == count);
  CHECK(control<QDoubleSpinBox>(window, "x").value() == 7);
  CHECK(control<QDoubleSpinBox>(window, "z").value() == 14);
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK(snapshot(window) == before);
  button(window, "stage_point");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* edited = window.session()
                           .part_document()
                           ->find_sketch_3d(Sketch3DId("spatial"))
                           ->find_point(SketchEntityId("b"));
  REQUIRE(edited);
  CHECK(edited->x().explicit_coordinate()->millimeters() == 7);
  CHECK(edited->z().explicit_coordinate()->millimeters() == 14);
}

TEST_CASE("Sketch3D placement rejects unobservable axes and planes and recovers from the origin",
          "[gui][block136-sketch3d-placement]") {
  ShellWindow window;
  prepare_handles(window);
  window.sketch_3d_tools().cancel();
  REQUIRE(window.sketch_3d_tools().begin());
  const auto before = snapshot(window);
  auto camera = window.viewport().camera_bookmark();
  camera.eye = {0, -100, 0};
  camera.target = {0, 0, 0};
  camera.up_direction = {0, 0, 1};
  REQUIRE(window.viewport().restore_camera_bookmark(camera));
  for (int lock : {2, 4}) {
    placement_mode(window, "", lock);
    const auto p = placement_screen(window, {0, 0, 0});
    placement_hover(window, p);
    CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
    pointer(window, QEvent::MouseButtonPress, p);
    pointer(window, QEvent::MouseButtonRelease, p);
    CHECK(control<QComboBox>(window, "point").count() == 1);
    CHECK(control<QLabel>(window, "summary").text().contains("Ansicht ändern"));
    CHECK(snapshot(window) == before);
  }
  placement_mode(window, "", 6);
  const auto p = placement_screen(window, {10, 0, 10});
  placement_hover(window, p);
  REQUIRE(control<QWidget>(window, "placement_overlay").isVisible());
  pointer(window, QEvent::MouseButtonPress, p);
  pointer(window, QEvent::MouseButtonRelease, p);
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* created =
      window.session().part_document()->sketches_3d().back().find_point(SketchEntityId("point.1"));
  REQUIRE(created);
  CHECK(created->y().explicit_coordinate()->millimeters() == 0);
  CHECK(created->x().explicit_coordinate()->millimeters() == Catch::Approx(10).margin(1));
  CHECK(created->z().explicit_coordinate()->millimeters() == Catch::Approx(10).margin(1));
}

TEST_CASE(
    "Sketch3D placement expires on save revision and document boundaries without leaking a point",
    "[gui][block136-sketch3d-placement]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  placement_mode(window);
  const auto p = placement_screen(window, {12, 6, 10});
  pointer(window, QEvent::MouseButtonPress, p);
  REQUIRE(window.viewport().mouseGrabber() == &window.viewport());
  SECTION("save committed state") {
    QTemporaryDir directory;
    REQUIRE(window.save_to(directory.filePath("pending-placement.blcad")));
    GuiDocumentSession opened;
    REQUIRE(opened.open_file(directory.filePath("pending-placement.blcad").toStdString()));
    CHECK(checked(serialize_part_document_to_json(*opened.part_document())) == before);
    CHECK(snapshot(window) == before);
  }
  SECTION("revision") {
    window.session().report_error(Error::validation("test", "Revision changed"));
    window.refresh();
    CHECK(snapshot(window) == before);
  }
  SECTION("document replacement") {
    (void)window.session().task().cancel();
    REQUIRE(window.session().close(true));
    window.refresh();
  }
  CHECK_FALSE(window.sketch_3d_tools().active());
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
}

namespace {
void direction_tuple(ShellWindow& window, const QString& distance, const QString& azimuth,
                     const QString& elevation) {
  control<QLineEdit>(window, "distance").setText(distance);
  control<QLineEdit>(window, "azimuth").setText(azimuth);
  control<QLineEdit>(window, "elevation").setText(elevation);
}
} // namespace

TEST_CASE("Sketch3D distance direction copies precise anchors and commits one reversible candidate",
          "[gui][block136-sketch3d-direction]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  select(window, "placement_anchor", "driven");
  control<QComboBox>(window, "lock").setCurrentIndex(1);
  direction_tuple(window, "10", "0", "30");
  button(window, "stage_direction");
  CHECK(control<QComboBox>(window, "point").currentData().toString() == "point.1");
  CHECK(control<QComboBox>(window, "placement_anchor").currentData().toString() == "driven");
  CHECK_FALSE(control<QCheckBox>(window, "detach").isChecked());
  select(window, "placement_anchor", "point.1");
  direction_tuple(window, "4", "-90", "0");
  button(window, "stage_direction");
  // Zero distance and signed angular boundaries are valid explicit point input.
  direction_tuple(window, "0", "-360", "-90");
  button(window, "stage_direction");
  CHECK(control<QComboBox>(window, "point").count() == 9);
  for (const auto* field : {"distance", "azimuth", "elevation"})
    CHECK(control<QLineEdit>(window, field).text().isEmpty());
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
  REQUIRE(window.sketch_3d_tools().preview());
  CHECK(snapshot(window) == before);
  REQUIRE(window.sketch_3d_tools().apply());
  const auto& sketch = *window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  const auto* first = sketch.find_point(SketchEntityId("point.1"));
  const auto* second = sketch.find_point(SketchEntityId("point.2"));
  const auto* zero = sketch.find_point(SketchEntityId("point.3"));
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(zero);
  CHECK(first->x().explicit_coordinate()->millimeters() == Catch::Approx(13.660254037844386));
  CHECK(first->y().explicit_coordinate()->millimeters() == 4.123456789123);
  CHECK(first->z().explicit_coordinate()->millimeters() == Catch::Approx(5));
  CHECK(second->x().explicit_coordinate()->millimeters() ==
        first->x().explicit_coordinate()->millimeters());
  CHECK(second->y().explicit_coordinate()->millimeters() == Catch::Approx(0.123456789123));
  CHECK(second->z().explicit_coordinate()->millimeters() ==
        first->z().explicit_coordinate()->millimeters());
  CHECK(zero->x().explicit_coordinate()->millimeters() ==
        first->x().explicit_coordinate()->millimeters());
  CHECK(zero->y().explicit_coordinate()->millimeters() ==
        first->y().explicit_coordinate()->millimeters());
  CHECK(zero->z().explicit_coordinate()->millimeters() ==
        first->z().explicit_coordinate()->millimeters());
  CHECK(first->parameter_dependencies().empty());
  CHECK(second->parameter_dependencies().empty());
  CHECK(sketch.find_point(SketchEntityId("driven"))->x().parameter() == ParameterId("px"));
  CHECK(sketch.arcs().size() == 1);
  CHECK(sketch.splines().size() == 1);
  CHECK(sketch.guide_curves().size() == 1);
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("direction.blcad")));
  REQUIRE(window.open_path(directory.filePath("direction.blcad")));
  CHECK(snapshot(window) == after);
}

TEST_CASE("Sketch3D distance direction rejects incomplete nonfinite and bounded inputs atomically",
          "[gui][block136-sketch3d-direction]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  const int count = control<QComboBox>(window, "point").count();
  select(window, "placement_anchor", "b");
  const std::vector<std::array<QString, 3>> invalid{
      {"", "", ""},        {"10", "", "0"},    {"10", "0", ""},
      {"", "0", "0"},      {"nan", "0", "0"},  {"10", "inf", "0"},
      {"10", "0", "-inf"}, {"-1", "0", "0"},   {"1000000001", "0", "0"},
      {"1e309", "0", "0"}, {"10", "361", "0"}, {"10", "-361", "0"},
      {"10", "0", "91"},   {"10", "0", "-91"}, {"1,5", "0", "0"},
      {"10 mm", "0", "0"}, {"2", "abc", "0"}};
  for (const auto& tuple : invalid) {
    CAPTURE(tuple);
    direction_tuple(window, tuple[0], tuple[1], tuple[2]);
    button(window, "stage_direction");
    CHECK(control<QComboBox>(window, "point").count() == count);
    CHECK(control<QLineEdit>(window, "distance").text() == tuple[0]);
    CHECK(control<QLineEdit>(window, "azimuth").text() == tuple[1]);
    CHECK(control<QLineEdit>(window, "elevation").text() == tuple[2]);
    CHECK_FALSE(control<QLabel>(window, "summary").text().isEmpty());
    CHECK(snapshot(window) == before);
    CHECK(window.session().undo_label() == undo);
  }
  direction_tuple(window, "2.5e0", " -90 ", "0");
  button(window, "stage_direction");
  REQUIRE(control<QComboBox>(window, "point").count() == count + 1);
  CHECK(control<QComboBox>(window, "point").currentData().toString() == "point.1");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto* point = window.session()
                          .part_document()
                          ->find_sketch_3d(Sketch3DId("spatial"))
                          ->find_point(SketchEntityId("point.1"));
  REQUIRE(point);
  CHECK(point->x().explicit_coordinate()->millimeters() == 5);
  CHECK(point->y().explicit_coordinate()->millimeters() == Catch::Approx(-2.5));
  CHECK(point->z().explicit_coordinate()->millimeters() == 10);
}

TEST_CASE(
    "Sketch3D pending direction blocks competing authors and clears without losing local edits",
    "[gui][block136-sketch3d-direction]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  point(window, -10, 5, 15);
  REQUIRE(window.sketch_3d_tools().preview());
  const int count = control<QComboBox>(window, "point").count();
  direction_tuple(window, "10", "", "");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK(window.sketch_3d_tools().manipulators().presentations().empty());
  button(window, "stage_point");
  button(window, "stage_line");
  placement_mode(window);
  const auto screen = placement_screen(window, {12, 6, 10});
  placement_hover(window, screen);
  CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
  pointer(window, QEvent::MouseButtonPress, screen);
  pointer(window, QEvent::MouseButtonRelease, screen);
  CHECK(control<QComboBox>(window, "point").count() == count);
  CHECK_FALSE(window.sketch_3d_tools().preview());
  CHECK_FALSE(window.sketch_3d_tools().apply());
  CHECK(snapshot(window) == before);
  button(window, "clear_direction");
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  CHECK(window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"))->points().size() ==
        6);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D direction input cancels live gestures before staging a separate point",
          "[gui][block136-sketch3d-direction]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  point(window, -10, 5, 15);
  select(window, "point", "b");
  select(window, "placement_anchor", "b");
  REQUIRE(window.sketch_3d_tools().preview());
  const int count = control<QComboBox>(window, "point").count();
  GuiSketchScreenPoint release;
  SECTION("point grip") {
    release = press(window, "z");
    numeric(window, "14 mm");
  }
  SECTION("pointer placement") {
    placement_mode(window);
    release = placement_screen(window, {12, 6, 10});
    pointer(window, QEvent::MouseButtonPress, release);
  }
  REQUIRE(window.viewport().mouseGrabber() == &window.viewport());
  direction_tuple(window, "2", "0", "0");
  CHECK(window.viewport().mouseGrabber() != &window.viewport());
  CHECK(window.sketch_3d_tools().manipulators().active_handle_id().empty());
  CHECK_FALSE(control<QWidget>(window, "placement_overlay").isVisible());
  pointer(window, QEvent::MouseButtonRelease, release);
  CHECK(control<QComboBox>(window, "point").count() == count);
  CHECK_FALSE(window.viewport().feature_preview_active());
  button(window, "stage_direction");
  CHECK(control<QComboBox>(window, "point").count() == count + 1);
  REQUIRE(window.sketch_3d_tools().preview());
  REQUIRE(window.sketch_3d_tools().apply());
  const auto& sketch = *window.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  CHECK(sketch.find_point(SketchEntityId("b"))->z().explicit_coordinate()->millimeters() == 10);
  CHECK(sketch.find_point(SketchEntityId("point.2"))->x().explicit_coordinate()->millimeters() ==
        7);
  CHECK(sketch.points().size() == 7);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Sketch3D direction guards pending XYZ overflow and session boundaries",
          "[gui][block136-sketch3d-direction]") {
  ShellWindow window;
  prepare_handles(window);
  const auto before = snapshot(window);
  SECTION("pending uninterpreted XYZ survives rejection") {
    select(window, "point", "b");
    auto* editor = control<QDoubleSpinBox>(window, "z").findChild<QLineEdit*>();
    REQUIRE(editor);
    editor->setText("14 mm");
    direction_tuple(window, "2", "0", "0");
    button(window, "stage_direction");
    CHECK(control<QComboBox>(window, "point").count() == 6);
    CHECK(control<QDoubleSpinBox>(window, "z").value() == 14);
    CHECK(control<QLineEdit>(window, "distance").text() == "2");
    button(window, "clear_direction");
    button(window, "stage_point");
    REQUIRE(window.sketch_3d_tools().preview());
    REQUIRE(window.sketch_3d_tools().apply());
    CHECK(window.session()
              .part_document()
              ->find_sketch_3d(Sketch3DId("spatial"))
              ->find_point(SketchEntityId("b"))
              ->z()
              .explicit_coordinate()
              ->millimeters() == 14);
    return;
  }
  SECTION("resolved coordinate overflow keeps prior local candidate") {
    point(window, 1e9, 0, 0);
    select(window, "placement_anchor", "point.1");
    direction_tuple(window, "1", "0", "0");
    button(window, "stage_direction");
    CHECK(control<QComboBox>(window, "point").count() == 7);
    CHECK(control<QLabel>(window, "summary").text().contains("XYZ-Bereich"));
    CHECK(snapshot(window) == before);
    button(window, "clear_direction");
    REQUIRE(window.sketch_3d_tools().preview());
    REQUIRE(window.sketch_3d_tools().apply());
    return;
  }
  SECTION("Escape") {
    direction_tuple(window, "10", "0", "90");
    button(window, "stage_direction");
    direction_tuple(window, "2", "", "");
    escape_handle(window);
  }
  SECTION("save") {
    direction_tuple(window, "10", "0", "90");
    button(window, "stage_direction");
    direction_tuple(window, "2", "", "");
    QTemporaryDir directory;
    REQUIRE(window.save_to(directory.filePath("pending-direction.blcad")));
    GuiDocumentSession opened;
    REQUIRE(opened.open_file(directory.filePath("pending-direction.blcad").toStdString()));
    CHECK(checked(serialize_part_document_to_json(*opened.part_document())) == before);
  }
  SECTION("revision") {
    direction_tuple(window, "2", "", "");
    window.session().report_error(Error::validation("test", "Revision changed"));
    window.refresh();
  }
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.sketch_3d_tools().active());
  for (const auto* field : {"distance", "azimuth", "elevation"})
    CHECK(control<QLineEdit>(window, field).text().isEmpty());
  edit(window);
  CHECK(control<QComboBox>(window, "placement_anchor").currentIndex() == 0);
  CHECK(control<QLineEdit>(window, "distance").text().isEmpty());
}
