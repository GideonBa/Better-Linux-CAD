#include "blcad/geometry/shape_cache.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kShapeCacheObjectId = "shape_cache";

} // namespace

Result<ShapeCache> ShapeCache::create(ShapeCacheId id) {
  if (id.empty()) {
    return Result<ShapeCache>::failure(
        Error::validation(kShapeCacheObjectId, "shape cache id must not be empty"));
  }

  return Result<ShapeCache>::success(ShapeCache(std::move(id)));
}

Result<std::size_t> ShapeCache::store_feature_shape(FeatureId feature_id, GeometryShape shape) {
  const auto validation = validate_cache_input(feature_id, shape);
  if (validation.has_error()) {
    return validation;
  }

  const auto existing = std::find_if(feature_shapes_.begin(), feature_shapes_.end(),
                                     [&feature_id](const CachedFeatureShape& cached_shape) {
                                       return cached_shape.feature_id == feature_id;
                                     });

  if (existing != feature_shapes_.end()) {
    existing->shape = std::move(shape);
    return Result<std::size_t>::success(
        static_cast<std::size_t>(std::distance(feature_shapes_.begin(), existing)));
  }

  feature_shapes_.push_back(CachedFeatureShape{std::move(feature_id), std::move(shape)});
  return Result<std::size_t>::success(feature_shapes_.size() - 1U);
}

Result<std::size_t> ShapeCache::set_final_shape(FeatureId source_feature_id, GeometryShape shape) {
  const auto validation = validate_cache_input(source_feature_id, shape);
  if (validation.has_error()) {
    return validation;
  }

  final_feature_id_ = source_feature_id;
  final_shape_ = shape;
  has_final_shape_ = true;

  return store_feature_shape(std::move(source_feature_id), std::move(shape));
}

Result<bool> ShapeCache::remove_feature_shape(FeatureId feature_id) {
  if (feature_id.empty()) {
    return Result<bool>::failure(
        Error::validation(kShapeCacheObjectId, "feature id must not be empty"));
  }

  const auto existing = std::find_if(feature_shapes_.begin(), feature_shapes_.end(),
                                     [&feature_id](const CachedFeatureShape& cached_shape) {
                                       return cached_shape.feature_id == feature_id;
                                     });

  if (existing == feature_shapes_.end()) {
    return Result<bool>::success(false);
  }

  feature_shapes_.erase(existing);

  if (has_final_shape_ && final_feature_id_ == feature_id) {
    has_final_shape_ = false;
    final_feature_id_ = FeatureId();
    final_shape_ = GeometryShape();
  }

  return Result<bool>::success(true);
}

Result<std::size_t> ShapeCache::store_body_shape(BodyId body_id, FeatureId source_feature_id,
                                                 GeometryShape shape) {
  if (body_id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "body id must not be empty"));
  }
  const auto validation = validate_cache_input(source_feature_id, shape);
  if (validation.has_error())
    return validation;

  const auto position = std::lower_bound(body_shapes_.begin(), body_shapes_.end(), body_id,
                                         [](const CachedBodyShape& cached, const BodyId& id) {
                                           return cached.body_id.value() < id.value();
                                         });
  if (position != body_shapes_.end() && position->body_id == body_id) {
    const auto index = static_cast<std::size_t>(std::distance(body_shapes_.begin(), position));
    position->source_feature_id = std::move(source_feature_id);
    position->shape = std::move(shape);
    return Result<std::size_t>::success(index);
  }

  const auto index = static_cast<std::size_t>(std::distance(body_shapes_.begin(), position));
  body_shapes_.insert(position, CachedBodyShape{std::move(body_id), std::move(source_feature_id),
                                                std::move(shape)});
  return Result<std::size_t>::success(index);
}

Result<bool> ShapeCache::remove_body_shape(BodyId body_id) {
  if (body_id.empty()) {
    return Result<bool>::failure(
        Error::validation(kShapeCacheObjectId, "body id must not be empty"));
  }
  const auto position = std::lower_bound(body_shapes_.begin(), body_shapes_.end(), body_id,
                                         [](const CachedBodyShape& cached, const BodyId& id) {
                                           return cached.body_id.value() < id.value();
                                         });
  if (position == body_shapes_.end() || position->body_id != body_id)
    return Result<bool>::success(false);
  body_shapes_.erase(position);
  return Result<bool>::success(true);
}

void ShapeCache::clear_final_shape() noexcept {
  has_final_shape_ = false;
  final_feature_id_ = FeatureId();
  final_shape_ = GeometryShape();
}

void ShapeCache::clear() noexcept {
  feature_shapes_.clear();
  body_shapes_.clear();
  clear_final_shape();
}

const ShapeCacheId& ShapeCache::id() const noexcept {
  return id_;
}

const std::vector<CachedFeatureShape>& ShapeCache::feature_shapes() const noexcept {
  return feature_shapes_;
}

std::size_t ShapeCache::feature_shape_count() const noexcept {
  return feature_shapes_.size();
}

const GeometryShape* ShapeCache::find_feature_shape(const FeatureId& feature_id) const noexcept {
  const auto existing = std::find_if(feature_shapes_.begin(), feature_shapes_.end(),
                                     [&feature_id](const CachedFeatureShape& cached_shape) {
                                       return cached_shape.feature_id == feature_id;
                                     });

  if (existing == feature_shapes_.end()) {
    return nullptr;
  }

  return &existing->shape;
}

const std::vector<CachedBodyShape>& ShapeCache::body_shapes() const noexcept {
  return body_shapes_;
}

std::size_t ShapeCache::body_shape_count() const noexcept {
  return body_shapes_.size();
}

const CachedBodyShape* ShapeCache::find_body_result(const BodyId& body_id) const noexcept {
  const auto position = std::lower_bound(body_shapes_.begin(), body_shapes_.end(), body_id,
                                         [](const CachedBodyShape& cached, const BodyId& id) {
                                           return cached.body_id.value() < id.value();
                                         });
  return position != body_shapes_.end() && position->body_id == body_id ? &*position : nullptr;
}

const GeometryShape* ShapeCache::find_body_shape(const BodyId& body_id) const noexcept {
  const CachedBodyShape* result = find_body_result(body_id);
  return result == nullptr ? nullptr : &result->shape;
}

bool ShapeCache::has_final_shape() const noexcept {
  return has_final_shape_;
}

const FeatureId& ShapeCache::final_feature_id() const noexcept {
  return final_feature_id_;
}

const GeometryShape* ShapeCache::final_shape() const noexcept {
  if (!has_final_shape_) {
    return nullptr;
  }

  return &final_shape_;
}

ShapeCache::ShapeCache(ShapeCacheId id) : id_(std::move(id)) {}

Result<std::size_t> ShapeCache::validate_cache_input(const FeatureId& feature_id,
                                                     const GeometryShape& shape) const {
  if (feature_id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "feature id must not be empty"));
  }

  if (shape.empty()) {
    return Result<std::size_t>::failure(
        Error::geometry(feature_id.value(), "shape cache cannot store an empty shape"));
  }

  return Result<std::size_t>::success(0U);
}

} // namespace blcad::geometry
