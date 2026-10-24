#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& field(ShellWindow& window, const char* id) {
  auto* result = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(result);
  return *result;
}
void choose(ShellWindow& window, const char* id, int row) {
  field<QComboBox>(window, id).setCurrentIndex(row);
}
void input(ShellWindow& window, const char* id, const QString& value) {
  auto& editor = field<QLineEdit>(window, id);
  editor.setText(value);
  REQUIRE(QMetaObject::invokeMethod(&editor, "editingFinished", Qt::DirectConnection));
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
double volume(ShellWindow& window, const BodyId& body) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(body);
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void applicable(ShellWindow& window) {
  REQUIRE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  REQUIRE(window.viewport().feature_preview_active());
}
void begin(ShellWindow& window) {
  REQUIRE(window.feature_tools().select_open_line(SketchId("sketch.line")));
  REQUIRE(window.feature_tools().extrude_action().isEnabled());
  CHECK_FALSE(window.feature_tools().revolve_action().isEnabled());
  REQUIRE(window.feature_tools().begin_extrude());
}
void seed(ShellWindow& window, const char* plane = "datum.xy", bool target = false) {
  REQUIRE(window.new_part("Thin extrusion"));
  REQUIRE(window.session().commit_part_transaction("Load open line", [&](PartDocument& part) {
    auto sketch = reference_checked(
        Sketch::create(SketchId("sketch.line"), "Open line", DatumPlaneId(plane)));
    REQUIRE(sketch.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("line.open"), {-10, 0}, {10, 0}))));
    REQUIRE(part.add_sketch(sketch));
    if (target) {
      for (const auto& [id, value] : {std::pair{"width", 30.0}, {"height", 30.0}, {"depth", 20.0}})
        REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
            ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
      auto base =
          reference_checked(Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId(plane)));
      REQUIRE(base.add_profile(reference_checked(RectangleProfile::create(
          ProfileId("profile.base"), ParameterId("width"), ParameterId("height")))));
      REQUIRE(part.add_sketch(base));
      REQUIRE(part.add_body(
          reference_checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
      auto feature = reference_checked(Feature::create_additive_extrude(
          FeatureId("feature.base"), "Base", base.id(), ParameterId("depth")));
      REQUIRE(part.add_feature(reference_checked(
          feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
              FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")))))));
    }
    return Result<std::size_t>::success(1);
  }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  begin(window);
}
} // namespace

TEST_CASE("Thin Extrude preserves three wall modes and directed bounds on every origin plane",
          "[gui][block136-thin-extrude]") {
  for (const auto* plane : {"datum.xy", "datum.xz", "datum.yz"})
    for (int mode = 0; mode < 3; ++mode)
      for (int opposite = 0; opposite < 2; ++opposite) {
        INFO(plane << " mode=" << mode << " opposite=" << opposite);
        ShellWindow window;
        seed(window, plane);
        const auto before = snapshot(window);
        choose(window, "blcad.extrude.thin_mode", mode);
        choose(window, "blcad.extrude.direction", opposite);
        input(window, "blcad.manipulator_numeric_hud", "12 mm");
        input(window, "blcad.extrude.thickness", "2 mm");
        if (mode == 2)
          input(window, "blcad.extrude.second_thickness", "3 mm");
        applicable(window);
        CHECK(snapshot(window) == before);
        const auto body = window.feature_tools().coordinator().extrude().result_body_id();
        REQUIRE(window.feature_tools().apply());
        const auto* part = window.session().part_document();
        CHECK(part->parameter_count() == (mode == 2 ? 3 : 2));
        CHECK(part->find_sketch(SketchId("sketch.line"))->profile_count() == 0);
        const auto& feature = part->features().back();
        REQUIRE(feature.extrude_intent().thin());
        CHECK_FALSE(feature.profile());
        CHECK(feature.extrude_intent().thin()->mode() == (mode == 0   ? ExtrudeThinMode::OneSided
                                                          : mode == 1 ? ExtrudeThinMode::MidPlane
                                                                      : ExtrudeThinMode::TwoSided));
        CHECK(volume(window, body) == Approx(20 * (mode == 2 ? 5 : 2) * 12).margin(1e-5));
        const auto bounds = reference_checked(geometry::BodyTransformAdapter{}.bounds(
            *window.session().part_shape_cache()->find_body_shape(body)));
        const auto view = reference_checked(
            GuiSketchWorkbench{}.plane_view(window.session(), SketchId("sketch.line")));
        const auto projection = [&](Vector3 axis, bool maximum) {
          const auto component = [maximum](double value, double low, double high) {
            return value * ((value >= 0) == maximum ? high : low);
          };
          return component(axis.x, bounds.minimum.x, bounds.maximum.x) +
                 component(axis.y, bounds.minimum.y, bounds.maximum.y) +
                 component(axis.z, bounds.minimum.z, bounds.maximum.z);
        };
        CHECK(projection(view.y_axis, false) == Approx(mode == 0   ? 0
                                                       : mode == 1 ? -1
                                                                   : -3)
                                                    .margin(1e-5));
        CHECK(projection(view.y_axis, true) == Approx(mode == 1 ? 1 : 2).margin(1e-5));
        CHECK(projection(view.normal, false) == Approx(opposite ? -12 : 0).margin(1e-5));
        CHECK(projection(view.normal, true) == Approx(opposite ? 0 : 12).margin(1e-5));
        const auto after = snapshot(window);
        REQUIRE(window.undo());
        CHECK(snapshot(window) == before);
        REQUIRE(window.redo());
        CHECK(snapshot(window) == after);
      }
}

TEST_CASE("Thin Extrude supports all Body operations and all seven straight extents",
          "[gui][block136-thin-extrude]") {
  for (int operation = 0; operation < 4; ++operation) {
    ShellWindow window;
    seed(window, "datum.xy", true);
    choose(window, "blcad.extrude.operation", operation);
    input(window, "blcad.manipulator_numeric_hud", "30 mm");
    input(window, "blcad.extrude.thickness", "2 mm");
    applicable(window);
    CHECK(window.viewport().feature_preview_replaces("body/body.base") == (operation != 0));
    const auto body = window.feature_tools().coordinator().extrude().result_body_id();
    REQUIRE(window.feature_tools().apply());
    const double expected[] = {1200, 18400, 17200, 800};
    CHECK(volume(window, body) == Approx(expected[operation]).margin(1e-5));
    CHECK(window.session().part_document()->body_count() == (operation == 0 ? 2 : 1));
  }
  for (int extent = 0; extent < 7; ++extent) {
    ShellWindow window;
    seed(window, "datum.xy", true);
    choose(window, "blcad.extrude.operation", 2);
    input(window, "blcad.manipulator_numeric_hud", "12 mm");
    input(window, "blcad.extrude.thickness", "2 mm");
    choose(window, "blcad.extrude.extent", extent);
    if (extent == 2)
      input(window, "blcad.extrude.second_distance", "4 mm");
    if (extent >= 5) {
      auto& first = field<QComboBox>(window, "blcad.extrude.first_face");
      first.setCurrentIndex(first.findData(QStringLiteral("feature.base/top")));
      if (extent == 6) {
        auto& second = field<QComboBox>(window, "blcad.extrude.second_face");
        second.setCurrentIndex(second.findData(QStringLiteral("feature.base/bottom")));
      }
    }
    applicable(window);
    REQUIRE(window.feature_tools().apply());
    CHECK(volume(window, BodyId("body.base")) == Approx(extent == 0 || extent == 2 ? 17520
                                                        : extent == 1              ? 17760
                                                                                   : 17200)
                                                     .margin(1e-5));
    CHECK(window.session().part_document()->parameter_count() == 3 + (extent == 2  ? 3
                                                                      : extent < 3 ? 2
                                                                                   : 1));
  }
}

TEST_CASE("Thin Extrude retains invalid text and switching wall modes drops unused thickness",
          "[gui][block136-thin-extrude]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  for (const auto* invalid : {"0", "-1", "nan", "2 deg", "1e999"}) {
    input(window, "blcad.extrude.thickness", invalid);
    CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(window.feature_tools().apply());
    CHECK(snapshot(window) == before);
  }
  input(window, "blcad.manipulator_numeric_hud", "8 mm");
  CHECK(field<QLineEdit>(window, "blcad.extrude.thickness").text() == "1e999");
  input(window, "blcad.extrude.thickness", "2.123456789012345 mm");
  choose(window, "blcad.extrude.thin_mode", 2);
  input(window, "blcad.extrude.second_thickness", "3 deg");
  CHECK_FALSE(window.feature_tools().apply());
  choose(window, "blcad.extrude.thin_mode", 1);
  applicable(window);
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->parameter_count() == 2);
  const auto& thin = *window.session().part_document()->features().back().extrude_intent().thin();
  CHECK_FALSE(thin.second_thickness_parameter());
  CHECK(window.session()
            .part_document()
            ->find_parameter(thin.first_thickness_parameter())
            ->value()
            .millimeters() == 2.123456789012345);
}

TEST_CASE("Thin Extrude thickness handles use the Sketch left direction and independent rollback",
          "[gui][block136-thin-extrude]") {
  for (const auto* plane : {"datum.xy", "datum.xz", "datum.yz"}) {
    ShellWindow window;
    seed(window, plane);
    input(window, "blcad.extrude.thickness", "3");
    input(window, "blcad.extrude.taper", "-2 deg");
    const auto view = reference_checked(
        GuiSketchWorkbench{}.plane_view(window.session(), SketchId("sketch.line")));
    const auto handle = window.feature_tools().coordinator().extrude().thin_handle();
    REQUIRE(handle);
    CHECK(dot(handle->axis, view.y_axis) == Approx(1));
    CHECK(dot(handle->axis, view.normal) == Approx(0));
    GuiViewportManipulatorCandidate candidate;
    candidate.handle_id = std::string(kExtrudeThinHandleId);
    candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
    candidate.scalar_value = .1;
    CHECK_FALSE(window.feature_tools().accept_manipulator_candidate(candidate));
    CHECK(window.feature_tools().coordinator().extrude().thin_thickness() == 3);
    CHECK(field<QLineEdit>(window, "blcad.extrude.thickness").text().toDouble() == 3);
    CHECK(field<QLineEdit>(window, "blcad.manipulator_numeric_hud").text() == "10");
    applicable(window);
    candidate.scalar_value = 4;
    REQUIRE(window.feature_tools().accept_manipulator_candidate(candidate));
    CHECK(field<QLineEdit>(window, "blcad.extrude.thickness").text().toDouble() == 4);
    CHECK(field<QLineEdit>(window, "blcad.manipulator_numeric_hud").text() == "10");
    REQUIRE(window.feature_tools().apply());
    const auto body = window.session().part_document()->bodies().back().id();
    CHECK(volume(window, body) > 600);
    CHECK(volume(window, body) < 800);
  }
}

TEST_CASE("Thin Extrude bounds selection and reserves both new wall identities before cancellation",
          "[gui][block136-thin-extrude]") {
  ShellWindow window;
  seed(window);
  window.feature_tools().cancel();
  REQUIRE(window.session().commit_part_transaction("Additional inputs", [](PartDocument& part) {
    auto chain = reference_checked(
        Sketch::create(SketchId("sketch.chain"), "Chain", DatumPlaneId("datum.xy")));
    REQUIRE(chain.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("one"), {0, 0}, {5, 0}))));
    REQUIRE(chain.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("two"), {5, 0}, {5, 5}))));
    REQUIRE(part.add_sketch(chain));
    auto mixed = reference_checked(
        Sketch::create(SketchId("sketch.mixed"), "Mixed", DatumPlaneId("datum.xy")));
    REQUIRE(mixed.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("one"), {0, 0}, {5, 0}))));
    REQUIRE(mixed.add_entity(reference_checked(
        ArcSegment::create_three_point(SketchEntityId("arc"), {5, 0}, {6, 1}, {5, 2}))));
    REQUIRE(part.add_sketch(mixed));
    return part.add_sketch(
        reference_checked(Sketch::create(SketchId("parameter.extrusion.1.depth.wall.second"),
                                         "Reserved wall", DatumPlaneId("datum.xy"))));
  }));
  CHECK_FALSE(window.feature_tools().select_open_line(SketchId("sketch.chain")));
  CHECK_FALSE(window.feature_tools().select_open_line(SketchId("sketch.mixed")));
  CHECK_FALSE(window.feature_tools().select_open_line(SketchId("missing")));
  CHECK_FALSE(window.feature_tools().begin_extrude());
  const auto before = snapshot(window);
  begin(window);
  CHECK(window.feature_tools().coordinator().extrude().feature_id() ==
        FeatureId("feature.extrusion.2"));
  CHECK(field<QComboBox>(window, "blcad.extrude.direction").count() == 2);
  choose(window, "blcad.extrude.thin_mode", 2);
  input(window, "blcad.extrude.second_thickness", "7.123456789012345");
  window.feature_tools().cancel();
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.viewport().feature_preview_active());
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("thin-inputs.blcad")));
  begin(window);
  CHECK(field<QComboBox>(window, "blcad.extrude.thin_mode").currentIndex() == 0);
  CHECK(field<QLineEdit>(window, "blcad.extrude.second_thickness").text() == "1");
  REQUIRE(window.new_part("Replacement"));
  CHECK_FALSE(window.feature_tools().active());
  CHECK_FALSE(window.session().task().active());
  CHECK(window.session().part_document()->features().empty());
}

TEST_CASE(
    "Thin Extrude semantic selection and pointer Escape preserve thickness depth and reopening",
    "[gui][block136-thin-extrude]") {
  ShellWindow window;
  seed(window);
  auto& tools = window.feature_tools();
  tools.cancel();
  REQUIRE(window.viewport().select_semantic("sketch/sketch.line/entity/line.open"));
  REQUIRE(tools.extrude_action().isEnabled());
  CHECK_FALSE(tools.revolve_action().isEnabled());
  REQUIRE(tools.begin_extrude());
  input(window, "blcad.extrude.thickness", "3");
  // Use an oblique view so Sketch-left and depth have distinct screen axes.
  // An axis-aligned camera can superimpose linear handle projections.
  auto camera = window.viewport().camera_bookmark();
  camera.eye = {100, 100, 100};
  camera.target = {};
  camera.up_direction = {0, 0, 1};
  camera.projection = GuiViewportProjection::Orthographic;
  REQUIRE(window.viewport().restore_camera_bookmark(camera));
  qApp->processEvents();
  QGuiApplication::sync();
  qApp->processEvents();
  const auto before = snapshot(window);
  tools.refresh_presentation();
  const auto presentations = tools.manipulators().presentations();
  const auto found =
      std::find_if(presentations.begin(), presentations.end(),
                   [](const auto& handle) { return handle.handle_id == kExtrudeThinHandleId; });
  REQUIRE(found != presentations.end());
  const double dx = found->endpoint.x - found->anchor.x;
  const double dy = found->endpoint.y - found->anchor.y;
  const double magnitude = std::hypot(dx, dy);
  REQUIRE(magnitude > 1);
  const QPointF from(found->endpoint.x, found->endpoint.y);
  const auto hits = tools.manipulators().hits_at({from.x(), from.y()});
  INFO("Camera scale=" << window.viewport().camera_bookmark().scale);
  for (const auto& item : presentations)
    UNSCOPED_INFO(item.handle_id << " anchor=" << item.anchor.x << "," << item.anchor.y
                                 << " end=" << item.endpoint.x << "," << item.endpoint.y);
  const auto thin = tools.coordinator().extrude().thin_handle();
  REQUIRE(thin);
  INFO("Thin axis=" << thin->axis.x << "," << thin->axis.y << "," << thin->axis.z);
  REQUIRE_FALSE(hits.empty());
  REQUIRE(hits.front().handle_id == kExtrudeThinHandleId);
  const QPointF to(from.x() + 25 * dx / magnitude, from.y() + 25 * dy / magnitude);
  auto& viewport = window.viewport();
  QMouseEvent press(QEvent::MouseButtonPress, from, viewport.mapToGlobal(from.toPoint()),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &press);
  REQUIRE(tools.manipulators().active_handle_id() == kExtrudeThinHandleId);
  QMouseEvent move(QEvent::MouseMove, to, viewport.mapToGlobal(to.toPoint()), Qt::NoButton,
                   Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &move);
  CHECK(tools.coordinator().extrude().thin_thickness() != 3);
  CHECK(tools.coordinator().extrude().distance() == 10);
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &escape);
  CHECK(tools.active());
  CHECK(tools.manipulators().active_handle_id().empty());
  CHECK(tools.coordinator().extrude().thin_thickness() == 3);
  CHECK(field<QLineEdit>(window, "blcad.extrude.thickness").text().toDouble() == 3);
  CHECK(tools.numeric_editor().text() == "10");
  CHECK(snapshot(window) == before);
  applicable(window);
  const auto body = tools.coordinator().extrude().result_body_id();
  REQUIRE(tools.apply());
  CHECK(volume(window, body) == Approx(600).margin(1e-5));
  const auto after = snapshot(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("thin.blcad");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == after);
  CHECK(volume(window, body) == Approx(600).margin(1e-5));
}

TEST_CASE("Thin Extrude resolves diagonal reversed lines on a tilted offset workplane",
          "[gui][block136-thin-extrude]") {
  for (const bool reversed : {false, true}) {
    ShellWindow window;
    seed(window);
    window.feature_tools().cancel();
    const double root = std::sqrt(.5);
    const Vector3 x{root, root, 0}, y{-.5, .5, root}, normal{.5, -.5, root};
    const Point3 origin{12, 5, 8};
    REQUIRE(window.session().commit_part_transaction("Tilt line plane", [&](PartDocument& part) {
      REQUIRE(part.add_construction_plane(reference_checked(ConstructionPlane::create_explicit(
          ConstructionPlaneId("plane.tilted"), "Tilted", origin, x, y, normal))));
      auto sketch = reference_checked(
          Sketch::create(SketchId("sketch.line"), "Tilted line", DatumPlaneId("plane.tilted")));
      REQUIRE(sketch.add_entity(reference_checked(LineSegment::create(
          SketchEntityId("line.open"), reversed ? Point2{16, 11} : Point2{4, -5},
          reversed ? Point2{4, -5} : Point2{16, 11}))));
      return part.update_sketch(sketch);
    }));
    begin(window);
    choose(window, "blcad.extrude.thin_mode", 2);
    choose(window, "blcad.extrude.direction", 1);
    input(window, "blcad.extrude.thickness", "2");
    input(window, "blcad.extrude.second_thickness", "3");
    applicable(window);
    const auto before = snapshot(window);
    const auto handle = window.feature_tools().coordinator().extrude().thin_handle();
    REQUIRE(handle);
    const double sign = reversed ? -1 : 1;
    const Vector3 left{sign * (-.8 * x.x + .6 * y.x), sign * (-.8 * x.y + .6 * y.y),
                       sign * (-.8 * x.z + .6 * y.z)};
    CHECK(dot(handle->axis, left) == Approx(1));
    CHECK(handle->origin.x == Approx(origin.x + 10 * x.x + 3 * y.x));
    CHECK(handle->origin.y == Approx(origin.y + 10 * x.y + 3 * y.y));
    CHECK(handle->origin.z == Approx(origin.z + 10 * x.z + 3 * y.z));
    const auto body = window.feature_tools().coordinator().extrude().result_body_id();
    REQUIRE(window.feature_tools().apply());
    CHECK(volume(window, body) == Approx(1000).margin(1e-5));
    std::vector<Point3> corners;
    for (const auto p : {Point2{4, -5}, Point2{16, 11}})
      for (const double wall : {2., -3.})
        for (const double depth : {0., -10.})
          corners.push_back({origin.x + p.x * x.x + p.y * y.x + wall * left.x + depth * normal.x,
                             origin.y + p.x * x.y + p.y * y.y + wall * left.y + depth * normal.y,
                             origin.z + p.x * x.z + p.y * y.z + wall * left.z + depth * normal.z});
    const auto bounds = reference_checked(geometry::BodyTransformAdapter{}.bounds(
        *window.session().part_shape_cache()->find_body_shape(body)));
    for (int component = 0; component < 3; ++component) {
      const auto coordinate = [component](Point3 p) {
        return component == 0 ? p.x : component == 1 ? p.y : p.z;
      };
      const auto extrema =
          std::minmax_element(corners.begin(), corners.end(),
                              [&](Point3 a, Point3 b) { return coordinate(a) < coordinate(b); });
      CHECK(coordinate(bounds.minimum) == Approx(coordinate(*extrema.first)).margin(1e-5));
      CHECK(coordinate(bounds.maximum) == Approx(coordinate(*extrema.second)).margin(1e-5));
    }
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
  }
}
