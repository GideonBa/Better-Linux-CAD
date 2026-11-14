#include "geometry_volume_oracle.hpp"

#include "blcad/geometry/occt_shape_view.hpp"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <stdexcept>

namespace blcad::test {

double distance_to_faces(const geometry::GeometryShape& shape, Point3 point) {
  const auto* native = geometry::OcctShapeView::get(shape);
  if (!native)
    throw std::invalid_argument("face oracle requires native geometry");
  BRep_Builder builder;
  TopoDS_Compound faces;
  builder.MakeCompound(faces);
  std::size_t count = 0;
  for (TopExp_Explorer it(*native, TopAbs_FACE); it.More(); it.Next()) {
    builder.Add(faces, it.Current());
    ++count;
  }
  if (!count)
    throw std::invalid_argument("face oracle requires actual faces");
  BRepExtrema_DistShapeShape query(
      BRepBuilderAPI_MakeVertex(gp_Pnt(point.x, point.y, point.z)).Vertex(), faces);
  if (!query.IsDone() || query.NbSolution() == 0)
    throw std::runtime_error("face distance failed");
  return query.Value();
}

IntegratedSolidVolume integrated_solid_volume(const geometry::GeometryShape& shape) {
  const auto* native = geometry::OcctShapeView::get(shape);
  if (!native)
    throw std::invalid_argument("volume oracle requires native solid geometry");
  GProp_GProps properties;
  const double error = BRepGProp::VolumeProperties(*native, properties, 1e-10, true);
  return {std::abs(properties.Mass()), error};
}

SolidPointLocation classify_solid_point(const geometry::GeometryShape& shape, Point3 point,
                                        double tolerance_mm) {
  const auto* native = geometry::OcctShapeView::get(shape);
  if (!native || !std::isfinite(tolerance_mm) || tolerance_mm <= 0)
    throw std::invalid_argument(
        "point oracle requires native solid geometry and positive tolerance");
  switch (BRepClass3d_SolidClassifier(*native, gp_Pnt(point.x, point.y, point.z), tolerance_mm)
              .State()) {
  case TopAbs_IN:
    return SolidPointLocation::Inside;
  case TopAbs_OUT:
    return SolidPointLocation::Outside;
  case TopAbs_ON:
    return SolidPointLocation::Boundary;
  default:
    return SolidPointLocation::Unknown;
  }
}

} // namespace blcad::test
