#pragma once

#include "blcad/gui/gui_command_registry.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve_binder.hpp"
#include "blcad/gui/gui_modeling_workspace_binder.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_part_operations_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/gui_spatial_surface_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator_binder.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QKeyEvent;
class QLineEdit;
class QTabBar;
class QTextEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace blcad::gui {

class MainWindow final : public QMainWindow {
public:
  explicit MainWindow(QWidget* parent = nullptr);

  [[nodiscard]] GuiDocumentSession& session() noexcept;
  [[nodiscard]] const GuiDocumentSession& session() const noexcept;
  [[nodiscard]] GuiCommandRegistry& command_registry() noexcept;
  [[nodiscard]] const GuiCommandRegistry& command_registry() const noexcept;
  [[nodiscard]] GuiSketchWorkbench& sketch_workbench() noexcept;
  [[nodiscard]] GuiSketchWorkspace& sketch_workspace() noexcept;
  [[nodiscard]] const GuiSketchWorkspace& sketch_workspace() const noexcept;
  [[nodiscard]] GuiModelingWorkspace& modeling_workspace() noexcept {
    modeling_workspace_shell_.ensure_installed();
    modeling_workspace_shell_.refresh();
    return modeling_workspace_;
  }
  [[nodiscard]] const GuiModelingWorkspace& modeling_workspace() const noexcept {
    modeling_workspace_shell_.ensure_installed();
    modeling_workspace_shell_.refresh();
    return modeling_workspace_;
  }
  [[nodiscard]] GuiViewportManipulatorLayer& viewport_manipulators() noexcept {
    viewport_manipulator_shell_.ensure_installed();
    viewport_manipulator_shell_.refresh();
    return viewport_manipulators_;
  }
  [[nodiscard]] const GuiViewportManipulatorLayer& viewport_manipulators() const noexcept {
    viewport_manipulator_shell_.ensure_installed();
    viewport_manipulator_shell_.refresh();
    return viewport_manipulators_;
  }
  void refresh_viewport_manipulators() {
    viewport_manipulator_shell_.refresh();
  }
  [[nodiscard]] GuiInteractiveFeatureCoordinator& interactive_features() noexcept {
    viewport_manipulator_shell_.ensure_installed();
    return interactive_features_;
  }
  [[nodiscard]] GuiPartFoundationWorkbench& part_foundation_workbench() noexcept;
  [[nodiscard]] GuiPartOperationsWorkbench& part_operations_workbench() noexcept;
  [[nodiscard]] GuiSpatialSurfaceWorkbench& spatial_surface_workbench() noexcept;
  [[nodiscard]] GuiAssemblyWorkbench& assembly_workbench() noexcept;
  [[nodiscard]] GuiAnalysisExportWorkbench& analysis_export_workbench() noexcept;
  [[nodiscard]] const std::optional<SketchId>& active_sketch() const noexcept;

  [[nodiscard]] bool request_workspace(GuiWorkspace workspace) noexcept;
  void refresh_command_state();

protected:
  void closeEvent(QCloseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  void create_shell();
  void create_menus();
  void create_command_bar();
  void create_docks();
  void create_default_commands();
  void synchronize_workspace_tab();
  void refresh_document_presentation();
  void refresh_model_browser();
  void show_node_properties(const GuiBrowserNode* node);
  void synchronize_tree_selection(std::string_view semantic_id);
  void preview_property_edit();
  void apply_property_edit();
  void cancel_property_edit();
  void create_xy_datum();
  void create_default_datum_axis();
  void create_sketch_on_selection();
  void edit_selected_sketch();
  void finish_sketch();
  void finish_sketch_with_modeling_handoff() {
    const auto sketch = active_sketch_;
    std::optional<ProfileId> profile;
    if (sketch.has_value() && session_.part_document() != nullptr) {
      const auto inspection = sketch_workbench_.inspect(session_, *sketch);
      const Sketch* source = session_.part_document()->find_sketch(*sketch);
      if (!inspection.has_error() && source != nullptr &&
          inspection.value().detected_region.has_value() &&
          GuiModelingWorkspace::has_materialized_profile(
              *source, *inspection.value().detected_region))
        profile = inspection.value().detected_region;
    }

    finish_sketch();
    if (sketch.has_value() && profile.has_value() && !active_sketch_.has_value())
      (void)modeling_workspace_.finish_sketch_handoff(session_, *sketch, *profile);
  }
  void add_numeric_sketch_line();
  void commit_numeric_sketch_line();
  void repeat_last_sketch_command();
  void inspect_selected_sketch();
  void repair_selected_sketch();
  void refresh_sketch_workspace_ui();
  void refresh_viewport_scene();
  [[nodiscard]] bool confirm_discard_dirty();
  void create_new_document(GuiDocumentKind kind);
  void open_document();
  void save_document(bool force_path);
  void show_error(const Error& error);

  GuiDocumentSession session_;
  GuiCommandRegistry command_registry_;
  GuiSketchWorkbench sketch_workbench_;
  GuiSketchWorkspace sketch_workspace_;
  GuiModelingWorkspace modeling_workspace_;
  GuiViewportManipulatorLayer viewport_manipulators_;
  GuiInteractiveFeatureCoordinator interactive_features_{&session_, &viewport_manipulators_};
  GuiPartFoundationWorkbench part_foundation_workbench_;
  GuiPartOperationsWorkbench part_operations_workbench_;
  GuiSpatialSurfaceWorkbench spatial_surface_workbench_;
  GuiAssemblyWorkbench assembly_workbench_;
  GuiAnalysisExportWorkbench analysis_export_workbench_;
  mutable GuiModelingWorkspaceShellBinder modeling_workspace_shell_{
      this, &session_, &modeling_workspace_,
      [this] { finish_sketch_with_modeling_handoff(); },
      [this] { refresh_command_state(); }};
  mutable GuiViewportManipulatorShellBinder viewport_manipulator_shell_{
      this, &viewport_manipulators_};
  std::optional<SketchId> active_sketch_;
  std::optional<GuiViewportCameraBookmark> sketch_camera_bookmark_;
  std::uint32_t sketch_selection_filter_mask_{0xFFFFFFFFU};
  QTabBar* workspace_tabs_{nullptr};
  QWidget* sketch_command_groups_{nullptr};
  QAction* sketch_command_groups_action_{nullptr};
  QLineEdit* sketch_numeric_hud_{nullptr};
  QAction* sketch_numeric_hud_action_{nullptr};
  QLabel* sketch_cursor_status_{nullptr};
  QLabel* sketch_snap_status_{nullptr};
  QLabel* sketch_dof_status_{nullptr};
  QLabel* sketch_solve_status_{nullptr};
  QTreeWidget* model_browser_{nullptr};
  QTableWidget* property_table_{nullptr};
  QLabel* property_validation_{nullptr};
  QPushButton* preview_button_{nullptr};
  QPushButton* apply_button_{nullptr};
  QPushButton* cancel_button_{nullptr};
  QTextEdit* diagnostics_{nullptr};
  OcctViewport* viewport_{nullptr};
  QAction* new_part_action_{nullptr};
  QAction* new_project_action_{nullptr};
  QAction* open_action_{nullptr};
  QAction* save_action_{nullptr};
  QAction* save_as_action_{nullptr};
  QAction* undo_action_{nullptr};
  QAction* redo_action_{nullptr};
  QAction* recompute_action_{nullptr};
  QAction* shaded_action_{nullptr};
  QAction* shaded_edges_action_{nullptr};
  QAction* wireframe_action_{nullptr};
  QAction* perspective_action_{nullptr};
  QAction* create_datum_action_{nullptr};
  QAction* create_axis_action_{nullptr};
  QAction* create_sketch_action_{nullptr};
  QAction* edit_sketch_action_{nullptr};
  QAction* finish_sketch_action_{nullptr};
  QAction* add_line_action_{nullptr};
  QAction* repeat_sketch_action_{nullptr};
  QAction* inspect_sketch_action_{nullptr};
  QAction* repair_sketch_action_{nullptr};
  std::vector<QAction*> sketch_create_actions_;
  std::size_t viewport_revision_{static_cast<std::size_t>(-1)};
  std::size_t browser_revision_{static_cast<std::size_t>(-1)};
  GuiDocumentBrowser browser_model_;
  std::string selected_browser_id_;
  bool synchronizing_selection_{false};
};

} // namespace blcad::gui
