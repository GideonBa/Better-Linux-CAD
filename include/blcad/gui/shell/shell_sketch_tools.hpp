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

// Ändern-Gruppe (Block 116 wiring): the pick-driven modal modify tools. Fillet
// and Chamfer are selection-first with a numeric prompt and have no modal state.
enum class SketchModifyKind { Trim, Extend, Split };

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
  // Ändern-Gruppe (Block 116). Trim/Extend/Split operate on a clicked curve at
  // the pick point; Fillet/Chamfer operate on exactly two selected curves with
  // a radius/distance. Each previews once and commits one atomic transaction.
  [[nodiscard]] Result<std::size_t>
  apply_modify_pick(SketchModifyKind kind, const std::string& entity_id, Point2 pick);
  [[nodiscard]] Result<std::size_t> apply_fillet(double radius);
  [[nodiscard]] Result<std::size_t> apply_chamfer(double distance);
  // Arms a modal Trim/Extend/Split tool; the next curve click runs the op.
  void begin_modify_tool(SketchModifyKind kind);
  [[nodiscard]] bool modify_tool_active() const { return modify_tool_.has_value(); }
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
  // Raw SketchEntityIds of the selected curves (strips the sketch/entity
  // prefix) for the modify service, which addresses core entity ids directly.
  [[nodiscard]] std::vector<SketchEntityId> selected_sketch_entity_ids() const;
  void handle_modify_press(SketchModifyKind kind, const std::optional<GuiSketchHit>& hit);
  // Authors one catalog constraint (preview → accept → commit); returns false
  // and stays silent when the preview is not accepted (e.g. already implied).
  [[nodiscard]] bool author_catalog_constraint(SketchSolverConstraintKind kind,
                                               std::vector<SketchConstraintIntentTarget> targets);
  // Removes catalog Coincident constraints whose two points are no longer at the
  // same place (or have vanished) — e.g. the old chained-corner coincidence that
  // a fillet/chamfer just pulled apart. Left in place it would collapse the new
  // corner on the next solve/drag. No geometry re-solve happens here.
  void prune_broken_coincidences();
  // After a fillet/chamfer commit, pins the inserted curve to the two trimmed
  // lines with Coincident (both corners) and, for fillets, Tangent — so the
  // corner survives dragging. Silently skips constraints the solver rejects.
  void auto_constrain_corner(const std::vector<std::string>& before_entity_ids,
                             SketchEntityId first_line, SketchEntityId second_line,
                             bool add_tangent);
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
  QAction* fillet_action_{nullptr};
  QAction* chamfer_action_{nullptr};
  std::vector<QAction*> constraint_actions_;
  std::vector<QAction*> dimension_actions_;
  std::vector<QAction*> tool_actions_;
  std::vector<QAction*> modify_actions_;

  std::optional<GuiSketchCreateController> create_controller_;
  std::optional<GuiSketchDragController> drag_controller_;
  // Armed Trim/Extend/Split modal tool; the next curve press runs the op.
  std::optional<SketchModifyKind> modify_tool_;
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
