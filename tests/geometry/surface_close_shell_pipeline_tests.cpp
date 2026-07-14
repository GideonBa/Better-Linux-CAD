#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/surface_adapter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

using Corner = std::array<double, 3>;

Parameter length_parameter(const std::string& id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "shell coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

ShapeCache cache() {
  auto result = ShapeCache::create(ShapeCacheId("surface.close-shell.cache"));
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

// One straight boundary segment between two corners.
std::vector<SweepPathSegment> edge(Corner p0, Corner p1) {
  return {{SweepPathSegmentKind::Line, {p0[0], p0[1], p0[2]}, {}, {p1[0], p1[1], p1[2]}}};
}

// A flat quad face lofted between two opposite box edges.
GeometryShape quad(const std::string& id, Corner a0, Corner a1, Corner b0, Corner b1) {
  auto face = SurfaceAdapter{}.make_boundary_surface(FeatureId(id), {edge(a0, a1), edge(b0, b1)});
  REQUIRE(face);
  return face.value();
}

// The six flat faces of the axis-aligned box [0,10]^3.
std::vector<GeometryShape> unit_box_faces() {
  return {quad("f.bottom", {0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {10, 10, 0}),
          quad("f.top", {0, 0, 10}, {10, 0, 10}, {0, 10, 10}, {10, 10, 10}),
          quad("f.front", {0, 0, 0}, {10, 0, 0}, {0, 0, 10}, {10, 0, 10}),
          quad("f.back", {0, 10, 0}, {10, 10, 0}, {0, 10, 10}, {10, 10, 10}),
          quad("f.left", {0, 0, 0}, {0, 10, 0}, {0, 0, 10}, {0, 10, 10}),
          quad("f.right", {10, 0, 0}, {10, 10, 0}, {10, 0, 10}, {10, 10, 10})};
}

// Registers an open straight-line boundary as a 3D-sketch PathCurve.
std::string open_line(PartDocument& document, const std::string& tag, Corner p0, Corner p1) {
  const Sketch3DId sketch_id("sketch3d." + tag);
  auto sketch = Sketch3D::create(sketch_id, tag);
  REQUIRE(sketch);
  auto start = SketchPoint3D::create(SketchEntityId(tag + ".a"), coordinate(p0[0]), coordinate(p0[1]),
                                     coordinate(p0[2]));
  auto finish = SketchPoint3D::create(SketchEntityId(tag + ".b"), coordinate(p1[0]),
                                      coordinate(p1[1]), coordinate(p1[2]));
  REQUIRE(start);
  REQUIRE(finish);
  REQUIRE(sketch.value().add_point(start.value()));
  REQUIRE(sketch.value().add_point(finish.value()));
  auto line = SketchLine3D::create(SketchEntityId(tag + ".l"), SketchEntityId(tag + ".a"),
                                   SketchEntityId(tag + ".b"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_line(line.value()));
  auto segment = PathSegmentReference::create_sketch_3d(sketch_id, SketchEntityId(tag + ".l"),
                                                        Sketch3DPathCurveKind::Line);
  REQUIRE(segment);
  REQUIRE(document.add_sketch_3d(sketch.value()));
  const PathCurveId path_id("path." + tag);
  auto path = PathCurve::create(path_id, tag, {segment.value()}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
  return path_id.value();
}

// A BoundarySurface feature producing one flat box face, plus its Surface Body.
void box_face(PartDocument& document, const std::string& tag, const std::string& body, Corner a0,
              Corner a1, Corner b0, Corner b1) {
  const std::string first = open_line(document, tag + ".A", a0, a1);
  const std::string second = open_line(document, tag + ".B", b0, b1);
  add_body(document, body, BodyKind::Surface);
  auto feature = BoundarySurfaceFeature::create(FeatureId("feature." + tag), tag,
                                                {boundary(first), boundary(second)}, BodyId(body));
  REQUIRE(feature);
  REQUIRE(document.add_surface_feature(feature.value()));
}

// Full pipeline: six boundary faces -> stitched shell -> closed-shell solid.
PartDocument close_document() {
  auto created = PartDocument::create(DocumentId("part.surface.close"), "Closed shell to solid");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_parameter(length_parameter("stitch.tolerance", 0.0001)));

  box_face(document, "bottom", "body.bottom", {0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {10, 10, 0});
  box_face(document, "top", "body.top", {0, 0, 10}, {10, 0, 10}, {0, 10, 10}, {10, 10, 10});
  box_face(document, "front", "body.front", {0, 0, 0}, {10, 0, 0}, {0, 0, 10}, {10, 0, 10});
  box_face(document, "back", "body.back", {0, 10, 0}, {10, 10, 0}, {0, 10, 10}, {10, 10, 10});
  box_face(document, "left", "body.left", {0, 0, 0}, {0, 10, 0}, {0, 0, 10}, {0, 10, 10});
  box_face(document, "right", "body.right", {10, 0, 0}, {10, 10, 0}, {10, 0, 10}, {10, 10, 10});

  add_body(document, "body.shell", BodyKind::Surface);
  auto stitch = SurfaceStitchFeature::create(
      FeatureId("feature.stitch"), "Stitch",
      {body_surface("body.bottom"), body_surface("body.top"), body_surface("body.front"),
       body_surface("body.back"), body_surface("body.left"), body_surface("body.right")},
      ParameterId("stitch.tolerance"), BodyId("body.shell"));
  REQUIRE(stitch);
  REQUIRE(document.add_surface_feature(stitch.value()));

  add_body(document, "body.solid", BodyKind::Solid);
  auto solid = ClosedShellToSolidFeature::create(FeatureId("feature.solid"), "Solidify",
                                                 body_surface("body.shell"), BodyId("body.solid"));
  REQUIRE(solid);
  REQUIRE(document.add_surface_feature(solid.value()));
  return document;
}

} // namespace

TEST_CASE("Block 92 converts a stitched closed box shell into one solid body",
          "[geometry][surface-to-solid][integration][part-construction-mvp]") {
  PartDocument document = close_document();
  ShapeCache shapes = cache();
  auto executed = GeometryRecomputeExecutor{}.execute_document(document, shapes);
  INFO((executed.has_error() ? executed.error().object_id() + ": " + executed.error().message()
                             : "success"));
  REQUIRE(executed);

  const GeometryShape* solid = shapes.find_body_shape(BodyId("body.solid"));
  REQUIRE(solid != nullptr);
  CHECK_FALSE(solid->empty());

  auto inspection = BodyResultInspector{}.inspect(document, shapes);
  REQUIRE(inspection);
  CHECK(inspection.value().solid_body_count == 1U);
  const auto& bodies = inspection.value().bodies;
  const auto entry = std::find_if(bodies.begin(), bodies.end(), [](const BodyResultInspection& b) {
    return b.body_id == BodyId("body.solid");
  });
  REQUIRE(entry != bodies.end());
  REQUIRE(entry->shape_summary.has_value());
  CHECK(entry->shape_summary->solid_count == 1U);
  CHECK(entry->shape_summary->volume_mm3 == Catch::Approx(1000.0).margin(1.0));
}

TEST_CASE("Block 92 closes a stitched box shell directly through the adapter",
          "[geometry][surface-to-solid]") {
  auto shell = SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), unit_box_faces(),
                                                0.0001);
  INFO((shell.has_error() ? shell.error().message() : "success"));
  REQUIRE(shell);
  auto solid = SurfaceAdapter{}.close_shell_to_solid(FeatureId("feature.solid"), shell.value());
  INFO((solid.has_error() ? solid.error().message() : "success"));
  REQUIRE(solid);
  auto bounds = BodyTransformAdapter{}.bounds(solid.value());
  REQUIRE(bounds);
  CHECK(bounds.value().minimum.x == Catch::Approx(0.0).margin(1.0e-6));
  CHECK(bounds.value().maximum.z == Catch::Approx(10.0).margin(1.0e-6));
}

TEST_CASE("Block 92 rejects an open (non-closed) shell", "[geometry][surface-to-solid]") {
  const std::vector<GeometryShape> two = {
      quad("f.a", {0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {10, 10, 0}),
      quad("f.b", {10, 0, 0}, {20, 0, 0}, {10, 10, 0}, {20, 10, 0})};
  auto shell = SurfaceAdapter{}.stitch_surfaces(FeatureId("feature.stitch"), two, 0.0001);
  REQUIRE(shell);
  auto solid = SurfaceAdapter{}.close_shell_to_solid(FeatureId("feature.solid"), shell.value());
  REQUIRE_FALSE(solid);
  CHECK(solid.error().message().find("free edges") != std::string::npos);
}

TEST_CASE("Block 92 rejects a lone face that is not a connected shell",
          "[geometry][surface-to-solid]") {
  const std::vector<std::vector<SweepPathSegment>> rectangle = {
      {{SweepPathSegmentKind::Line, {0, 0, 0}, {}, {10, 0, 0}},
       {SweepPathSegmentKind::Line, {10, 0, 0}, {}, {10, 10, 0}},
       {SweepPathSegmentKind::Line, {10, 10, 0}, {}, {0, 10, 0}},
       {SweepPathSegmentKind::Line, {0, 10, 0}, {}, {0, 0, 0}}}};
  auto face = SurfaceAdapter{}.make_fill_surface(FeatureId("feature.lone"), rectangle);
  REQUIRE(face);
  auto solid = SurfaceAdapter{}.close_shell_to_solid(FeatureId("feature.solid"), face.value());
  REQUIRE_FALSE(solid);
  CHECK(solid.error().message().find("one connected shell") != std::string::npos);
}
