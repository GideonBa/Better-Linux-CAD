#pragma once

#include "blcad/core/result.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/sweep_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class LoftAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  loft_closed_sections(FeatureId feature_id,
                       const std::vector<std::vector<ClosedProfileCurveSegment>>& ordered_sections,
                       bool make_solid) const;

  [[nodiscard]] Result<GeometryShape>
  loft_open_sections(FeatureId feature_id,
                     const std::vector<std::vector<SweepPathSegment>>& ordered_sections) const;
};

} // namespace blcad::geometry
