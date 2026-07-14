#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class MirrorAdapter {
public:
  // Preserves authored source order and returns reflected copies only.
  [[nodiscard]] Result<std::vector<GeometryShape>>
  reflect_sources(FeatureId feature_id, const std::vector<GeometryShape>& sources,
                  Point3 plane_origin, Vector3 plane_normal) const;

private:
  BodyTransformAdapter transform_adapter_;
};

} // namespace blcad::geometry
