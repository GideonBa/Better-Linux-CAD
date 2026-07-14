#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/fillet_adapter.hpp"
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

PartDocument box_document(double radius = 2.0) {
  auto document = PartDocument::create(DocumentId("part.fillet"), "Fillet");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("fillet.radius", radius)));
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
  auto reference = EdgeReference::create_linear(PartFeatureInputRole::FilletEdge, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

EdgeReference circular_edge(SemanticCircularEdge semantic_edge) {
  auto semantic = SemanticCircularEdgeReference::create(FeatureId("feature.hole"),
                                                        ProfileId("profile.hole"), semantic_edge);
  REQUIRE(semantic);
  auto reference =
      EdgeReference::create_circular(PartFeatureInputRole::FilletEdge, semantic.value());
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

void add_fillet(PartDocument& document, std::vector<EdgeReference> edges) {
  auto fillet = FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.base"),
                                      std::move(edges), ParameterId("fillet.radius"));
  REQUIRE(fillet);
  REQUIRE(document.add_fillet_feature(fillet.value()));
}

ShapeCache shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.fillet"));
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

TEST_CASE("Block 69 executes single and ordered multi-edge constant-radius fillets",
          "[geometry][fillet-feature][integration][part-construction-mvp]") {
  auto single = box_document();
  add_fillet(single, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache single_cache = shape_cache();
  const auto single_result = GeometryRecomputeExecutor{}.execute_document(single, single_cache);
  INFO((single_result.has_error()
            ? single_result.error().object_id() + ": " + single_result.error().message()
            : "success"));
  REQUIRE(single_result);

  auto multiple = box_document();
  add_fillet(multiple, {edge(FeatureId("feature.base"), SemanticEdge::TopFront),
                        edge(FeatureId("feature.base"), SemanticEdge::TopBack)});
  ShapeCache multiple_cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(multiple, multiple_cache));

  CHECK(volume(single_cache) < 1920.0);
  CHECK(volume(multiple_cache) < volume(single_cache));
  CHECK(bounds(multiple_cache).minimum.x == Catch::Approx(-10.0));
  CHECK(bounds(multiple_cache).maximum.z == Catch::Approx(8.0));
}

TEST_CASE("Block 69 recomputes radius and recovers an edge after upstream dimension edits",
          "[geometry][fillet-feature]") {
  auto document = box_document(1.0);
  add_fillet(document, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const double small_radius_volume = volume(cache);

  document.mark_all_clean();
  auto radius = Quantity::length_mm(2.0, "fillet.radius");
  REQUIRE(radius);
  REQUIRE(document.set_parameter_value(ParameterId("fillet.radius"), radius.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto radius_result = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(radius_result);
  CHECK(radius_result.value().executed_feature_count == 1U);
  CHECK(volume(cache) < small_radius_volume);

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

TEST_CASE("Block 69 resolves a semantic circular hole rim", "[geometry][fillet-feature]") {
  auto document = box_document(0.5);
  add_hole(document);
  add_fillet(document, {circular_edge(SemanticCircularEdge::SourceRim)});
  ShapeCache cache = shape_cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(cache.find_feature_shape(FeatureId("feature.fillet")) != nullptr);
  CHECK(bounds(cache).minimum.z == Catch::Approx(0.0).margin(1.0e-6));
  CHECK(bounds(cache).maximum.z == Catch::Approx(8.0).margin(1.0e-6));
}

TEST_CASE("Block 69 rejects broken semantic edges without changing cached geometry",
          "[geometry][fillet-feature][integration][part-construction-mvp]") {
  auto document = box_document();
  ShapeCache cache = shape_cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  const double original_volume = volume(cache);
  auto invalid = FilletFeature::create(
      FeatureId("feature.invalid-fillet"), "Invalid", BodyId("body.base"),
      {edge(FeatureId("feature.missing"), SemanticEdge::TopFront)}, ParameterId("fillet.radius"));
  REQUIRE(invalid);
  const GeometryShape* target = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(target != nullptr);
  const auto result = FilletAdapter{}.apply(document, invalid.value(), *target, cache, 2.0);
  REQUIRE(result.has_error());
  CHECK(result.error().object_id() == "feature.missing");
  CHECK(volume(cache) == Catch::Approx(original_volume));
}

TEST_CASE("Block 69 fails an excessive radius transactionally", "[geometry][fillet-feature]") {
  auto document = box_document(1.0);
  add_fillet(document, {edge(FeatureId("feature.base"), SemanticEdge::TopFront)});
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const double valid_volume = volume(cache);

  document.mark_all_clean();
  auto radius = Quantity::length_mm(20.0, "fillet.radius");
  REQUIRE(radius);
  REQUIRE(document.set_parameter_value(ParameterId("fillet.radius"), radius.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  const auto result = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(result.has_error());
  CHECK(result.error().object_id() == "feature.fillet");
  CHECK(volume(cache) == Catch::Approx(valid_volume));
}
