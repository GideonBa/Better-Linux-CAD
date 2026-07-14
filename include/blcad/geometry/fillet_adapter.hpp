#pragma once

#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {
class ShapeCache;
class FilletAdapter {
public:
  [[nodiscard]] Result<GeometryShape> apply(const PartDocument& document,
                                            const FilletFeature& feature,
                                            const GeometryShape& target,
                                            const ShapeCache& shape_cache, double radius_mm) const;
};
} // namespace blcad::geometry
