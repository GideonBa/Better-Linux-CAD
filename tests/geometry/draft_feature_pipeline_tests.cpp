#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/draft_adapter.hpp"
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

Parameter angle_parameter(double value) {
  auto quantity = Quantity::angle_deg(value, "draft.angle");
  REQUIRE(quantity);
  auto parameter =
      Parameter::create_angle(ParameterId("draft.angle"), "Draft angle", quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument box_document(double angle = 5.0) {
  auto document = PartDocument::create(DocumentId("part.draft-geometry"), "Draft geometry");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.height", 12.0)));
  REQUIRE(document.value().add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.value().add_parameter(angle_parameter(angle)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto pull = DatumAxis::create_explicit(DatumAxisId("axis.pull"), "Pull", {}, {0.0, 0.0, 1.0});
  REQUIRE(pull);
  REQUIRE(document.value().add_datum_axis(pull.value()));
  auto tilted = ConstructionLine::create_explicit(ConstructionLineId("line.tilted"), "Tilted", {},
                                                  {0.0, 0.6, 0.8});
  REQUIRE(tilted);
  REQUIRE(document.value().add_construction_line(tilted.value()));
  auto offset = ConstructionPlane::create_explicit(
      ConstructionPlaneId("plane.offset"), "Offset neutral", {0.0, 0.0, 2.0}, {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0});
  REQUIRE(offset);
  REQUIRE(document.value().add_construction_plane(offset.value()));
  auto tilted_plane =
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane.tilted"), "Tilted neutral", {},
                                         {1.0, 0.0, 0.0}, {0.0, 0.8, -0.6}, {0.0, 0.6, 0.8});
  REQUIRE(tilted_plane);
  REQUIRE(document.value().add_construction_plane(tilted_plane.value()));
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

FaceReference face(SemanticFace value, FeatureId source = FeatureId("feature.base")) {
  auto semantic = SemanticFaceReference::create(std::move(source), value);
  REQUIRE(semantic);
  auto reference = FaceReference::create_planar(PartFeatureInputRole::DraftFace, semantic.value());
  REQUIRE(reference);
  return reference.value();
}

AxisReference datum_pull() {
  auto result = AxisReference::create_datum_axis(PartFeatureInputRole::DraftPullDirection,
                                                 DatumAxisId("axis.pull"));
  REQUIRE(result);
  return result.value();
}

AxisReference tilted_pull() {
  auto result = AxisReference::create_construction_line(PartFeatureInputRole::DraftPullDirection,
                                                        ConstructionLineId("line.tilted"),
                                                        PartFeatureInputCapability::Line);
  REQUIRE(result);
  return result.value();
}

PlaneReference datum_neutral() {
  auto result = PlaneReference::create_datum_plane(PartFeatureInputRole::DraftNeutralPlane,
                                                   DatumPlaneId("datum.xy"));
  REQUIRE(result);
  return result.value();
}

PlaneReference construction_neutral(const char* id) {
  auto result = PlaneReference::create_construction_plane(PartFeatureInputRole::DraftNeutralPlane,
                                                          ConstructionPlaneId(id));
  REQUIRE(result);
  return result.value();
}

void add_draft(PartDocument& document, std::vector<FaceReference> faces,
               AxisReference pull = datum_pull(), PlaneReference neutral = datum_neutral()) {
  auto draft = DraftFeature::create(FeatureId("feature.draft"), "Draft", BodyId("body.base"),
                                    std::move(faces), std::move(pull), std::move(neutral),
                                    ParameterId("draft.angle"));
  REQUIRE(draft);
  REQUIRE(document.add_draft_feature(draft.value()));
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("cache.draft"));
  REQUIRE(result);
  return result.value();
}

double volume(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

GeometryShapeBounds bounds(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}
} // namespace

TEST_CASE("Block 74 executes signed positive and negative draft", "[geometry][draft-feature]") {
  auto positive = box_document(5.0);
  add_draft(positive, {face(SemanticFace::Right)});
  ShapeCache positive_cache = cache();
  auto positive_result = GeometryRecomputeExecutor{}.execute_document(positive, positive_cache);
  INFO((positive_result.has_error()
            ? positive_result.error().object_id() + ": " + positive_result.error().message()
            : "success"));
  REQUIRE(positive_result);
  CHECK(positive_result.value().executed_feature_count == 2U);
  CHECK(volume(positive_cache) > 1920.0);
  CHECK(bounds(positive_cache).maximum.x > 10.0);

  auto negative = box_document(-5.0);
  add_draft(negative, {face(SemanticFace::Right)});
  ShapeCache negative_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(negative, negative_cache));
  CHECK(volume(negative_cache) < 1920.0);
  CHECK(volume(positive_cache) > volume(negative_cache));
}

TEST_CASE("Block 74 drafts multiple semantic faces", "[geometry][draft-feature]") {
  auto document = box_document();
  add_draft(document, {face(SemanticFace::Right), face(SemanticFace::Front)});
  ShapeCache shape_cache = cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  INFO((result.has_error() ? result.error().object_id() + ": " + result.error().message()
                           : "success"));
  REQUIRE(result);
  CHECK(volume(shape_cache) > 1920.0);
  CHECK(bounds(shape_cache).maximum.x > 10.0);
  CHECK(bounds(shape_cache).minimum.y < -6.0);
}

TEST_CASE("Block 74 accepts arbitrary pull directions and non-root neutral planes",
          "[geometry][draft-feature]") {
  auto tilted = box_document();
  add_draft(tilted, {face(SemanticFace::Right)}, tilted_pull(),
            construction_neutral("plane.tilted"));
  ShapeCache tilted_cache = cache();
  const auto tilted_result = GeometryRecomputeExecutor{}.execute_document(tilted, tilted_cache);
  INFO((tilted_result.has_error()
            ? tilted_result.error().object_id() + ": " + tilted_result.error().message()
            : "success"));
  REQUIRE(tilted_result);
  CHECK(volume(tilted_cache) != Catch::Approx(1920.0));

  auto offset = box_document();
  add_draft(offset, {face(SemanticFace::Right)}, datum_pull(),
            construction_neutral("plane.offset"));
  ShapeCache offset_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(offset, offset_cache));
  CHECK(volume(offset_cache) > 1920.0);
}

TEST_CASE("Block 74 recomputes angle and recovers faces after upstream edits",
          "[geometry][draft-feature]") {
  auto document = box_document();
  add_draft(document, {face(SemanticFace::Right)});
  ShapeCache shape_cache = cache();
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(document, shape_cache));
  const double initial_volume = volume(shape_cache);

  document.mark_all_clean();
  auto angle = Quantity::angle_deg(10.0, "draft.angle");
  REQUIRE(angle);
  REQUIRE(document.set_parameter_value(ParameterId("draft.angle"), angle.value()));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto angle_result = executor.execute_plan(document, plan.value(), shape_cache);
  REQUIRE(angle_result);
  CHECK(angle_result.value().executed_feature_count == 1U);
  CHECK(volume(shape_cache) > initial_volume);

  document.mark_all_clean();
  auto width = Quantity::length_mm(30.0, "base.width");
  REQUIRE(width);
  REQUIRE(document.set_parameter_value(ParameterId("base.width"), width.value()));
  plan = document.create_recompute_plan();
  REQUIRE(plan);
  auto upstream = executor.execute_plan(document, plan.value(), shape_cache);
  REQUIRE(upstream);
  CHECK(upstream.value().executed_feature_count == 2U);
  CHECK(bounds(shape_cache).maximum.x > 15.0);
}

TEST_CASE("Block 74 rejects broken tangent and self-intersecting drafts transactionally",
          "[geometry][draft-feature]") {
  auto direct_document = box_document();
  ShapeCache direct_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(direct_document, direct_cache));
  const double original_volume = volume(direct_cache);
  auto invalid =
      DraftFeature::create(FeatureId("feature.invalid-draft"), "Invalid", BodyId("body.base"),
                           {face(SemanticFace::Right, FeatureId("feature.missing"))}, datum_pull(),
                           datum_neutral(), ParameterId("draft.angle"));
  REQUIRE(invalid);
  const GeometryShape* target = direct_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(target != nullptr);
  auto broken = DraftAdapter{}.apply(direct_document, invalid.value(), *target, direct_cache,
                                     {0.0, 0.0, 1.0}, {}, {0.0, 0.0, 1.0}, 5.0);
  REQUIRE(broken.has_error());
  CHECK(broken.error().object_id() == "feature.missing");
  CHECK(volume(direct_cache) == Catch::Approx(original_volume));

  auto tangent = DraftFeature::create(FeatureId("feature.tangent"), "Tangent", BodyId("body.base"),
                                      {face(SemanticFace::Right)}, datum_pull(), datum_neutral(),
                                      ParameterId("draft.angle"));
  REQUIRE(tangent);
  auto rejected = DraftAdapter{}.apply(direct_document, tangent.value(), *target, direct_cache,
                                       {1.0, 0.0, 0.0}, {}, {0.0, 0.0, 1.0}, 5.0);
  REQUIRE(rejected.has_error());
  CHECK(volume(direct_cache) == Catch::Approx(original_volume));

  auto crossing =
      DraftFeature::create(FeatureId("feature.crossing"), "Crossing", BodyId("body.base"),
                           {face(SemanticFace::Right), face(SemanticFace::Left)}, datum_pull(),
                           datum_neutral(), ParameterId("draft.angle"));
  REQUIRE(crossing);
  auto self_intersecting =
      DraftAdapter{}.apply(direct_document, crossing.value(), *target, direct_cache,
                           {0.0, 0.0, 1.0}, {}, {0.0, 0.0, 1.0}, -80.0);
  REQUIRE(self_intersecting.has_error());
  CHECK(volume(direct_cache) == Catch::Approx(original_volume));
}
