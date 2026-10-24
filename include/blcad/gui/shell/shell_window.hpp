#pragma once

#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QMainWindow>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

class QAction;
class QCloseEvent;
class QPlainTextEdit;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;

namespace blcad::gui {

class ShellRibbon;
class ShellNavigationTools;
class ShellFeatureTools;
class ShellSketchTools;
class ShellMeasureTools;
class ShellInspectTools;
class ShellAssemblyTools;
class ShellFeatureEditTools;
class ShellConstructionTools;
class ShellReferenceRepairTools;
class ShellSketchDiagnosticsTools;
class ShellSketch3DTools;
class ShellParameterTools;
class ShellHoleTools;

// GUI Shell Reset MVP-9R Block 133: the single Qt shell window (replaces the
// deleted MainWindow-and-binders architecture). ShellWindow owns every
// component and wires them by explicit reference. Forbidden here by contract
// (docs/gui-shell-reset-sequence-mvp9r.md): findChild discovery across
// components, self-installing components, rewiring foreign QActions, and
// polling timers.
class ShellWindow final : public QMainWindow {
public:
  ShellWindow();
  ~ShellWindow() override;

  [[nodiscard]] GuiDocumentSession& session() noexcept { return session_; }
  [[nodiscard]] const GuiDocumentSession& session() const noexcept { return session_; }
  [[nodiscard]] OcctViewport& viewport() noexcept { return *viewport_; }
  [[nodiscard]] ShellRibbon& ribbon() noexcept { return *ribbon_; }
  [[nodiscard]] QTreeWidget& model_browser() noexcept { return *browser_tree_; }
  [[nodiscard]] QTableWidget& property_inspector() noexcept { return *properties_; }
  [[nodiscard]] const GuiDocumentBrowser& browser_model() const noexcept { return browser_model_; }
  [[nodiscard]] const std::optional<SketchId>& active_sketch() const noexcept {
    return active_sketch_;
  }
  [[nodiscard]] ShellSketchTools& sketch_tools() noexcept { return *sketch_tools_; }
  [[nodiscard]] ShellFeatureTools& feature_tools() noexcept { return *feature_tools_; }

  [[nodiscard]] ShellMeasureTools& measure_tools() noexcept {
    return *measure_tools_;
  }

  [[nodiscard]] ShellAssemblyTools& assembly_tools() noexcept {
    return *assembly_tools_;
  }
  [[nodiscard]] ShellFeatureEditTools& feature_edit_tools() noexcept {
    return *feature_edit_tools_;
  }

  [[nodiscard]] ShellInspectTools& inspect_tools() noexcept {
    return *inspect_tools_;
  }
  [[nodiscard]] ShellConstructionTools& construction_tools() noexcept {
    return *construction_tools_;
  }

  [[nodiscard]] ShellReferenceRepairTools& reference_repair_tools() noexcept {
    return *reference_repair_tools_;
  }

  [[nodiscard]] ShellSketchDiagnosticsTools& sketch_diagnostics_tools() noexcept {
    return *sketch_diagnostics_tools_;
  }

  [[nodiscard]] ShellSketch3DTools& sketch_3d_tools() noexcept {
    return *sketch_3d_tools_;
  }

  [[nodiscard]] ShellParameterTools& parameter_tools() noexcept {
    return *parameter_tools_;
  }
  [[nodiscard]] ShellHoleTools& hole_tools() noexcept {
    return *hole_tools_;
  }

  // Appends to the diagnostics panel and the status bar (shell components
  // report through here).
  void report_error(const Error& error);

  // Dialog-free command surface. The ribbon slots prompt and then call these;
  // tests call them directly.
  [[nodiscard]] bool new_part(const QString& name);
  [[nodiscard]] bool new_project(const QString& name);
  [[nodiscard]] bool open_path(const QString& path,
                               GuiDocumentOpenMode mode = GuiDocumentOpenMode::Normal);
  [[nodiscard]] bool save_to(const QString& path); // empty → current session path
  [[nodiscard]] bool create_sketch_on_selection();
  [[nodiscard]] bool finish_active_sketch();
  [[nodiscard]] bool undo();
  [[nodiscard]] bool redo();

  // Command enablement plus revision-gated presentation refresh (browser and
  // viewport rebuild only when session().presentation_revision() moved).
  void refresh();
  void refresh_feature_selection_scene();

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void build_ribbon();
  void build_docks();
  void wire_viewport();
  void synchronize_navigation();
  [[nodiscard]] bool confirm_document_replacement();
  void reset_document_interaction();
  void reconcile_active_sketch();
  [[nodiscard]] bool enter_sketch(SketchId sketch);
  void apply_viewport_selection(const std::optional<GuiSelection>& selection);
  void handle_tree_selection(QTreeWidgetItem* current);
  // Double-clicking a planar Sketch node re-enters it for editing.
  void handle_tree_activation(QTreeWidgetItem* item);
  void synchronize_tree_selection(const std::string& viewport_semantic_id);
  // Property inspector: mirrors the selected browser node's properties and
  // commits edits to editable ones (e.g. parameter value/expression).
  void refresh_properties();
  void handle_property_changed(QTableWidgetItem* item);
  void refresh_browser();
  void refresh_viewport_scene();
  void append_diagnostic(const Error& error);
  void show_error(const Error& error);
  [[nodiscard]] std::optional<DatumPlaneId> selected_datum_plane() const;

  GuiDocumentSession session_;
  GuiSketchWorkbench sketch_workbench_;
  GuiSketchWorkspace sketch_workspace_;
  GuiDocumentBrowser browser_model_;
  std::unique_ptr<ShellSketchTools> sketch_tools_;
  std::unique_ptr<ShellFeatureTools> feature_tools_;
  std::unique_ptr<ShellMeasureTools> measure_tools_;
  std::unique_ptr<ShellInspectTools> inspect_tools_;
  std::unique_ptr<ShellAssemblyTools> assembly_tools_;
  std::unique_ptr<ShellFeatureEditTools> feature_edit_tools_;
  std::unique_ptr<ShellConstructionTools> construction_tools_;
  std::unique_ptr<ShellReferenceRepairTools> reference_repair_tools_;
  std::unique_ptr<ShellSketchDiagnosticsTools> sketch_diagnostics_tools_;
  std::unique_ptr<ShellSketch3DTools> sketch_3d_tools_;
  std::unique_ptr<ShellParameterTools> parameter_tools_;
  std::unique_ptr<ShellHoleTools> hole_tools_;

  OcctViewport* viewport_{nullptr};
  ShellRibbon* ribbon_{nullptr};
  QTreeWidget* browser_tree_{nullptr};
  QTableWidget* properties_{nullptr};
  QPlainTextEdit* diagnostics_{nullptr};

  QAction* action_undo_{nullptr};
  QAction* action_redo_{nullptr};
  QAction* action_new_part_{nullptr};
  QAction* action_open_{nullptr};
  QAction* action_open_repair_{nullptr};
  QAction* action_save_{nullptr};
  QAction* action_save_as_{nullptr};
  QAction* action_create_sketch_{nullptr};
  QAction* action_finish_sketch_{nullptr};
  std::unique_ptr<ShellNavigationTools> navigation_tools_;
  QAction* action_perspective_{nullptr};
  QAction* action_shaded_{nullptr};
  QAction* action_shaded_edges_{nullptr};
  QAction* action_wireframe_{nullptr};

  int tab_model_{-1};
  int tab_sketch_{-1};
  int tab_view_{-1};
  int tab_assembly_{-1};

  std::optional<SketchId> active_sketch_;
  std::optional<GuiViewportCameraBookmark> sketch_camera_bookmark_;
  std::uint32_t model_selection_filter_mask_{0xFFFFFFFFU};
  std::string selected_semantic_id_;
  // Semantic id of the browser node shown in the property inspector (a node
  // may have no viewport id — e.g. parameters — so this is tracked separately).
  std::string property_semantic_id_;
  std::size_t browser_revision_{std::numeric_limits<std::size_t>::max()};
  std::size_t viewport_revision_{std::numeric_limits<std::size_t>::max()};
  bool synchronizing_selection_{false};
};

} // namespace blcad::gui
