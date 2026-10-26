#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_rapid_dimension.hpp"
#include "blcad/gui/gui_sketch_create.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_operations.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QObject>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QAction;
class QLineEdit;

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
                              GuiSketchSnapKind kind = GuiSketchSnapKind::None,
                              std::optional<SketchEntityId> tangent_reference = std::nullopt);
  [[nodiscard]] bool complete_tool();
  [[nodiscard]] bool finish_tool(); // Commit an active fitted spline; discard incomplete picks.
  void cancel_tool();
  [[nodiscard]] bool tool_active() const;
  [[nodiscard]] Result<std::size_t> apply_constraint(SketchSolverConstraintKind kind);
  [[nodiscard]] Result<std::size_t> remove_selected_constraint();
  [[nodiscard]] Result<std::size_t>
  apply_dimension(SketchDimensionKind kind, bool reference,
                  std::optional<ParameterId> parameter = std::nullopt);
  [[nodiscard]] Result<std::size_t> edit_selected_dimension(const std::string& text);
  [[nodiscard]] bool begin_rapid_dimension();
  [[nodiscard]] bool rapid_dimension_active() const noexcept {
    return rapid_dimension_active_;
  }
  [[nodiscard]] const std::optional<GuiRapidDimensionCandidate>&
  rapid_dimension_candidate() const noexcept {
    return rapid_candidate_;
  }
  // Ändern-Gruppe (Block 116). Trim/Extend/Split operate on a clicked curve at
  // the pick point; Fillet/Chamfer operate on exactly two selected curves with
  // a radius/distance. Each previews once and commits one atomic transaction.
  [[nodiscard]] Result<std::size_t>
  apply_modify_pick(SketchModifyKind kind, const std::string& entity_id, Point2 pick);
  [[nodiscard]] Result<std::size_t>
  apply_modify_profile_pick(SketchModifyKind kind, const ProfileId& profile_id, Point2 pick);
  [[nodiscard]] Result<std::size_t> apply_fillet(double radius);
  [[nodiscard]] Result<std::size_t> apply_chamfer(double distance);
  [[nodiscard]] Result<GuiSketchOperationPreview> preview_operation(GuiSketchOperationRequest request);
  [[nodiscard]] Result<std::size_t> apply_operation(GuiSketchOperationRequest request);
  void cancel_operation_preview();
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
  void build_operation_group();
  void show_operation_dialog(GuiSketchOperationKind kind);
  [[nodiscard]] Result<GuiSketchOperationRequest> prepare_operation(GuiSketchOperationRequest request) const;
  [[nodiscard]] Result<std::vector<SketchEntityId>> selected_operation_entities() const;
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
  void begin_trim_stroke(GuiSketchScreenPoint screen);
  void continue_trim_stroke(GuiSketchScreenPoint screen);
  void queue_trim_stroke_hits(std::vector<GuiSketchHit> hits);
  void handle_trim_stroke_hit(const std::optional<GuiSketchHit>& hit, bool report_miss);
  void end_trim_stroke() noexcept;
  void cancel_trim_stroke() noexcept;
  void after_pick();
  void apply_chain_coincidence();
  void update_preview();
  void apply_numeric_buffer();
  void mirror_numeric_buffer();
  void clear_numeric_buffer();
  void cancel_label_drag();
  [[nodiscard]] Result<std::size_t> edit_dimension(SketchDimensionId id,
                                                 const std::string& text);
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
  [[nodiscard]] bool drag_handle_visible(const GuiSketchDragHandle& handle) const;
  void publish_scene_for(const Sketch& sketch, bool include_annotations = true);
  void report(const Error& error) const;
  void report_message(const std::string& message) const;
  void show_workspace_status() const;
  [[nodiscard]] std::optional<SketchConstraintIntentTarget>
  rapid_target(const SketchTopology& topology, const std::optional<GuiSketchHit>& hit,
               Point2 pointer) const;
  void update_rapid_dimension(Point2 pointer, const std::optional<GuiSketchHit>& hit);
  void rapid_dimension_press(Point2 pointer, const std::optional<GuiSketchHit>& hit);
  [[nodiscard]] Result<std::size_t>
  commit_dimension(SketchDimensionKind kind, bool reference, std::optional<ParameterId> parameter,
                   std::vector<SketchConstraintIntentTarget> targets,
                   std::optional<Point2> label_position = std::nullopt);

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
  QAction* rapid_dimension_action_{nullptr};
  bool rapid_dimension_active_{false};
  bool rapid_alternate_radial_{false};
  std::vector<SketchConstraintIntentTarget> rapid_targets_;
  std::optional<GuiRapidDimensionCandidate> rapid_candidate_;
  QAction* fillet_action_{nullptr};
  QAction* chamfer_action_{nullptr};
  std::vector<QAction*> constraint_actions_;
  std::vector<QAction*> dimension_actions_;
  std::vector<QAction*> tool_actions_;
  std::vector<QAction*> modify_actions_;
  std::vector<QAction*> operation_actions_;
  QAction* project_action_{nullptr};
  QAction* break_link_action_{nullptr};
  bool operation_preview_active_{false};
  std::function<void(GuiSketchPointerPhase, Point2, const std::optional<GuiSketchHit>&)>
      operation_phase_;
  std::function<void(Point2)> operation_pointer_;
  std::function<void()> operation_selection_;
  std::function<void()> close_operation_;

  std::optional<GuiSketchCreateController> create_controller_;
  bool suppress_tool_right_release_{false};
  std::optional<GuiSketchDragController> drag_controller_;
  // Armed Trim/Extend/Split modal tool; the next curve press runs the op.
  std::optional<SketchModifyKind> modify_tool_;
  // Inventor-style Trim painting. The screen guard prevents the pointer update
  // preceding Release from trimming a newly exposed, overlapping curve at the
  // same position after publish_scene rebuilt the interaction scene.
  bool trim_stroke_active_{false};
  std::optional<GuiSketchScreenPoint> trim_stroke_last_screen_;
  std::unordered_set<std::string> trim_stroke_visited_;
  std::size_t trim_stroke_generation_{0U};
  std::size_t trim_stroke_pending_{0U};
  // Only the in-flight delta is transient. Release persists one plane-space
  // offset in the dimension catalog, together with dirty state and history.
  std::optional<std::string> label_drag_;
  Point2 label_drag_delta_{};
  Point2 label_drag_initial_offset_{};
  Point2 label_drag_last_{};
  bool label_publish_scheduled_{false};
  QLineEdit* dimension_editor_{nullptr};
  std::optional<SketchDimensionId> dimension_editor_id_;
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
