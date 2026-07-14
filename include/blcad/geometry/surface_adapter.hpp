#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/sweep_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class SurfaceAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  make_boundary_surface(FeatureId feature_id,
                        const std::vector<std::vector<SweepPathSegment>>& boundaries) const;

  [[nodiscard]] Result<GeometryShape>
  make_fill_surface(FeatureId feature_id,
                    const std::vector<std::vector<SweepPathSegment>>& boundaries) const;

  [[nodiscard]] Result<GeometryShape>
  extract_semantic_face(FeatureId feature_id, const PartDocument& document,
                        const ShapeCache& shape_cache,
                        const SemanticFaceReference& reference) const;

  [[nodiscard]] Result<GeometryShape>
  trim_surface(FeatureId feature_id, const GeometryShape& target,
               const std::vector<SweepPathSegment>& closed_boundary) const;

  [[nodiscard]] Result<GeometryShape> extend_surface(FeatureId feature_id,
                                                     const GeometryShape& target,
                                                     const std::vector<SweepPathSegment>& boundary,
                                                     double distance_mm) const;

  [[nodiscard]] Result<GeometryShape>
  stitch_surfaces(FeatureId feature_id, const std::vector<GeometryShape>& surfaces,
                  double tolerance_mm) const;

  [[nodiscard]] Result<GeometryShape> close_shell_to_solid(FeatureId feature_id,
                                                           const GeometryShape& shell) const;
};

} // namespace blcad::geometry
