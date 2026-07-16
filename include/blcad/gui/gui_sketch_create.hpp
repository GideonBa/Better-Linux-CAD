#pragma once

#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

enum class GuiSketchCreateTool {
  Point,
  Line,
  Polyline,
  CornerRectangle,
  CenterRectangle,
  ThreePointRectangle,
  Parallelogram,
  RegularPolygon,
  Centerline,
};

[[nodiscard]] std::string_view to_string(GuiSketchCreateTool tool) noexcept;

struct GuiSketchCreateOptions {
  std::size_t polygon_sides{6U};

  friend bool operator==(const GuiSketchCreateOptions&, const GuiSketchCreateOptions&) = default;
};

struct GuiSketchCreatePick {
  Point2 point;
  GuiSketchSnapKind snap{GuiSketchSnapKind::None};

  friend bool operator==(const GuiSketchCreatePick&, const GuiSketchCreatePick&) = default;
};

struct GuiSketchAutoConstraintPreview {
  std::string kind;
  Point2 anchor;

  friend bool operator==(const GuiSketchAutoConstraintPreview&,
                         const GuiSketchAutoConstraintPreview&) = default;
};

struct GuiSketchCreatePreview {
  std::vector<Point2> rubber_band;
  bool closed{false};
  std::vector<GuiSketchAutoConstraintPreview> constraints;
};

struct GuiSketchCreateExpansion {
  std::vector<LineSegment> lines;
  std::optional<ClosedProfile> profile;
  std::optional<ConstructionPoint> construction_point;
  std::optional<ConstructionLine> construction_line;
  std::string label;
};

// Headless multi-click creation authority for Block 111. Picks are snapped
// plane coordinates supplied by the Block-107 interaction layer; expansion
// produces ordinary persistent Sketch/construction intent and commit publishes
// exactly one document transaction per completed tool.
class GuiSketchCreateController {
public:
  [[nodiscard]] static Result<GuiSketchCreateController>
  begin(GuiSketchCreateTool tool, SketchId sketch, GuiSketchPlaneView plane,
        GuiSketchCreateOptions options = {});

  [[nodiscard]] GuiSketchCreateTool tool() const noexcept;
  [[nodiscard]] const SketchId& sketch() const noexcept;
  [[nodiscard]] const GuiSketchCreateOptions& options() const noexcept;
  [[nodiscard]] const std::vector<GuiSketchCreatePick>& picks() const noexcept;
  [[nodiscard]] std::size_t required_picks() const noexcept;
  [[nodiscard]] bool accepts_more_picks() const noexcept;
  [[nodiscard]] bool complete_ready() const noexcept;

  [[nodiscard]] Result<std::size_t> add_pick(GuiSketchCreatePick pick);
  [[nodiscard]] Result<std::size_t> pop_pick();

  // Applies one numeric HUD entry. For RegularPolygon before the first pick a
  // plain integer sets the side count and yields no pick. Otherwise formats
  // are: "x;y" absolute plane point before the first pick, "a;b" relative
  // offset from the last pick, and "L<A" / "L" polar length with explicit
  // angle in degrees or the direction toward the supplied hover point.
  [[nodiscard]] Result<std::optional<GuiSketchCreatePick>>
  apply_numeric(std::string_view text, std::optional<Point2> hover) const;
  [[nodiscard]] Result<std::size_t> set_polygon_sides(std::size_t sides);

  [[nodiscard]] GuiSketchCreatePreview
  preview(std::optional<GuiSketchCreatePick> hover) const;

  [[nodiscard]] Result<GuiSketchCreateExpansion>
  expansion(const PartDocument& document) const;
  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session,
                                           const GuiSketchWorkbench& workbench) const;

private:
  GuiSketchCreateController(GuiSketchCreateTool tool, SketchId sketch, GuiSketchPlaneView plane,
                            GuiSketchCreateOptions options);

  [[nodiscard]] Result<std::vector<Point2>> corner_ring(std::optional<Point2> hover) const;

  GuiSketchCreateTool tool_;
  SketchId sketch_;
  GuiSketchPlaneView plane_;
  GuiSketchCreateOptions options_;
  std::vector<GuiSketchCreatePick> picks_;
};

} // namespace blcad::gui
