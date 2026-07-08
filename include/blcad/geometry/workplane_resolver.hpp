#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct ResolvedWorkplane {
  DatumPlaneId id;
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;
};

class WorkplaneResolver {
public:
  [[nodiscard]] Result<ResolvedWorkplane> resolve(const PartDocument& document,
                                                  DatumPlaneId workplane_id) const;

  [[nodiscard]] Point3 evaluate_point(const ResolvedWorkplane& workplane,
                                      Point2 local_point) const noexcept;
};

} // namespace blcad::geometry
