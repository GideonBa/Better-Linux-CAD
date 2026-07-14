#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "surface coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point(const std::string& id, Point3 value) {
  auto result = SketchPoint3D::create(SketchEntityId(id), coordinate(value.x), coordinate(value.y),
                                      coordinate(value.z));
  REQUIRE(result);
  return result.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("surface.boundary-fill.cache"));
  REQUIRE(result);
  return result.value();
}

BoundaryCurveReference boundary(const std::string& path_id) {
  auto result = BoundaryCurveReference::create_path_curve(PathCurveId(path_id));
  REQUIRE(result);
  return result.value();
}

void add_surface_body(PartDocument& document, const std::string& id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Surface);
  REQUIRE(body);
  REQUIRE(document.add_body(body.value()));
}

PartDocument empty_surface_document(const std::string& id) {
  auto result = PartDocument::create(DocumentId(id), id);
  REQUIRE(result);
  return result.value();
}

void add_spatial_boundary_network(PartDocument& document) {
  const Sketch3DId sketch_id("sketch3d.boundary");
  auto sketch = Sketch3D::create(sketch_id, "Boundary network");
  REQUIRE(sketch);
  const std::vector<Point3> points = {
      {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 3.0}, {0.0, 10.0, 0.0}};
  for (std::size_t index = 0; index < points.size(); ++index)
    REQUIRE(sketch.value().add_point(point("point." + std::to_string(index), points[index])));
  for (std::size_t index = 0; index < points.size(); ++index) {
    const std::string line_id = "line." + std::to_string(index);
    const std::string start_id = "point." + std::to_string(index);
    const std::string end_id = "point." + std::to_string((index + 1U) % points.size());
    auto line = SketchLine3D::create(SketchEntityId(line_id), SketchEntityId(start_id),
                                     SketchEntityId(end_id));
    REQUIRE(line);
    REQUIRE(sketch.value().add_line(line.value()));
  }
  REQUIRE(document.add_sketch_3d(sketch.value()));

  for (std::size_t index = 0; index < points.size(); ++index) {
    const std::string suffix = std::to_string(index);
    auto segment = PathSegmentReference::create_sketch_3d(
        sketch_id, SketchEntityId("line." + suffix), Sketch3DPathCurveKind::Line);
    REQUIRE(segment);
    auto path =
        PathCurve::create(PathCurveId("path." + suffix), "Boundary " + suffix, {segment.value()},
                          PathClosure::Open, PathOrientationRule::MinimumTwist);
    REQUIRE(path);
    REQUIRE(document.add_path_curve(path.value()));
  }
}

PathCurveId add_planar_rectangle(PartDocument& document) {
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  const SketchId sketch_id("sketch.fill");
  auto sketch = Sketch::create(sketch_id, "Fill boundary", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  const std::vector<std::pair<Point2, Point2>> lines = {{{0.0, 0.0}, {12.0, 0.0}},
                                                        {{12.0, 0.0}, {12.0, 8.0}},
                                                        {{12.0, 8.0}, {0.0, 8.0}},
                                                        {{0.0, 8.0}, {0.0, 0.0}}};
  std::vector<PathSegmentReference> segments;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    const SketchEntityId line_id("fill.line." + std::to_string(index));
    auto line = LineSegment::create(line_id, lines[index].first, lines[index].second);
    REQUIRE(line);
    REQUIRE(sketch.value().add_entity(line.value()));
    auto segment =
        PathSegmentReference::create_planar(sketch_id, line_id, PlanarPathCurveKind::Line);
    REQUIRE(segment);
    segments.push_back(segment.value());
  }
  REQUIRE(document.add_sketch(sketch.value()));
  auto path = PathCurve::create(PathCurveId("path.fill"), "Fill boundary", std::move(segments),
                                PathClosure::Closed, PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
  return PathCurveId("path.fill");
}

} // namespace

TEST_CASE("Block 89 builds an open surface between two boundaries",
          "[geometry][surface-boundary-fill]") {
  const std::vector<std::vector<SweepPathSegment>> boundaries = {
      {{SweepPathSegmentKind::Line, {0.0, 0.0, 0.0}, {}, {10.0, 0.0, 0.0}}},
      {{SweepPathSegmentKind::Line, {0.0, 8.0, 2.0}, {}, {10.0, 8.0, 2.0}}}};
  auto surface =
      SurfaceAdapter{}.make_boundary_surface(FeatureId("feature.two-boundary"), boundaries);
  REQUIRE(surface);
  CHECK_FALSE(surface.value().empty());
  CHECK(RectangleExtrusionAdapter{}.summarize(surface.value()).solid_count == 0U);
}

TEST_CASE("Block 89 builds a surface from four spatial boundaries",
          "[geometry][surface-boundary-fill]") {
  PartDocument document = empty_surface_document("part.surface.boundary");
  add_spatial_boundary_network(document);
  add_surface_body(document, "body.surface");
  auto feature = BoundarySurfaceFeature::create(
      FeatureId("feature.surface"), "Boundary surface",
      {boundary("path.0"), boundary("path.1"), boundary("path.2"), boundary("path.3")},
      BodyId("body.surface"));
  REQUIRE(feature);
  REQUIRE(document.add_surface_feature(feature.value()));

  ShapeCache shapes = cache();
  auto executed = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  REQUIRE(executed);
  CHECK(executed.value().executed_feature_count == 1U);
  const GeometryShape* body_shape = shapes.find_body_shape(BodyId("body.surface"));
  REQUIRE(body_shape != nullptr);
  CHECK_FALSE(body_shape->empty());
  CHECK(shapes.find_feature_shape(FeatureId("feature.surface")) != nullptr);
  CHECK(RectangleExtrusionAdapter{}.summarize(*body_shape).solid_count == 0U);
  CHECK(shapes.final_shape() == nullptr);
}

TEST_CASE("Block 89 fills one supported closed planar boundary",
          "[geometry][surface-boundary-fill]") {
  PartDocument document = empty_surface_document("part.surface.fill");
  const PathCurveId fill_path = add_planar_rectangle(document);
  add_surface_body(document, "body.fill");
  auto feature = FillSurfaceFeature::create(FeatureId("feature.fill"), "Fill surface",
                                            {boundary(fill_path.value())}, BodyId("body.fill"));
  REQUIRE(feature);
  REQUIRE(document.add_surface_feature(feature.value()));

  ShapeCache shapes = cache();
  auto executed = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  REQUIRE(executed);
  const GeometryShape* body_shape = shapes.find_body_shape(BodyId("body.fill"));
  REQUIRE(body_shape != nullptr);
  CHECK_FALSE(body_shape->empty());
  CHECK(RectangleExtrusionAdapter{}.summarize(*body_shape).solid_count == 0U);
}

TEST_CASE("Block 89 rejects an open fill contour transactionally",
          "[geometry][surface-boundary-fill]") {
  PartDocument document = empty_surface_document("part.surface.invalid-fill");
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  for (std::size_t index = 0; index < 2U; ++index) {
    const std::string suffix = std::to_string(index);
    auto sketch =
        Sketch::create(SketchId("sketch." + suffix), "Open boundary", DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto line = LineSegment::create(SketchEntityId("line." + suffix),
                                    {0.0, 2.0 * static_cast<double>(index)},
                                    {10.0, 2.0 * static_cast<double>(index)});
    REQUIRE(line);
    REQUIRE(sketch.value().add_entity(line.value()));
    REQUIRE(document.add_sketch(sketch.value()));
    auto segment = PathSegmentReference::create_planar(
        SketchId("sketch." + suffix), SketchEntityId("line." + suffix), PlanarPathCurveKind::Line);
    REQUIRE(segment);
    auto path = PathCurve::create(PathCurveId("path." + suffix), "Open boundary", {segment.value()},
                                  PathClosure::Open, PathOrientationRule::MinimumTwist);
    REQUIRE(path);
    REQUIRE(document.add_path_curve(path.value()));
  }
  add_surface_body(document, "body.invalid");
  auto feature =
      FillSurfaceFeature::create(FeatureId("feature.invalid"), "Invalid fill",
                                 {boundary("path.0"), boundary("path.1")}, BodyId("body.invalid"));
  REQUIRE(feature);
  REQUIRE(document.add_surface_feature(feature.value()));

  ShapeCache shapes = cache();
  auto executed = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  REQUIRE_FALSE(executed);
  CHECK(shapes.find_body_shape(BodyId("body.invalid")) == nullptr);
  CHECK(shapes.find_feature_shape(FeatureId("feature.invalid")) == nullptr);
}
