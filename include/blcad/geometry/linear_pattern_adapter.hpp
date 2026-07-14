#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

class LinearPatternAdapter {
public:
  // Instance-major order: (instance_index, source_index). Instance zero is
  // the original placement; generated instances follow at positive multiples
  // of spacing along the already signed direction.
  [[nodiscard]] Result<std::vector<GeometryShape>>
  generate_instances(FeatureId feature_id, const std::vector<GeometryShape>& sources,
                     Vector3 direction, std::size_t count, double spacing_mm) const;

private:
  BodyTransformAdapter transform_adapter_;
};

} // namespace blcad::geometry
