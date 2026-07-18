#include "blcad/gui/gui_measure.hpp"
#include "blcad/gui/main_window.hpp"

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value())
      .value();
}

void build_measured_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.measure"), "Measure"));
  REQUIRE(session.commit_part_transaction("Build measured Part", [](PartDocument& part) {
    for (auto parameter : {length("width", 10.0), length("height", 20.0), length("depth", 3.0)}) {
      auto added = part.add_parameter(std::move(parameter));
      if (added.has_error())
        return added;
    }
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error())
      return added;
    auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
    if (auto added =
            sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                        ParameterId("width"), ParameterId("height"))
                                   .value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(sketch)); added.has_error())
      return added;
    if (auto added =
            part.add_body(Body::create(BodyId("body.main"), "Main", BodyKind::Solid).value());
        added.has_error())
      return added;
    auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                    BodyId("body.main"));
    if (context.has_error())
      return Result<std::size_t>::failure(context.error());
    auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                    SketchId("sketch.base"), ParameterId("depth"));
    if (feature.has_error())
      return Result<std::size_t>::failure(feature.error());
    auto contextual = feature.value().with_body_result_context(context.value());
    if (contextual.has_error())
      return Result<std::size_t>::failure(contextual.error());
    return part.add_feature(std::move(contextual.value()));
  }));
  REQUIRE(session.recompute());
}

geometry::AssemblyLineTargetDescriptor line(Point3 origin, Vector3 direction) {
  return {origin, direction};
}

geometry::AssemblyPlanarTargetDescriptor plane(Point3 origin, Vector3 normal) {
  return {origin, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, normal};
}

} // namespace

TEST_CASE("Block 131 Measure reports point edge face distances and angles without mutation",
          "[gui][measure]") {
  GuiDocumentSession session;
  build_measured_part(session);
  const std::string before = serialize_part_document_to_json(*session.part_document()).value();
  const auto revision = session.presentation_revision();

  GuiMeasureController measure;
  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(Point3{3.0, 4.0, 0.0}));
  REQUIRE(measure.add_target(Point3{0.0, 0.0, 0.0}));
  auto point_distance = measure.preview(session);
  REQUIRE(point_distance);
  CHECK(point_distance.value().primary_value == Approx(5.0));
  CHECK(point_distance.value().primary_unit == "mm");
  REQUIRE(point_distance.value().overlay_points.size() == 2U);

  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(geometry::AssemblyPointTargetDescriptor{{3.0, 4.0, 0.0}}));
  REQUIRE(measure.add_target(geometry::AssemblyPointTargetDescriptor{{0.0, 0.0, 0.0}}));
  CHECK(measure.preview(session).value().primary_value == Approx(5.0));

  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(Point3{0.0, 3.0, 4.0}));
  REQUIRE(measure.add_target(line({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0})));
  CHECK(measure.preview(session).value().primary_value == Approx(5.0));

  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(Point3{0.0, 0.0, 7.0}));
  REQUIRE(measure.add_target(plane({0.0, 0.0, 2.0}, {0.0, 0.0, 1.0})));
  CHECK(measure.preview(session).value().primary_value == Approx(5.0));

  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(line({0.0, 0.0, 7.0}, {1.0, 0.0, 0.0})));
  REQUIRE(measure.add_target(plane({0.0, 0.0, 2.0}, {0.0, 0.0, 1.0})));
  CHECK(measure.preview(session).value().primary_value == Approx(5.0));

  REQUIRE(measure.begin(session, GuiMeasureKind::Angle));
  REQUIRE(measure.add_target(line({}, {1.0, 0.0, 0.0})));
  REQUIRE(measure.add_target(line({}, {0.0, 1.0, 0.0})));
  auto angle = measure.preview(session);
  REQUIRE(angle);
  CHECK(angle.value().primary_value == Approx(90.0));
  CHECK(angle.value().primary_unit == "deg");

  REQUIRE(measure.begin(session, GuiMeasureKind::Angle));
  REQUIRE(measure.add_target(line({}, {1.0, 0.0, 0.0})));
  REQUIRE(measure.add_target(plane({}, {0.0, 0.0, 1.0})));
  CHECK(measure.preview(session).value().primary_value == Approx(0.0));

  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
  CHECK(session.presentation_revision() == revision);
}

TEST_CASE("Block 131 Measure reports radius diameter and existing Body summaries",
          "[gui][measure]") {
  GuiDocumentSession session;
  build_measured_part(session);
  GuiMeasureController measure;

  geometry::AssemblyCircularEdgeTargetDescriptor circle{
      {}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, 4.0};
  REQUIRE(measure.begin(session, GuiMeasureKind::RadiusDiameter));
  REQUIRE(measure.add_target(circle));
  auto radial = measure.preview(session);
  REQUIRE(radial);
  CHECK(radial.value().primary_value == Approx(4.0));
  REQUIRE(radial.value().secondary_value.has_value());
  CHECK(*radial.value().secondary_value == Approx(8.0));

  REQUIRE(measure.begin(session, GuiMeasureKind::BodySummary));
  REQUIRE(measure.select_body(BodyId("body.main")));
  auto body = measure.preview(session);
  REQUIRE(body);
  CHECK(body.value().solid_count == 1U);
  CHECK(body.value().volume_mm3 == Approx(600.0));
  CHECK(body.value().primary_unit == "mm3");
}

TEST_CASE("Block 131 Measure fails closed for incomplete incompatible and stale queries",
          "[gui][measure]") {
  GuiDocumentSession session;
  build_measured_part(session);
  const std::string before = serialize_part_document_to_json(*session.part_document()).value();
  GuiMeasureController measure;

  REQUIRE(measure.begin(session, GuiMeasureKind::Distance));
  REQUIRE(measure.add_target(Point3{}));
  CHECK(measure.preview(session).has_error());
  REQUIRE(measure.add_target(geometry::AssemblyCircularEdgeTargetDescriptor{
      {}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, 2.0}));
  CHECK(measure.preview(session).has_error());

  REQUIRE(measure.begin(session, GuiMeasureKind::BodySummary));
  REQUIRE(measure.select_body(BodyId("body.missing")));
  CHECK(measure.preview(session).has_error());
  session.set_derived_results_fresh(false);
  CHECK(measure.preview(session).has_error());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
}

TEST_CASE("Block 131 application shell owns the transient Measure command", "[gui][measure]") {
  MainWindow window;
  REQUIRE(window.session().create_part(DocumentId("part.measure.shell"), "Measure shell"));
  REQUIRE(window.measure_controller().begin(window.session(), GuiMeasureKind::Distance));
  CHECK(window.measure_controller().active());
  window.measure_controller().cancel();
  CHECK_FALSE(window.measure_controller().active());
}
