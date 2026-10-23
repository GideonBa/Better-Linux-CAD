#pragma once

#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <QObject>
#include <array>
#include <string_view>
#include <vector>

class QAction;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace blcad::gui {
class ShellRibbon;
class ShellWindow;

// Candidate-only construction intent. The shell owns the task and its widgets;
// Apply uses the existing document transaction and Geometry resolver authorities.
class ShellConstructionTools final : public QObject {
public:
  enum class Kind {
    Point,
    Line,
    LineThroughPoints,
    Axis,
    AxisFromLine,
    Plane,
    OffsetPlane,
    PlaneThroughPoints,
    FacePlane,
    PointOnEdge,
    PointOnVertex,
    ParallelLineThroughPoint,
    ParallelEdgeThroughPoint,
    ParallelPlaneThroughPoint
  };

  ShellConstructionTools(ShellWindow&, ShellRibbon&, int model_tab);
  [[nodiscard]] Result<std::size_t> begin(Kind);
  [[nodiscard]] Result<std::size_t> begin_edit(std::string_view object_id);
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  void cancel();
  void refresh();
  [[nodiscard]] bool accept_selection(const GuiSelection&);
  [[nodiscard]] std::vector<geometry::ViewportSceneItem> selection_items() const;
  [[nodiscard]] bool active() const noexcept {
    return active_;
  }

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  [[nodiscard]] Result<std::size_t> populate(PartDocument&) const;
  [[nodiscard]] bool current() const;
  [[nodiscard]] Result<std::size_t> open_task();
  void update_inputs();
  void update_preview();
  void fill_references();
  void update_pick_roles();
  void update_pick_scope();
  [[nodiscard]] QComboBox* pick_input() const;
  void show_row(QWidget*, bool);
  ShellWindow& window_;
  QDockWidget* dock_{};
  QFormLayout* form_{};
  QComboBox* kind_{};
  QComboBox* pick_role_{};
  QLineEdit* name_{};
  std::array<QDoubleSpinBox*, 3> origin_{};
  std::array<QDoubleSpinBox*, 3> direction_{};
  std::array<QDoubleSpinBox*, 3> y_direction_{};
  QWidget* origin_row_{};
  QWidget* direction_row_{};
  QWidget* y_direction_row_{};
  QComboBox* plane_{};
  QComboBox* line_{};
  std::array<QComboBox*, 3> points_{};
  QComboBox* parameter_{};
  QDoubleSpinBox* offset_{};
  QComboBox* face_{};
  QComboBox* edge_{};
  QComboBox* vertex_{};
  QLabel* summary_{};
  QPushButton* apply_{};
  QAction* construction_action_{};
  QAction* workplane_action_{};
  std::string id_;
  std::string relation_id_;
  std::string original_name_;
  std::vector<ParameterId> dependencies_;
  std::array<Vector3, 3> initial_values_{};
  std::array<Vector3, 3> original_values_{};
  Vector3 original_normal_{};
  Kind edit_kind_{};
  std::size_t revision_{};
  bool active_{false};
  bool editing_{false};
  bool updating_{false};
  std::uint32_t previous_filter_{0xFFFFFFFFU};
  std::optional<std::vector<std::string>> previous_scope_;
  std::vector<geometry::ConstructionSelectionTarget> selection_targets_;
};
} // namespace blcad::gui
