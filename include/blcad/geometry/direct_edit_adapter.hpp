#pragma once

#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {
class ShapeCache;
class StepImportTopologyContext;
class DirectEditAdapter {
public:
  [[nodiscard]] Result<GeometryShape> apply(const PartDocument&, const DirectEditFeature&,
                                            const GeometryShape&, const ShapeCache&,
                                            StepImportTopologyContext* = nullptr) const;
};
} // namespace blcad::geometry
