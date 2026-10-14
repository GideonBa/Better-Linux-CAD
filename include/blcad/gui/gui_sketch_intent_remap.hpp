#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/core/sketch_dimension_authoring.hpp"
#include "blcad/core/sketch_topology.hpp"

#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

// Keep authored GUI intent valid when legacy coincident endpoints are folded
// into the canonical topology used by a newly materialized closed profile.
// Shared by Finish Sketch and feature-time recovery of legacy/raw contours.
inline std::string remapped_point_identity(const SketchTopologyMigrationReport& migration,
                                           std::string identity) {
  for (const auto& change : migration.identity_changes())
    if (change.previous_identity == identity) {
      identity = change.canonical_identity;
      break;
    }
  return identity;
}

inline Result<std::size_t> remap_sketch_intent_catalogs(
    const DocumentId& document, const SketchId& sketch,
    const SketchTopologyMigrationReport& migration,
    std::vector<SketchConstraintCatalog>& constraint_catalogs,
    std::vector<SketchDimensionCatalog>& dimension_catalogs) {
  std::size_t changed = 0U;
  const auto remap_target = [&migration](const SketchConstraintIntentTarget& target)
      -> Result<SketchConstraintIntentTarget> {
    if (target.kind() == SketchConstraintIntentTargetKind::Entity)
      return SketchConstraintIntentTarget::entity(target.id());
    return SketchConstraintIntentTarget::point(
        SketchPointId(remapped_point_identity(migration, target.id())));
  };

  for (auto& catalog : constraint_catalogs) {
    if (catalog.sketch() != sketch)
      continue;
    std::vector<SketchConstraintIntent> rebuilt;
    rebuilt.reserve(catalog.constraints().size());
    for (const auto& intent : catalog.constraints()) {
      std::vector<SketchConstraintIntentTarget> targets;
      targets.reserve(intent.targets().size());
      for (const auto& target : intent.targets()) {
        auto remapped = remap_target(target);
        if (remapped.has_error())
          return Result<std::size_t>::failure(remapped.error());
        targets.push_back(std::move(remapped.value()));
      }
      // Shared topology now carries this coincidence; keeping the old
      // same-target constraint would be redundant and invalid.
      if (intent.kind() == SketchSolverConstraintKind::Coincident && targets.size() == 2U &&
          targets.front() == targets.back()) {
        ++changed;
        continue;
      }
      auto remapped = SketchConstraintIntent::create(intent.id(), intent.kind(),
                                                     std::move(targets), intent.source());
      if (remapped.has_error())
        return Result<std::size_t>::failure(remapped.error());
      rebuilt.push_back(std::move(remapped.value()));
    }
    auto replacement =
        SketchConstraintCatalog::create(document, sketch, std::move(rebuilt));
    if (replacement.has_error())
      return Result<std::size_t>::failure(replacement.error());
    catalog = std::move(replacement.value());
  }

  for (auto& catalog : dimension_catalogs) {
    if (catalog.sketch() != sketch)
      continue;
    std::vector<SketchDimensionIntent> rebuilt;
    rebuilt.reserve(catalog.dimensions().size());
    for (const auto& dimension : catalog.dimensions()) {
      std::vector<SketchConstraintIntentTarget> targets;
      targets.reserve(dimension.targets().size());
      for (const auto& target : dimension.targets()) {
        auto remapped = remap_target(target);
        if (remapped.has_error())
          return Result<std::size_t>::failure(remapped.error());
        targets.push_back(std::move(remapped.value()));
      }
      auto remapped = SketchDimensionIntent::create(
          dimension.id(), dimension.kind(), dimension.mode(), std::move(targets),
          dimension.parameter(), dimension.label_offset());
      if (remapped.has_error())
        return Result<std::size_t>::failure(remapped.error());
      rebuilt.push_back(std::move(remapped.value()));
    }
    auto replacement = SketchDimensionCatalog::create(document, sketch, std::move(rebuilt));
    if (replacement.has_error())
      return Result<std::size_t>::failure(replacement.error());
    catalog = std::move(replacement.value());
  }
  return Result<std::size_t>::success(changed);
}

} // namespace blcad::gui
