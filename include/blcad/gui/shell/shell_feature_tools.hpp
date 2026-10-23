#pragma once

#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_feature_coordinator.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <QObject>

#include <optional>
#include <utility>

class QAction;
class QComboBox;
class QListWidget;
class QDockWidget;
class QEvent;
class QLabel;
class QFormLayout;
class QLineEdit;
class QPushButton;
class QString;
class QWidget;

namespace blcad::gui {

class OcctViewport;
class ShellRibbon;
class ShellWindow;

// GUI Shell Reset MVP-9R Blocks 135/136: explicit, shell-owned integration of
// Extrude/Revolve, finishing and shell/draft controllers with the candidate-only
// manipulator layer. It owns the overlay and task panel, receives references in
// its constructor, and synchronizes only in response to commands/input/camera
// notifications (no discovery, self-install, foreign action rewiring, or
// polling timer).
class ShellFeatureTools final : public QObject {
public:
  enum class Command { None, Extrude, Revolve, Fillet, Chamfer, Shell, Draft };

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
  [[nodiscard]] Result<std::size_t> begin_finishing(Command command);
  [[nodiscard]] bool accept_finishing_selection(const GuiSelection& selection);
  [[nodiscard]] std::vector<geometry::ViewportSceneItem> finishing_selection_items() const;
  [[nodiscard]] QComboBox& finishing_body_editor() noexcept {
    return *finishing_body_;
  }
  [[nodiscard]] QListWidget& finishing_target_editor() noexcept {
    return *finishing_targets_list_;
  }
  [[nodiscard]] QComboBox& finishing_mode_editor() noexcept {
    return *finishing_mode_;
  }
  [[nodiscard]] QLineEdit& secondary_numeric_editor() noexcept {
    return *secondary_numeric_editor_;
  }
  [[nodiscard]] Result<GuiPartFeaturePreview> set_secondary_numeric_text(const QString& text);
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
  [[nodiscard]] bool finishing() const noexcept;
  [[nodiscard]] std::string_view command_id() const noexcept;
  [[nodiscard]] bool angular_value() const noexcept;
  [[nodiscard]] QString finishing_summary() const;
  void build_finishing_controls(QWidget* panel, QFormLayout& form);
  void reload_finishing_targets();
  [[nodiscard]] Result<std::size_t> rebuild_finishing_controller();
  void refresh_finishing_preview();
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
  QLabel* value_label_{nullptr};
  QLineEdit* numeric_editor_{nullptr};
  QPushButton* apply_button_{nullptr};
  QPushButton* cancel_button_{nullptr};
  QAction* action_extrude_{nullptr};
  QAction* action_revolve_{nullptr};
  QAction* action_escape_{nullptr};
  std::vector<std::pair<Command, QAction*>> finishing_actions_;
  QWidget* finishing_controls_{nullptr};
  QComboBox* finishing_body_{nullptr};
  QListWidget* finishing_targets_list_{nullptr};
  QComboBox* finishing_mode_{nullptr};
  QLineEdit* secondary_numeric_editor_{nullptr};
  QComboBox* draft_axis_{nullptr};
  QComboBox* draft_plane_{nullptr};
  FeatureId finishing_feature_{""};
  std::string finishing_name_;
  ParameterId finishing_parameter_{""};
  ParameterId finishing_second_parameter_{""};
  std::vector<geometry::FinishingSelectionTarget> finishing_targets_;
  std::uint32_t previous_selection_mask_{0};
  double finishing_first_value_{1};
  double finishing_second_value_{1};

  std::optional<std::pair<SketchId, ProfileId>> selected_profile_;
  std::optional<double> drag_start_value_;
  std::string drag_start_handle_;
  Command command_{Command::None};
  // True only while the currently authored controller state has produced and
  // published a real geometry preview.  Keeping this separate from `active()`
  // prevents a generic shell refresh from re-enabling Apply after preview
  // generation failed.
  bool preview_valid_{false};
  bool synchronizing_numeric_{false};
};

} // namespace blcad::gui
