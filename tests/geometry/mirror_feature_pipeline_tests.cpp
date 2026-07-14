#include "blcad/geometry/mirror_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter length_parameter(const std::string& id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id) {
  auto value = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context(const char* id) {
  auto value =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext modifying_context(FeatureBodyOperationMode mode, const char* id) {
  auto value = FeatureBodyResultContext::create(mode, BodyId(id), std::nullopt);
  REQUIRE(value);
  return value.value();
}

MirrorSourceReference feature_source(const char* id) {
  auto value = MirrorSourceReference::feature(FeatureId(id));
  REQUIRE(value);
  return value.value();
}

MirrorSourceReference body_source(const char* id) {
  auto value = MirrorSourceReference::body(BodyId(id));
  REQUIRE(value);
  return value.value();
}

PlaneReference construction_plane(const char* id) {
  auto value = PlaneReference::create_construction_plane(PartFeatureInputRole::MirrorPlane,
                                                         ConstructionPlaneId(id));
  REQUIRE(value);
  return value.value();
}

PlaneReference semantic_top(const char* id) {
  auto face = SemanticFaceReference::create(FeatureId(id), SemanticFace::Top);
  REQUIRE(face);
  auto value =
      PlaneReference::create_semantic_face(PartFeatureInputRole::MirrorPlane, face.value());
  REQUIRE(value);
  return value.value();
}

void add_box(PartDocument& document, const char* prefix, const char* body_id, double width,
             double height, double depth, Point2 center = {}) {
  const std::string p(prefix);
  REQUIRE(document.add_parameter(length_parameter(p + ".width", width)));
  REQUIRE(document.add_parameter(length_parameter(p + ".height", height)));
  REQUIRE(document.add_parameter(length_parameter(p + ".depth", depth)));
  auto sketch = Sketch::create(SketchId("sketch." + p), p, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto profile = RectangleProfile::create(ProfileId("profile." + p), ParameterId(p + ".width"),
                                          ParameterId(p + ".height"), center);
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  REQUIRE(document.add_body(body(body_id)));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature." + p), p, SketchId("sketch." + p), ParameterId(p + ".depth"));
  REQUIRE(feature);
  auto contextual = feature.value().with_body_result_context(new_body_context(body_id));
  REQUIRE(contextual);
  REQUIRE(document.add_feature(contextual.value()));
}

PartDocument base_document(const char* id) {
  auto document = PartDocument::create(DocumentId(id), id);
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  return document.value();
}

void add_yz_mirror_plane(PartDocument& document) {
  auto plane =
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane.mirror"), "Mirror", {},
                                         {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0});
  REQUIRE(plane);
  REQUIRE(document.add_construction_plane(plane.value()));
}

ShapeCache cache() {
  auto value = ShapeCache::create(ShapeCacheId("cache.mirror"));
  REQUIRE(value);
  return value.value();
}

double volume(const ShapeCache& value, const char* body_id) {
  const GeometryShape* shape = value.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

GeometryShapeBounds bounds(const ShapeCache& value, const char* body_id) {
  const GeometryShape* shape = value.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}

void add_mirror(PartDocument& document, std::vector<MirrorSourceReference> sources,
                PlaneReference plane, FeatureBodyResultContext context) {
  auto mirror = MirrorFeature::create(FeatureId("feature.mirror"), "Mirror", std::move(sources),
                                      std::move(plane), std::move(context));
  REQUIRE(mirror);
  REQUIRE(document.add_mirror_feature(mirror.value()));
}

} // namespace

TEST_CASE("Block 67 mirrors feature and body sources deterministically",
          "[geometry][mirror-feature][integration][part-construction-mvp]") {
  auto feature_document = base_document("part.mirror.feature");
  add_yz_mirror_plane(feature_document);
  add_box(feature_document, "source", "body.source", 4.0, 6.0, 10.0, {15.0, 2.0});
  REQUIRE(feature_document.add_body(body("body.mirror")));
  add_mirror(feature_document, {feature_source("feature.source")},
             construction_plane("plane.mirror"), new_body_context("body.mirror"));
  ShapeCache feature_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(feature_document, feature_cache));
  CHECK(volume(feature_cache, "body.mirror") == Catch::Approx(240.0));
  CHECK(bounds(feature_cache, "body.mirror").minimum.x == Catch::Approx(-17.0));

  auto direct = MirrorAdapter{}.reflect_sources(
      FeatureId("direct"),
      {*feature_cache.find_feature_shape(FeatureId("feature.source")),
       *feature_cache.find_feature_shape(FeatureId("feature.source"))},
      {}, {1.0, 0.0, 0.0});
  REQUIRE(direct);
  REQUIRE(direct.value().size() == 2U);
  CHECK(BodyTransformAdapter{}.bounds(direct.value().front()).value().maximum.x ==
        Catch::Approx(-13.0));

  // Rebuild the same intent with a typed Body source.
  auto body_doc = base_document("part.mirror.body");
  add_yz_mirror_plane(body_doc);
  add_box(body_doc, "source", "body.source", 4.0, 6.0, 10.0, {15.0, 2.0});
  REQUIRE(body_doc.add_body(body("body.mirror")));
  add_mirror(body_doc, {body_source("body.source")}, construction_plane("plane.mirror"),
             new_body_context("body.mirror"));
  ShapeCache body_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(body_doc, body_cache));
  CHECK(bounds(body_cache, "body.mirror").maximum.x == Catch::Approx(-13.0));
}

TEST_CASE("Block 67 resolves non-axis-aligned and semantic mirror planes",
          "[geometry][mirror-feature]") {
  auto diagonal = base_document("part.mirror.diagonal");
  const double s = 1.0 / std::sqrt(2.0);
  auto plane = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.diagonal"), "Diagonal",
                                                  {}, {s, -s, 0.0}, {0.0, 0.0, -1.0}, {s, s, 0.0});
  REQUIRE(plane);
  REQUIRE(diagonal.add_construction_plane(plane.value()));
  add_box(diagonal, "source", "body.source", 2.0, 4.0, 6.0, {10.0, 0.0});
  REQUIRE(diagonal.add_body(body("body.mirror")));
  add_mirror(diagonal, {feature_source("feature.source")}, construction_plane("plane.diagonal"),
             new_body_context("body.mirror"));
  ShapeCache diagonal_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(diagonal, diagonal_cache));
  CHECK(bounds(diagonal_cache, "body.mirror").minimum.y == Catch::Approx(-11.0));
  CHECK(bounds(diagonal_cache, "body.mirror").maximum.x == Catch::Approx(2.0));

  auto semantic = base_document("part.mirror.semantic");
  add_box(semantic, "base", "body.base", 20.0, 20.0, 10.0);
  add_box(semantic, "source", "body.source", 4.0, 4.0, 4.0, {15.0, 0.0});
  REQUIRE(semantic.add_body(body("body.mirror")));
  add_mirror(semantic, {feature_source("feature.source")}, semantic_top("feature.base"),
             new_body_context("body.mirror"));
  ShapeCache semantic_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(semantic, semantic_cache));
  CHECK(bounds(semantic_cache, "body.mirror").minimum.z == Catch::Approx(16.0));
  CHECK(bounds(semantic_cache, "body.mirror").maximum.z == Catch::Approx(20.0));
}

TEST_CASE("Block 67 supports Join Cut and Intersect body result modes",
          "[geometry][mirror-feature]") {
  const auto run = [](FeatureBodyOperationMode mode) {
    auto document = base_document("part.mirror.operation");
    add_yz_mirror_plane(document);
    const double target_size = mode == FeatureBodyOperationMode::Join ? 20.0 : 30.0;
    const double source_center = mode == FeatureBodyOperationMode::Join ? 11.0 : 12.0;
    const double source_size = mode == FeatureBodyOperationMode::Join ? 4.0 : 6.0;
    add_box(document, "target", "body.target", target_size, target_size, 10.0);
    add_box(document, "tool", "body.tool", source_size, source_size, 10.0, {source_center, 0.0});
    add_mirror(document, {body_source("body.tool")}, construction_plane("plane.mirror"),
               modifying_context(mode, "body.target"));
    ShapeCache result = cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, result));
    return volume(result, "body.target");
  };
  CHECK(run(FeatureBodyOperationMode::Join) == Catch::Approx(4120.0).epsilon(1.0e-8));
  CHECK(run(FeatureBodyOperationMode::Cut) == Catch::Approx(8640.0).epsilon(1.0e-8));
  CHECK(run(FeatureBodyOperationMode::Intersect) == Catch::Approx(360.0).epsilon(1.0e-8));
}

TEST_CASE("Block 67 recomputes semantic planes and fails transactionally on missing sources",
          "[geometry][mirror-feature][integration][part-construction-mvp]") {
  auto document = base_document("part.mirror.recompute");
  add_box(document, "base", "body.base", 20.0, 20.0, 10.0);
  add_box(document, "source", "body.source", 4.0, 4.0, 4.0, {15.0, 0.0});
  REQUIRE(document.add_body(body("body.mirror")));
  add_mirror(document, {feature_source("feature.source")}, semantic_top("feature.base"),
             new_body_context("body.mirror"));
  ShapeCache value = cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, value));
  CHECK(bounds(value, "body.mirror").maximum.z == Catch::Approx(20.0));
  auto deeper = Quantity::length_mm(15.0, "base.depth");
  REQUIRE(deeper);
  REQUIRE(document.set_parameter_value(ParameterId("base.depth"), deeper.value()));
  REQUIRE(executor.execute_document(document, value));
  CHECK(bounds(value, "body.mirror").minimum.z == Catch::Approx(26.0));

  REQUIRE(value.remove_feature_shape(FeatureId("feature.source")));
  const double preserved_volume = volume(value, "body.mirror");
  auto failed = executor.execute_mirror(document, FeatureId("feature.mirror"), value);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().object_id() == "feature.source");
  CHECK(volume(value, "body.mirror") == Catch::Approx(preserved_volume));
}
