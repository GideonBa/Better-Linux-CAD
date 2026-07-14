#pragma once

#include "blcad/core/draft_feature.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {

class ShapeCache;

class DraftAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  apply(const PartDocument& document, const DraftFeature& feature, const GeometryShape& target,
        const ShapeCache& shape_cache, Vector3 pull_direction, Point3 neutral_origin,
        Vector3 neutral_normal, double angle_deg) const;
};

} // namespace blcad::geometry
