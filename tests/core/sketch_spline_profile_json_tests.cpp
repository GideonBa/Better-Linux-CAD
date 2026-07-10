#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Sketch make_spline_profile_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.spline"), "SplineSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto bottom =
      LineSegment::create(SketchEntityId("line.bottom"), Point2{0.0, 0.0}, Point2{20.0, 0.0});
  REQUIRE(bottom);
  REQUIRE(sketch.value().add_entity(bottom.value()));

  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.right"), Point2{20.0, 0.0},
                                         Point2{30.0, 5.0}, Point2{30.0, 15.0}, Point2{20.0, 20.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));

  auto left =
      LineSegment::create(SketchEntityId("line.left"), Point2{20.0, 20.0}, Point2{0.0, 0.0});
  REQUIRE(left);
  REQUIRE(sketch.value().add_entity(left.value()));

  auto tangent = SketchTangentContinuity::create_tangent(
      SketchConstraintId("tangent.bottom_spline"), SketchEntityId("line.bottom"),
      SketchEntityId("spline.right"));
  REQUIRE(tangent);
  REQUIRE(sketch.value().add_tangent_continuity(tangent.value()));

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.spline"),
      {SketchEntityId("line.bottom"), SketchEntityId("spline.right"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

PartDocument make_document() {
  auto document = PartDocument::create(DocumentId("part.spline_json"), "SplineJson");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(make_spline_profile_sketch()));
  return document.value();
}

} // namespace

TEST_CASE("Spline sketch profile and tangent metadata roundtrip through JSON",
          "[core][json][spline]") {
  PartDocument document = make_document();

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"spline_segments\"") != std::string::npos);
  CHECK(serialized.value().find("\"tangent_continuities\"") != std::string::npos);
  CHECK(serialized.value().find("\"kind\": \"cubic_bezier\"") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);

  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.spline"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->find_spline_segment(SketchEntityId("spline.right")) != nullptr);
  REQUIRE(sketch->find_tangent_continuity(SketchConstraintId("tangent.bottom_spline")) != nullptr);
  REQUIRE(sketch->find_arc_closed_profile(ProfileId("profile.spline")) != nullptr);

  const SplineSegment* spline = sketch->find_spline_segment(SketchEntityId("spline.right"));
  REQUIRE(spline != nullptr);
  CHECK(spline->control1() == Point2{30.0, 5.0});
  CHECK(spline->control2() == Point2{30.0, 15.0});
}
