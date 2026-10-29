#pragma once

#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <optional>

namespace blcad::geometry {

class ShapeCache;
class StepImportTopologyContext;

class ChamferAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  apply(const PartDocument& document, const ChamferFeature& feature, const GeometryShape& target,
        const ShapeCache& shape_cache, double first_value, std::optional<double> second_value,
        StepImportTopologyContext* import_context = nullptr) const;
};

} // namespace blcad::geometry
