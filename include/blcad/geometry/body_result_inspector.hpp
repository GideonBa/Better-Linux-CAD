#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace blcad::geometry {

struct BodyResultInspection {
  BodyId body_id;
  std::string name;
  BodyKind kind = BodyKind::Solid;
  BodyVisibility visibility = BodyVisibility::Visible;
  std::optional<FeatureId> source_feature_id;
  std::optional<ShapeSummary> shape_summary;
};

struct PartBodyInspection {
  std::vector<BodyResultInspection> bodies;
  std::size_t solid_body_count = 0;
  std::size_t surface_body_count = 0;
};

class BodyResultInspector {
public:
  [[nodiscard]] Result<PartBodyInspection> inspect(const PartDocument& document,
                                                   const ShapeCache& shape_cache) const;

  // Historical final-shape access is compatible only for a zero-Body document or exactly one
  // explicit Solid Body. Multi-body and Surface-only documents fail instead of selecting a shape.
  [[nodiscard]] Result<GeometryShape>
  resolve_compatible_final_shape(const PartDocument& document, const ShapeCache& shape_cache) const;

private:
  RectangleExtrusionAdapter shape_inspector_;
};

} // namespace blcad::geometry
