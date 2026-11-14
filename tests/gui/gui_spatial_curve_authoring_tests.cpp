#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace {
template <class T> T& control(ShellWindow& w, const char* id) {
  auto* p = w.findChild<T*>(QStringLiteral("blcad.sketch3d.") + id);
  REQUIRE(p);
  return *p;
}
void click(ShellWindow& w, const char* id) {
  control<QPushButton>(w, id).click();
}
void select(ShellWindow& w, const char* id, const QVariant& data) {
  auto& box = control<QComboBox>(w, id);
  const int row = box.findData(data);
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
void append(ShellWindow& w, const QString& label) {
  auto& box = control<QComboBox>(w, "curve_point_source");
  const int row = box.findText(label);
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
  click(w, "curve_add_point");
}
std::string snapshot(const GuiDocumentSession& s) {
  return f::checked(serialize_part_document_to_json(*s.part_document()));
}
void seed(ShellWindow& w) {
  REQUIRE(w.new_part("Curves"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    auto owner = f::checked(Sketch3D::create(Sketch3DId("owner"), "Owner"));
    for (const auto& [id, p] : {std::pair{"a", Point3{0, 0, 0}},
                                {"b", Point3{5, 4, 2}},
                                {"c", Point3{10, 0, 5}},
                                {"d", Point3{12, 8, 9}}})
      (void)f::checked(owner.add_point(f::point(id, p)));
    return part.add_sketch_3d(owner);
  }));
  w.refresh();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("owner")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
}
} // namespace
TEST_CASE("Spatial curve task creates a point-free helix and publishes owned resources only once",
          "[gui][parity-b165][spatial-curve-authoring]") {
  ShellWindow w;
  REQUIRE(w.new_part("Owned helix"));
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin());
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
  control<QComboBox>(w, "curve_kind").setCurrentIndex(5);
  select(w, "helix_axis", -3);
  control<QSpinBox>(w, "helix_turns").setValue(2);
  click(w, "curve_accept");
  INFO(control<QLabel>(w, "curve_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  const auto& part = *w.session().part_document();
  REQUIRE(part.sketches_3d().size() == 1);
  const auto& owner = part.sketches_3d().front();
  CHECK(owner.points().empty());
  REQUIRE(owner.helices().size() == 1);
  const auto& helix = owner.helices().front();
  REQUIRE(helix.radius_binding().explicit_length());
  CHECK(helix.radius_binding().explicit_length()->millimeters() == 4);
  CHECK(helix.pitch_binding().explicit_length()->millimeters() == 5);
  CHECK(part.find_parameter(helix.turns_parameter())->value().count_value() == 2);
  REQUIRE(geometry::Sketch3DGeometryAdapter{}.convert(part, owner));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto file = directory.filePath("helix.blcad.json").toStdString();
  REQUIRE(w.session().save_as(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file));
  CHECK(snapshot(reopened) == after);
}
TEST_CASE(
    "Spatial curve task authors every point-based curve with stable identity and explicit rebind",
    "[gui][parity-b165][spatial-curve-authoring]") {
  ShellWindow w;
  seed(w);
  const auto before = snapshot(w.session());
  for (int kind : {0, 1, 2, 3, 4}) {
    select(w, "curve", QString());
    control<QComboBox>(w, "curve_kind").setCurrentIndex(kind);
    control<QLineEdit>(w, "curve_id").setText(QString("created.%1").arg(kind));
    for (const auto& p : {"a", "b"})
      append(w, QString("Local / ") + p);
    if (kind != 0)
      append(w, "Local / c");
    if (kind == 3 || kind == 4)
      append(w, "Local / d");
    click(w, "curve_accept");
    INFO(control<QLabel>(w, "curve_feedback").text().toStdString());
    REQUIRE(control<QComboBox>(w, "curve").findData(QString("created.%1").arg(kind)) >= 0);
  }
  select(w, "curve", "created.2");
  control<QListWidget>(w, "curve_points").setCurrentRow(1);
  click(w, "curve_remove_point");
  append(w, "Linked construction / external.point");
  control<QListWidget>(w, "curve_points").setCurrentRow(2);
  click(w, "curve_point_up");
  click(w, "curve_accept");
  INFO(control<QLabel>(w, "curve_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  const auto* owner = w.session().part_document()->find_sketch_3d(Sketch3DId("owner"));
  REQUIRE(owner->lines().size() == 1);
  REQUIRE(owner->polylines().size() == 1);
  REQUIRE(owner->arcs().size() == 1);
  REQUIRE(owner->splines().size() == 2);
  CHECK(owner->arcs().front().id().value() == "created.2");
  REQUIRE(owner->arcs().front().intermediate().construction_point());
  CHECK(owner->arcs().front().intermediate().construction_point()->value() == "external.point");
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  w.refresh();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("owner")));
  select(w, "curve", "created.0");
  click(w, "curve_delete");
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  CHECK_FALSE(w.session()
                  .part_document()
                  ->find_sketch_3d(Sketch3DId("owner"))
                  ->find_line(SketchEntityId("created.0")));
}
TEST_CASE("Spatial helix editing preserves shared formulas until an explicit scalar rebind",
          "[gui][parity-b165][spatial-curve-bindings]") {
  ShellWindow w;
  REQUIRE(w.new_part("Bound helix"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    (void)f::checked(part.add_expression_parameter(ParameterId("radius.formula"), "Formula radius",
                                                   ParameterType::Length, "radius * 2"));
    auto owner = f::sketch();
    const auto* helix = owner.find_helix(SketchEntityId("helix"));
    (void)f::checked(owner.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
        helix->id(), helix->axis(),
        f::checked(SketchLength3D::create_parameter(ParameterId("radius.formula"))),
        helix->pitch_binding(), helix->turns_parameter(), helix->handedness()))));
    return part.add_sketch_3d(owner);
  }));
  w.refresh();
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  select(w, "curve", "helix");
  CHECK_FALSE(control<QDoubleSpinBox>(w, "helix_radius").isEnabled());
  CHECK(control<QDoubleSpinBox>(w, "helix_radius").value() == 8);
  select(w, "helix_radius_source", QString());
  control<QDoubleSpinBox>(w, "helix_radius").setValue(6.125);
  click(w, "curve_accept");
  INFO(control<QLabel>(w, "curve_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.sketch_3d_tools().apply());
  const auto& part = *w.session().part_document();
  const auto* helix =
      part.find_sketch_3d(Sketch3DId("spatial"))->find_helix(SketchEntityId("helix"));
  REQUIRE(helix->radius_binding().explicit_length());
  CHECK(helix->radius_binding().explicit_length()->millimeters() == 6.125);
  CHECK(helix->pitch_binding().parameter_id().value() == "pitch");
  CHECK(helix->turns_parameter().value() == "turns");
  CHECK(part.find_parameter(ParameterId("radius"))->value().millimeters() == 4);
  CHECK(part.find_parameter(ParameterId("radius.formula"))->formula() == "radius * 2");
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
}
TEST_CASE("Spatial equation correction and guide metadata remain local and dependency checked",
          "[gui][parity-b165][spatial-curve-authoring]") {
  ShellWindow w;
  seed(w);
  const auto before = snapshot(w.session());
  control<QComboBox>(w, "curve_kind").setCurrentIndex(6);
  control<QLineEdit>(w, "equation_x").setText("unknown_symbol");
  click(w, "curve_accept");
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == before);
  control<QLineEdit>(w, "equation_x").setText("10mm*t");
  control<QLineEdit>(w, "equation_y").setText("3mm*t*t");
  click(w, "curve_accept");
  INFO(control<QLabel>(w, "curve_feedback").text().toStdString());
  control<QComboBox>(w, "curve_guide_role").setCurrentIndex(2);
  click(w, "curve_guide_add");
  click(w, "curve_delete");
  CHECK(control<QLabel>(w, "curve_feedback").text().contains("guide", Qt::CaseInsensitive));
  select(w, "curve_guide", "guide.1");
  click(w, "curve_guide_remove");
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto* owner = w.session().part_document()->find_sketch_3d(Sketch3DId("owner"));
  REQUIRE(owner->equation_curves().size() == 1);
  CHECK(owner->guide_curves().empty());
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
}

TEST_CASE("Native spatial curve controls preserve a reachable footer and cancel the entire draft",
          "[gui][parity-b165][native-spatial-curves]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT curve presentation");
  ShellWindow w;
  REQUIRE(w.new_part("Native curves"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    return part.add_sketch_3d(f::sketch("spatial", true));
  }));
  const auto area =
      QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
  w.move(area.topLeft());
  w.resize(std::min(1280, area.width()), std::min(800, area.height()));
  w.show();
  w.refresh();
  QGuiApplication::sync();
  qApp->processEvents();
  REQUIRE(w.viewport().native_viewer_available());
  w.viewport().fit_all();
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
  select(w, "curve", "helix");
  select(w, "helix_radius_source", QString());
  control<QDoubleSpinBox>(w, "helix_radius").setValue(6.5);
  click(w, "curve_accept");
  REQUIRE(w.sketch_3d_tools().preview());
  w.viewport().fit_feature_preview();
  qApp->processEvents();
  auto& apply = control<QPushButton>(w, "apply");
  CHECK(apply.isVisible());
  CHECK(w.rect().contains(apply.mapTo(&w, apply.rect().bottomRight())));
  CHECK(w.viewport().width() > 180);
  CHECK(w.viewport().height() > 180);
  auto* area_widget = qobject_cast<QScrollArea*>(control<QTabWidget>(w, "tabs").currentWidget());
  REQUIRE(area_widget);
  area_widget->ensureWidgetVisible(&control<QLabel>(w, "curve_feedback"));
  qApp->processEvents();
  CHECK_FALSE(area_widget->horizontalScrollBar()->isVisible());
  const auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!root.isEmpty()) {
    REQUIRE(QDir().mkpath(root));
    QGuiApplication::sync();
    qApp->processEvents();
    REQUIRE(w.grab().save(root + "/spatial-curves.png"));
  }
  w.sketch_3d_tools().cancel();
  CHECK(snapshot(w.session()) == before);
  CHECK_FALSE(w.viewport().feature_preview_active());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  select(w, "curve", "helix");
  CHECK(control<QDoubleSpinBox>(w, "helix_radius").value() == 4);
  w.sketch_3d_tools().cancel();
}

TEST_CASE("Spatial curve edits retain untouched owned scalar and modeling tolerance precision",
          "[gui][parity-b165][spatial-curve-bindings]") {
  ShellWindow w;
  REQUIRE(w.new_part("Precision"));
  constexpr double radius = 4.12345678912345, pitch = 5.23456789123456,
                   tolerance = .000123456789123;
  REQUIRE(w.session().commit_part_transaction("Source", [&](PartDocument& part) {
    part = f::part();
    auto owner = f::sketch("spatial", true);
    const auto helix = *owner.find_helix(SketchEntityId("helix"));
    (void)f::checked(owner.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
        helix.id(), helix.axis(),
        f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(radius)))),
        f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(pitch)))),
        helix.turns_parameter(), helix.handedness()))));
    const auto equation = *owner.find_equation_curve(SketchEntityId("equation"));
    (void)f::checked(owner.replace_equation_curve(f::checked(
        EquationCurve::create(equation.id(), equation.mode(), equation.variable(), equation.lower(),
                              equation.upper(), equation.coordinates(), tolerance))));
    return part.add_sketch_3d(owner);
  }));
  w.refresh();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  select(w, "curve", "helix");
  control<QComboBox>(w, "helix_handedness").setCurrentIndex(1);
  click(w, "curve_accept");
  select(w, "curve", "equation");
  control<QLineEdit>(w, "equation_x").setText("2mm*u");
  click(w, "curve_accept");
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto* owner = w.session().part_document()->find_sketch_3d(Sketch3DId("spatial"));
  CHECK(owner->find_helix(SketchEntityId("helix"))
            ->radius_binding()
            .explicit_length()
            ->millimeters() == radius);
  CHECK(owner->find_helix(SketchEntityId("helix"))
            ->pitch_binding()
            .explicit_length()
            ->millimeters() == pitch);
  CHECK(owner->find_equation_curve(SketchEntityId("equation"))->tolerance_mm() == tolerance);
}
