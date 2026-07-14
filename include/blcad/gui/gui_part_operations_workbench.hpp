#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace blcad::gui {

enum class GuiPartOperationCommand { LinearPattern, CircularPattern, Mirror, Fillet, Chamfer, Shell, Draft, BodyBoolean, BodyTransform, StepExport };

struct GuiPartOperationPrompt {
  std::string text;
  std::optional<GuiSelectionKind> required_kind;
  std::string required_capability;
};

struct GuiPartOperationPreview {
  FeatureId feature;
  std::string semantic_summary;
  std::size_t affected_nodes{0};
};

class GuiPartOperationsWorkbench {
public:
  [[nodiscard]] static GuiPartOperationPrompt prompt_for(GuiPartOperationCommand);

#define BLCAD_GUI_PART_OPERATION(NAME, TYPE) \
  [[nodiscard]] Result<GuiPartOperationPreview> preview_##NAME(const GuiDocumentSession&, TYPE) const; \
  [[nodiscard]] Result<std::size_t> apply_##NAME(GuiDocumentSession&, TYPE) const;
  BLCAD_GUI_PART_OPERATION(linear_pattern, LinearPatternFeature)
  BLCAD_GUI_PART_OPERATION(circular_pattern, CircularPatternFeature)
  BLCAD_GUI_PART_OPERATION(mirror, MirrorFeature)
  BLCAD_GUI_PART_OPERATION(fillet, FilletFeature)
  BLCAD_GUI_PART_OPERATION(chamfer, ChamferFeature)
  BLCAD_GUI_PART_OPERATION(shell, ShellFeature)
  BLCAD_GUI_PART_OPERATION(draft, DraftFeature)
  BLCAD_GUI_PART_OPERATION(body_boolean, BodyBooleanFeature)
#undef BLCAD_GUI_PART_OPERATION

  [[nodiscard]] Result<GuiPartOperationPreview> preview_body_transform(
      const GuiDocumentSession&, BodyTransform) const;
  [[nodiscard]] Result<std::size_t> apply_body_transform(GuiDocumentSession&, BodyTransform) const;
  [[nodiscard]] Result<std::size_t> export_visible_bodies_step(const GuiDocumentSession&,
                                                               const std::filesystem::path&) const;
};

} // namespace blcad::gui
