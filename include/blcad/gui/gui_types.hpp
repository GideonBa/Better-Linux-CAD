#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace blcad::gui {

enum class GuiDocumentKind { None, Part, Project };

enum class GuiWorkspace { Part, Sketch, Assembly, Inspect, Exchange };

enum class GuiTaskStage { Inactive, CollectingSelection, EditingParameters, Preview };

enum class GuiSelectionKind {
  Body,
  Face,
  Edge,
  Vertex,
  Datum,
  SketchEntity,
  Component,
  AssemblyTarget,
};

[[nodiscard]] constexpr std::uint32_t selection_kind_bit(GuiSelectionKind kind) noexcept {
  return std::uint32_t{1} << static_cast<std::uint32_t>(kind);
}

inline constexpr std::string_view kGuiSketchHitRoleMarker = "\x1F" "blcad-hit-role:";

struct GuiSelection {
  GuiSelectionKind kind{GuiSelectionKind::Body};
  std::string semantic_id;
  // Sketch interaction may expose several selectable roles for one persistent
  // entity. candidate_id preserves the exact endpoint/curve/glyph role while
  // semantic_id remains the clean document-level identity.
  std::string candidate_id;

  GuiSelection() = default;

  GuiSelection(GuiSelectionKind selection_kind, std::string semantic,
               std::string candidate = {})
      : kind(selection_kind), semantic_id(std::move(semantic)),
        candidate_id(std::move(candidate)) {
    if (kind != GuiSelectionKind::SketchEntity || !candidate_id.empty()) return;
    const auto marker = semantic_id.find(kGuiSketchHitRoleMarker);
    if (marker == std::string::npos) return;
    candidate_id = semantic_id.substr(marker + kGuiSketchHitRoleMarker.size());
    semantic_id.erase(marker);
  }

  [[nodiscard]] bool operator==(const GuiSelection&) const = default;
};

struct GuiCommandContext {
  GuiDocumentKind document_kind{GuiDocumentKind::None};
  GuiWorkspace workspace{GuiWorkspace::Part};
  GuiTaskStage task_stage{GuiTaskStage::Inactive};
  std::size_t selection_count{0};
  std::uint32_t selection_kind_mask{0};
  bool derived_results_fresh{false};

  [[nodiscard]] bool has_document() const noexcept {
    return document_kind != GuiDocumentKind::None;
  }

  [[nodiscard]] bool task_active() const noexcept {
    return task_stage != GuiTaskStage::Inactive;
  }

  [[nodiscard]] bool has_selection_kind(GuiSelectionKind kind) const noexcept {
    return (selection_kind_mask & selection_kind_bit(kind)) != 0;
  }
};

} // namespace blcad::gui
