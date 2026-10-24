#pragma once

#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_feature_coordinator.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <QObject>

#include <array>
#include <optional>
#include <utility>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QDockWidget;
class QEvent;
class QLabel;
class QFormLayout;
class QLineEdit;
class QSpinBox;
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
  enum class Command {
    None,
    Extrude,
    Revolve,
    Fillet,
    Chamfer,
    Shell,
    Draft,
    LinearPattern,
    CircularPattern,
    BodyMirror,
    BodyBoolean,
    BodyTranslate,
    BodyRotate,
    BodyScale,
    PathCurve,
    Sweep,
    Loft,
    BoundarySurface,
    FillSurface,
    TrimSurface,
    ExtendSurface,
    SurfaceStitch,
    ClosedShellToSolid
  };

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
  [[nodiscard]] Result<std::size_t> begin_pattern(Command command);
  [[nodiscard]] Result<std::size_t> begin_body_operation(Command command);
  [[nodiscard]] bool accept_body_selection(const GuiSelection& selection);
  [[nodiscard]] QListWidget& body_source_editor() noexcept {
    return *body_sources_;
  }
  [[nodiscard]] QComboBox& boolean_target_editor() noexcept {
    return *boolean_target_;
  }
  [[nodiscard]] QComboBox& boolean_operation_editor() noexcept {
    return *boolean_operation_;
  }
  [[nodiscard]] QComboBox& boolean_result_editor() noexcept {
    return *boolean_result_;
  }
  [[nodiscard]] QCheckBox& boolean_keep_tools_editor() noexcept {
    return *boolean_keep_tools_;
  }
  [[nodiscard]] QComboBox& body_axis_editor() noexcept {
    return *body_axis_;
  }
  [[nodiscard]] QPushButton& body_reference_button() noexcept {
    return *body_reference_button_;
  }
  [[nodiscard]] QCheckBox& body_owned_sketches_editor() noexcept {
    return *body_owned_sketches_;
  }
  [[nodiscard]] QCheckBox& body_owned_construction_editor() noexcept {
    return *body_owned_construction_;
  }
  [[nodiscard]] QLineEdit& translation_editor(std::size_t component) noexcept {
    return component == 0 ? *numeric_editor_ : *body_translation_editors_.at(component - 1);
  }
  [[nodiscard]] QLineEdit& scale_center_editor(std::size_t component) noexcept {
    return *body_center_editors_.at(component);
  }
  [[nodiscard]] Result<GuiPartFeaturePreview> set_translation_text(std::size_t component,
                                                                   const QString& text);
  [[nodiscard]] Result<GuiPartFeaturePreview> set_scale_center_text(std::size_t component,
                                                                    const QString& text);
  [[nodiscard]] Result<std::size_t> begin_spatial(Command command);
  [[nodiscard]] Result<std::size_t> begin_surface(Command command);
  [[nodiscard]] bool accept_surface_selection(const GuiSelection& selection);
  [[nodiscard]] Result<std::size_t> append_surface_input(int choice);
  [[nodiscard]] QComboBox& surface_source_editor() noexcept {
    return *surface_source_;
  }
  [[nodiscard]] QComboBox& surface_target_editor() noexcept {
    return *surface_target_;
  }
  [[nodiscard]] QComboBox& surface_pick_editor() noexcept {
    return *surface_pick_;
  }
  [[nodiscard]] QListWidget& surface_order_editor() noexcept {
    return *surface_order_;
  }
  [[nodiscard]] bool accept_spatial_selection(const GuiSelection& selection);
  [[nodiscard]] Result<std::size_t> append_spatial_input(int choice);
  [[nodiscard]] Result<std::size_t> move_spatial_input(int from, int to);
  [[nodiscard]] QComboBox& spatial_source_editor() noexcept {
    return *spatial_source_;
  }
  [[nodiscard]] QListWidget& spatial_order_editor() noexcept {
    return *spatial_order_;
  }
  [[nodiscard]] QComboBox& spatial_mode_editor() noexcept {
    return *spatial_mode_;
  }
  [[nodiscard]] QComboBox& spatial_target_editor() noexcept {
    return *spatial_target_;
  }
  [[nodiscard]] QComboBox& spatial_path_editor() noexcept {
    return *spatial_path_;
  }
  [[nodiscard]] QComboBox& spatial_guide_editor() noexcept {
    return *spatial_guide_;
  }
  [[nodiscard]] QListWidget& spatial_guides_editor() noexcept {
    return *spatial_guides_;
  }
  [[nodiscard]] QComboBox& spatial_pick_editor() noexcept {
    return *spatial_pick_;
  }
  [[nodiscard]] QComboBox& spatial_orientation_editor() noexcept {
    return *spatial_orientation_;
  }
  [[nodiscard]] QComboBox& spatial_continuity_editor() noexcept {
    return *spatial_continuity_;
  }
  [[nodiscard]] QCheckBox& spatial_closed_editor() noexcept {
    return *spatial_closed_;
  }
  [[nodiscard]] QCheckBox& sweep_twist_editor() noexcept {
    return *sweep_twist_;
  }
  [[nodiscard]] QPushButton& spatial_add_button() noexcept {
    return *spatial_add_;
  }
  [[nodiscard]] QPushButton& spatial_add_guide_button() noexcept {
    return *spatial_add_guide_;
  }
  [[nodiscard]] bool accept_pattern_selection(const GuiSelection& selection);
  [[nodiscard]] QListWidget& pattern_source_editor() noexcept {
    return *pattern_sources_;
  }
  [[nodiscard]] QComboBox& pattern_reference_editor() noexcept {
    return *pattern_reference_;
  }
  [[nodiscard]] QComboBox& pattern_extent_editor() noexcept {
    return *pattern_extent_;
  }
  [[nodiscard]] QComboBox& pattern_direction_editor() noexcept {
    return *pattern_direction_;
  }
  [[nodiscard]] QSpinBox& pattern_count_editor() noexcept {
    return *pattern_count_editor_;
  }
  [[nodiscard]] QPushButton& pattern_reference_button() noexcept {
    return *pattern_reference_button_;
  }
  [[nodiscard]] Result<GuiPartFeaturePreview> set_pattern_count(int value);
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
  struct RevolveAxisChoice {
    AxisReference reference;
    GuiFeatureManipulatorFrame frame;
    std::string label;
    std::string semantic_id;
  };
  [[nodiscard]] std::vector<RevolveAxisChoice>
  revolve_axis_choices(const GuiSketchPlaneView& plane) const;
  void build_revolve_controls(QWidget* panel);
  [[nodiscard]] Result<std::size_t> reset_revolve_controls(const GuiSketchPlaneView& plane,
                                                           std::optional<DatumAxisId> requested);
  void update_revolve_controls();
  void refresh_revolve_preview();
  [[nodiscard]] Result<std::size_t> configure_revolve_candidate();
  void build_extrude_controls(QWidget* panel);
  void reset_extrude_controls(GuiFeatureManipulatorFrame frame);
  void update_extrude_controls();
  void invalidate_extrude_preview();
  void refresh_extrude_preview();
  [[nodiscard]] Result<std::size_t> configure_extrude_candidate();
  [[nodiscard]] bool finishing() const noexcept;
  [[nodiscard]] bool pattern() const noexcept;
  [[nodiscard]] bool surface() const noexcept;
  [[nodiscard]] bool surface_scalar() const noexcept;
  void build_surface_controls(QWidget* panel);
  void refresh_surface_preview();
  [[nodiscard]] Result<std::size_t> rebuild_surface_controller();
  void refresh_surface_enablement(bool idle_part);
  void update_surface_pick_scope();
  struct SurfaceInput {
    std::string label;
    std::string semantic_id;
    std::optional<BoundaryCurveReference> boundary;
    std::optional<SurfaceReference> surface;
    std::optional<TrimmingReference> trimming;
  };
  [[nodiscard]] std::vector<SurfaceInput>
  surface_inputs(std::vector<geometry::FinishingSelectionTarget>* targets = nullptr) const;
  [[nodiscard]] bool spatial() const noexcept;
  [[nodiscard]] QString spatial_summary() const;
  void build_spatial_controls(QWidget* panel);
  void refresh_spatial_preview();
  [[nodiscard]] Result<std::size_t> rebuild_spatial_controller();
  void refresh_spatial_enablement(bool idle_part);
  void update_spatial_controls();
  void update_spatial_pick_scope();
  [[nodiscard]] bool spatial_scalar() const noexcept;
  struct SpatialInput {
    std::string label;
    std::string semantic_id;
    std::optional<PathSegmentReference> segment;
    std::optional<std::pair<SketchId, ProfileId>> profile;
    std::optional<PathCurveId> path;
  };
  [[nodiscard]] std::vector<SpatialInput> spatial_inputs_for(Command command) const;
  [[nodiscard]] bool body_operation() const noexcept;
  [[nodiscard]] bool body_transform() const noexcept;
  [[nodiscard]] QString body_summary() const;
  void build_body_controls(QWidget* panel);
  void refresh_body_preview();
  [[nodiscard]] Result<std::size_t> rebuild_body_controller();
  void synchronize_body_editors();
  void update_body_pick_mode();
  void refresh_body_enablement(bool idle_part);
  [[nodiscard]] std::vector<std::string> body_preview_replacements() const;
  [[nodiscard]] QString pattern_summary() const;
  void build_pattern_controls(QWidget* panel);
  void refresh_pattern_preview();
  [[nodiscard]] Result<std::size_t> rebuild_pattern_controller();
  void update_pattern_pick_mode();
  void synchronize_pattern_editors();
  [[nodiscard]] double value_for_handle(std::string_view handle) const noexcept;
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
  QWidget* extrude_controls_{nullptr};
  QComboBox* extrude_extent_{nullptr};
  QComboBox* extrude_operation_{nullptr};
  QComboBox* extrude_target_{nullptr};
  QComboBox* extrude_direction_{nullptr};
  QComboBox* extrude_path_{nullptr};
  QComboBox* extrude_first_face_{nullptr};
  QComboBox* extrude_second_face_{nullptr};
  QLineEdit* extrude_second_distance_{nullptr};
  QLineEdit* extrude_taper_{nullptr};
  std::vector<FaceReference> extrude_faces_;
  GuiFeatureManipulatorFrame extrude_frame_;
  QWidget* revolve_controls_{nullptr};
  QFormLayout* revolve_form_{nullptr};
  QComboBox* revolve_extent_{nullptr};
  QComboBox* revolve_side_{nullptr};
  QComboBox* revolve_operation_{nullptr};
  QComboBox* revolve_target_{nullptr};
  QComboBox* revolve_axis_{nullptr};
  std::vector<RevolveAxisChoice> revolve_axes_;
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

  struct PatternAxisChoice {
    AxisReference reference;
    Point3 origin;
    Vector3 direction;
    std::string semantic_id;
  };
  struct PatternPlaneChoice {
    PlaneReference reference;
    std::string semantic_id;
  };
  std::vector<PatternAxisChoice> pattern_axes_;
  std::vector<PatternPlaneChoice> pattern_planes_;
  std::vector<std::pair<Command, QAction*>> pattern_actions_;
  QWidget* pattern_controls_{nullptr};
  QListWidget* pattern_sources_{nullptr};
  QComboBox* pattern_reference_{nullptr};
  QComboBox* pattern_extent_{nullptr};
  QComboBox* pattern_direction_{nullptr};
  QSpinBox* pattern_count_editor_{nullptr};
  QPushButton* pattern_reference_button_{nullptr};
  FeatureId pattern_feature_{""};
  BodyId pattern_result_{""};
  std::string pattern_name_;
  ParameterId pattern_count_parameter_{""};
  ParameterId pattern_extent_parameter_{""};
  double pattern_value_{40};
  int pattern_count_{3};
  bool pattern_reference_picking_{false};

  struct BodyAxisChoice {
    BodyTransformRotationAxis reference;
    Point3 origin;
    Vector3 direction;
    std::string semantic_id;
  };
  std::vector<BodyAxisChoice> body_axes_;
  std::vector<std::pair<Command, QAction*>> body_actions_;
  QWidget* body_controls_{nullptr};
  QFormLayout* body_form_{nullptr};
  QListWidget* body_sources_{nullptr};
  QComboBox* boolean_target_{nullptr};
  QComboBox* boolean_operation_{nullptr};
  QComboBox* boolean_result_{nullptr};
  QCheckBox* boolean_keep_tools_{nullptr};
  QComboBox* body_axis_{nullptr};
  QPushButton* body_reference_button_{nullptr};
  QCheckBox* body_owned_sketches_{nullptr};
  QCheckBox* body_owned_construction_{nullptr};
  std::array<QLineEdit*, 2> body_translation_editors_{};
  std::array<QLineEdit*, 3> body_center_editors_{};
  FeatureId body_feature_{""};
  BodyId body_result_{""};
  std::string body_result_name_;
  std::vector<BodyTransformId> body_transform_ids_;
  Vector3 body_translation_{};
  Point3 body_scale_center_{};
  double body_value_{0};
  bool body_reference_picking_{false};

  std::vector<SpatialInput> spatial_inputs_;
  std::vector<std::pair<Command, QAction*>> spatial_actions_;
  QWidget* spatial_controls_{nullptr};
  QComboBox* spatial_source_{nullptr};
  QListWidget* spatial_order_{nullptr};
  QComboBox* spatial_mode_{nullptr};
  QComboBox* spatial_target_{nullptr};
  QComboBox* spatial_path_{nullptr};
  QComboBox* spatial_guide_{nullptr};
  QListWidget* spatial_guides_{nullptr};
  QComboBox* spatial_pick_{nullptr};
  QComboBox* spatial_orientation_{nullptr};
  QComboBox* spatial_continuity_{nullptr};
  QCheckBox* spatial_closed_{nullptr};
  QCheckBox* sweep_twist_{nullptr};
  std::array<QDoubleSpinBox*, 3> spatial_up_{};
  QPushButton* spatial_add_{nullptr};
  QPushButton* spatial_add_guide_{nullptr};
  QWidget* spatial_order_buttons_{nullptr};
  QLabel* spatial_order_hint_{nullptr};
  QPushButton* spatial_remove_guide_{nullptr};
  FeatureId spatial_feature_{""};
  PathCurveId spatial_path_id_{""};
  BodyId spatial_result_{""};
  ParameterId spatial_twist_parameter_{""};
  std::string spatial_name_;
  double spatial_twist_{0};
  std::optional<std::vector<std::string>> previous_semantic_scope_;

  std::vector<SurfaceInput> surface_inputs_;
  std::vector<geometry::FinishingSelectionTarget> surface_targets_;
  std::vector<std::pair<Command, QAction*>> surface_actions_;
  QWidget* surface_controls_{nullptr};
  QComboBox* surface_source_{nullptr};
  QComboBox* surface_target_{nullptr};
  QComboBox* surface_pick_{nullptr};
  QListWidget* surface_order_{nullptr};
  QPushButton* surface_add_{nullptr};
  QWidget* surface_order_buttons_{nullptr};
  QLabel* surface_hint_{nullptr};
  FeatureId surface_feature_{""};
  BodyId surface_result_{""};
  ParameterId surface_parameter_{""};
  std::string surface_name_;
  double surface_value_{5};

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
