#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

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

PartDocument box_document(bool owned = true) {
  auto document = PartDocument::create(DocumentId("part.transform_geometry"), "TransformGeometry");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_parameter(length_parameter("width", 20.0)));
  REQUIRE(document.value().add_parameter(length_parameter("height", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("depth", 5.0)));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("width"),
                                            ParameterId("height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto unowned = Sketch::create(SketchId("sketch.unowned"), "Unowned", DatumPlaneId("datum.xy"));
  REQUIRE(unowned);
  REQUIRE(document.value().add_sketch(unowned.value()));
  auto body = Body::create(BodyId("body.base"), "Base", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.value().add_body(body.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                  SketchId("sketch.base"), ParameterId("depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  REQUIRE(document.value().add_feature(attached.value()));
  if (owned) {
    auto ownership = SketchOwnership::create(SketchId("sketch.base"), BodyId("body.base"),
                                             SketchAssociation::DrivesBody);
    REQUIRE(ownership);
    REQUIRE(document.value().add_sketch_ownership(ownership.value()));
  }
  return document.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("cache.transform_geometry"));
  REQUIRE(result);
  return result.value();
}

BodyTransform translation(const char* id, Vector3 offset, bool move_owned = true) {
  auto result = BodyTransform::create_translate(BodyTransformId(id), BodyId("body.base"),
                                                BodyTransformCoordinateSpace::World, std::nullopt,
                                                offset, move_owned, move_owned);
  REQUIRE(result);
  return result.value();
}

BodyTransform rotation(const char* id, double angle_deg) {
  auto axis = BodyTransformRotationAxis::create_explicit({}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  auto result = BodyTransform::create_rotate(BodyTransformId(id), BodyId("body.base"),
                                             BodyTransformCoordinateSpace::World, std::nullopt,
                                             axis.value(), angle_deg, true, true);
  REQUIRE(result);
  return result.value();
}

GeometryShapeBounds bounds(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}

} // namespace

TEST_CASE("Body transform Geometry executes translation and remains incremental-idempotent",
          "[geometry][body-transform]") {
  PartDocument document = box_document();
  auto top_face = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(top_face);
  auto top_workplane = DerivedWorkplane::create_on_feature_face(DatumPlaneId("workplane.top"),
                                                                "Top", top_face.value());
  REQUIRE(top_workplane);
  REQUIRE(document.add_derived_workplane(top_workplane.value()));
  auto projection_sketch =
      Sketch::create(SketchId("sketch.projection"), "Projection", DatumPlaneId("workplane.top"));
  REQUIRE(projection_sketch);
  REQUIRE(document.add_sketch(projection_sketch.value()));
  REQUIRE(document.add_body_transform(translation("transform.translate", {10.0, -2.0, 3.0})));
  ShapeCache shape_cache = cache();
  const auto summary = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2U);
  auto transformed = bounds(shape_cache);
  CHECK(transformed.minimum.x == Catch::Approx(0.0).margin(1.0e-5));
  CHECK(transformed.maximum.x == Catch::Approx(20.0).margin(1.0e-5));
  CHECK(transformed.minimum.z == Catch::Approx(3.0).margin(1.0e-5));
  CHECK(transformed.maximum.z == Catch::Approx(8.0).margin(1.0e-5));
  auto semantic_edge =
      SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(semantic_edge);
  auto resolved_edge =
      SemanticReferenceEvaluator{}.resolve_edge(document, semantic_edge.value(), shape_cache);
  REQUIRE(resolved_edge);
  CHECK(resolved_edge.value().start == Point3{0.0, 3.0, 8.0});
  CHECK(resolved_edge.value().end == Point3{20.0, 3.0, 8.0});
  auto projected = ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("projected.top_front"), semantic_edge.value());
  REQUIRE(projected);
  auto projected_line = SketchReferenceProjector{}.resolve_line(
      document, *document.find_sketch(SketchId("sketch.projection")), projected.value(),
      shape_cache);
  REQUIRE(projected_line);
  CHECK(projected_line.value().point == Point2{-10.0, 5.0});
  CHECK(projected_line.value().direction == Vector2{1.0, 0.0});

  document.mark_all_clean();
  REQUIRE(document.mark_body_transform_changed(BodyTransformId("transform.translate")));
  auto plan = document.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(document, plan.value(), shape_cache));
  transformed = bounds(shape_cache);
  CHECK(transformed.minimum.x == Catch::Approx(0.0).margin(1.0e-5));
  CHECK(transformed.maximum.x == Catch::Approx(20.0).margin(1.0e-5));
}

TEST_CASE("Body transform Geometry applies uniform scale to OCCT result",
          "[geometry][body-transform]") {
  PartDocument document = box_document();
  auto scale = BodyTransform::create_uniform_scale(
      BodyTransformId("transform.scale"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, 2.0, {}, true, true);
  REQUIRE(scale);
  REQUIRE(document.add_body_transform(scale.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.base"));
  REQUIRE(shape != nullptr);
  CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 == Catch::Approx(8000.0));
}

TEST_CASE("Body transform stack order is geometrically significant", "[geometry][body-transform]") {
  PartDocument translate_then_rotate = box_document();
  REQUIRE(translate_then_rotate.add_body_transform(
      translation("transform.translate", {10.0, 0.0, 0.0})));
  REQUIRE(translate_then_rotate.add_body_transform(rotation("transform.rotate", 90.0)));
  ShapeCache first_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(translate_then_rotate, first_cache));

  PartDocument rotate_then_translate = box_document();
  REQUIRE(rotate_then_translate.add_body_transform(rotation("transform.rotate", 90.0)));
  REQUIRE(rotate_then_translate.add_body_transform(
      translation("transform.translate", {10.0, 0.0, 0.0})));
  ShapeCache second_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(rotate_then_translate, second_cache));

  const auto first = bounds(first_cache);
  const auto second = bounds(second_cache);
  CHECK(first.minimum.x == Catch::Approx(-5.0).margin(1.0e-5));
  CHECK(first.maximum.y == Catch::Approx(20.0).margin(1.0e-5));
  CHECK(second.minimum.x == Catch::Approx(5.0).margin(1.0e-5));
  CHECK(second.maximum.y == Catch::Approx(10.0).margin(1.0e-5));
}

TEST_CASE("Body-local translation follows the preceding transformed frame",
          "[geometry][body-transform]") {
  PartDocument document = box_document();
  REQUIRE(document.add_body_transform(rotation("transform.rotate", 90.0)));
  auto local_translation = BodyTransform::create_translate(
      BodyTransformId("transform.local"), BodyId("body.base"),
      BodyTransformCoordinateSpace::BodyLocal, std::nullopt, Vector3{10.0, 0.0, 0.0}, false, false);
  REQUIRE(local_translation);
  REQUIRE(document.add_body_transform(local_translation.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const auto transformed = bounds(shape_cache);
  CHECK(transformed.minimum.x == Catch::Approx(-5.0).margin(1.0e-5));
  CHECK(transformed.minimum.y == Catch::Approx(0.0).margin(1.0e-5));
  CHECK(transformed.maximum.y == Catch::Approx(20.0).margin(1.0e-5));
}

TEST_CASE("Owned sketch frames move while local coordinates and unowned sketches stay fixed",
          "[geometry][body-transform]") {
  PartDocument document = box_document();
  REQUIRE(document.add_body_transform(translation("transform.translate", {4.0, 5.0, 6.0})));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const Sketch* owned = document.find_sketch(SketchId("sketch.base"));
  const Sketch* unowned = document.find_sketch(SketchId("sketch.unowned"));
  REQUIRE(owned != nullptr);
  REQUIRE(unowned != nullptr);
  WorkplaneResolver resolver;
  auto moved = resolver.resolve_for_sketch(document, *owned, shape_cache);
  auto fixed = resolver.resolve_for_sketch(document, *unowned, shape_cache);
  REQUIRE(moved);
  REQUIRE(fixed);
  CHECK(moved.value().origin == Point3{4.0, 5.0, 6.0});
  CHECK(fixed.value().origin == Point3{});
  CHECK(resolver.evaluate_point(moved.value(), {2.0, 3.0}) == Point3{6.0, 8.0, 6.0});
  CHECK(owned->rectangle_profiles().front().center() == Point2{});
  REQUIRE(shape_cache.find_reference_transform("datum.xy") != nullptr);
}

TEST_CASE("Ownership state crosses transforms that do not move owned references",
          "[geometry][body-transform]") {
  PartDocument document = box_document();
  REQUIRE(document.add_body_transform(translation("transform.first", {10.0, 0.0, 0.0})));
  auto fixed_axis = BodyTransformRotationAxis::create_explicit({}, {0.0, 0.0, 1.0});
  REQUIRE(fixed_axis);
  auto fixed_rotation = BodyTransform::create_rotate(
      BodyTransformId("transform.fixed"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, fixed_axis.value(), 90.0, false, false);
  REQUIRE(fixed_rotation);
  REQUIRE(document.add_body_transform(fixed_rotation.value()));
  REQUIRE(document.add_body_transform(translation("transform.last", {0.0, 2.0, 0.0})));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const Sketch* sketch = document.find_sketch(SketchId("sketch.base"));
  REQUIRE(sketch != nullptr);
  auto frame = WorkplaneResolver{}.resolve_for_sketch(document, *sketch, shape_cache);
  REQUIRE(frame);
  CHECK(frame.value().origin == Point3{10.0, 2.0, 0.0});
  CHECK(frame.value().x_axis == Vector3{1.0, 0.0, 0.0});
}
