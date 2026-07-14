#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

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

SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "surface coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchCoordinate3D parameter_coordinate(const std::string& id) {
  auto result = SketchCoordinate3D::create_parameter(ParameterId(id));
  REQUIRE(result);
  return result.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("surface.trim-extend.cache"));
  REQUIRE(result);
  return result.value();
}

void add_body(PartDocument& document, const std::string& id, BodyKind kind) {
  auto body = Body::create(BodyId(id), id, kind);
  REQUIRE(body);
  REQUIRE(document.add_body(body.value()));
}

BoundaryCurveReference boundary(const std::string& path) {
  auto result = BoundaryCurveReference::create_path_curve(PathCurveId(path));
  REQUIRE(result);
  return result.value();
}

SurfaceReference body_surface(const std::string& body) {
  auto result = SurfaceReference::create_body(BodyId(body));
  REQUIRE(result);
  return result.value();
}

PartDocument trim_document() {
  auto created = PartDocument::create(DocumentId("part.surface.trim"), "Trim surface");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_parameter(length_parameter("base.width", 20.0)));
  REQUIRE(document.add_parameter(length_parameter("base.height", 14.0)));
  REQUIRE(document.add_parameter(length_parameter("base.depth", 8.0)));
  REQUIRE(document.add_parameter(length_parameter("trim.width", 10.0)));
  REQUIRE(document.add_parameter(length_parameter("trim.height", 6.0)));
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));

  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                               ParameterId("base.height"));
  REQUIRE(base_profile);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(document.add_sketch(base_sketch.value()));
  add_body(document, "body.base", BodyKind::Solid);
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.base"));
  REQUIRE(context);
  auto contextual = base.value().with_body_result_context(context.value());
  REQUIRE(contextual);
  REQUIRE(document.add_feature(contextual.value()));

  auto plane = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.trim"), "Trim plane",
                                                  {0.0, 0.0, 8.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
                                                  {0.0, 0.0, 1.0});
  REQUIRE(plane);
  REQUIRE(document.add_construction_plane(plane.value()));
  auto trim_sketch = Sketch::create(SketchId("sketch.trim"), "Trim", DatumPlaneId("plane.trim"));
  REQUIRE(trim_sketch);
  auto trim_profile = RectangleProfile::create(ProfileId("profile.trim"), ParameterId("trim.width"),
                                               ParameterId("trim.height"));
  REQUIRE(trim_profile);
  REQUIRE(trim_sketch.value().add_profile(trim_profile.value()));
  REQUIRE(document.add_sketch(trim_sketch.value()));

  add_body(document, "body.trim", BodyKind::Surface);
  auto semantic = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(semantic);
  auto target = SurfaceReference::create_semantic_face(semantic.value());
  REQUIRE(target);
  auto profile = ProfileRegionReference::create(SketchId("sketch.trim"), ProfileId("profile.trim"),
                                                PartFeatureInputRole::ProfileRegion);
  REQUIRE(profile);
  auto trimming = TrimmingReference::create_profile_region(profile.value());
  REQUIRE(trimming);
  auto trim = TrimSurfaceFeature::create(FeatureId("feature.trim"), "Trim", target.value(),
                                         trimming.value(), BodyId("body.trim"));
  REQUIRE(trim);
  REQUIRE(document.add_surface_feature(trim.value()));
  return document;
}

PartDocument extend_document() {
  auto created = PartDocument::create(DocumentId("part.surface.extend"), "Extend surface");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_parameter(length_parameter("boundary.width", 12.0)));
  REQUIRE(document.add_parameter(length_parameter("extend.distance", 3.0)));

  const Sketch3DId sketch_id("sketch3d.boundary");
  auto sketch = Sketch3D::create(sketch_id, "Parameter boundary");
  REQUIRE(sketch);
  const std::vector<SketchPoint3D> points = {
      SketchPoint3D::create(SketchEntityId("p0"), coordinate(0.0), coordinate(0.0), coordinate(0.0))
          .value(),
      SketchPoint3D::create(SketchEntityId("p1"), parameter_coordinate("boundary.width"),
                            coordinate(0.0), coordinate(0.0))
          .value(),
      SketchPoint3D::create(SketchEntityId("p2"), parameter_coordinate("boundary.width"),
                            coordinate(8.0), coordinate(0.0))
          .value(),
      SketchPoint3D::create(SketchEntityId("p3"), coordinate(0.0), coordinate(8.0), coordinate(0.0))
          .value()};
  for (const auto& point : points)
    REQUIRE(sketch.value().add_point(point));
  std::vector<PathSegmentReference> closed_segments;
  for (std::size_t index = 0U; index < 4U; ++index) {
    const SketchEntityId line_id("line." + std::to_string(index));
    auto line = SketchLine3D::create(line_id, SketchEntityId("p" + std::to_string(index)),
                                     SketchEntityId("p" + std::to_string((index + 1U) % 4U)));
    REQUIRE(line);
    REQUIRE(sketch.value().add_line(line.value()));
    auto segment =
        PathSegmentReference::create_sketch_3d(sketch_id, line_id, Sketch3DPathCurveKind::Line);
    REQUIRE(segment);
    closed_segments.push_back(segment.value());
  }
  REQUIRE(document.add_sketch_3d(sketch.value()));
  auto closed = PathCurve::create(PathCurveId("path.closed"), "Closed boundary", closed_segments,
                                  PathClosure::Closed, PathOrientationRule::MinimumTwist);
  REQUIRE(closed);
  REQUIRE(document.add_path_curve(closed.value()));
  auto edge =
      PathCurve::create(PathCurveId("path.extend-edge"), "Extend edge", {closed_segments.front()},
                        PathClosure::Open, PathOrientationRule::MinimumTwist);
  REQUIRE(edge);
  REQUIRE(document.add_path_curve(edge.value()));

  add_body(document, "body.fill", BodyKind::Surface);
  add_body(document, "body.extend", BodyKind::Surface);
  auto fill = FillSurfaceFeature::create(FeatureId("feature.fill"), "Fill",
                                         {boundary("path.closed")}, BodyId("body.fill"));
  REQUIRE(fill);
  REQUIRE(document.add_surface_feature(fill.value()));
  auto extend = ExtendSurfaceFeature::create(
      FeatureId("feature.extend"), "Extend", body_surface("body.fill"),
      boundary("path.extend-edge"), ParameterId("extend.distance"), BodyId("body.extend"));
  REQUIRE(extend);
  REQUIRE(document.add_surface_feature(extend.value()));
  return document;
}

} // namespace

TEST_CASE("Block 90 trims a recovered semantic face and recomputes after source changes",
          "[geometry][surface-trim-extend]") {
  PartDocument document = trim_document();
  ShapeCache shapes = cache();
  auto first = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO(
      (first.has_error() ? first.error().object_id() + ": " + first.error().message() : "success"));
  REQUIRE(first);
  REQUIRE(shapes.find_body_shape(BodyId("body.trim")) != nullptr);
  CHECK_FALSE(shapes.find_body_shape(BodyId("body.trim"))->empty());

  auto wider = Quantity::length_mm(24.0, "wider base");
  REQUIRE(wider);
  REQUIRE(document.set_parameter_value(ParameterId("base.width"), wider.value()));
  auto second = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO((second.has_error() ? second.error().object_id() + ": " + second.error().message()
                           : "success"));
  REQUIRE(second);
  CHECK(shapes.find_feature_shape(FeatureId("feature.trim")) != nullptr);
}

TEST_CASE("Block 90 extends one recovered linear Surface boundary",
          "[geometry][surface-trim-extend]") {
  PartDocument document = extend_document();
  ShapeCache shapes = cache();
  auto first = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO(
      (first.has_error() ? first.error().object_id() + ": " + first.error().message() : "success"));
  REQUIRE(first);
  const GeometryShape* extended = shapes.find_body_shape(BodyId("body.extend"));
  REQUIRE(extended != nullptr);
  auto first_bounds = BodyTransformAdapter{}.bounds(*extended);
  REQUIRE(first_bounds);
  CHECK(first_bounds.value().minimum.y == Catch::Approx(-3.0).margin(1.0e-6));

  auto wider = Quantity::length_mm(16.0, "wider boundary");
  REQUIRE(wider);
  REQUIRE(document.set_parameter_value(ParameterId("boundary.width"), wider.value()));
  auto second = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO((second.has_error() ? second.error().object_id() + ": " + second.error().message()
                           : "success"));
  REQUIRE(second);
  extended = shapes.find_body_shape(BodyId("body.extend"));
  REQUIRE(extended != nullptr);
  auto second_bounds = BodyTransformAdapter{}.bounds(*extended);
  REQUIRE(second_bounds);
  CHECK(second_bounds.value().maximum.x == Catch::Approx(16.0).margin(1.0e-6));
}

TEST_CASE("Block 90 fails closed on an ambiguous multi-face trim target",
          "[geometry][surface-trim-extend]") {
  const std::vector<std::vector<SweepPathSegment>> boundaries = {
      {{SweepPathSegmentKind::Line, {0.0, 0.0, 0.0}, {}, {5.0, 0.0, 1.0}},
       {SweepPathSegmentKind::Line, {5.0, 0.0, 1.0}, {}, {10.0, 0.0, 0.0}}},
      {{SweepPathSegmentKind::Line, {0.0, 8.0, 0.0}, {}, {5.0, 8.0, 1.0}},
       {SweepPathSegmentKind::Line, {5.0, 8.0, 1.0}, {}, {10.0, 8.0, 0.0}}}};
  auto target = SurfaceAdapter{}.make_boundary_surface(FeatureId("feature.ambiguous"), boundaries);
  REQUIRE(target);
  const std::vector<SweepPathSegment> trimming = {
      {SweepPathSegmentKind::Line, {2.0, 2.0, 0.0}, {}, {8.0, 2.0, 0.0}},
      {SweepPathSegmentKind::Line, {8.0, 2.0, 0.0}, {}, {8.0, 6.0, 0.0}},
      {SweepPathSegmentKind::Line, {8.0, 6.0, 0.0}, {}, {2.0, 6.0, 0.0}},
      {SweepPathSegmentKind::Line, {2.0, 6.0, 0.0}, {}, {2.0, 2.0, 0.0}}};
  auto result = SurfaceAdapter{}.trim_surface(FeatureId("feature.trim"), target.value(), trimming);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("unambiguous") != std::string::npos);
}
