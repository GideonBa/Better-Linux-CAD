#pragma once

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

class TopoDS_Shape;

namespace blcad::geometry {

// Read-only, transient bridge for OCCT consumers such as the GUI viewport.
// The returned shape remains owned by GeometryShape and must never be persisted.
class OcctShapeView {
public:
  [[nodiscard]] static const TopoDS_Shape* get(const GeometryShape& shape) noexcept;
};

} // namespace blcad::geometry
