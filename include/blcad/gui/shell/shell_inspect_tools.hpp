#pragma once

#include "blcad/gui/gui_analysis_export_workbench.hpp"

#include <QObject>

class QAction;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QSpinBox;
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
  [[nodiscard]] bool source_unchanged();
  void clear_result();
  void update_inputs();
  void update_sweep_joint();
  void highlight_result(QTableWidgetItem*);
  void choose_export(GuiStepExportMode);
  void add_row(QTableWidgetItem* first, QTableWidgetItem* second, QString result);
  ShellWindow& window_;
  GuiAnalysisExportWorkbench workbench_;
  QDockWidget* dock_{};
  QComboBox* kind_{};
  QDoubleSpinBox* clearance_{};
  QFormLayout* form_{};
  QComboBox* sweep_joint_{};
  QDoubleSpinBox* sweep_start_{};
  QDoubleSpinBox* sweep_end_{};
  QSpinBox* sweep_samples_{};
  QLabel* sweep_hint_{};
  QListWidget* group_{};
  QLabel* summary_{};
  QTableWidget* results_{};
  QAction* analysis_action_{};
  std::vector<std::pair<GuiStepExportMode, QAction*>> export_actions_;
  std::vector<geometry::AssemblyRevoluteSweepJointReference> sweep_joints_;
  std::uint32_t previous_mask_{0xFFFFFFFFU};
  std::optional<std::vector<std::string>> previous_scope_;
  std::optional<GuiSelection> previous_selection_;
  std::string highlighted_occurrence_;
  std::string source_json_;
  std::size_t revision_{};
  bool active_{false};
};
} // namespace blcad::gui
