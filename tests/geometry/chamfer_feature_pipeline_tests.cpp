#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/chamfer_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
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

Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument box_document(double first = 1.0, double second = 2.0, double angle = 45.0) {
  auto document = PartDocument::create(DocumentId("part.chamfer"), "Chamfer");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("chamfer.first", first)));
  REQUIRE(document.value().add_parameter(length_parameter("chamfer.second", second)));
  REQUIRE(document.value().add_parameter(angle_parameter("chamfer.angle", angle)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                            ParameterId("base.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto contextual = base.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.value().add_feature(contextual.value()));
  return document.value();
}

EdgeReference edge(FeatureId source, SemanticEdge semantic_edge) {
  auto semantic = SemanticEdgeReference::create(std::move(source), semantic_edge);
  REQUIRE(semantic);
  auto reference =
      EdgeReference::create_linear(PartFeatureInputRole::ChamferEdge, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

EdgeReference circular_edge(SemanticCircularEdge semantic_edge) {
  auto semantic = SemanticCircularEdgeReference::create(FeatureId("feature.hole"),
                                                        ProfileId("profile.hole"), semantic_edge);
  REQUIRE(semantic);
  auto reference =
      EdgeReference::create_circular(PartFeatureInputRole::ChamferEdge, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

void add_hole(PartDocument& document) {
  REQUIRE(document.add_parameter(length_parameter("hole.diameter", 4.0)));
  auto sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto hole = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base"));
  REQUIRE(hole);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                  BodyId("body.base"), std::nullopt);
  REQUIRE(context);
  auto contextual = hole.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.add_feature(contextual.value()));
}

void add_equal(PartDocument& document, std::vector<EdgeReference> edges) {
  auto chamfer = ChamferFeature::create_equal_distance(FeatureId("feature.chamfer"), "Chamfer",
                                                       BodyId("body.base"), std::move(edges),
                                                       ParameterId("chamfer.first"));
  REQUIRE(chamfer);
  REQUIRE(document.add_chamfer_feature(chamfer.value()));
}

void add_two_distance(PartDocument& document, std::vector<EdgeReference> edges) {
  auto chamfer = ChamferFeature::create_two_distance(
      FeatureId("feature.chamfer"), "Chamfer", BodyId("body.base"), std::move(edges),
      ParameterId("chamfer.first"), ParameterId("chamfer.second"));
  REQUIRE(chamfer);
  REQUIRE(document.add_chamfer_feature(chamfer.value()));
}

void add_distance_angle(PartDocument& document, std::vector<EdgeReference> edges) {
  auto chamfer = ChamferFeature::create_distance_angle(
      FeatureId("feature.chamfer"), "Chamfer", BodyId("body.base"), std::move(edges),
      ParameterId("chamfer.first"), ParameterId("chamfer.angle"));
  REQUIRE(chamfer);
  REQUIRE(document.add_chamfer_feature(chamfer.value()));
}

ShapeCache shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.chamfer"));
  REQUIRE(cache);
  return cache.value();
}

double volume(const ShapeCache& cache) {
  const GeometryShape* shape = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

GeometryShapeBounds bounds(const ShapeCache& cache) {
  const GeometryShape* shape = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}

} // namespace

TEST_CASE("Block 70 executes single and ordered multi-edge equal-distance chamfers",
          "[geometry][chamfer-feature][integration][part-construction-mvp]") {
  auto single = box_document();
  add_equal(single, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache single_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(single, single_cache));

  auto multiple = box_document();
  add_equal(multiple, {edge(FeatureId("feature.base"), SemanticEdge::TopFront),
                       edge(FeatureId("feature.base"), SemanticEdge::TopBack)});
  ShapeCache multiple_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(multiple, multiple_cache));
  CHECK(volume(single_cache) < 1920.0);
  CHECK(volume(multiple_cache) < volume(single_cache));
  CHECK(bounds(multiple_cache).minimum.x == Catch::Approx(-10.0));
}

TEST_CASE("Block 70 executes deterministic two-distance and distance-angle chamfers",
          "[geometry][chamfer-feature]") {
  auto two_distance = box_document(1.0, 2.0, 45.0);
  add_two_distance(two_distance, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache two_distance_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(two_distance, two_distance_cache));

  auto distance_angle = box_document(1.0, 2.0, 30.0);
  add_distance_angle(distance_angle, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache distance_angle_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(distance_angle, distance_angle_cache));
  CHECK(volume(two_distance_cache) == Catch::Approx(1900.0).epsilon(1.0e-8));
  CHECK(volume(distance_angle_cache) ==
        Catch::Approx(1920.0 - 10.0 * std::tan(30.0 * 3.14159265358979323846 / 180.0))
            .epsilon(1.0e-8));
  CHECK(volume(two_distance_cache) != Catch::Approx(volume(distance_angle_cache)));
}

TEST_CASE("Block 70 resolves a semantic circular hole rim", "[geometry][chamfer-feature]") {
  auto document = box_document(0.5);
  add_hole(document);
  add_distance_angle(document, {circular_edge(SemanticCircularEdge::SourceRim)});
  ShapeCache cache = shape_cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(cache.find_feature_shape(FeatureId("feature.chamfer")) != nullptr);
  CHECK(bounds(cache).minimum.z == Catch::Approx(0.0).margin(1.0e-6));
  CHECK(bounds(cache).maximum.z == Catch::Approx(8.0).margin(1.0e-6));
}

TEST_CASE("Block 70 recomputes parameters and recovers edges after upstream edits",
          "[geometry][chamfer-feature]") {
  auto document = box_document(0.5);
  add_equal(document, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const double initial_volume = volume(cache);

  document.mark_all_clean();
  auto distance = Quantity::length_mm(1.5, "chamfer.first");
  REQUIRE(distance);
  REQUIRE(document.set_parameter_value(ParameterId("chamfer.first"), distance.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto distance_result = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(distance_result);
  CHECK(distance_result.value().executed_feature_count == 1U);
  CHECK(volume(cache) < initial_volume);

  document.mark_all_clean();
  auto width = Quantity::length_mm(30.0, "base.width");
  REQUIRE(width);
  REQUIRE(document.set_parameter_value(ParameterId("base.width"), width.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto upstream_result = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(upstream_result);
  CHECK(upstream_result.value().executed_feature_count == 2U);
  CHECK(bounds(cache).minimum.x == Catch::Approx(-15.0));
  CHECK(bounds(cache).maximum.x == Catch::Approx(15.0));
}

TEST_CASE("Block 70 preserves cached geometry on broken edges and excessive distances",
          "[geometry][chamfer-feature]") {
  auto direct_document = box_document();
  ShapeCache direct_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(direct_document, direct_cache));
  const double original_volume = volume(direct_cache);
  auto invalid = ChamferFeature::create_equal_distance(
      FeatureId("feature.invalid-chamfer"), "Invalid", BodyId("body.base"),
      {edge(FeatureId("feature.missing"), SemanticEdge::TopFront)}, ParameterId("chamfer.first"));
  REQUIRE(invalid);
  const GeometryShape* target = direct_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(target != nullptr);
  const auto broken = ChamferAdapter{}.apply(direct_document, invalid.value(), *target,
                                             direct_cache, 1.0, std::nullopt);
  REQUIRE(broken.has_error());
  CHECK(broken.error().object_id() == "feature.missing");
  CHECK(volume(direct_cache) == Catch::Approx(original_volume));

  auto document = box_document(1.0);
  add_equal(document, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const double valid_volume = volume(cache);
  document.mark_all_clean();
  auto excessive = Quantity::length_mm(20.0, "chamfer.first");
  REQUIRE(excessive);
  REQUIRE(document.set_parameter_value(ParameterId("chamfer.first"), excessive.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto result = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(result.has_error());
  CHECK(result.error().object_id() == "feature.chamfer");
  CHECK(volume(cache) == Catch::Approx(valid_volume));
}
