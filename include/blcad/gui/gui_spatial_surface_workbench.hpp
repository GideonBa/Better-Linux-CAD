#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <optional>
#include <string>

namespace blcad::gui {

enum class GuiSpatialCommand { Sketch3D, PathCurve, Sweep, PathExtrude, Loft, BoundaryFill, TrimExtend, Stitch, ClosedShellToSolid };

struct GuiSpatialSelectionPrompt {
  std::string text;
  std::optional<GuiSelectionKind> required_kind;
  std::string required_capability;
};

struct GuiSpatialFeaturePreview {
  FeatureId feature;
  std::string semantic_summary;
  std::size_t affected_nodes{0};
  BodyKind output_kind{BodyKind::Solid};
};

class GuiSpatialSurfaceWorkbench {
public:
  [[nodiscard]] static GuiSpatialSelectionPrompt prompt_for(GuiSpatialCommand);
  [[nodiscard]] Result<std::size_t> create_sketch_3d(GuiDocumentSession&, Sketch3D) const;
  [[nodiscard]] Result<std::size_t> create_path_curve(GuiDocumentSession&, PathCurve) const;

  [[nodiscard]] Result<GuiSpatialFeaturePreview> preview_path_extrude(const GuiDocumentSession&, Feature) const;
  [[nodiscard]] Result<std::size_t> apply_path_extrude(GuiDocumentSession&, Feature) const;
  [[nodiscard]] Result<GuiSpatialFeaturePreview> preview_sweep(const GuiDocumentSession&, SweepFeature) const;
  [[nodiscard]] Result<std::size_t> apply_sweep(GuiDocumentSession&, SweepFeature) const;
  [[nodiscard]] Result<GuiSpatialFeaturePreview> preview_loft(const GuiDocumentSession&, LoftFeature) const;
  [[nodiscard]] Result<std::size_t> apply_loft(GuiDocumentSession&, LoftFeature) const;
  [[nodiscard]] Result<GuiSpatialFeaturePreview> preview_surface(const GuiDocumentSession&, SurfaceFeature) const;
  [[nodiscard]] Result<std::size_t> apply_surface(GuiDocumentSession&, SurfaceFeature) const;
};

} // namespace blcad::gui
