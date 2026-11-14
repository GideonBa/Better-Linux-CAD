#pragma once
#include "spatial_sketch_curve_editor.hpp"
class QCheckBox;
namespace blcad::gui {
struct SpatialSketchConstraintCallbacks {
  SpatialSketchEditorCallbacks edit;
  std::function<Result<std::vector<SpatialSketchTargetChoice>>()> targets;
  std::function<Result<SpatialCurveJet>(const SpatialConstraintTarget&, double)> jet;
  std::function<Result<Point3>(const SpatialConstraintTarget&)> position;
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

private:
  SpatialSketchConstraintCallbacks callbacks_;
  QWidget* page_{};
  QComboBox *record_{}, *kind_{}, *source_{}, *angle_{};
  std::array<QComboBox*, 3> targets_{};
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
