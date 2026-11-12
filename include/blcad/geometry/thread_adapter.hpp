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
struct ThreadSupportInspection {
  Point3 origin;
  Vector3 axis;
  Vector3 x_axis;
  double radius_mm = 0;
  double axial_length_mm = 0;
  double taper_angle_deg = 0;
};

// Checked single-start, ideal symmetric V-profile execution. Standard labels
// remain metadata; they do not supply unprovided root radii or allowances.
class ThreadAdapter {
public:
  // Derived, current-cache authoring information for an exterior cylinder/cone.
  // A prefix cache requires its explicit matching history scope; omission requires
  // a complete current result. Neither form accepts downstream or stale inputs.
  // Execution still validates the complete authored profile/material envelope.
  [[nodiscard]] static Result<ThreadSupportInspection>
  inspect_support(const PartDocument&, const BodyId&, const FaceReference&, const ShapeCache&,
                  ThreadDirection direction = ThreadDirection::AlongSupportAxis,
                  StepImportTopologyContext* imports = nullptr,
                  std::optional<std::size_t> history_prefix = std::nullopt);
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
