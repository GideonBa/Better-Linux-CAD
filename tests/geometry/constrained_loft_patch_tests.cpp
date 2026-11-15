#include "../../src/geometry/constrained_loft_patch.hpp"
#include "../support/loft_section_refinement_fixture.hpp"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomConvert_CompCurveToBSplineCurve.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <gp.hxx>
#include <gp_Ax3.hxx>
#include <numbers>
#include <sstream>
#include <string>
#include <utility>
using namespace blcad;
using namespace blcad::geometry::detail;
namespace {
template <class T> T checked(Result<T> value) {
  INFO((value ? std::string("ok") : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
TopoDS_Face patch(const Handle(Geom_Surface) & surface, double u0, double u1, double v0,
                  double v1) {
  BRepBuilderAPI_MakeFace made(surface, u0, u1, v0, v1, 1e-9);
  REQUIRE(made.IsDone());
  return made.Face();
}
TopoDS_Edge line(gp_Pnt a, gp_Pnt b) {
  return BRepBuilderAPI_MakeEdge(a, b).Edge();
}
TopoDS_Edge arch(double y, bool reversed = false) {
  TColgp_Array1OfPnt poles(1, 7);
  for (int i = 0; i <= 6; ++i)
    poles.SetValue(i + 1, {10.0 * i / 6.0, y, i == 3 ? 3.2 : 0.0});
  const Handle(Geom_Curve) curve = new Geom_BezierCurve(poles);
  auto edge = BRepBuilderAPI_MakeEdge(curve).Edge();
  if (reversed)
    edge.Reverse();
  return edge;
}
std::vector<LoftPatchBoundary> strip() {
  const Handle(Geom_Surface) plane = new Geom_Plane(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
  return {{line({0, -2, 0}, {0, 2, 0}), patch(plane, -4, 0, -2, 2), LoftBoundaryContinuity::G2},
          {arch(2), {}},
          {line({10, 2, 0}, {10, -2, 0}), patch(plane, 10, 14, -2, 2), LoftBoundaryContinuity::G2},
          {arch(-2, true), {}}};
}
std::string snapshot(const std::vector<LoftPatchBoundary>& boundaries) {
  std::ostringstream stream;
  for (const auto& boundary : boundaries) {
    BRepTools::Write(boundary.curve, stream);
    if (boundary.support)
      BRepTools::Write(*boundary.support, stream);
  }
  return stream.str();
}
void on_face(const TopoDS_Face& face, gp_Pnt p) {
  BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(p).Vertex(), face);
  REQUIRE(distance.IsDone());
  CHECK(distance.Value() <= 1e-5);
}
GeomLProp_SLProps properties(const TopoDS_Face& face, gp_Pnt point) {
  const auto surface = BRep_Tool::Surface(face);
  GeomAPI_ProjectPointOnSurf projected(point, surface);
  REQUIRE(projected.NbPoints() > 0);
  REQUIRE(projected.LowerDistance() <= 1e-5);
  double u, v;
  projected.LowerDistanceParameters(u, v);
  GeomLProp_SLProps properties(surface, u, v, 2, 1e-12);
  REQUIRE(properties.IsNormalDefined());
  REQUIRE(properties.IsCurvatureDefined());
  return properties;
}
} // namespace

TEST_CASE("Constrained native Loft patch follows sextic rails and G2 plane supports",
          "[geometry][constrained-loft-patch][parity-b169]") {
  const auto boundaries = strip();
  const auto before = snapshot(boundaries);
  const auto middle = line({5, -2, 1}, {5, 2, 1});
  const auto result = checked(build_constrained_loft_patch(FeatureId("smooth.strip"), boundaries,
                                                           {middle}, 1e-5, 1e-4, 1e-4));
  REQUIRE(BRepCheck_Analyzer(result.face, true).IsValid());
  REQUIRE(result.joins.size() == 2);
  CHECK(snapshot(boundaries) == before);
  for (int i = 0; i <= 16; ++i) {
    const double t = double(i) / 16, z = 64 * t * t * t * (1 - t) * (1 - t) * (1 - t),
                 y = -2 + 4 * t;
    on_face(result.face, {10 * t, -2, z});
    on_face(result.face, {10 * t, 2, z});
    on_face(result.face, {5, y, 1});
    for (const double x : {0.0, 10.0}) {
      auto p = properties(result.face, {x, y, 0});
      auto normal = p.Normal();
      if (result.face.Orientation() == TopAbs_REVERSED)
        normal.Reverse();
      CHECK(normal.Z() >= std::cos(1e-4));
      CHECK(std::abs(p.MaxCurvature()) <= 1e-4);
      CHECK(std::abs(p.MinCurvature()) <= 1e-4);
    }
  }
}

TEST_CASE(
    "Constrained native Loft patch retains cylindrical radius sections and boundary curvature",
    "[geometry][constrained-loft-patch][parity-b169]") {
  const Handle(Geom_Surface) cylinder =
      new Geom_CylindricalSurface(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()), 10);
  const auto arc = [](double z, bool reversed = false) {
    const auto curve = GC_MakeArcOfCircle(gp_Pnt(10, 0, z), gp_Pnt(std::sqrt(50), std::sqrt(50), z),
                                          gp_Pnt(0, 10, z))
                           .Value();
    auto edge = BRepBuilderAPI_MakeEdge(curve).Edge();
    if (reversed)
      edge.Reverse();
    return edge;
  };
  const std::vector<LoftPatchBoundary> boundaries{
      {arc(0), patch(cylinder, 0, std::numbers::pi / 2, -4, 0), LoftBoundaryContinuity::G2},
      {line({0, 10, 0}, {0, 10, 10}), {}},
      {arc(10, true), patch(cylinder, 0, std::numbers::pi / 2, 10, 14), LoftBoundaryContinuity::G2},
      {line({10, 0, 10}, {10, 0, 0}), {}}};
  const auto before = snapshot(boundaries);
  const auto result = checked(build_constrained_loft_patch(FeatureId("smooth.cylinder"), boundaries,
                                                           {arc(5)}, 1e-5, 1e-4, 1e-4));
  CHECK(snapshot(boundaries) == before);
  REQUIRE(BRepCheck_Analyzer(result.face, true).IsValid());
  GProp_GProps area;
  BRepGProp::SurfaceProperties(result.face, area);
  CHECK(area.Mass() == Catch::Approx(50 * std::numbers::pi).margin(1e-3));
  for (int i = 0; i <= 16; ++i) {
    const double angle = std::numbers::pi * .5 * i / 16;
    for (const double z : {0.0, 5.0, 10.0}) {
      const gp_Pnt point(10 * std::cos(angle), 10 * std::sin(angle), z);
      on_face(result.face, point);
      if (z == 5)
        continue;
      auto p = properties(result.face, point);
      const double first = std::abs(p.MaxCurvature()), second = std::abs(p.MinCurvature());
      CHECK(std::min(first, second) <= 1e-4);
      CHECK(std::max(first, second) == Catch::Approx(.1).margin(1e-4));
      auto normal = p.Normal();
      if (result.face.Orientation() == TopAbs_REVERSED)
        normal.Reverse();
      CHECK(normal.Dot(gp_Dir(std::cos(angle), std::sin(angle), 0)) >= std::cos(1e-4));
    }
  }
}

TEST_CASE("Constrained native Loft patch rejects incompatible normals without touching its sources",
          "[geometry][constrained-loft-patch][parity-b169]") {
  auto boundaries = strip();
  boundaries[2].reverse_normal = true;
  const auto before = snapshot(boundaries);
  REQUIRE_FALSE(build_constrained_loft_patch(FeatureId("bad.normals"), boundaries,
                                             {line({5, -2, 1}, {5, 2, 1})}, 1e-5, 1e-4, 1e-4));
  CHECK(snapshot(boundaries) == before);
  boundaries[2].reverse_normal = false;
  std::swap(boundaries[0], boundaries[1]);
  REQUIRE_FALSE(
      build_constrained_loft_patch(FeatureId("bad.order"), boundaries, {}, 1e-5, 1e-4, 1e-4));
}

TEST_CASE("Native constrained Loft retains unequal rolled airfoil section spans with exact rails",
          "[geometry][constrained-loft-patch][parity-b169]") {
  namespace air = blcad::test::loft_refinement;
  const auto source = air::sections();
  const auto profile = [&](std::size_t station) {
    GeomConvert_CompCurveToBSplineCurve joined;
    for (std::size_t i = 0; i < air::counts[station][0]; ++i) {
      const auto& span = source[station][i];
      TColgp_Array1OfPnt poles(1, 4);
      const std::array points{span.start, span.mid, span.control2, span.end};
      for (int j = 0; j < 4; ++j)
        poles.SetValue(j + 1, {points[j].x, points[j].y, points[j].z});
      const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
      REQUIRE(joined.Add(curve, 1e-10, true, true, 0));
    }
    return joined.BSplineCurve();
  };
  const auto root = profile(0), middle = profile(1), tip = profile(2);
  const auto rail = [&](bool last) {
    const auto at = [last](const Handle(Geom_BSplineCurve) & curve) {
      return curve->Value(last ? curve->LastParameter() : curve->FirstParameter());
    };
    const auto a = at(root), b = at(tip), m = at(middle);
    const gp_Vec d(0, 0, 40);
    TColgp_Array1OfPnt poles(1, 6);
    poles.SetValue(1, a);
    poles.SetValue(2, a.Translated(d / 5));
    poles.SetValue(3, a.Translated(d * (2.0 / 5)));
    poles.SetValue(4, b.Translated(d * (-2.0 / 5)));
    poles.SetValue(5, b.Translated(d * (-1.0 / 5)));
    poles.SetValue(6, b);
    Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
    curve->Increase(6);
    const gp_Vec correction(curve->Value(.5), m);
    curve->SetPole(4, curve->Pole(4).Translated(correction * 3.2));
    auto edge = BRepBuilderAPI_MakeEdge(curve).Edge();
    if (!last)
      edge.Reverse();
    return edge;
  };
  const Handle(Geom_Surface) root_surface = new Geom_SurfaceOfLinearExtrusion(root, gp::DZ());
  const Handle(Geom_Surface) tip_surface = new Geom_SurfaceOfLinearExtrusion(tip, gp::DZ());
  auto tip_edge = BRepBuilderAPI_MakeEdge(tip).Edge();
  tip_edge.Reverse();
  const std::vector<LoftPatchBoundary> boundaries{
      {BRepBuilderAPI_MakeEdge(root).Edge(),
       patch(root_surface, root->FirstParameter(), root->LastParameter(), -5, 0),
       LoftBoundaryContinuity::G2},
      {rail(true), {}},
      {tip_edge, patch(tip_surface, tip->FirstParameter(), tip->LastParameter(), 0, 5),
       LoftBoundaryContinuity::G2},
      {rail(false), {}}};
  const auto before = snapshot(boundaries);
  const auto result = checked(build_constrained_loft_patch(FeatureId("rolled.airfoil"), boundaries,
                                                           {BRepBuilderAPI_MakeEdge(middle).Edge()},
                                                           1e-5, 1e-4, 1e-4));
  CHECK(snapshot(boundaries) == before);
  for (int i = 0; i <= 16; ++i) {
    const double t = double(i) / 16;
    for (std::size_t station = 0; station < 3; ++station) {
      const auto point = air::model(station, air::at(air::poles(station, true), t));
      on_face(result.face, {point.x, point.y, point.z});
      if (station == 1)
        continue;
      const auto p = air::poles(station, true);
      auto d1 = air::derivative(p, t);
      Point2 d2{
          6 * ((1 - t) * (p[2].x - 2 * p[1].x + p[0].x) + t * (p[3].x - 2 * p[2].x + p[1].x)),
          6 * ((1 - t) * (p[2].y - 2 * p[1].y + p[0].y) + t * (p[3].y - 2 * p[2].y + p[1].y))};
      const double angle = station == 0 ? 0 : -.08;
      d1.x *= std::cos(angle);
      d2.x *= std::cos(angle);
      const double expected =
          std::abs(d1.x * d2.y - d1.y * d2.x) / std::pow(d1.x * d1.x + d1.y * d1.y, 1.5);
      auto measured = properties(result.face, {point.x, point.y, point.z});
      const double k1 = std::abs(measured.MaxCurvature()), k2 = std::abs(measured.MinCurvature());
      CHECK(std::min(k1, k2) <= 1e-4);
      CHECK(std::max(k1, k2) == Catch::Approx(expected).margin(1e-4));
    }
  }
}
