#include "blcad/geometry/linear_pattern_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter count_parameter(const char* id, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* body_id) {
  auto value = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                BodyId(body_id));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext modifying_context(FeatureBodyOperationMode mode, const char* body_id) {
  auto value = FeatureBodyResultContext::create(mode, BodyId(body_id), std::nullopt);
  REQUIRE(value);
  return value.value();
}

AxisReference pattern_axis() {
  auto value =
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.x"));
  REQUIRE(value);
  return value.value();
}

PatternSourceReference feature_source(const char* id) {
  auto value = PatternSourceReference::feature(FeatureId(id));
  REQUIRE(value);
  return value.value();
}

PatternSourceReference body_source(const char* id) {
  auto value = PatternSourceReference::body(BodyId(id));
  REQUIRE(value);
  return value.value();
}

ShapeCache shape_cache() {
  auto value = ShapeCache::create(ShapeCacheId("cache.linear_pattern"));
  REQUIRE(value);
  return value.value();
}

double volume(const ShapeCache& cache, const char* body_id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

GeometryShapeBounds bounds(const ShapeCache& cache, const char* body_id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  auto value = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(value);
  return value.value();
}

void add_box_feature(PartDocument& document, const char* prefix, const char* body_id, double width,
                     double height, double depth, Point2 center = {}) {
  const std::string width_id = std::string(prefix) + ".width";
  const std::string height_id = std::string(prefix) + ".height";
  const std::string depth_id = std::string(prefix) + ".depth";
  const std::string sketch_id = std::string("sketch.") + prefix;
  const std::string profile_id = std::string("profile.") + prefix;
  const std::string feature_id = std::string("feature.") + prefix;
  REQUIRE(document.add_parameter(length_parameter(width_id.c_str(), width)));
  REQUIRE(document.add_parameter(length_parameter(height_id.c_str(), height)));
  REQUIRE(document.add_parameter(length_parameter(depth_id.c_str(), depth)));
  auto sketch = Sketch::create(SketchId(sketch_id), prefix, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId(profile_id), ParameterId(width_id),
                                            ParameterId(height_id), center);
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  REQUIRE(document.add_body(body(body_id)));
  auto feature = Feature::create_additive_extrude(FeatureId(feature_id), prefix,
                                                  SketchId(sketch_id), ParameterId(depth_id));
  REQUIRE(feature);
  auto with_context = feature.value().with_body_result_context(new_body_context(body_id));
  REQUIRE(with_context);
  REQUIRE(document.add_feature(with_context.value()));
}

PartDocument new_body_pattern(bool use_body_source, LinearPatternExtentMode extent_mode,
                              PatternDirectionSign sign, double count = 4.0, double extent = 20.0) {
  auto document = PartDocument::create(DocumentId("part.linear_pattern"), "LinearPattern");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1.0, 0.0, 0.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", count)));
  REQUIRE(document.value().add_parameter(length_parameter("pattern.extent", extent)));
  add_box_feature(document.value(), "source", "body.source", 10.0, 10.0, 10.0);
  REQUIRE(document.value().add_body(body("body.pattern")));
  const auto source =
      use_body_source ? body_source("body.source") : feature_source("feature.source");
  auto pattern = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear", {source}, pattern_axis(), ParameterId("pattern.count"),
      extent_mode, ParameterId("pattern.extent"), sign, new_body_context("body.pattern"));
  REQUIRE(pattern);
  REQUIRE(document.value().add_linear_pattern_feature(pattern.value()));
  return document.value();
}

PartDocument modifying_pattern(FeatureBodyOperationMode mode) {
  auto document = PartDocument::create(DocumentId("part.linear_pattern.operation"), "Operation");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1.0, 0.0, 0.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", 3.0)));
  const double spacing = mode == FeatureBodyOperationMode::Join ? 20.0 : 30.0;
  REQUIRE(document.value().add_parameter(length_parameter("pattern.spacing", spacing)));
  const double target_width = mode == FeatureBodyOperationMode::Join ? 20.0 : 100.0;
  const double source_center = mode == FeatureBodyOperationMode::Join ? 15.0 : -30.0;
  add_box_feature(document.value(), "target", "body.target", target_width, 20.0, 10.0);
  add_box_feature(document.value(), "tool", "body.tool", 8.0, 8.0, 10.0, {source_center, 0.0});
  auto pattern = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear", {body_source("body.tool")}, pattern_axis(),
      ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.spacing"), PatternDirectionSign::Positive,
      modifying_context(mode, "body.target"));
  REQUIRE(pattern);
  REQUIRE(document.value().add_linear_pattern_feature(pattern.value()));
  return document.value();
}

GeometryShape box(double size) {
  auto width = Quantity::length_mm(size, "box.width");
  auto height = Quantity::length_mm(size, "box.height");
  auto depth = Quantity::length_mm(size, "box.depth");
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(depth);
  auto shape = RectangleExtrusionAdapter{}.make_extruded_rectangle(width.value(), height.value(),
                                                                   depth.value());
  REQUIRE(shape);
  return shape.value();
}

} // namespace

TEST_CASE("Block 64 adapter preserves deterministic instance-major source order",
          "[geometry][linear-pattern][integration][part-construction-mvp]") {
  GeometryShape first = box(2.0);
  auto second = BodyTransformAdapter{}.translate(first, {0.0, 10.0, 0.0});
  REQUIRE(second);
  auto instances = LinearPatternAdapter{}.generate_instances(
      FeatureId("pattern.order"), {first, second.value()}, {1.0, 0.0, 0.0}, 3U, 5.0);
  REQUIRE(instances);
  REQUIRE(instances.value().size() == 6U);
  const BodyTransformAdapter adapter;
  auto zero_first = adapter.bounds(instances.value().at(0));
  auto zero_second = adapter.bounds(instances.value().at(1));
  auto one_first = adapter.bounds(instances.value().at(2));
  auto two_second = adapter.bounds(instances.value().at(5));
  REQUIRE(zero_first);
  REQUIRE(zero_second);
  REQUIRE(one_first);
  REQUIRE(two_second);
  CHECK(zero_first.value().minimum.x == Catch::Approx(-1.0));
  CHECK(zero_second.value().minimum.y == Catch::Approx(9.0));
  CHECK(one_first.value().minimum.x == Catch::Approx(4.0));
  CHECK(two_second.value().minimum.x == Catch::Approx(9.0));
  CHECK(two_second.value().minimum.y == Catch::Approx(9.0));
}

TEST_CASE("Block 64 executes feature and body source NewBody patterns",
          "[geometry][linear-pattern]") {
  const GeometryRecomputeExecutor executor;
  auto feature_document =
      new_body_pattern(false, LinearPatternExtentMode::Spacing, PatternDirectionSign::Positive);
  auto body_document =
      new_body_pattern(true, LinearPatternExtentMode::Spacing, PatternDirectionSign::Positive);
  ShapeCache feature_cache = shape_cache();
  ShapeCache body_cache = shape_cache();
  REQUIRE(executor.execute_document(feature_document, feature_cache));
  REQUIRE(executor.execute_document(body_document, body_cache));
  CHECK(volume(feature_cache, "body.pattern") == Catch::Approx(4000.0));
  CHECK(volume(body_cache, "body.pattern") == Catch::Approx(4000.0));
  CHECK(bounds(feature_cache, "body.pattern").minimum.x == Catch::Approx(-5.0));
  CHECK(bounds(feature_cache, "body.pattern").maximum.x == Catch::Approx(65.0));
}

TEST_CASE("Block 64 maps total extent and negative direction deterministically",
          "[geometry][linear-pattern]") {
  auto document = new_body_pattern(false, LinearPatternExtentMode::TotalExtent,
                                   PatternDirectionSign::Negative, 4.0, 60.0);
  ShapeCache cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const auto result_bounds = bounds(cache, "body.pattern");
  CHECK(result_bounds.minimum.x == Catch::Approx(-65.0));
  CHECK(result_bounds.maximum.x == Catch::Approx(5.0));
  CHECK(volume(cache, "body.pattern") == Catch::Approx(4000.0));
}

TEST_CASE("Block 64 executes Join Cut and Intersect Body operations",
          "[geometry][linear-pattern]") {
  const GeometryRecomputeExecutor executor;
  auto join_document = modifying_pattern(FeatureBodyOperationMode::Join);
  auto cut_document = modifying_pattern(FeatureBodyOperationMode::Cut);
  auto intersect_document = modifying_pattern(FeatureBodyOperationMode::Intersect);
  ShapeCache join_cache = shape_cache();
  ShapeCache cut_cache = shape_cache();
  ShapeCache intersect_cache = shape_cache();
  REQUIRE(executor.execute_document(join_document, join_cache));
  REQUIRE(executor.execute_document(cut_document, cut_cache));
  REQUIRE(executor.execute_document(intersect_document, intersect_cache));
  CHECK(volume(join_cache, "body.target") == Catch::Approx(5920.0).epsilon(1.0e-8));
  CHECK(volume(cut_cache, "body.target") == Catch::Approx(18080.0).epsilon(1.0e-8));
  CHECK(volume(intersect_cache, "body.target") == Catch::Approx(1920.0).epsilon(1.0e-8));
}

TEST_CASE("Block 64 incrementally recomputes count and spacing transactionally",
          "[geometry][linear-pattern]") {
  auto document =
      new_body_pattern(false, LinearPatternExtentMode::Spacing, PatternDirectionSign::Positive);
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  document.mark_all_clean();
  auto count = Quantity::count(3.0, "pattern.count");
  REQUIRE(count);
  REQUIRE(document.set_parameter_value(ParameterId("pattern.count"), count.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto recomputed_count = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(recomputed_count);
  CHECK(recomputed_count.value().executed_feature_count == 1U);
  CHECK(volume(cache, "body.pattern") == Catch::Approx(3000.0));

  document.mark_all_clean();
  auto spacing = Quantity::length_mm(25.0, "pattern.extent");
  REQUIRE(spacing);
  REQUIRE(document.set_parameter_value(ParameterId("pattern.extent"), spacing.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(executor.execute_plan(document, plan.value(), cache));
  CHECK(bounds(cache, "body.pattern").maximum.x == Catch::Approx(55.0));

  document.mark_all_clean();
  auto invalid_count = Quantity::count(1.0, "pattern.count");
  REQUIRE(invalid_count);
  REQUIRE(document.set_parameter_value(ParameterId("pattern.count"), invalid_count.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto failed = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(failed.has_error());
  CHECK(failed.error().object_id() == "pattern.linear");
  CHECK(volume(cache, "body.pattern") == Catch::Approx(3000.0));
  CHECK(bounds(cache, "body.pattern").maximum.x == Catch::Approx(55.0));
}
