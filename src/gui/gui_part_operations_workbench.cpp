#include "blcad/gui/gui_part_operations_workbench.hpp"

#include "blcad/geometry/step_exporter.hpp"

#include <utility>

namespace blcad::gui {
namespace {
template <typename Object, typename Add>
Result<GuiPartOperationPreview> preview(const GuiDocumentSession& session, Object object, Add add,
                                        FeatureId id, std::string summary) {
  if (!session.part_document())
    return Result<GuiPartOperationPreview>::failure(
        Error::validation("gui.part_operations", "preview requires an active Part document"));
  PartDocument candidate = *session.part_document();
  auto added = (candidate.*add)(std::move(object));
  if (added.has_error()) return Result<GuiPartOperationPreview>::failure(added.error());
  auto plan = candidate.create_recompute_plan();
  if (plan.has_error()) return Result<GuiPartOperationPreview>::failure(plan.error());
  return Result<GuiPartOperationPreview>::success(
      {std::move(id), std::move(summary), plan.value().step_count()});
}

template <typename Object, typename Add>
Result<std::size_t> apply(GuiDocumentSession& session, std::string label, Object object, Add add) {
  if (!session.part_document())
    return Result<std::size_t>::failure(
        Error::validation("gui.part_operations", "Apply requires an active Part document"));
  return session.commit_part_transaction(std::move(label),
      [object = std::move(object), add](PartDocument& part) mutable {
        return (part.*add)(std::move(object));
      });
}
} // namespace

GuiPartOperationPrompt GuiPartOperationsWorkbench::prompt_for(GuiPartOperationCommand command) {
  switch (command) {
  case GuiPartOperationCommand::LinearPattern: return {"Select ordered seed features/Bodies and an axis-capable direction", std::nullopt, "ordered_sources_and_axis"};
  case GuiPartOperationCommand::CircularPattern: return {"Select ordered seed features/Bodies and a rotation axis", std::nullopt, "ordered_sources_and_axis"};
  case GuiPartOperationCommand::Mirror: return {"Select ordered seed features/Bodies and a datum, construction, or planar-face mirror plane", std::nullopt, "ordered_sources_and_plane"};
  case GuiPartOperationCommand::Fillet: return {"Select stable semantic edges on one target Body", GuiSelectionKind::Edge, "fillet_edges"};
  case GuiPartOperationCommand::Chamfer: return {"Select stable semantic edges and a supported chamfer mode", GuiSelectionKind::Edge, "chamfer_edges"};
  case GuiPartOperationCommand::Shell: return {"Select a target Body and ordered removable planar faces", GuiSelectionKind::Face, "shell_faces"};
  case GuiPartOperationCommand::Draft: return {"Select faces, pull direction, and neutral plane", GuiSelectionKind::Face, "draft_faces_direction_plane"};
  case GuiPartOperationCommand::BodyBoolean: return {"Select one target Body and ordered tool Bodies", GuiSelectionKind::Body, "target_and_tool_bodies"};
  case GuiPartOperationCommand::BodyTransform: return {"Select one Body and a coordinate-space reference when required", GuiSelectionKind::Body, "body_transform"};
  case GuiPartOperationCommand::StepExport: return {"Export all visible recomputed Bodies", GuiSelectionKind::Body, "fresh_visible_body_results"};
  }
  return {"Select compatible Part inputs", std::nullopt, "part_operation_input"};
}

#define BLCAD_GUI_PART_IMPL(NAME, TYPE, ADD, SUMMARY) \
Result<GuiPartOperationPreview> GuiPartOperationsWorkbench::preview_##NAME(const GuiDocumentSession& session, TYPE value) const { \
  const auto id = value.id(); return preview(session, std::move(value), &PartDocument::ADD, id, SUMMARY); \
} \
Result<std::size_t> GuiPartOperationsWorkbench::apply_##NAME(GuiDocumentSession& session, TYPE value) const { \
  auto checked = preview_##NAME(session, value); if (checked.has_error()) return Result<std::size_t>::failure(checked.error()); \
  return apply(session, "Apply " #NAME, std::move(value), &PartDocument::ADD); \
}

BLCAD_GUI_PART_IMPL(linear_pattern, LinearPatternFeature, add_linear_pattern_feature, "linear pattern with ordered sources")
BLCAD_GUI_PART_IMPL(circular_pattern, CircularPatternFeature, add_circular_pattern_feature, "circular pattern with ordered sources")
BLCAD_GUI_PART_IMPL(mirror, MirrorFeature, add_mirror_feature, "mirror with ordered sources")
BLCAD_GUI_PART_IMPL(fillet, FilletFeature, add_fillet_feature, "fillet with semantic edge targets")
BLCAD_GUI_PART_IMPL(chamfer, ChamferFeature, add_chamfer_feature, "chamfer with semantic edge targets")
BLCAD_GUI_PART_IMPL(shell, ShellFeature, add_shell_feature, "shell with ordered removal faces")
BLCAD_GUI_PART_IMPL(draft, DraftFeature, add_draft_feature, "draft with direction and neutral plane")
BLCAD_GUI_PART_IMPL(body_boolean, BodyBooleanFeature, add_body_boolean_feature, "Body Boolean with ordered tools")

#undef BLCAD_GUI_PART_IMPL

Result<GuiPartOperationPreview> GuiPartOperationsWorkbench::preview_body_transform(
    const GuiDocumentSession& session, BodyTransform value) const {
  const FeatureId id("body-transform:" + value.id().value());
  return preview(session, std::move(value), &PartDocument::add_body_transform, id,
                 "associative Body transform");
}
Result<std::size_t> GuiPartOperationsWorkbench::apply_body_transform(GuiDocumentSession& session,
                                                                     BodyTransform value) const {
  auto checked = preview_body_transform(session, value);
  if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
  return apply(session, "Apply Body transform", std::move(value), &PartDocument::add_body_transform);
}
Result<std::size_t> GuiPartOperationsWorkbench::export_visible_bodies_step(
    const GuiDocumentSession& session, const std::filesystem::path& path) const {
  if (!session.part_document() || !session.part_shape_cache() || !session.derived_results_fresh())
    return Result<std::size_t>::failure(Error::export_error("gui.step_export", "STEP export requires fresh recomputed Part Body results"));
  auto result = geometry::StepExporter{}.write_part_step(*session.part_document(),
                                                         *session.part_shape_cache(), path.string());
  return result.has_error() ? Result<std::size_t>::failure(result.error())
                            : Result<std::size_t>::success(result.value().written_bytes);
}

} // namespace blcad::gui
