#include "../../src/geometry/surface_join_verification.hpp"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <gp.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <numbers>
#include <string>
using namespace blcad;
using namespace blcad::geometry::detail;
namespace {
TopoDS_Face patch(const Handle(Geom_Surface) & surface, double u0, double u1, double v0,
                  double v1) {
  BRepBuilderAPI_MakeFace builder(surface, u0, u1, v0, v1, 1e-9);
  REQUIRE(builder.IsDone());
  return builder.Face();
}
Handle(Geom_Surface) quadratic(double coefficient, bool singular = false) {
  TColgp_Array2OfPnt poles(1, 3, 1, 2);
  for (int u = 1; u <= 3; ++u)
    for (int v = 1; v <= 2; ++v)
      poles.SetValue(u, v,
                     {singular ? (u == 3 ? 4.0 : 0.0) : 2.0 * (u - 1), v == 1 ? -2.0 : 2.0,
                      u == 3 ? 16 * coefficient : 0.0});
  return new Geom_BezierSurface(poles);
}
Result<SurfaceJoinReport> verify(const TopoDS_Edge& edge, const TopoDS_Face& candidate,
                                 const TopoDS_Face& support, LoftBoundaryContinuity continuity,
                                 double curvature = 1e-5, bool reverse = false) {
  return verify_surface_join(FeatureId("test.join"), edge, candidate, support, continuity, 1e-7,
                             1e-5, curvature, reverse);
}
} // namespace
TEST_CASE("Native G2 verification distinguishes transverse curvature from a "
          "C1-only cylinder join",
          "[geometry][surface-join-verification][parity-b169]") {
  const Handle(Geom_Surface) cylinder =
      new Geom_CylindricalSurface(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()), 10);
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp_Pnt(10, 0, 0), gp::DX(), gp::DY()));
  const auto candidate = patch(cylinder, 0, .6, -3, 3);
  const auto support = patch(plane, -2, 0, -3, 3);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(10, 0, -3), gp_Pnt(10, 0, 3)).Edge();
  REQUIRE(verify(edge, candidate, support, LoftBoundaryContinuity::G1));
  const auto failed = verify(edge, candidate, support, LoftBoundaryContinuity::G2, .01);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().message().find("curvature") != std::string::npos);
  // Along the common line both curvatures are zero; the transverse cylinder
  // curvature is -1/R.
  REQUIRE(verify(edge, candidate, support, LoftBoundaryContinuity::G2, .11));
}
TEST_CASE("Exact cylindrical continuation retains native G2 under rigid "
          "placement and explicit normal reversal",
          "[geometry][surface-join-verification][parity-b169]") {
  const Handle(Geom_Surface) cylinder =
      new Geom_CylindricalSurface(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()), 10);
  const auto candidate = patch(cylinder, 0, .6, -3, 3);
  const auto support = patch(cylinder, -.4, 0, -3, 3);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(10, 0, -3), gp_Pnt(10, 0, 3)).Edge();
  const auto report = verify(edge, candidate, support, LoftBoundaryContinuity::G2);
  REQUIRE(report);
  CHECK(report.value().stations == 129);
  CHECK(report.value().position_error_mm < 1e-10);
  CHECK(report.value().normal_error_rad < 1e-10);
  CHECK(report.value().curvature_error_per_mm < 1e-10);
  auto reversed = support;
  reversed.Reverse();
  REQUIRE_FALSE(verify(edge, candidate, reversed, LoftBoundaryContinuity::G2));
  REQUIRE(verify(edge, candidate, reversed, LoftBoundaryContinuity::G2, 1e-5, true));
  gp_Trsf rotation, translation;
  rotation.SetRotation(gp_Ax1(gp::Origin(), gp_Dir(1, 2, 3)), .7);
  translation.SetTranslation(gp_Vec(200, -30, 9));
  const auto pose = translation * rotation;
  const auto moved_edge = TopoDS::Edge(BRepBuilderAPI_Transform(edge, pose, true).Shape());
  const auto moved_candidate =
      TopoDS::Face(BRepBuilderAPI_Transform(candidate, pose, true).Shape());
  const auto moved_support = TopoDS::Face(BRepBuilderAPI_Transform(support, pose, true).Shape());
  REQUIRE(verify(moved_edge, moved_candidate, moved_support, LoftBoundaryContinuity::G2));
}
TEST_CASE("Native second fundamental form enforces a nearly failing quadratic "
          "boundary",
          "[geometry][surface-join-verification][parity-b169]") {
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto support = patch(plane, -4, 0, -2, 2);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, -2, 0), gp_Pnt(0, 2, 0)).Edge();
  const auto passing = patch(quadratic(4e-6), 0, 1, 0, 1);
  const auto failing = patch(quadratic(6e-6), 0, 1, 0, 1);
  const auto result = verify(edge, passing, support, LoftBoundaryContinuity::G2);
  REQUIRE(result);
  // Independent polynomial oracle: z=a*x*x has signed transverse curvature 2a
  // at x=0.
  CHECK(result.value().curvature_error_per_mm == Catch::Approx(8e-6).margin(1e-11));
  REQUIRE(verify(edge, failing, support, LoftBoundaryContinuity::G1));
  REQUIRE_FALSE(verify(edge, failing, support, LoftBoundaryContinuity::G2));
}
TEST_CASE("Continuity verification rejects singular differential frames and "
          "infinite-carrier impostors",
          "[geometry][surface-join-verification][parity-b169]") {
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto support = patch(plane, -4, 0, -2, 2);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, -2, 0), gp_Pnt(0, 2, 0)).Edge();
  const auto singular = patch(quadratic(0, true), 0, 1, 0, 1);
  REQUIRE_FALSE(verify(edge, singular, support, LoftBoundaryContinuity::G2));
  const auto outside = patch(plane, 1, 3, -2, 2);
  REQUIRE_FALSE(verify(edge, patch(plane, 0, 4, -2, 2), outside, LoftBoundaryContinuity::G2));
  const Handle(Geom_Surface) sphere =
      new Geom_SphericalSurface(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()), 5);
  const auto meridian =
      GC_MakeArcOfCircle(gp_Pnt(5, 0, 0), gp_Pnt(5 / std::sqrt(2), 0, 5 / std::sqrt(2)),
                         gp_Pnt(0, 0, 5))
          .Value();
  const auto pole_edge = BRepBuilderAPI_MakeEdge(meridian).Edge();
  REQUIRE(verify(pole_edge, patch(sphere, 0, .5, 0, std::numbers::pi / 2),
                 patch(sphere, -.5, 0, 0, std::numbers::pi / 2), LoftBoundaryContinuity::G1));
  REQUIRE(verify(pole_edge, patch(sphere, 0, .5, 0, std::numbers::pi / 2),
                 patch(sphere, -.5, 0, 0, std::numbers::pi / 2), LoftBoundaryContinuity::G1));
  REQUIRE_FALSE(verify(pole_edge, patch(sphere, 0, .5, 0, std::numbers::pi / 2),
                       patch(sphere, -.5, 0, 0, std::numbers::pi / 2), LoftBoundaryContinuity::G2));
}

TEST_CASE("Native positional and angular join limits retain their independent units",
          "[geometry][surface-join-verification][parity-b169]") {
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto support = patch(plane, -4, 0, -2, 2);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, -2, 0), gp_Pnt(0, 2, 0)).Edge();
  for (const auto gap : {9e-8, 11e-8}) {
    const Handle(Geom_Surface) offset =
        new Geom_Plane(gp_Ax3(gp_Pnt(0, 0, gap), gp::DZ(), gp::DX()));
    const auto result =
        verify(edge, patch(offset, 0, 4, -2, 2), support, LoftBoundaryContinuity::G2);
    if (gap < 1e-7) {
      REQUIRE(result);
      CHECK(result.value().position_error_mm == Catch::Approx(gap).margin(1e-12));
    } else {
      REQUIRE_FALSE(result);
    }
  }
  for (const auto radians : {9e-6, 11e-6}) {
    const Handle(Geom_Surface) tilted =
        new Geom_Plane(gp_Ax3(gp::Origin(), gp_Dir(-std::sin(radians), 0, std::cos(radians)),
                              gp_Dir(std::cos(radians), 0, std::sin(radians))));
    const auto result =
        verify(edge, patch(tilted, 0, 4, -2, 2), support, LoftBoundaryContinuity::G1);
    if (radians < 1e-5) {
      REQUIRE(result);
      CHECK(result.value().normal_error_rad == Catch::Approx(radians).margin(1e-12));
    } else {
      REQUIRE_FALSE(result);
    }
  }
}

TEST_CASE("Diagonal normal curvature detects a mixed second fundamental form impostor",
          "[geometry][surface-join-verification][parity-b169]") {
  constexpr double a = 1e-4;
  TColgp_Array2OfPnt poles(1, 2, 1, 2);
  for (int u = 1; u <= 2; ++u)
    for (int v = 1; v <= 2; ++v) {
      const double x = u == 1 ? 0 : 4, y = v == 1 ? -.01 : .01;
      poles.SetValue(u, v, {x, y, a * x * y});
    }
  const Handle(Geom_Surface) saddle = new Geom_BezierSurface(poles);
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto support = patch(plane, -4, 0, -.01, .01);
  const auto candidate = patch(saddle, 0, 1, 0, 1);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, -.01, 0), gp_Pnt(0, .01, 0)).Edge();
  REQUIRE(verify(edge, candidate, support, LoftBoundaryContinuity::G1));
  REQUIRE_FALSE(verify(edge, candidate, support, LoftBoundaryContinuity::G2, .9 * a));
  const auto report = verify(edge, candidate, support, LoftBoundaryContinuity::G2, 1.1 * a);
  REQUIRE(report);
  // Independent z=a*x*y oracle: both axial curvatures vanish, diagonal curvature is +/-a at the
  // origin.
  CHECK(report.value().curvature_error_per_mm == Catch::Approx(a).margin(1e-10));
}

TEST_CASE(
    "G2 verification bounds the complete normal curvature tensor rather than three directions",
    "[geometry][surface-join-verification][surface-join-spectrum][parity-b169]") {
  constexpr double q = 8e-6;
  // z = .5*q*x*x + q*x*y - .5*q*y*y. Every tested axial/diagonal value is <=q,
  // but the independent Hessian eigenvalues are +/-sqrt(2)*q > 1e-5.
  TColgp_Array2OfPnt poles(1, 3, 1, 3);
  for (int u = 1; u <= 3; ++u)
    for (int v = 1; v <= 3; ++v) {
      const double x = 2.0 * (u - 1), y = .01 * (v - 2);
      const double x2 = u == 3 ? 16.0 : 0.0, y2 = v == 2 ? -1e-4 : 1e-4;
      poles.SetValue(u, v, {x, y, .5 * q * x2 + q * x * y - .5 * q * y2});
    }
  const Handle(Geom_Surface) graph = new Geom_BezierSurface(poles);
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto candidate = patch(graph, 0, 1, 0, 1);
  const auto support = patch(plane, -4, 0, -.01, .01);
  TColgp_Array1OfPnt boundary_poles(1, 3);
  for (int v = 1; v <= 3; ++v)
    boundary_poles.SetValue(v, poles.Value(1, v));
  const Handle(Geom_Curve) boundary = new Geom_BezierCurve(boundary_poles);
  const auto edge = BRepBuilderAPI_MakeEdge(boundary).Edge();
  REQUIRE(verify(edge, candidate, support, LoftBoundaryContinuity::G1));
  REQUIRE_FALSE(verify(edge, candidate, support, LoftBoundaryContinuity::G2));
  const auto report = verify(edge, candidate, support, LoftBoundaryContinuity::G2, 1.2e-5);
  REQUIRE(report);
  CHECK(report.value().curvature_error_per_mm == Catch::Approx(std::sqrt(2) * q).margin(1e-10));
}

TEST_CASE("Trim incidence uses physical millimeters with strongly scaled native parameters",
          "[geometry][surface-join-verification][parity-b169]") {
  TColgp_Array2OfPnt poles(1, 2, 1, 2);
  for (int u = 1; u <= 2; ++u)
    for (int v = 1; v <= 2; ++v)
      poles.SetValue(u, v, gp_Pnt(u - 2, v == 1 ? -2 : 2, 0));
  TColStd_Array1OfInteger multiplicities(1, 2);
  multiplicities.Init(2);
  for (const double scale : {.001, 1000.0}) {
    TColStd_Array1OfReal uknots(1, 2), vknots(1, 2);
    uknots.SetValue(1, 0);
    uknots.SetValue(2, scale);
    vknots.SetValue(1, 0);
    vknots.SetValue(2, 1);
    const Handle(Geom_Surface) surface =
        new Geom_BSplineSurface(poles, uknots, vknots, multiplicities, multiplicities, 1, 1);
    const auto face = patch(surface, 0, scale, 0, 1);
    for (const double gap : {0.0, 1e-8, 1e-5}) {
      const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(-gap, -1, 0), gp_Pnt(-gap, 1, 0)).Edge();
      const auto result = verify(edge, face, face, LoftBoundaryContinuity::G2);
      INFO("native U scale=" << scale << ", physical gap=" << gap);
      if (gap <= 1e-7) {
        REQUIRE(result);
        CHECK(result.value().position_error_mm == Catch::Approx(gap).margin(1e-12));
      } else {
        REQUIRE_FALSE(result);
      }
    }
  }
}

TEST_CASE("Native support knot crossings expose narrow curvature spikes between uniform stations",
          "[geometry][surface-join-verification][surface-join-knots][parity-b169]") {
  const std::array<double, 5> values{0, .30001, .30002, .30003, 1};
  TColgp_Array2OfPnt poles(1, 3, 1, 5);
  for (int u = 1; u <= 3; ++u)
    for (int v = 1; v <= 5; ++v)
      poles(u, v) = gp_Pnt(2.0 * (u - 1), 4 * values[v - 1], u == 3 && v == 3 ? .016 : 0);
  TColStd_Array1OfReal uk(1, 2), vk(1, 5);
  TColStd_Array1OfInteger um(1, 2), vm(1, 5);
  uk(1) = 0;
  uk(2) = 1;
  um.Init(3);
  for (int i = 1; i <= 5; ++i) {
    vk(i) = values[i - 1];
    vm(i) = i == 1 || i == 5 ? 2 : 1;
  }
  const Handle(Geom_Surface) surface = new Geom_BSplineSurface(poles, uk, vk, um, vm, 2, 1);
  const auto candidate = patch(surface, 0, 1, 0, 1);
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto support = patch(plane, -4, 0, 0, 4);
  const auto boundary = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(0, 4, 0)).Edge();
  const auto tangent = verify(boundary, candidate, support, LoftBoundaryContinuity::G1);
  REQUIRE(tangent);
  CHECK(tangent.value().stations > 129);
  // At v=.30002, z=.001*x*x, hence transverse curvature .002/mm.
  // All 129 uniform boundary stations miss the entire nonzero interval.
  const auto failed = verify(boundary, candidate, support, LoftBoundaryContinuity::G2);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().message().find("curvature") != std::string::npos);
}

TEST_CASE("Extruded trimmed spline supports expose hidden native knot tangent spikes",
          "[geometry][surface-join-verification][surface-join-knots][parity-b169]") {
  const std::array<double, 5> values{0, .30001, .30002, .30003, 1};
  TColgp_Array1OfPnt poles(1, 5);
  TColStd_Array1OfReal knots(1, 5);
  TColStd_Array1OfInteger mult(1, 5);
  for (int i = 1; i <= 5; ++i) {
    poles(i) = gp_Pnt(4 * values[i - 1], 0, i == 3 ? 1e-8 : 0);
    knots(i) = values[i - 1];
    mult(i) = i == 1 || i == 5 ? 2 : 1;
  }
  const Handle(Geom_Curve) spline = new Geom_BSplineCurve(poles, knots, mult, 1);
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  const auto candidate = patch(plane, 0, 4, 0, 2);
  const auto edge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(4, 0, 0)).Edge();
  for (const bool wrapped : {false, true}) {
    Handle(Geom_Curve) basis = spline;
    if (wrapped)
      basis = new Geom_TrimmedCurve(spline, 0, 1);
    const Handle(Geom_Surface) surface = new Geom_SurfaceOfLinearExtrusion(basis, gp::DY());
    const auto support = patch(surface, 0, 1, -2, 0);
    // The positional deviation is only 1e-8 mm, but a knot-side slope is
    // 1e-8 / (4e-5)=.00025 rad, above the independent 1e-5 normal budget.
    const auto failed = verify(edge, candidate, support, LoftBoundaryContinuity::G1);
    INFO(wrapped);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().message().find("normal") != std::string::npos);
  }
}
