#include "geometry_volume_oracle.hpp"

#include "blcad/geometry/occt_shape_view.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <stdexcept>

namespace blcad::test {

IntegratedSolidVolume integrated_solid_volume(const geometry::GeometryShape& shape) {
  const auto* native = geometry::OcctShapeView::get(shape);
  if (!native)
    throw std::invalid_argument("volume oracle requires native solid geometry");
  GProp_GProps properties;
  const double error = BRepGProp::VolumeProperties(*native, properties, 1e-10, true);
  return {std::abs(properties.Mass()), error};
}

} // namespace blcad::test
