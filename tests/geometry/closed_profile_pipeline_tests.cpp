#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
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

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);
  return cache.value();
}

Sketch make_triangle_sketch(SketchId id, const char* name, DatumPlaneId workplane) {
  auto sketch = Sketch::create(id, name, workplane);
  REQUIRE(sketch);

  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{20.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{20.0, 0.0}, Point2{0.0, 20.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 20.0}, Point2{0.0, 0.0}).value()));

  auto profile = ClosedProfile::create(
      ProfileId("profile.triangle"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

Sketch make_small_triangle_cut_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.triangle_cut"), "Sketch_TriangleCut",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut_a"), Point2{-10.0, -10.0}, Point2{10.0, -10.0})
          .value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut_b"), Point2{10.0, -10.0}, Point2{-10.0, 10.0})
          .value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.cut_c"), Point2{-10.0, 10.0}, Point2{-10.0, -10.0})
          .value()));

  auto profile = ClosedProfile::create(
      ProfileId("profile.triangle_cut"),
      {SketchEntityId("line.cut_a"), SketchEntityId("line.cut_b"), SketchEntityId("line.cut_c")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

PartDocument make_triangle_prism_document() {
  auto document = PartDocument::create(DocumentId("part.triangle_prism"), "TrianglePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_triangle_sketch(
      SketchId("sketch.triangle"), "Sketch_Triangle", DatumPlaneId("datum.xy"))));

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.triangle_prism"), "TrianglePrism",
                                       SketchId("sketch.triangle"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

PartDocument make_rectangle_with_triangle_cut_document() {
  auto document = PartDocument::create(DocumentId("part.triangle_cut_plate"), "TriangleCutPlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  REQUIRE(document.value().add_sketch(make_small_triangle_cut_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.triangle_cut"), "TriangleCut",
                                                 SketchId("sketch.triangle_cut"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes an additive closed triangle profile", "[geometry][closed_profile]") {
  const PartDocument document = make_triangle_prism_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1);
  REQUIRE(cache.final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Catch::Approx(2000.0).margin(1.0));
}

TEST_CASE("Geometry recomputes a subtractive through-all closed profile cut",
          "[geometry][closed_profile]") {
  const PartDocument document = make_rectangle_with_triangle_cut_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.final_shape() != nullptr);
  CHECK(cache.final_feature_id().value() == "feature.triangle_cut");

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Catch::Approx(120.0 * 80.0 * 8.0 - 200.0 * 8.0).margin(2.0));
}
