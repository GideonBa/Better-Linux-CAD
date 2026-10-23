#pragma once

#include "blcad/gui/gui_analysis_export_workbench.hpp"

#include <QObject>

class QAction;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QTableWidget;
class QTableWidgetItem;

namespace blcad::gui {
class ShellRibbon;
class ShellWindow;

// Explicitly owned consumers of fresh geometry. Neither analysis nor export
// changes document intent, history, or authored assembly poses.
class ShellInspectTools final : public QObject {
public:
  ShellInspectTools(ShellWindow&, ShellRibbon&, int model_tab, int assembly_tab);
  [[nodiscard]] Result<std::size_t> begin_analysis();
  [[nodiscard]] Result<std::size_t> run_analysis();
  [[nodiscard]] Result<GuiStepExportResult> export_step(const GuiStepExportRequest&);
  void cancel();
  void refresh();
  [[nodiscard]] bool active() const noexcept {
    return active_;
  }

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  [[nodiscard]] bool idle_and_fresh() const;
  void clear_result();
  void update_inputs();
  void choose_export(GuiStepExportMode);
  void add_row(QTableWidgetItem* first, QTableWidgetItem* second, QString result);
  ShellWindow& window_;
  GuiAnalysisExportWorkbench workbench_;
  QDockWidget* dock_{};
  QComboBox* kind_{};
  QDoubleSpinBox* clearance_{};
  QListWidget* group_{};
  QLabel* summary_{};
  QTableWidget* results_{};
  QAction* analysis_action_{};
  std::vector<std::pair<GuiStepExportMode, QAction*>> export_actions_;
  std::size_t revision_{};
  bool active_{false};
};
} // namespace blcad::gui
