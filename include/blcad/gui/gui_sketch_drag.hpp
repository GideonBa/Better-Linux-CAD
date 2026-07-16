#pragma once

#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

enum class GuiSketchDragHandleKind {
  Endpoint,
  Midpoint,
  Center,
  Radius,
  Arc,
  Spline,
  Dimension,
};

[[nodiscard]] std::string_view to_string(GuiSketchDragHandleKind kind) noexcept;

enum class GuiSketchDragTargetKind {
  Point,
  LineMidpoint,
  ArcCenter,
  ArcRadius,
};

[[nodiscard]] std::string_view to_string(GuiSketchDragTargetKind kind) noexcept;

struct GuiSketchDragHandle {
  std::string id;
  GuiSketchDragHandleKind kind{GuiSketchDragHandleKind::Endpoint};
  GuiSketchDragTargetKind target_kind{GuiSketchDragTargetKind::Point};
  Point2 position;
  std::optional<SketchPointId> point_id;
  std::string entity_id;
  std::optional<SketchDimensionId> dimension_id;
  bool reference{false};

  friend bool operator==(const GuiSketchDragHandle&, const GuiSketchDragHandle&) = default;
};

class GuiSketchDragPreview {
public:
  GuiSketchDragPreview(Point2 pointer, SketchTopology topology, Sketch preview_sketch,
                       SketchSolveResult solve);

  [[nodiscard]] Point2 pointer() const noexcept;
  [[nodiscard]] const SketchTopology& topology() const noexcept;
  [[nodiscard]] const Sketch& preview_sketch() const noexcept;
  [[nodiscard]] const SketchSolveResult& solve() const noexcept;

private:
  Point2 pointer_;
  SketchTopology topology_;
  Sketch preview_sketch_;
  SketchSolveResult solve_;
};

class GuiSketchDragController {
public:
  [[nodiscard]] static Result<GuiSketchDragController>
  create(const PartDocument& document, SketchId sketch_id);
  [[nodiscard]] static Result<GuiSketchDragController>
  create(const GuiDocumentSession& session, SketchId sketch_id);

  [[nodiscard]] const SketchId& sketch_id() const noexcept;
  [[nodiscard]] const Sketch& source_sketch() const noexcept;
  [[nodiscard]] const SketchTopology& source_topology() const noexcept;
  [[nodiscard]] const SketchConstraintSystem& source_system() const noexcept;
  [[nodiscard]] const SketchSolveResult& baseline_solve() const noexcept;
  [[nodiscard]] const std::vector<GuiSketchDragHandle>& handles() const noexcept;
  [[nodiscard]] std::vector<GuiSketchDragHandle>
  handles_for_topology(const SketchTopology& topology) const;

  [[nodiscard]] Result<std::size_t> begin(std::string_view handle_id);
  [[nodiscard]] Result<std::size_t> queue_pointer(Point2 pointer);
  [[nodiscard]] Result<GuiSketchDragPreview> process_pending();
  [[nodiscard]] Result<GuiSketchDragPreview> flush(Point2 final_pointer);
  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session);
  void cancel() noexcept;

  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool has_pending() const noexcept;
  [[nodiscard]] const GuiSketchDragHandle* active_handle() const noexcept;
  [[nodiscard]] const std::optional<GuiSketchDragPreview>& latest_preview() const noexcept;
  [[nodiscard]] const std::optional<Point2>& pending_pointer() const noexcept;
  [[nodiscard]] const std::optional<Point2>& processed_pointer() const noexcept;
  [[nodiscard]] std::size_t solve_count() const noexcept;

private:
  GuiSketchDragController(Sketch source_sketch, SketchTopology source_topology,
                          SketchConstraintSystem source_system,
                          SketchSolveResult baseline_solve,
                          std::vector<GuiSketchDragHandle> handles,
                          std::optional<SketchConstraintCatalog> source_catalog = std::nullopt);

  [[nodiscard]] Result<GuiSketchDragPreview> solve_pointer(Point2 pointer);

  Sketch source_sketch_;
  SketchTopology source_topology_;
  SketchConstraintSystem source_system_;
  SketchSolveResult baseline_solve_;
  std::vector<GuiSketchDragHandle> handles_;
  std::optional<SketchConstraintCatalog> source_catalog_;
  std::optional<std::size_t> active_handle_index_;
  std::optional<Point2> pending_pointer_;
  std::optional<Point2> processed_pointer_;
  std::optional<GuiSketchDragPreview> latest_preview_;
  std::size_t solve_count_{0U};
};

} // namespace blcad::gui
