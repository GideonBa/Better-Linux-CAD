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

Body make_solid_body(const char* id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

Feature with_body_context(Feature feature, FeatureBodyOperationMode mode,
                          std::optional<BodyId> target, std::optional<BodyId> produced) {
  auto context = FeatureBodyResultContext::create(mode, std::move(target), std::move(produced));
  REQUIRE(context);
  auto attached = feature.with_body_result_context(context.value());
  REQUIRE(attached);
  return attached.value();
}

PartDocument make_two_body_document() {
  auto document = PartDocument::create(DocumentId("part.two_body"), "TwoBody");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("a.width", "a_width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("a.height", "a_height", 30.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("a.depth", "a_depth", 5.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("b.width", "b_width", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("b.height", "b_height", 10.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("b.depth", "b_depth", 7.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  for (const char prefix : {'a', 'b'}) {
    const std::string p(1, prefix);
    auto sketch = Sketch::create(SketchId("sketch." + p), "Sketch" + p, DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto rectangle = RectangleProfile::create(ProfileId("profile." + p), ParameterId(p + ".width"),
                                              ParameterId(p + ".height"));
    REQUIRE(rectangle);
    REQUIRE(sketch.value().add_profile(rectangle.value()));
    REQUIRE(document.value().add_sketch(sketch.value()));
    REQUIRE(document.value().add_body(make_solid_body(("body." + p).c_str())));
    auto feature =
        Feature::create_additive_extrude(FeatureId("feature." + p), "Extrude" + p,
                                         SketchId("sketch." + p), ParameterId(p + ".depth"));
    REQUIRE(feature);
    REQUIRE(document.value().add_feature(with_body_context(
        feature.value(), FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body." + p))));
  }
  return document.value();
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

TEST_CASE("GeometryRecomputeExecutor accepts Block 59 Extrude intent in Block 60",
          "[geometry][extrude-extent]") {
  auto document = PartDocument::create(DocumentId("part.symmetric"), "Symmetric");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_sketch(make_rectangle_sketch()));
  auto extent = ExtrudeExtentIntent::symmetric(ParameterId("part.thickness"));
  REQUIRE(extent);
  auto intent = ExtrudeFeatureIntent::create(extent.value());
  REQUIRE(intent);
  auto feature = Feature::create_additive_extrude(FeatureId("feature.symmetric"), "Symmetric",
                                                  SketchId("sketch.base"), intent.value());
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  ShapeCache cache = make_shape_cache();
  const auto result = GeometryRecomputeExecutor{}.execute_additive_extrude(
      document.value(), FeatureId("feature.symmetric"), cache);
  REQUIRE(result);
  CHECK(cache.feature_shape_count() == 1U);
  CHECK(cache.body_shape_count() == 0U);
  CHECK(cache.has_final_shape());
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
        "additive extrude requires exactly one rectangle, closed profile, arc closed profile, "
        "composite closed profile, or detected simple region");
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
        "subtractive extrude requires exactly one circle, closed profile, arc closed profile, "
        "composite closed profile, circular hole pattern, or detected simple region");
  CHECK_FALSE(cache.has_final_shape());
}

TEST_CASE("GeometryRecomputeExecutor produces two independent body results",
          "[geometry][multi-body-recompute]") {
  const PartDocument document = make_two_body_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  CHECK(cache.body_shape_count() == 2);
  REQUIRE(cache.find_body_shape(BodyId("body.a")) != nullptr);
  REQUIRE(cache.find_body_shape(BodyId("body.b")) != nullptr);
  CHECK(cache.body_shapes()[0].body_id.value() == "body.a");
  CHECK(cache.body_shapes()[1].body_id.value() == "body.b");
  CHECK_FALSE(cache.has_final_shape());

  const RectangleExtrusionAdapter inspector;
  CHECK(inspector.summarize(*cache.find_body_shape(BodyId("body.a"))).volume_mm3 ==
        Catch::Approx(6000.0));
  CHECK(inspector.summarize(*cache.find_body_shape(BodyId("body.b"))).volume_mm3 ==
        Catch::Approx(1400.0));
}

TEST_CASE("GeometryRecomputeExecutor preserves unaffected body cache entries",
          "[geometry][multi-body-recompute]") {
  auto document = make_two_body_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const CachedBodyShape before_b = *cache.find_body_result(BodyId("body.b"));

  REQUIRE(document.mark_parameter_changed(ParameterId("a.width")));
  const auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(plan.value().contains("feature.a"));
  CHECK_FALSE(plan.value().contains("feature.b"));
  REQUIRE(executor.execute_plan(document, plan.value(), cache));

  const CachedBodyShape* after_b = cache.find_body_result(BodyId("body.b"));
  REQUIRE(after_b != nullptr);
  CHECK(after_b->source_feature_id == before_b.source_feature_id);
  const RectangleExtrusionAdapter inspector;
  CHECK(inspector.summarize(after_b->shape).volume_mm3 ==
        inspector.summarize(before_b.shape).volume_mm3);
}

TEST_CASE("GeometryRecomputeExecutor cuts a target body and fails closed when it is missing",
          "[geometry][multi-body-recompute]") {
  auto document = make_document_with_base_plate_and_hole_cut();
  REQUIRE(document.add_body(make_solid_body("body.base")));

  const Feature base = with_body_context(document.features()[0], FeatureBodyOperationMode::NewBody,
                                         std::nullopt, BodyId("body.base"));
  const Feature cut = with_body_context(document.features()[1], FeatureBodyOperationMode::Cut,
                                        BodyId("body.base"), std::nullopt);

  auto rebuilt = PartDocument::create(DocumentId("part.body_cut"), "BodyCut");
  REQUIRE(rebuilt);
  for (const auto& parameter : document.parameters())
    REQUIRE(rebuilt.value().add_parameter(parameter));
  for (const auto& datum : document.datum_planes())
    REQUIRE(rebuilt.value().add_datum_plane(datum));
  for (const auto& sketch : document.sketches())
    REQUIRE(rebuilt.value().add_sketch(sketch));
  REQUIRE(rebuilt.value().add_body(make_solid_body("body.base")));
  REQUIRE(rebuilt.value().add_feature(base));
  REQUIRE(rebuilt.value().add_feature(cut));

  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(
      executor.execute_additive_extrude(rebuilt.value(), FeatureId("feature.base_extrude"), cache));
  const RectangleExtrusionAdapter inspector;
  const double base_volume =
      inspector.summarize(*cache.find_body_shape(BodyId("body.base"))).volume_mm3;
  REQUIRE(executor.execute_subtractive_extrude(rebuilt.value(),
                                               FeatureId("feature.center_hole_cut"), cache));
  REQUIRE(cache.find_body_result(BodyId("body.base")) != nullptr);
  CHECK(cache.find_body_result(BodyId("body.base"))->source_feature_id.value() ==
        "feature.center_hole_cut");
  CHECK(inspector.summarize(*cache.find_body_shape(BodyId("body.base"))).volume_mm3 < base_volume);
  CHECK(cache.has_final_shape());
  REQUIRE(cache.remove_body_shape(BodyId("body.base")));
  const auto failed = executor.execute_subtractive_extrude(
      rebuilt.value(), FeatureId("feature.center_hole_cut"), cache);
  REQUIRE(failed.has_error());
  CHECK(failed.error().object_id() == "body.base");
  CHECK(failed.error().message() == "target body shape must exist in the shape cache");

  REQUIRE(rebuilt.value().mark_parameter_changed(ParameterId("part.hole_diameter")));
  const auto plan = rebuilt.value().create_recompute_plan();
  REQUIRE(plan);
  const auto failed_plan = executor.execute_plan(rebuilt.value(), plan.value(), cache);
  REQUIRE(failed_plan.has_error());
  CHECK(cache.find_feature_shape(FeatureId("feature.center_hole_cut")) != nullptr);
}
