#include "blcad/geometry/occt_shape_view.hpp"

#include "geometry_shape_internal.hpp"

namespace blcad::geometry {

const TopoDS_Shape* OcctShapeView::get(const GeometryShape& shape) noexcept {
  if (shape.impl_ == nullptr || shape.impl_->shape.IsNull())
    return nullptr;
  return &shape.impl_->shape;
}

} // namespace blcad::geometry
