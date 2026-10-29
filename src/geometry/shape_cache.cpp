#include "blcad/geometry/shape_cache.hpp"

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <iterator>
#include <set>
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

  std::erase_if(imported_body_results_,
                [&](const auto& imported) { return imported.feature_id == feature_id; });
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

Result<std::size_t> ShapeCache::store_reference_result(CachedReferencePartResult result) {
  const auto invalid = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation(kShapeCacheObjectId, std::move(message)));
  };
  if (result.part_document.empty() || result.shape.empty() || result.bodies.empty() ||
      result.bodies.size() != result.definition.outputs().size())
    return invalid("reference cache requires a complete owned geometry result");
  if (result.source.mode() != StepImportMode::Reference ||
      result.source.id() != result.definition.source() ||
      result.catalog.id() != result.definition.catalog() ||
      result.catalog.source() != result.source.id())
    return invalid("reference cache source, catalog and definition must agree");

  std::set<std::string> bodies;
  std::set<std::string> entries;
  for (std::size_t index = 0; index < result.bodies.size(); ++index) {
    const auto& body = result.bodies[index];
    const auto* entry = result.catalog.find(body.output.imported_body);
    if (body.output != result.definition.outputs()[index] || body.shape.empty() ||
        body.output.body.empty() || !bodies.insert(body.output.body.value()).second ||
        !entries.insert(body.output.imported_body.value()).second || entry == nullptr ||
        entry->descriptor.kind != ImportedTopologyKind::Body ||
        entry->descriptor.body_kind != body.output.kind)
      return invalid("reference cache bodies must exactly match the selected catalog outputs");
  }
  const std::size_t count = result.bodies.size();
  clear();
  reference_result_ = std::move(result);
  return Result<std::size_t>::success(count);
}

const CachedReferencePartResult* ShapeCache::reference_result() const noexcept {
  return reference_result_ ? &*reference_result_ : nullptr;
}

Result<const CachedReferencePartResult*>
ShapeCache::validated_reference_result(const PartDocument& document) const {
  const auto invalid = [&document](std::string message) {
    return Result<const CachedReferencePartResult*>::failure(
        Error::geometry(document.id().value(), std::move(message)));
  };
  if (!document.is_reference_part() || !reference_result_)
    return invalid("Reference Part requires its own computed reference result");
  const auto& result = *reference_result_;
  const auto* definition = document.reference_part_definition();
  const auto* source = document.find_step_import_source(result.source.id());
  const auto* catalog = document.find_imported_topology_catalog(result.catalog.id());
  if (result.part_document != document.id() || definition == nullptr ||
      result.definition != *definition || source == nullptr || result.source != *source ||
      catalog == nullptr || result.catalog != *catalog)
    return invalid("reference cache owner or source/catalog/body intent is stale");
  if (document.bodies().size() != result.bodies.size())
    return invalid("reference cache must cover the complete declared body set");
  for (const auto& body : result.bodies) {
    const auto* declared = document.find_body(body.output.body);
    if (declared == nullptr || declared->kind() != body.output.kind)
      return invalid("reference cache body must belong to the inspected Reference Part");
  }
  return Result<const CachedReferencePartResult*>::success(&result);
}

Result<std::size_t> ShapeCache::store_imported_body_result(FeatureId feature_id,
                                                           CachedReferencePartResult result) {
  const auto invalid = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation(kShapeCacheObjectId, std::move(message)));
  };
  if (feature_id.empty() || reference_result_ || result.part_document.empty() ||
      result.shape.empty() || result.bodies.empty() ||
      result.bodies.size() != result.definition.outputs().size() ||
      result.source.mode() != StepImportMode::EditableBody ||
      result.source.id() != result.definition.source() ||
      result.catalog.id() != result.definition.catalog() ||
      result.catalog.source() != result.source.id())
    return invalid("imported base cache requires complete editable feature/source/output intent");
  std::set<std::string> bodies;
  std::set<std::string> entries;
  for (std::size_t index = 0; index < result.bodies.size(); ++index) {
    const auto& body = result.bodies[index];
    const auto* entry = result.catalog.find(body.output.imported_body);
    if (body.output != result.definition.outputs()[index] || body.shape.empty() ||
        !bodies.insert(body.output.body.value()).second ||
        !entries.insert(body.output.imported_body.value()).second || entry == nullptr ||
        entry->descriptor.kind != ImportedTopologyKind::Body ||
        entry->descriptor.body_kind != body.output.kind)
      return invalid("imported base cache must retain each ordered catalog Body separately");
  }
  const auto found =
      std::find_if(imported_body_results_.begin(), imported_body_results_.end(),
                   [&](const auto& value) { return value.feature_id == feature_id; });
  if (found != imported_body_results_.end()) {
    found->imported = std::move(result);
    return Result<std::size_t>::success(
        static_cast<std::size_t>(std::distance(imported_body_results_.begin(), found)));
  }
  imported_body_results_.push_back({std::move(feature_id), std::move(result)});
  return Result<std::size_t>::success(imported_body_results_.size() - 1);
}

const CachedImportedBodyFeatureResult*
ShapeCache::find_imported_body_result(const FeatureId& feature_id) const noexcept {
  const auto found =
      std::find_if(imported_body_results_.begin(), imported_body_results_.end(),
                   [&](const auto& value) { return value.feature_id == feature_id; });
  return found == imported_body_results_.end() ? nullptr : &*found;
}

const GeometryShape* ShapeCache::find_imported_body_shape(const FeatureId& feature_id,
                                                          const BodyId& body_id) const noexcept {
  const auto* result = find_imported_body_result(feature_id);
  if (!result)
    return nullptr;
  const auto found = std::find_if(result->imported.bodies.begin(), result->imported.bodies.end(),
                                  [&](const auto& body) { return body.output.body == body_id; });
  return found == result->imported.bodies.end() ? nullptr : &found->shape;
}

Result<const CachedImportedBodyFeatureResult*>
ShapeCache::validated_imported_body_result(const PartDocument& document,
                                           const ImportedBodyFeature& feature) const {
  const auto invalid = [&document](std::string message) {
    return Result<const CachedImportedBodyFeatureResult*>::failure(
        Error::geometry(document.id().value(), std::move(message)));
  };
  const auto* cached = find_imported_body_result(feature.id());
  const auto& features = document.imported_body_features();
  const auto owned = std::find(features.begin(), features.end(), feature);
  if (document.is_reference_part() || cached == nullptr || owned == features.end())
    return invalid("incremental imported base requires its owned computed feature result");
  const auto& imported = cached->imported;
  const auto* source = document.find_step_import_source(feature.definition().source());
  const auto* catalog = document.find_imported_topology_catalog(feature.definition().catalog());
  if (imported.part_document != document.id() || imported.definition != feature.definition() ||
      source == nullptr || imported.source != *source || catalog == nullptr ||
      imported.catalog != *catalog || !find_feature_shape(feature.id()))
    return invalid("imported base cache owner or source/catalog/output intent is stale");
  for (const auto& body : imported.bodies) {
    const auto* declared = document.find_body(body.output.body);
    if (declared == nullptr || declared->kind() != body.output.kind)
      return invalid("imported base output must retain its declared Body kind");
  }
  return Result<const CachedImportedBodyFeatureResult*>::success(cached);
}

Result<std::size_t> ShapeCache::validate_imported_body_owners(const PartDocument& document) const {
  for (const auto& cached : imported_body_results_) {
    const auto* current = document.find_imported_body_feature(cached.feature_id);
    if (cached.imported.part_document != document.id() || current == nullptr)
      return Result<std::size_t>::failure(Error::geometry(
          document.id().value(), "cached imported base no longer belongs to the current Part"));
    const auto* catalog = document.find_imported_topology_catalog(current->definition().catalog());
    if (current->definition() != cached.imported.definition || catalog == nullptr ||
        *catalog != cached.imported.catalog)
      return Result<std::size_t>::failure(Error::geometry(
          cached.feature_id.value(),
          "cached imported base identity has different immutable catalog/output intent"));
  }
  return Result<std::size_t>::success(imported_body_results_.size());
}

Result<std::size_t> ShapeCache::reconcile_imported_body_owners(const PartDocument& document) {
  std::vector<FeatureId> removed;
  std::vector<BodyId> removed_bodies;
  for (const auto& cached : imported_body_results_) {
    if (cached.imported.part_document != document.id())
      return Result<std::size_t>::failure(
          Error::geometry(document.id().value(), "imported base cache belongs to another Part"));
    if (const auto* current = document.find_imported_body_feature(cached.feature_id)) {
      const auto* catalog =
          document.find_imported_topology_catalog(current->definition().catalog());
      if (current->definition() != cached.imported.definition || catalog == nullptr ||
          *catalog != cached.imported.catalog)
        return Result<std::size_t>::failure(Error::geometry(
            cached.feature_id.value(),
            "cached imported base identity has different immutable catalog/output intent"));
      continue;
    }
    if (document.dependency_graph().has_node(cached.feature_id.value()))
      return Result<std::size_t>::failure(Error::geometry(
          cached.feature_id.value(), "removed imported base identity has another authority"));
    for (const auto& output : cached.imported.definition.outputs()) {
      const auto* body = find_body_result(output.body);
      if (!body)
        continue;
      if (body->source_feature_id == cached.feature_id)
        removed_bodies.push_back(output.body);
      else if (!document.dependency_graph().has_dependency(body->source_feature_id.value(),
                                                           "body:" + output.body.value()))
        return Result<std::size_t>::failure(Error::geometry(
            output.body.value(),
            "removed imported base retains a deleted modifier result; rebuild from a clean cache"));
    }
    removed.push_back(cached.feature_id);
  }
  // Validate the whole removal set before changing even this disposable cache.
  for (const auto& body : removed_bodies) {
    auto removed_body = remove_body_shape(body);
    if (!removed_body)
      return Result<std::size_t>::failure(removed_body.error());
  }
  for (const auto& feature : removed) {
    auto removed_feature = remove_feature_shape(feature);
    if (!removed_feature)
      return Result<std::size_t>::failure(removed_feature.error());
  }
  return Result<std::size_t>::success(removed.size());
}

Result<std::size_t>
ShapeCache::store_body_transform_state(BodyTransformId transform_id, BodyId body_id,
                                       GeometryAffineTransform cumulative_transform) {
  if (reference_result_)
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "Reference Part cache rejects body transforms"));
  if (transform_id.empty() || body_id.empty())
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "body transform and body ids must not be empty"));
  const auto existing = std::find_if(
      body_transform_states_.begin(), body_transform_states_.end(),
      [&](const CachedBodyTransformState& state) { return state.transform_id == transform_id; });
  if (existing != body_transform_states_.end()) {
    existing->body_id = std::move(body_id);
    existing->cumulative_transform = cumulative_transform;
    return Result<std::size_t>::success(
        static_cast<std::size_t>(std::distance(body_transform_states_.begin(), existing)));
  }
  body_transform_states_.push_back(
      {std::move(transform_id), std::move(body_id), cumulative_transform});
  return Result<std::size_t>::success(body_transform_states_.size() - 1U);
}

Result<std::size_t>
ShapeCache::store_reference_transform(std::string reference_id, BodyId body_id,
                                      BodyTransformId source_transform_id,
                                      GeometryAffineTransform cumulative_transform) {
  if (reference_result_)
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "Reference Part cache rejects modeled transforms"));
  if (reference_id.empty() || body_id.empty() || source_transform_id.empty())
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "reference transform ids must not be empty"));
  const auto existing = std::find_if(reference_transforms_.begin(), reference_transforms_.end(),
                                     [&](const CachedReferenceTransform& state) {
                                       return state.reference_id == reference_id &&
                                              state.source_transform_id == source_transform_id;
                                     });
  if (existing != reference_transforms_.end()) {
    *existing = {std::move(reference_id), std::move(body_id), std::move(source_transform_id),
                 cumulative_transform};
    return Result<std::size_t>::success(
        static_cast<std::size_t>(std::distance(reference_transforms_.begin(), existing)));
  }
  reference_transforms_.push_back({std::move(reference_id), std::move(body_id),
                                   std::move(source_transform_id), cumulative_transform});
  return Result<std::size_t>::success(reference_transforms_.size() - 1U);
}

void ShapeCache::clear_final_shape() noexcept {
  has_final_shape_ = false;
  final_feature_id_ = FeatureId();
  final_shape_ = GeometryShape();
}

void ShapeCache::clear() noexcept {
  feature_shapes_.clear();
  body_shapes_.clear();
  body_transform_states_.clear();
  reference_transforms_.clear();
  reference_result_.reset();
  imported_body_results_.clear();
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
  if (reference_result_) {
    const auto found =
        std::find_if(reference_result_->bodies.begin(), reference_result_->bodies.end(),
                     [&body_id](const auto& body) { return body.output.body == body_id; });
    return found == reference_result_->bodies.end() ? nullptr : &found->shape;
  }
  const CachedBodyShape* result = find_body_result(body_id);
  return result == nullptr ? nullptr : &result->shape;
}

const CachedBodyTransformState*
ShapeCache::find_body_transform_state(const BodyTransformId& transform_id) const noexcept {
  const auto existing = std::find_if(
      body_transform_states_.begin(), body_transform_states_.end(),
      [&](const CachedBodyTransformState& state) { return state.transform_id == transform_id; });
  return existing == body_transform_states_.end() ? nullptr : &*existing;
}

const CachedBodyTransformState*
ShapeCache::find_latest_body_transform_state(const BodyId& body_id) const noexcept {
  const auto existing =
      std::find_if(body_transform_states_.rbegin(), body_transform_states_.rend(),
                   [&](const CachedBodyTransformState& state) { return state.body_id == body_id; });
  return existing == body_transform_states_.rend() ? nullptr : &*existing;
}

const CachedReferenceTransform*
ShapeCache::find_reference_transform(std::string_view reference_id) const noexcept {
  const auto existing = std::find_if(
      reference_transforms_.rbegin(), reference_transforms_.rend(),
      [&](const CachedReferenceTransform& state) { return state.reference_id == reference_id; });
  return existing == reference_transforms_.rend() ? nullptr : &*existing;
}

const CachedReferenceTransform*
ShapeCache::find_reference_transform(std::string_view reference_id,
                                     const BodyTransformId& source_transform_id) const noexcept {
  const auto existing = std::find_if(reference_transforms_.begin(), reference_transforms_.end(),
                                     [&](const CachedReferenceTransform& state) {
                                       return state.reference_id == reference_id &&
                                              state.source_transform_id == source_transform_id;
                                     });
  return existing == reference_transforms_.end() ? nullptr : &*existing;
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
  if (reference_result_)
    return Result<std::size_t>::failure(
        Error::validation(kShapeCacheObjectId, "Reference Part cache rejects feature results"));
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
