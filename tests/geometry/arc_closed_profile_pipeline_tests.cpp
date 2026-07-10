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

void add_arc(Sketch& sketch, const char* id, Point2 start, Point2 mid, Point2 end) {
  auto arc = ArcSegment::create_three_point(SketchEntityId(id), start, mid, end);
  REQUIRE(arc);
  REQUIRE(sketch.add_entity(arc.value()));
}

Sketch make_arc_profile_sketch(SketchId id) {
  auto sketch = Sketch::create(id, "Arc", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.left", Point2{-10.0, 0.0}, Point2{-10.0, -10.0});
  add_line(sketch.value(), "line.bottom", Point2{-10.0, -10.0}, Point2{10.0, -10.0});
  add_line(sketch.value(), "line.right", Point2{10.0, -10.0}, Point2{10.0, 0.0});
  add_arc(sketch.value(), "arc.top", Point2{10.0, 0.0}, Point2{0.0, 10.0}, Point2{-10.0, 0.0});
  auto profile = ArcClosedProfile::create(
      ProfileId("profile.arc"), {SketchEntityId("line.left"), SketchEntityId("line.bottom"),
                                  SketchEntityId("line.right"), SketchEntityId("arc.top")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_additive_document() {
  auto document = PartDocument::create(DocumentId("part.arc.additive"), "ArcAdditive");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_arc_profile_sketch(SketchId("sketch.arc"))));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.arc"), "Arc",
                                                  SketchId("sketch.arc"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_subtractive_document() {
  auto document = PartDocument::create(DocumentId("part.arc.cut"), "ArcCut");
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

  REQUIRE(document.value().add_sketch(make_arc_profile_sketch(SketchId("sketch.arc"))));
  auto cut_feature = Feature::create_subtractive_extrude(FeatureId("feature.arc_cut"), "ArcCut",
                                                         SketchId("sketch.arc"),
                                                         FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes additive extrude from arc closed profile",
          "[geometry][arc-profile]") {
  PartDocument document = make_additive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.arc.additive"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.arc"));
}

TEST_CASE("Geometry recomputes subtractive cut from arc closed profile",
          "[geometry][arc-profile]") {
  PartDocument document = make_subtractive_document();
  auto cache = ShapeCache::create(ShapeCacheId("cache.arc.cut"));
  REQUIRE(cache);
  const GeometryRecomputeExecutor executor;
  auto summary = executor.execute_document(document, cache.value());
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id() == FeatureId("feature.arc_cut"));
}
