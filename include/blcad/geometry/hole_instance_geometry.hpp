#pragma once

#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

namespace blcad::geometry {
struct HoleInstanceFrame {
  ResolvedWorkplane frame;
  double radius_mm;
  GeometryShape support;
  double opening_radius_mm;
};
class HoleInstanceGeometry {
public:
  [[nodiscard]] static Result<HoleInstanceFrame>
  resolve(const PartDocument&, const HoleInstanceReference&, const ShapeCache&);
  // Used by the executor on already-validated candidate/predecessor geometry.
  [[nodiscard]] static Result<HoleInstanceFrame>
  resolve_shape(const PartDocument&, const HoleInstanceReference&, const GeometryShape&);
};
} // namespace blcad::geometry
