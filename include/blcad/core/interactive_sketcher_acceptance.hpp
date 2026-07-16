#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace blcad {

enum class SketchAcceptanceEvidence : unsigned {
  MouseScriptEquivalence = 1U << 0U,
  PersistenceRecompute = 1U << 1U,
  ExactUndoRedo = 1U << 2U,
  ConflictAtomicity = 1U << 3U,
  ReferenceRepair = 1U << 4U,
  KeyboardApplyCancel = 1U << 5U,
  HighDpiMapping = 1U << 6U,
  NoStalePreview = 1U << 7U
};

constexpr unsigned operator|(SketchAcceptanceEvidence left, SketchAcceptanceEvidence right) noexcept {
  return static_cast<unsigned>(left) | static_cast<unsigned>(right);
}

struct SketchToolCoverageRecord {
  std::string_view capability;
  std::string_view contract;
  std::string_view focused_tag;
  unsigned evidence;
};

struct SketchInteractionPerformanceBudget {
  std::string_view operation;
  std::size_t representative_entity_count;
  double budget_milliseconds;
};

class InteractiveSketcherAcceptance final {
public:
  static constexpr unsigned all_evidence =
      static_cast<unsigned>(SketchAcceptanceEvidence::MouseScriptEquivalence) |
      static_cast<unsigned>(SketchAcceptanceEvidence::PersistenceRecompute) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ExactUndoRedo) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ConflictAtomicity) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ReferenceRepair) |
      static_cast<unsigned>(SketchAcceptanceEvidence::KeyboardApplyCancel) |
      static_cast<unsigned>(SketchAcceptanceEvidence::HighDpiMapping) |
      static_cast<unsigned>(SketchAcceptanceEvidence::NoStalePreview);

  [[nodiscard]] static constexpr std::array<SketchToolCoverageRecord, 15> coverage() noexcept {
    return {{{"workspace-command-lifecycle", "docs/gui-interactive-sketch-workspace-mvp8.md", "[integration][interactive-sketcher]", all_evidence},
             {"plane-mapping-hit-testing-snapping", "docs/gui-sketch-plane-interaction-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"shared-planar-topology", "docs/sketch-shared-topology-mvp8.md", "[integration][interactive-sketcher]", all_evidence},
             {"planar-solver-diagnostics", "docs/sketch-planar-constraint-solver-mvp8.md", "[integration][interactive-sketcher]", all_evidence},
             {"solver-backed-drag", "docs/gui-sketch-solver-drag-mvp8.md", "[performance][sketch-interaction]", all_evidence},
             {"basic-creation", "docs/gui-sketch-basic-creation-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"conics-and-slots", "docs/gui-sketch-conic-slot-creation-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"splines-and-text", "docs/gui-sketch-spline-text-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"constraint-authoring", "docs/gui-sketch-constraint-authoring-mvp8.md", "[integration][interactive-sketcher]", all_evidence},
             {"dimension-authoring", "docs/gui-sketch-dimension-authoring-mvp8.md", "[integration][interactive-sketcher]", all_evidence},
             {"modify-tools", "docs/gui-sketch-modify-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"offset-project-references", "docs/gui-sketch-offset-project-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"transform-mirror-pattern", "docs/gui-sketch-transform-pattern-mvp8.md", "[integration][sketch-gui-headless]", all_evidence},
             {"regions-and-finish", "docs/gui-sketch-regions-finish-mvp8.md", "[performance][sketch-interaction]", all_evidence},
             {"interactive-sketch3d", "docs/gui-sketch3d-interaction-mvp8.md", "[integration][interactive-sketcher]", all_evidence}}};
  }

  [[nodiscard]] static constexpr std::array<SketchInteractionPerformanceBudget, 4>
  performance_budgets() noexcept {
    return {{{"hover-hit-test", 500U, 8.0},
             {"solver-backed-drag-frame", 100U, 16.0},
             {"planar-solve", 250U, 50.0},
             {"region-recognition", 500U, 50.0}}};
  }

  [[nodiscard]] static constexpr bool covers_all_evidence(unsigned evidence) noexcept {
    return (evidence & all_evidence) == all_evidence;
  }
};

} // namespace blcad
