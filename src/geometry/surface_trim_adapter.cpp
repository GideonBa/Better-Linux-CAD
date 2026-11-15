#include "blcad/geometry/surface_trim_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <cmath>
#include <stdexcept>
namespace blcad::geometry {
namespace {
TopoDS_Shape connected_faces(const TopoDS_Shape& shape, double tolerance,
                             std::vector<detail::FaceLineage>& lineage) {
  TopTools_IndexedMapOfShape faces;
  TopExp::MapShapes(shape, TopAbs_FACE, faces);
  if (faces.Extent() == 0)
    throw std::runtime_error("Trim selected an empty region");
  if (faces.Extent() == 1)
    return faces(1);
  BRepBuilderAPI_Sewing sew(tolerance);
  for (int i = 1; i <= faces.Extent(); ++i)
    sew.Add(faces(i));
  sew.Perform();
  TopTools_IndexedMapOfShape shells, after;
  TopExp::MapShapes(sew.SewedShape(), TopAbs_SHELL, shells);
  TopExp::MapShapes(sew.SewedShape(), TopAbs_FACE, after);
  if (sew.NbMultipleEdges() || shells.Extent() != 1 || faces.Extent() != after.Extent())
    throw std::runtime_error(
        "Trim region is disconnected or non-manifold; author separate explicit regions");
  for (auto& entry : lineage) {
    const auto mapped = sew.ModifiedSubShape(entry.face);
    if (!mapped.IsNull() && mapped.ShapeType() == TopAbs_FACE)
      entry.face = TopoDS::Face(mapped);
  }
  return shells(1);
}
} // namespace
Result<GeometryShape> SurfaceTrimAdapter::trim(FeatureId id, const GeometryShape& target,
                                               const std::vector<SweepPathSegment>& contour,
                                               SurfaceTrimSide side,
                                               SurfaceTrimProjection projection,
                                               std::optional<Vector3> direction, double tolerance,
                                               const std::string& contour_identity) const {
  const auto fail = [&](std::string message) {
    return Result<GeometryShape>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (id.empty() || target.empty() || contour.empty() || contour.size() > 256 ||
      contour_identity.empty() || !std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1 ||
      (side != SurfaceTrimSide::KeepInside && side != SurfaceTrimSide::RemoveInside) ||
      (projection != SurfaceTrimProjection::OnSurface &&
       projection != SurfaceTrimProjection::AlongDirection) ||
      (projection == SurfaceTrimProjection::AlongDirection) != direction.has_value())
    return fail("Trim requires an explicit region, projection policy and bounded tolerance");
  try {
    BRepBuilderAPI_Copy copy(target.impl_->shape, true, false);
    auto working = std::make_shared<GeometryShape::Impl>(copy.Shape());
    for (auto entry : target.impl_->face_lineage) {
      entry.face = TopoDS::Face(copy.ModifiedShape(entry.face));
      detail::append_lineage(working->face_lineage, entry, working->shape);
    }
    TopTools_IndexedMapOfShape target_faces;
    TopExp::MapShapes(working->shape, TopAbs_FACE, target_faces);
    if (target_faces.Extent() < 1 || target_faces.Extent() > 64 ||
        TopExp_Explorer(working->shape, TopAbs_SOLID).More() ||
        !BRepCheck_Analyzer(working->shape, true).IsValid() ||
        !BRepAlgoAPI_Check(working->shape, false, true).IsValid())
      return fail("Trim requires a valid material-free quilt of one to sixty-four faces");
    auto wire_shape = SweepAdapter{}.path_shape(PathCurveId("surface.trim.contour"), contour, true);
    if (!wire_shape)
      return Result<GeometryShape>::failure(wire_shape.error());
    const auto& wire = wire_shape.value().impl_->shape;
    if (wire.ShapeType() != TopAbs_WIRE)
      return fail("Trim requires one closed native contour");
    BRepBuilderAPI_MakeFace face(TopoDS::Wire(wire), true);
    if (!face.IsDone())
      return fail("Projected Trim currently requires a finite planar closed contour");
    if (!BRepCheck_Analyzer(face.Face(), true).IsValid() ||
        !BRepAlgoAPI_Check(face.Face(), false, true).IsValid())
      return fail("Trim contour is self-intersecting or has no unambiguous planar inside");
    BRepAdaptor_Surface plane(face.Face());
    if (plane.GetType() != GeomAbs_Plane)
      return fail("Trim projection contour is not planar");
    gp_Dir axis = plane.Plane().Axis().Direction();
    if (direction) {
      const auto d = *direction;
      axis = gp_Dir(d.x, d.y, d.z);
      if (std::abs(axis.Dot(plane.Plane().Axis().Direction())) < 1e-8)
        return fail("Trim projection direction is tangent to the contour plane");
    }
    if (projection == SurfaceTrimProjection::OnSurface) {
      // On-surface planar replay is explicit. Curved quilts use an authored projection direction.
      for (TopExp_Explorer it(working->shape, TopAbs_FACE); it.More(); it.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
        if (surface.GetType() != GeomAbs_Plane ||
            std::abs(std::abs(surface.Plane().Axis().Direction().Dot(axis)) - 1) > 1e-9 ||
            surface.Plane().Distance(plane.Plane().Location()) > tolerance)
          return fail("Curved Trim requires an explicit projection direction; no side is inferred");
      }
    }
    Bnd_Box bounds;
    BRepBndLib::Add(working->shape, bounds);
    BRepBndLib::Add(face.Face(), bounds);
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    const double span = 2 * gp_Pnt(x0, y0, z0).Distance(gp_Pnt(x1, y1, z1)) + 10 * tolerance;
    const bool forward = projection == SurfaceTrimProjection::AlongDirection;
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(axis) * (forward ? -tolerance : -span));
    BRepBuilderAPI_Transform shifted(face.Face(), shift, true);
    BRepPrimAPI_MakePrism prism(shifted.Shape(),
                                gp_Vec(axis) * (forward ? span + tolerance : 2 * span), true);
    if (!prism.IsDone() || prism.Shape().ShapeType() != TopAbs_SOLID)
      return fail("Trim projection could not construct a closed region tool");
    auto tool = TopoDS::Solid(prism.Shape());
    if (!BRepLib::OrientClosedSolid(tool) || !BRepCheck_Analyzer(tool, true).IsValid() ||
        !BRepAlgoAPI_Check(tool, false, true).IsValid())
      return fail("Trim projection region is invalid or self-intersecting");
    TopoDS_Shape raw;
    std::vector<detail::FaceLineage> mapped;
    const auto run = [&](auto& operation) {
      TopTools_ListOfShape arguments, tools;
      arguments.Append(working->shape);
      tools.Append(tool);
      operation.SetArguments(arguments);
      operation.SetTools(tools);
      operation.SetNonDestructive(true);
      operation.SetFuzzyValue(tolerance);
      operation.Build();
      if (!operation.IsDone())
        throw std::runtime_error("Native curved Trim intersection failed");
      raw = operation.Shape();
      mapped = detail::map_lineage(operation, raw, working->face_lineage);
    };
    if (side == SurfaceTrimSide::KeepInside) {
      BRepAlgoAPI_Common operation;
      run(operation);
    } else {
      BRepAlgoAPI_Cut operation;
      run(operation);
    }
    auto shape = connected_faces(raw, tolerance, mapped);
    if (!BRepCheck_Analyzer(shape, true).IsValid() ||
        !BRepAlgoAPI_Check(shape, false, true).IsValid())
      return fail("Trim result is invalid or self-intersecting");
    GProp_GProps before, after;
    BRepGProp::SurfaceProperties(working->shape, before);
    BRepGProp::SurfaceProperties(shape, after);
    if (std::abs(before.Mass() - after.Mass()) <=
        std::max(tolerance * tolerance, std::abs(before.Mass()) * 1e-12))
      return fail("Trim contour makes no measurable change in the selected projection direction");
    auto result = std::make_shared<GeometryShape::Impl>(shape);
    const auto segment = [](const std::string& value) {
      return std::to_string(value.size()) + ":" + value + "/";
    };
    // Preserve all descendants. Side-specific authored roles become lost after an incompatible side
    // change.
    for (auto entry : mapped) {
      detail::append_lineage(result->face_lineage, entry, shape);
      entry.origin = {id,
                      "surface/trim/" +
                          std::string(side == SurfaceTrimSide::KeepInside ? "keep/" : "remove/") +
                          segment(contour_identity) + segment(entry.origin.feature.value()) +
                          segment(entry.origin.role)};
      detail::append_lineage(result->face_lineage, entry, shape);
    }
    return Result<GeometryShape>::success(GeometryShape(std::move(result)));
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry
