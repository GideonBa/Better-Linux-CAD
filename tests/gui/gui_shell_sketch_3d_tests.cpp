#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLabel>
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
