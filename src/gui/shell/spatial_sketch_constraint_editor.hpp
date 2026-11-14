#pragma once
#include "blcad/geometry/viewport_scene.hpp"
#include "spatial_sketch_curve_editor.hpp"
#include <QPointF>
class QCheckBox;
namespace blcad::gui {
class ShellPropertySelector;
}
namespace blcad::gui {
struct SpatialSketchConstraintCallbacks {
  SpatialSketchEditorCallbacks edit;
  std::function<Result<std::vector<SpatialSketchTargetChoice>>()> targets;
  std::function<Result<SpatialCurveJet>(const SpatialConstraintTarget&, double)> jet;
  std::function<Result<Point3>(const SpatialConstraintTarget&)> position;
  std::function<Result<QPointF>(Point3)> project;
  std::function<bool(const SpatialSketchTargetChoice&)> visible;
  std::function<void()> pick_changed;
};
class SpatialSketchConstraintEditor final : public QObject {
public:
  SpatialSketchConstraintEditor(QWidget*, SpatialSketchConstraintCallbacks);
  QWidget* page() const noexcept {
    return page_;
  }
  bool pending() const noexcept {
    return pending_;
  }
  void reset();
  void refresh();
  [[nodiscard]] bool picking() const noexcept {
    return pick_slot_.has_value();
  }
  [[nodiscard]] std::vector<geometry::ViewportSceneItem> pick_presentations();
  bool accept_pick(std::string_view, std::optional<QPointF>);
  void stop_picking();

private:
  SpatialSketchConstraintCallbacks callbacks_;
  QWidget* page_{};
  QComboBox *record_{}, *kind_{}, *source_{}, *angle_{};
  std::array<QComboBox*, 3> targets_{};
  std::array<ShellPropertySelector*, 3> selectors_{};
  QCheckBox* hidden_{};
  std::optional<std::size_t> pick_slot_;
  std::vector<std::pair<std::string, std::vector<std::size_t>>> pick_aliases_;
  QLineEdit* id_{};
  QDoubleSpinBox *value_{}, *seed_{};
  std::array<QDoubleSpinBox*, 3> fixed_{};
  QLabel *feedback_{}, *differential_{};
  std::vector<SpatialSketchTargetChoice> choices_;
  bool loading_{false}, pending_{false};
  std::optional<SpatialConstraint> loaded_;
  std::optional<Point3> copied_anchor_;
  bool value_changed_{false}, seed_changed_{false};
  std::array<bool, 3> fixed_changed_{};
  void changed();
  void load();
  void mode();
  void fill_targets();
  void stage();
  void remove();
  void inspect();
  bool compatible(const SpatialSketchTargetChoice&, std::size_t) const;
  std::size_t target_count() const;
  const SpatialSketchTargetChoice& selected(std::size_t) const;
  void report(std::function<void()>);
};
} // namespace blcad::gui
