#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

class CircularPatternAdapter {
public:
  // Instance-major order: (instance_index, source_index). Instance zero is
  // the original placement. A full circle excludes the duplicate endpoint;
  // a partial angle includes both authored range boundaries.
  [[nodiscard]] Result<std::vector<GeometryShape>>
  generate_instances(FeatureId feature_id, const std::vector<GeometryShape>& sources,
                     Point3 axis_origin, Vector3 axis_direction, std::size_t count,
                     double total_angle_deg) const;

private:
  BodyTransformAdapter transform_adapter_;
};

} // namespace blcad::geometry
