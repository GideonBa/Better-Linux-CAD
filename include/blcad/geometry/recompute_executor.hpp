#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/body_boolean_adapter.hpp"
#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace blcad::geometry {

struct GeometryRecomputeSummary {
  std::size_t executed_feature_count = 0;
};

class GeometryRecomputeExecutor {
public:
  [[nodiscard]] Result<std::size_t> execute_additive_extrude(const PartDocument& document,
                                                             FeatureId feature_id,
                                                             ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_subtractive_extrude(const PartDocument& document,
                                                                FeatureId feature_id,
                                                                ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_body_boolean(const PartDocument& document,
                                                         FeatureId feature_id,
                                                         ShapeCache& shape_cache) const;

  [[nodiscard]] Result<GeometryRecomputeSummary> execute_plan(const PartDocument& document,
                                                              const RecomputePlan& plan,
                                                              ShapeCache& shape_cache) const;

  [[nodiscard]] Result<GeometryRecomputeSummary> execute_document(const PartDocument& document,
                                                                  ShapeCache& shape_cache) const;

private:
  [[nodiscard]] Result<std::size_t> execute_feature(const PartDocument& document,
                                                    const Feature& feature,
                                                    ShapeCache& shape_cache) const;

  RectangleExtrusionAdapter rectangle_extrusion_adapter_;
  CircularCutAdapter circular_cut_adapter_;
  ClosedProfileAdapter closed_profile_adapter_;
  BodyBooleanAdapter body_boolean_adapter_;
  WorkplaneResolver workplane_resolver_;
};

} // namespace blcad::geometry
