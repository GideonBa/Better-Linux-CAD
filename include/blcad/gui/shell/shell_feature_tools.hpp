#pragma once

#include "blcad/gui/gui_interactive_feature_coordinator.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <QObject>

#include <optional>
#include <utility>

class QAction;
class QDockWidget;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QString;
class QWidget;

namespace blcad::gui {

class OcctViewport;
class ShellRibbon;
class ShellWindow;

// GUI Shell Reset MVP-9R Block 135: explicit, shell-owned integration of the
// existing interactive Extrude/Revolve controllers and candidate-only
// manipulator layer. It owns the overlay and task panel, receives references in
// its constructor, and synchronizes only in response to commands/input/camera
// notifications (no discovery, self-install, foreign action rewiring, or
// polling timer).
class ShellFeatureTools final : public QObject {
public:
  enum class Command { None, Extrude, Revolve };

  ShellFeatureTools(ShellWindow& window, GuiDocumentSession& session,
                    const GuiSketchWorkbench& sketch_workbench, OcctViewport& viewport,
                    ShellRibbon& ribbon, int model_tab);
  ~ShellFeatureTools() override;

  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] Command command() const noexcept {
    return command_;
  }
  [[nodiscard]] GuiViewportManipulatorLayer& manipulators() noexcept {
    return layer_;
  }
  [[nodiscard]] const GuiViewportManipulatorLayer& manipulators() const noexcept {
    return layer_;
  }
  [[nodiscard]] GuiInteractiveFeatureCoordinator& coordinator() noexcept {
    return coordinator_;
  }
  [[nodiscard]] const std::optional<std::pair<SketchId, ProfileId>>&
  selected_profile() const noexcept {
    return selected_profile_;
  }
  [[nodiscard]] QAction& extrude_action() noexcept {
    return *action_extrude_;
  }
  [[nodiscard]] QAction& revolve_action() noexcept {
    return *action_revolve_;
  }
  [[nodiscard]] QLineEdit& numeric_editor() noexcept {
    return *numeric_editor_;
  }

  // Selection-first dialog-free command surface (also used by tests).
  [[nodiscard]] bool select_profile(SketchId sketch, ProfileId profile);
  void clear_profile_selection() noexcept;
  [[nodiscard]] Result<std::size_t> begin_extrude();
  [[nodiscard]] Result<std::size_t> begin_revolve(std::optional<DatumAxisId> axis = std::nullopt);
  [[nodiscard]] Result<GuiPartFeaturePreview> set_numeric_text(const QString& text);
  [[nodiscard]] Result<GuiPartFeaturePreview> set_numeric_value(double value);
  [[nodiscard]] Result<GuiPartFeaturePreview>
  accept_manipulator_candidate(const GuiViewportManipulatorCandidate& candidate);
  [[nodiscard]] Result<std::size_t> apply();
  void cancel() noexcept;
  void reset() noexcept;

  // ShellWindow calls these after selection/document/camera changes.
  void refresh_enablement();
  void refresh_presentation();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  [[nodiscard]] Result<std::pair<SketchId, ProfileId>> profile_from_selection() const;
  [[nodiscard]] Result<GuiSketchPlaneView> selected_profile_plane() const;
  [[nodiscard]] Result<DatumAxisId> default_revolve_axis(const GuiSketchPlaneView& plane) const;
  [[nodiscard]] Result<GuiPartFeaturePreview> preview_and_publish();
  [[nodiscard]] double current_value() const noexcept;
  [[nodiscard]] bool update_mapping();
  void synchronize_numeric_editor(double value);
  void synchronize_overlay();
  void restore_drag_start_value();
  void handle_escape();
  void show_task_panel(QString title, QString unit, double value);
  void hide_task_panel() noexcept;
  void report(const Error& error);

  ShellWindow& window_;
  GuiDocumentSession& session_;
  const GuiSketchWorkbench& sketch_workbench_;
  OcctViewport& viewport_;
  GuiViewportManipulatorLayer layer_;
  GuiInteractiveFeatureCoordinator coordinator_;

  QWidget* overlay_{nullptr};
  QDockWidget* task_dock_{nullptr};
  QLabel* task_summary_{nullptr};
  QLineEdit* numeric_editor_{nullptr};
  QPushButton* apply_button_{nullptr};
  QPushButton* cancel_button_{nullptr};
  QAction* action_extrude_{nullptr};
  QAction* action_revolve_{nullptr};
  QAction* action_escape_{nullptr};

  std::optional<std::pair<SketchId, ProfileId>> selected_profile_;
  std::optional<double> drag_start_value_;
  Command command_{Command::None};
  // True only while the currently authored controller state has produced and
  // published a real geometry preview.  Keeping this separate from `active()`
  // prevents a generic shell refresh from re-enabling Apply after preview
  // generation failed.
  bool preview_valid_{false};
  bool synchronizing_numeric_{false};
};

} // namespace blcad::gui
