#include "../support/mixed_path_loft_fixture.hpp"
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
using blcad::test::mixed_loft::checked;
using Catch::Approx;
namespace {
template <typename T> T& field(ShellWindow& window, const char* id) {
  auto* widget = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(widget);
  return *widget;
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
  return checked(serialize_part_document_to_json(*window.session().part_document()));
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
void begin(ShellWindow& window, const SketchId& sketch = SketchId("sketch.tool"),
           const ProfileId& profile = ProfileId("profile.tool")) {
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  REQUIRE(window.feature_tools().select_profile(sketch, profile));
  REQUIRE(window.feature_tools().begin_extrude());
}
void seed(ShellWindow& window, const char* plane = "datum.xy", bool with_path = true) {
  REQUIRE(window.new_part(QStringLiteral("Path Extrude tests")));
  REQUIRE(window.session().commit_part_transaction("Load path inputs", [&](PartDocument& part) {
    for (const auto& [id, value] : {std::pair{"width", 4.0},
                                    {"height", 6.0},
                                    {"base.width", 30.0},
                                    {"base.height", 30.0},
                                    {"depth", 20.0}})
      REQUIRE(part.add_parameter(checked(
          Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
    for (const bool base : {false, true}) {
      auto sketch = checked(Sketch::create(SketchId(base ? "sketch.base" : "sketch.tool"),
                                           base ? "Base" : "Tool", DatumPlaneId(plane)));
      REQUIRE(sketch.add_profile(
          checked(RectangleProfile::create(ProfileId(base ? "profile.base" : "profile.tool"),
                                           ParameterId(base ? "base.width" : "width"),
                                           ParameterId(base ? "base.height" : "height")))));
      REQUIRE(part.add_sketch(sketch));
    }
    REQUIRE(part.add_body(checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
    auto feature = checked(Feature::create_additive_extrude(
        FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("depth")));
    REQUIRE(part.add_feature(
        checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
            FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")))))));
    if (with_path) {
      const auto normal = part.find_datum_plane(DatumPlaneId(plane))->normal();
      REQUIRE(part.add_construction_point(checked(
          ConstructionPoint::create_explicit(ConstructionPointId("point.start"), "Start", {}))));
      REQUIRE(part.add_construction_point(checked(
          ConstructionPoint::create_explicit(ConstructionPointId("point.end"), "End",
                                             {30 * normal.x, 30 * normal.y, 30 * normal.z}))));
      auto relation = checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId("relation.path"), ConstructionPointId("point.start"),
          ConstructionPointId("point.end")));
      REQUIRE(part.add_construction_line(checked(ConstructionLine::create_through_two_points(
          ConstructionLineId("line.path"), "Path", relation))));
      REQUIRE(part.add_path_curve(
          checked(PathCurve::create(PathCurveId("path.route"), "Route",
                                    {checked(PathSegmentReference::create_construction_line(
                                        ConstructionLineId("line.path")))},
                                    PathClosure::Open, PathOrientationRule::MinimumTwist))));
    }
    return Result<std::size_t>::success(1);
  }));
  begin(window);
}
} // namespace

TEST_CASE("Path Extrude applies every Body operation on three oriented profile planes atomically",
          "[gui][block136-path-extrude]") {
  for (const auto* plane : {"datum.xy", "datum.xz", "datum.yz"})
    for (int operation = 0; operation < 4; ++operation) {
      INFO(plane << " operation=" << operation);
      ShellWindow window;
      seed(window, plane);
      const auto before = snapshot(window);
      const auto parameters = window.session().part_document()->parameter_count();
      choose(window, "blcad.extrude.operation", operation);
      choose(window, "blcad.extrude.direction", 2);
      applicable(window);
      CHECK(window.feature_tools().manipulators().handles().empty());
      CHECK_FALSE(field<QLineEdit>(window, "blcad.manipulator_numeric_hud").isVisible());
      CHECK_FALSE(field<QComboBox>(window, "blcad.extrude.extent").isEnabled());
      CHECK_FALSE(field<QLineEdit>(window, "blcad.extrude.taper").isEnabled());
      CHECK(window.viewport().feature_preview_replaces("body/body.base") == (operation != 0));
      CHECK(snapshot(window) == before);
      const auto body = window.feature_tools().coordinator().extrude().result_body_id();
      REQUIRE(window.feature_tools().apply());
      const auto* part = window.session().part_document();
      CHECK(part->parameter_count() == parameters);
      CHECK(part->body_count() == (operation == 0 ? 2 : 1));
      const double expected[] = {720, 18240, 17520, 480};
      CHECK(volume(window, body) == Approx(expected[operation]).margin(1e-5));
      const auto& created = part->features().back();
      CHECK(created.type() == FeatureType::AdditiveExtrude);
      CHECK(created.direction() == ExtrudeDirection::Path);
      CHECK(created.path_curve() == PathCurveId("path.route"));
      CHECK(created.profile() == ProfileId("profile.tool"));
      const auto after = snapshot(window);
      REQUIRE(window.undo());
      CHECK(snapshot(window) == before);
      REQUIRE(window.redo());
      CHECK(snapshot(window) == after);
    }
}

TEST_CASE("Path Extrude follows a mixed line spline reversed arc and reopens canonically",
          "[gui][block136-path-extrude]") {
  ShellWindow window;
  REQUIRE(window.new_part("Mixed path Extrude"));
  REQUIRE(window.session().commit_part_transaction("Load mixed inputs", [](PartDocument& part) {
    part = test::mixed_loft::inputs();
    return Result<std::size_t>::success(1);
  }));
  begin(window, SketchId("section.0"), ProfileId("profile.0"));
  const auto before = snapshot(window);
  const auto parameters = window.session().part_document()->parameter_count();
  choose(window, "blcad.extrude.direction", 2);
  applicable(window);
  CHECK(snapshot(window) == before);
  const auto body = window.feature_tools().coordinator().extrude().result_body_id();
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->parameter_count() == parameters);
  CHECK(volume(window, body) > 12 * test::mixed_loft::path_length() * .95);
  CHECK(volume(window, body) < 12 * test::mixed_loft::path_length() * 1.05);
  const auto bounds = checked(geometry::BodyTransformAdapter{}.bounds(
      *window.session().part_shape_cache()->find_body_shape(body)));
  CHECK(bounds.maximum.x > 39);
  CHECK(bounds.maximum.z > 89);
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  const auto path = directory.filePath("mixed-path-extrude.blcad");
  REQUIRE(window.save_to(path));
  const auto expected_volume = volume(window, body);
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == after);
  CHECK(volume(window, body) == Approx(expected_volume).margin(1e-5));
}

TEST_CASE(
    "Path Extrude mode changes restore straight intent without unused inputs or stale handles",
    "[gui][block136-path-extrude]") {
  for (const bool finish_path : {false, true}) {
    ShellWindow window;
    seed(window);
    const auto before = snapshot(window);
    const auto parameters = window.session().part_document()->parameter_count();
    choose(window, "blcad.extrude.extent", 2);
    input(window, "blcad.manipulator_numeric_hud", "12 mm");
    input(window, "blcad.extrude.second_distance", "4 mm");
    input(window, "blcad.extrude.taper", "-1 deg");
    applicable(window);
    choose(window, "blcad.extrude.direction", 2);
    applicable(window);
    GuiViewportManipulatorCandidate stale;
    stale.handle_id = std::string(kExtrudeExtentHandleId);
    stale.value_kind = GuiViewportManipulatorValueKind::Distance;
    stale.scalar_value = 77;
    CHECK_FALSE(window.feature_tools().accept_manipulator_candidate(stale));
    CHECK_FALSE(window.feature_tools().coordinator().extrude().apply_manipulator(stale));
    if (!finish_path) {
      choose(window, "blcad.extrude.direction", 1);
      applicable(window);
      CHECK(window.feature_tools().manipulators().handles().size() == 2);
      CHECK(field<QLineEdit>(window, "blcad.manipulator_numeric_hud").text() == "12");
      CHECK(window.feature_tools().coordinator().extrude().taper() == -1);
      input(window, "blcad.extrude.taper", "");
    }
    CHECK(snapshot(window) == before);
    const auto body = window.feature_tools().coordinator().extrude().result_body_id();
    REQUIRE(window.feature_tools().apply());
    const auto* part = window.session().part_document();
    CHECK(part->parameter_count() == parameters + (finish_path ? 0 : 2));
    CHECK(part->features().back().direction() ==
          (finish_path ? ExtrudeDirection::Path : ExtrudeDirection::OppositeSketchNormal));
    CHECK(part->features().back().path_curve().has_value() == finish_path);
    CHECK(volume(window, body) == Approx(finish_path ? 720 : 384).margin(1e-5));
  }
}

TEST_CASE("Path Extrude rejects missing selection and ignores inactive invalid straight fields",
          "[gui][block136-path-extrude]") {
  for (const bool with_path : {false, true}) {
    ShellWindow window;
    seed(window, "datum.xy", with_path);
    const auto before = snapshot(window);
    choose(window, "blcad.extrude.extent", 2);
    input(window, "blcad.extrude.second_distance", "4 deg");
    input(window, "blcad.extrude.taper", "90 deg");
    CHECK_FALSE(window.feature_tools().apply());
    choose(window, "blcad.extrude.direction", 2);
    if (with_path) {
      applicable(window);
      choose(window, "blcad.extrude.path", -1);
    }
    CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(window.feature_tools().apply());
    CHECK(snapshot(window) == before);
    if (with_path) {
      choose(window, "blcad.extrude.path", 0);
      applicable(window);
    }
    choose(window, "blcad.extrude.direction", 0);
    CHECK_FALSE(window.feature_tools().apply());
    input(window, "blcad.extrude.second_distance", "4 mm");
    input(window, "blcad.extrude.taper", "");
    applicable(window);
    window.feature_tools().cancel();
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
  }
}

TEST_CASE("Path Extrude cancellation and document replacement discard local choices",
          "[gui][block136-path-extrude]") {
  ShellWindow window;
  seed(window);
  window.feature_tools().cancel();
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("inputs.blcad")));
  const auto before = snapshot(window);
  begin(window);
  choose(window, "blcad.extrude.direction", 2);
  applicable(window);
  window.feature_tools().cancel();
  CHECK(snapshot(window) == before);
  begin(window);
  CHECK(field<QComboBox>(window, "blcad.extrude.direction").currentIndex() == 0);
  choose(window, "blcad.extrude.direction", 2);
  REQUIRE(window.new_part("Replacement"));
  CHECK_FALSE(window.feature_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.session().task().active());
  CHECK(window.session().part_document()->features().empty());
}

TEST_CASE("Path Extrude excludes closed routes and recovers from incompatible path orientation",
          "[gui][block136-path-extrude]") {
  ShellWindow window;
  seed(window);
  window.feature_tools().cancel();
  REQUIRE(window.session().commit_part_transaction("Load alternative routes", [](PartDocument&
                                                                                     part) {
    auto loop = checked(Sketch::create(SketchId("sketch.loop"), "Loop", DatumPlaneId("datum.xy")));
    const std::array<Point2, 4> corners{{{-10, -10}, {10, -10}, {10, 10}, {-10, 10}}};
    std::vector<PathSegmentReference> segments;
    for (std::size_t index = 0; index < corners.size(); ++index) {
      const SketchEntityId id("loop." + std::to_string(index));
      REQUIRE(loop.add_entity(
          checked(LineSegment::create(id, corners[index], corners[(index + 1) % corners.size()]))));
      segments.push_back(
          checked(PathSegmentReference::create_planar(loop.id(), id, PlanarPathCurveKind::Line)));
    }
    REQUIRE(part.add_sketch(loop));
    REQUIRE(part.add_path_curve(
        checked(PathCurve::create(PathCurveId("path.closed"), "Closed loop", segments,
                                  PathClosure::Closed, PathOrientationRule::ProfileNormal))));
    return part.add_path_curve(checked(PathCurve::create(
        PathCurveId("path.incompatible"), "Parallel fixed-up vector",
        {checked(PathSegmentReference::create_construction_line(ConstructionLineId("line.path")))},
        PathClosure::Open, PathOrientationRule::FixedUpVector, Vector3{0, 0, 1})));
  }));
  begin(window);
  const auto before = snapshot(window);
  auto& paths = field<QComboBox>(window, "blcad.extrude.path");
  CHECK(paths.count() == 2);
  CHECK(paths.findData(QStringLiteral("path.closed")) == -1);
  choose(window, "blcad.extrude.direction", 2);
  const auto invalid = paths.findData(QStringLiteral("path.incompatible"));
  REQUIRE(invalid >= 0);
  paths.setCurrentIndex(invalid);
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.feature_tools().apply());
  CHECK(snapshot(window) == before);
  const auto valid = paths.findData(QStringLiteral("path.route"));
  REQUIRE(valid >= 0);
  paths.setCurrentIndex(valid);
  applicable(window);
  const auto body = window.feature_tools().coordinator().extrude().result_body_id();
  REQUIRE(window.feature_tools().apply());
  CHECK(volume(window, body) == Approx(720).margin(1e-5));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}
