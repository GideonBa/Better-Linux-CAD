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

PartDocument make_back_face_cut_document(Point2 hole_center = {}) {
  auto document = PartDocument::create(DocumentId("part.back_face_plate"), "BackFacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.side_hole_diameter", "side_hole_diameter", 4.0)));

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

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto face_reference =
      SemanticFaceReference::create(FeatureId("feature.base_extrude"), SemanticFace::Back);
  REQUIRE(face_reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(DatumPlaneId("workplane.base_back"),
                                                            "BaseBackFace", face_reference.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));

  auto side_hole_sketch = Sketch::create(SketchId("sketch.back_hole"), "Sketch_BackHole",
                                         DatumPlaneId("workplane.base_back"));
  REQUIRE(side_hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.back_hole"),
                                      ParameterId("part.side_hole_diameter"), hole_center);
  REQUIRE(circle);
  REQUIRE(side_hole_sketch.value().add_profile(circle.value()));
  REQUIRE(document.value().add_sketch(side_hole_sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.back_hole_cut"), "BackHoleCut",
                                                 SketchId("sketch.back_hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.back_face"));
  REQUIRE(cache);

  return cache.value();
}

} // namespace

TEST_CASE("Back-face workplane recomputes an axis-directed through-all circular cut",
          "[geometry][workplane]") {
  const PartDocument document = make_back_face_cut_document(Point2{-12.0, 0.0});
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.back_hole_cut");

  const RectangleExtrusionAdapter inspector;
  const GeometryShape* base_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(base_shape != nullptr);
  const double base_volume = inspector.summarize(*base_shape).volume_mm3;

  REQUIRE(cache.final_shape() != nullptr);
  const ShapeSummary final_summary = inspector.summarize(*cache.final_shape());
  REQUIRE(final_summary.solid_count == 1);

  const double expected_removed_volume = std::numbers::pi * 2.0 * 2.0 * 80.0;
  CHECK(final_summary.volume_mm3 ==
        Catch::Approx(base_volume - expected_removed_volume).margin(2.0));
}

TEST_CASE("Back-face workplane rejects holes outside side-face bounds", "[geometry][workplane]") {
  const PartDocument document = make_back_face_cut_document(Point2{0.0, 3.5});
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary.has_error());
  CHECK(summary.error().category() == ErrorCategory::Validation);
  CHECK(summary.error().object_id() == "profile.back_hole");
  CHECK(summary.error().message() ==
        "circle profile must lie fully inside resolved workplane bounds");

  REQUIRE(cache.find_feature_shape(FeatureId("feature.base_extrude")) != nullptr);
  CHECK(cache.find_feature_shape(FeatureId("feature.back_hole_cut")) == nullptr);
}
