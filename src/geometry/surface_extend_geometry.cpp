#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/geometry/surface_trim_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include "surface_extension_selection.hpp"
#include "surface_join_verification.hpp"
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
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shell.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace blcad::geometry {
Result<GeometryShape> SurfaceTrimAdapter::extend(FeatureId id, const GeometryShape& target,
                                                 const std::vector<SweepPathSegment>& boundary,
                                                 double distance, SurfaceExtendMode mode,
                                                 double tolerance,
                                                 std::optional<double> maximum_deviation,
                                                 const std::string& boundary_identity) const {
  const auto fail = [&](std::string message) {
    return Result<GeometryShape>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (id.empty() || target.empty() || boundary.empty() || boundary_identity.empty() ||
      !std::isfinite(distance) || distance < 1e-6 || distance > 1e6 || !std::isfinite(tolerance) ||
      tolerance < 1e-9 || tolerance > 1 || tolerance > distance * .01 ||
      (mode != SurfaceExtendMode::Natural && mode != SurfaceExtendMode::Tangent) ||
      (mode == SurfaceExtendMode::Tangent) != maximum_deviation.has_value() ||
      (maximum_deviation && (!std::isfinite(*maximum_deviation) || *maximum_deviation < tolerance ||
                             *maximum_deviation > distance * .01)))
    return fail(
        "Extend requires explicit natural/tangent mode and bounded physical distance/error");
  try {
    BRepBuilderAPI_Copy copy(target.impl_->shape, true, false);
    auto working = std::make_shared<GeometryShape::Impl>(copy.Shape());
    for (auto entry : target.impl_->face_lineage) {
      entry.face = TopoDS::Face(copy.ModifiedShape(entry.face));
      detail::append_lineage(working->face_lineage, entry, working->shape);
    }
    const bool closed = blcad::distance(boundary.front().start, boundary.back().end) <= tolerance;
    auto selected =
        SweepAdapter{}.path_shape(PathCurveId("surface.extend.boundary"), boundary, closed);
    if (!selected)
      return Result<GeometryShape>::failure(selected.error());
    const auto& selection = selected.value().impl_->shape;
    const auto selected_boundary =
        detail::resolve_extension_boundary(id, working->shape, selection, tolerance);
    if (!selected_boundary)
      return Result<GeometryShape>::failure(selected_boundary.error());
    const auto& data = selected_boundary.value();
    const auto& face = data.face;
    const auto& edge = data.edge;
    const double u0 = data.u0, u1 = data.u1, v0 = data.v0, v1 = data.v1;
    const int side = data.side;
    const auto native = BRep_Tool::Surface(face);
    BRepAdaptor_Surface surface(face);
    BRepAdaptor_Curve curve(edge);
    TopoDS_Shape replacement;
    const bool tangent_strip =
        mode == SurfaceExtendMode::Tangent && surface.GetType() == GeomAbs_Cylinder && side < 2;
    if (tangent_strip) {
      // A straight axial generator has one constant outward tangent. Its prism is an exact C1
      // strip.
      const double u = side == 0 ? u0 : u1;
      gp_Pnt point;
      gp_Vec du, dv;
      native->D1(u, (v0 + v1) / 2, point, du, dv);
      du.Normalize();
      if (side == 0)
        du.Reverse();
      BRepPrimAPI_MakePrism strip(edge, du * distance, true);
      if (!strip.IsDone())
        return fail("Could not construct the tangent extension strip");
      replacement = strip.Shape();
      if (replacement.ShapeType() != TopAbs_FACE)
        return fail("Tangent strip must have one native face");
      auto patch = TopoDS::Face(replacement);
      auto joined = detail::verify_surface_join(id, edge, face, patch, LoftBoundaryContinuity::G1,
                                                tolerance, 1e-5, 1);
      if (!joined) {
        patch.Reverse();
        joined = detail::verify_surface_join(id, edge, face, patch, LoftBoundaryContinuity::G1,
                                             tolerance, 1e-5, 1);
      }
      if (!joined)
        return fail("Tangent extension does not satisfy the oriented native G1 join: " +
                    joined.error().message());
      replacement = patch;
      for (int i = 0; i <= 32; ++i) {
        const auto p =
            curve.Value(std::lerp(curve.FirstParameter(), curve.LastParameter(), i / 32.0));
        for (const double fraction : {.25, .5, 1.}) {
          const auto expected = p.Translated(du * (distance * fraction));
          BRepExtrema_DistShapeShape error(BRepBuilderAPI_MakeVertex(expected), replacement);
          if (!error.IsDone() || error.Value() > *maximum_deviation)
            return fail("Tangent extension exceeds its declared extrapolation error");
        }
      }
    } else {
      const double scale =
          surface.GetType() == GeomAbs_Cylinder && side < 2 ? surface.Cylinder().Radius() : 1;
      const double delta = distance / scale;
      double a = u0, b = u1, c = v0, d = v1;
      if (side == 0)
        a -= delta;
      else if (side == 1)
        b += delta;
      else if (side == 2)
        c -= delta;
      else
        d += delta;
      if (side < 2 && surface.IsUPeriodic() && b - a >= surface.UPeriod() - 1e-10)
        return fail("Natural extension would overlap a periodic seam");
      BRepBuilderAPI_MakeFace extended(native, a, b, c, d, tolerance);
      if (!extended.IsDone())
        return fail("Could not construct the natural analytic extension");
      auto output = extended.Face();
      output.Orientation(face.Orientation());
      replacement = output;
      for (int station = 0; station <= 16; ++station) {
        const double along =
            side < 2 ? std::lerp(v0, v1, station / 16.0) : std::lerp(u0, u1, station / 16.0);
        const auto actual = BRep_Tool::Surface(output);
        GeomAdaptor_Curve extension(side < 2 ? actual->VIso(along) : actual->UIso(along));
        const double start = side == 0 ? u0 : side == 1 ? u1 : side == 2 ? v0 : v1;
        const double end = side == 0 ? a : side == 1 ? b : side == 2 ? c : d;
        const double measured = GCPnts_AbscissaPoint::Length(extension, std::min(start, end),
                                                             std::max(start, end), tolerance * .05);
        if (!std::isfinite(measured) || std::abs(measured - distance) > tolerance)
          return fail("Measured native analytic extension length exceeds tolerance");
      }
    }
    BRepBuilderAPI_Sewing sewing(tolerance);
    for (TopExp_Explorer it(working->shape, TopAbs_FACE); it.More(); it.Next())
      if (tangent_strip || !it.Current().IsSame(face))
        sewing.Add(it.Current());
    sewing.Add(replacement);
    sewing.Perform();
    auto shape = sewing.SewedShape();
    TopTools_IndexedMapOfShape shells, faces;
    TopExp::MapShapes(shape, TopAbs_SHELL, shells);
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (sewing.NbMultipleEdges() || faces.Extent() == 0 ||
        (faces.Extent() > 1 && shells.Extent() != 1))
      return fail("Extended quilt is disconnected or non-manifold");
    if (faces.Extent() == 1)
      shape = faces(1);
    else
      shape = shells(1);
    if (!BRepCheck_Analyzer(shape, true).IsValid() ||
        !BRepAlgoAPI_Check(shape, false, true).IsValid())
      return fail("Extended quilt is invalid or self-intersecting");
    auto result = std::make_shared<GeometryShape::Impl>(shape);
    for (auto entry : working->face_lineage) {
      if (entry.face.IsSame(face) && !tangent_strip)
        entry.face = TopoDS::Face(replacement);
      const auto image = sewing.ModifiedSubShape(entry.face);
      if (!image.IsNull() && image.ShapeType() == TopAbs_FACE)
        entry.face = TopoDS::Face(image);
      detail::append_lineage(result->face_lineage, entry, shape);
    }
    auto added = TopoDS::Face(replacement);
    const auto image = sewing.ModifiedSubShape(added);
    if (!image.IsNull() && image.ShapeType() == TopAbs_FACE)
      added = TopoDS::Face(image);
    const auto intrinsic = SurfaceSelectionGeometry::tag_single_patch(
        GeometryShape(std::make_shared<GeometryShape::Impl>(added)), id,
        "surface/extend/" + std::string(tangent_strip ? "tangent/" : "natural/") +
            std::to_string(boundary_identity.size()) + ":" + boundary_identity);
    for (const auto& entry : intrinsic.impl_->face_lineage)
      detail::append_lineage(result->face_lineage, entry, shape);
    return Result<GeometryShape>::success(GeometryShape(std::move(result)));
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry
