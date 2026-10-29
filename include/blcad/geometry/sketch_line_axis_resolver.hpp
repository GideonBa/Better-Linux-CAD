#pragma once

#include "blcad/core/sketch_line_reference.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <utility>

namespace blcad::geometry {

// Resolves authored Sketch line endpoints in the current cache-aware workplane.
// The returned frame already includes ownership transforms; do not apply them twice.
class SketchLineAxisResolver {
public:
  [[nodiscard]] Result<std::pair<Point3, Vector3>> resolve(const PartDocument& document,
                                                           const SketchLineReference& reference,
                                                           const ShapeCache& cache) const;
};

} // namespace blcad::geometry
