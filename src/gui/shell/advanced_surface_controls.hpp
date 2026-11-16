#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/core/surface_feature.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/gui/gui_selection_model.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QWidget>
#include <array>
#include <optional>
namespace blcad::gui {
// One widget instance serves Surface creation and double-click editing.
struct AdvancedSurfaceControls {
  QWidget* panel{};
  QFormLayout* form{};
  QComboBox *side{}, *trim_side{}, *projection{}, *extension{}, *approximation{}, *operation{},
      *body{};
  QLineEdit *tolerance{}, *deviation{};
  std::array<QLineEdit*, 3> direction{};
  QWidget* direction_row{};
  QCheckBox *boundaries{}, *override_formulas{};
  QLabel* feedback{};
  bool highlighting_input = false;
  std::optional<FeatureId> editing;
  std::optional<geometry::ShapeCache> input_cache;
  std::optional<std::size_t> input_prefix;
  GuiSelectionModel entry_selection;
  std::optional<GuiSelection> entry_viewport_selection;
  std::array<ParameterId, 3> override_ids{ParameterId(""), ParameterId(""), ParameterId("")};
  explicit AdvancedSurfaceControls(QWidget* parent) {
    panel = new QWidget(parent);
    panel->setObjectName("blcad.surface.advanced");
    form = new QFormLayout(panel);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const auto combo = [&](const char* id, const char* title, QStringList labels) {
      auto* value = new QComboBox(panel);
      value->setObjectName(QString("blcad.surface.") + id);
      value->addItems(labels);
      value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      form->addRow(title, value);
      return value;
    };
    side = combo("side", "Direction",
                 {"Along Normal", "Opposite Normal", "Symmetric — Total Distance"});
    trim_side = combo("trim_side", "Retain", {"Inside Contour", "Outside Contour"});
    projection = combo("projection", "Contour Projection", {"On Surface", "Along Direction"});
    extension = combo("extension_mode", "Continuation", {"Natural", "Tangent (G1)"});
    direction_row = new QWidget(panel);
    auto* row = new QHBoxLayout(direction_row);
    row->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
      row->addWidget(new QLabel(QString(QLatin1Char("XYZ"[i])), direction_row));
      direction[i] = new QLineEdit(i == 2 ? "1" : "0", direction_row);
      direction[i]->setObjectName(QString("blcad.surface.projection_%1").arg(i));
      direction[i]->setMinimumWidth(45);
      direction[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
      row->addWidget(direction[i]);
    }
    form->addRow("Projection Vector", direction_row);
    approximation =
        combo("approximation", "Construction", {"Exact Analytic", "Verified Approximation"});
    tolerance = new QLineEdit("0.000001", panel);
    tolerance->setObjectName("blcad.surface.tolerance");
    form->addRow("Tolerance (mm)", tolerance);
    deviation = new QLineEdit("0.0001", panel);
    deviation->setObjectName("blcad.surface.deviation");
    form->addRow("Maximum Deviation (mm)", deviation);
    operation = combo("operation", "Result", {"New Body", "Join", "Cut", "Intersect"});
    body = combo("operation_target", "Target Solid", {});
    override_formulas = new QCheckBox("Make changed shared values independent", panel);
    override_formulas->setObjectName("blcad.surface.override_formulas");
    override_formulas->setToolTip("Creates a separate parameter for each changed field; keeps "
                                  "shared values and formulas unchanged.");
    form->addRow(override_formulas);
    boundaries = new QCheckBox("Show free edges and gap diagnostics", panel);
    boundaries->setObjectName("blcad.surface.boundaries");
    boundaries->setChecked(true);
    form->addRow(boundaries);
    feedback = new QLabel(panel);
    feedback->setObjectName("blcad.surface.measurements");
    feedback->setWordWrap(true);
    feedback->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    form->addRow(feedback);
  }
  void configure(SurfaceFeatureKind kind) {
    const bool offset =
        kind == SurfaceFeatureKind::OffsetSurface || kind == SurfaceFeatureKind::Thicken;
    const bool trim = kind == SurfaceFeatureKind::TrimSurface,
               extend = kind == SurfaceFeatureKind::ExtendSurface;
    form->setRowVisible(side, offset);
    form->setRowVisible(trim_side, trim);
    form->setRowVisible(projection, trim);
    form->setRowVisible(direction_row, trim && projection->currentIndex() == 1);
    form->setRowVisible(extension, extend);
    form->setRowVisible(approximation, offset);
    form->setRowVisible(tolerance, offset || trim || extend);
    form->setRowVisible(deviation, (offset && approximation->currentIndex() == 1) ||
                                       (extend && extension->currentIndex() == 1));
    form->setRowVisible(operation, kind == SurfaceFeatureKind::Thicken);
    form->setRowVisible(body,
                        kind == SurfaceFeatureKind::Thicken && operation->currentIndex() != 0);
    form->setRowVisible(override_formulas, editing.has_value() && (offset || trim || extend));
  }
  void reset(const PartDocument& part) {
    editing.reset();
    input_cache.reset();
    input_prefix.reset();
    override_ids = {ParameterId(""), ParameterId(""), ParameterId("")};
    override_formulas->setChecked(false);
    side->setCurrentIndex(0);
    trim_side->setCurrentIndex(0);
    projection->setCurrentIndex(0);
    extension->setCurrentIndex(0);
    approximation->setCurrentIndex(0);
    operation->setCurrentIndex(0);
    tolerance->setText("0.000001");
    deviation->setText("0.0001");
    feedback->clear();
    body->clear();
    for (const auto& item : part.bodies())
      if (item.kind() == BodyKind::Solid)
        body->addItem(QString::fromStdString(item.name()),
                      QString::fromStdString(item.id().value()));
  }
};
} // namespace blcad::gui
