#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
PartDocument inputs(bool curved, PathOrientationRule mode, double scale = 1) {
  auto part = f::part();
  auto owner = f::checked(Sketch3D::create(Sketch3DId("twist.owner"), "Twist source"));
  (void)f::checked(owner.add_equation_curve(f::checked(EquationCurve::create(
      SketchEntityId("curve"), EquationCurveMode::Parametric3D, {"u", CurveUnit::Scalar}, "0",
      curved ? "pi/2" : "1",
      curved ? std::array<std::string, 3>{std::to_string(20 * scale) + "mm*(1-cos(u))", "0mm",
                                          std::to_string(20 * scale) + "mm*sin(u)"}
             : std::array<std::string, 3>{"0mm", "0mm", std::to_string(10 * scale) + "mm*(u+u*u)"},
      1e-7))));
  (void)f::checked(part.add_sketch_3d(owner));
  (void)f::checked(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("twist.path"), "Twist path",
      {f::checked(PathSegmentReference::create_sketch_3d(owner.id(), SketchEntityId("curve"),
                                                         Sketch3DPathCurveKind::EquationCurve))},
      PathClosure::Open, mode,
      mode == PathOrientationRule::FixedUpVector ? std::optional<Vector3>{{0, 1, 0}}
                                                 : std::nullopt))));
  return part;
}
std::vector<SweepPathSegment> path(const PartDocument& part) {
  const auto cache = f::checked(ShapeCache::create(ShapeCacheId("twist.query")));
  return f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(part, PathCurveId("twist.path"), cache));
}
std::vector<ClosedProfileCurveSegment> profile(double scale = 1) {
  const std::array<Point3, 4> corners{{{-.4 * scale, -.2 * scale, 0},
                                       {.4 * scale, -.2 * scale, 0},
                                       {.4 * scale, .2 * scale, 0},
                                       {-.4 * scale, .2 * scale, 0}}};
  std::vector<ClosedProfileCurveSegment> result;
  for (std::size_t i = 0; i < corners.size(); ++i)
    result.push_back(
        {ClosedProfileCurveKind::Line, corners[i], {}, corners[(i + 1) % corners.size()]});
  return result;
}
Point3 expected(bool curved, double fraction, double x, double y, double twist, double scale = 1) {
  const double angle = twist * std::numbers::pi / 180 * fraction;
  const double across = x * std::cos(angle) - y * std::sin(angle),
               along = x * std::sin(angle) + y * std::cos(angle);
  if (!curved)
    return {across, along, 20 * scale * fraction};
  const double u = std::numbers::pi / 2 * fraction;
  return {20 * scale * (1 - std::cos(u)) + across * std::cos(u), along,
          20 * scale * std::sin(u) - across * std::sin(u)};
}
void check_sections(const GeometryShape& shape, bool curved, double twist, double scale = 1) {
  for (double fraction : {.0, .125, .25, .375, .5, .625, .75, .875, 1.}) {
    CAPTURE(fraction, curved, twist);
    for (const auto p :
         std::array<Point2, 6>{{{-.4, -.2}, {.4, -.2}, {.4, .2}, {-.4, .2}, {.4, 0}, {0, .2}}})
      CHECK(blcad::test::distance_to_faces(
                shape, expected(curved, fraction, p.x * scale, p.y * scale, twist, scale)) <
            std::max(1e-7, 2e-5 * scale));
  }
}
} // namespace

TEST_CASE("G01 native equation twist follows true arc length and measured normal-plane sections",
          "[geometry][parity-b166][native-path-twist]") {
  for (bool curved : {false, true})
    for (auto mode : {PathOrientationRule::MinimumTwist, PathOrientationRule::ProfileNormal,
                      PathOrientationRule::FixedUpVector}) {
      CAPTURE(curved, static_cast<int>(mode));
      auto part = inputs(curved, mode);
      const auto up = mode == PathOrientationRule::FixedUpVector ? std::optional<Vector3>{{0, 1, 0}}
                                                                 : std::nullopt;
      const auto legacy = path(part);
      CHECK_FALSE(SweepAdapter{}.sweep_closed_profile(FeatureId("legacy.twist"), profile(), legacy,
                                                      mode, up, 90));
      REQUIRE(part.set_path_execution_policy(
          PathCurveId("twist.path"), f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
      const auto before = f::checked(serialize_part_document_to_json(part));
      const auto native = path(part);
      REQUIRE(native.size() == 1);
      CHECK(native[0].modeling_tolerance_mm == 1e-6);
      for (double twist : {90., -135., 360.}) {
        CAPTURE(twist);
        const auto result = SweepAdapter{}.sweep_closed_profile(FeatureId("native.twist"),
                                                                profile(), native, mode, up, twist);
        INFO((result ? "Swept" : result.error().message()));
        REQUIRE(result);
        const double expected_volume = .8 * .4 * (curved ? 10 * std::numbers::pi : 20);
        CHECK(RectangleExtrusionAdapter{}.summarize(result.value()).volume_mm3 ==
              Catch::Approx(expected_volume).margin(2e-5));
        check_sections(result.value(), curved, twist);
        CHECK(f::checked(serialize_part_document_to_json(part)) == before);
      }
    }
}

TEST_CASE("G01 native twist keeps its measured frame under model scaling",
          "[geometry][parity-b166][native-path-twist]") {
  for (double scale : {.01, 1., 100.}) {
    CAPTURE(scale);
    auto part = inputs(true, PathOrientationRule::MinimumTwist, scale);
    REQUIRE(part.set_path_execution_policy(
        PathCurveId("twist.path"),
        f::checked(PathExecutionPolicy::curve_preserving(1e-6 * scale))));
    const auto result =
        SweepAdapter{}.sweep_closed_profile(FeatureId("scaled.twist"), profile(scale), path(part),
                                            PathOrientationRule::MinimumTwist, {}, 135);
    INFO((result ? "Swept" : result.error().message()));
    REQUIRE(result);
    const double volume = .8 * .4 * 10 * std::numbers::pi * scale * scale * scale;
    CHECK(RectangleExtrusionAdapter{}.summarize(result.value()).volume_mm3 ==
          Catch::Approx(volume).margin(volume * 1e-5));
    check_sections(result.value(), true, 135, scale);
  }
}

TEST_CASE("G01 twisted open profiles form the independently measured helicoid surface",
          "[geometry][parity-b166][native-path-twist]") {
  auto part = inputs(false, PathOrientationRule::MinimumTwist);
  REQUIRE(part.set_path_execution_policy(PathCurveId("twist.path"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
  const std::vector<SweepPathSegment> section{
      {SweepPathSegmentKind::Line, {-.4, 0, 0}, {}, {.4, 0, 0}}};
  const auto result = SweepAdapter{}.sweep_open_profile(
      FeatureId("twist.surface"), section, path(part), PathOrientationRule::MinimumTwist, {}, 90);
  INFO((result ? "Swept" : result.error().message()));
  REQUIRE(result);
  CHECK(RectangleExtrusionAdapter{}.summarize(result.value()).solid_count == 0);
  GProp_GProps area;
  BRepGProp::SurfaceProperties(*OcctShapeView::get(result.value()), area);
  const double k = std::numbers::pi / 40, a = .4;
  CHECK(
      area.Mass() ==
      Catch::Approx(20 * (a * std::sqrt(1 + k * k * a * a) + std::asinh(k * a) / k)).margin(1e-5));
  for (double fraction : {0., .125, .25, .5, .75, .875, 1.})
    for (double across : {-.4, -.2, 0., .2, .4})
      CHECK(blcad::test::distance_to_faces(result.value(),
                                           expected(false, fraction, across, 0, 90)) < 2e-5);
}

TEST_CASE("G01 twist rejects conflicting controls nonfinite angles and unsupported frame budgets",
          "[geometry][parity-b166][native-path-twist]") {
  auto part = inputs(false, PathOrientationRule::MinimumTwist);
  REQUIRE(part.set_path_execution_policy(PathCurveId("twist.path"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto native = path(part);
  const std::vector<SweepPathSegment> section{
      {SweepPathSegmentKind::Line, {-.4, 0, 0}, {}, {.4, 0, 0}}};
  for (double angle :
       {1441., -1441., std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CAPTURE(angle);
    CHECK_FALSE(SweepAdapter{}.sweep_closed_profile(FeatureId("bad.solid"), profile(), native,
                                                    PathOrientationRule::MinimumTwist, {}, angle));
    CHECK_FALSE(SweepAdapter{}.sweep_open_profile(FeatureId("bad.surface"), section, native,
                                                  PathOrientationRule::MinimumTwist, {}, angle));
  }
  auto guide = native;
  for (auto& segment : guide) {
    segment.start.y += 1;
    segment.end.y += 1;
    segment.derived_curve = translated_derived_curve(segment.derived_curve, {0, 1, 0});
  }
  const auto conflict = SweepAdapter{}.sweep_closed_profile(
      FeatureId("conflict"), profile(), native, PathOrientationRule::MinimumTwist, {}, 90, &guide);
  REQUIRE_FALSE(conflict);
  CHECK(conflict.error().message().find("combined guide and twist") != std::string::npos);
  CHECK_FALSE(SweepAdapter{}.sweep_closed_profile(FeatureId("parallel"), profile(), native,
                                                  PathOrientationRule::FixedUpVector,
                                                  Vector3{0, 0, 1}, 90));
  auto curved = inputs(true, PathOrientationRule::FixedUpVector);
  REQUIRE(curved.set_path_execution_policy(
      PathCurveId("twist.path"), f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
  const auto endpoint_parallel =
      SweepAdapter{}.sweep_closed_profile(FeatureId("parallel.end"), profile(), path(curved),
                                          PathOrientationRule::FixedUpVector, Vector3{1, 0, 0}, 90);
  REQUIRE_FALSE(endpoint_parallel);
  CHECK(endpoint_parallel.error().message().find("parallel") != std::string::npos);
  const std::vector<SweepPathSegment> kink{
      {SweepPathSegmentKind::Line, {0, 0, 0}, {}, {0, 0, 10}, {}, 1e-6},
      {SweepPathSegmentKind::Line, {0, 0, 10}, {}, {5, 0, 10}, {}, 1e-6}};
  const auto kinked = SweepAdapter{}.sweep_closed_profile(
      FeatureId("kink"), profile(), kink, PathOrientationRule::MinimumTwist, {}, 90);
  REQUIRE_FALSE(kinked);
  CHECK(kinked.error().message().find("tangent-continuous") != std::string::npos);
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
}

TEST_CASE(
    "G01 authored equation Sweep retains certified geometry on rejected twist and reopens exactly",
    "[geometry][parity-b166][native-path-twist][native-twist-recompute]") {
  auto part = inputs(false, PathOrientationRule::MinimumTwist);
  REQUIRE(part.set_path_execution_policy(PathCurveId("twist.path"),
                                         f::checked(PathExecutionPolicy::curve_preserving(1e-6))));
  REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("twist.width"), "Twist width", f::checked(Quantity::length_mm(.8))))));
  REQUIRE(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("twist.height"), "Twist height", f::checked(Quantity::length_mm(.4))))));
  REQUIRE(part.add_parameter(f::checked(Parameter::create_angle(
      ParameterId("twist.angle"), "Twist angle", f::checked(Quantity::angle_deg(90))))));
  auto section = f::checked(
      Sketch::create(SketchId("twist.profile"), "Twist profile", DatumPlaneId("datum.xy")));
  REQUIRE(section.add_profile(f::checked(RectangleProfile::create(
      ProfileId("twist.rectangle"), ParameterId("twist.width"), ParameterId("twist.height")))));
  REQUIRE(part.add_sketch(section));
  REQUIRE(
      part.add_body(f::checked(Body::create(BodyId("twist.body"), "Twisted", BodyKind::Solid))));
  const auto profile_ref = f::checked(
      SweepProfileReference::create_closed_region(f::checked(ProfileRegionReference::create(
          section.id(), ProfileId("twist.rectangle"), PartFeatureInputRole::SweepProfile))));
  REQUIRE(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("twist.feature"), "Twisted Sweep", profile_ref, PathCurveId("twist.path"),
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("twist.body"))),
      {}, {}, ParameterId("twist.angle")))));
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("twist.accepted")));
  const auto execution = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((execution ? "Executed" : execution.error().message()));
  REQUIRE(execution);
  const auto* shape = cache.find_body_shape(BodyId("twist.body"));
  REQUIRE(shape);
  CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
        Catch::Approx(6.4).margin(2e-5));
  check_sections(*shape, false, 90);
  const auto accepted = part;
  const auto saved = f::checked(serialize_part_document_to_json(part));
  auto restored = f::checked(deserialize_part_document_from_json(saved));
  CHECK(f::checked(serialize_part_document_to_json(restored)) == saved);
  auto fresh = f::checked(ShapeCache::create(ShapeCacheId("twist.reopened")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored, fresh));
  check_sections(*fresh.find_body_shape(BodyId("twist.body")), false, 90);
  REQUIRE(
      part.set_parameter_value(ParameterId("twist.angle"), f::checked(Quantity::angle_deg(1441))));
  const auto invalid = f::checked(serialize_part_document_to_json(part));
  const auto rejected = GeometryRecomputeExecutor{}.execute_document(part, cache);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("four turns") != std::string::npos);
  CHECK(cache.validate_complete_geometry_result(accepted));
  CHECK_FALSE(cache.validate_complete_geometry_result(part));
  CHECK(f::checked(serialize_part_document_to_json(part)) == invalid);
  check_sections(*cache.find_body_shape(BodyId("twist.body")), false, 90);
  REQUIRE(
      part.set_parameter_value(ParameterId("twist.angle"), f::checked(Quantity::angle_deg(45))));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  check_sections(*cache.find_body_shape(BodyId("twist.body")), false, 45);
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("base.body")))
            .volume_mm3 == Catch::Approx(240).margin(1e-6));
}
