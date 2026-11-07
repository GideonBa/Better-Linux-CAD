#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/physical_properties.hpp"
#include "occt_failure_message.hpp"
#include "physical_property_math.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace blcad::geometry {
namespace {
Error error(std::string message) {
  return Error::geometry("physical.shape", std::move(message));
}
gp_Pnt anchor(const TopoDS_Shape& shape) {
  Bnd_Box bounds;
  BRepBndLib::Add(shape, bounds, false);
  if (bounds.IsVoid() || bounds.IsWhole())
    throw std::runtime_error("Physical scope requires finite bounded geometry");
  double x1, y1, z1, x2, y2, z2;
  bounds.Get(x1, y1, z1, x2, y2, z2);
  for (const auto value : {x1, y1, z1, x2, y2, z2})
    if (!std::isfinite(value))
      throw std::runtime_error("Physical scope bounds are non-finite");
  return {std::midpoint(x1, x2), std::midpoint(y1, y2), std::midpoint(z1, z2)};
}
} // namespace

Result<ShapePhysicalProperties> inspect_shape_physical_properties(const GeometryShape& shape) {
  try {
    const auto* source = OcctShapeView::get(shape);
    if (!source || source->IsNull() || !BRepCheck_Analyzer(*source).IsValid())
      throw std::runtime_error("Physical properties require a nonempty valid shape");
    GProp_GProps area(anchor(*source));
    ShapePhysicalProperties result;
    result.estimated_relative_area_error =
        BRepGProp::SurfaceProperties(*source, area, physical_relative_integration_tolerance, false);
    result.area_mm2 = area.Mass();
    if (!std::isfinite(result.area_mm2) || result.area_mm2 < 0 ||
        !std::isfinite(result.estimated_relative_area_error) ||
        result.estimated_relative_area_error < 0)
      throw std::runtime_error("Surface integration produced an invalid area or error estimate");
    TopTools_IndexedMapOfShape solid_faces;
    std::vector<detail::WeightedPhysicalProperties> volumes;
    for (TopExp_Explorer solids(*source, TopAbs_SOLID); solids.More(); solids.Next()) {
      const auto& solid = solids.Current();
      const auto origin = anchor(solid);
      gp_Trsf to_local;
      to_local.SetTranslation(gp_Vec(-origin.X(), -origin.Y(), -origin.Z()));
      BRepBuilderAPI_Transform localized(solid, to_local, true);
      if (!localized.IsDone())
        throw std::runtime_error("Could not prepare a local physical integration frame");
      GProp_GProps properties;
      const auto estimate = BRepGProp::VolumeProperties(
          localized.Shape(), properties, physical_relative_integration_tolerance, true, false);
      const auto signed_volume = properties.Mass();
      if (!std::isfinite(estimate) || estimate < 0 || !std::isfinite(signed_volume) ||
          signed_volume == 0)
        throw std::runtime_error("Solid integration requires a closed finite nonzero volume");
      const auto center = properties.CentreOfMass();
      const auto inertia = properties.MatrixOfInertia();
      detail::WeightedPhysicalProperties measured{
          std::abs(signed_volume),
          {center.X() + origin.X(), center.Y() + origin.Y(), center.Z() + origin.Z()},
          {}};
      const double orientation = signed_volume < 0 ? -1 : 1;
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          measured.central_inertia[3 * i + j] = orientation * inertia.Value(i + 1, j + 1);
      volumes.push_back(std::move(measured));
      result.estimated_relative_volume_error =
          std::max(result.estimated_relative_volume_error, estimate);
      TopExp::MapShapes(solid, TopAbs_FACE, solid_faces);
    }
    result.solid_count = volumes.size();
    TopTools_IndexedMapOfShape all_faces;
    TopExp::MapShapes(*source, TopAbs_FACE, all_faces);
    for (int i = 1; i <= all_faces.Extent(); ++i)
      if (!solid_faces.Contains(all_faces(i)))
        ++result.free_face_count;
    if (!volumes.empty()) {
      const auto total = detail::sum_physical_properties(volumes);
      if (!total)
        return Result<ShapePhysicalProperties>::failure(total.error());
      result.volume = VolumePhysicalProperties{total.value().weight, total.value().centroid,
                                               total.value().central_inertia};
    }
    return Result<ShapePhysicalProperties>::success(std::move(result));
  } catch (const Standard_Failure& e) {
    return Result<ShapePhysicalProperties>::failure(error(standard_failure_message(e)));
  } catch (const std::exception& e) {
    return Result<ShapePhysicalProperties>::failure(error(e.what()));
  }
}
} // namespace blcad::geometry
