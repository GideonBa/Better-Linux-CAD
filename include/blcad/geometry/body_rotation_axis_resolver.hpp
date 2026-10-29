#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <utility>

namespace blcad::geometry {

// Authoritative model-space resolution for referenced Body Rotate axes.
// Explicit axes require the consuming transform's coordinate frame and remain
// with GeometryRecomputeExecutor; callers cannot silently treat them as World.
class BodyRotationAxisResolver {
public:
  [[nodiscard]] Result<std::pair<Point3, Vector3>> resolve(const PartDocument& document,
                                                           const BodyTransformRotationAxis& axis,
                                                           const ShapeCache& cache) const;
};

} // namespace blcad::geometry
