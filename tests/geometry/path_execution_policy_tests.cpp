#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/geometry/loft_section_compatibility.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;

TEST_CASE("Curve-preserving execution certifies the upgraded source without rewriting its inputs",
          "[geometry][parity-b166][path-execution-policy]") {
  auto part = sf::straight_sweep();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("policy")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto previous = part;
  const auto count = cache.feature_shape_count();
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  const auto volume = RectangleExtrusionAdapter{}
                          .summarize(*cache.find_body_shape(BodyId("driven.body")))
                          .volume_mm3;
  CHECK(volume == Catch::Approx(6.4).margin(1e-6));
  REQUIRE(part.set_path_execution_policy(PathCurveId("driven.path"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-4))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE(result);
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  CHECK(cache.feature_shape_count() == count);
  REQUIRE(cache.find_body_shape(BodyId("driven.body")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("driven.body")))
            .volume_mm3 == Catch::Approx(volume).margin(1e-12));
  CHECK_FALSE(cache.validate_complete_geometry_result(previous));
  CHECK(cache.validate_complete_geometry_result(part));
}
namespace {
PathExecutionPolicy policy(double tolerance = 1e-5) {
  return f::checked(PathExecutionPolicy::curve_preserving(tolerance));
}
PathCurve spatial_path(const char* id, Sketch3DId owner, const char* entity,
                       Sketch3DPathCurveKind kind, bool reverse = false) {
  return f::checked(PathCurve::create(PathCurveId(id), id,
                                      {f::checked(PathSegmentReference::create_sketch_3d(
                                          owner, SketchEntityId(entity), kind, reverse))},
                                      PathClosure::Open, PathOrientationRule::MinimumTwist));
}
GeometryShape path_shape(const PartDocument& p, const char* id) {
  const auto cache = f::checked(ShapeCache::create(ShapeCacheId("path.query")));
  const auto& path = *p.find_path_curve(PathCurveId(id));
  auto segments =
      f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(p, path.id(), cache));
  return f::checked(
      SweepAdapter{}.path_shape(path.id(), segments, path.closure() == PathClosure::Closed));
}
BRepAdaptor_Curve first_curve(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  TopExp_Explorer edges(*native, TopAbs_EDGE);
  REQUIRE(edges.More());
  const auto edge = TopoDS::Edge(edges.Current());
  edges.Next();
  CHECK_FALSE(edges.More());
  return BRepAdaptor_Curve(edge);
}
void on_curve(const GeometryShape& shape, Point3 p, double tolerance) {
  auto vertex = BRepBuilderAPI_MakeVertex(gp_Pnt(p.x, p.y, p.z)).Vertex();
  BRepExtrema_DistShapeShape distance(vertex, *OcctShapeView::get(shape));
  REQUIRE(distance.IsDone());
  CHECK(distance.Value() <= tolerance);
}
void parameterized_boundary_witness(const GeometryShape& shape, Point3 expected, double parameter,
                                    double tolerance) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  double nearest = std::numeric_limits<double>::infinity();
  for (TopExp_Explorer edges(*native, TopAbs_EDGE); edges.More(); edges.Next()) {
    const BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
    if (parameter >= curve.FirstParameter() && parameter <= curve.LastParameter())
      nearest = std::min(
          nearest, curve.Value(parameter).Distance(gp_Pnt(expected.x, expected.y, expected.z)));
  }
  // Query actual output edges at the independently known parameter. This also proves
  // the witness when OCCT's nearest-point solver misses its zero-distance root.
  CHECK(nearest <= tolerance);
}
double curve_length(const BRepAdaptor_Curve& curve) {
  return GCPnts_AbscissaPoint::Length(curve, curve.FirstParameter(), curve.LastParameter(), 1e-9);
}
} // namespace
TEST_CASE("G01 explicit upgrade preserves a rational quarter circle beyond historical chords",
          "[geometry][parity-b166][native-path-precision]") {
  auto part = f::part();
  const double c = std::sqrt(.5), w = (1 + 2 * c) / 3, h = 8 * c / (1 + 2 * c);
  auto sketch =
      f::checked(Sketch::create(SketchId("rational"), "Rational", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_entity(f::checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("circle"), {4, 0}, {4, h}, {h, 4}, {0, 4}, {1, w, w, 1}))));
  REQUIRE(part.add_sketch(sketch));
  auto reference = f::checked(PathSegmentReference::create_planar(
      sketch.id(), SketchEntityId("circle"), PlanarPathCurveKind::Spline));
  auto path =
      f::checked(PathCurve::create(PathCurveId("rational.path"), "Rational path", {reference},
                                   PathClosure::Open, PathOrientationRule::MinimumTwist));
  REQUIRE(part.add_path_curve(path));
  const auto old = f::checked(serialize_part_document_to_json(part));
  const auto cache = f::checked(ShapeCache::create(ShapeCacheId("rational.query")));
  const auto chords =
      f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(part, path.id(), cache));
  REQUIRE(chords.size() == 32);
  double chord_length = 0;
  for (const auto& s : chords)
    chord_length += distance(s.start, s.end);
  CHECK(chord_length < 2 * std::numbers::pi - 1e-4);
  REQUIRE(part.set_path_execution_policy(path.id(), policy(1e-8)));
  const auto shape = path_shape(part, "rational.path");
  auto curve = first_curve(shape);
  CHECK(curve.GetType() == GeomAbs_BezierCurve);
  CHECK(curve_length(curve) == Catch::Approx(2 * std::numbers::pi).margin(1e-8));
  for (int i = 0; i <= 80; ++i) {
    const double a = std::numbers::pi * i / 160.;
    on_curve(shape, {4 * std::cos(a), 4 * std::sin(a), 0}, 1e-8);
  }
  gp_Pnt p;
  gp_Vec tangent;
  curve.D1(curve.FirstParameter(), p, tangent);
  CHECK(tangent.X() == Catch::Approx(0).margin(1e-10));
  CHECK(tangent.Y() > 0);
  CHECK(f::checked(serialize_part_document_to_json(part)).find("execution_policy") !=
        std::string::npos);
  REQUIRE(part.set_path_execution_policy(path.id(), PathExecutionPolicy::legacy()));
  CHECK(f::checked(serialize_part_document_to_json(part)) == old);
}
TEST_CASE("G01 spatial paths reuse the actual native control and fit splines with stable reversal",
          "[geometry][parity-b166][native-path-precision]") {
  auto part = f::part();
  auto owner = f::sketch();
  REQUIRE(part.add_sketch_3d(owner));
  auto native = f::checked(Sketch3DGeometryAdapter{}.convert(part, owner.id()));
  for (const auto* entity : {"spline", "fit"}) {
    CAPTURE(entity);
    auto path = spatial_path(entity, owner.id(), entity, Sketch3DPathCurveKind::Spline, true);
    REQUIRE(part.add_path_curve(path));
    REQUIRE(part.set_path_execution_policy(path.id(), policy()));
    const auto shape = path_shape(part, entity);
    auto actual = first_curve(shape);
    auto source = first_curve(native.find_product(SketchEntityId(entity))->shape);
    CHECK(actual.GetType() == GeomAbs_BSplineCurve);
    CHECK(curve_length(actual) == Catch::Approx(curve_length(source)).margin(1e-8));
    for (int i = 0; i <= 60; ++i) {
      const double t = double(i) / 60;
      auto a = actual.Value(std::lerp(actual.FirstParameter(), actual.LastParameter(), t));
      auto b = source.Value(std::lerp(source.LastParameter(), source.FirstParameter(), t));
      CHECK(a.Distance(b) < 1e-8);
    }
    gp_Pnt a, b;
    gp_Vec ta, tb;
    actual.D1(actual.FirstParameter(), a, ta);
    source.D1(source.LastParameter(), b, tb);
    CHECK(ta.Normalized().Dot(tb.Normalized()) == Catch::Approx(-1).margin(1e-9));
  }
}
TEST_CASE("G01 bounded Helix approximation meets independent distance tangent and length oracles",
          "[geometry][parity-b166][native-path-precision]") {
  for (bool right : {true, false}) {
    auto part = f::part();
    auto owner = f::checked(Sketch3D::create(Sketch3DId("helix.owner"), "Helix"));
    REQUIRE(owner.add_helix(f::checked(SketchHelix3D::create(
        SketchEntityId("helix"),
        f::checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z"))),
        ParameterId("radius"), ParameterId("pitch"), ParameterId("turns"),
        right ? SketchHelix3DHandedness::RightHanded : SketchHelix3DHandedness::LeftHanded))));
    REQUIRE(part.add_sketch_3d(owner));
    auto path = spatial_path("helix.path", owner.id(), "helix", Sketch3DPathCurveKind::Helix);
    REQUIRE(part.add_path_curve(path));
    const auto cache = f::checked(ShapeCache::create(ShapeCacheId("helix.query")));
    auto old =
        f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(part, path.id(), cache));
    REQUIRE(old.size() == 144);
    for (double tol : {1e-4, 1e-7}) {
      REQUIRE(part.set_path_execution_policy(path.id(), policy(tol)));
      const auto shape = path_shape(part, "helix.path");
      auto curve = first_curve(shape);
      CHECK(curve_length(curve) ==
            Catch::Approx(3 * std::hypot(8 * std::numbers::pi, 2.)).margin(20 * tol));
      CHECK(curve.Value(curve.FirstParameter())
                .Distance(gp_Pnt(old.front().start.x, old.front().start.y, old.front().start.z)) <
            1e-8);
      for (int i = 0; i <= 180; ++i) {
        const double t = double(i) / 180, a = (right ? 1 : -1) * 6 * std::numbers::pi * t;
        on_curve(shape, {-4 * std::cos(a), -4 * std::sin(a), 6 * t}, tol);
        gp_Pnt p;
        gp_Vec v;
        curve.D1(std::lerp(curve.FirstParameter(), curve.LastParameter(), t), p, v);
        const gp_Vec expected((right ? 1 : -1) * 4 * std::sin(a),
                              -(right ? 1 : -1) * 4 * std::cos(a), 1 / std::numbers::pi);
        CHECK(v.Normalized().Dot(expected.Normalized()) > 1 - 20 * tol);
      }
    }
    REQUIRE(part.set_parameter_value(ParameterId("turns"), f::checked(Quantity::count(129))));
    const auto rejected = GeometryRecomputeExecutor{}.resolve_path_segments(part, path.id(), cache);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().message().find("128") != std::string::npos);
  }
}

TEST_CASE("G01 singular upgraded paths retain the previously certified consumer",
          "[geometry][parity-b166][native-path-precision]") {
  auto part = sf::straight_sweep();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("singular.cache")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto certified = part;
  auto owner = f::checked(Sketch3D::create(Sketch3DId("singular"), "Singular"));
  REQUIRE(owner.add_point(f::point("a", {0, 0, 0})));
  REQUIRE(owner.add_point(f::point("b", {0, 0, 0})));
  REQUIRE(owner.add_point(f::point("c", {0, 0, 5})));
  REQUIRE(owner.add_point(f::point("d", {0, 0, 10})));
  REQUIRE(owner.add_spline(f::checked(
      SketchSpline3D::create(SketchEntityId("curve"), SketchSpline3DRepresentation::ControlPoints,
                             {f::local("a"), f::local("b"), f::local("c"), f::local("d")}, 3,
                             SketchSpline3DContinuity::Curvature))));
  REQUIRE(part.add_sketch_3d(owner));
  REQUIRE(part.add_path_curve(
      spatial_path("singular.path", owner.id(), "curve", Sketch3DPathCurveKind::Spline)));
  REQUIRE(part.set_path_execution_policy(PathCurveId("singular.path"), policy()));
  const auto before = f::checked(serialize_part_document_to_json(part));
  auto query_cache = f::checked(ShapeCache::create(ShapeCacheId("singular.query")));
  const auto path = GeometryRecomputeExecutor{}.resolve_path_segments(
      part, PathCurveId("singular.path"), query_cache);
  REQUIRE_FALSE(path);
  CHECK(path.error().message().find("singular") != std::string::npos);
  const auto execution = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(execution);
  CHECK(execution.error().message().find("singular") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  CHECK(cache.validate_complete_geometry_result(certified));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("driven.body")))
            .volume_mm3 == Catch::Approx(6.4).margin(1e-6));
}

TEST_CASE("G01 native Loft sections refine unequal counts without changing the source curve",
          "[geometry][parity-b166][native-path-precision]") {
  auto part = f::part();
  auto owner = f::sketch();
  REQUIRE(part.add_sketch_3d(owner));
  auto path = spatial_path("section", owner.id(), "spline", Sketch3DPathCurveKind::Spline);
  REQUIRE(part.add_path_curve(path));
  REQUIRE(part.set_path_execution_policy(path.id(), policy()));
  const auto cache = f::checked(ShapeCache::create(ShapeCacheId("section.query")));
  const auto section =
      f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(part, path.id(), cache));
  const std::vector<SweepPathSegment> upper{
      {SweepPathSegmentKind::Line, {8, 0, 21}, {}, {4, 1.5, 21.5}},
      {SweepPathSegmentKind::Line, {4, 1.5, 21.5}, {}, {0, 3, 22}}};
  const auto refined =
      f::checked(make_loft_sections_compatible(FeatureId("refine"), {section, upper}));
  REQUIRE(refined.adjusted);
  REQUIRE(refined.sections[0].size() == 2);
  CHECK(refined.source_vertices[0] == std::vector<std::size_t>{0, 2});
  for (const auto& segment : refined.sections[0])
    CHECK(segment.kind == SweepPathSegmentKind::DerivedNative);
  const auto refined_shape =
      f::checked(SweepAdapter{}.path_shape(path.id(), refined.sections[0], false));
  const auto loft =
      f::checked(LoftAdapter{}.loft_open_sections(FeatureId("native.loft"), refined.sections));
  const auto before = f::checked(serialize_part_document_to_json(part));
  for (int i = 0; i <= 80; ++i) {
    CAPTURE(i);
    const double t = double(i) / 80, u = 1 - t;
    // Independent cubic Bernstein evaluation, including the middle of each native span.
    const Point3 p{8 * u * u * u + 30 * u * u * t + 6 * u * t * t, 3 * u * t * t + 3 * t * t * t,
                   u * u * u + 3 * u * u * t + 2 * t * t * t};
    parameterized_boundary_witness(refined_shape, p, t, 1e-8);
    parameterized_boundary_witness(loft, p, t, 2e-6);
  }
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
}
