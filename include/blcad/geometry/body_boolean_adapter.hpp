#pragma once

#include "blcad/core/body_boolean_feature.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class BodyBooleanAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  combine(FeatureId feature_id, BodyBooleanOperation operation, const GeometryShape& target,
          const std::vector<GeometryShape>& tools) const;
};

} // namespace blcad::geometry
