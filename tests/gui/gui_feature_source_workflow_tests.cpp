#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_measure.hpp"

#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result.has_error() ? result.error().message() : "success"));
  REQUIRE(result);
  return std::move(result.value());
}

Sketch source(double width, bool closed = true) {
  auto sketch = checked(Sketch::create(SketchId("source"), "Source", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_entity(checked(LineSegment::create(SketchEntityId("a"), {0, 0}, {width, 0}))));
  REQUIRE(sketch.add_entity(
      checked(LineSegment::create(SketchEntityId("b"), {width, 0}, {width, 10}))));
  REQUIRE(
      sketch.add_entity(checked(LineSegment::create(SketchEntityId("c"), {width, 10}, {0, 10}))));
  if (closed)
    REQUIRE(sketch.add_entity(checked(LineSegment::create(SketchEntityId("d"), {0, 10}, {0, 0}))));
  return sketch;
}

GuiDocumentSession fixture() {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("source-workflow"), "Source workflow"));
  REQUIRE(session.commit_part_transaction("Seed raw Sketch", [](PartDocument& part) {
    REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
    return part.add_sketch(source(20));
  }));
  return session;
}

void change_source(GuiDocumentSession& session, double width, bool closed = true) {
  REQUIRE(session.commit_part_transaction("Edit raw source", [&](PartDocument& part) {
    return part.update_sketch(source(width, closed));
  }));
}

GuiInteractiveExtrudeController controller(const GuiDocumentSession& session) {
  const auto regions = checked(analyze_interactive_raw_profiles(
      *session.part_document(), *session.part_document()->find_sketch(SketchId("source"))));
  REQUIRE(regions.regions.size() == 1);
  GuiInteractiveExtrudeController result;
  REQUIRE(result.begin_with_new_inputs(
      session, SketchId("source"), regions.regions.front().id, FeatureId("extrude"), "Extrude",
      checked(
          Parameter::create_length(ParameterId("depth"), "Depth", checked(Quantity::length_mm(8)))),
      checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  return result;
}

std::string json(const GuiDocumentSession& session) {
  return checked(serialize_part_document_to_json(*session.part_document()));
}

double volume(const geometry::GeometryShape& shape) {
  return geometry::RectangleExtrusionAdapter{}.summarize(shape).volume_mm3;
}
} // namespace

TEST_CASE(
    "Explicit Extrude source refresh preserves settings and replaces pending raw-profile intent",
    "[gui][parity-b145][source-sketch-workflow][source-sketch-controller]") {
  auto session = fixture();
  const auto before = json(session);
  REQUIRE(session.begin_part_workflow());
  auto extrude = controller(session);
  CHECK(volume(checked(extrude.preview_shape(session))) == Catch::Approx(1600).margin(1e-6));
  change_source(session, 30, false);
  const auto open = json(session);
  CHECK_FALSE(extrude.refresh_source(session));
  CHECK(extrude.handles().empty());
  CHECK_FALSE(extrude.preview(session));
  CHECK_FALSE(extrude.preview_shape(session));
  CHECK_FALSE(extrude.apply(session));
  CHECK(json(session) == open); // Older pending materialization cannot overwrite the edited source.
  CHECK(extrude.active());
  CHECK(extrude.distance() == 8);
  change_source(session, 30);
  REQUIRE(extrude.refresh_source(session));
  CHECK(volume(checked(extrude.preview_shape(session))) == Catch::Approx(2400).margin(1e-6));
  REQUIRE(extrude.apply(session));
  REQUIRE(session.publish_part_workflow("Extrude with changed source"));
  REQUIRE(session.part_shape_cache()->find_body_shape(BodyId("body")));
  CHECK(volume(*session.part_shape_cache()->find_body_shape(BodyId("body"))) ==
        Catch::Approx(2400).margin(1e-6));
  REQUIRE(session.undo());
  CHECK(json(session) == before);
  CHECK(session.part_document()->find_sketch(SketchId("source"))->profile_count() == 0);
  REQUIRE(session.redo());
  CHECK(volume(*session.part_shape_cache()->find_body_shape(BodyId("body"))) ==
        Catch::Approx(2400).margin(1e-6));
}

TEST_CASE("Thin Extrude source refresh rejects a second line and revalidates corrected geometry",
          "[gui][parity-b145][source-sketch-workflow][source-sketch-controller]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("thin-source"), "Thin source"));
  const auto line = [](double length, bool extra) {
    auto sketch = checked(Sketch::create(SketchId("source"), "Line", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_entity(
        checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {length, 0}))));
    if (extra)
      REQUIRE(sketch.add_entity(
          checked(LineSegment::create(SketchEntityId("other"), {length, 0}, {length, 5}))));
    return sketch;
  };
  REQUIRE(session.commit_part_transaction("Seed line", [&](PartDocument& part) {
    REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
    return part.add_sketch(line(20, false));
  }));
  const auto before = json(session);
  REQUIRE(session.begin_part_workflow());
  GuiInteractiveExtrudeController extrude;
  REQUIRE(extrude.begin_with_new_inputs(
      session, SketchId("source"), std::nullopt, FeatureId("thin"), "Thin",
      checked(
          Parameter::create_length(ParameterId("depth"), "Depth", checked(Quantity::length_mm(8)))),
      checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  extrude.set_thin_new_inputs(ExtrudeThinMode::MidPlane,
                              checked(Parameter::create_length(ParameterId("wall"), "Wall",
                                                               checked(Quantity::length_mm(1)))));
  CHECK(volume(checked(extrude.preview_shape(session))) == Catch::Approx(160).margin(1e-6));
  REQUIRE(session.commit_part_transaction(
      "Add another line", [&](PartDocument& part) { return part.update_sketch(line(30, true)); }));
  CHECK_FALSE(extrude.refresh_source(session));
  CHECK_FALSE(extrude.apply(session));
  REQUIRE(session.commit_part_transaction(
      "Correct line", [&](PartDocument& part) { return part.update_sketch(line(30, false)); }));
  REQUIRE(extrude.refresh_source(session));
  CHECK(volume(checked(extrude.preview_shape(session))) == Catch::Approx(240).margin(1e-6));
  REQUIRE(extrude.apply(session));
  REQUIRE(session.publish_part_workflow("Apply thin source edit"));
  REQUIRE(session.undo());
  CHECK(json(session) == before);
  REQUIRE(session.redo());
  CHECK(volume(*session.part_shape_cache()->find_body_shape(BodyId("body"))) ==
        Catch::Approx(240).margin(1e-6));
}

TEST_CASE(
    "STEP and full-model Measure reject a Part workflow draft without consuming or writing it",
    "[gui][parity-b145][source-sketch-workflow][source-sketch-consumers]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  auto session = fixture();
  auto extrude = controller(session);
  REQUIRE(extrude.apply(session));
  GuiMeasureController measure;
  REQUIRE(measure.begin(session, GuiMeasureKind::BodySummary));
  REQUIRE(measure.select_body(BodyId("body")));
  CHECK(checked(measure.preview(session)).volume_mm3 == Catch::Approx(1600).margin(1e-6));
  const auto target = std::filesystem::path(directory.filePath("unchanged.step").toStdString());
  std::ofstream(target) << "existing export";
  GuiStepExportRequest request{GuiStepExportMode::PartMultiBody, target};
  REQUIRE(session.begin_part_workflow());
  CHECK_FALSE(measure.preview(session));
  GuiMeasureController another;
  CHECK_FALSE(another.begin(session, GuiMeasureKind::BodySummary));
  CHECK_FALSE(GuiAnalysisExportWorkbench{}.preflight_step_export(session, request));
  CHECK_FALSE(GuiAnalysisExportWorkbench{}.export_step(session, request));
  std::ifstream input(target);
  std::string bytes;
  std::getline(input, bytes);
  CHECK(bytes == "existing export");
  session.cancel_part_workflow();
  CHECK(checked(measure.preview(session)).volume_mm3 == Catch::Approx(1600).margin(1e-6));
  REQUIRE(GuiAnalysisExportWorkbench{}.preflight_step_export(session, request));
}
