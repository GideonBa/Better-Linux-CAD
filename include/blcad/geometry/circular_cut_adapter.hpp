#pragma once

#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {

// Cuts a through-all circular hole out of an existing solid body. The cut is the
// MVP `SubtractiveExtrude` with `through_all`. The adapter encapsulates the OCCT
// boolean cut and therefore stays on the Geometry side of the Core/OCCT
// boundary.
class CircularCutAdapter {
public:
  [[nodiscard]] Result<GeometryShape> cut_circular_hole(const GeometryShape& target,
                                                        const Quantity& diameter,
                                                        Point2 center = {}) const;

  [[nodiscard]] Result<GeometryShape> cut_circular_hole_along_axis(const GeometryShape& target,
                                                                   const Quantity& diameter,
                                                                   Point3 center,
                                                                   Vector3 axis_direction) const;
};

} // namespace blcad::geometry
