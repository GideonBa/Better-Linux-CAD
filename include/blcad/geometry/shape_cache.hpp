#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <cstddef>
#include <vector>

namespace blcad::geometry {

struct CachedFeatureShape {
  FeatureId feature_id;
  GeometryShape shape;
};

class ShapeCache {
public:
  [[nodiscard]] static Result<ShapeCache> create(ShapeCacheId id);

  [[nodiscard]] Result<std::size_t> store_feature_shape(FeatureId feature_id, GeometryShape shape);
  [[nodiscard]] Result<std::size_t> set_final_shape(FeatureId source_feature_id,
                                                    GeometryShape shape);
  [[nodiscard]] Result<bool> remove_feature_shape(FeatureId feature_id);

  void clear() noexcept;

  [[nodiscard]] const ShapeCacheId& id() const noexcept;
  [[nodiscard]] const std::vector<CachedFeatureShape>& feature_shapes() const noexcept;
  [[nodiscard]] std::size_t feature_shape_count() const noexcept;
  [[nodiscard]] const GeometryShape* find_feature_shape(const FeatureId& feature_id) const noexcept;
  [[nodiscard]] bool has_final_shape() const noexcept;
  [[nodiscard]] const FeatureId& final_feature_id() const noexcept;
  [[nodiscard]] const GeometryShape* final_shape() const noexcept;

private:
  explicit ShapeCache(ShapeCacheId id);

  [[nodiscard]] Result<std::size_t> validate_cache_input(const FeatureId& feature_id,
                                                         const GeometryShape& shape) const;

  ShapeCacheId id_;
  std::vector<CachedFeatureShape> feature_shapes_;
  bool has_final_shape_ = false;
  FeatureId final_feature_id_;
  GeometryShape final_shape_;
};

} // namespace blcad::geometry
