#pragma once

#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {
class ShapeCache;
class StepImportTopologyContext;
class FilletAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  apply(const PartDocument& document, const FilletFeature& feature, const GeometryShape& target,
        const ShapeCache& shape_cache, double radius_mm,
        StepImportTopologyContext* import_context = nullptr) const;
};
} // namespace blcad::geometry
