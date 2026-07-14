#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/body_boolean_adapter.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/chamfer_adapter.hpp"
#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/circular_pattern_adapter.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/draft_adapter.hpp"
#include "blcad/geometry/fillet_adapter.hpp"
#include "blcad/geometry/linear_pattern_adapter.hpp"
#include "blcad/geometry/mirror_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/revolve_adapter.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/shell_adapter.hpp"
#include "blcad/geometry/sweep_adapter.hpp"
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

  [[nodiscard]] Result<std::size_t> execute_body_transform(const PartDocument& document,
                                                           BodyTransformId transform_id,
                                                           ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_revolve(const PartDocument& document,
                                                    FeatureId feature_id,
                                                    ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_linear_pattern(const PartDocument& document,
                                                           FeatureId feature_id,
                                                           ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_circular_pattern(const PartDocument& document,
                                                             FeatureId feature_id,
                                                             ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t>
  execute_mirror(const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t>
  execute_fillet(const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_chamfer(const PartDocument& document,
                                                    FeatureId feature_id,
                                                    ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t>
  execute_shell(const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t>
  execute_draft(const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t>
  execute_sweep(const PartDocument& document, FeatureId feature_id, ShapeCache& shape_cache) const;

  [[nodiscard]] Result<GeometryRecomputeSummary> execute_plan(const PartDocument& document,
                                                              const RecomputePlan& plan,
                                                              ShapeCache& shape_cache) const;

  [[nodiscard]] Result<GeometryRecomputeSummary> execute_document(const PartDocument& document,
                                                                  ShapeCache& shape_cache) const;

private:
  [[nodiscard]] Result<std::size_t> execute_path_extrude(const PartDocument& document,
                                                         const Feature& feature,
                                                         ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_richer_extrude(const PartDocument& document,
                                                           const Feature& feature,
                                                           ShapeCache& shape_cache) const;

  [[nodiscard]] Result<std::size_t> execute_feature(const PartDocument& document,
                                                    const Feature& feature,
                                                    ShapeCache& shape_cache) const;

  RectangleExtrusionAdapter rectangle_extrusion_adapter_;
  CircularCutAdapter circular_cut_adapter_;
  ClosedProfileAdapter closed_profile_adapter_;
  FilletAdapter fillet_adapter_;
  ChamferAdapter chamfer_adapter_;
  ShellAdapter shell_adapter_;
  DraftAdapter draft_adapter_;
  BodyBooleanAdapter body_boolean_adapter_;
  BodyTransformAdapter body_transform_adapter_;
  RevolveAdapter revolve_adapter_;
  SweepAdapter sweep_adapter_;
  CircularPatternAdapter circular_pattern_adapter_;
  LinearPatternAdapter linear_pattern_adapter_;
  MirrorAdapter mirror_adapter_;
  WorkplaneResolver workplane_resolver_;
};

} // namespace blcad::geometry
