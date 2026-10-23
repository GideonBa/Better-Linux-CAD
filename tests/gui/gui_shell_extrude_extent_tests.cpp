#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& field(ShellWindow& window, const char* name) {
  auto* result = window.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(result);
  return *result;
}
void choose(ShellWindow& window, const char* name, int row) {
  field<QComboBox>(window, name).setCurrentIndex(row);
}
void input(ShellWindow& window, const char* name, const QString& text) {
  auto& editor = field<QLineEdit>(window, name);
  editor.setText(text);
  REQUIRE(QMetaObject::invokeMethod(&editor, "editingFinished", Qt::DirectConnection));
}
void face(ShellWindow& window, const char* name, const char* value) {
  auto& combo = field<QComboBox>(window, name);
  const auto row = combo.findData(QStringLiteral("feature.base/") + value);
  REQUIRE(row >= 0);
  combo.setCurrentIndex(row);
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
void seed(ShellWindow& window, bool target = false, const char* plane = "datum.xy") {
  REQUIRE(window.new_part(QStringLiteral("Extent tests")));
  REQUIRE(window.session().commit_part_transaction("Load extent inputs", [&](PartDocument& part) {
    for (const auto& [id, value] : {std::pair{"width", 4.0},
                                    {"height", 6.0},
                                    {"base.width", 30.0},
                                    {"base.height", 30.0},
                                    {"depth", 20.0}})
      REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
    auto sketch =
        reference_checked(Sketch::create(SketchId("sketch.tool"), "Tool", DatumPlaneId(plane)));
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId("profile.tool"), ParameterId("width"), ParameterId("height")))));
    REQUIRE(part.add_sketch(sketch));
    if (target) {
      auto base =
          reference_checked(Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId(plane)));
      REQUIRE(base.add_profile(reference_checked(RectangleProfile::create(
          ProfileId("profile.base"), ParameterId("base.width"), ParameterId("base.height")))));
      REQUIRE(part.add_sketch(base));
      REQUIRE(part.add_body(
          reference_checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
      auto feature = reference_checked(Feature::create_additive_extrude(
          FeatureId("feature.base"), "Base", base.id(), ParameterId("depth")));
      auto context = reference_checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")));
      REQUIRE(part.add_feature(reference_checked(feature.with_body_result_context(context))));
    }
    return Result<std::size_t>::success(1);
  }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(window.feature_tools().begin_extrude());
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
} // namespace

TEST_CASE("Extrude scalar extents preserve orientation and provision only used distances",
          "[gui][block136-extrude-extents]") {
  for (const auto* plane : {"datum.xy", "datum.xz", "datum.yz"})
    for (int mode = 0; mode < 3; ++mode)
      for (int direction = 0; direction < 2; ++direction) {
        INFO(plane << " mode=" << mode << " direction=" << direction);
        ShellWindow window;
        seed(window, false, plane);
        const auto before = snapshot(window);
        const auto count = window.session().part_document()->parameter_count();
        choose(window, "blcad.extrude.extent", mode);
        choose(window, "blcad.extrude.direction", direction);
        input(window, "blcad.manipulator_numeric_hud", "12 mm");
        if (mode == 2)
          input(window, "blcad.extrude.second_distance", "4 mm");
        applicable(window);
        CHECK(snapshot(window) == before);
        const auto frame = window.feature_tools().manipulators().handles().front().axis;
        const auto expected = reference_checked(GuiSketchWorkbench{}.plane_view(
                                                    window.session(), SketchId("sketch.tool")))
                                  .normal;
        CHECK(frame.x * expected.x + frame.y * expected.y + frame.z * expected.z ==
              Approx(direction ? -1 : 1));
        REQUIRE(window.feature_tools().apply());
        const auto* part = window.session().part_document();
        const auto body = part->bodies().back().id();
        CHECK(volume(window, body) == Approx(24 * (mode == 2 ? 16 : 12)).margin(1e-6));
        const auto bounds = reference_checked(geometry::BodyTransformAdapter{}.bounds(
            *window.session().part_shape_cache()->find_body_shape(body)));
        const auto coordinate = [&](bool maximum) {
          const Vector3 axis{expected.x * (direction ? -1 : 1), expected.y * (direction ? -1 : 1),
                             expected.z * (direction ? -1 : 1)};
          const auto component = [maximum](double axis_value, double low, double high) {
            return axis_value * ((axis_value >= 0) == maximum ? high : low);
          };
          return component(axis.x, bounds.minimum.x, bounds.maximum.x) +
                 component(axis.y, bounds.minimum.y, bounds.maximum.y) +
                 component(axis.z, bounds.minimum.z, bounds.maximum.z);
        };
        CHECK(coordinate(false) == Approx(mode == 1 ? -6 : mode == 2 ? -4 : 0).margin(1e-6));
        CHECK(coordinate(true) == Approx(mode == 1 ? 6 : 12).margin(1e-6));
        CHECK(part->parameter_count() == count + (mode == 2 ? 2 : 1));
        CHECK(part->features().back().extrude_intent().extent().mode() ==
              static_cast<ExtrudeExtentMode>(mode));
        const auto after = snapshot(window);
        REQUIRE(window.undo());
        CHECK(snapshot(window) == before);
        REQUIRE(window.redo());
        CHECK(snapshot(window) == after);
      }
}

TEST_CASE("Extrude body operations and target driven terminations publish the previewed Body",
          "[gui][block136-extrude-extents]") {
  for (int mode = 0; mode < 5; ++mode) {
    ShellWindow window;
    seed(window, true);
    const auto before = snapshot(window);
    const auto parameters = window.session().part_document()->parameter_count();
    choose(window, "blcad.extrude.operation", mode == 0 ? 1 : mode == 1 ? 3 : 2);
    if (mode < 3)
      input(window, "blcad.manipulator_numeric_hud", "30 mm");
    else
      choose(window, "blcad.extrude.extent", mode); // ThroughAll / ToNext
    applicable(window);
    CHECK(window.viewport().feature_preview_replaces("body/body.base"));
    CHECK(snapshot(window) == before);
    REQUIRE(window.feature_tools().apply());
    CHECK(window.session().part_document()->body_count() == 1);
    const double expected = mode == 0 ? 18240 : mode == 1 ? 480 : 17520;
    CHECK(volume(window, BodyId("body.base")) == Approx(expected).margin(1e-5));
    CHECK(window.session().part_document()->parameter_count() == parameters + (mode < 3 ? 1 : 0));
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
  }
}

TEST_CASE("Extrude planar face bounds reject transverse or coincident limits and recover",
          "[gui][block136-extrude-extents]") {
  for (int mode : {5, 6}) {
    ShellWindow window;
    seed(window, true);
    const auto before = snapshot(window);
    const auto count = window.session().part_document()->parameter_count();
    choose(window, "blcad.extrude.extent", mode);
    face(window, "blcad.extrude.first_face", "right");
    CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
    CHECK_FALSE(window.feature_tools().apply());
    CHECK(snapshot(window) == before);
    face(window, "blcad.extrude.first_face", "top");
    if (mode == 6) {
      face(window, "blcad.extrude.second_face", "top");
      CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
      face(window, "blcad.extrude.second_face", "bottom");
    }
    applicable(window);
    const auto body = window.feature_tools().coordinator().extrude().result_body_id();
    REQUIRE(window.feature_tools().apply());
    CHECK(window.session().part_document()->parameter_count() == count);
    CHECK(volume(window, body) == Approx(480));
    QTemporaryDir directory;
    const auto path = directory.filePath("face-extents.blcad");
    REQUIRE(window.save_to(path));
    const auto after = snapshot(window);
    REQUIRE(window.open_path(path));
    CHECK(snapshot(window) == after);
    CHECK(volume(window, body) == Approx(480));
  }
}

TEST_CASE("Extrude pending text cannot apply stale values and switching modes drops unused inputs",
          "[gui][block136-extrude-extents]") {
  ShellWindow window;
  seed(window, true);
  const auto before = snapshot(window);
  const auto count = window.session().part_document()->parameter_count();
  choose(window, "blcad.extrude.extent", 2);
  for (const auto* text : {"nan", "-3", "0", "7 deg", "1e999"}) {
    input(window, "blcad.extrude.second_distance", text);
    CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
    CHECK_FALSE(window.feature_tools().apply());
    CHECK(snapshot(window) == before);
  }
  input(window, "blcad.extrude.second_distance", "5");
  input(window, "blcad.extrude.taper", "90 deg");
  CHECK_FALSE(window.feature_tools().apply());
  input(window, "blcad.extrude.taper", "");
  choose(window, "blcad.extrude.extent", 3);
  CHECK_FALSE(window.feature_tools().apply()); // A target operation is required.
  choose(window, "blcad.extrude.operation", 2);
  applicable(window);
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->parameter_count() == count);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Extrude taper text and handles share rollback and persistence authority",
          "[gui][block136-extrude-extents]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  input(window, "blcad.extrude.taper", "-2.123456789012345 deg");
  applicable(window);
  auto& tools = window.feature_tools();
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kExtrudeTaperHandleId);
  candidate.value_kind = GuiViewportManipulatorValueKind::Angle;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = -3;
  REQUIRE(tools.accept_manipulator_candidate(candidate));
  CHECK(field<QLineEdit>(window, "blcad.extrude.taper").text().toDouble() == Approx(-3));
  candidate.scalar_value = -89;
  CHECK_FALSE(tools.accept_manipulator_candidate(candidate));
  CHECK(tools.coordinator().extrude().taper() == -3);
  CHECK(field<QLineEdit>(window, "blcad.extrude.taper").text().toDouble() == Approx(-3));
  applicable(window);
  REQUIRE(tools.apply());
  const auto body = window.session().part_document()->bodies().back().id();
  CHECK(volume(window, body) < 240);
  CHECK(window.session().part_document()->features().back().extrude_intent().taper_angle_deg() ==
        -3);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Extrude cancels local bounds and reserves both distances across the global ID namespace",
          "[gui][block136-extrude-extents]") {
  ShellWindow window;
  seed(window);
  window.feature_tools().cancel();
  REQUIRE(window.session().commit_part_transaction(
      "Reserve a second distance identity", [](PartDocument& part) {
        return part.add_sketch(reference_checked(Sketch::create(
            SketchId("parameter.extrusion.1.depth.second"), "Reserved", DatumPlaneId("datum.xy"))));
      }));
  const auto before = snapshot(window);
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(window.feature_tools().coordinator().extrude().feature_id() ==
        FeatureId("feature.extrusion.2"));
  choose(window, "blcad.extrude.extent", 2);
  input(window, "blcad.extrude.second_distance", "17.123456789012345");
  window.feature_tools().cancel();
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.session().task().active());
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(field<QComboBox>(window, "blcad.extrude.extent").currentIndex() == 0);
  CHECK(field<QLineEdit>(window, "blcad.extrude.second_distance").text() == "5");
  window.feature_tools().cancel();
}
