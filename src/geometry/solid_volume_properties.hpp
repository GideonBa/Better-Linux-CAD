#pragma once

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>

namespace blcad::geometry::detail {
// Open faces may coexist with solid bodies in an imported product. They retain
// their distance/contact geometry but cannot contribute to a solid overlap volume.
[[nodiscard]] inline double solid_volume_mm3(const TopoDS_Shape& shape) {
  TopTools_IndexedMapOfShape solids;
  TopExp::MapShapes(shape, TopAbs_SOLID, solids);
  double volume = 0;
  for (int index = 1; index <= solids.Extent(); ++index) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(solids(index), properties);
    volume += properties.Mass();
  }
  return volume;
}
} // namespace blcad::geometry::detail
