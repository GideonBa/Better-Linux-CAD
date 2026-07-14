#include "blcad/geometry/circular_pattern_adapter.hpp"
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

AxisReference datum_pattern_axis() {
  auto value =
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.z"));
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
  auto value = ShapeCache::create(ShapeCacheId("cache.circular_pattern"));
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

PartDocument new_body_pattern(bool use_body_source, double total_angle = 360.0,
                              double count = 4.0) {
  auto document = PartDocument::create(DocumentId("part.circular_pattern"), "CircularPattern");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", count)));
  add_box_feature(document.value(), "source", "body.source", 4.0, 4.0, 10.0, {20.0, 0.0});
  REQUIRE(document.value().add_body(body("body.pattern")));
  const auto source =
      use_body_source ? body_source("body.source") : feature_source("feature.source");
  auto pattern = CircularPatternFeature::create(
      FeatureId("pattern.circular"), "Circular", {source}, datum_pattern_axis(),
      ParameterId("pattern.count"), total_angle, true, new_body_context("body.pattern"));
  REQUIRE(pattern);
  REQUIRE(document.value().add_circular_pattern_feature(pattern.value()));
  return document.value();
}

PartDocument modifying_pattern(FeatureBodyOperationMode mode) {
  auto document = PartDocument::create(DocumentId("part.circular_pattern.operation"), "Operation");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", 4.0)));
  const double target_size = mode == FeatureBodyOperationMode::Join ? 20.0 : 30.0;
  const double source_center = mode == FeatureBodyOperationMode::Join ? 11.0 : 12.0;
  const double source_size = mode == FeatureBodyOperationMode::Join ? 4.0 : 6.0;
  add_box_feature(document.value(), "target", "body.target", target_size, target_size, 10.0);
  add_box_feature(document.value(), "tool", "body.tool", source_size, source_size, 10.0,
                  {source_center, 0.0});
  auto pattern = CircularPatternFeature::create(
      FeatureId("pattern.circular"), "Circular", {body_source("body.tool")}, datum_pattern_axis(),
      ParameterId("pattern.count"), 360.0, true, modifying_context(mode, "body.target"));
  REQUIRE(pattern);
  REQUIRE(document.value().add_circular_pattern_feature(pattern.value()));
  return document.value();
}

PartDocument semantic_axis_pattern() {
  auto document = PartDocument::create(DocumentId("part.circular_pattern.semantic"), "Semantic");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_parameter(count_parameter("pattern.count", 4.0)));
  add_box_feature(document.value(), "base", "body.base", 50.0, 50.0, 10.0);

  REQUIRE(document.value().add_parameter(length_parameter("hole.diameter", 6.0)));
  auto hole_sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  auto hole =
      CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"), {5.0, 0.0});
  REQUIRE(hole);
  REQUIRE(hole_sketch.value().add_profile(hole.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));
  auto cut = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base"));
  REQUIRE(cut);
  auto cut_context = cut.value().with_body_result_context(
      modifying_context(FeatureBodyOperationMode::Cut, "body.base"));
  REQUIRE(cut_context);
  REQUIRE(document.value().add_feature(cut_context.value()));

  add_box_feature(document.value(), "source", "body.source", 2.0, 2.0, 4.0, {15.0, 0.0});
  REQUIRE(document.value().add_body(body("body.pattern")));
  auto semantic = SemanticAxisReference::create(FeatureId("feature.hole"));
  REQUIRE(semantic);
  auto axis =
      AxisReference::create_semantic_axis(PartFeatureInputRole::PatternAxis, semantic.value());
  REQUIRE(axis);
  auto pattern = CircularPatternFeature::create(
      FeatureId("pattern.circular"), "Circular", {feature_source("feature.source")}, axis.value(),
      ParameterId("pattern.count"), 360.0, true, new_body_context("body.pattern"));
  REQUIRE(pattern);
  REQUIRE(document.value().add_circular_pattern_feature(pattern.value()));
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

TEST_CASE("Block 65 adapter preserves full and partial instance-major angular order",
          "[geometry][circular-pattern]") {
  auto first = BodyTransformAdapter{}.translate(box(2.0), {10.0, 0.0, 0.0});
  auto second = BodyTransformAdapter{}.translate(box(2.0), {20.0, 0.0, 0.0});
  REQUIRE(first);
  REQUIRE(second);
  const CircularPatternAdapter adapter;
  auto full = adapter.generate_instances(FeatureId("pattern.full"), {first.value()}, {},
                                         {0.0, 0.0, 1.0}, 4U, 360.0);
  auto partial =
      adapter.generate_instances(FeatureId("pattern.partial"), {first.value(), second.value()}, {},
                                 {0.0, 0.0, 1.0}, 3U, 180.0);
  REQUIRE(full);
  REQUIRE(partial);
  REQUIRE(full.value().size() == 4U);
  REQUIRE(partial.value().size() == 6U);
  auto full_last = BodyTransformAdapter{}.bounds(full.value().at(3));
  auto partial_middle_first = BodyTransformAdapter{}.bounds(partial.value().at(2));
  auto partial_last_second = BodyTransformAdapter{}.bounds(partial.value().at(5));
  REQUIRE(full_last);
  REQUIRE(partial_middle_first);
  REQUIRE(partial_last_second);
  CHECK(full_last.value().minimum.y == Catch::Approx(-11.0));
  CHECK(partial_middle_first.value().minimum.y == Catch::Approx(9.0));
  CHECK(partial_last_second.value().maximum.x == Catch::Approx(-19.0));
}

TEST_CASE("Block 65 executes feature and body source full and partial patterns",
          "[geometry][circular-pattern]") {
  const GeometryRecomputeExecutor executor;
  auto feature_document = new_body_pattern(false);
  auto body_document = new_body_pattern(true, 180.0, 3.0);
  ShapeCache feature_cache = shape_cache();
  ShapeCache body_cache = shape_cache();
  REQUIRE(executor.execute_document(feature_document, feature_cache));
  REQUIRE(executor.execute_document(body_document, body_cache));
  CHECK(volume(feature_cache, "body.pattern") == Catch::Approx(640.0));
  CHECK(volume(body_cache, "body.pattern") == Catch::Approx(480.0));
  CHECK(bounds(feature_cache, "body.pattern").minimum.x == Catch::Approx(-22.0));
  CHECK(bounds(feature_cache, "body.pattern").maximum.y == Catch::Approx(22.0));
  CHECK(bounds(body_cache, "body.pattern").minimum.x == Catch::Approx(-22.0));
}

TEST_CASE("Block 65 resolves an offset semantic axis for Circular Pattern",
          "[geometry][circular-pattern]") {
  auto document = semantic_axis_pattern();
  ShapeCache cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  CHECK(volume(cache, "body.pattern") == Catch::Approx(64.0));
  const auto result_bounds = bounds(cache, "body.pattern");
  CHECK(result_bounds.minimum.x == Catch::Approx(-6.0));
  CHECK(result_bounds.maximum.x == Catch::Approx(16.0));
  CHECK(result_bounds.minimum.y == Catch::Approx(-11.0));
  CHECK(result_bounds.maximum.y == Catch::Approx(11.0));
}

TEST_CASE("Block 65 executes Join Cut and Intersect Body operations",
          "[geometry][circular-pattern]") {
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
  CHECK(volume(join_cache, "body.target") == Catch::Approx(4480.0).epsilon(1.0e-8));
  CHECK(volume(cut_cache, "body.target") == Catch::Approx(7560.0).epsilon(1.0e-8));
  CHECK(volume(intersect_cache, "body.target") == Catch::Approx(1440.0).epsilon(1.0e-8));
}

TEST_CASE("Block 65 incrementally recomputes Count transactionally",
          "[geometry][circular-pattern]") {
  auto document = new_body_pattern(false);
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  document.mark_all_clean();
  auto count = Quantity::count(3.0, "pattern.count");
  REQUIRE(count);
  REQUIRE(document.set_parameter_value(ParameterId("pattern.count"), count.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto recomputed = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(recomputed);
  CHECK(recomputed.value().executed_feature_count == 1U);
  CHECK(volume(cache, "body.pattern") == Catch::Approx(480.0));

  document.mark_all_clean();
  auto invalid_count = Quantity::count(1.0, "pattern.count");
  REQUIRE(invalid_count);
  REQUIRE(document.set_parameter_value(ParameterId("pattern.count"), invalid_count.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto failed = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(failed.has_error());
  CHECK(failed.error().object_id() == "pattern.circular");
  CHECK(volume(cache, "body.pattern") == Catch::Approx(480.0));
}
