#include "blcad/geometry/shape_cache.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

GeometryShape make_plate_shape() {
  const auto width = Quantity::length_mm(80.0, "plate.width");
  const auto height = Quantity::length_mm(40.0, "plate.height");
  const auto thickness = Quantity::length_mm(10.0, "plate.thickness");

  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(thickness);

  const RectangleExtrusionAdapter adapter;
  auto shape = adapter.make_extruded_rectangle(width.value(), height.value(), thickness.value());
  REQUIRE(shape);

  return shape.value();
}

} // namespace

TEST_CASE("ShapeCache starts empty with a stable id", "[geometry][shape_cache]") {
  const auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));

  REQUIRE(cache);
  CHECK(cache.value().id().value() == "shape_cache.part");
  CHECK(cache.value().feature_shape_count() == 0);
  CHECK_FALSE(cache.value().has_final_shape());
  CHECK(cache.value().final_shape() == nullptr);
}

TEST_CASE("ShapeCache rejects empty cache ids", "[geometry][shape_cache]") {
  const auto cache = ShapeCache::create(ShapeCacheId());

  REQUIRE(cache.has_error());
  CHECK(cache.error().category() == ErrorCategory::Validation);
  CHECK(cache.error().object_id() == "shape_cache");
  CHECK(cache.error().message() == "shape cache id must not be empty");
}

TEST_CASE("ShapeCache stores and finds feature shapes", "[geometry][shape_cache]") {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  const auto stored =
      cache.value().store_feature_shape(FeatureId("feature.base_extrude"), make_plate_shape());

  REQUIRE(stored);
  CHECK(stored.value() == 0);
  CHECK(cache.value().feature_shape_count() == 1);

  const GeometryShape* shape = cache.value().find_feature_shape(FeatureId("feature.base_extrude"));
  REQUIRE(shape != nullptr);
  CHECK_FALSE(shape->empty());

  const RectangleExtrusionAdapter adapter;
  const ShapeSummary summary = adapter.summarize(*shape);
  CHECK_FALSE(summary.is_null);
  CHECK(summary.solid_count == 1);
}

TEST_CASE("ShapeCache updates existing feature shapes instead of duplicating them",
          "[geometry][shape_cache]") {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  const auto first =
      cache.value().store_feature_shape(FeatureId("feature.base_extrude"), make_plate_shape());
  const auto second =
      cache.value().store_feature_shape(FeatureId("feature.base_extrude"), make_plate_shape());

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == 0);
  CHECK(second.value() == 0);
  CHECK(cache.value().feature_shape_count() == 1);
}

TEST_CASE("ShapeCache tracks the final shape and source feature", "[geometry][shape_cache]") {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  const auto stored =
      cache.value().set_final_shape(FeatureId("feature.base_extrude"), make_plate_shape());

  REQUIRE(stored);
  CHECK(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id().value() == "feature.base_extrude");
  REQUIRE(cache.value().final_shape() != nullptr);
  CHECK_FALSE(cache.value().final_shape()->empty());
  CHECK(cache.value().feature_shape_count() == 1);
}

TEST_CASE("ShapeCache clear removes cached feature and final shapes", "[geometry][shape_cache]") {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  REQUIRE(cache.value().set_final_shape(FeatureId("feature.base_extrude"), make_plate_shape()));

  cache.value().clear();

  CHECK(cache.value().feature_shape_count() == 0);
  CHECK_FALSE(cache.value().has_final_shape());
  CHECK(cache.value().final_feature_id().empty());
  CHECK(cache.value().final_shape() == nullptr);
}

TEST_CASE("ShapeCache rejects empty feature ids and empty shapes", "[geometry][shape_cache]") {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.part"));
  REQUIRE(cache);

  const auto missing_feature = cache.value().store_feature_shape(FeatureId(), make_plate_shape());
  REQUIRE(missing_feature.has_error());
  CHECK(missing_feature.error().category() == ErrorCategory::Validation);
  CHECK(missing_feature.error().object_id() == "shape_cache");
  CHECK(missing_feature.error().message() == "feature id must not be empty");

  const auto empty_shape =
      cache.value().store_feature_shape(FeatureId("feature.base_extrude"), GeometryShape());
  REQUIRE(empty_shape.has_error());
  CHECK(empty_shape.error().category() == ErrorCategory::Geometry);
  CHECK(empty_shape.error().object_id() == "feature.base_extrude");
  CHECK(empty_shape.error().message() == "shape cache cannot store an empty shape");
}
