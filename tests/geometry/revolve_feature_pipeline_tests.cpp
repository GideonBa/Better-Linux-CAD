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

Body solid_body(const char* id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

FeatureBodyResultContext context(FeatureBodyOperationMode mode, const char* body_id) {
  auto value = FeatureBodyResultContext::create(
      mode,
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{}
                                                : std::optional<BodyId>{BodyId(body_id)},
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId(body_id)}
                                                : std::optional<BodyId>{});
  REQUIRE(value);
  return value.value();
}

ProfileRegionReference revolve_profile(const char* sketch, const char* profile) {
  auto value = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                              PartFeatureInputRole::RevolveProfile);
  REQUIRE(value);
  return value.value();
}

AxisReference construction_axis(const char* id) {
  auto value = AxisReference::create_construction_line(
      PartFeatureInputRole::RevolveAxis, ConstructionLineId(id), PartFeatureInputCapability::Axis);
  REQUIRE(value);
  return value.value();
}

ShapeCache shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("cache.revolve"));
  REQUIRE(cache);
  return cache.value();
}

double body_volume(const ShapeCache& cache, const char* body_id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

PartDocument new_body_revolve(RevolveAngleExtent extent, double center_x = 15.0) {
  auto document = PartDocument::create(DocumentId("part.revolve.new"), "RevolveNew");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("profile.width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("profile.height", 20.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.revolve"), "Revolve", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle =
      RectangleProfile::create(ProfileId("profile.revolve"), ParameterId("profile.width"),
                               ParameterId("profile.height"), {center_x, 0.0});
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto axis =
      ConstructionLine::create_explicit(ConstructionLineId("axis.y"), "Y", {}, {0.0, 1.0, 0.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_construction_line(axis.value()));
  REQUIRE(document.value().add_body(solid_body("body.revolve")));
  auto feature = RevolveFeature::create_revolve(
      FeatureId("revolve.main"), "Main", revolve_profile("sketch.revolve", "profile.revolve"),
      construction_axis("axis.y"), std::move(extent),
      context(FeatureBodyOperationMode::NewBody, "body.revolve"));
  REQUIRE(feature);
  REQUIRE(document.value().add_revolve_feature(feature.value()));
  return document.value();
}

PartDocument modifying_revolve(FeatureBodyOperationMode mode) {
  auto document = PartDocument::create(DocumentId("part.revolve.modify"), "RevolveModify");
  REQUIRE(document);
  for (const auto& [id, value] : {std::pair{"base.width", 50.0},
                                  {"base.height", 20.0},
                                  {"base.depth", 30.0},
                                  {"revolve.width", 10.0},
                                  {"revolve.height", 20.0}})
    REQUIRE(document.value().add_parameter(length_parameter(id, value)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                               ParameterId("base.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto revolve_sketch =
      Sketch::create(SketchId("sketch.revolve"), "Revolve", DatumPlaneId("datum.xy"));
  REQUIRE(revolve_sketch);
  auto revolved_profile =
      RectangleProfile::create(ProfileId("profile.revolve"), ParameterId("revolve.width"),
                               ParameterId("revolve.height"), {15.0, 0.0});
  REQUIRE(revolved_profile);
  REQUIRE(revolve_sketch.value().add_profile(revolved_profile.value()));
  REQUIRE(document.value().add_sketch(revolve_sketch.value()));
  auto axis =
      ConstructionLine::create_explicit(ConstructionLineId("axis.y"), "Y", {}, {0.0, 1.0, 0.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_construction_line(axis.value()));
  REQUIRE(document.value().add_body(solid_body("body.base")));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto base_with_context = base.value().with_body_result_context(
      context(FeatureBodyOperationMode::NewBody, "body.base"));
  REQUIRE(base_with_context);
  REQUIRE(document.value().add_feature(base_with_context.value()));

  Result<RevolveFeature> revolve =
      mode == FeatureBodyOperationMode::Cut
          ? RevolveFeature::create_revolve_cut(
                FeatureId("revolve.operation"), "Operation",
                revolve_profile("sketch.revolve", "profile.revolve"), construction_axis("axis.y"),
                RevolveAngleExtent::full(), context(mode, "body.base"))
          : RevolveFeature::create_revolve(FeatureId("revolve.operation"), "Operation",
                                           revolve_profile("sketch.revolve", "profile.revolve"),
                                           construction_axis("axis.y"), RevolveAngleExtent::full(),
                                           context(mode, "body.base"));
  REQUIRE(revolve);
  REQUIRE(document.value().add_revolve_feature(revolve.value()));
  return document.value();
}

PartDocument semantic_axis_document() {
  auto document = PartDocument::create(DocumentId("part.revolve.semantic"), "SemanticAxis");
  REQUIRE(document);
  for (const auto& [id, value] : {std::pair{"base.width", 50.0},
                                  {"base.height", 50.0},
                                  {"base.depth", 10.0},
                                  {"hole.diameter", 8.0},
                                  {"revolve.width", 8.0},
                                  {"revolve.height", 16.0}})
    REQUIRE(document.value().add_parameter(length_parameter(id, value)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto xz = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.xz"), "XZ", {},
                                               {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0});
  REQUIRE(xz);
  REQUIRE(document.value().add_construction_plane(xz.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  auto hole_sketch = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  auto revolve_sketch =
      Sketch::create(SketchId("sketch.revolve"), "Revolve", DatumPlaneId("plane.xz"));
  REQUIRE(base_sketch);
  REQUIRE(hole_sketch);
  REQUIRE(revolve_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                               ParameterId("base.height"));
  auto hole_profile =
      CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"));
  auto revolved_profile =
      RectangleProfile::create(ProfileId("profile.revolve"), ParameterId("revolve.width"),
                               ParameterId("revolve.height"), {14.0, 0.0});
  REQUIRE(base_profile);
  REQUIRE(hole_profile);
  REQUIRE(revolved_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(hole_sketch.value().add_profile(hole_profile.value()));
  REQUIRE(revolve_sketch.value().add_profile(revolved_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  REQUIRE(document.value().add_sketch(hole_sketch.value()));
  REQUIRE(document.value().add_sketch(revolve_sketch.value()));
  REQUIRE(document.value().add_body(solid_body("body.base")));
  REQUIRE(document.value().add_body(solid_body("body.revolve")));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto base_context = base.value().with_body_result_context(
      context(FeatureBodyOperationMode::NewBody, "body.base"));
  REQUIRE(base_context);
  REQUIRE(document.value().add_feature(base_context.value()));
  auto hole = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base"));
  REQUIRE(hole);
  auto hole_context =
      hole.value().with_body_result_context(context(FeatureBodyOperationMode::Cut, "body.base"));
  REQUIRE(hole_context);
  REQUIRE(document.value().add_feature(hole_context.value()));

  auto semantic = SemanticAxisReference::create(FeatureId("feature.hole"));
  REQUIRE(semantic);
  auto axis =
      AxisReference::create_semantic_axis(PartFeatureInputRole::RevolveAxis, semantic.value());
  REQUIRE(axis);
  auto revolve = RevolveFeature::create_revolve(
      FeatureId("revolve.semantic"), "Semantic",
      revolve_profile("sketch.revolve", "profile.revolve"), axis.value(),
      RevolveAngleExtent::full(), context(FeatureBodyOperationMode::NewBody, "body.revolve"));
  REQUIRE(revolve);
  REQUIRE(document.value().add_revolve_feature(revolve.value()));
  return document.value();
}

} // namespace

TEST_CASE("Block 62 executes full partial and symmetric solid revolutions",
          "[geometry][revolve-feature]") {
  auto full_document = new_body_revolve(RevolveAngleExtent::full());
  auto partial_extent = RevolveAngleExtent::angle(180.0, RevolveSide::Negative);
  auto symmetric_extent = RevolveAngleExtent::symmetric(120.0);
  REQUIRE(partial_extent);
  REQUIRE(symmetric_extent);
  auto partial_document = new_body_revolve(partial_extent.value());
  auto symmetric_document = new_body_revolve(symmetric_extent.value());
  const GeometryRecomputeExecutor executor;
  ShapeCache full_cache = shape_cache();
  ShapeCache partial_cache = shape_cache();
  ShapeCache symmetric_cache = shape_cache();
  REQUIRE(executor.execute_document(full_document, full_cache));
  REQUIRE(executor.execute_document(partial_document, partial_cache));
  REQUIRE(executor.execute_document(symmetric_document, symmetric_cache));

  const double full = body_volume(full_cache, "body.revolve");
  const double partial = body_volume(partial_cache, "body.revolve");
  const double symmetric = body_volume(symmetric_cache, "body.revolve");
  CHECK(full == Catch::Approx(6000.0 * 3.14159265358979323846).epsilon(1.0e-6));
  CHECK(partial == Catch::Approx(full / 2.0).epsilon(1.0e-6));
  CHECK(symmetric == Catch::Approx(full / 3.0).epsilon(1.0e-6));
}

TEST_CASE("Block 62 applies Join Cut and Intersect including a revolved groove",
          "[geometry][revolve-feature]") {
  const GeometryRecomputeExecutor executor;
  auto join_document = modifying_revolve(FeatureBodyOperationMode::Join);
  auto cut_document = modifying_revolve(FeatureBodyOperationMode::Cut);
  auto intersect_document = modifying_revolve(FeatureBodyOperationMode::Intersect);
  ShapeCache join_cache = shape_cache();
  ShapeCache cut_cache = shape_cache();
  ShapeCache intersect_cache = shape_cache();
  REQUIRE(executor.execute_document(join_document, join_cache));
  REQUIRE(executor.execute_document(cut_document, cut_cache));
  REQUIRE(executor.execute_document(intersect_document, intersect_cache));

  const double base_volume = 50.0 * 20.0 * 30.0;
  CHECK(body_volume(join_cache, "body.base") > base_volume);
  CHECK(body_volume(cut_cache, "body.base") < base_volume);
  CHECK(body_volume(cut_cache, "body.base") > 0.0);
  CHECK(body_volume(intersect_cache, "body.base") > 0.0);
  CHECK(body_volume(intersect_cache, "body.base") < base_volume);
  CHECK(cut_cache.find_feature_shape(FeatureId("revolve.operation")) != nullptr);
}

TEST_CASE("Block 62 semantic axis dependency invalidates and recomputes Revolve",
          "[geometry][revolve-feature]") {
  auto document = semantic_axis_document();
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, cache));
  const double before = body_volume(cache, "body.revolve");
  document.mark_all_clean();
  auto changed = Quantity::length_mm(10.0, "hole.diameter");
  REQUIRE(changed);
  REQUIRE(document.set_parameter_value(ParameterId("hole.diameter"), changed.value()));
  const auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("feature.hole"));
  CHECK(plan.value().contains("revolve.semantic"));
  const auto summary = executor.execute_plan(document, plan.value(), cache);
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  CHECK(body_volume(cache, "body.revolve") == Catch::Approx(before).epsilon(1.0e-8));
}

TEST_CASE("Block 62 rejects a self-intersecting axis-crossing profile transactionally",
          "[geometry][revolve-feature]") {
  auto valid_document = new_body_revolve(RevolveAngleExtent::full(), 15.0);
  auto invalid_document = new_body_revolve(RevolveAngleExtent::full(), 0.0);
  ShapeCache cache = shape_cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(valid_document, cache));
  const double before = body_volume(cache, "body.revolve");
  const auto failed = executor.execute_document(invalid_document, cache);
  REQUIRE(failed.has_error());
  CHECK(failed.error().object_id() == "revolve.main");
  CHECK(failed.error().message() == "revolve profile crosses its axis and would self-intersect");
  CHECK(body_volume(cache, "body.revolve") == Catch::Approx(before));
}
