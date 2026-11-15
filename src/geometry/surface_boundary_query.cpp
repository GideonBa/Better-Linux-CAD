#include "blcad/geometry/surface_boundary_query.hpp"
#include "geometry_shape_internal.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS_Edge.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace blcad::geometry {
namespace {
double directed_gap(const TopoDS_Edge& first, const TopoDS_Edge& second, double tolerance) {
  BRepAdaptor_Curve curve(first);
  const double start = curve.FirstParameter(), end = curve.LastParameter();
  const double integration = std::max(1e-12, tolerance * .05);
  const double length = GCPnts_AbscissaPoint::Length(curve, start, end, integration);
  std::vector<double> knots = {start, end};
  if (curve.GetType() == GeomAbs_BSplineCurve) {
    const auto spline = curve.BSpline();
    for (int i = 1; i <= spline->NbKnots(); ++i)
      if (spline->Knot(i) > start && spline->Knot(i) < end)
        knots.push_back(spline->Knot(i));
  }
  if (knots.size() > 258)
    throw std::runtime_error("Surface gap curve exceeds native knot budget");
  std::sort(knots.begin(), knots.end());
  double previous = -1, maximum = 0;
  for (const int divisions : {32, 64, 128}) {
    maximum = 0;
    const auto measure = [&](double parameter) {
      BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(curve.Value(parameter)),
                                          second);
      if (!distance.IsDone() || !std::isfinite(distance.Value()))
        throw std::runtime_error("Surface gap distance did not converge");
      maximum = std::max(maximum, distance.Value());
    };
    for (int i = 0; i <= divisions; ++i) {
      double parameter = i == divisions ? end : start;
      if (i > 0 && i < divisions) {
        GCPnts_AbscissaPoint station(integration, curve, length * double(i) / divisions, start);
        if (!station.IsDone())
          throw std::runtime_error("Surface gap station did not converge");
        parameter = station.Parameter();
      }
      measure(parameter);
    }
    for (std::size_t i = 1; i < knots.size(); ++i)
      for (int j = 0; j <= 16; ++j)
        measure(std::lerp(knots[i - 1], knots[i], j / 16.0));
    if (previous >= 0 && std::abs(maximum - previous) <= tolerance * .05)
      return maximum;
    previous = maximum;
  }
  throw std::runtime_error("Surface gap maximum did not stabilize within its refinement budget");
}
} // namespace
Result<SurfaceBoundaryReport>
SurfaceBoundaryQuery::inspect(const std::vector<GeometryShape>& shapes, double tolerance,
                              double search_radius) {
  const auto fail = [](std::string message) {
    return Result<SurfaceBoundaryReport>::failure(
        Error::geometry("surface.boundaries", std::move(message)));
  };
  if (shapes.empty() || !std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1 ||
      !std::isfinite(search_radius) || search_radius < tolerance || search_radius > 1e6)
    return fail("Surface gap query requires finite authored tolerance and search radius");
  try {
    SurfaceBoundaryReport report;
    report.tolerance_mm = tolerance;
    std::vector<TopoDS_Edge> edges;
    for (std::size_t source = 0; source < shapes.size(); ++source) {
      if (shapes[source].empty())
        return fail("Surface boundary query has an empty input");
      TopTools_IndexedDataMapOfShapeListOfShape ancestors;
      // This map retains edge USES: a periodic seam has two uses on the same face.
      TopExp::MapShapesAndAncestors(shapes[source].impl_->shape, TopAbs_EDGE, TopAbs_FACE,
                                    ancestors);
      for (int i = 1; i <= ancestors.Extent(); ++i) {
        const auto edge = TopoDS::Edge(ancestors.FindKey(i));
        if (BRep_Tool::Degenerated(edge))
          continue;
        const auto uses = ancestors.FindFromIndex(i).Extent();
        if (uses > 2)
          ++report.non_manifold_edges;
        if (uses != 1)
          continue;
        if (edges.size() >= 256)
          return fail("Surface boundary overlay exceeds 256 free edges");
        BRepAdaptor_Curve curve(edge);
        const double length = GCPnts_AbscissaPoint::Length(curve, curve.FirstParameter(),
                                                           curve.LastParameter(), tolerance * .05);
        if (!std::isfinite(length) || length <= tolerance)
          return fail("Surface has a collapsed free edge");
        GCPnts_AbscissaPoint half(tolerance * .05, curve, length / 2, curve.FirstParameter());
        if (!half.IsDone())
          return fail("Surface free-edge midpoint did not converge");
        const auto point = curve.Value(half.Parameter());
        edges.push_back(edge);
        report.free_edges.push_back({GeometryShape(std::make_shared<GeometryShape::Impl>(edge)),
                                     {point.X(), point.Y(), point.Z()},
                                     length,
                                     source});
      }
    }
    for (std::size_t i = 0; i < edges.size(); ++i)
      for (std::size_t j = i + 1; j < edges.size(); ++j) {
        if (report.free_edges[i].source == report.free_edges[j].source)
          continue;
        BRepExtrema_DistShapeShape minimum(edges[i], edges[j]);
        if (!minimum.IsDone())
          return fail("Surface boundary minimum distance did not converge");
        if (minimum.Value() > search_radius)
          continue;
        const double maximum = std::max(directed_gap(edges[i], edges[j], tolerance),
                                        directed_gap(edges[j], edges[i], tolerance));
        if (maximum > search_radius)
          continue;
        report.gaps.push_back({i, j, minimum.Value(), maximum, maximum <= tolerance});
      }
    return Result<SurfaceBoundaryReport>::success(std::move(report));
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry
