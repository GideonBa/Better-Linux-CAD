#include "../support/equation_curve_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/equation_curve_geometry.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/revolve_adapter.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "derived_curve_internal.hpp"
#include <GeomAdaptor_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::equation_fixture;
namespace {
constexpr double pi = std::numbers::pi;
EquationCurve equation(std::array<std::string, 3> xyz, std::string lower = "0",
                       std::string upper = "1", double tolerance = .001, bool closed = false) {
  const auto mode =
      xyz[2].empty() ? EquationCurveMode::Parametric2D : EquationCurveMode::Parametric3D;
  return f::checked(
      EquationCurve::create(SketchEntityId("eq"), mode, {"u", CurveUnit::Scalar}, std::move(lower),
                            std::move(upper), std::move(xyz), tolerance,
                            closed ? EquationCurveClosure::Periodic : EquationCurveClosure::Open));
}
PartDocument document() {
  auto part = f::checked(PartDocument::create(DocumentId("eq.part"), "Equation geometry"));
  (void)f::checked(part.add_datum_plane(f::checked(DatumPlane::xy())));
  return part;
}
EquationCurveGeometry converted_geometry(const PartDocument& part, const EquationCurve& source) {
  auto result = EquationCurveGeometryAdapter{}.convert(part, source);
  INFO((result.has_error() ? result.error().message() : "resolved"));
  REQUIRE(result);
  return std::move(result.value());
}
Point3 sample(const EquationCurveGeometry& result, double fraction) {
  const auto& c = result.curve->curve;
  const auto p = c->Value(std::lerp(c->FirstParameter(), c->LastParameter(), fraction));
  return {p.X(), p.Y(), p.Z()};
}
FeatureBodyResultContext new_body() {
  return f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                     std::nullopt, BodyId("body")));
}
PartDocument circle_part(double center = 0) {
  auto part = document();
  (void)f::checked(part.add_parameter(f::checked(
      Parameter::create_length(ParameterId("r"), "Radius", f::checked(Quantity::length_mm(2))))));
  auto sketch = f::checked(Sketch::create(SketchId("sketch"), "Circle", DatumPlaneId("datum.xy")));
  (void)f::checked(sketch.add_entity(equation(
      {std::to_string(center) + "mm+@{r}*cos(u)", "@{r}*sin(u)", ""}, "0", "2*pi", .001, true)));
  auto profile =
      ArcClosedProfile::create_for_sketch(ProfileId("profile"), {SketchEntityId("eq")}, sketch);
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  REQUIRE(part.add_sketch(sketch));
  return part;
}
double volume(const PartDocument& part) {
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("cache")));
  auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result.has_error() ? result.error().message() : "recomputed"));
  REQUIRE(result);
  const auto* shape = cache.find_body_shape(BodyId("body"));
  REQUIRE(shape);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
} // namespace

TEST_CASE("Equation Geometry retains analytic circles and cubic polynomials with bounded error",
          "[geometry][parity-b161][equation-geometry]") {
  const auto part = document();
  for (double radius : {2., 1000000.}) {
    auto source =
        equation({std::to_string(radius) + "mm*cos(u)", std::to_string(radius) + "mm*sin(u)", ""},
                 "0", "2*pi", .001, true);
    auto result = converted_geometry(part, source);
    CHECK(result.kind == EquationCurveGeometryKind::AnalyticCircle);
    CHECK(GeomAdaptor_Curve(result.curve->curve).GetType() == GeomAbs_Circle);
    CHECK(result.endpoints[0].role == "start");
    CHECK(result.endpoints[1].role == "end");
    CHECK(distance(result.endpoints[0].position, result.endpoints[1].position) == 0);
    CHECK(f::checked(derived_curve_length(result.curve, "eq")) ==
          Catch::Approx(2 * pi * radius).epsilon(1e-10));
    for (int i = 0; i <= 1000; ++i) {
      const double t = i / 1000.;
      CHECK(distance(sample(result, t),
                     {radius * std::cos(2 * pi * t), radius * std::sin(2 * pi * t), 0}) <=
            result.error_bound_mm);
    }
  }
  auto parabola = converted_geometry(part, equation({"u*1mm", "u*u*1mm", ""}, "-2", "3"));
  CHECK(parabola.kind == EquationCurveGeometryKind::Polynomial);
  for (int i = 0; i <= 1000; ++i) {
    const double u = -2 + 5 * i / 1000.;
    CHECK(distance(sample(parabola, i / 1000.), {u, u * u, 0}) <= parabola.error_bound_mm);
  }
}

TEST_CASE("Adaptive equations have a certified smooth curve rather than an aliased display polygon",
          "[geometry][parity-b161][equation-geometry]") {
  auto part = document();
  for (const auto& source : {equation({"u*1mm", "exp(u)*1mm", ""}, "0", "4", .01),
                             equation({"2mm*cos(u)", "2mm*sin(u)", "u*1mm"}, "0", "2*pi", .01)}) {
    const auto result = converted_geometry(part, source);
    CHECK(result.kind == EquationCurveGeometryKind::Adaptive);
    const auto spline = Handle(Geom_BSplineCurve)::DownCast(result.curve->curve);
    REQUIRE_FALSE(spline.IsNull());
    CHECK(spline->IsCN(1));
    const auto program = f::checked(EquationCurveProgram::compile(part, source));
    const auto domain = f::checked(program.domain(part));
    CHECK(result.error_bound_mm <= source.tolerance_mm() / 2);
    for (int i = 0; i <= 2000; ++i) {
      const double t = i / 2000.;
      const double u = std::lerp(domain.first, domain.second, t);
      const Point3 expected = source.mode() == EquationCurveMode::Parametric3D
                                  ? Point3{2 * std::cos(u), 2 * std::sin(u), u}
                                  : Point3{u, std::exp(u), 0};
      CHECK(distance(sample(result, t), expected) <= result.error_bound_mm);
    }
    for (int i = 2; i < spline->NbKnots(); ++i) {
      const double u = spline->Knot(i);
      gp_Pnt p;
      gp_Vec left, right;
      spline->D1(std::nextafter(u, 0.), p, left);
      spline->D1(std::nextafter(u, 1.), p, right);
      CHECK((left - right).Magnitude() < 1e-7);
    }
    if (source.mode() == EquationCurveMode::Parametric3D) {
      const auto tangent = f::checked(derived_curve_start_tangent(result.curve, "eq"));
      CHECK(tangent.x == Catch::Approx(0).margin(1e-12));
      CHECK(tangent.y == Catch::Approx(2 / std::sqrt(5.)).margin(1e-10));
      CHECK(tangent.z == Catch::Approx(1 / std::sqrt(5.)).margin(1e-10));
      CHECK(f::checked(derived_curve_length(result.curve, "eq")) ==
            Catch::Approx(2 * pi * std::sqrt(5.)).margin(.0001));
    }
  }
}

TEST_CASE("Equation Geometry fails closed on singularities closure mismatch and bounded work",
          "[geometry][parity-b161][equation-geometry]") {
  const auto part = document();
  for (const auto& source :
       {equation({"u*1mm", "1mm/(u-.5)", ""}), equation({"u*u*1mm", "u*u*1mm", ""}, "-1", "1"),
        equation({"cos(u)*1mm", "sin(u)*1mm", ""}, "0", "6.283185307179586", .01, true),
        equation({"1e16mm+u*1mm", "sin(u)*1mm", ""})}) {
    auto result = EquationCurveGeometryAdapter{}.convert(part, source);
    CHECK_FALSE(result);
    if (!result)
      CHECK((result.error().object_id() == "eq" || result.error().object_id().starts_with("eq/")));
  }
  const auto oscillation = equation({"u*1mm", "sin(2048*pi*u)*1mm", ""});
  auto first = EquationCurveGeometryAdapter{}.convert(part, oscillation, {8, 8, 64});
  auto second = EquationCurveGeometryAdapter{}.convert(part, oscillation, {8, 8, 64});
  REQUIRE_FALSE(first);
  REQUIRE_FALSE(second);
  CHECK(first.error().message() == second.error().message());
  CHECK_FALSE(EquationCurveGeometryAdapter{}.convert(part, oscillation, {8193, 28, 32768}));
  auto leaf = converted_geometry(
      part, equation({"4mm*u*(1-u)", "4mm*(2*u-1)*u*(1-u)", ""}, "0", "1", .001, true));
  CHECK(leaf.curve->closure_proven);
}

TEST_CASE("Equation circle materializes and drives exact Extrude with parameter edits and reopen",
          "[geometry][parity-b161][equation-consumers]") {
  auto part = circle_part();
  const auto* sketch = part.find_sketch(SketchId("sketch"));
  REQUIRE(sketch);
  auto finish = SketchFinishService::analyze_unmaterialized(part, *sketch);
  REQUIRE(finish);
  REQUIRE(part.add_parameter(f::checked(
      Parameter::create_length(ParameterId("h"), "Height", f::checked(Quantity::length_mm(5))))));
  REQUIRE(part.add_body(f::checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  auto feature = f::checked(Feature::create_additive_extrude(FeatureId("extrude"), "Extrude",
                                                             SketchId("sketch"), ParameterId("h")));
  REQUIRE(part.add_feature(f::checked(f::checked(feature.with_body_result_context(new_body()))
                                          .with_profile(ProfileId("profile")))));
  CHECK(volume(part) == Catch::Approx(20 * pi).epsilon(1e-8));
  REQUIRE(part.set_parameter_value(ParameterId("r"), f::checked(Quantity::length_mm(3))));
  CHECK(volume(part) == Catch::Approx(45 * pi).epsilon(1e-8));
  auto json = f::checked(serialize_part_document_to_json(part));
  auto reopened = f::checked(deserialize_part_document_from_json(json));
  CHECK(volume(reopened) == Catch::Approx(45 * pi).epsilon(1e-8));
  CHECK(f::checked(serialize_part_document_to_json(reopened)) == json);
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("scene")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  auto scene = ViewportSceneBuilder{}.build_part(part, cache);
  REQUIRE(scene);
  CHECK(std::any_of(scene.value().begin(), scene.value().end(), [](const auto& item) {
    return item.semantic_id == "sketch/sketch/entity/eq";
  }));
}

TEST_CASE("Equation circle drives real Revolve with an independent torus volume",
          "[geometry][parity-b161][equation-consumers]") {
  auto part = circle_part(10);
  REQUIRE(part.add_body(f::checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  REQUIRE(part.add_construction_line(f::checked(
      ConstructionLine::create_explicit(ConstructionLineId("axis"), "Axis", {}, {0, 1, 0}))));
  auto profile = f::checked(ProfileRegionReference::create(SketchId("sketch"), ProfileId("profile"),
                                                           PartFeatureInputRole::RevolveProfile));
  auto axis = f::checked(AxisReference::create_construction_line(PartFeatureInputRole::RevolveAxis,
                                                                 ConstructionLineId("axis"),
                                                                 PartFeatureInputCapability::Axis));
  REQUIRE(part.add_revolve_feature(f::checked(RevolveFeature::create_revolve(
      FeatureId("revolve"), "Revolve", profile, axis, RevolveAngleExtent::full(), new_body()))));
  CHECK(volume(part) == Catch::Approx(80 * pi * pi).epsilon(1e-8));
}

TEST_CASE("Spatial equation PathCurve drives real Sweep and retains source authority",
          "[geometry][parity-b161][equation-consumers]") {
  auto part = circle_part();
  auto spatial = f::checked(Sketch3D::create(Sketch3DId("space"), "Path"));
  REQUIRE(spatial.add_equation_curve(equation({"0mm", "0mm", "u*1mm"}, "0", "12")));
  REQUIRE(part.add_sketch_3d(spatial));
  auto segment = f::checked(PathSegmentReference::create_sketch_3d(
      Sketch3DId("space"), SketchEntityId("eq"), Sketch3DPathCurveKind::EquationCurve));
  REQUIRE(part.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("path"), "Path", {segment}, PathClosure::Open,
                                   PathOrientationRule::MinimumTwist))));
  REQUIRE(part.add_body(f::checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  auto profile = f::checked(
      SweepProfileReference::create_closed_region(f::checked(ProfileRegionReference::create(
          SketchId("sketch"), ProfileId("profile"), PartFeatureInputRole::SweepProfile))));
  REQUIRE(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("sweep"), "Sweep", profile, PathCurveId("path"), new_body()))));
  CHECK(volume(part) == Catch::Approx(48 * pi).epsilon(1e-8));
  auto reopened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(part))));
  CHECK(volume(reopened) == Catch::Approx(48 * pi).epsilon(1e-8));
  CHECK(reopened.find_sketch_3d(Sketch3DId("space"))->points().empty());
  auto edited = *part.find_sketch_3d(Sketch3DId("space"));
  REQUIRE(edited.replace_equation_curve(equation({"0mm", "0mm", "u*1mm"}, "0", "18", .0005)));
  REQUIRE(part.update_sketch_3d(edited));
  CHECK(volume(part) == Catch::Approx(72 * pi).epsilon(1e-8));
  auto reversed = f::checked(PathSegmentReference::create_sketch_3d(
      Sketch3DId("space"), SketchEntityId("eq"), Sketch3DPathCurveKind::EquationCurve, true));
  REQUIRE(part.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("reverse"), "Reversed", {reversed},
                                   PathClosure::Open, PathOrientationRule::MinimumTwist))));
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("reverse.cache")));
  auto resolved = f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(part, PathCurveId("reverse"), cache));
  REQUIRE(resolved.size() == 1);
  CHECK(resolved[0].kind == SweepPathSegmentKind::DerivedEquation);
  CHECK(resolved[0].start.z == 18);
  CHECK(resolved[0].end.z == 0);
  CHECK(f::checked(derived_curve_start_tangent(resolved[0].derived_curve, "eq")).z ==
        Catch::Approx(-1));
  auto edited_reopen = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(part))));
  CHECK(volume(edited_reopen) == Catch::Approx(72 * pi).epsilon(1e-8));
  CHECK(edited_reopen.find_sketch_3d(Sketch3DId("space"))
            ->find_equation_curve(SketchEntityId("eq"))
            ->tolerance_mm() == .0005);
}

TEST_CASE("Equation regions prove seams by parameter roles and reject interior seam crossings",
          "[geometry][parity-b161][equation-consumers]") {
  auto part = document();
  for (bool crossing : {false, true}) {
    auto sketch =
        f::checked(Sketch::create(SketchId("region"), "Region", DatumPlaneId("datum.xy")));
    auto source = crossing
                      ? equation({"sin(u)*1mm", "sin(2*u)*1mm", ""}, "0", "2*pi", .001, true)
                      : equation({"4mm*u*(1-u)", "4mm*(2*u-1)*u*(1-u)", ""}, "0", "1", .001, true);
    REQUIRE(sketch.add_entity(source));
    auto result = SketchRegionFinder{}.find_single_curve_region(part, sketch);
    if (crossing)
      CHECK_FALSE(result);
    else {
      INFO((result.has_error() ? result.error().message() : "valid region"));
      REQUIRE(result);
      CHECK(result.value().curve_segments == std::vector{SketchEntityId("eq")});
    }
  }
}
TEST_CASE("High frequency equation acceptance is controlled by error rather than midpoint aliases",
          "[geometry][parity-b161][equation-geometry]") {
  auto part = document();
  auto source = equation({"u*1mm", "sin(20*pi*u)*1mm", ""}, "0", "1", .005);
  const auto result = converted_geometry(part, source);
  CHECK(result.accepted_intervals > 100);
  for (int i = 0; i <= 3000; ++i) {
    const double u = i / 3000.;
    CHECK(distance(sample(result, u), {u, std::sin(20 * pi * u), 0}) <= result.error_bound_mm);
  }
}

TEST_CASE("Derived curve reversal placement bounds and length stations retain analytic geometry",
          "[geometry][parity-b161][equation-geometry]") {
  const auto part = document();
  const auto result =
      converted_geometry(part, equation({"2mm*cos(u)", "2mm*sin(u)", ""}, "0", "2*pi", .001, true));
  const auto station = f::checked(derived_curve_point_at_length_fraction(result.curve, .25, "eq"));
  CHECK(distance(station, {0, 2, 0}) < 1e-8);
  const auto reversed = reversed_derived_curve(result.curve);
  const auto reverse_station =
      f::checked(derived_curve_point_at_length_fraction(reversed, .25, "eq"));
  CHECK(distance(reverse_station, {0, -2, 0}) < 1e-8);
  const auto transformed =
      transformed_derived_curve(result.curve, {5, 6, 7}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
  const auto placed = f::checked(derived_curve_point_at_length_fraction(transformed, .25, "eq"));
  CHECK(distance(placed, {7, 6, 7}) < 1e-8);
  const auto box = f::checked(derived_curve_bounds(transformed, "eq"));
  CHECK(box.first.x == Catch::Approx(3).margin(1e-8));
  CHECK(box.second.x == Catch::Approx(7).margin(1e-8));
  CHECK(box.first.z == Catch::Approx(5).margin(1e-8));
  CHECK(box.second.z == Catch::Approx(9).margin(1e-8));
}
TEST_CASE("Equation helix sweeps an independently oriented round section into a physical pipe",
          "[geometry][parity-b161][equation-consumers]") {
  const auto part = document();
  const auto helix =
      converted_geometry(part, equation({"2mm*cos(u)", "2mm*sin(u)", "u*1mm"}, "0", "2*pi", .002));
  const Vector3 x{1, 0, 0}, y{0, 1 / std::sqrt(5.), -2 / std::sqrt(5.)};
  const auto section = [&](double angle) {
    return Point3{2 + .1 * x.x * std::cos(angle) + .1 * y.x * std::sin(angle),
                  .1 * x.y * std::cos(angle) + .1 * y.y * std::sin(angle),
                  .1 * x.z * std::cos(angle) + .1 * y.z * std::sin(angle)};
  };
  std::vector<ClosedProfileCurveSegment> profile{
      {ClosedProfileCurveKind::CircularArc, section(0), section(pi / 2), section(pi)},
      {ClosedProfileCurveKind::CircularArc, section(pi), section(3 * pi / 2), section(2 * pi)}};
  auto solid = SweepAdapter{}.sweep_closed_profile(FeatureId("pipe"), profile,
                                                   {{SweepPathSegmentKind::DerivedEquation,
                                                     helix.endpoints[0].position,
                                                     {},
                                                     helix.endpoints[1].position,
                                                     helix.curve}},
                                                   PathOrientationRule::MinimumTwist);
  INFO((solid.has_error() ? solid.error().message() : "pipe"));
  REQUIRE(solid);
  CHECK(RectangleExtrusionAdapter{}.summarize(solid.value()).volume_mm3 ==
        Catch::Approx(.01 * pi * 2 * pi * std::sqrt(5.)).margin(2e-5));
}

TEST_CASE("Equation paths reject unsupported twist without substituting a chord guide",
          "[geometry][parity-b161][equation-consumers]") {
  const auto part = document();
  const auto path = converted_geometry(part, equation({"0mm", "0mm", "u*1mm"}, "0", "10"));
  std::vector<SweepPathSegment> profile{{SweepPathSegmentKind::Line, {0, 0, 0}, {}, {1, 0, 0}}};
  auto result =
      SweepAdapter{}.sweep_open_profile(FeatureId("twist"), profile,
                                        {{SweepPathSegmentKind::DerivedEquation,
                                          path.endpoints[0].position,
                                          {},
                                          path.endpoints[1].position,
                                          path.curve}},
                                        PathOrientationRule::MinimumTwist, std::nullopt, 90);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("curve-preserving twist") != std::string::npos);
}
