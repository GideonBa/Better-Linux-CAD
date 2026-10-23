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
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
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
