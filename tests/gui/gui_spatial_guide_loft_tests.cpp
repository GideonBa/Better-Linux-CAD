#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace {
template <class T> T& control(ShellWindow& w, const char* id) {
  auto* p = w.findChild<T*>(QString::fromUtf8(id));
  REQUIRE(p);
  return *p;
}
template <class T> T& spatial(ShellWindow& w, const char* id) {
  return control<T>(w, ("blcad.sketch3d." + std::string(id)).c_str());
}
void click(ShellWindow& w, const char* id) {
  spatial<QPushButton>(w, id).click();
}
void select(QComboBox& c, const QVariant& data) {
  const auto row = c.findData(data);
  INFO(data.toString().toStdString());
  REQUIRE(row >= 0);
  c.setCurrentIndex(row);
}
void choose(QComboBox& c, const QString& text) {
  const auto row = c.findText(text);
  INFO(text.toStdString());
  REQUIRE(row >= 0);
  c.setCurrentIndex(row);
}
std::string snapshot(const GuiDocumentSession& s) {
  return f::checked(serialize_part_document_to_json(*s.part_document()));
}
PartDocument inputs() {
  auto p = f::checked(PartDocument::create(DocumentId("duct"), "Spatial guide duct"));
  auto owner = f::checked(Sketch3D::create(Sketch3DId("guide"), "Guides"));
  for (const auto& [id, point] :
       std::array<std::pair<const char*, Point3>, 6>{{{"a", {0, 0, 0}},
                                                      {"b", {0, 2, 20. / 3}},
                                                      {"c", {0, -2, 40. / 3}},
                                                      {"d", {0, 0, 20}},
                                                      {"q1", {0, 0, 20. / 3}},
                                                      {"q2", {0, 0, 40. / 3}}}}) {
    (void)f::checked(owner.add_point(f::point(id, point)));
    if (std::string_view(id) != "b" && std::string_view(id) != "c")
      (void)f::checked(owner.add_constraint(f::checked(
          SpatialConstraint::create(SketchConstraintId(std::string("fixed.") + id),
                                    SpatialConstraintKind::Fixed, {f::target(id)}, {}, point))));
  }
  for (const auto& [id, z] : {std::pair{"b", 20. / 3}, {"c", 40. / 3}}) {
    const DatumAxisId axis(std::string("axis.") + id);
    (void)f::checked(p.add_datum_axis(
        f::checked(DatumAxis::create_explicit(axis, "Y at control", {0, 0, z}, {0, 1, 0}))));
    const auto target = f::checked(SpatialConstraintTarget::axis(f::checked(
        AxisReference::create_datum_axis(PartFeatureInputRole::SpatialConstraint, axis))));
    (void)f::checked(owner.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId(std::string("on.") + id),
                                  SpatialConstraintKind::PointOnLine, {f::target(id), target}))));
  }
  (void)f::checked(p.add_sketch_3d(owner));
  auto sections = f::checked(Sketch3D::create(Sketch3DId("sections"), "Sections"));
  for (int i = 0; i < 3; ++i) {
    const auto suffix = std::to_string(i);
    (void)f::checked(sections.add_point(f::point("start." + suffix, {0, 0, 10. * i})));
    (void)f::checked(sections.add_point(f::point("end." + suffix, {4, 0, 10. * i})));
    (void)f::checked(sections.add_line(f::checked(
        SketchLine3D::create(SketchEntityId("line." + suffix), SketchEntityId("start." + suffix),
                             SketchEntityId("end." + suffix)))));
  }
  (void)f::checked(p.add_sketch_3d(sections));
  for (int i = 0; i < 3; ++i) {
    const auto suffix = std::to_string(i);
    (void)f::checked(p.add_path_curve(f::checked(PathCurve::create(
        PathCurveId("section." + suffix), "Section " + suffix,
        {f::checked(PathSegmentReference::create_sketch_3d(
            sections.id(), SketchEntityId("line." + suffix), Sketch3DPathCurveKind::Line))},
        PathClosure::Open, PathOrientationRule::MinimumTwist))));
  }
  return p;
}
void target(ShellWindow& w, int slot, const char* id) {
  choose(spatial<QComboBox>(w, ("constraint_target_" + std::to_string(slot)).c_str()),
         QString("Local point / ") + id);
}
void dimension(ShellWindow& w, const char* name, const char* point, const char* anchor,
               double value) {
  select(spatial<QComboBox>(w, "constraint"), QString());
  spatial<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Distance));
  spatial<QLineEdit>(w, "constraint_id").setText(name);
  target(w, 0, point);
  target(w, 1, anchor);
  spatial<QDoubleSpinBox>(w, "constraint_value").setValue(value);
  click(w, "constraint_accept");
  INFO(spatial<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(spatial<QComboBox>(w, "constraint").findData(name) >= 0);
}
void set_dimension(ShellWindow& w, const char* id, double value) {
  select(spatial<QComboBox>(w, "constraint"), QString::fromUtf8(id));
  spatial<QDoubleSpinBox>(w, "constraint_value").setValue(value);
  click(w, "constraint_accept");
  INFO(spatial<QLabel>(w, "constraint_feedback").text().toStdString());
}
// Independent historical path oracle: cubic polynomial, 32 chord segments and arc-length ratio.
Point3 guide_point(double amplitude, double ratio) {
  const auto bezier = [&](double u) {
    return Point3{0, 3 * amplitude * u * (1 - u) * (1 - 2 * u), 20 * u};
  };
  const auto distance = [](Point3 a, Point3 b) {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
  };
  double length = 0;
  for (int i = 1; i <= 32; ++i)
    length += distance(bezier((i - 1) / 32.), bezier(i / 32.));
  double remaining = ratio * length;
  for (int i = 1; i <= 32; ++i) {
    auto a = bezier((i - 1) / 32.), b = bezier(i / 32.);
    const double span = distance(a, b);
    if (remaining <= span + 1e-12) {
      const auto t = remaining / span;
      return {0, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
    remaining -= span;
  }
  return bezier(1);
}
void witnesses(const GuiDocumentSession& session, const BodyId& body, double amplitude) {
  const auto* shape = session.part_shape_cache()->find_body_shape(body);
  REQUIRE(shape);
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(*shape);
  CHECK(summary.solid_count == 0);
  CHECK_FALSE(summary.is_null);
  for (int i = 0; i <= 8; ++i) {
    const double ratio = i / 8.;
    const auto g = guide_point(amplitude, ratio);
    INFO("Station " << ratio << " guide " << g.y << ", " << g.z);
    CHECK(blcad::test::distance_to_faces(*shape, g) < 2e-6);
    CHECK(blcad::test::distance_to_faces(*shape, {2, g.y / 2, (g.z + 20 * ratio) / 2}) < 2e-6);
    CHECK(blcad::test::distance_to_faces(*shape, {2, g.y / 2 + 2, (g.z + 20 * ratio) / 2}) > 1);
  }
}
} // namespace
TEST_CASE("Spatial task authors a dimensioned guide and verifies native Loft surface witnesses",
          "[gui][parity-b165][spatial-guide-loft][native-spatial-guide-loft]") {
  ShellWindow w;
  REQUIRE(w.new_part("Guide driven Loft"));
  REQUIRE(w.session().commit_part_transaction("Sources", [](PartDocument& p) {
    p = inputs();
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  if (QApplication::platformName() == "xcb") {
    const auto area =
        QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
    w.move(area.topLeft());
    w.resize(std::min(1280, area.width()), std::min(800, area.height()));
    w.show();
    qApp->processEvents();
    REQUIRE(w.viewport().native_viewer_available());
    auto camera = w.viewport().camera_bookmark();
    camera.eye = {35, 28, 40};
    camera.target = {2, 0, 10};
    camera.scale = 30;
    REQUIRE(w.viewport().restore_camera_bookmark(camera));
  }
  const auto before = snapshot(w.session());
  std::vector<std::string> history{before};
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("guide")));
  spatial<QTabWidget>(w, "tabs").setCurrentIndex(1);
  spatial<QComboBox>(w, "curve_kind").setCurrentIndex(4);
  spatial<QLineEdit>(w, "curve_id").setText("spline");
  for (const auto* id : {"a", "b", "c", "d"}) {
    choose(spatial<QComboBox>(w, "curve_point_source"), QString("Local / ") + id);
    click(w, "curve_add_point");
  }
  click(w, "curve_accept");
  INFO(spatial<QLabel>(w, "curve_feedback").text().toStdString());
  REQUIRE(spatial<QComboBox>(w, "curve").findData("spline") >= 0);
  spatial<QComboBox>(w, "curve_guide_role").setCurrentIndex(2);
  click(w, "curve_guide_add");
  spatial<QTabWidget>(w, "tabs").setCurrentIndex(2);
  dimension(w, "amp.b", "b", "q1", 2);
  dimension(w, "amp.c", "c", "q2", 2);
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  history.push_back(snapshot(w.session()));
  auto& tools = w.feature_tools();
  REQUIRE(tools.begin_spatial(ShellFeatureTools::Command::PathCurve));
  choose(tools.spatial_source_editor(), "Guides / spline");
  tools.spatial_add_button().click();
  REQUIRE(tools.apply());
  const auto path = w.session().part_document()->path_curves().back().id();
  history.push_back(snapshot(w.session()));
  REQUIRE(tools.begin_spatial(ShellFeatureTools::Command::Loft));
  tools.spatial_mode_editor().setCurrentIndex(2);
  for (int i = 0; i < 3; ++i) {
    choose(tools.spatial_source_editor(), QString("Section %1 (Open Path)").arg(i));
    tools.spatial_add_button().click();
  }
  select(tools.spatial_guide_editor(), QString::fromStdString(path.value()));
  tools.spatial_add_guide_button().click();
  REQUIRE(tools.apply());
  history.push_back(snapshot(w.session()));
  const auto body = w.session()
                        .part_document()
                        ->loft_features()
                        .back()
                        .body_result_context()
                        .effective_produced_body();
  witnesses(w.session(), body, 2);
  const auto original_loft = history.back();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("guide")));
  spatial<QTabWidget>(w, "tabs").setCurrentIndex(2);
  set_dimension(w, "amp.b", 3);
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == original_loft);
  CHECK_FALSE(spatial<QPushButton>(w, "apply").isEnabled());
  set_dimension(w, "amp.c", 3);
  REQUIRE(w.sketch_3d_tools().preview());
  // Reject a conflicting relation, retaining the accepted dimension edits for correction.
  select(spatial<QComboBox>(w, "constraint"), QString());
  spatial<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Distance));
  spatial<QLineEdit>(w, "constraint_id").setText("conflict");
  target(w, 0, "b");
  target(w, 1, "q1");
  spatial<QDoubleSpinBox>(w, "constraint_value").setValue(99);
  click(w, "constraint_accept");
  CHECK(spatial<QComboBox>(w, "constraint").findData("conflict") < 0);
  CHECK_FALSE(w.sketch_3d_tools().preview());
  CHECK(snapshot(w.session()) == original_loft);
  if (QApplication::platformName() == "xcb") {
    qApp->processEvents();
    for (auto* label : {&spatial<QLabel>(w, "solver_info"), &spatial<QLabel>(w, "summary")}) {
      INFO(label->text().toStdString());
      CHECK(label->height() >= label->heightForWidth(label->width()));
    }
  }
  click(w, "constraint_discard");
  CHECK(spatial<QLabel>(w, "solver_info").text().startsWith("DOF: 0 total"));
  REQUIRE(w.sketch_3d_tools().preview());
  if (QApplication::platformName() == "xcb") {
    qApp->processEvents();
    auto& tabs = spatial<QTabWidget>(w, "tabs");
    auto* area = qobject_cast<QScrollArea*>(tabs.currentWidget());
    REQUIRE(area);
    CHECK_FALSE(area->horizontalScrollBar()->isVisible());
    for (auto* label : {&spatial<QLabel>(w, "solver_info"), &spatial<QLabel>(w, "summary")})
      CHECK(label->height() >= label->heightForWidth(label->width()));
  }
  if (const auto directory = qEnvironmentVariable("BLCAD_CAPTURE_DIR"); !directory.isEmpty()) {
    QDir().mkpath(directory);
    REQUIRE(w.grab().save(directory + "/spatial-guide-loft.png"));
  }
  REQUIRE(w.sketch_3d_tools().apply());
  history.push_back(snapshot(w.session()));
  witnesses(w.session(), body, 3);
  const auto& committed = *w.session().part_document()->find_sketch_3d(Sketch3DId("guide"));
  CHECK(committed.points().size() == 6);
  CHECK(committed.splines().size() == 1);
  REQUIRE(committed.guide_curves().size() == 1);
  CHECK(committed.guide_curves().front().source_curve() == SketchEntityId("spline"));
  CHECK(committed.find_point(SketchEntityId("b"))->y().explicit_coordinate()->millimeters() ==
        Catch::Approx(3).margin(1e-7));
  CHECK(committed.find_point(SketchEntityId("c"))->y().explicit_coordinate()->millimeters() ==
        Catch::Approx(-3).margin(1e-7));
  CHECK(w.session().part_document()->loft_features().back().guide_curves() ==
        std::vector<PathCurveId>{path});
  const auto final = history.back();
  for (std::size_t i = history.size() - 1; i > 0; --i) {
    REQUIRE(w.session().undo());
    CHECK(snapshot(w.session()) == history[i - 1]);
  }
  for (std::size_t i = 1; i < history.size(); ++i) {
    REQUIRE(w.session().redo());
    CHECK(snapshot(w.session()) == history[i]);
  }
  witnesses(w.session(), body, 3);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto file = directory.filePath("guide-loft.blcad").toStdString();
  REQUIRE(w.session().save_as(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file));
  CHECK(snapshot(reopened) == final);
  witnesses(reopened, body, 3);
}

TEST_CASE(
    "New spatial equation intent remains a selectable native Sweep path with unique point ids",
    "[gui][parity-b165][spatial-equation-path][native-spatial-guide-loft]") {
  ShellWindow w;
  REQUIRE(w.new_part("Equation path"));
  REQUIRE(w.session().commit_part_transaction("Profile", [](PartDocument& p) {
    p = f::part();
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  if (QApplication::platformName() == "xcb") {
    const auto area =
        QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
    w.move(area.topLeft());
    w.resize(std::min(1280, area.width()), std::min(800, area.height()));
    w.show();
    qApp->processEvents();
    REQUIRE(w.viewport().native_viewer_available());
  }
  const auto original = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin());
  spatial<QTabWidget>(w, "tabs").setCurrentIndex(1);
  spatial<QComboBox>(w, "curve_kind").setCurrentIndex(6);
  spatial<QLineEdit>(w, "curve_id").setText("point.1");
  spatial<QLineEdit>(w, "equation_x").setText("0mm");
  spatial<QLineEdit>(w, "equation_z").setText("10mm*t");
  click(w, "curve_accept");
  spatial<QTabWidget>(w, "tabs").setCurrentIndex(0);
  click(w, "stage_point");
  REQUIRE(spatial<QComboBox>(w, "point").findData("point.2") >= 0);
  REQUIRE(w.sketch_3d_tools().preview());
  REQUIRE(w.sketch_3d_tools().apply());
  const auto authored = snapshot(w.session());
  const auto owner = w.session().part_document()->sketches_3d().back().id();
  auto& tools = w.feature_tools();
  REQUIRE(tools.begin_spatial(ShellFeatureTools::Command::PathCurve));
  choose(tools.spatial_source_editor(), "3D Sketch 1 / point.1");
  tools.spatial_add_button().click();
  REQUIRE(tools.apply());
  const auto path = w.session().part_document()->path_curves().back().id();
  REQUIRE(tools.begin_spatial(ShellFeatureTools::Command::Sweep));
  choose(tools.spatial_source_editor(), "Base / base.profile");
  select(tools.spatial_path_editor(), QString::fromStdString(path.value()));
  REQUIRE(tools.apply());
  const auto body = w.session()
                        .part_document()
                        ->sweep_features()
                        .back()
                        .body_result_context()
                        .effective_produced_body();
  const auto verify = [&](const GuiDocumentSession& s) {
    const auto* shape = s.part_shape_cache()->find_body_shape(body);
    REQUIRE(shape);
    CHECK(blcad::test::integrated_solid_volume(*shape).volume_mm3 ==
          Catch::Approx(480).margin(1e-6));
    const auto* sketch = s.part_document()->find_sketch_3d(owner);
    REQUIRE(sketch);
    REQUIRE(sketch->find_equation_curve(SketchEntityId("point.1")));
    CHECK(sketch->find_equation_curve(SketchEntityId("point.1"))->coordinates()[2] == "10mm*t");
    CHECK(sketch->find_point(SketchEntityId("point.2")));
  };
  verify(w.session());
  const auto final = snapshot(w.session());
  REQUIRE(w.session().undo());
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == authored);
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == original);
  REQUIRE(w.session().redo());
  REQUIRE(w.session().redo());
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == final);
  verify(w.session());
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto file = directory.filePath("equation-sweep.blcad").toStdString();
  REQUIRE(w.session().save_as(file));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(file));
  CHECK(snapshot(reopened) == final);
  verify(reopened);
}
