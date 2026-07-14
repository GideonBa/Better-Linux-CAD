#pragma once

#include "blcad/core/result.hpp"
#include "blcad/geometry/sweep_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class SurfaceAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  make_boundary_surface(FeatureId feature_id,
                        const std::vector<std::vector<SweepPathSegment>>& boundaries) const;

  [[nodiscard]] Result<GeometryShape>
  make_fill_surface(FeatureId feature_id,
                    const std::vector<std::vector<SweepPathSegment>>& boundaries) const;
};

} // namespace blcad::geometry
