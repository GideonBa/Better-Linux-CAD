#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

Sketch make_spline_profile_sketch(SketchId sketch_id, DatumPlaneId workplane_id) {
  auto sketch = Sketch::create(sketch_id, "SplineProfile", workplane_id);
  REQUIRE(sketch);

  add_line(sketch.value(), "line.bottom", Point2{-10.0, -8.0}, Point2{10.0, -8.0});
  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.right"), Point2{10.0, -8.0},
                                         Point2{18.0, -4.0}, Point2{18.0, 4.0}, Point2{10.0, 8.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));
  add_line(sketch.value(), "line.top", Point2{10.0, 8.0}, Point2{-10.0, 8.0});
  add_line(sketch.value(), "line.left", Point2{-10.0, 8.0}, Point2{-10.0, -8.0});

  auto tangent = SketchTangentContinuity::create_tangent(
      SketchConstraintId("tangent.bottom_spline"), SketchEntityId("line.bottom"),
      SketchEntityId("spline.right"));
  REQUIRE(tangent);
  REQUIRE(sketch.value().add_tangent_continuity(tangent.value()));

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.spline"), {SketchEntityId("line.bottom"), SketchEntityId("spline.right"),
                                    SketchEntityId("line.top"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_additive_spline_document() {
  auto document = PartDocument::create(DocumentId("part.spline_additive"), "SplineAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 6.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(
      make_spline_profile_sketch(SketchId("sketch.spline"), DatumPlaneId("datum.xy"))));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.spline"), "SplineExtrude",
                                       SketchId("sketch.spline"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_spline_document() {
  auto document = PartDocument::create(DocumentId("part.spline_cut"), "SplineCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 12.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  REQUIRE(document.value().add_sketch(
      make_spline_profile_sketch(SketchId("sketch.cut"), DatumPlaneId("datum.xy"))));
  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.cut"), "SplineCut",
                                                 SketchId("sketch.cut"), FeatureId("feature.base"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes additive extrude from spline closed profile", "[geometry][spline]") {
  PartDocument document = make_additive_spline_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.spline_additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;

  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  REQUIRE(cache.value().final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
}

TEST_CASE("Geometry recomputes subtractive cut from spline closed profile", "[geometry][spline]") {
  PartDocument document = make_subtractive_spline_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.spline_cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;

  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  REQUIRE(cache.value().final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
}
