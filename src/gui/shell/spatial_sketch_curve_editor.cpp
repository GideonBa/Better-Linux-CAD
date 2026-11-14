#include "spatial_sketch_curve_editor.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "spatial_sketch_form_input.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>
namespace blcad::gui {
namespace {
QString q(std::string_view s) {
  return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}
template <class T> T take(Result<T> result) {
  if (!result)
    throw std::runtime_error(result.error().message());
  return std::move(result.value());
}
QString object(const char* id) {
  return QStringLiteral("blcad.sketch3d.") + id;
}
bool exists(const Sketch3D& s, SketchEntityId id) {
  return s.find_point(id) || s.find_line(id) || s.find_polyline(id) || s.find_arc(id) ||
         s.find_spline(id) || s.find_helix(id) || s.find_equation_curve(id) ||
         s.find_guide_curve(id);
}
std::string fresh_resource(const PartDocument& part, const SpatialSketchTaskResources& resources,
                           std::string stem) {
  for (std::size_t i = 1;; ++i) {
    auto id = stem + "." + std::to_string(i);
    const bool used = part.dependency_graph().has_node(id) ||
                      std::any_of(resources.parameters.begin(), resources.parameters.end(),
                                  [&](const auto& p) { return p.id().value() == id; }) ||
                      std::any_of(resources.axes.begin(), resources.axes.end(),
                                  [&](const auto& a) { return a.id().value() == id; });
    if (!used)
      return id;
  }
}
} // namespace
SpatialSketchCurveEditor::SpatialSketchCurveEditor(QWidget* parent,
                                                   SpatialSketchEditorCallbacks callbacks)
    : callbacks_(std::move(callbacks)), page_(new QWidget(parent)) {
  auto* column = new QVBoxLayout(page_);
  auto* form = new QFormLayout;
  auto combo = [&](const char* name, const QString& label, QFormLayout* target = nullptr) {
    auto* value = new QComboBox(page_);
    value->setObjectName(object(name));
    value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    value->setMinimumContentsLength(10);
    (target ? target : form)->addRow(label, value);
    connect(value, &QComboBox::currentIndexChanged, this, [this] { changed(); });
    return value;
  };
  auto field = [&](const char* name, const QString& label, const QString& initial,
                   QFormLayout* target = nullptr) {
    auto* value = new QLineEdit(initial, page_);
    value->setObjectName(object(name));
    (target ? target : form)->addRow(label, value);
    connect(value, &QLineEdit::textChanged, this, [this] { changed(); });
    return value;
  };
  auto spin = [&](const char* name, const QString& label, double initial, double minimum,
                  double maximum, QFormLayout* target) {
    auto* value = new QDoubleSpinBox(page_);
    value->setObjectName(object(name));
    value->setDecimals(9);
    value->setRange(minimum, maximum);
    value->setValue(initial);
    value->setKeyboardTracking(false);
    target->addRow(label, value);
    connect(value, &QDoubleSpinBox::valueChanged, this, [this] { changed(); });
    new SpatialFormNumberGuard(*this, *value, [this, value] {
      if (value == radius_)
        radius_changed_ = true;
      if (value == pitch_)
        pitch_changed_ = true;
      if (value == tolerance_)
        tolerance_changed_ = true;
      changed();
    });
    return value;
  };
  auto button = [&](const char* name, const QString& label, QVBoxLayout* target) {
    auto* value = make_gui_button(label, page_);
    value->setObjectName(object(name));
    target->addWidget(value);
    return value;
  };
  curve_ = combo("curve", "Edit Curve");
  kind_ = combo("curve_kind", "Curve Type");
  kind_->addItems({"Line", "Polyline", "Three-point Arc", "Fit Spline", "Control Spline", "Helix",
                   "Equation 3D"});
  id_ = field("curve_id", "Identity", {});
  column->addLayout(form);
  handles_ = new QCheckBox("Show Curve Radius / Pitch Handles", page_);
  handles_->setObjectName(object("curve_grips"));
  column->addWidget(handles_);
  auto* handle_hint = new QLabel("Bound scalars and linked defining points remain read-only until "
                                 "explicitly rebound. Integer turns are discrete sources.",
                                 page_);
  handle_hint->setWordWrap(true);
  column->addWidget(handle_hint);
  point_fields_ = new QWidget(page_);
  auto* point_column = new QVBoxLayout(point_fields_);
  point_column->setContentsMargins(0, 0, 0, 0);
  auto* point_form = new QFormLayout;
  point_source_ = combo("curve_point_source", "Point Reference", point_form);
  point_column->addLayout(point_form);
  points_ = new QListWidget(page_);
  points_->setObjectName(object("curve_points"));
  points_->setMaximumHeight(120);
  point_column->addWidget(points_);
  auto* add_point = button("curve_add_point", "Append Point Reference", point_column);
  auto* remove_point = button("curve_remove_point", "Remove Selected Reference", point_column);
  auto* up = button("curve_point_up", "Move Reference Up", point_column);
  auto* down = button("curve_point_down", "Move Reference Down", point_column);
  auto* edit_point =
      button("curve_edit_point", "Edit Selected Local Point / Handles", point_column);
  column->addWidget(point_fields_);
  spline_fields_ = new QWidget(page_);
  auto* spline_form = new QFormLayout(spline_fields_);
  spline_form->setContentsMargins(0, 0, 0, 0);
  degree_ = new QSpinBox(page_);
  degree_->setObjectName(object("curve_degree"));
  degree_->setRange(1, 5);
  degree_->setValue(3);
  spline_form->addRow("Degree", degree_);
  representation_continuity_ =
      combo("curve_internal_continuity", "Internal Continuity", spline_form);
  representation_continuity_->addItems({"C0", "C1", "C2"});
  column->addWidget(spline_fields_);
  helix_fields_ = new QWidget(page_);
  auto* helix_form = new QFormLayout(helix_fields_);
  helix_form->setContentsMargins(0, 0, 0, 0);
  axis_ = combo("helix_axis", "Axis", helix_form);
  radius_source_ = combo("helix_radius_source", "Radius Source", helix_form);
  radius_ = spin("helix_radius", "Radius (mm)", 4, 1e-9, 1e9, helix_form);
  pitch_source_ = combo("helix_pitch_source", "Pitch Source", helix_form);
  pitch_ = spin("helix_pitch", "Pitch (mm)", 5, 1e-9, 1e9, helix_form);
  turns_source_ = combo("helix_turns_source", "Integer Turns Source", helix_form);
  turns_ = new QSpinBox(page_);
  turns_->setObjectName(object("helix_turns"));
  turns_->setRange(1, 10000);
  turns_->setValue(2);
  helix_form->addRow("New Integer Turns", turns_);
  handedness_ = combo("helix_handedness", "Handedness", helix_form);
  handedness_->addItems({"Right", "Left"});
  binding_info_ = new QLabel("Choose an owned value or explicitly rebind. Shared parameters and "
                             "formulas are never edited here.",
                             page_);
  binding_info_->setWordWrap(true);
  helix_form->addRow(binding_info_);
  column->addWidget(helix_fields_);
  equation_fields_ = new QWidget(page_);
  auto* equation_form = new QFormLayout(equation_fields_);
  equation_form->setContentsMargins(0, 0, 0, 0);
  variable_ = field("equation_variable", "Variable", "t", equation_form);
  variable_unit_ = combo("equation_unit", "Variable Unit", equation_form);
  variable_unit_->addItems({"Scalar", "Length (mm)", "Angle (rad)"});
  lower_ = field("equation_lower", "Domain Start", "0", equation_form);
  upper_ = field("equation_upper", "Domain End", "1", equation_form);
  for (int i = 0; i < 3; ++i)
    equations_[i] = field((std::string("equation_") + "xyz"[i]).c_str(), QString("XYZ"[i]) + " (t)",
                          i == 0 ? "10mm*t" : "0mm", equation_form);
  tolerance_ = spin("equation_tolerance", "Model Tolerance (mm)", .001, 1e-9, 1, equation_form);
  closure_ = combo("equation_closure", "Closure", equation_form);
  closure_->addItems({"Open", "Periodic"});
  construction_ = new QCheckBox("Construction Curve", page_);
  construction_->setObjectName(object("equation_construction"));
  equation_form->addRow(construction_);
  column->addWidget(equation_fields_);
  auto* stage_button = button("curve_accept", "Accept Curve Locally", column);
  auto* delete_button = button("curve_delete", "Delete Curve Locally", column);
  auto* discard = button("curve_discard", "Discard Form Changes", column);
  auto* guide_form = new QFormLayout;
  guide_ = combo("curve_guide", "Guide Alias", guide_form);
  guide_role_ = combo("curve_guide_role", "Guide Role", guide_form);
  guide_role_->addItems({"General", "Sweep Path", "Loft Guide", "Centerline"});
  column->addLayout(guide_form);
  for (const auto& [name, label, operation] :
       {std::tuple{"curve_guide_add", "Add Guide Alias to Selected Curve", 0},
        std::tuple{"curve_guide_rebind", "Rebind Selected Alias to This Curve", 1},
        std::tuple{"curve_guide_remove", "Remove Selected Guide Alias", 2}}) {
    auto* action = button(name, label, column);
    connect(action, &QPushButton::clicked, this,
            [this, operation] { report([&] { guide_operation(operation); }); });
  }
  feedback_ = new QLabel(page_);
  feedback_->setObjectName(object("curve_feedback"));
  feedback_->setWordWrap(true);
  column->addWidget(feedback_);
  column->addStretch();
  connect(handles_, &QCheckBox::toggled, this, [this] {
    if (!loading_ && callbacks_.selection_changed)
      callbacks_.selection_changed();
  });
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] { mode(); });
  connect(degree_, &QSpinBox::valueChanged, this, [this] { changed(); });
  connect(turns_, &QSpinBox::valueChanged, this, [this] { changed(); });
  new SpatialFormNumberGuard(*this, *degree_, [this] { changed(); });
  new SpatialFormNumberGuard(*this, *turns_, [this] { changed(); });
  connect(construction_, &QCheckBox::toggled, this, [this] { changed(); });
  connect(radius_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (!loading_)
      radius_changed_ = true;
  });
  connect(pitch_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (!loading_)
      pitch_changed_ = true;
  });
  connect(tolerance_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (!loading_)
      tolerance_changed_ = true;
  });
  for (auto* source : {radius_source_, pitch_source_, turns_source_})
    connect(source, &QComboBox::currentIndexChanged, this, [this] { mode(); });
  connect(add_point, &QPushButton::clicked, this, [this] {
    report([&] {
      take(callbacks_.edit());
      const auto i = point_source_->currentData().toInt();
      if (i < 0 || static_cast<std::size_t>(i) >= point_sources_.size())
        throw std::runtime_error("Select a compatible point reference.");
      ordered_.push_back(point_sources_[i].reference);
      show_points();
      changed();
    });
  });
  connect(remove_point, &QPushButton::clicked, this, [this] {
    report([&] {
      take(callbacks_.edit());
      const auto i = points_->currentRow();
      if (i >= 0) {
        ordered_.erase(ordered_.begin() + i);
        show_points();
        changed();
      }
    });
  });
  const auto reorder = [this](int delta) {
    report([&] {
      take(callbacks_.edit());
      const int a = points_->currentRow(), b = a + delta;
      if (a >= 0 && b >= 0 && b < static_cast<int>(ordered_.size())) {
        std::swap(ordered_[a], ordered_[b]);
        show_points();
        points_->setCurrentRow(b);
        changed();
      }
    });
  };
  connect(up, &QPushButton::clicked, this, [reorder] { reorder(-1); });
  connect(down, &QPushButton::clicked, this, [reorder] { reorder(1); });
  connect(edit_point, &QPushButton::clicked, this, [this] {
    report([&] {
      if (pending_)
        throw std::runtime_error("Accept or discard the curve form first.");
      const int i = points_->currentRow();
      if (i < 0 || !ordered_[i].local_point())
        throw std::runtime_error("This source-linked point is read-only here. Explicitly rebind it "
                                 "to a local point to edit.");
      callbacks_.select_point(*ordered_[i].local_point());
    });
  });
  connect(stage_button, &QPushButton::clicked, this, [this] { report([&] { stage(); }); });
  connect(delete_button, &QPushButton::clicked, this, [this] { report([&] { remove(); }); });
  connect(discard, &QPushButton::clicked, this, [this] { load(); });
  // Browsing choices alone does not create an unaccepted record.
  for (auto* selector : {curve_, point_source_, guide_, guide_role_})
    disconnect(selector, &QComboBox::currentIndexChanged, this, nullptr);
  connect(curve_, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading_) {
      pending_ = false;
      if (callbacks_.selection_changed)
        callbacks_.selection_changed();
      load();
    }
  });
}
void SpatialSketchCurveEditor::report(std::function<void()> action) {
  try {
    action();
  } catch (const std::exception& e) {
    feedback_->setText(QString::fromUtf8(e.what()));
  }
}
void SpatialSketchCurveEditor::changed() {
  if (loading_ || !callbacks_.sketch())
    return;
  pending_ = true;
  const auto ready = callbacks_.edit();
  feedback_->setText(ready ? "Curve form changed. Accept or discard it before previewing."
                           : q(ready.error().message()));
}
void SpatialSketchCurveEditor::mode() {
  const auto k = static_cast<Kind>(kind_->currentIndex());
  point_fields_->setVisible(k != Kind::Helix && k != Kind::Equation);
  spline_fields_->setVisible(k == Kind::FitSpline || k == Kind::ControlSpline);
  helix_fields_->setVisible(k == Kind::Helix);
  equation_fields_->setVisible(k == Kind::Equation);
  radius_->setEnabled(radius_source_->currentData().toString().isEmpty());
  pitch_->setEnabled(pitch_source_->currentData().toString().isEmpty());
  turns_->setEnabled(turns_source_->currentData().toString().isEmpty());
  if (callbacks_.sketch())
    for (const auto& pair :
         {std::pair{radius_source_, radius_}, std::pair{pitch_source_, pitch_}}) {
      const ParameterId id(pair.first->currentData().toString().toStdString());
      const Parameter* selected = callbacks_.part().find_parameter(id);
      if (!selected)
        for (const auto& p : callbacks_.resources().parameters)
          if (p.id() == id)
            selected = &p;
      if (selected)
        pair.second->setValue(selected->value().millimeters());
    }
}
QString SpatialSketchCurveEditor::point_label(const SketchPointReference3D& ref) const {
  for (const auto& p : point_sources_)
    if (p.reference.same_source(ref))
      return p.label;
  return "Unavailable source (explicit rebind required)";
}
void SpatialSketchCurveEditor::show_points() {
  points_->clear();
  for (const auto& p : ordered_)
    points_->addItem(point_label(p));
}
void SpatialSketchCurveEditor::fill_sources() {
  const auto& sketch = *callbacks_.sketch();
  const auto& part = callbacks_.part();
  point_sources_.clear();
  point_source_->clear();
  const auto add_point = [&](QString label, Result<SketchPointReference3D> ref) {
    if (ref) {
      point_source_->addItem(label, static_cast<int>(point_sources_.size()));
      point_sources_.push_back({label, std::move(ref.value())});
    }
  };
  for (const auto& p : sketch.points())
    add_point("Local / " + q(p.id().value()), SketchPointReference3D::create_local_point(p.id()));
  const auto descendants = part.dependency_graph().transitive_dependents(sketch.id().value());
  const auto allowed = [&](std::string_view id) {
    return std::find(descendants.begin(), descendants.end(), id) == descendants.end();
  };
  for (const auto& p : part.construction_points())
    if (allowed(p.id().value()))
      add_point("Linked construction / " + q(p.id().value()),
                SketchPointReference3D::create_construction_point(p.id()));
  for (const auto& s : part.sketches())
    if (allowed(s.id().value())) {
      for (const auto& p : s.projected_points())
        add_point("Linked / " + q(s.id().value()) + " / " + q(p.id().value()),
                  SketchPointReference3D::create_planar_sketch_point(
                      s.id(), take(SketchReferenceTarget::create_projected_point(p.id()))));
      for (const auto& line : s.line_segments())
        for (bool end : {false, true})
          add_point(
              "Linked / " + q(s.id().value()) + " / " + q(line.id().value()) +
                  (end ? " / End" : " / Start"),
              SketchPointReference3D::create_planar_sketch_point(
                  s.id(), take(end ? SketchReferenceTarget::create_line_segment_end(line.id())
                                   : SketchReferenceTarget::create_line_segment_start(line.id()))));
    }
  axes_.clear();
  axis_->clear();
  for (int i = 0; i < 3; ++i)
    axis_->addItem(QString("New Owned ") + "XYZ"[i] + " Axis", -i - 1);
  const auto axis_add = [&](QString label, SketchHelixAxis3D value) {
    axis_->addItem(label, static_cast<int>(axes_.size()));
    axes_.push_back(std::move(value));
  };
  for (const auto& a : part.datum_axes())
    if (allowed(a.id().value()))
      axis_add(q(a.name()), take(SketchHelixAxis3D::create_datum_axis(a.id())));
  for (const auto& a : callbacks_.resources().axes)
    axis_add(q(a.name()), take(SketchHelixAxis3D::create_datum_axis(a.id())));
  for (const auto& a : part.construction_lines())
    if (allowed(a.id().value()))
      axis_add(q(a.name()), take(SketchHelixAxis3D::create_construction_line(a.id())));
  for (auto* box : {radius_source_, pitch_source_, turns_source_}) {
    box->clear();
    box->addItem(box == turns_source_ ? "New Owned Integer Count" : "Owned Value (Explicit Rebind)",
                 QString());
  }
  const auto parameter = [&](const Parameter& p) {
    for (auto* box : {radius_source_, pitch_source_, turns_source_})
      if ((box == turns_source_ && p.type() == ParameterType::Count) ||
          (box != turns_source_ && p.type() == ParameterType::Length))
        box->addItem(q(p.name()) + (p.formula() ? " [formula, read-only]" : " [shared, read-only]"),
                     q(p.id().value()));
  };
  for (const auto& p : part.parameters())
    parameter(p);
  for (const auto& p : callbacks_.resources().parameters)
    parameter(p);
}
void SpatialSketchCurveEditor::reset() {
  pending_ = false;
  loading_ = true;
  handles_->setChecked(false);
  kind_->setCurrentIndex(0);
  radius_->setValue(4);
  pitch_->setValue(5);
  turns_->setValue(2);
  degree_->setValue(3);
  representation_continuity_->setCurrentIndex(0);
  handedness_->setCurrentIndex(0);
  variable_->setText("t");
  variable_unit_->setCurrentIndex(0);
  lower_->setText("0");
  upper_->setText("1");
  for (int i = 0; i < 3; ++i)
    equations_[i]->setText(i == 0 ? "10mm*t" : "0mm");
  tolerance_->setValue(.001);
  closure_->setCurrentIndex(0);
  construction_->setChecked(false);
  curve_->clear();
  loading_ = false;
  refresh();
}
void SpatialSketchCurveEditor::refresh() {
  if (!callbacks_.sketch() || pending_)
    return;
  const auto previous = curve_->currentData().toString();
  loading_ = true;
  curves_.clear();
  curve_->clear();
  curve_->addItem("New Curve", QString());
  const auto add = [&](const auto& records, Kind kind) {
    for (const auto& r : records) {
      curves_.push_back({r.id(), kind});
      curve_->addItem(q(r.id().value()), q(r.id().value()));
    }
  };
  const auto& s = *callbacks_.sketch();
  const auto prior_guide = guide_->currentData();
  guide_->clear();
  guide_->addItem("Select Guide Alias", QString());
  for (const auto& guide : s.guide_curves())
    guide_->addItem(q(guide.id().value()) + " / " + q(guide.source_curve().value()),
                    q(guide.id().value()));
  guide_->setCurrentIndex(std::max(0, guide_->findData(prior_guide)));
  add(s.lines(), Kind::Line);
  add(s.polylines(), Kind::Polyline);
  add(s.arcs(), Kind::Arc);
  for (const auto& r : s.splines()) {
    const auto k = r.representation() == SketchSpline3DRepresentation::FitPoints
                       ? Kind::FitSpline
                       : Kind::ControlSpline;
    curves_.push_back({r.id(), k});
    curve_->addItem(q(r.id().value()), q(r.id().value()));
  }
  add(s.helices(), Kind::Helix);
  add(s.equation_curves(), Kind::Equation);
  curve_->setCurrentIndex(std::max(0, curve_->findData(previous)));
  loading_ = false;
  load();
}
bool SpatialSketchCurveEditor::select_curve(SketchEntityId id) {
  const int i = curve_->findData(q(id.value()));
  if (i < 0 || pending_)
    return false;
  curve_->setCurrentIndex(i);
  return true;
}
SketchEntityId SpatialSketchCurveEditor::selected_curve() const {
  return SketchEntityId(curve_->currentData().toString().toStdString());
}
bool SpatialSketchCurveEditor::handles_requested() const {
  return handles_->isChecked();
}
void SpatialSketchCurveEditor::load() {
  if (!callbacks_.sketch())
    return;
  report([&] {
    loading_ = true;
    fill_sources();
    ordered_.clear();
    const auto& s = *callbacks_.sketch();
    const SketchEntityId id(curve_->currentData().toString().toStdString());
    const bool existing = !id.empty();
    id_->setReadOnly(existing);
    kind_->setEnabled(!existing);
    id_->setText(q(id.value()));
    if (!existing) {
      axis_->setCurrentIndex(2);
      for (std::size_t i = 1;; ++i) {
        const auto next = "curve." + std::to_string(i);
        if (!exists(s, SketchEntityId(next))) {
          id_->setText(q(next));
          break;
        }
      }
    } else {
      const auto found =
          std::find_if(curves_.begin(), curves_.end(), [&](const auto& c) { return c.id == id; });
      if (found == curves_.end())
        throw std::runtime_error("Select the curve again.");
      kind_->setCurrentIndex(static_cast<int>(found->kind));
      if (const auto* line = s.find_line(id))
        ordered_ = {take(SketchPointReference3D::create_local_point(line->start_point())),
                    take(SketchPointReference3D::create_local_point(line->end_point()))};
      if (const auto* poly = s.find_polyline(id))
        for (const auto& p : poly->ordered_vertices())
          ordered_.push_back(take(SketchPointReference3D::create_local_point(p)));
      if (const auto* arc = s.find_arc(id))
        ordered_ = arc->point_references();
      if (const auto* spline = s.find_spline(id)) {
        ordered_ = spline->ordered_points();
        degree_->setValue(static_cast<int>(spline->degree()));
        representation_continuity_->setCurrentIndex(static_cast<int>(spline->continuity()));
      }
      if (const auto* helix = s.find_helix(id)) {
        const auto parameter = [&](const ParameterId& id) -> const Parameter* {
          if (const auto* value = callbacks_.part().find_parameter(id))
            return value;
          for (const auto& value : callbacks_.resources().parameters)
            if (value.id() == id)
              return &value;
          return nullptr;
        };
        for (std::size_t i = 0; i < axes_.size(); ++i)
          if (axes_[i].source() == helix->axis().source() &&
              axes_[i].referenced_node_id() == helix->axis().referenced_node_id())
            axis_->setCurrentIndex(axis_->findData(static_cast<int>(i)));
        const auto source = [&](QComboBox* box, QDoubleSpinBox* value,
                                const SketchLength3D& binding) {
          box->setCurrentIndex(std::max(0, box->findData(q(binding.parameter_id().value()))));
          if (binding.explicit_length())
            value->setValue(binding.explicit_length()->millimeters());
          else if (const auto* bound = parameter(binding.parameter_id()))
            value->setValue(bound->value().millimeters());
        };
        source(radius_source_, radius_, helix->radius_binding());
        source(pitch_source_, pitch_, helix->pitch_binding());
        turns_source_->setCurrentIndex(
            turns_source_->findData(q(helix->turns_parameter().value())));
        if (const auto* bound = parameter(helix->turns_parameter()))
          turns_->setValue(static_cast<int>(bound->value().count_value()));
        handedness_->setCurrentIndex(static_cast<int>(helix->handedness()));
      }
      if (const auto* equation = s.find_equation_curve(id)) {
        variable_->setText(q(equation->variable().name));
        variable_unit_->setCurrentIndex(static_cast<int>(equation->variable().unit));
        lower_->setText(q(equation->lower()));
        upper_->setText(q(equation->upper()));
        for (int i = 0; i < 3; ++i)
          equations_[i]->setText(q(equation->coordinates()[i]));
        tolerance_->setValue(equation->tolerance_mm());
        closure_->setCurrentIndex(static_cast<int>(equation->closure()));
        construction_->setChecked(equation->construction());
      }
    }
    show_points();
    mode();
    radius_changed_ = pitch_changed_ = tolerance_changed_ = false;
    loading_ = false;
    pending_ = false;
    feedback_->setText("Point references retain their sources. Internal spline C2 is distinct from "
                       "endpoint Smooth/G2.");
  });
  loading_ = false;
}
void SpatialSketchCurveEditor::stage() {
  const auto selected_kind = static_cast<Kind>(kind_->currentIndex());
  if (selected_kind == Kind::Helix) {
    if (radius_->isEnabled())
      finish_spatial_number(*radius_);
    if (pitch_->isEnabled())
      finish_spatial_number(*pitch_);
    if (turns_->isEnabled())
      finish_spatial_number(*turns_);
  }
  if (selected_kind == Kind::FitSpline || selected_kind == Kind::ControlSpline)
    finish_spatial_number(*degree_);
  if (selected_kind == Kind::Equation)
    finish_spatial_number(*tolerance_);
  take(callbacks_.edit());
  auto candidate = *callbacks_.sketch();
  auto resources = callbacks_.resources();
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  const SketchEntityId id(id_->text().trimmed().toStdString());
  const bool existing = !curve_->currentData().toString().isEmpty();
  if (!existing && exists(candidate, id))
    throw std::runtime_error("Choose an unused curve identity.");
  if (kind == Kind::Line || kind == Kind::Polyline) {
    std::vector<SketchEntityId> local;
    for (const auto& ref : ordered_) {
      if (!ref.local_point())
        throw std::runtime_error(
            "Lines and polylines require local points. Choose local references explicitly.");
      local.push_back(*ref.local_point());
    }
    if (kind == Kind::Line) {
      if (local.size() != 2)
        throw std::runtime_error("A line requires exactly two ordered points.");
      auto value = take(SketchLine3D::create(id, local[0], local[1]));
      take(existing ? candidate.replace_line(value) : candidate.add_line(value));
    } else {
      auto value = take(SketchPolyline3D::create(id, local));
      take(existing ? candidate.replace_polyline(value) : candidate.add_polyline(value));
    }
  } else if (kind == Kind::Arc) {
    if (ordered_.size() != 3)
      throw std::runtime_error(
          "An arc requires start, intermediate and end references in that order.");
    auto value = take(SketchArc3D::create_three_point(id, ordered_[0], ordered_[1], ordered_[2]));
    take(existing ? candidate.replace_arc(value) : candidate.add_arc(value));
  } else if (kind == Kind::FitSpline || kind == Kind::ControlSpline) {
    auto value = take(SketchSpline3D::create(
        id,
        kind == Kind::FitSpline ? SketchSpline3DRepresentation::FitPoints
                                : SketchSpline3DRepresentation::ControlPoints,
        ordered_, static_cast<std::size_t>(degree_->value()),
        static_cast<SketchSpline3DContinuity>(representation_continuity_->currentIndex())));
    take(existing ? candidate.replace_spline(value) : candidate.add_spline(value));
  } else if (kind == Kind::Helix) {
    const auto axis_index = axis_->currentData().toInt();
    auto selected_axis = [&] {
      if (axis_index >= 0)
        return axes_.at(static_cast<std::size_t>(axis_index));
      const auto identity =
          fresh_resource(callbacks_.part(), resources, candidate.id().value() + ".axis");
      Vector3 direction{0, 0, 0};
      (axis_index == -1 ? direction.x : axis_index == -2 ? direction.y : direction.z) = 1;
      resources.axes.push_back(take(
          DatumAxis::create_explicit(DatumAxisId(identity), "Owned Helix Axis", {}, direction)));
      ++resources.generation;
      return take(SketchHelixAxis3D::create_datum_axis(DatumAxisId(identity)));
    }();
    const auto binding = [&](QComboBox* box, QDoubleSpinBox* value) {
      const auto parameter = box->currentData().toString();
      if (existing) {
        const auto* old = candidate.find_helix(id);
        const auto& original = box == radius_source_ ? old->radius_binding() : old->pitch_binding();
        const bool edited = box == radius_source_ ? radius_changed_ : pitch_changed_;
        if (!edited && parameter == q(original.parameter_id().value()))
          return original;
      }
      return parameter.isEmpty()
                 ? take(SketchLength3D::create_explicit(take(Quantity::length_mm(value->value()))))
                 : take(SketchLength3D::create_parameter(ParameterId(parameter.toStdString())));
    };
    auto turns = turns_source_->currentData().toString().toStdString();
    if (turns.empty()) {
      turns = fresh_resource(callbacks_.part(), resources, candidate.id().value() + ".turns");
      resources.parameters.push_back(take(Parameter::create_count(
          ParameterId(turns), "Owned Helix Turns", take(Quantity::count(turns_->value())))));
      ++resources.generation;
    }
    auto value = take(SketchHelix3D::create_with_bindings(
        id, selected_axis, binding(radius_source_, radius_), binding(pitch_source_, pitch_),
        ParameterId(turns), static_cast<SketchHelix3DHandedness>(handedness_->currentIndex())));
    take(existing ? candidate.replace_helix(value) : candidate.add_helix(value));
  } else {
    std::array<std::string, 3> expressions;
    for (int i = 0; i < 3; ++i)
      expressions[i] = equations_[i]->text().toStdString();
    auto value = take(EquationCurve::create(
        id, EquationCurveMode::Parametric3D,
        {variable_->text().trimmed().toStdString(),
         static_cast<CurveUnit>(variable_unit_->currentIndex())},
        lower_->text().toStdString(), upper_->text().toStdString(), expressions,
        existing && !tolerance_changed_ ? candidate.find_equation_curve(id)->tolerance_mm()
                                        : tolerance_->value(),
        static_cast<EquationCurveClosure>(closure_->currentIndex()), construction_->isChecked()));
    take(existing ? candidate.replace_equation_curve(value) : candidate.add_equation_curve(value));
  }
  take(callbacks_.accept(std::move(candidate), std::move(resources)));
  pending_ = false;
  refresh();
  select_curve(id);
  feedback_->setText("Curve accepted locally. Preview validates every downstream consumer.");
}
void SpatialSketchCurveEditor::remove() {
  take(callbacks_.edit());
  auto candidate = *callbacks_.sketch();
  const SketchEntityId id(curve_->currentData().toString().toStdString());
  if (id.empty())
    throw std::runtime_error("Select an existing curve.");
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  auto removed = kind == Kind::Line       ? candidate.remove_line(id)
                 : kind == Kind::Polyline ? candidate.remove_polyline(id)
                 : kind == Kind::Arc      ? candidate.remove_arc(id)
                 : kind == Kind::FitSpline || kind == Kind::ControlSpline
                     ? candidate.remove_spline(id)
                 : kind == Kind::Helix ? candidate.remove_helix(id)
                                       : candidate.remove_equation_curve(id);
  take(removed);
  take(callbacks_.accept(std::move(candidate), callbacks_.resources()));
  pending_ = false;
  refresh();
  feedback_->setText(
      "Curve deleted locally. Referenced curves require explicit rebind/removal first.");
}
void SpatialSketchCurveEditor::guide_operation(int operation) {
  if (pending_)
    throw std::runtime_error("Accept or discard the curve form first.");
  take(callbacks_.edit());
  auto candidate = *callbacks_.sketch();
  const SketchEntityId source(curve_->currentData().toString().toStdString());
  auto id = SketchEntityId(guide_->currentData().toString().toStdString());
  if (operation != 0 && !candidate.find_guide_curve(id))
    throw std::runtime_error("Select the guide alias to change.");
  std::string label;
  if (operation != 0) {
    label = candidate.find_guide_curve(id)->label();
    take(candidate.remove_guide_curve(id));
  } else {
    for (std::size_t i = 1;; ++i) {
      id = SketchEntityId("guide." + std::to_string(i));
      if (!exists(candidate, id))
        break;
    }
  }
  if (operation != 2) {
    if (source.empty())
      throw std::runtime_error("Select the existing source curve first.");
    take(candidate.add_guide_curve(take(SketchGuideCurve3D::create(
        id, source, static_cast<SketchGuideCurve3DRole>(guide_role_->currentIndex()), label))));
  }
  take(callbacks_.accept(std::move(candidate), callbacks_.resources()));
  refresh();
  if (operation != 2)
    guide_->setCurrentIndex(guide_->findData(q(id.value())));
  feedback_->setText("Guide metadata accepted locally. Existing source references remain checked.");
}
} // namespace blcad::gui
