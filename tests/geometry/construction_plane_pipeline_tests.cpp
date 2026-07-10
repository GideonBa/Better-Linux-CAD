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

  auto profile = ClosedProfile::create(ProfileId("profile.closed_rectangle"),
                                       {SketchEntityId("line.a"), SketchEntityId("line.b"),
                                        SketchEntityId("line.c"), SketchEntityId("line.d")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

Sketch make_rectangle_profile_sketch_on_angled_plane() {
  auto sketch =
      Sketch::create(SketchId("sketch.rectangle_on_angled_plane"), "Sketch_RectangleOnAngledPlane",
                     DatumPlaneId("construction_plane.angled"));
  REQUIRE(sketch);
  auto profile = RectangleProfile::create(ProfileId("profile.rectangle"), ParameterId("part.width"),
                                          ParameterId("part.height"));
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

Sketch make_circle_cut_sketch_on_angled_plane() {
  auto sketch =
      Sketch::create(SketchId("sketch.circle_cut_on_angled_plane"), "Sketch_CircleCutOnAngledPlane",
                     DatumPlaneId("construction_plane.angled_cut"));
  REQUIRE(sketch);
  auto profile =
      CircleProfile::create(ProfileId("profile.circle_cut"), ParameterId("part.hole_diameter"));
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

ConstructionPlane make_angled_plane(const char* id, const char* name, Point3 origin) {
  auto plane = ConstructionPlane::create_explicit(
      ConstructionPlaneId(id), name, origin, Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 0.7071067811865475, 0.7071067811865475},
      Vector3{0.0, -0.7071067811865475, 0.7071067811865475});
  REQUIRE(plane);
  return plane.value();
}

PartDocument make_explicit_construction_plane_document() {
  auto document =
      PartDocument::create(DocumentId("part.construction_plane_prism"), "ConstructionPlanePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 7.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.plane_offset", "plane_offset", 25.0)));

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

PartDocument make_angled_rectangle_profile_document() {
  auto document =
      PartDocument::create(DocumentId("part.angled_rectangle_prism"), "AngledRectanglePrism");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 10.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 7.0)));
  REQUIRE(document.value().add_construction_plane(
      make_angled_plane("construction_plane.angled", "Angled", Point3{0.0, 0.0, 0.0})));
  REQUIRE(document.value().add_sketch(make_rectangle_profile_sketch_on_angled_plane()));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.angled_rectangle_prism"), "AngledRectanglePrism",
      SketchId("sketch.rectangle_on_angled_plane"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PartDocument make_angled_circle_cut_document() {
  auto document = PartDocument::create(DocumentId("part.angled_circle_cut"), "AngledCircleCut");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 12.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 8.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                               ParameterId("part.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));
  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base_feature);
  REQUIRE(document.value().add_feature(base_feature.value()));

  REQUIRE(document.value().add_construction_plane(
      make_angled_plane("construction_plane.angled_cut", "AngledCut", Point3{0.0, 0.0, 6.0})));
  REQUIRE(document.value().add_sketch(make_circle_cut_sketch_on_angled_plane()));
  auto cut_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.angled_cut"), "AngledCut", SketchId("sketch.circle_cut_on_angled_plane"),
      FeatureId("feature.base"));
  REQUIRE(cut_feature);
  REQUIRE(document.value().add_feature(cut_feature.value()));
  return document.value();
}

PartDocument make_relation_offset_construction_plane_document() {
  auto document = PartDocument::create(DocumentId("part.relation_construction_plane_prism"),
                                       "RelationConstructionPlanePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 7.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.plane_offset", "plane_offset", 25.0)));

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
  auto document =
      PartDocument::create(DocumentId("part.through_points_plane"), "ThroughPointsPlane");
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

TEST_CASE("Geometry recomputes a rectangle profile on an angled construction plane",
          "[geometry][construction_plane]") {
  const PartDocument document = make_angled_rectangle_profile_document();
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

TEST_CASE("Geometry recomputes a circular through-all cut on an angled construction plane",
          "[geometry][construction_plane]") {
  const PartDocument document = make_angled_circle_cut_document();
  ShapeCache cache = make_shape_cache();
  const GeometryRecomputeExecutor executor;

  const auto summary = executor.execute_document(document, cache);

  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 2);
  REQUIRE(cache.final_shape() != nullptr);
  CHECK(cache.final_feature_id() == FeatureId("feature.angled_cut"));

  const RectangleExtrusionAdapter inspector;
  const ShapeSummary shape = inspector.summarize(*cache.final_shape());
  CHECK_FALSE(shape.is_null);
  CHECK(shape.solid_count == 1);
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
