#pragma once

#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/gui/gui_modeling_workspace.hpp"
#include "blcad/gui/gui_sketch_intent_remap.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

// The retained thin adapter consumes one real open line, without treating
// additional curves or profile records as part of that wall.
inline const LineSegment* interactive_thin_line(const Sketch& sketch) {
  if (sketch.profile_count() != 0 || sketch.line_segments().size() != 1 ||
      !sketch.arc_segments().empty() || !sketch.spline_segments().empty() ||
      !sketch.projected_lines().empty() || !sketch.reference_generated_lines().empty())
    return nullptr;
  const auto& line = sketch.line_segments().front();
  return std::hypot(line.end().x - line.start().x, line.end().y - line.start().y) > 1e-7 ? &line
                                                                                         : nullptr;
}

inline std::optional<SketchId> interactive_thin_selection(const PartDocument& part,
                                                          std::string_view semantic) {
  if (const auto* sketch = part.find_sketch(SketchId(std::string(semantic))))
    return interactive_thin_line(*sketch) ? std::optional(sketch->id()) : std::nullopt;
  if (!semantic.starts_with("sketch/"))
    return std::nullopt;
  semantic.remove_prefix(7);
  const auto slash = semantic.find('/');
  if (slash == std::string_view::npos)
    return std::nullopt;
  const auto* sketch = part.find_sketch(SketchId(std::string(semantic.substr(0, slash))));
  const auto* line = sketch ? interactive_thin_line(*sketch) : nullptr;
  if (!line)
    return std::nullopt;
  semantic.remove_prefix(slash + 1);
  for (const auto prefix : {std::string_view("line/"), std::string_view("entity/")})
    if (semantic.starts_with(prefix) && semantic.substr(prefix.size()) == line->id().value())
      return sketch->id();
  return std::nullopt;
}

struct GuiPendingProfileMaterialization {
  Sketch completed_sketch;
  ProfileId selected_profile;
  SketchTopologyMigrationReport migration;
};

// Keep the GUI call sites explicit while delegating identity and filtering to
// the geometry-layer authority shared with viewport presentation.
inline Result<geometry::SketchRegionAnalysis>
analyze_interactive_raw_profiles(const PartDocument& part, const Sketch& source) {
  return geometry::SketchFinishService::analyze_unmaterialized(part, source);
}

// Resolve a profile without mutating the source document. Materialized
// profiles need no pending state. Legacy/raw linework may coexist with already
// materialized profiles; only still-unowned curves participate in recovery. A
// raw selection is accepted only when every remaining connected component is a
// valid bounded region, and all such regions are materialized together so no
// remaining contour becomes unreachable.
inline Result<std::optional<GuiPendingProfileMaterialization>>
prepare_interactive_profile(const PartDocument& part, const SketchId& sketch_id,
                            const ProfileId& profile_id) {
  const Sketch* source = part.find_sketch(sketch_id);
  if (source == nullptr)
    return Result<std::optional<GuiPendingProfileMaterialization>>::failure(
        Error::validation(sketch_id.value(), "interactive feature Sketch must exist"));
  if (GuiModelingWorkspace::has_materialized_profile(*source, profile_id))
    return Result<std::optional<GuiPendingProfileMaterialization>>::success(std::nullopt);

  auto analysis = analyze_interactive_raw_profiles(part, *source);
  if (analysis.has_error())
    return Result<std::optional<GuiPendingProfileMaterialization>>::failure(analysis.error());
  if (!analysis.value().diagnostics.empty() || analysis.value().regions.empty())
    return Result<std::optional<GuiPendingProfileMaterialization>>::failure(
        Error::validation(sketch_id.value(),
                          "raw Sketch must contain only closed, unambiguous regions"));
  const auto selected = std::find_if(
      analysis.value().regions.begin(), analysis.value().regions.end(),
      [&profile_id](const auto& region) { return region.id == profile_id; });
  if (selected == analysis.value().regions.end())
    return Result<std::optional<GuiPendingProfileMaterialization>>::failure(
        Error::validation(profile_id.value(), "detected profile is not available"));

  Sketch completed = *source;
  geometry::SketchRegionFinder finder;
  for (const auto& region : analysis.value().regions) {
    auto added = finder.materialize_profile(completed, region);
    if (added.has_error())
      return Result<std::optional<GuiPendingProfileMaterialization>>::failure(added.error());
  }
  SketchTopologyMigrationReport migration;
  auto topology = SketchTopology::migrate_legacy(completed, &migration);
  if (topology.has_error())
    return Result<std::optional<GuiPendingProfileMaterialization>>::failure(topology.error());
  return Result<std::optional<GuiPendingProfileMaterialization>>::success(
      GuiPendingProfileMaterialization{std::move(completed), profile_id,
                                       std::move(migration)});
}

inline Result<std::size_t>
publish_interactive_profile(PartDocument& part,
                            const GuiPendingProfileMaterialization& pending,
                            std::vector<SketchConstraintCatalog>& constraint_catalogs,
                            std::vector<SketchDimensionCatalog>& dimension_catalogs) {
  const SketchId sketch = pending.completed_sketch.id();
  auto updated = part.update_sketch(pending.completed_sketch);
  if (updated.has_error())
    return Result<std::size_t>::failure(updated.error());
  auto remapped = remap_sketch_intent_catalogs(part.id(), sketch, pending.migration,
                                               constraint_catalogs, dimension_catalogs);
  if (remapped.has_error())
    return Result<std::size_t>::failure(remapped.error());
  return Result<std::size_t>::success(1U + remapped.value());
}

} // namespace blcad::gui
