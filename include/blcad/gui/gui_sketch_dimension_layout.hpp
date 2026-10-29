#pragma once

#include "blcad/gui/gui_sketch_interaction.hpp"

#include <array>
#include <optional>
#include <vector>

namespace blcad::gui {
enum class GuiDimensionStrokeRole { Dimension, Extension, Leader, Origin };
struct GuiDimensionScreenStroke {
  GuiDimensionStrokeRole role;
  std::vector<GuiSketchScreenPoint> points;
  std::vector<Point2> plane_points;
};
struct GuiDimensionScreenArrow {
  std::array<GuiSketchScreenPoint, 3> polygon;
  Point2 plane_tip;
};
struct GuiDimensionScreenLayout {
  GuiSketchScreenPoint label_anchor;
  std::optional<GuiSketchScreenRect> text_bounds;
  std::vector<GuiSketchScreenPoint> measured_references;
  std::vector<GuiDimensionScreenStroke> strokes;
  std::vector<GuiDimensionScreenArrow> arrows;
};
struct GuiDimensionLayoutMetrics {
  double arrow_length_dip{7};
  double witness_gap_dip{3};
  double witness_overrun_dip{5};
  double arc_error_dip{.25};
  double text_gap_dip{3};
};
struct GuiDimensionLayoutHit {
  Point2 plane_point;
  double distance_dip;
};

// Painting and hit testing consume the same projected strokes and triangles.
// The mapping is refreshed by the viewport; all metrics use logical DIPs.
[[nodiscard]] Result<GuiDimensionScreenLayout>
resolve_dimension_screen_layout(const GuiSketchAnnotationPrimitive& annotation,
                                const GuiSketchPlaneMapping& mapping,
                                GuiDimensionLayoutMetrics metrics = {});
[[nodiscard]] std::optional<GuiDimensionLayoutHit>
hit_dimension_screen_layout(const GuiDimensionScreenLayout& layout, GuiSketchScreenPoint point,
                            double tolerance_dip);
} // namespace blcad::gui
