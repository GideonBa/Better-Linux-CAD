#pragma once
#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/gui/gui_interactive_assembly.hpp"
#include <QObject>
#include <array>

class QAction;
class QCheckBox;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTableWidget;
namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class OcctViewport;
class ShellAssemblyTools final : public QObject {
public:
  enum class Command {
    None,
    InsertComponent,
    InsertSubassembly,
    Placement,
    Relationship,
    Joint,
    Motion,
    Solve,
    ValidateHierarchy,
    CrossRelationship,
    CrossJoint,
    CrossSolve,
    CrossMotion
  };
  struct Context {
    Command command;
    GuiSelection selection;
    std::string source;
  };
  struct HierarchyTarget {
    AssemblyHierarchyConstraintEndpoint endpoint;
    geometry::AssemblyInteractionTarget target;
  };
  [[nodiscard]] QAction* context_action(Command) const noexcept;
  [[nodiscard]] std::optional<Context> context(Command) const;
  [[nodiscard]] bool context_available(const Context&) const;
  [[nodiscard]] bool select_context(const Context&);
  [[nodiscard]] Command command() const noexcept {
    return command_;
  }
  [[nodiscard]] const std::vector<HierarchyTarget>& hierarchy_targets() const noexcept {
    return hierarchy_targets_;
  }
  ShellAssemblyTools(ShellWindow&, GuiDocumentSession&, OcctViewport&, ShellRibbon&, int tab);
  ~ShellAssemblyTools() override;
  [[nodiscard]] Result<std::size_t> import_definition(const QString& path);
  [[nodiscard]] Result<std::size_t> begin(Command);
  [[nodiscard]] Result<std::size_t> apply();
  void cancel();
  void refresh();
  void refresh_presentation();
  [[nodiscard]] bool active() const noexcept {
    return command_ != Command::None;
  }
  [[nodiscard]] bool accept_selection(const GuiSelection&);
  [[nodiscard]] std::vector<geometry::ViewportSceneItem> selection_items() const;
  [[nodiscard]] GuiViewportManipulatorLayer& manipulators() noexcept {
    return layer_;
  }
  [[nodiscard]] GuiInteractiveAssemblyCoordinator& coordinator() noexcept {
    return coordinator_;
  }
  [[nodiscard]] Result<std::size_t> set_drag_text(const QString& text);
  [[nodiscard]] QComboBox& definition_editor() noexcept {
    return *definition_;
  }
  [[nodiscard]] QComboBox& placement_drag_editor() noexcept {
    return *placement_drag_;
  }
  [[nodiscard]] QComboBox& occurrence_editor() noexcept {
    return *occurrence_;
  }
  [[nodiscard]] QDoubleSpinBox& placement_editor(std::size_t i) noexcept {
    return *placement_values_.at(i);
  }
  [[nodiscard]] QCheckBox& grounding_editor() noexcept {
    return *grounding_;
  }
  [[nodiscard]] QCheckBox& visibility_editor() noexcept {
    return *visibility_;
  }
  [[nodiscard]] QCheckBox& suppression_editor() noexcept {
    return *suppression_;
  }
  [[nodiscard]] QComboBox& first_editor() noexcept {
    return *first_;
  }
  [[nodiscard]] QComboBox& second_editor() noexcept {
    return *second_;
  }
  [[nodiscard]] QComboBox& pick_editor() noexcept {
    return *pick_;
  }
  [[nodiscard]] QComboBox& relationship_editor() noexcept {
    return *relationship_type_;
  }
  [[nodiscard]] QComboBox& joint_type_editor() noexcept {
    return *joint_type_;
  }
  [[nodiscard]] QTableWidget& slots_editor() noexcept {
    return *slots_;
  }
  [[nodiscard]] QComboBox& joint_editor() noexcept {
    return *joint_;
  }
  [[nodiscard]] QComboBox& coordinate_editor() noexcept {
    return *coordinate_;
  }
  [[nodiscard]] QDoubleSpinBox& value_editor() noexcept {
    return *value_;
  }
  [[nodiscard]] QSlider& slider() noexcept {
    return *slider_;
  }
  [[nodiscard]] const std::vector<geometry::AssemblyInteractionTarget>& targets() const noexcept {
    return targets_;
  }

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  [[nodiscard]] std::optional<Context> context_for(Command, const GuiSelection&) const;
  [[nodiscard]] bool placement() const noexcept;
  [[nodiscard]] bool advanced() const noexcept;
  void create_advanced_controls(QWidget* panel);
  [[nodiscard]] Result<std::size_t> begin_advanced();
  [[nodiscard]] Result<std::size_t> build_hierarchy_targets();
  void load_advanced_record();
  void load_cross_motion();
  void synchronize_cross_motion();
  void update_advanced_second_choices();
  [[nodiscard]] Result<QString> preview_advanced(Project&);
  [[nodiscard]] Result<std::size_t> apply_advanced();
  [[nodiscard]] const HierarchyTarget* hierarchy_selected(QComboBox*) const;
  [[nodiscard]] Result<AssemblyHierarchyConstraint> cross_relationship() const;
  [[nodiscard]] Result<AssemblyHierarchyJoint> cross_joint() const;
  [[nodiscard]] Result<std::size_t> rebuild_controller();
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> candidate(const GuiViewportManipulatorCandidate&);
  void publish_handles();
  void changed();
  void update_second_choices();
  void update_pick_scope();
  void load_placement();
  void load_joint();
  void load_slots();
  void synchronize_values();
  void end(bool applied);
  void restore_drag();
  [[nodiscard]] RigidTransform transform() const;
  [[nodiscard]] std::vector<AssemblyJointCoordinateSlot>
  coordinate_slots(Result<std::size_t>& status) const;
  [[nodiscard]] const geometry::AssemblyInteractionTarget* selected(QComboBox*) const;
  ShellWindow& window_;
  GuiDocumentSession& session_;
  OcctViewport& viewport_;
  GuiViewportManipulatorLayer layer_;
  GuiInteractiveAssemblyCoordinator coordinator_;
  Command command_{Command::None};
  QWidget* overlay_{};
  QDockWidget* dock_{};
  QWidget* advanced_controls_{};
  QComboBox* advanced_record_{};
  QLineEdit* advanced_name_{};
  QCheckBox* advanced_active_{};
  std::vector<HierarchyTarget> hierarchy_targets_;
  std::vector<AssemblyCrossHierarchySolveGroup> cross_groups_;
  std::optional<geometry::AssemblyCrossHierarchySolveResult> cross_solved_;
  std::optional<geometry::AssemblyCrossHierarchyJointMotionResult> cross_moved_;
  QWidget* placement_controls_{};
  QWidget* target_controls_{};
  QWidget* motion_controls_{};
  QComboBox* definition_{};
  QComboBox* occurrence_{};
  QComboBox* placement_drag_{};
  QLineEdit* name_{};
  std::array<QDoubleSpinBox*, 6> placement_values_{};
  QCheckBox* grounding_{};
  QCheckBox* visibility_{};
  QCheckBox* suppression_{};
  QComboBox* relationship_type_{};
  QComboBox* joint_type_{};
  QComboBox* first_{};
  QComboBox* second_{};
  QComboBox* pick_{};
  QTableWidget* slots_{};
  QComboBox* joint_{};
  QComboBox* coordinate_{};
  QSlider* slider_{};
  QDoubleSpinBox* value_{};
  QLabel* value_label_{};
  QLabel* slider_label_{};
  QLabel* summary_{};
  QLineEdit* drag_editor_{};
  QListWidget* solve_group_{};
  QPushButton* apply_{};
  std::vector<std::pair<Command, QAction*>> actions_;
  QAction* import_action_{};
  std::vector<geometry::AssemblyInteractionTarget> targets_;
  std::optional<geometry::AssemblySolveResult> solved_;
  ComponentInstanceId component_id_{""};
  SubassemblyInstanceId subassembly_id_{""};
  AssemblyConstraintId relationship_id_{""};
  AssemblyJointId joint_id_{""};
  std::size_t revision_{};
  std::string source_json_;
  std::uint32_t previous_mask_{};
  std::optional<std::vector<std::string>> previous_scope_;
  std::optional<RigidTransform> drag_transform_;
  std::optional<double> drag_value_;
  std::string drag_buffer_;
  GuiSketchScreenPoint last_pointer_{};
  double slider_lower_{}, slider_upper_{};
  bool sync_{false};
  bool valid_{false};
};
} // namespace blcad::gui
