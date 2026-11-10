#pragma once

#include "blcad/core/thread_feature.hpp"
#include "blcad/geometry/shape_cache.hpp"

namespace blcad {
class HoleFeature;
}
namespace blcad::geometry {
class StepImportTopologyContext;
struct CosmeticThreadPresentation {
  std::string identity;
  GeometryShape shape;
};

// Checked single-start, ideal symmetric V-profile execution. Standard labels
// remain metadata; they do not supply unprovided root radii or allowances.
class ThreadAdapter {
public:
  [[nodiscard]] Result<GeometryShape> apply(const PartDocument& part, const ThreadFeature& feature,
                                            const GeometryShape& target, const ShapeCache& cache,
                                            StepImportTopologyContext* imports = nullptr) const;
  [[nodiscard]] Result<GeometryShape>
  apply_hole(const PartDocument& part, const HoleFeature& feature, const GeometryShape& target,
             std::size_t placement, Point3 origin, Vector3 direction, Vector3 x_axis) const;
  [[nodiscard]] static Result<std::vector<CosmeticThreadPresentation>>
  cosmetic_presentations(const GeometryShape& body);
};
} // namespace blcad::geometry
