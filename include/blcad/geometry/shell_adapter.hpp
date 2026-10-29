#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/shell_feature.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::geometry {

class ShapeCache;
class StepImportTopologyContext;

class ShellAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  apply(const PartDocument& document, const ShellFeature& feature, const GeometryShape& target,
        const ShapeCache& shape_cache, double thickness_mm,
        StepImportTopologyContext* import_context = nullptr) const;
};

} // namespace blcad::geometry
