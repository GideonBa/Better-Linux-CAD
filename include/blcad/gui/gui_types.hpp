#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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

struct GuiSelection {
  GuiSelectionKind kind{GuiSelectionKind::Body};
  std::string semantic_id;

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
