#pragma once
#include "spatial_sketch_task_context.hpp"
#include <QObject>
#include <QString>
#include <functional>
class QComboBox;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QListWidget;
class QLabel;
class QWidget;
namespace blcad::gui {
struct SpatialSketchEditorCallbacks {
  std::function<const Sketch3D*()> sketch;
  std::function<const PartDocument&()> part;
  std::function<const SpatialSketchTaskResources&()> resources;
  std::function<Result<std::size_t>(Sketch3D, SpatialSketchTaskResources)> accept;
  std::function<Result<std::size_t>()> edit;
  std::function<void(SketchEntityId)> select_point;
};
// Forms propose typed records to the existing task authority. They own neither a second Sketch3D
// nor persistent parameter edits. Incomplete forms remain separate from the accepted draft.
class SpatialSketchCurveEditor final : public QObject {
public:
  SpatialSketchCurveEditor(QWidget*, SpatialSketchEditorCallbacks);
  QWidget* page() const noexcept {
    return page_;
  }
  void reset();
  void refresh();
  bool pending() const noexcept {
    return pending_;
  }
  bool select_curve(SketchEntityId);

private:
  enum class Kind { Line, Polyline, Arc, FitSpline, ControlSpline, Helix, Equation };
  struct Curve {
    SketchEntityId id;
    Kind kind;
  };
  struct PointSource {
    QString label;
    SketchPointReference3D reference;
  };
  SpatialSketchEditorCallbacks callbacks_;
  QWidget* page_{};
  QComboBox *curve_{}, *kind_{}, *point_source_{}, *representation_continuity_{}, *axis_{},
      *radius_source_{}, *pitch_source_{}, *turns_source_{}, *handedness_{}, *variable_unit_{},
      *closure_{};
  QLineEdit *id_{}, *variable_{}, *lower_{}, *upper_{};
  std::array<QLineEdit*, 3> equations_{};
  QListWidget* points_{};
  QSpinBox *degree_{}, *turns_{};
  QDoubleSpinBox *radius_{}, *pitch_{}, *tolerance_{};
  QCheckBox* construction_{};
  QComboBox *guide_{}, *guide_role_{};
  QLabel *feedback_{}, *binding_info_{};
  QWidget *point_fields_{}, *spline_fields_{}, *helix_fields_{}, *equation_fields_{};
  std::vector<Curve> curves_;
  std::vector<PointSource> point_sources_;
  std::vector<SketchPointReference3D> ordered_;
  std::vector<SketchHelixAxis3D> axes_;
  bool loading_{false}, pending_{false};
  void changed();
  void load();
  void mode();
  void fill_sources();
  void show_points();
  void stage();
  void remove();
  void guide_operation(int);
  void report(std::function<void()>);
  QString point_label(const SketchPointReference3D&) const;
};
} // namespace blcad::gui
