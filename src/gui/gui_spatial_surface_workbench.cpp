#include "blcad/gui/gui_spatial_surface_workbench.hpp"

#include <utility>

namespace blcad::gui {
namespace {
template <typename Object, typename Add>
Result<GuiSpatialFeaturePreview> preview(const GuiDocumentSession& session, Object object, Add add,
                                         FeatureId id, std::string summary, BodyKind output) {
  if (!session.part_document())
    return Result<GuiSpatialFeaturePreview>::failure(
        Error::validation("gui.spatial_surface", "preview requires an active Part document"));
  PartDocument candidate = *session.part_document();
  auto added = (candidate.*add)(std::move(object));
  if (added.has_error()) return Result<GuiSpatialFeaturePreview>::failure(added.error());
  auto plan = candidate.create_recompute_plan();
  if (plan.has_error()) return Result<GuiSpatialFeaturePreview>::failure(plan.error());
  return Result<GuiSpatialFeaturePreview>::success(
      {std::move(id), std::move(summary), plan.value().step_count(), output});
}
template <typename Object, typename Add>
Result<std::size_t> apply(GuiDocumentSession& session, std::string label, Object object, Add add) {
  if (!session.part_document())
    return Result<std::size_t>::failure(
        Error::validation("gui.spatial_surface", "Apply requires an active Part document"));
  return session.commit_part_transaction(std::move(label),
      [object = std::move(object), add](PartDocument& part) mutable {
        return (part.*add)(std::move(object));
      });
}
} // namespace

GuiSpatialSelectionPrompt GuiSpatialSurfaceWorkbench::prompt_for(GuiSpatialCommand command) {
  switch (command) {
  case GuiSpatialCommand::Sketch3D: return {"Enter model-space point/curve coordinates and ordered references", GuiSelectionKind::SketchEntity, "model_space_curve_entities"};
  case GuiSpatialCommand::PathCurve: return {"Select a connected ordered chain of planar or 3D sketch curves", GuiSelectionKind::SketchEntity, "connected_ordered_path"};
  case GuiSpatialCommand::Sweep: return {"Select profile, path, optional guide, orientation, and twist", GuiSelectionKind::SketchEntity, "profile_path_guide"};
  case GuiSpatialCommand::PathExtrude: return {"Select profile and connected path permitted by Extrude/Cut intent", GuiSelectionKind::SketchEntity, "profile_and_path"};
  case GuiSpatialCommand::Loft: return {"Select ordered sections, optional guides, and continuity", GuiSelectionKind::SketchEntity, "ordered_sections_and_guides"};
  case GuiSpatialCommand::BoundaryFill: return {"Select ordered boundary paths or semantic edges", GuiSelectionKind::Edge, "surface_boundaries"};
  case GuiSpatialCommand::TrimExtend: return {"Select a Surface Body and trimming/boundary input", GuiSelectionKind::Face, "surface_and_trim_boundary"};
  case GuiSpatialCommand::Stitch: return {"Select ordered Surface Bodies and tolerance", GuiSelectionKind::Body, "ordered_surface_bodies"};
  case GuiSpatialCommand::ClosedShellToSolid: return {"Select one closed Surface shell and a Solid result Body", GuiSelectionKind::Body, "closed_shell"};
  }
  return {"Select compatible spatial inputs", std::nullopt, "spatial_input"};
}

Result<std::size_t> GuiSpatialSurfaceWorkbench::create_sketch_3d(GuiDocumentSession& session, Sketch3D sketch) const {
  return apply(session, "Create 3D sketch", std::move(sketch), &PartDocument::add_sketch_3d);
}
Result<std::size_t> GuiSpatialSurfaceWorkbench::create_path_curve(GuiDocumentSession& session, PathCurve path) const {
  return apply(session, "Create PathCurve", std::move(path), &PartDocument::add_path_curve);
}
Result<GuiSpatialFeaturePreview> GuiSpatialSurfaceWorkbench::preview_path_extrude(const GuiDocumentSession& session, Feature feature) const {
  const auto id = feature.id();
  return preview(session, std::move(feature), &PartDocument::add_feature, id,
                 "path-following Extrude/Cut with explicit profile and path", BodyKind::Solid);
}
Result<std::size_t> GuiSpatialSurfaceWorkbench::apply_path_extrude(GuiDocumentSession& session, Feature feature) const {
  auto checked = preview_path_extrude(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return apply(session, "Apply path Extrude", std::move(feature), &PartDocument::add_feature);
}
Result<GuiSpatialFeaturePreview> GuiSpatialSurfaceWorkbench::preview_sweep(const GuiDocumentSession& session, SweepFeature feature) const {
  const auto id = feature.id(); const BodyKind kind = feature.kind() == SweepFeatureKind::SweepSurface ? BodyKind::Surface : BodyKind::Solid;
  return preview(session, std::move(feature), &PartDocument::add_sweep_feature, id,
                 "Sweep with ordered profile/path/guide and orientation intent", kind);
}
Result<std::size_t> GuiSpatialSurfaceWorkbench::apply_sweep(GuiDocumentSession& session, SweepFeature feature) const {
  auto checked = preview_sweep(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return apply(session, "Apply Sweep", std::move(feature), &PartDocument::add_sweep_feature);
}
Result<GuiSpatialFeaturePreview> GuiSpatialSurfaceWorkbench::preview_loft(const GuiDocumentSession& session, LoftFeature feature) const {
  const auto id = feature.id(); const BodyKind kind = feature.kind() == LoftFeatureKind::LoftSurface ? BodyKind::Surface : BodyKind::Solid;
  return preview(session, std::move(feature), &PartDocument::add_loft_feature, id,
                 "Loft with ordered sections, guides, alignment, and continuity", kind);
}
Result<std::size_t> GuiSpatialSurfaceWorkbench::apply_loft(GuiDocumentSession& session, LoftFeature feature) const {
  auto checked = preview_loft(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return apply(session, "Apply Loft", std::move(feature), &PartDocument::add_loft_feature);
}
Result<GuiSpatialFeaturePreview> GuiSpatialSurfaceWorkbench::preview_surface(const GuiDocumentSession& session, SurfaceFeature feature) const {
  const auto id = surface_feature_id(feature); const auto kind = surface_feature_kind(feature);
  const BodyKind output = kind == SurfaceFeatureKind::ClosedShellToSolid ? BodyKind::Solid : BodyKind::Surface;
  return preview(session, std::move(feature), &PartDocument::add_surface_feature, id,
                 std::string(to_string(kind)) + " with ordered surface inputs", output);
}
Result<std::size_t> GuiSpatialSurfaceWorkbench::apply_surface(GuiDocumentSession& session, SurfaceFeature feature) const {
  auto checked = preview_surface(session, feature); if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return apply(session, "Apply Surface feature", std::move(feature), &PartDocument::add_surface_feature);
}

} // namespace blcad::gui
