#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
template <class T> T& control(ShellWindow& w, const char* id) {
  auto* p = w.findChild<T*>(QStringLiteral("blcad.sketch3d.") + id);
  REQUIRE(p);
  return *p;
}
void click(ShellWindow& w, const char* id) {
  control<QPushButton>(w, id).click();
}
void select(ShellWindow& w, const char* id, const QString& data) {
  auto& box = control<QComboBox>(w, id);
  const auto row = box.findData(data);
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
void target(ShellWindow& w, int slot, const QString& label) {
  auto& box = control<QComboBox>(w, ("constraint_target_" + std::to_string(slot)).c_str());
  int row = box.findText(label);
  if (row < 0)
    for (int i = 1; i < box.count(); ++i)
      if (box.itemText(i).endsWith(label, Qt::CaseInsensitive)) {
        row = i;
        break;
      }
  INFO(label.toStdString());
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
std::string snapshot(const GuiDocumentSession& s) {
  return f::checked(serialize_part_document_to_json(*s.part_document()));
}
double volume(const GuiDocumentSession& s) {
  const auto* body = s.part_shape_cache()->find_body_shape(BodyId("driven.body"));
  REQUIRE(body);
  return geometry::RectangleExtrusionAdapter{}.summarize(*body).volume_mm3;
}
} // namespace
TEST_CASE(
    "Spatial dimension rebind and fixed face constraints drive an actual Sweep in one operation",
    "[gui][parity-b165][spatial-constraint-authoring][native-spatial-constraints]") {
  ShellWindow w;
  REQUIRE(w.new_part("Spatial dimensions"));
  const Point3 anchor{1.12345678912345, 1.23456789123456, 5};
  REQUIRE(w.session().commit_part_transaction("Source", [&](PartDocument& part) {
    part = sf::straight_sweep();
    auto s = *part.find_sketch_3d(Sketch3DId("driven"));
    (void)f::checked(s.add_point(f::point("face.point", anchor)));
    return part.update_sketch_3d(s);
  }));
  w.refresh();
  if (QApplication::platformName() == "xcb") {
    const auto area =
        QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
    w.move(area.topLeft());
    w.resize(std::min(1280, area.width()), std::min(800, area.height()));
    w.show();
    QGuiApplication::sync();
    qApp->processEvents();
    REQUIRE(w.viewport().native_viewer_available());
  }
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("driven")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(2);
  select(w, "constraint", "distance");
  CHECK_FALSE(control<QDoubleSpinBox>(w, "constraint_value").isEnabled());
  select(w, "constraint_source", QString());
  auto* numeric = control<QDoubleSpinBox>(w, "constraint_value").findChild<QLineEdit*>();
  REQUIRE(numeric);
  numeric->setText("-");
  REQUIRE(QMetaObject::invokeMethod(numeric, "textEdited", Qt::DirectConnection,
                                    Q_ARG(QString, QString("-"))));
  QFocusEvent focus_out(QEvent::FocusOut);
  QApplication::sendEvent(numeric->parentWidget(), &focus_out);
  click(w, "constraint_accept");
  CHECK(numeric->text() == "-");
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  control<QDoubleSpinBox>(w, "constraint_value").setValue(15);
  click(w, "constraint_accept");
  INFO(control<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(control<QComboBox>(w, "constraint").findData("distance") >= 0);
  select(w, "constraint", QString());
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::PointOnFace));
  target(w, 0, "Local point / face.point");
  target(w, 1, "/ top");
  click(w, "constraint_accept");
  INFO(control<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(control<QComboBox>(w, "constraint").findData("constraint.1") >= 0);
  select(w, "constraint", QString());
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Fixed));
  target(w, 0, "Local point / face.point");
  click(w, "constraint_fixed_current");
  click(w, "constraint_accept");
  INFO(control<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  if (QApplication::platformName() == "xcb") {
    w.viewport().fit_feature_preview();
    qApp->processEvents();
    auto& apply = control<QPushButton>(w, "apply");
    CHECK(apply.isVisible());
    CHECK(w.rect().contains(apply.mapTo(&w, apply.rect().bottomRight())));
    auto* area = qobject_cast<QScrollArea*>(control<QTabWidget>(w, "tabs").currentWidget());
    REQUIRE(area);
    area->ensureWidgetVisible(&control<QLabel>(w, "constraint_feedback"));
    qApp->processEvents();
    CHECK_FALSE(area->horizontalScrollBar()->isVisible());
    const auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!root.isEmpty()) {
      REQUIRE(QDir().mkpath(root));
      QGuiApplication::sync();
      qApp->processEvents();
      REQUIRE(w.grab().save(root + "/spatial-constraints.png"));
    }
  }
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  CHECK(volume(w.session()) == Catch::Approx(.64 * 15).margin(1e-6));
  const auto* s = w.session().part_document()->find_sketch_3d(Sketch3DId("driven"));
  REQUIRE(s->constraints().size() == 5);
  REQUIRE(s->find_constraint(SketchConstraintId("constraint.2"))->fixed_position());
  CHECK(*s->find_constraint(SketchConstraintId("constraint.2"))->fixed_position() == anchor);
  CHECK(w.session()
            .part_document()
            ->find_parameter(ParameterId("spatial.length"))
            ->value()
            .millimeters() == 10);
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  CHECK(volume(w.session()) == Catch::Approx(.64 * 10).margin(1e-6));
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto file = directory.filePath("constrained.blcad.json").toStdString();
  REQUIRE(w.session().save_as(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file));
  CHECK(snapshot(reopened) == after);
  CHECK(volume(reopened) == Catch::Approx(.64 * 15).margin(1e-6));
  w.refresh();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("driven")));
  select(w, "constraint", "constraint.2");
  click(w, "constraint_delete");
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  CHECK_FALSE(w.session()
                  .part_document()
                  ->find_sketch_3d(Sketch3DId("driven"))
                  ->find_constraint(SketchConstraintId("constraint.2")));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == after);
}
TEST_CASE("Spatial Smooth rejects a fixed G1-only join and reports actual native curvature",
          "[gui][parity-b165][spatial-constraint-authoring]") {
  ShellWindow w;
  REQUIRE(w.new_part("Endpoint continuity"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    auto s = f::checked(Sketch3D::create(Sketch3DId("join"), "Join"));
    const std::array<Point3, 8> points{Point3{-3, 0, 0}, {-2, 0, 0}, {-1, 0, 0}, {0, 0, 0},
                                       {0, 0, 0},        {1, 0, 0},  {2, 1, 0},  {3, 2, 0}};
    for (std::size_t i = 0; i < points.size(); ++i) {
      const auto id = "p" + std::to_string(i);
      (void)f::checked(s.add_point(f::point(id, points[i])));
      (void)f::checked(s.add_constraint(f::checked(
          SpatialConstraint::create(SketchConstraintId("fixed." + id), SpatialConstraintKind::Fixed,
                                    {f::target(id.c_str())}, {}, points[i]))));
    }
    for (int i = 0; i < 2; ++i) {
      std::vector<SketchPointReference3D> refs;
      for (int p = 0; p < 4; ++p)
        refs.push_back(f::local(("p" + std::to_string(i * 4 + p)).c_str()));
      (void)f::checked(s.add_spline(f::checked(SketchSpline3D::create(
          SketchEntityId(i ? "second" : "first"), SketchSpline3DRepresentation::ControlPoints, refs,
          3, SketchSpline3DContinuity::Curvature))));
    }
    return part.add_sketch_3d(s);
  }));
  w.refresh();
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("join")));
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Smooth));
  target(w, 0, "Local curve / first / end");
  target(w, 1, "Local curve / second / start");
  click(w, "constraint_inspect");
  const auto derivatives = control<QLabel>(w, "constraint_differential").text();
  CHECK(derivatives.contains("0.6666666667"));
  click(w, "constraint_accept");
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Tangent));
  click(w, "constraint_accept");
  INFO(control<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto* s = w.session().part_document()->find_sketch_3d(Sketch3DId("join"));
  REQUIRE(s->constraints().size() == 9);
  CHECK(s->find_constraint(SketchConstraintId("constraint.1"))->kind() ==
        SpatialConstraintKind::Tangent);
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
}
TEST_CASE("Spatial constraint modes retain typed capability selectors and correctable drafts",
          "[gui][parity-b165][spatial-constraint-selectors]") {
  ShellWindow w;
  REQUIRE(w.new_part("Target matrix"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    return part.add_sketch_3d(f::sketch());
  }));
  w.refresh();
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  auto& kind = control<QComboBox>(w, "constraint_kind");
  REQUIRE(kind.count() == 16);
  for (int i = 0; i < 16; ++i) {
    kind.setCurrentIndex(i);
    const auto type = static_cast<SpatialConstraintKind>(i);
    CHECK(control<QComboBox>(w, "constraint_target_0").count() > 1);
    CHECK(control<QComboBox>(w, "constraint_target_1").isEnabled() ==
          (type != SpatialConstraintKind::Fixed && type != SpatialConstraintKind::Radius &&
           type != SpatialConstraintKind::Length));
    CHECK_FALSE(control<QComboBox>(w, "constraint_target_2").isEnabled());
  }
  kind.setCurrentIndex(static_cast<int>(SpatialConstraintKind::Angle));
  control<QComboBox>(w, "constraint_angle").setCurrentIndex(1);
  CHECK(control<QComboBox>(w, "constraint_target_2").isEnabled());
  CHECK(control<QComboBox>(w, "constraint_source").findData("angle") >= 0);
  CHECK(control<QComboBox>(w, "constraint_source").findData("radius") == -1);
  click(w, "constraint_accept");
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  click(w, "constraint_discard");
  w.sketch_3d_tools().cancel();
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().close(true));
  kind.setCurrentIndex(static_cast<int>(SpatialConstraintKind::Radius));
  CHECK_FALSE(w.sketch_3d_tools().active());
}
