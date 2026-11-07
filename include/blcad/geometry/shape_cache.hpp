#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/imported_part.hpp"
#include "blcad/core/imported_topology.hpp"
#include "blcad/core/part_feature_history.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/step_import_source.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {
class PartDocument;
}

namespace blcad::geometry {

struct CachedReferenceBodyShape {
  ImportedBodyOutput output;
  GeometryShape shape;
};

struct CachedReferencePartResult {
  DocumentId part_document;
  StepImportSource source;
  ImportedTopologyCatalog catalog;
  ImportedPartDefinition definition;
  std::vector<CachedReferenceBodyShape> bodies;
  GeometryShape shape;
};

// Immutable imported-base outputs retained independently of each Body's latest
// downstream result. A multi-body feature shape alone cannot identify one input.
struct CachedImportedBodyFeatureResult {
  FeatureId feature_id;
  CachedReferencePartResult imported;
};

struct CachedFeatureShape {
  FeatureId feature_id;
  GeometryShape shape;
};

struct CachedBodyShape {
  BodyId body_id;
  FeatureId source_feature_id;
  GeometryShape shape;
};

struct CachedBodyTransformState {
  BodyTransformId transform_id;
  BodyId body_id;
  GeometryAffineTransform cumulative_transform;
};

struct CachedReferenceTransform {
  std::string reference_id;
  BodyId body_id;
  BodyTransformId source_transform_id;
  GeometryAffineTransform cumulative_transform;
};

class ShapeCache {
public:
  [[nodiscard]] static Result<ShapeCache> create(ShapeCacheId id);

  [[nodiscard]] Result<std::size_t> store_feature_shape(FeatureId feature_id, GeometryShape shape);
  [[nodiscard]] Result<std::size_t> set_final_shape(FeatureId source_feature_id,
                                                    GeometryShape shape);
  [[nodiscard]] Result<bool> remove_feature_shape(FeatureId feature_id);
  [[nodiscard]] Result<std::size_t> store_body_shape(BodyId body_id, FeatureId source_feature_id,
                                                     GeometryShape shape);
  [[nodiscard]] Result<bool> remove_body_shape(BodyId body_id);
  [[nodiscard]] Result<std::size_t> store_reference_result(CachedReferencePartResult result);
  [[nodiscard]] const CachedReferencePartResult* reference_result() const noexcept;
  // Exact intent/owner check. A result from another Part, accepted digest,
  // catalog baseline or body mapping cannot be presented as this document.
  [[nodiscard]] Result<const CachedReferencePartResult*>
  validated_reference_result(const PartDocument& document) const;
  [[nodiscard]] Result<std::size_t> store_imported_body_result(FeatureId feature_id,
                                                               CachedReferencePartResult result);
  [[nodiscard]] const CachedImportedBodyFeatureResult*
  find_imported_body_result(const FeatureId& feature_id) const noexcept;
  [[nodiscard]] const GeometryShape* find_imported_body_shape(const FeatureId& feature_id,
                                                              const BodyId& body_id) const noexcept;
  [[nodiscard]] Result<const CachedImportedBodyFeatureResult*>
  validated_imported_body_result(const PartDocument& document,
                                 const ImportedBodyFeature& feature) const;
  [[nodiscard]] Result<std::size_t>
  validate_imported_body_owners(const PartDocument& document) const;
  // Remove only orphaned, untouched base outputs in a disposable recompute cache.
  // Deleted modifier results or reused authorities require an explicit clean rebuild.
  [[nodiscard]] Result<std::size_t> reconcile_imported_body_owners(const PartDocument& document);
  [[nodiscard]] Result<std::size_t>
  store_body_transform_state(BodyTransformId transform_id, BodyId body_id,
                             GeometryAffineTransform cumulative_transform);
  [[nodiscard]] Result<std::size_t>
  store_reference_transform(std::string reference_id, BodyId body_id,
                            BodyTransformId source_transform_id,
                            GeometryAffineTransform cumulative_transform);
  void clear_final_shape() noexcept;

  void clear() noexcept;

  [[nodiscard]] const ShapeCacheId& id() const noexcept;
  [[nodiscard]] const std::vector<CachedFeatureShape>& feature_shapes() const noexcept;
  [[nodiscard]] std::size_t feature_shape_count() const noexcept;
  [[nodiscard]] const GeometryShape* find_feature_shape(const FeatureId& feature_id) const noexcept;
  [[nodiscard]] const std::vector<CachedBodyShape>& body_shapes() const noexcept;
  [[nodiscard]] std::size_t body_shape_count() const noexcept;
  [[nodiscard]] const CachedBodyShape* find_body_result(const BodyId& body_id) const noexcept;
  [[nodiscard]] const GeometryShape* find_body_shape(const BodyId& body_id) const noexcept;
  [[nodiscard]] const CachedBodyTransformState*
  find_body_transform_state(const BodyTransformId& transform_id) const noexcept;
  [[nodiscard]] const CachedBodyTransformState*
  find_latest_body_transform_state(const BodyId& body_id) const noexcept;
  [[nodiscard]] const CachedReferenceTransform*
  find_reference_transform(std::string_view reference_id) const noexcept;
  [[nodiscard]] const CachedReferenceTransform*
  find_reference_transform(std::string_view reference_id,
                           const BodyTransformId& source_transform_id) const noexcept;
  [[nodiscard]] bool has_final_shape() const noexcept;
  [[nodiscard]] const FeatureId& final_feature_id() const noexcept;
  [[nodiscard]] const GeometryShape* final_shape() const noexcept;

  // Exact source/scope validation. A Core plan alone is never a fresh result.
  [[nodiscard]] Result<std::size_t>
  validate_complete_history_result(const PartDocument& document) const;
  [[nodiscard]] Result<std::size_t>
  validate_history_input(const PartDocument& document, std::string_view executing_feature = {},
                         std::string_view source_feature = {}) const;
  [[nodiscard]] bool has_history_scope() const noexcept;
  [[nodiscard]] const PartHistoryEvaluationPlan* history_plan() const noexcept;
  [[nodiscard]] bool history_body_available(const BodyId& body) const noexcept;
  [[nodiscard]] const PartHistoryBodyAvailability*
  history_body_input(std::string_view feature, const BodyId& body) const noexcept;

private:
  friend class GeometryRecomputeExecutor;
  struct HistoryResult {
    std::string source_json;
    std::uint64_t executable_generation;
    PartHistoryEvaluationPlan plan;
    bool evaluating = true;
    bool sealed = false;
    std::string current_feature;
  };
  void invalidate_history_result() noexcept;
  [[nodiscard]] Result<std::size_t> match_history_source(const PartDocument& document) const;
  explicit ShapeCache(ShapeCacheId id);

  [[nodiscard]] Result<std::size_t> validate_cache_input(const FeatureId& feature_id,
                                                         const GeometryShape& shape) const;

  ShapeCacheId id_;
  std::vector<CachedFeatureShape> feature_shapes_;
  std::vector<CachedBodyShape> body_shapes_;
  std::vector<CachedBodyTransformState> body_transform_states_;
  std::vector<CachedReferenceTransform> reference_transforms_;
  std::optional<CachedReferencePartResult> reference_result_;
  std::vector<CachedImportedBodyFeatureResult> imported_body_results_;
  bool has_final_shape_ = false;
  FeatureId final_feature_id_;
  GeometryShape final_shape_;
  std::optional<HistoryResult> history_result_;
};

} // namespace blcad::geometry
