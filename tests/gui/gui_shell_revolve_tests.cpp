#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <numbers>

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
void axis(ShellWindow& window, const QString& id) {
  auto& combo = field<QComboBox>(window, "blcad.revolve.axis");
  const int row = combo.findData(id);
  REQUIRE(row >= 0);
  combo.setCurrentIndex(row);
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
void seed(ShellWindow& window, const char* plane = "datum.xy", Point2 center = {15, 5},
          double width = 10, bool base = false, bool hole = false) {
  REQUIRE(window.new_part(QStringLiteral("Revolve tests")));
  REQUIRE(
      window.session().commit_part_transaction("Load rotational inputs", [&](PartDocument& part) {
        for (const auto& [id, value] : {std::pair{"width", width},
                                        {"height", 4.0},
                                        {"base.width", 30.0},
                                        {"base.height", 30.0},
                                        {"depth", 12.0},
                                        {"diameter", 4.0}})
          REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
              ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
        auto sketch =
            reference_checked(Sketch::create(SketchId("sketch.tool"), "Tool", DatumPlaneId(plane)));
        REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
            ProfileId("profile.tool"), ParameterId("width"), ParameterId("height"), center))));
        REQUIRE(part.add_sketch(sketch));
        REQUIRE(part.add_construction_line(reference_checked(ConstructionLine::create_explicit(
            ConstructionLineId("line.x"), "Construction X", {}, {1, 0, 0}))));
        REQUIRE(part.add_construction_line(reference_checked(ConstructionLine::create_explicit(
            ConstructionLineId("line.crossing"), "Crossing", {0, 5, 0}, {1, 0, 0}))));
        REQUIRE(part.add_datum_axis(reference_checked(DatumAxis::create_from_construction_line(
            DatumAxisId("axis.derived.x"), "Derived X", ConstructionLineId("line.x")))));
        if (base) {
          auto blank = reference_checked(
              Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")));
          REQUIRE(blank.add_profile(reference_checked(RectangleProfile::create(
              ProfileId("profile.base"), ParameterId("base.width"), ParameterId("base.height")))));
          REQUIRE(part.add_sketch(blank));
          REQUIRE(part.add_body(
              reference_checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
          auto feature = reference_checked(Feature::create_additive_extrude(
              FeatureId("feature.base"), "Base", blank.id(), ParameterId("depth")));
          auto context = reference_checked(FeatureBodyResultContext::create(
              FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")));
          REQUIRE(part.add_feature(reference_checked(feature.with_body_result_context(context))));
          if (hole) {
            auto circular = reference_checked(
                Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy")));
            REQUIRE(circular.add_profile(reference_checked(
                CircleProfile::create(ProfileId("profile.hole"), ParameterId("diameter")))));
            REQUIRE(part.add_sketch(circular));
            auto cut = reference_checked(Feature::create_subtractive_extrude(
                FeatureId("feature.hole"), "Hole", circular.id(), FeatureId("feature.base"),
                SubtractiveExtrudeDepth::ThroughAll));
            auto cut_context = reference_checked(FeatureBodyResultContext::create(
                FeatureBodyOperationMode::Cut, BodyId("body.base"), std::nullopt));
            REQUIRE(part.add_feature(reference_checked(cut.with_body_result_context(cut_context))));
          }
        }
        return Result<std::size_t>::success(1);
      }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(window.feature_tools().begin_revolve());
}
} // namespace

TEST_CASE("Revolve full directed and symmetric extents preserve signed spans and atomic history",
          "[gui][block136-revolve]") {
  for (const auto* plane : {"datum.xy", "datum.xz", "datum.yz"})
    for (int mode = 0; mode < 4; ++mode) {
      INFO(plane << " " << mode);
      ShellWindow window;
      seed(window, plane);
      const auto before = snapshot(window);
      const auto parameters = window.session().part_document()->parameter_count();
      REQUIRE(window.feature_tools().set_numeric_text(QStringLiteral("90 deg")));
      choose(window, "blcad.revolve.extent", mode == 3 ? 2 : mode == 0 ? 0 : 1);
      if (mode == 2)
        choose(window, "blcad.revolve.side", 1);
      applicable(window);
      CHECK(snapshot(window) == before);
      CHECK(window.feature_tools().manipulators().handles().empty() == (mode == 0));
      const auto body = window.feature_tools().coordinator().revolve().result_body_id();
      const auto id = window.feature_tools().coordinator().revolve().feature_id();
      REQUIRE(window.feature_tools().apply());
      CHECK(volume(window, body) ==
            Approx((mode == 0 ? 400 : 100) * std::numbers::pi).margin(1e-5));
      const auto* feature = window.session().part_document()->find_revolve_feature(id);
      REQUIRE(feature);
      CHECK(feature->extent().mode() == (mode == 0   ? RevolveExtentMode::Full
                                         : mode == 3 ? RevolveExtentMode::Symmetric
                                                     : RevolveExtentMode::Angle));
      if (mode == 1 || mode == 2)
        CHECK(feature->extent().side() ==
              (mode == 2 ? RevolveSide::Negative : RevolveSide::Positive));
      CHECK(window.session().part_document()->parameter_count() == parameters);
      const auto bounds = reference_checked(geometry::BodyTransformAdapter{}.bounds(
          *window.session().part_shape_cache()->find_body_shape(body)));
      const double low = std::string(plane) == "datum.xy"   ? bounds.minimum.z
                         : std::string(plane) == "datum.xz" ? -bounds.maximum.y
                                                            : bounds.minimum.x;
      const double high = std::string(plane) == "datum.xy"   ? bounds.maximum.z
                          : std::string(plane) == "datum.xz" ? -bounds.minimum.y
                                                             : bounds.maximum.x;
      const double span = mode == 3 ? 7 / std::sqrt(2.0) : 7;
      // BRepBndLib::Add includes triangulation deflection after native display.
      // Keep the analytic offscreen check tight; allow the viewer's conservative
      // bound expansion while still distinguishing every signed sweep.
      const double bounds_tolerance = window.viewport().native_viewer_available() ? 0.05 : 1e-6;
      CHECK(low ==
            Approx(mode == 0 || mode == 2 || mode == 3 ? -span : 0).margin(bounds_tolerance));
      CHECK(high == Approx(mode == 2 ? 0 : span).margin(bounds_tolerance));
      const auto after = snapshot(window);
      REQUIRE(window.undo());
      CHECK(snapshot(window) == before);
      REQUIRE(window.redo());
      CHECK(snapshot(window) == after);
    }
}

TEST_CASE("Revolve operations map Cut intent and replace only the chosen target Body",
          "[gui][block136-revolve]") {
  for (int operation = 0; operation < 4; ++operation) {
    ShellWindow window;
    seed(window, "datum.xy", {0, 5}, 12, true);
    const auto before = snapshot(window);
    choose(window, "blcad.revolve.extent", 0);
    choose(window, "blcad.revolve.operation", operation);
    applicable(window);
    CHECK(window.viewport().feature_preview_replaces("body/body.base") == (operation != 0));
    const auto body = window.feature_tools().coordinator().revolve().result_body_id();
    const auto id = window.feature_tools().coordinator().revolve().feature_id();
    REQUIRE(window.feature_tools().apply());
    const double half = 240 * std::numbers::pi;
    CHECK(volume(window, body) == Approx(operation == 0   ? 2 * half
                                         : operation == 1 ? 10800 + half
                                         : operation == 2 ? 10800 - half
                                                          : half)
                                      .margin(1e-5));
    CHECK(window.session().part_document()->find_revolve_feature(id)->kind() ==
          (operation == 2 ? RevolveFeatureKind::RevolveCut : RevolveFeatureKind::Revolve));
    CHECK(window.session().part_document()->body_count() == (operation == 0 ? 2 : 1));
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
  }
}

TEST_CASE("Revolve construction derived datum and generated edge axes retain typed identity",
          "[gui][block136-revolve]") {
  for (const auto* choice : {"construction-line/line.x", "datum-axis/axis.derived.x",
                             "construction-source/12:feature.base/edge/bottom_back"}) {
    ShellWindow window;
    seed(window, "datum.xy", {0, 5}, 12, true);
    const auto before = snapshot(window);
    axis(window, choice);
    choose(window, "blcad.revolve.extent", 0);
    applicable(window);
    const auto body = window.feature_tools().coordinator().revolve().result_body_id();
    const auto id = window.feature_tools().coordinator().revolve().feature_id();
    REQUIRE(window.feature_tools().apply());
    CHECK(volume(window, body) ==
          Approx((std::string(choice).starts_with("construction-source") ? 1920 : 480) *
                 std::numbers::pi)
              .margin(1e-5));
    const auto source =
        window.session().part_document()->find_revolve_feature(id)->axis().source_kind();
    CHECK(source == (std::string(choice).starts_with("construction-source")
                         ? PartFeatureInputSourceKind::SemanticLinearEdge
                     : std::string(choice).starts_with("datum-axis")
                         ? PartFeatureInputSourceKind::DatumAxis
                         : PartFeatureInputSourceKind::ConstructionLine));
    QTemporaryDir directory;
    const auto path = directory.filePath("axes.blcad");
    REQUIRE(window.save_to(path));
    const auto after = snapshot(window);
    REQUIRE(window.open_path(path));
    CHECK(snapshot(window) == after);
    CHECK(volume(window, body) > 0);
    CHECK(before != after);
  }
}

TEST_CASE("Revolve circular feature axes share the execution frame and canonical persistence",
          "[gui][block136-revolve]") {
  ShellWindow window;
  seed(window, "datum.xz", {5, 0}, 4, true, true);
  axis(window, "feature-axis/feature.hole");
  choose(window, "blcad.revolve.extent", 0);
  applicable(window);
  const auto body = window.feature_tools().coordinator().revolve().result_body_id();
  const auto id = window.feature_tools().coordinator().revolve().feature_id();
  REQUIRE(window.feature_tools().apply());
  CHECK(volume(window, body) == Approx(160 * std::numbers::pi).margin(1e-5));
  CHECK(window.session().part_document()->find_revolve_feature(id)->axis().source_kind() ==
        PartFeatureInputSourceKind::SemanticAxis);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("circular-axis.blcad")));
  const auto after = snapshot(window);
  REQUIRE(window.open_path(directory.filePath("circular-axis.blcad")));
  CHECK(snapshot(window) == after);
}

TEST_CASE("Revolve invalid initial axes remain correctable and incompatible axes are omitted",
          "[gui][block136-revolve]") {
  ShellWindow window;
  seed(window, "datum.xy", {15, 0});
  CHECK(window.feature_tools().active());
  CHECK_FALSE(field<QPushButton>(window, "blcad.feature.apply").isEnabled());
  const auto before = snapshot(window);
  CHECK(field<QComboBox>(window, "blcad.revolve.axis").findData("datum-axis/datum.axis.z") < 0);
  axis(window, "datum-axis/datum.axis.y");
  applicable(window);
  axis(window, "datum-axis/datum.axis.x");
  CHECK_FALSE(window.feature_tools().apply());
  CHECK(snapshot(window) == before);
  axis(window, "datum-axis/datum.axis.y");
  applicable(window);
  REQUIRE(window.feature_tools().apply());
}

TEST_CASE("Revolve mode switching preserves angle rollback and discards local state on cancel",
          "[gui][block136-revolve]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.set_numeric_text(QStringLiteral("90 deg")));
  for (const auto* text : {"nan", "360", "0 deg", "-4 deg", "20 mm"}) {
    CHECK_FALSE(tools.set_numeric_text(QString::fromLatin1(text)));
    CHECK(tools.numeric_editor().text() == "90");
    CHECK(snapshot(window) == before);
  }
  choose(window, "blcad.revolve.side", 1);
  CHECK(tools.manipulators().handles().front().plane_normal.x == -1);
  choose(window, "blcad.revolve.extent", 0);
  CHECK(tools.manipulators().handles().empty());
  CHECK_FALSE(tools.set_numeric_value(30));
  choose(window, "blcad.revolve.extent", 2);
  CHECK(tools.manipulators().handles().front().plane_normal.x == 1);
  CHECK(tools.coordinator().revolve().angle() == 90);
  tools.cancel();
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.viewport().feature_preview_active());
  REQUIRE(tools.select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(tools.begin_revolve());
  CHECK(field<QComboBox>(window, "blcad.revolve.extent").currentIndex() == 1);
  CHECK(field<QComboBox>(window, "blcad.revolve.side").currentIndex() == 0);
  CHECK(tools.coordinator().revolve().angle() == 180);
  tools.cancel();
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("replacement-source.blcad")));
  REQUIRE(tools.select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
  REQUIRE(tools.begin_revolve());
  REQUIRE(window.new_part(QStringLiteral("Replacement")));
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}

TEST_CASE("Revolve supports offset construction axes and mixed-feature global identity allocation",
          "[gui][block136-revolve]") {
  ShellWindow window;
  seed(window);
  window.feature_tools().cancel();
  REQUIRE(window.session().commit_part_transaction(
      "Offset axes and reserved identities", [](PartDocument& part) {
        REQUIRE(part.add_body(
            reference_checked(Body::create(BodyId("body.1"), "Existing", BodyKind::Solid))));
        auto feature = reference_checked(
            Feature::create_additive_extrude(FeatureId("feature.existing"), "Existing",
                                             SketchId("sketch.tool"), ParameterId("depth")));
        REQUIRE(part.add_feature(reference_checked(
            feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
                FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.1")))))));
        REQUIRE(part.add_sketch(reference_checked(
            Sketch::create(SketchId("feature.drehung.1"), "Reserved", DatumPlaneId("datum.xy")))));
        REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
            ParameterId("body.2"), "Reserved body", reference_checked(Quantity::length_mm(1))))));
        REQUIRE(part.add_construction_plane(reference_checked(
            ConstructionPlane::create_explicit(ConstructionPlaneId("plane.offset"), "Offset",
                                               {0, 0, 20}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
        REQUIRE(part.add_construction_line(reference_checked(ConstructionLine::create_explicit(
            ConstructionLineId("line.offset"), "Offset axis", {0, 0, 20}, {1, 0, 0}))));
        auto sketch = reference_checked(Sketch::create(SketchId("sketch.offset"), "Offset profile",
                                                       DatumPlaneId("plane.offset")));
        REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
            ProfileId("profile.offset"), ParameterId("width"), ParameterId("height"), {15, 5}))));
        return part.add_sketch(sketch);
      }));
  window.refresh();
  REQUIRE(window.feature_tools().select_profile(SketchId("sketch.offset"),
                                                ProfileId("profile.offset")));
  REQUIRE(field<QAction>(window, "blcad.action.revolve").isEnabled());
  const auto before = snapshot(window);
  REQUIRE(window.feature_tools().begin_revolve());
  CHECK(window.feature_tools().coordinator().revolve().feature_id() ==
        FeatureId("feature.drehung.3"));
  axis(window, "construction-line/line.offset");
  applicable(window);
  REQUIRE(window.feature_tools().apply());
  CHECK(volume(window, BodyId("body.3")) == Approx(200 * std::numbers::pi).margin(1e-5));
  CHECK(volume(window, BodyId("body.1")) == Approx(480));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  REQUIRE(window.feature_tools().select_profile(SketchId("sketch.offset"),
                                                ProfileId("profile.offset")));
  REQUIRE(window.feature_tools().begin_extrude());
  CHECK(window.feature_tools().coordinator().extrude().result_body_id() == BodyId("body.4"));
  window.feature_tools().cancel();
  CHECK(snapshot(window) == after);
}

TEST_CASE("Revolve offers all editable Sketch line roles through its in-plane selector",
          "[gui][block136-revolve][sketch-line-axis-revolve]") {
  for (const auto role :
       {SketchLineRole::Geometry, SketchLineRole::Construction, SketchLineRole::Centerline}) {
    ShellWindow window;
    seed(window, "datum.xy", {20, 16}, 4);
    window.feature_tools().cancel();
    REQUIRE(window.session().commit_part_transaction(
        "Oblique Sketch axis", [&](PartDocument& part) -> Result<std::size_t> {
          auto source =
              Sketch::create(SketchId("sketch.axis"), "Oblique", DatumPlaneId("datum.xy"));
          if (!source)
            return Result<std::size_t>::failure(source.error());
          auto added = source.value().add_entity(
              LineSegment::create(SketchEntityId("line"), {20, 0}, {30, 10}, role).value());
          if (!added)
            return added;
          return part.add_sketch(source.value());
        }));
    REQUIRE(
        window.feature_tools().select_profile(SketchId("sketch.tool"), ProfileId("profile.tool")));
    REQUIRE(window.feature_tools().begin_revolve());
    auto source =
        SketchLineReference::create(SketchId("sketch.axis"), SketchEntityId("line")).value();
    axis(window, QString::fromStdString(source.identity()));
    choose(window, "blcad.revolve.extent", 0);
    applicable(window);
    const auto body = window.feature_tools().coordinator().revolve().result_body_id();
    const auto before = snapshot(window);
    REQUIRE(window.feature_tools().apply());
    CHECK(std::get<SketchLineReference>(
              window.session().part_document()->revolve_features().back().axis().source()) ==
          source);
    CHECK(volume(window, body) > 100);
    const auto after = snapshot(window);
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
    REQUIRE(window.redo());
    CHECK(snapshot(window) == after);
  }
}
