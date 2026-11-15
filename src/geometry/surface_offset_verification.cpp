#include "surface_offset_verification.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_OffsetSurface.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Lin.hxx>
#include <limits>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
struct Domain {
  Handle(Geom_Surface) surface;
  std::vector<double> u, v;
};
Domain domain(const TopoDS_Face& face) {
  Domain result;
  result.surface = BRep_Tool::Surface(face);
  double u0, u1, v0, v1;
  BRepTools::UVBounds(face, u0, u1, v0, v1);
  if (!std::isfinite(u0) || !std::isfinite(u1) || !std::isfinite(v0) || !std::isfinite(v1) ||
      u0 >= u1 || v0 >= v1)
    throw std::runtime_error("Offset requires finite nondegenerate trimmed domains");
  result.u = {u0, u1};
  result.v = {v0, v1};
  auto basis = result.surface;
  for (int depth = 0; depth < 16; ++depth) {
    if (const auto trimmed = Handle(Geom_RectangularTrimmedSurface)::DownCast(basis))
      basis = trimmed->BasisSurface();
    else if (const auto offset = Handle(Geom_OffsetSurface)::DownCast(basis))
      basis = offset->BasisSurface();
    else
      break;
    if (depth == 15)
      throw std::runtime_error("Offset surface nesting exceeds the verification budget");
  }
  if (!basis->IsCNu(2) || !basis->IsCNv(2))
    throw std::runtime_error(
        "Verified offset requires native C2 patches; sharp knots are unsupported");
  if (const auto spline = Handle(Geom_BSplineSurface)::DownCast(basis)) {
    for (int i = 1; i <= spline->NbUKnots(); ++i)
      if (spline->UKnot(i) > u0 && spline->UKnot(i) < u1)
        result.u.push_back(spline->UKnot(i));
    for (int i = 1; i <= spline->NbVKnots(); ++i)
      if (spline->VKnot(i) > v0 && spline->VKnot(i) < v1)
        result.v.push_back(spline->VKnot(i));
  }
  Handle(Geom_Curve) generating;
  bool along_u = true;
  if (const auto extrusion = Handle(Geom_SurfaceOfLinearExtrusion)::DownCast(basis))
    generating = extrusion->BasisCurve();
  else if (const auto revolution = Handle(Geom_SurfaceOfRevolution)::DownCast(basis)) {
    generating = revolution->BasisCurve();
    along_u = false;
  }
  if (!generating.IsNull()) {
    while (const auto trimmed = Handle(Geom_TrimmedCurve)::DownCast(generating))
      generating = trimmed->BasisCurve();
    if (const auto spline = Handle(Geom_BSplineCurve)::DownCast(generating)) {
      auto& knots = along_u ? result.u : result.v;
      const double low = along_u ? u0 : v0, high = along_u ? u1 : v1;
      for (int i = 1; i <= spline->NbKnots(); ++i)
        if (spline->Knot(i) > low && spline->Knot(i) < high)
          knots.push_back(spline->Knot(i));
    }
  }
  std::sort(result.u.begin(), result.u.end());
  std::sort(result.v.begin(), result.v.end());
  if ((result.u.size() - 1) * (result.v.size() - 1) > 128)
    throw std::runtime_error("Offset exceeds the native verification cell budget");
  return result;
}
template <class Function>
std::size_t stations(const TopoDS_Face& face, const Domain& data, int subdivisions, Function check,
                     bool include_boundary = false) {
  std::size_t count = 0;
  for (std::size_t i = 1; i < data.u.size(); ++i)
    for (std::size_t j = 1; j < data.v.size(); ++j)
      for (int a = 0; a < subdivisions + int(include_boundary); ++a)
        for (int b = 0; b < subdivisions + int(include_boundary); ++b) {
          // Interior stations avoid native intersections at side-wall/skin junctions.
          const double u =
              std::lerp(data.u[i - 1], data.u[i], (a + (include_boundary ? 0 : .5)) / subdivisions);
          const double v =
              std::lerp(data.v[j - 1], data.v[j], (b + (include_boundary ? 0 : .5)) / subdivisions);
          BRepClass_FaceClassifier classifier(face, gp_Pnt2d(u, v), 1e-12);
          if (classifier.State() != TopAbs_IN &&
              !(include_boundary && classifier.State() == TopAbs_ON))
            continue;
          GeomLProp_SLProps properties(data.surface, u, v, 2, 1e-12);
          if (!properties.IsTangentUDefined() || !properties.IsTangentVDefined() ||
              !properties.IsNormalDefined() || !properties.IsCurvatureDefined())
            throw std::runtime_error("Offset source has a singular normal or curvature");
          const auto point = properties.Value();
          auto normal = properties.Normal();
          const double sign = face.Orientation() == TopAbs_REVERSED ? -1 : 1;
          if (sign < 0)
            normal.Reverse();
          const double first = sign * properties.MinCurvature(),
                       last = sign * properties.MaxCurvature();
          if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()) ||
              !std::isfinite(first) || !std::isfinite(last))
            throw std::runtime_error("Offset source has nonfinite native differential geometry");
          check(point, normal, first, last);
          ++count;
        }
  if (!count)
    throw std::runtime_error("Offset trim contains no resolvable interior verification stations");
  return count;
}
double normal_hit(IntCurvesFace_ShapeIntersector& ray, const gp_Pnt& point, const gp_Dir& normal,
                  double expected, double budget) {
  if (expected == 0)
    return 0;
  // OCCT can miss an OffsetSurface root when its search interval is clipped
  // to the error budget. Search a physical interval, then enforce the same budget.
  const double search = std::max(std::abs(expected), 4 * budget);
  ray.Perform(gp_Lin(point, normal), expected - search, expected + search);
  if (!ray.IsDone() || ray.NbPnt() != 1)
    throw std::runtime_error(
        "Offset skin has no unique native normal-ray intersection within budget: expected=" +
        std::to_string(expected) + ", done=" + std::to_string(ray.IsDone()) +
        ", hits=" + std::to_string(ray.NbPnt()) + ", source=(" + std::to_string(point.X()) + "," +
        std::to_string(point.Y()) + "," + std::to_string(point.Z()) + ")");
  const double measured = ray.WParameter(1);
  if (!std::isfinite(measured) || std::abs(measured - expected) > budget)
    throw std::runtime_error("Offset skin exceeds the declared physical distance error");
  return measured;
}
} // namespace
void validate_offset_source(const TopoDS_Shape& shape, double first, double last, double tolerance,
                            bool approximate) {
  TopTools_IndexedDataMapOfShapeListOfShape edges;
  TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edges);
  TopTools_IndexedMapOfShape faces;
  TopExp::MapShapes(shape, TopAbs_FACE, faces);
  std::vector<int> component(faces.Extent());
  for (int i = 0; i < faces.Extent(); ++i)
    component[i] = i;
  for (int i = 1; i <= edges.Extent(); ++i) {
    const auto& uses = edges.FindFromIndex(i);
    if (uses.Extent() > 2)
      throw std::runtime_error("Offset rejects non-manifold quilt boundaries");
    if (uses.Extent() != 2)
      continue;
    const auto a = TopoDS::Face(uses.First()), b = TopoDS::Face(uses.Last());
    const int ca = component[faces.FindIndex(a) - 1], cb = component[faces.FindIndex(b) - 1];
    for (auto& value : component)
      if (value == cb)
        value = ca;
    if (a.IsSame(b))
      continue; // A native periodic seam has two uses of one face.
    BRepAdaptor_Curve curve(TopoDS::Edge(edges.FindKey(i)));
    for (int j = 0; j <= 32; ++j) {
      const auto point =
          curve.Value(std::lerp(curve.FirstParameter(), curve.LastParameter(), j / 32.0));
      std::vector<gp_Dir> normals;
      for (const auto& face : {a, b}) {
        const auto surface = BRep_Tool::Surface(face);
        GeomAPI_ProjectPointOnSurf projection(point, surface);
        if (!projection.IsDone() || projection.NbPoints() == 0 ||
            projection.LowerDistance() > tolerance)
          throw std::runtime_error("Offset quilt seam has no native surface incidence");
        double u, v;
        projection.LowerDistanceParameters(u, v);
        GeomLProp_SLProps properties(surface, u, v, 1, 1e-12);
        if (!properties.IsNormalDefined())
          throw std::runtime_error("Offset quilt has a singular seam");
        auto normal = properties.Normal();
        if (face.Orientation() == TopAbs_REVERSED)
          normal.Reverse();
        normals.push_back(normal);
      }
      if (normals[0].Dot(normals[1]) < 1 - 1e-8)
        throw std::runtime_error("Simple offset supports consistently oriented G1 quilts; sharp "
                                 "joins require explicit construction");
    }
  }
  if (!component.empty() && std::any_of(component.begin(), component.end(),
                                        [&](int c) { return c != component.front(); }))
    throw std::runtime_error(
        "Offset requires one connected quilt; disconnected regions need separate features");
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    const auto type = BRepAdaptor_Surface(face).GetType();
    if (!approximate && type != GeomAbs_Plane && type != GeomAbs_Cylinder)
      throw std::runtime_error("Exact-analytic offset supports planes and cylinders; choose "
                               "verified approximation for curved patches");
    const auto data = domain(face);
    stations(
        face, data, 17,
        [&](const gp_Pnt&, const gp_Dir&, double k1, double k2) {
          for (const double distance : {first, last})
            for (const double k : {k1, k2})
              if (distance != 0 &&
                  1 - distance * k <=
                      std::max(1e-6, 10 * tolerance / std::max(std::abs(distance), tolerance)))
                throw std::runtime_error(
                    "Offset crosses a local curvature singularity or collapses a narrow concavity");
        },
        true);
  }
}
SurfaceDistanceReport verify_offset_skins(const std::vector<OffsetSkinPair>& pairs,
                                          double requested, double budget,
                                          SurfaceDistanceCriterion criterion, bool approximate) {
  SurfaceDistanceReport report{criterion, requested,   std::numeric_limits<double>::infinity(),
                               0,         0,           budget,
                               0,         approximate, true};
  if (pairs.empty())
    throw std::runtime_error("Offset has no verified source-to-skin history");
  for (const auto& pair : pairs) {
    const auto data = domain(pair.source);
    IntCurvesFace_ShapeIntersector first, last;
    if (pair.first_distance != 0)
      first.Load(pair.first_skin, budget * .1);
    if (pair.last_distance != 0)
      last.Load(pair.last_skin, budget * .1);
    double previous = -1;
    bool converged = false;
    for (const int density : {9, 17, 33}) {
      double maximum = 0;
      report.stations += stations(
          pair.source, data, density,
          [&](const gp_Pnt& point, const gp_Dir& normal, double, double) {
            const double a = normal_hit(first, point, normal, pair.first_distance, budget * .5);
            const double b = normal_hit(last, point, normal, pair.last_distance, budget * .5);
            const double separation = std::abs(b - a);
            const double error = std::abs(separation - requested);
            if (error > budget)
              throw std::runtime_error("Measured wall/skin separation exceeds the declared error");
            maximum = std::max(maximum, error);
            report.minimum_mm = std::min(report.minimum_mm, separation);
            report.maximum_mm = std::max(report.maximum_mm, separation);
            report.maximum_error_mm = std::max(report.maximum_error_mm, error);
          });
      if (previous >= 0 && std::abs(maximum - previous) <= budget * .05) {
        converged = true;
        break;
      }
      previous = maximum;
    }
    if (!converged)
      throw std::runtime_error(
          "Offset distance refinement did not converge within its station budget");
  }
  return report;
}
} // namespace blcad::geometry::detail
