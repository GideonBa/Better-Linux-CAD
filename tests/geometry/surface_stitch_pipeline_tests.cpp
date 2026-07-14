#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/surface_adapter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
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
  auto quantity = Quantity::linear_displacement_mm(value, "stitch coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("surface.stitch.cache"));
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

// Registers a closed rectangular boundary as a 3D-sketch PathCurve and returns
// its PathCurveId string.
std::string closed_rectangle(PartDocument& document, const std::string& tag,
                             const std::array<std::array<double, 3>, 4>& corners) {
  const Sketch3DId sketch_id("sketch3d." + tag);
  auto sketch = Sketch3D::create(sketch_id, tag);
  REQUIRE(sketch);
  for (std::size_t index = 0U; index < corners.size(); ++index) {
    auto point = SketchPoint3D::create(
        SketchEntityId(tag + ".p" + std::to_string(index)), coordinate(corners[index][0]),
        coordinate(corners[index][1]), coordinate(corners[index][2]));
    REQUIRE(point);
    REQUIRE(sketch.value().add_point(point.value()));
  }
  std::vector<PathSegmentReference> segments;
  for (std::size_t index = 0U; index < corners.size(); ++index) {
    const SketchEntityId line_id(tag + ".line." + std::to_string(index));
    auto line = SketchLine3D::create(
        line_id, SketchEntityId(tag + ".p" + std::to_string(index)),
        SketchEntityId(tag + ".p" + std::to_string((index + 1U) % corners.size())));
    REQUIRE(line);
    REQUIRE(sketch.value().add_line(line.value()));
    auto segment =
        PathSegmentReference::create_sketch_3d(sketch_id, line_id, Sketch3DPathCurveKind::Line);
    REQUIRE(segment);
    segments.push_back(segment.value());
  }
  REQUIRE(document.add_sketch_3d(sketch.value()));
  const PathCurveId path_id("path." + tag);
  auto path = PathCurve::create(path_id, tag, segments, PathClosure::Closed,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
  return path_id.value();
}

// Two coplanar quads that share the x = 10 edge, plus a stitch over both.
PartDocument stitch_document() {
  auto created = PartDocument::create(DocumentId("part.surface.stitch"), "Stitch surface");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_parameter(length_parameter("stitch.tolerance", 0.001)));

  const std::string left = closed_rectangle(
      document, "left", {{{{0.0, 0.0, 0.0}}, {{10.0, 0.0, 0.0}}, {{10.0, 10.0, 0.0}},
                          {{0.0, 10.0, 0.0}}}});
  const std::string right = closed_rectangle(
      document, "right", {{{{10.0, 0.0, 0.0}}, {{20.0, 0.0, 0.0}}, {{20.0, 10.0, 0.0}},
                           {{10.0, 10.0, 0.0}}}});

  add_body(document, "body.left", BodyKind::Surface);
  add_body(document, "body.right", BodyKind::Surface);
  add_body(document, "body.shell", BodyKind::Surface);

  auto fill_left = FillSurfaceFeature::create(FeatureId("feature.left"), "Left", {boundary(left)},
                                              BodyId("body.left"));
  REQUIRE(fill_left);
  REQUIRE(document.add_surface_feature(fill_left.value()));
  auto fill_right = FillSurfaceFeature::create(FeatureId("feature.right"), "Right",
                                               {boundary(right)}, BodyId("body.right"));
  REQUIRE(fill_right);
  REQUIRE(document.add_surface_feature(fill_right.value()));

  auto stitch = SurfaceStitchFeature::create(
      FeatureId("feature.stitch"), "Stitch",
      {body_surface("body.left"), body_surface("body.right")}, ParameterId("stitch.tolerance"),
      BodyId("body.shell"));
  REQUIRE(stitch);
  REQUIRE(document.add_surface_feature(stitch.value()));
  return document;
}

std::vector<SweepPathSegment> rectangle_boundary(double x0, double x1, double y0, double y1,
                                                 double z) {
  return {{SweepPathSegmentKind::Line, {x0, y0, z}, {}, {x1, y0, z}},
          {SweepPathSegmentKind::Line, {x1, y0, z}, {}, {x1, y1, z}},
          {SweepPathSegmentKind::Line, {x1, y1, z}, {}, {x0, y1, z}},
          {SweepPathSegmentKind::Line, {x0, y1, z}, {}, {x0, y0, z}}};
}

GeometryShape fill_face(const std::string& id, double x0, double x1) {
  auto face = SurfaceAdapter{}.make_fill_surface(FeatureId(id),
                                                 {rectangle_boundary(x0, x1, 0.0, 10.0, 0.0)});
  REQUIRE(face);
  return face.value();
}

} // namespace

TEST_CASE("Block 91 stitches two adjacent Surface bodies into one shell and recomputes",
          "[geometry][surface-stitch][integration][part-construction-mvp]") {
  PartDocument document = stitch_document();
  ShapeCache shapes = cache();
  auto first = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO(
      (first.has_error() ? first.error().object_id() + ": " + first.error().message() : "success"));
  REQUIRE(first);
  const GeometryShape* shell = shapes.find_body_shape(BodyId("body.shell"));
  REQUIRE(shell != nullptr);
  CHECK_FALSE(shell->empty());
  auto bounds = BodyTransformAdapter{}.bounds(*shell);
  REQUIRE(bounds);
  // The stitched shell spans both quads: filling bulges the boundary slightly,
  // so assert the shell reaches into the left and right extents rather than an
  // exact edge coordinate.
  CHECK(bounds.value().minimum.x < 1.0);
  CHECK(bounds.value().maximum.x > 19.0);

  auto looser = Quantity::length_mm(0.01, "looser stitch tolerance");
  REQUIRE(looser);
  REQUIRE(document.set_parameter_value(ParameterId("stitch.tolerance"), looser.value()));
  auto second = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO((second.has_error() ? second.error().object_id() + ": " + second.error().message()
                           : "success"));
  REQUIRE(second);
  REQUIRE(shapes.find_body_shape(BodyId("body.shell")) != nullptr);
  CHECK(shapes.find_feature_shape(FeatureId("feature.stitch")) != nullptr);
}

TEST_CASE("Block 91 stitch adapter joins edge-sharing faces into one connected shell",
          "[geometry][surface-stitch]") {
  const GeometryShape left = fill_face("feature.left", 0.0, 10.0);
  const GeometryShape right = fill_face("feature.right", 10.0, 20.0);
  auto result = SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), {left, right}, 0.001);
  INFO((result.has_error() ? result.error().message() : "success"));
  REQUIRE(result);
  auto bounds = BodyTransformAdapter{}.bounds(result.value());
  REQUIRE(bounds);
  CHECK(bounds.value().minimum.x < 1.0);
  CHECK(bounds.value().maximum.x > 19.0);
}

TEST_CASE("Block 91 stitch fails closed when surfaces do not connect within tolerance",
          "[geometry][surface-stitch]") {
  const GeometryShape left = fill_face("feature.left", 0.0, 10.0);
  const GeometryShape far = fill_face("feature.far", 50.0, 60.0);
  auto result = SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), {left, far}, 0.001);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("connected shell") != std::string::npos);
}

TEST_CASE("Block 91 stitch rejects a single surface and a non-positive tolerance",
          "[geometry][surface-stitch]") {
  const GeometryShape left = fill_face("feature.left", 0.0, 10.0);
  const GeometryShape right = fill_face("feature.right", 10.0, 20.0);
  auto too_few = SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), {left}, 0.001);
  REQUIRE_FALSE(too_few);
  CHECK(too_few.error().message().find("at least two") != std::string::npos);
  auto bad_tolerance =
      SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), {left, right}, 0.0);
  REQUIRE_FALSE(bad_tolerance);
  CHECK(bad_tolerance.error().message().find("positive") != std::string::npos);
}
