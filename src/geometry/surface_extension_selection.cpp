#include "surface_extension_selection.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shell.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace blcad::geometry::detail {
namespace {
void on_shape(const TopoDS_Shape& curve_shape, const TopoDS_Shape& target, double tolerance) {
  for (TopExp_Explorer it(curve_shape, TopAbs_EDGE); it.More(); it.Next()) {
    BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));
    const double length = GCPnts_AbscissaPoint::Length(curve, curve.FirstParameter(),
                                                       curve.LastParameter(), tolerance * .05);
    for (int i = 0; i <= 64; ++i) {
      double parameter = i == 64 ? curve.LastParameter() : curve.FirstParameter();
      if (i > 0 && i < 64) {
        GCPnts_AbscissaPoint station(tolerance * .05, curve, length * i / 64.0,
                                     curve.FirstParameter());
        if (!station.IsDone())
          throw std::runtime_error("Extend boundary station did not converge");
        parameter = station.Parameter();
      }
      BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(curve.Value(parameter)),
                                          target);
      if (!distance.IsDone() || distance.Value() > tolerance)
        throw std::runtime_error("Extend boundary does not match the complete native free edge");
    }
  }
}
} // namespace
Result<AnalyticSurfaceBoundary> resolve_extension_boundary(FeatureId id, const TopoDS_Shape& target,
                                                           const TopoDS_Shape& selection,
                                                           double tolerance) {
  const auto fail = [&](std::string message) {
    return Result<AnalyticSurfaceBoundary>::failure(
        Error::geometry(id.value(), std::move(message)));
  };
  try {
    if (target.IsNull() || selection.IsNull() || !BRepCheck_Analyzer(target, true).IsValid() ||
        TopExp_Explorer(target, TopAbs_SOLID).More())
      return fail("Extend requires valid material-free faces or a quilt");
    TopTools_IndexedMapOfShape target_faces;
    TopExp::MapShapes(target, TopAbs_FACE, target_faces);
    if (target_faces.Extent() < 1 || target_faces.Extent() > 64)
      return fail("Extend supports a quilt of one to sixty-four faces");
    TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
    TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    std::vector<std::pair<TopoDS_Edge, TopoDS_Face>> matches;
    for (int i = 1; i <= edge_faces.Extent(); ++i) {
      const auto& uses = edge_faces.FindFromIndex(i);
      if (uses.Extent() != 1)
        continue;
      const auto edge = TopoDS::Edge(edge_faces.FindKey(i));
      try {
        on_shape(edge, selection, tolerance);
        on_shape(selection, edge, tolerance);
      } catch (const std::exception&) {
        continue;
      }
      matches.push_back({edge, TopoDS::Face(uses.First())});
    }
    if (matches.size() != 1)
      return fail("Extend requires exactly one complete, unambiguous free boundary");
    const auto& [edge, face] = matches.front();
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane && surface.GetType() != GeomAbs_Cylinder)
      return fail("Natural/tangent extension currently supports analytic planes and cylinders");
    TopTools_IndexedMapOfShape wires;
    TopExp::MapShapes(face, TopAbs_WIRE, wires);
    if (wires.Extent() != 1)
      return fail("Extend a rectangular analytic patch; inner trimmed loops require an explicit "
                  "patch construction");
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    const auto native = BRep_Tool::Surface(face);
    BRepAdaptor_Curve curve(edge);
    std::vector<gp_Pnt2d> uv;
    for (int i = 0; i <= 32; ++i) {
      const auto point =
          curve.Value(std::lerp(curve.FirstParameter(), curve.LastParameter(), i / 32.0));
      GeomAPI_ProjectPointOnSurf projection(point, native);
      if (!projection.IsDone() || projection.NbPoints() == 0 ||
          projection.LowerDistance() > tolerance)
        return fail("Extend boundary has no native surface parameterization");
      double u, v;
      projection.LowerDistanceParameters(u, v);
      // Periodic seam coordinates are equivalent; physical samples decide constant-side identity.
      if (surface.IsUPeriodic()) {
        const double period = surface.UPeriod();
        while (u < u0 - 1e-10)
          u += period;
        while (u > u1 + 1e-10)
          u -= period;
      }
      uv.emplace_back(u, v);
    }
    const auto same_side = [&](int side) {
      for (const auto& point : uv) {
        const double u = point.X(), v = point.Y();
        const auto on = side == 0   ? native->Value(u0, v)
                        : side == 1 ? native->Value(u1, v)
                        : side == 2 ? native->Value(u, v0)
                                    : native->Value(u, v1);
        if (on.Distance(native->Value(u, v)) > tolerance)
          return false;
      }
      return true;
    };
    std::vector<int> sides;
    for (int i = 0; i < 4; ++i)
      if (same_side(i))
        sides.push_back(i);
    if (sides.size() != 1)
      return fail("Extend boundary is not one unique native isoparametric side");
    const int side = sides.front();
    GeomAdaptor_Curve complete_side(side < 2 ? native->UIso(side == 0 ? u0 : u1)
                                             : native->VIso(side == 2 ? v0 : v1));
    const double side_length = GCPnts_AbscissaPoint::Length(complete_side, side < 2 ? v0 : u0,
                                                            side < 2 ? v1 : u1, tolerance * .05);
    const double selected_length = GCPnts_AbscissaPoint::Length(
        curve, curve.FirstParameter(), curve.LastParameter(), tolerance * .05);
    if (std::abs(side_length - selected_length) > tolerance)
      return fail("Extend requires the complete native isoparametric side, not a split segment");
    // Verify all authored patch boundaries are native isoparametric sides before rebuilding a
    // domain.
    for (TopExp_Explorer it(face, TopAbs_EDGE); it.More(); it.Next()) {
      BRepAdaptor_Curve boundary_curve(TopoDS::Edge(it.Current()));
      for (int i = 0; i <= 16; ++i) {
        const auto point = boundary_curve.Value(
            std::lerp(boundary_curve.FirstParameter(), boundary_curve.LastParameter(), i / 16.0));
        GeomAPI_ProjectPointOnSurf projection(point, native);
        if (!projection.IsDone() || projection.NbPoints() == 0 ||
            projection.LowerDistance() > tolerance)
          return fail("Extend patch boundary has no native surface incidence");
        double u, v;
        projection.LowerDistanceParameters(u, v);
        const double minimum =
            std::min({point.Distance(native->Value(u0, v)), point.Distance(native->Value(u1, v)),
                      point.Distance(native->Value(u, v0)), point.Distance(native->Value(u, v1))});
        if (minimum > tolerance)
          return fail(
              "Extend cannot replace an arbitrary trimmed boundary with a rectangular domain");
      }
    }

    return Result<AnalyticSurfaceBoundary>::success({face, edge, u0, u1, v0, v1, side});
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry::detail
