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

ShapeCache make_shape_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape_cache.construction_plane"));
  REQUIRE(cache);
  return cache.value();
}

Sketch make_closed_rectangle_sketch_on_construction_plane() {
  auto sketch = Sketch::create(SketchId("sketch.closed_rectangle_on_plane"),
                               "Sketch_ClosedRectangleOnConstructionPlane",
                               DatumPlaneId("construction_plane.offset_xy"));
  REQUIRE(sketch);

  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{-10.0, -5.0}, Point2{10.0, -5.0})
          .value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{10.0, -5.0}, Point2{10.0, 5.0})
          .value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{10.0, 5.0}, Point2{-10.0, 5.0})
          .value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.d"), Point2{-10.0, 5.0}, Point2{-10.0, -5.0})
          .value()));

  auto profile = ClosedProfile::create(
      ProfileId("profile.closed_rectangle"), {SketchEntityId("line.a"), SketchEntityId("line.b"),
                                               SketchEntityId("line.c"), SketchEntityId("line.d")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

PartDocument make_explicit_construction_plane_document() {
  auto document = PartDocument::create(DocumentId("part.construction_plane_prism"),
                                       "ConstructionPlanePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 7.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.plane_offset", "plane_offset", 25.0)));

  auto plane = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", Point3{0.0, 0.0, 25.0},
      Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
      {ParameterId("part.plane_offset")});
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));

  REQUIRE(document.value().add_sketch(make_closed_rectangle_sketch_on_construction_plane()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.closed_rectangle_prism"), "ClosedRectanglePrism",
      SketchId("sketch.closed_rectangle_on_plane"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

PartDocument make_relation_offset_construction_plane_document() {
  auto document = PartDocument::create(DocumentId("part.relation_construction_plane_prism"),
                                       "RelationConstructionPlanePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 7.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.plane_offset", "plane_offset", 25.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto relation = ConstructionRelation::create_plane_offset_from_plane(
      ConstructionRelationId("relation.offset_xy"), DatumPlaneId("datum.xy"),
      ParameterId("part.plane_offset"));
  REQUIRE(relation);
  auto plane = ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", relation.value());
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));

  REQUIRE(document.value().add_sketch(make_closed_rectangle_sketch_on_construction_plane()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.closed_rectangle_prism"), "ClosedRectanglePrism",
      SketchId("sketch.closed_rectangle_on_plane"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

PartDocument make_plane_through_points_document() {
  auto document = PartDocument::create(DocumentId("part.through_points_plane"),
                                       "ThroughPointsPlane");
  REQUIRE(document);

  REQUIRE(document.value().add_construction_point(
      ConstructionPoint::create_explicit(ConstructionPointId("construction_point.a"), "A",
                                         Point3{1.0, 2.0, 3.0})
          .value()));
  REQUIRE(document.value().add_construction_point(
      ConstructionPoint::create_explicit(ConstructionPointId("construction_point.b"), "B",
                                         Point3{11.0, 2.0, 3.0})
          .value()));
  REQUIRE(document.value().add_construction_point(
      ConstructionPoint::create_explicit(ConstructionPointId("construction_point.c"), "C",
                                         Point3{1.0, 12.0, 3.0})
          .value()));

  auto relation = ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("relation.plane_abc"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"), ConstructionPointId("construction_point.c"));
  REQUIRE(relation);
  auto plane = ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("construction_plane.abc"), "PlaneABC", relation.value());
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));

  return document.value();
}

} // namespace

TEST_CASE("Geometry recomputes a closed profile on an explicit construction plane",
          "[geometry][construction_plane]") {
  const PartDocument document = make_explicit_construction_plane_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1);
  REQUIRE(cache.final_shape() != nullptr);
  CHECK(cache.final_feature_id().value() == "feature.closed_rectangle_prism");

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Catch::Approx(20.0 * 10.0 * 7.0).margin(1.0));
}

TEST_CASE("Geometry recomputes a closed profile on an offset construction plane relation",
          "[geometry][construction_plane]") {
  const PartDocument document = make_relation_offset_construction_plane_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1);
  REQUIRE(cache.final_shape() != nullptr);

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
  CHECK(shape.volume_mm3 == Catch::Approx(20.0 * 10.0 * 7.0).margin(1.0));
}

TEST_CASE("WorkplaneResolver resolves explicit construction plane frames",
          "[geometry][construction_plane]") {
  const PartDocument document = make_explicit_construction_plane_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("construction_plane.offset_xy"));

  REQUIRE(resolved);
  CHECK(resolved.value().id.value() == "construction_plane.offset_xy");
  CHECK(resolved.value().origin == Point3{0.0, 0.0, 25.0});
  CHECK(resolved.value().x_axis == Vector3{1.0, 0.0, 0.0});
  CHECK(resolved.value().y_axis == Vector3{0.0, 1.0, 0.0});
  CHECK(resolved.value().normal == Vector3{0.0, 0.0, 1.0});
  CHECK_FALSE(resolved.value().bounds.enabled);

  const Point3 global = resolver.evaluate_point(resolved.value(), Point2{2.0, 3.0});
  CHECK(global == Point3{2.0, 3.0, 25.0});
}

TEST_CASE("WorkplaneResolver resolves offset construction plane relations",
          "[geometry][construction_plane]") {
  const PartDocument document = make_relation_offset_construction_plane_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("construction_plane.offset_xy"));

  REQUIRE(resolved);
  CHECK(resolved.value().id.value() == "construction_plane.offset_xy");
  CHECK(resolved.value().origin == Point3{0.0, 0.0, 25.0});
  CHECK(resolved.value().x_axis == Vector3{1.0, 0.0, 0.0});
  CHECK(resolved.value().y_axis == Vector3{0.0, 1.0, 0.0});
  CHECK(resolved.value().normal == Vector3{0.0, 0.0, 1.0});
  CHECK_FALSE(resolved.value().bounds.enabled);
}

TEST_CASE("WorkplaneResolver resolves construction planes through three points",
          "[geometry][construction_plane]") {
  const PartDocument document = make_plane_through_points_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("construction_plane.abc"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin == Point3{1.0, 2.0, 3.0});
  CHECK(resolved.value().x_axis == Vector3{1.0, 0.0, 0.0});
  CHECK(resolved.value().y_axis == Vector3{0.0, 1.0, 0.0});
  CHECK(resolved.value().normal == Vector3{0.0, 0.0, 1.0});

  const Point3 global = resolver.evaluate_point(resolved.value(), Point2{2.0, 3.0});
  CHECK(global == Point3{3.0, 5.0, 3.0});
}
