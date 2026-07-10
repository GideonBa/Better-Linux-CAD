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

Sketch make_composite_sketch(SketchId id) {
  auto sketch = Sketch::create(id, "Composite", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "outer.bottom", Point2{-20.0, -10.0}, Point2{20.0, -10.0});
  add_line(sketch.value(), "outer.right", Point2{20.0, -10.0}, Point2{20.0, 10.0});
  add_line(sketch.value(), "outer.top", Point2{20.0, 10.0}, Point2{-20.0, 10.0});
  add_line(sketch.value(), "outer.left", Point2{-20.0, 10.0}, Point2{-20.0, -10.0});
  add_line(sketch.value(), "inner.bottom", Point2{-5.0, -3.0}, Point2{5.0, -3.0});
  add_line(sketch.value(), "inner.right", Point2{5.0, -3.0}, Point2{5.0, 3.0});
  add_line(sketch.value(), "inner.top", Point2{5.0, 3.0}, Point2{-5.0, 3.0});
  add_line(sketch.value(), "inner.left", Point2{-5.0, 3.0}, Point2{-5.0, -3.0});
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.composite"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top"),
       SketchEntityId("outer.left")},
      {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top"),
        SketchEntityId("inner.left")}});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_additive_document() {
  auto document = PartDocument::create(DocumentId("part.composite.additive"), "CompositeAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_composite_sketch(SketchId("sketch.composite"))));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.composite"), "Composite",
                                                  SketchId("sketch.composite"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_document() {
  auto document = PartDocument::create(DocumentId("part.composite.cut"), "CompositeCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 8.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                               ParameterId("part.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base_feature = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                       SketchId("sketch.base"),
                                                       ParameterId("part.depth"));
  REQUIRE(base_feature);
  REQUIRE(document.value().add_feature(base_feature.value()));

  REQUIRE(document.value().add_sketch(make_composite_sketch(SketchId("sketch.composite"))));
  auto cut_feature = Feature::create_subtractive_extrude(FeatureId("feature.composite_cut"), "Cut",
                                                         SketchId("sketch.composite"),
                                                         FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes additive extrude from composite closed profile with a hole",
          "[geometry][composite-profile]") {
  PartDocument document = make_additive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.composite.additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.composite"));

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.value().final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 < 4000.0);
}

TEST_CASE("Geometry recomputes subtractive cut from composite closed profile with a hole",
          "[geometry][composite-profile]") {
  PartDocument document = make_subtractive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.composite.cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.composite_cut"));
}
