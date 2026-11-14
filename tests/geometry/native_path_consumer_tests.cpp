#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
const double route_length = 10 + 10 * std::numbers::pi;
PathExecutionPolicy native_policy() {
  return f::checked(PathExecutionPolicy::curve_preserving(1e-6));
}
void mixed_route(PartDocument& part, const std::string& id, double y) {
  for (const auto& [name, z] : {std::pair{"start", 0.}, {"join", 5.}})
    (void)f::checked(part.add_construction_point(f::checked(
        ConstructionPoint::create_explicit(ConstructionPointId(id + name), name, {0, y, z}))));
  auto line = f::checked(ConstructionLine::create_through_two_points(
      ConstructionLineId(id + ".line"), "Lead-in",
      f::checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId(id + ".relation"), ConstructionPointId(id + "start"),
          ConstructionPointId(id + "join")))));
  (void)f::checked(part.add_construction_line(line));
  (void)f::checked(part.add_construction_plane(f::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId(id + ".plane"), "Route plane",
                                         {0, y, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
  auto sketch = f::checked(
      Sketch::create(SketchId(id + ".planar"), "Rational return", DatumPlaneId(id + ".plane")));
  const double c = std::sqrt(.5), w = (1 + 2 * c) / 3, h = 40 * c / (1 + 2 * c);
  // Stored backwards: the path's explicit reversal restores the forward quarter-circle.
  (void)f::checked(sketch.add_entity(f::checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("arc"), {20, 25}, {20 - h, 25}, {0, 5 + h}, {0, 5}, {1, w, w, 1}))));
  (void)f::checked(part.add_sketch(sketch));
  auto spatial = f::checked(Sketch3D::create(Sketch3DId(id + ".spatial"), "Native tail"));
  for (const auto& [name, x] : {std::pair{"a", 20.}, {"b", 21.}, {"c", 24.}})
    (void)f::checked(spatial.add_point(f::point(name, {x, y, 25})));
  (void)f::checked(spatial.add_point(f::checked(SketchPoint3D::create(
      SketchEntityId("d"),
      f::checked(SketchCoordinate3D::create_parameter(ParameterId("tail.end"))), f::coordinate(y),
      f::coordinate(25)))));
  (void)f::checked(spatial.add_spline(f::checked(
      SketchSpline3D::create(SketchEntityId("tail"), SketchSpline3DRepresentation::ControlPoints,
                             {f::local("a"), f::local("b"), f::local("c"), f::local("d")}, 3,
                             SketchSpline3DContinuity::Curvature))));
  (void)f::checked(part.add_sketch_3d(spatial));
  (void)f::checked(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId(id), "Mixed route",
      {f::checked(PathSegmentReference::create_construction_line(line.id())),
       f::checked(PathSegmentReference::create_planar(sketch.id(), SketchEntityId("arc"),
                                                      PlanarPathCurveKind::Spline, true)),
       f::checked(PathSegmentReference::create_sketch_3d(spatial.id(), SketchEntityId("tail"),
                                                         Sketch3DPathCurveKind::Spline))},
      PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7, native_policy()))));
}
PartDocument route_inputs() {
  auto part = f::part();
  for (const auto& [id, value] : {std::pair{"width", 80.}, {"height", 80.}, {"depth", 40.}})
    (void)f::checked(
        part.set_parameter_value(ParameterId(id), f::checked(Quantity::length_mm(value))));
  for (const auto& [id, value] :
       {std::pair{"tail.end", 25.}, {"section.width", .8}, {"section.height", .4}})
    (void)f::checked(part.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  mixed_route(part, "route", 0);
  mixed_route(part, "guide", 1);
  auto section =
      f::checked(Sketch::create(SketchId("section"), "Section", DatumPlaneId("datum.xy")));
  (void)f::checked(section.add_profile(f::checked(RectangleProfile::create(
      ProfileId("rectangle"), ParameterId("section.width"), ParameterId("section.height")))));
  (void)f::checked(part.add_sketch(section));
  auto open =
      f::checked(Sketch::create(SketchId("open"), "Open section", DatumPlaneId("datum.xy")));
  (void)f::checked(
      open.add_entity(f::checked(LineSegment::create(SketchEntityId("open"), {-.4, 0}, {.4, 0}))));
  (void)f::checked(part.add_sketch(open));
  (void)f::checked(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("open.section"), "Open section",
      {f::checked(PathSegmentReference::create_planar(open.id(), SketchEntityId("open"),
                                                      PlanarPathCurveKind::Line))},
      PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7, native_policy()))));
  return part;
}
std::vector<SweepPathSegment> resolve(const PartDocument& part, const char* id) {
  const auto cache = f::checked(ShapeCache::create(ShapeCacheId("route.query")));
  return f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(part, PathCurveId(id), cache));
}
std::vector<ClosedProfileCurveSegment> rectangle() {
  const std::array<Point3, 4> p{{{-.4, -.2, 0}, {.4, -.2, 0}, {.4, .2, 0}, {-.4, .2, 0}}};
  std::vector<ClosedProfileCurveSegment> result;
  for (std::size_t i = 0; i < p.size(); ++i)
    result.push_back({ClosedProfileCurveKind::Line, p[i], {}, p[(i + 1) % p.size()]});
  return result;
}
Point3 section_point(double station, double x, double y, double twist = 0) {
  const double theta = twist * std::numbers::pi / 180 * station / route_length;
  const double across = x * std::cos(theta) - y * std::sin(theta),
               along = x * std::sin(theta) + y * std::cos(theta);
  if (station <= 5)
    return {across, along, station};
  if (station >= 5 + 10 * std::numbers::pi)
    return {20 + station - 5 - 10 * std::numbers::pi, along, 25 - across};
  const double angle = (station - 5) / 20;
  return {20 * (1 - std::cos(angle)) + across * std::cos(angle), along,
          5 + 20 * std::sin(angle) - across * std::sin(angle)};
}
void sections(const GeometryShape& shape, bool surface, double twist = 0) {
  for (const double station :
       {1., 4., 5., 7., 15., 25., 5 + 10 * std::numbers::pi, route_length - 1}) {
    CAPTURE(station, surface, twist);
    if (surface) {
      for (const double x : {-.4, -.2, 0., .2, .4})
        CHECK(blcad::test::distance_to_faces(shape, section_point(station, x, 0, twist)) < 2e-5);
    } else
      for (const auto p :
           std::array<Point2, 6>{{{-.4, -.2}, {.4, -.2}, {.4, .2}, {-.4, .2}, {0, .2}, {.4, 0}}})
        CHECK(blcad::test::distance_to_faces(shape, section_point(station, p.x, p.y, twist)) <
              2e-5);
  }
}
} // namespace

TEST_CASE("G01 mixed native spans retain signed arc-length twist across frame joints",
          "[geometry][parity-b166][native-path-consumers]") {
  auto part = route_inputs();
  const auto strict = SweepAdapter{}.sweep_closed_profile(
      FeatureId("strict.mixed.twist"), rectangle(), resolve(part, "route"),
      PathOrientationRule::MinimumTwist, {}, 135);
  REQUIRE_FALSE(strict);
  CHECK(strict.error().message().find("approximation") != std::string::npos);
  REQUIRE(part.set_path_execution_policy(PathCurveId("route"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-4))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto route = resolve(part, "route");
  REQUIRE(route.size() == 3);
  CHECK(route[0].kind == SweepPathSegmentKind::Line);
  CHECK(route[1].kind == SweepPathSegmentKind::DerivedNative);
  CHECK(route[2].kind == SweepPathSegmentKind::DerivedNative);
  for (const double angle : {135., -90.}) {
    const auto result = SweepAdapter{}.sweep_closed_profile(
        FeatureId("mixed.twist"), rectangle(), route, PathOrientationRule::MinimumTwist, {}, angle);
    INFO((result ? "Swept" : result.error().message()));
    REQUIRE(result);
    CHECK(RectangleExtrusionAdapter{}.summarize(result.value()).volume_mm3 ==
          Catch::Approx(.32 * route_length).margin(2e-5));
    sections(result.value(), false, angle);
  }
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
}

TEST_CASE(
    "G01 authored mixed native consumers preserve physical sections through edits and reopening",
    "[geometry][parity-b166][native-path-consumers]") {
  for (const int kind : {0, 1, 2, 3, 4}) {
    CAPTURE(kind);
    const bool surface = kind == 2, cut = kind == 1 || kind == 4;
    auto part = route_inputs();
    const auto context = f::checked(FeatureBodyResultContext::create(
        cut ? FeatureBodyOperationMode::Cut : FeatureBodyOperationMode::NewBody,
        cut ? std::optional<BodyId>{BodyId("base.body")} : std::nullopt,
        cut ? std::nullopt : std::optional<BodyId>{BodyId("result")}));
    if (!cut)
      REQUIRE(part.add_body(f::checked(Body::create(
          BodyId("result"), "Result", surface ? BodyKind::Surface : BodyKind::Solid))));
    if (kind < 3) {
      const auto profile =
          surface ? f::checked(SweepProfileReference::create_open_path(PathCurveId("open.section")))
                  : f::checked(SweepProfileReference::create_closed_region(f::checked(
                        ProfileRegionReference::create(SketchId("section"), ProfileId("rectangle"),
                                                       PartFeatureInputRole::SweepProfile))));
      const auto feature =
          kind == 0   ? SweepFeature::create_sweep(FeatureId("consumer"), "Sweep", profile,
                                                   PathCurveId("route"), context, {}, {}, {},
                                                   PathCurveId("guide"))
          : kind == 1 ? SweepFeature::create_sweep_cut(FeatureId("consumer"), "Cut", profile,
                                                       PathCurveId("route"), context, {}, {}, {},
                                                       PathCurveId("guide"))
                      : SweepFeature::create_sweep_surface(FeatureId("consumer"), "Surface",
                                                           profile, PathCurveId("route"), context,
                                                           {}, {}, {}, PathCurveId("guide"));
      REQUIRE(part.add_sweep_feature(f::checked(feature)));
    } else {
      auto feature = f::checked(
          cut ? Feature::create_subtractive_path_extrude(
                    FeatureId("consumer"), "Path cut", SketchId("section"),
                    FeatureId("base.feature"), PathCurveId("route"))
              : Feature::create_additive_path_extrude(FeatureId("consumer"), "Path extrude",
                                                      SketchId("section"), PathCurveId("route")));
      REQUIRE(part.add_feature(f::checked(feature.with_body_result_context(context))));
    }
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("consumer.accepted")));
    for (const double end : {25., 27.}) {
      REQUIRE(
          part.set_parameter_value(ParameterId("tail.end"), f::checked(Quantity::length_mm(end))));
      const auto source = f::checked(serialize_part_document_to_json(part));
      const auto execution = GeometryRecomputeExecutor{}.execute_document(part, cache);
      INFO((execution ? "Executed" : execution.error().message()));
      REQUIRE(execution);
      const auto* shape = cache.find_body_shape(cut ? BodyId("base.body") : BodyId("result"));
      REQUIRE(shape);
      sections(*shape, surface);
      const double length = route_length + end - 25;
      if (surface) {
        CHECK(RectangleExtrusionAdapter{}.summarize(*shape).solid_count == 0);
        GProp_GProps area;
        BRepGProp::SurfaceProperties(*OcctShapeView::get(*shape), area);
        CHECK(area.Mass() == Catch::Approx(.8 * length).margin(2e-5));
      } else {
        const double expected = cut ? 256000 - .32 * length : .32 * length;
        CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
              Catch::Approx(expected).margin(2e-5));
        for (const double station : {2., 12., 30., route_length - 1})
          CHECK(blcad::test::classify_solid_point(*shape, section_point(station, 0, 0)) ==
                (cut ? blcad::test::SolidPointLocation::Outside
                     : blcad::test::SolidPointLocation::Inside));
      }
      CHECK(f::checked(serialize_part_document_to_json(part)) == source);
      auto reopened = f::checked(deserialize_part_document_from_json(source));
      CHECK(f::checked(serialize_part_document_to_json(reopened)) == source);
      auto fresh = f::checked(ShapeCache::create(ShapeCacheId("consumer.fresh")));
      REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened, fresh));
      sections(*fresh.find_body_shape(cut ? BodyId("base.body") : BodyId("result")), surface);
      CHECK(RectangleExtrusionAdapter{}
                .summarize(*fresh.find_body_shape(BodyId("base.body")))
                .volume_mm3 == Catch::Approx(cut ? 256000 - .32 * length : 256000).margin(2e-5));
    }
  }
}

TEST_CASE("G01 whole-route reversal preserves each authored native segment and endpoint role",
          "[geometry][parity-b166][native-path-consumers]") {
  auto part = route_inputs();
  const auto& source = *part.find_path_curve(PathCurveId("route"));
  const auto& refs = source.segments();
  REQUIRE(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("reverse"), "Reverse",
      {f::checked(PathSegmentReference::create_sketch_3d(*refs[2].sketch_3d(), *refs[2].entity(),
                                                         *refs[2].sketch_3d_curve_kind(), true)),
       f::checked(PathSegmentReference::create_planar(*refs[1].planar_sketch(), *refs[1].entity(),
                                                      *refs[1].planar_curve_kind(), false)),
       f::checked(
           PathSegmentReference::create_construction_line(*refs[0].construction_line(), true))},
      source.closure(), source.orientation_rule(), {}, {}, source.connection_tolerance_mm(),
      native_policy()))));
  const auto forward = resolve(part, "route"), reverse = resolve(part, "reverse");
  REQUIRE(forward.size() == 3);
  REQUIRE(reverse.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    CHECK(distance(reverse[i].start, forward[2 - i].end) < 1e-8);
    CHECK(distance(reverse[i].end, forward[2 - i].start) < 1e-8);
  }
  const auto forward_shape =
      f::checked(SweepAdapter{}.path_shape(PathCurveId("route"), forward, false));
  const auto reverse_shape =
      f::checked(SweepAdapter{}.path_shape(PathCurveId("reverse"), reverse, false));
  GProp_GProps a, b;
  BRepGProp::LinearProperties(*OcctShapeView::get(forward_shape), a);
  BRepGProp::LinearProperties(*OcctShapeView::get(reverse_shape), b);
  CHECK(a.Mass() == Catch::Approx(route_length).margin(1e-8));
  CHECK(b.Mass() == Catch::Approx(route_length).margin(1e-8));
  std::vector<TopoDS_Edge> forward_edges, reverse_edges;
  for (TopExp_Explorer edges(*OcctShapeView::get(forward_shape), TopAbs_EDGE); edges.More();
       edges.Next())
    forward_edges.push_back(TopoDS::Edge(edges.Current()));
  for (TopExp_Explorer edges(*OcctShapeView::get(reverse_shape), TopAbs_EDGE); edges.More();
       edges.Next())
    reverse_edges.push_back(TopoDS::Edge(edges.Current()));
  REQUIRE(forward_edges.size() == 3);
  REQUIRE(reverse_edges.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    const BRepAdaptor_Curve curve(reverse_edges[i]), other(forward_edges[2 - i]);
    for (const double t : {0., .2, .5, .8, 1.}) {
      gp_Pnt p, q;
      gp_Vec v, w;
      curve.D1(std::lerp(curve.FirstParameter(), curve.LastParameter(), t), p, v);
      other.D1(std::lerp(other.LastParameter(), other.FirstParameter(), t), q, w);
      CHECK(p.Distance(q) < 1e-8);
      CHECK(v.Normalized().Dot(w.Normalized()) == Catch::Approx(-1).margin(1e-8));
    }
  }
}

TEST_CASE(
    "G01 unresolved mixed-frame gaps reject complete publication and retain certified geometry",
    "[geometry][parity-b166][native-path-consumers]") {
  auto part = route_inputs();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("connection.accepted")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto accepted = part;
  REQUIRE(part.update_construction_point(f::checked(ConstructionPoint::create_explicit(
      ConstructionPointId("routejoin"), "Disconnected", {0, 0, 4}))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto plan = f::checked(part.create_recompute_plan());
  const auto incremental = GeometryRecomputeExecutor{}.execute_plan(part, plan, cache);
  REQUIRE_FALSE(incremental);
  CHECK(incremental.error().message().find("connect") != std::string::npos);
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("connect") != std::string::npos);
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("base.body")))
            .volume_mm3 == Catch::Approx(256000).margin(1e-6));
}

TEST_CASE("G01 near-degenerate standalone native paths reject below resolvable model size",
          "[geometry][parity-b166][native-path-consumers]") {
  for (const auto& [extent, accepted] : {std::pair{.001, true}, {1e-9, false}}) {
    CAPTURE(extent, accepted);
    auto part = f::part();
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("short.accepted")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto certified = part;
    REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
        ParameterId("short.extent"), "Short extent", f::checked(Quantity::length_mm(extent))))));
    auto owner = f::checked(Sketch3D::create(Sketch3DId("short"), "Short path"));
    auto end = f::checked(SketchPoint3D::create(
        SketchEntityId("b"), f::coordinate(0), f::coordinate(0),
        f::checked(SketchCoordinate3D::create_parameter(ParameterId("short.extent")))));
    REQUIRE(owner.add_point(f::point("a", {})));
    REQUIRE(owner.add_point(end));
    REQUIRE(owner.add_point(f::point("c", {0, 0, extent / 3})));
    REQUIRE(owner.add_point(f::point("d", {0, 0, extent * 2 / 3})));
    REQUIRE(owner.add_spline(f::checked(
        SketchSpline3D::create(SketchEntityId("curve"), SketchSpline3DRepresentation::ControlPoints,
                               {f::local("a"), f::local("c"), f::local("d"), f::local("b")}, 3,
                               SketchSpline3DContinuity::Curvature))));
    REQUIRE(part.add_sketch_3d(owner));
    REQUIRE(part.add_path_curve(f::checked(PathCurve::create(
        PathCurveId("short.path"), "Short path",
        {f::checked(PathSegmentReference::create_sketch_3d(owner.id(), SketchEntityId("curve"),
                                                           Sketch3DPathCurveKind::Spline))},
        PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-12, native_policy()))));
    const auto source = f::checked(serialize_part_document_to_json(part));
    const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
    INFO((result ? "Executed" : result.error().message()));
    CHECK(static_cast<bool>(result) == accepted);
    CHECK(f::checked(serialize_part_document_to_json(part)) == source);
    if (accepted) {
      CHECK(cache.validate_complete_geometry_result(part));
      const auto wire = f::checked(
          SweepAdapter{}.path_shape(PathCurveId("short.path"), resolve(part, "short.path"), false));
      GProp_GProps length;
      BRepGProp::LinearProperties(*OcctShapeView::get(wire), length);
      CHECK(length.Mass() == Catch::Approx(extent).margin(1e-10));
    } else {
      CHECK(cache.validate_complete_geometry_result(certified));
      CHECK_FALSE(cache.validate_complete_geometry_result(part));
    }
  }
}

TEST_CASE("G01 native helical Sweep preserves measured circular sections in both hands with roll",
          "[geometry][parity-b166][native-path-consumers][native-helix-consumer]") {
  const double q = 1 / std::numbers::pi, norm = std::hypot(4., q);
  for (const bool right : {false, true})
    for (const bool roll : {false, true}) {
      CAPTURE(right, roll);
      const double hand = right ? 1 : -1;
      auto part = f::part();
      auto owner = f::checked(Sketch3D::create(Sketch3DId("helix.source"), "Helix source"));
      REQUIRE(owner.add_helix(f::checked(SketchHelix3D::create(
          SketchEntityId("helix"),
          f::checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z"))),
          ParameterId("radius"), ParameterId("pitch"), ParameterId("turns"),
          right ? SketchHelix3DHandedness::RightHanded : SketchHelix3DHandedness::LeftHanded))));
      REQUIRE(part.add_sketch_3d(owner));
      REQUIRE(part.add_path_curve(f::checked(PathCurve::create(
          PathCurveId("helix.route"), "Helix route",
          {f::checked(PathSegmentReference::create_sketch_3d(owner.id(), SketchEntityId("helix"),
                                                             Sketch3DPathCurveKind::Helix))},
          PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
          f::checked(PathExecutionPolicy::curve_preserving(1e-5))))));
      REQUIRE(part.add_construction_plane(f::checked(ConstructionPlane::create_explicit(
          ConstructionPlaneId("helix.section.plane"), "Normal section", {-4, 0, 0}, {1, 0, 0},
          {0, q / norm, hand * 4 / norm}, {0, -hand * 4 / norm, q / norm}))));
      REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
          ParameterId("wire.diameter"), "Wire diameter", f::checked(Quantity::length_mm(.2))))));
      auto section = f::checked(
          Sketch::create(SketchId("helix.section"), "Wire", DatumPlaneId("helix.section.plane")));
      REQUIRE(section.add_profile(
          f::checked(CircleProfile::create(ProfileId("wire"), ParameterId("wire.diameter")))));
      REQUIRE(part.add_sketch(section));
      REQUIRE(part.add_body(
          f::checked(Body::create(BodyId("helix.body"), "Helical wire", BodyKind::Solid))));
      const auto profile = f::checked(
          SweepProfileReference::create_closed_region(f::checked(ProfileRegionReference::create(
              section.id(), ProfileId("wire"), PartFeatureInputRole::SweepProfile))));
      REQUIRE(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
          FeatureId("helix.consumer"), "Native helical Sweep", profile, PathCurveId("helix.route"),
          f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                      BodyId("helix.body"))),
          {}, {}, roll ? std::optional<ParameterId>{ParameterId("angle")} : std::nullopt))));
      const auto before = f::checked(serialize_part_document_to_json(part));
      auto cache = f::checked(ShapeCache::create(ShapeCacheId("helix.accepted")));
      const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
      INFO((result ? "Executed" : result.error().message()));
      REQUIRE(result);
      const auto check = [&](const ShapeCache& geometry) {
        const auto* shape = geometry.find_body_shape(BodyId("helix.body"));
        REQUIRE(shape);
        CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
              Catch::Approx(.01 * std::numbers::pi * 3 * std::hypot(8 * std::numbers::pi, 2.))
                  .margin(1e-5));
        for (const double t : {0., .07, .2, .4, .55, .8, .93, 1.}) {
          const double a = hand * 6 * std::numbers::pi * t, c = std::cos(a), s = std::sin(a);
          const Point3 center{-4 * c, -4 * s, 6 * t};
          CHECK(blcad::test::classify_solid_point(*shape, center) ==
                (t == 0 || t == 1 ? blcad::test::SolidPointLocation::Boundary
                                  : blcad::test::SolidPointLocation::Inside));
          for (int i = 0; i < 8; ++i) {
            const double theta = i * std::numbers::pi / 4;
            const double x = .1 * std::cos(theta), y = .1 * std::sin(theta);
            const Point3 witness{center.x + c * x - q * s / norm * y,
                                 center.y + s * x + q * c / norm * y,
                                 center.z + hand * 4 / norm * y};
            CHECK(blcad::test::distance_to_faces(*shape, witness) < 2e-5);
          }
        }
      };
      check(cache);
      CHECK(f::checked(serialize_part_document_to_json(part)) == before);
      auto restored = f::checked(deserialize_part_document_from_json(before));
      auto fresh = f::checked(ShapeCache::create(ShapeCacheId("helix.reopened")));
      REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored, fresh));
      check(fresh);
    }
}
