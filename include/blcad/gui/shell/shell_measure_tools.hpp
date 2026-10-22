#pragma once

#include "blcad/geometry/measure_selection.hpp"
#include "blcad/gui/gui_measure.hpp"

#include <QObject>
#include <limits>

class QAction;
class QComboBox;
class QDockWidget;
class QFormLayout;
class QLabel;
class QPushButton;
class QWidget;

namespace blcad::gui {
class OcctViewport;
class ShellRibbon;
class ShellWindow;

// Explicit shell-owned, read-only Measure presentation. Catalogue identities
// live for one derived-geometry generation and never become document intent.
class ShellMeasureTools final : public QObject {
public:
  ShellMeasureTools(ShellWindow& window, GuiDocumentSession& session, OcctViewport& viewport,
                    ShellRibbon& ribbon, int model_tab);
  ~ShellMeasureTools() override;
  [[nodiscard]] Result<std::size_t> begin(GuiMeasureKind kind);
  void cancel();
  void refresh();
  void refresh_presentation();
  [[nodiscard]] bool active() const noexcept {
    return controller_.active();
  }
  [[nodiscard]] bool accept_selection(const GuiSelection& selection);
  [[nodiscard]] std::vector<geometry::ViewportSceneItem> selection_items() const;
  [[nodiscard]] const std::vector<geometry::MeasureSelectionTarget>& targets() const noexcept {
    return targets_;
  }
  [[nodiscard]] const std::optional<GuiMeasureResult>& result() const noexcept {
    return result_;
  }
  [[nodiscard]] QComboBox& kind_editor() noexcept {
    return *kind_;
  }
  [[nodiscard]] QComboBox& pick_editor() noexcept {
    return *pick_;
  }
  [[nodiscard]] QComboBox& filter_editor() noexcept {
    return *filter_;
  }
  [[nodiscard]] QComboBox& target_editor(std::size_t role) noexcept {
    return role == 0 ? *first_ : *second_;
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  [[nodiscard]] Result<std::size_t> rebuild_catalogue();
  void populate_inputs();
  void update_pick_scope();
  void update_result();
  void clear_inputs();
  void finish(bool refresh_window);
  [[nodiscard]] bool accepts(geometry::MeasureSelectionKind kind) const;
  [[nodiscard]] const geometry::MeasureSelectionTarget* selected(QComboBox* combo) const;

  ShellWindow& window_;
  GuiDocumentSession& session_;
  OcctViewport& viewport_;
  GuiMeasureController controller_;
  QWidget* overlay_{nullptr};
  QDockWidget* dock_{nullptr};
  QFormLayout* form_{nullptr};
  QComboBox* kind_{nullptr};
  QComboBox* pick_{nullptr};
  QComboBox* filter_{nullptr};
  QComboBox* first_{nullptr};
  QComboBox* second_{nullptr};
  QLabel* readout_{nullptr};
  QLabel* hint_{nullptr};
  QPushButton* copy_{nullptr};
  std::vector<std::pair<GuiMeasureKind, QAction*>> actions_;
  std::vector<geometry::MeasureSelectionTarget> targets_;
  std::optional<GuiMeasureResult> result_;
  std::uint32_t previous_mask_{0};
  std::optional<std::vector<std::string>> previous_scope_;
  std::size_t revision_{std::numeric_limits<std::size_t>::max()};
  std::size_t generation_{0};
  bool fresh_{false};
};
} // namespace blcad::gui
