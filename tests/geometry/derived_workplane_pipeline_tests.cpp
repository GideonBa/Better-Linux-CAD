#include "blcad/geometry/recompute_executor.hpp"

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

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

PartDocument make_top_face_cut_document(Point2 hole_center = {}) {
  auto document = PartDocument::create(DocumentId("part.top_face_plate"), "TopFacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.top_hole_diameter", "top_hole_diameter", 20.0)));

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

  auto base = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                               SketchId("sketch.base"),
                                               ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto face_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                      SemanticFace::Top);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.base_top"), "BaseTopFace", face_reference.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));

  auto top_hole_sketch = Sketch::create(SketchId("sketch.top_hole"), "Sketch_TopHole",
                                        DatumPlaneId("workplane.base_top"));
  REQUIRE(top_hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.top_hole"),
                                      ParameterId("part.top_hole_diameter"), hole_center);
  REQUIRE(circle);
  REQUIRE(top_hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(top_hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.top_hole_cut"), "TopHoleCut",
                                                 SketchId("sketch.top_hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.top_face"));
  REQUIRE(cache);

  return cache.value();
}

} // namespace

TEST_CASE("MVP-2 seed recomputes a cut from a sketch on a derived top-face workplane",
          "[geometry][workplane]") {
  const PartDocument document = make_top_face_cut_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.top_hole_cut");

  const RectangleExtrusionAdapter inspector;
  const GeometryShape* base_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(base_shape != nullptr);
  const double base_volume = inspector.summarize(*base_shape).volume_mm3;

  REQUIRE(cache.final_shape() != nullptr);
  const ShapeSummary final_summary = inspector.summarize(*cache.final_shape());
  CHECK(final_summary.solid_count == 1);
  CHECK(final_summary.volume_mm3 < base_volume);
}

TEST_CASE("Derived top-face workplane cut follows an off-center sketch point",
          "[geometry][workplane]") {
  const PartDocument document = make_top_face_cut_document(Point2{25.0, -10.0});
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);

  const RectangleExtrusionAdapter inspector;
  const GeometryShape* base_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(base_shape != nullptr);
  const double base_volume = inspector.summarize(*base_shape).volume_mm3;

  REQUIRE(cache.final_shape() != nullptr);
  const ShapeSummary final_summary = inspector.summarize(*cache.final_shape());
  REQUIRE(final_summary.solid_count == 1);

  const double expected_removed_volume = std::numbers::pi * 10.0 * 10.0 * 8.0;
  CHECK(final_summary.volume_mm3 == Catch::Approx(base_volume - expected_removed_volume).margin(1.0));
}

TEST_CASE("Derived top-face workplane accepts a near-edge hole inside the face bounds",
          "[geometry][workplane]") {
  const PartDocument document = make_top_face_cut_document(Point2{50.0, 30.0});
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.top_hole_cut");
}

TEST_CASE("Derived top-face workplane rejects holes outside the face bounds",
          "[geometry][workplane]") {
  const PartDocument document = make_top_face_cut_document(Point2{55.0, 0.0});
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary.has_error());
  CHECK(summary.error().category() == ErrorCategory::Validation);
  CHECK(summary.error().object_id() == "profile.top_hole");
  CHECK(summary.error().message() == "circle profile must lie fully inside resolved workplane bounds");

  const GeometryShape* base_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(base_shape != nullptr);
  REQUIRE(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.base_extrude");
  CHECK(cache.find_feature_shape(FeatureId("feature.top_hole_cut")) == nullptr);
}
