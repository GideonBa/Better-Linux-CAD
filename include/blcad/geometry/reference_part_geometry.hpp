#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/shape_cache.hpp"

namespace blcad::geometry {

class StepImportTopologyContext;

// Runtime geometry of an immutable imported Part. Body and topology identities
// remain Core intent; no synthetic feature producer is introduced for Reference.
class ReferencePartGeometry {
public:
  [[nodiscard]] Result<CachedReferencePartResult> resolve(const PartDocument& document,
                                                          StepImportTopologyContext& context) const;
  [[nodiscard]] Result<CachedReferencePartResult>
  resolve_imported_body_feature(const PartDocument& document, const ImportedBodyFeature& feature,
                                StepImportTopologyContext& context) const;

  // Both seed operations publish a complete cache copy only after every selected
  // body has uniquely recovered with the authored kind and source bytes reverify.
  [[nodiscard]] Result<std::size_t> seed_reference(const PartDocument& document,
                                                   StepImportTopologyContext& context,
                                                   ShapeCache& cache) const;
  [[nodiscard]] Result<std::size_t> seed_imported_body_feature(const PartDocument& document,
                                                               const ImportedBodyFeature& feature,
                                                               StepImportTopologyContext& context,
                                                               ShapeCache& cache) const;

private:
  [[nodiscard]] Result<CachedReferencePartResult>
  resolve_definition(const PartDocument& document, const ImportedPartDefinition& definition,
                     StepImportTopologyContext& context) const;
};

} // namespace blcad::geometry
