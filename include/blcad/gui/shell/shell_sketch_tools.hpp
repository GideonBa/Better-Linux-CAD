#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_create.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QObject>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class QAction;

namespace blcad::gui {

class ShellRibbon;
class ShellWindow;

// GUI Shell Reset MVP-9R Block 134: the sketch tool surface of the new shell.
// Replaces the five deleted sketch binders as ONE shell-owned component with
// explicit references (window, session, workbench, workspace, viewport,
// ribbon) — no findChild discovery, no self-installation, no action rewiring.
// All headless controllers (create, drag, constraint, dimension, interaction
// scene builder) are consumed unchanged. Every ribbon slot funnels through the
// public dialog-free command surface below, which doubles as the test surface.
class ShellSketchTools final : public QObject {
public:
  ShellSketchTools(ShellWindow& window, GuiDocumentSession& session,
                   GuiSketchWorkbench& workbench, GuiSketchWorkspace& workspace,
                   OcctViewport& viewport, ShellRibbon& ribbon, int sketch_tab);

  // Lifecycle, driven by ShellWindow.
  void activate();   // publish the real interaction scene + arm the drag layer
  void deactivate(); // tear down tools, preview, handles
  void publish_scene();
  void refresh_enablement();

  // Dialog-free command surface (ribbon slots and tests call these).
  [[nodiscard]] bool begin_tool(GuiSketchCreateTool tool);
  [[nodiscard]] bool add_pick(Point2 point,
                              GuiSketchSnapKind kind = GuiSketchSnapKind::None);
  [[nodiscard]] bool complete_tool();
  void cancel_tool();
  [[nodiscard]] bool tool_active() const;
  [[nodiscard]] Result<std::size_t> apply_constraint(SketchSolverConstraintKind kind);
  [[nodiscard]] Result<std::size_t> remove_selected_constraint();
  [[nodiscard]] Result<std::size_t>
  apply_dimension(SketchDimensionKind kind, bool reference,
                  std::optional<ParameterId> parameter = std::nullopt);
  [[nodiscard]] Result<std::size_t> edit_selected_dimension(const std::string& text);
  // Deletes whatever is selected: a constraint glyph, a dimension, or a sketch
  // entity (in that priority). Fails closed when nothing deletable is selected.
  [[nodiscard]] Result<std::size_t> delete_selection();
  [[nodiscard]] bool edit_selected_dimension_interactive(); // prompt + apply
  [[nodiscard]] const std::optional<GuiSketchDragController>& drag_controller() const noexcept {
    return drag_controller_;
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void build_ribbon_groups();
  void wire_viewport_callbacks();
  [[nodiscard]] GuiSketchGridConfig grid_config() const;
  void apply_grid_config();
  [[nodiscard]] bool sketch_context_ready() const;
  [[nodiscard]] Result<SketchTopology> active_topology() const;
  [[nodiscard]] Result<std::vector<SketchConstraintIntentTarget>>
  selected_targets(const SketchTopology& topology, bool resolve_point_roles) const;
  [[nodiscard]] std::optional<SketchConstraintId> selected_accepted_constraint() const;
  [[nodiscard]] std::optional<SketchDimensionId> selected_dimension() const;
  [[nodiscard]] std::optional<std::string> selected_entity() const;
  void after_pick();
  void apply_chain_coincidence();
  void update_preview();
  void apply_numeric_buffer();
  void mirror_numeric_buffer();
  void clear_numeric_buffer();
  void synchronize_sketch_selection(const std::vector<GuiSelection>& selections);
  void handle_pointer(Point2 raw_point, const GuiSketchSnapResult& snap,
                      const std::optional<GuiSketchHit>& hit);
  // Solver-backed drag (default tool when no create tool is armed).
  void arm_drag_controller();
  void begin_drag(GuiSketchScreenPoint screen);
  void release_drag(Point2 final_pointer);
  void cancel_drag(bool cancel_workspace, const std::string& message);
  void publish_drag_preview(const GuiSketchDragPreview& preview);
  void publish_handles(const std::vector<GuiSketchDragHandle>& handles);
  void publish_scene_for(const Sketch& sketch, bool include_annotations = true);
  void report(const Error& error) const;
  void report_message(const std::string& message) const;
  void show_workspace_status() const;

  ShellWindow& window_;
  GuiDocumentSession& session_;
  GuiSketchWorkbench& workbench_;
  GuiSketchWorkspace& workspace_;
  OcctViewport& viewport_;
  ShellRibbon& ribbon_;
  int sketch_tab_{-1};

  QAction* grid_action_{nullptr};
  QAction* grid_snap_action_{nullptr};
  QAction* reference_dimension_action_{nullptr};
  QAction* delete_constraint_action_{nullptr};
  QAction* edit_dimension_action_{nullptr};
  std::vector<QAction*> constraint_actions_;
  std::vector<QAction*> dimension_actions_;
  std::vector<QAction*> tool_actions_;

  std::optional<GuiSketchCreateController> create_controller_;
  std::optional<GuiSketchDragController> drag_controller_;
  // Transient (session-local) dimension label placement: dimension id → plane
  // offset from the resolver's default anchor; dragged with the mouse.
  std::unordered_map<std::string, Point2> dimension_label_offsets_;
  std::optional<std::string> label_drag_;
  Point2 label_drag_last_{};
  bool label_publish_scheduled_{false};
  // Inventor-style line chaining: end of the last committed segment; the next
  // segment starts here and the shared corner is coupled coincident.
  std::optional<Point2> chain_anchor_;
  std::unordered_map<std::string, std::string> candidate_by_semantic_;
  std::string numeric_buffer_;
  bool preview_scheduled_{false};
  bool solve_scheduled_{false};
  bool synchronizing_selection_{false};
};

} // namespace blcad::gui
