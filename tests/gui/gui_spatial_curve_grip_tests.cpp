#include "../../src/gui/shell/spatial_sketch_task_context.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace {
template <class T> T& control(ShellWindow& w, const char* id) {
  auto* p = w.findChild<T*>(QStringLiteral("blcad.sketch3d.") + id);
  REQUIRE(p);
  return *p;
}
std::string snapshot(const GuiDocumentSession& session) {
  return f::checked(serialize_part_document_to_json(*session.part_document()));
}
PartDocument source_part() {
  auto part = f::part();
  (void)f::checked(part.add_expression_parameter(ParameterId("shared.radius"), "Shared radius",
                                                 ParameterType::Length, "radius * 2"));
  auto s = f::checked(Sketch3D::create(Sketch3DId("curves"), "Curve grips"));
  for (const auto& [id, p] : {std::pair{"a", Point3{14, 0, 0}},
                              {"b", Point3{10, 4, 0}},
                              {"c", Point3{6, 0, 0}},
                              {"d", Point3{12, 6, 8}}})
    (void)f::checked(s.add_point(f::point(id, p)));
  (void)f::checked(s.add_arc(f::checked(SketchArc3D::create_three_point(
      SketchEntityId("arc"), f::local("a"), f::local("b"), f::local("c")))));
  (void)f::checked(s.add_spline(f::checked(
      SketchSpline3D::create(SketchEntityId("spline"), SketchSpline3DRepresentation::ControlPoints,
                             {f::local("a"), f::local("b"), f::local("c"), f::local("d")}, 3,
                             SketchSpline3DContinuity::Curvature))));
  (void)f::checked(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("radius"), SpatialConstraintKind::Radius,
      {f::checked(SpatialConstraintTarget::curve(SketchEntityId("arc")))}, f::length(4)))));
  const auto axis = f::checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z")));
  (void)f::checked(s.add_helix(f::checked(SketchHelix3D::create_with_bindings(
      SketchEntityId("owned"), axis,
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(4)))),
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(5)))),
      ParameterId("turns"), SketchHelix3DHandedness::RightHanded))));
  (void)f::checked(s.add_helix(f::checked(SketchHelix3D::create(
      SketchEntityId("shared"), axis, ParameterId("shared.radius"), ParameterId("pitch"),
      ParameterId("turns"), SketchHelix3DHandedness::RightHanded))));
  (void)f::checked(part.add_sketch_3d(s));
  return part;
}
SpatialCurveJet jet(const SpatialSketchTaskContext& context, const Sketch3D& owner, const char* id,
                    double u) {
  return f::checked(
      context.jet(owner, f::checked(SpatialConstraintTarget::curve(SketchEntityId(id))), u));
}
double circle_radius(const SpatialSketchTaskContext& context, const Sketch3D& owner) {
  const auto j = jet(context, owner, "arc", .5);
  const auto c = cross(j.first, j.second);
  return std::pow(std::sqrt(dot(j.first, j.first)), 3) / std::sqrt(dot(c, c));
}
void seed(GuiDocumentSession& s) {
  REQUIRE(s.create_part(DocumentId("grips.part"), "Grips"));
  REQUIRE(s.commit_part_transaction("Source", [](PartDocument& part) {
    part = source_part();
    return Result<std::size_t>::success(1);
  }));
}
void select(ShellWindow& w, const char* id) {
  auto& box = control<QComboBox>(w, "curve");
  const int row = box.findData(QString::fromUtf8(id));
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
  qApp->processEvents();
}
void prepare(ShellWindow& w) {
  seed(w.session());
  const auto area =
      QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
  w.move(area.topLeft());
  w.resize(std::min(1280, area.width()), std::min(800, area.height()));
  w.show();
  w.refresh();
  qApp->processEvents();
  auto camera = w.viewport().camera_bookmark();
  camera.eye = {150, 110, 140};
  camera.target = {4, 0, 7};
  camera.scale = 55;
  REQUIRE(w.viewport().restore_camera_bookmark(camera));
  if (QApplication::platformName() == "xcb") {
    REQUIRE(w.viewport().native_viewer_available());
    w.raise();
    w.activateWindow();
    QGuiApplication::sync();
    qApp->processEvents();
    if (!w.isActiveWindow()) {
      QEventLoop loop;
      QTimer poll, deadline;
      deadline.setSingleShot(true);
      QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (w.isActiveWindow())
          loop.quit();
      });
      QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
      poll.start(5);
      deadline.start(2000);
      loop.exec();
    }
    REQUIRE(w.isActiveWindow());
  }
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("curves")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
  select(w, "owned");
  control<QCheckBox>(w, "curve_grips").setChecked(true);
  w.sketch_3d_tools().refresh_presentation();
  qApp->processEvents();
}
void pointer(ShellWindow& w, QEvent::Type type, GuiSketchScreenPoint p) {
  const QPointF local(p.x, p.y);
  QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                    type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &event);
  qApp->processEvents();
}
GuiSketchScreenPoint press(ShellWindow& w, const char* name) {
  w.sketch_3d_tools().refresh_presentation();
  const auto values = w.sketch_3d_tools().manipulators().presentations();
  const auto found = std::find_if(values.begin(), values.end(), [&](const auto& p) {
    return p.handle_id == std::string("sketch3d.curve.") + name;
  });
  REQUIRE(found != values.end());
  REQUIRE(
      w.viewport().rect().contains(QPoint(qRound(found->endpoint.x), qRound(found->endpoint.y))));
  pointer(w, QEvent::MouseButtonPress, found->endpoint);
  REQUIRE(w.sketch_3d_tools().manipulators().active_handle_id() ==
          std::string("sketch3d.curve.") + name);
  return found->endpoint;
}
void numeric(ShellWindow& w, const QString& value) {
  auto& field = control<QLineEdit>(w, "grip_numeric");
  REQUIRE(field.isVisible());
  REQUIRE_FALSE(field.isReadOnly());
  field.selectAll();
  QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, value);
  QApplication::sendEvent(&field, &event);
  qApp->processEvents();
}
void drag(ShellWindow& w, const char* name, const QString& value) {
  const auto p = press(w, name);
  numeric(w, value);
  REQUIRE(w.sketch_3d_tools().manipulators().numeric_override_active());
  pointer(w, QEvent::MouseMove, {p.x + 5, p.y + 3});
  pointer(w, QEvent::MouseButtonRelease, {p.x + 17, p.y + 11});
  CHECK(w.sketch_3d_tools().manipulators().active_handle_id().empty());
}
} // namespace
TEST_CASE(
    "Spatial curve frames expose only owned scalars and exact candidates preserve discrete sources",
    "[gui][parity-b165][spatial-curve-grip-model]") {
  GuiDocumentSession s;
  seed(s);
  const auto before = snapshot(s);
  const auto& owner = *s.part_document()->find_sketch_3d(Sketch3DId("curves"));
  auto ctx = f::checked(SpatialSketchTaskContext::create(s, owner.id(), {}));
  const auto frames = f::checked(ctx->curve_grips(owner, SketchEntityId("owned")));
  REQUIRE(frames.size() == 2);
  CHECK(f::checked(ctx->curve_grips(owner, SketchEntityId("shared"))).empty());
  for (const auto& frame : frames) {
    const double requested =
        frame.kind == SpatialCurveGripKind::HelixRadius ? 6.12345678912345 : 2.34567891234567;
    const auto moved = f::checked(move_spatial_curve_grip(owner, frame, requested));
    const auto solved = ctx->solve(moved.sketch, moved.options);
    INFO(solved.diagnostic);
    REQUIRE(solved.accepted());
    const auto* h = solved.sketch.find_helix(SketchEntityId("owned"));
    CHECK(
        (frame.kind == SpatialCurveGripKind::HelixRadius ? h->radius_binding() : h->pitch_binding())
            .explicit_length()
            ->millimeters() == requested);
    CHECK(h->turns_parameter() == ParameterId("turns"));
    const auto end = jet(*ctx, solved.sketch, "owned", 1).point;
    const auto start = jet(*ctx, solved.sketch, "owned", 0).point;
    CHECK(end.z ==
          Catch::Approx(3 * h->pitch_binding().explicit_length()->millimeters()).margin(1e-8));
    CHECK(std::hypot(start.x, start.y) ==
          Catch::Approx(h->radius_binding().explicit_length()->millimeters()).margin(1e-8));
  }
  const auto arc = f::checked(ctx->curve_grips(owner, SketchEntityId("arc")));
  REQUIRE(arc.size() == 1);
  const auto moved = f::checked(move_spatial_curve_grip(owner, arc.front(), 6));
  CHECK(circle_radius(*ctx, moved.sketch) == Catch::Approx(6).margin(1e-12));
  CHECK_FALSE(ctx->solve(moved.sketch, moved.options).accepted());
  for (const auto text : {"-3 mm", "0 mm", "nan", "1e10 mm", "2 deg", "-", "1e-10 mm"})
    CHECK_FALSE(valid_spatial_curve_grip_text(text));
  for (const auto text : {"6.12345678912345 mm", " 2.5mm ", "1e-9"})
    CHECK(valid_spatial_curve_grip_text(text));
  CHECK(snapshot(s) == before);
}
TEST_CASE("Spatial curve grips release exact helix values and reject an incompatible arc radius "
          "atomically",
          "[gui][parity-b165][spatial-curve-grips]") {
  ShellWindow w;
  prepare(w);
  const auto before = snapshot(w.session());
  drag(w, "radius", "6.12345678912345 mm");
  drag(w, "pitch", "2.34567891234567 mm");
  CHECK(snapshot(w.session()) == before);
  select(w, "shared");
  CHECK(w.sketch_3d_tools().manipulators().handles().empty());
  select(w, "arc");
  const auto p = press(w, "radius");
  numeric(w, "6 mm");
  pointer(w, QEvent::MouseButtonRelease, {p.x + 9, p.y + 4});
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.viewport().feature_preview_active());
  const auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (QApplication::platformName() == "xcb" && !root.isEmpty()) {
    REQUIRE(QDir().mkpath(root));
    QGuiApplication::sync();
    qApp->processEvents();
    REQUIRE(w.grab().save(root + "/spatial-curve-grips.png"));
  }
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  const auto& part = *w.session().part_document();
  const auto* owner = part.find_sketch_3d(Sketch3DId("curves"));
  const auto* h = owner->find_helix(SketchEntityId("owned"));
  CHECK(h->radius_binding().explicit_length()->millimeters() == 6.12345678912345);
  CHECK(h->pitch_binding().explicit_length()->millimeters() == 2.34567891234567);
  CHECK(part.find_parameter(ParameterId("turns"))->value().count_value() == 3);
  CHECK(part.find_parameter(ParameterId("shared.radius"))->formula() == "radius * 2");
  const auto final_context =
      f::checked(SpatialSketchTaskContext::create(w.session(), owner->id(), {}));
  CHECK(circle_radius(*final_context, *owner) == Catch::Approx(4).margin(1e-12));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto file = directory.filePath("grips.blcad.json").toStdString();
  REQUIRE(w.session().save_as(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file));
  CHECK(snapshot(reopened) == after);
}
TEST_CASE("Spatial curve interruption restores the complete preceding local preview",
          "[gui][parity-b165][spatial-curve-grips]") {
  ShellWindow w;
  prepare(w);
  const auto before = snapshot(w.session());
  drag(w, "radius", "6.125 mm");
  (void)press(w, "pitch");
  numeric(w, "7.25 mm");
  SECTION("Escape") {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&control<QLineEdit>(w, "grip_numeric"), &event);
  }
  SECTION("Lost capture") {
    QEvent event(QEvent::UngrabMouse);
    QApplication::sendEvent(&w.viewport(), &event);
  }
  SECTION("Camera") {
    auto camera = w.viewport().camera_bookmark();
    camera.scale *= 1.15;
    REQUIRE(w.viewport().restore_camera_bookmark(camera));
  }
  SECTION("Resize") {
    w.resize(w.width() - 23, w.height() - 13);
    qApp->processEvents();
  }
  SECTION("Window focus") {
    QEvent event(QEvent::WindowDeactivate);
    QApplication::sendEvent(&w, &event);
  }
  SECTION("Curve selection") {
    select(w, "arc");
  }
  CHECK(w.sketch_3d_tools().manipulators().active_handle_id().empty());
  CHECK(w.viewport().mouseGrabber() != &w.viewport());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto* h = w.session()
                      .part_document()
                      ->find_sketch_3d(Sketch3DId("curves"))
                      ->find_helix(SketchEntityId("owned"));
  CHECK(h->radius_binding().explicit_length()->millimeters() == 6.125);
  CHECK(h->pitch_binding().explicit_length()->millimeters() == 5);
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
}

TEST_CASE("Spatial arc radius and spline defining-point grips keep their source identities",
          "[gui][parity-b165][spatial-curve-grips]") {
  ShellWindow w;
  prepare(w);
  const auto before = snapshot(w.session());
  control<QTabWidget>(w, "tabs").setCurrentIndex(2);
  auto& relation = control<QComboBox>(w, "constraint");
  const int row = relation.findData("radius");
  REQUIRE(row >= 0);
  relation.setCurrentIndex(row);
  control<QPushButton>(w, "constraint_delete").click();
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
  select(w, "arc");
  constexpr double requested = 6.12345678912345;
  drag(w, "radius", "6.12345678912345 mm");
  select(w, "spline");
  control<QListWidget>(w, "curve_points").setCurrentRow(3);
  control<QPushButton>(w, "curve_edit_point").click();
  REQUIRE(control<QTabWidget>(w, "tabs").currentIndex() == 0);
  const auto handles = w.sketch_3d_tools().manipulators().presentations();
  const auto z = std::find_if(handles.begin(), handles.end(),
                              [](const auto& h) { return h.handle_id == "sketch3d.position.z"; });
  REQUIRE(z != handles.end());
  const auto p = z->endpoint;
  pointer(w, QEvent::MouseButtonPress, p);
  REQUIRE(w.sketch_3d_tools().manipulators().active_handle_id() == "sketch3d.position.z");
  numeric(w, "11.12345678912345 mm");
  pointer(w, QEvent::MouseButtonRelease, {p.x + 9, p.y + 7});
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  const auto* owner = w.session().part_document()->find_sketch_3d(Sketch3DId("curves"));
  const auto context = f::checked(SpatialSketchTaskContext::create(w.session(), owner->id(), {}));
  CHECK(circle_radius(*context, *owner) == Catch::Approx(requested).margin(1e-11));
  CHECK(owner->find_point(SketchEntityId("b"))->y().explicit_coordinate()->millimeters() ==
        requested);
  CHECK(owner->find_point(SketchEntityId("d"))->z().explicit_coordinate()->millimeters() ==
        11.12345678912345);
  CHECK(jet(*context, *owner, "spline", 1).point.z ==
        Catch::Approx(11.12345678912345).margin(1e-12));
  CHECK(owner->find_spline(SketchEntityId("spline"))->ordered_points().back().local_point() ==
        SketchEntityId("d"));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
}
