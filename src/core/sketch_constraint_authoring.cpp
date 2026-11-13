#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/core/sketch_dimension_catalog_system.hpp"

namespace blcad {
Result<SketchConstraintAuthoringPreview> SketchConstraintAuthoringService::preview(
    const PartDocument& document, const SketchConstraintCatalog& catalog,
    const SketchDimensionCatalog& dimensions, const SketchConstraintIntent& candidate) const {
  auto after = catalog;
  auto added = after.add(candidate);
  if (!added)
    return Result<SketchConstraintAuthoringPreview>::failure(added.error());
  auto solved = SketchDimensionCatalogSystemBuilder::solve(document, after, dimensions);
  if (!solved)
    return Result<SketchConstraintAuthoringPreview>::failure(solved.error());
  auto result = std::move(solved.value());
  if (!result.accepted)
    after = catalog;
  return Result<SketchConstraintAuthoringPreview>::success(
      {candidate, std::move(after), std::move(result.solve), std::move(result.solved_sketch),
       result.accepted, std::move(result.conflict_ids), std::move(result.redundant_ids)});
}
} // namespace blcad
