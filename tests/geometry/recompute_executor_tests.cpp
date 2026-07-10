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

Sketch make_rectangle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  return sketch.value();
}

Sketch make_circle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_CircleOnly", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));

  return sketch.value();
}

PartDocument make_document_with_base_additive_extrude() {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_rectangle_sketch()));

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

Sketch make_hole_circle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.hole"), "Sketch_CenterHole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));

  return sketch.value();
}

PartDocument make_document_with_base_plate_and_hole_cut() {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_rectangle_sketch()));
  REQUIRE(document.value().add_sketch(make_hole_circle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

PartDocument make_document_with_circle_only_additive_extrude() {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_circle_sketch()));

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  return cache.value();
}

} // namespace

TEST_CASE("GeometryRecomputeExecutor executes an additive extrude into the shape cache",
          "[geometry][recompute]") {
  const PartDocument document = make_document_with_base_additive_extrude();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto executed =
      executor.execute_additive_extrude(document, FeatureId("feature.base_extrude"), cache);

  REQUIRE(executed);
  CHECK(executed.value() == 0);
  CHECK(cache.feature_shape_count() == 1);
  CHECK(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.base_extrude");

  const GeometryShape* feature_shape = cache.find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(feature_shape != nullptr);
  CHECK_FALSE(feature_shape->empty());

  REQUIRE(cache.final_shape() != nullptr);
  CHECK_FALSE(cache.final_shape()->empty());

  const RectangleExtrusionAdapter adapter;
  const ShapeSummary summary = adapter.summarize(*cache.final_shape());
  CHECK_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1);
}

TEST_CASE("GeometryRecomputeExecutor executes additive feature nodes from a recompute plan",
          "[geometry][recompute]") {
  auto document = make_document_with_base_additive_extrude();
  REQUIRE(document.mark_parameter_changed(ParameterId("part.width")));

  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(plan.value().contains("sketch.base"));
  REQUIRE(plan.value().contains("feature.base_extrude"));

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_plan(document, plan.value(), cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1);
  CHECK(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.base_extrude");
  CHECK(cache.feature_shape_count() == 1);
}

TEST_CASE("GeometryRecomputeExecutor rejects missing feature ids", "[geometry][recompute]") {
  const PartDocument document = make_document_with_base_additive_extrude();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto executed =
      executor.execute_additive_extrude(document, FeatureId("feature.missing"), cache);

  REQUIRE(executed.has_error());
  CHECK(executed.error().category() == ErrorCategory::Validation);
  CHECK(executed.error().object_id() == "feature.missing");
  CHECK(executed.error().message() == "feature must exist in part document");
  CHECK(cache.feature_shape_count() == 0);
  CHECK_FALSE(cache.has_final_shape());
}

TEST_CASE("GeometryRecomputeExecutor rejects unsupported additive sketches",
          "[geometry][recompute]") {
  const PartDocument document = make_document_with_circle_only_additive_extrude();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto executed =
      executor.execute_additive_extrude(document, FeatureId("feature.base_extrude"), cache);

  REQUIRE(executed.has_error());
  CHECK(executed.error().category() == ErrorCategory::Validation);
  CHECK(executed.error().object_id() == "sketch.base");
  CHECK(executed.error().message() ==
        "additive extrude requires exactly one rectangle, closed profile, composite closed profile, or detected simple region");
  CHECK(cache.feature_shape_count() == 0);
  CHECK_FALSE(cache.has_final_shape());
}

TEST_CASE("GeometryRecomputeExecutor cuts a central hole into a cached base body",
          "[geometry][recompute]") {
  const PartDocument document = make_document_with_base_plate_and_hole_cut();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto base =
      executor.execute_additive_extrude(document, FeatureId("feature.base_extrude"), cache);
  REQUIRE(base);

  const RectangleExtrusionAdapter inspector;
  const double base_volume = inspector.summarize(*cache.final_shape()).volume_mm3;

  const auto cut =
      executor.execute_subtractive_extrude(document, FeatureId("feature.center_hole_cut"), cache);

  REQUIRE(cut);
  CHECK(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.center_hole_cut");

  REQUIRE(cache.final_shape() != nullptr);
  const ShapeSummary summary = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1);
  CHECK(summary.volume_mm3 < base_volume);
}

TEST_CASE("GeometryRecomputeExecutor runs additive then subtractive nodes from a plan",
          "[geometry][recompute]") {
  auto document = make_document_with_base_plate_and_hole_cut();
  REQUIRE(document.mark_parameter_changed(ParameterId("part.width")));

  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(plan.value().contains("feature.base_extrude"));
  REQUIRE(plan.value().contains("feature.center_hole_cut"));

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_plan(document, plan.value(), cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  CHECK(cache.has_final_shape());
  CHECK(cache.final_feature_id().value() == "feature.center_hole_cut");
}

TEST_CASE("GeometryRecomputeExecutor requires the target body in the cache before cutting",
          "[geometry][recompute]") {
  const PartDocument document = make_document_with_base_plate_and_hole_cut();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto cut =
      executor.execute_subtractive_extrude(document, FeatureId("feature.center_hole_cut"), cache);

  REQUIRE(cut.has_error());
  CHECK(cut.error().category() == ErrorCategory::Geometry);
  CHECK(cut.error().object_id() == "feature.base_extrude");
  CHECK(cut.error().message() == "target feature shape must exist in the shape cache");
  CHECK_FALSE(cache.has_final_shape());
}

TEST_CASE("GeometryRecomputeExecutor rejects unsupported subtractive sketches",
          "[geometry][recompute]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_rectangle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.base"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto executed = executor.execute_subtractive_extrude(
      document.value(), FeatureId("feature.center_hole_cut"), cache);

  REQUIRE(executed.has_error());
  CHECK(executed.error().category() == ErrorCategory::Validation);
  CHECK(executed.error().object_id() == "sketch.base");
  CHECK(executed.error().message() ==
        "subtractive extrude requires exactly one circle, closed profile, composite closed profile, or detected simple region");
  CHECK_FALSE(cache.has_final_shape());
}
