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

FeatureBodyResultContext body_context(FeatureBodyOperationMode mode, const char* body_id) {
  auto context = FeatureBodyResultContext::create(
      mode, mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{} : BodyId(body_id),
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId(body_id)}
                                                : std::optional<BodyId>{});
  REQUIRE(context);
  return context.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("cache.path-extrude"));
  REQUIRE(result);
  return result.value();
}

double volume(const ShapeCache& cache, const char* body_id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(body_id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

void add_bounded_path(PartDocument& document) {
  auto start = ConstructionPoint::create_explicit(ConstructionPointId("point.start"), "Start", {});
  auto end =
      ConstructionPoint::create_explicit(ConstructionPointId("point.end"), "End", {0.0, 0.0, 20.0});
  REQUIRE(start);
  REQUIRE(end);
  REQUIRE(document.add_construction_point(start.value()));
  REQUIRE(document.add_construction_point(end.value()));
  auto relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.path"), ConstructionPointId("point.start"),
      ConstructionPointId("point.end"));
  REQUIRE(relation);
  auto line = ConstructionLine::create_through_two_points(ConstructionLineId("line.path"), "Path",
                                                          relation.value());
  REQUIRE(line);
  REQUIRE(document.add_construction_line(line.value()));
  auto segment = PathSegmentReference::create_construction_line(ConstructionLineId("line.path"));
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId("path.route"), "Route", {segment.value()},
                                PathClosure::Open, PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
}

void add_curved_path(PartDocument& document) {
  auto plane =
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane.path"), "Path", {},
                                         {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0});
  REQUIRE(plane);
  REQUIRE(document.add_construction_plane(plane.value()));
  auto sketch = Sketch::create(SketchId("sketch.path"), "Path", DatumPlaneId("plane.path"));
  REQUIRE(sketch);
  auto arc = ArcSegment::create_three_point(SketchEntityId("arc.path"), {0.0, 0.0}, {4.0, 6.0},
                                            {10.0, 10.0});
  REQUIRE(arc);
  REQUIRE(sketch.value().add_entity(arc.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto segment = PathSegmentReference::create_planar(
      SketchId("sketch.path"), SketchEntityId("arc.path"), PlanarPathCurveKind::Arc);
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId("path.route"), "Route", {segment.value()},
                                PathClosure::Open, PathOrientationRule::ProfileNormal);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
}

Sketch rectangle_sketch(const char* id, const char* profile_id, const char* width,
                        const char* height, Point2 center = {}) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto profile = RectangleProfile::create(ProfileId(profile_id), ParameterId(width),
                                          ParameterId(height), center);
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

} // namespace

TEST_CASE("Block 83 executes AdditiveExtrude in path direction", "[geometry][path-extrude]") {
  auto created = PartDocument::create(DocumentId("part.path-add"), "Path add");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_parameter(length_parameter("profile.width", 4.0)));
  REQUIRE(document.add_parameter(length_parameter("profile.height", 6.0)));
  REQUIRE(document.add_sketch(
      rectangle_sketch("sketch.profile", "profile.path", "profile.width", "profile.height")));
  add_bounded_path(document);
  REQUIRE(document.add_body(solid_body("body.path")));
  auto feature =
      Feature::create_additive_path_extrude(FeatureId("feature.path"), "Path extrude",
                                            SketchId("sketch.profile"), PathCurveId("path.route"));
  REQUIRE(feature);
  auto attached = feature.value().with_body_result_context(
      body_context(FeatureBodyOperationMode::NewBody, "body.path"));
  REQUIRE(attached);
  REQUIRE(document.add_feature(attached.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  CHECK(volume(shape_cache, "body.path") == Catch::Approx(480.0).epsilon(1.0e-6));
}

TEST_CASE("Block 83 executes SubtractiveExtrude along a path transactionally",
          "[geometry][path-extrude][integration][part-construction-mvp]") {
  auto created = PartDocument::create(DocumentId("part.path-cut"), "Path cut");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.add_parameter(length_parameter("base.height", 20.0)));
  REQUIRE(document.add_parameter(length_parameter("base.depth", 20.0)));
  REQUIRE(document.add_parameter(length_parameter("cut.width", 4.0)));
  REQUIRE(document.add_parameter(length_parameter("cut.height", 4.0)));
  REQUIRE(document.add_sketch(
      rectangle_sketch("sketch.base", "profile.base", "base.width", "base.height")));
  REQUIRE(document.add_sketch(
      rectangle_sketch("sketch.cut", "profile.cut", "cut.width", "cut.height")));
  add_bounded_path(document);
  REQUIRE(document.add_body(solid_body("body.base")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto base_attached = base.value().with_body_result_context(
      body_context(FeatureBodyOperationMode::NewBody, "body.base"));
  REQUIRE(base_attached);
  REQUIRE(document.add_feature(base_attached.value()));
  auto cut = Feature::create_subtractive_path_extrude(
      FeatureId("feature.path-cut"), "Path cut", SketchId("sketch.cut"), FeatureId("feature.base"),
      PathCurveId("path.route"));
  REQUIRE(cut);
  auto cut_attached = cut.value().with_body_result_context(
      body_context(FeatureBodyOperationMode::Cut, "body.base"));
  REQUIRE(cut_attached);
  REQUIRE(document.add_feature(cut_attached.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  CHECK(volume(shape_cache, "body.base") == Catch::Approx(7680.0).epsilon(1.0e-6));
}

TEST_CASE("Block 83 path Extrude follows a curved planar route", "[geometry][path-extrude]") {
  auto created = PartDocument::create(DocumentId("part.path-curved"), "Curved path extrude");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_parameter(length_parameter("profile.width", 4.0)));
  REQUIRE(document.add_parameter(length_parameter("profile.height", 6.0)));
  REQUIRE(document.add_sketch(
      rectangle_sketch("sketch.profile", "profile.path", "profile.width", "profile.height")));
  add_curved_path(document);
  REQUIRE(document.add_body(solid_body("body.path")));
  auto feature =
      Feature::create_additive_path_extrude(FeatureId("feature.path"), "Curved path extrude",
                                            SketchId("sketch.profile"), PathCurveId("path.route"));
  REQUIRE(feature);
  auto attached = feature.value().with_body_result_context(
      body_context(FeatureBodyOperationMode::NewBody, "body.path"));
  REQUIRE(attached);
  REQUIRE(document.add_feature(attached.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  CHECK(volume(shape_cache, "body.path") > 300.0);
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.path"));
  REQUIRE(shape != nullptr);
  auto bounds = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(bounds);
  CHECK(bounds.value().maximum.x - bounds.value().minimum.x > 9.0);
  CHECK(bounds.value().maximum.z - bounds.value().minimum.z > 9.0);
}
