#include "spatial_sketch_constraint_editor.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "spatial_sketch_form_input.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace blcad::gui {
namespace {
QString q(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
template <class T> T take(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().message());
  return std::move(value.value());
}
using K = SpatialConstraintKind;
bool point(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Point ||
         (t.kind() == SpatialTargetKind::Curve &&
          t.curve_location() != SpatialCurveLocation::Whole);
}
bool endpoint(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Curve && t.curve_location() != SpatialCurveLocation::Whole;
}
bool whole(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Curve && t.curve_location() == SpatialCurveLocation::Whole;
}
bool direction(const SpatialSketchTargetChoice& t) {
  return t.target.kind() == SpatialTargetKind::Axis || (whole(t.target) && t.straight);
}
bool radius(const Sketch3D& owner, const SpatialConstraintTarget& t) {
  if (const auto* c = std::get_if<SpatialLocalCurve>(&t.source()))
    return owner.find_arc(c->entity) || owner.find_helix(c->entity);
  if (const auto* c = std::get_if<SpatialExternalCurve>(&t.source()))
    return c->reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Helix ||
           c->reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Arc ||
           c->reference.planar_curve_kind() == PlanarPathCurveKind::Arc;
  return false;
}
bool straight(const Sketch3D& owner, const SpatialConstraintTarget& t) {
  if (const auto* c = std::get_if<SpatialLocalCurve>(&t.source()))
    return owner.find_line(c->entity);
  if (const auto* c = std::get_if<SpatialExternalCurve>(&t.source()))
    return c->reference.sketch_3d_curve_kind() == Sketch3DPathCurveKind::Line ||
           c->reference.planar_curve_kind() == PlanarPathCurveKind::Line ||
           c->reference.source_kind() == PathSegmentSourceKind::ConstructionLine;
  return t.kind() == SpatialTargetKind::Axis;
}
} // namespace
SpatialSketchConstraintEditor::SpatialSketchConstraintEditor(
    QWidget* parent, SpatialSketchConstraintCallbacks callbacks)
    : callbacks_(std::move(callbacks)), page_(new QWidget(parent)) {
  auto* column = new QVBoxLayout(page_);
  auto* form = new QFormLayout;
  const auto combo = [&](const char* id, const QString& label) {
    auto* box = new QComboBox(page_);
    box->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(10);
    form->addRow(label, box);
    return box;
  };
  const auto spin = [&](const char* id, const QString& label, double min, double max,
                        double initial) {
    auto* box = new QDoubleSpinBox(page_);
    box->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    box->setDecimals(9);
    box->setRange(min, max);
    box->setValue(initial);
    box->setKeyboardTracking(false);
    form->addRow(label, box);
    connect(box, &QDoubleSpinBox::valueChanged, this, [this] { changed(); });
    new SpatialFormNumberGuard(*this, *box, [this, box] {
      if (box == value_)
        value_changed_ = true;
      if (box == seed_)
        seed_changed_ = true;
      for (std::size_t i = 0; i < 3; ++i)
        if (box == fixed_[i])
          fixed_changed_[i] = true;
      changed();
    });
    return box;
  };
  record_ = combo("constraint", "Edit Constraint");
  kind_ = combo("constraint_kind", "Relation");
  kind_->addItems({"Coincident", "Fixed", "Parallel to Axis", "Parallel to Plane", "Point on Line",
                   "Point on Plane", "Point on Curve", "Point on Face", "Parallel", "Perpendicular",
                   "Tangent (G1)", "Smooth (G2)", "Distance", "Angle", "Radius", "Length"});
  id_ = new QLineEdit(page_);
  id_->setObjectName("blcad.sketch3d.constraint_id");
  form->addRow("Identity", id_);
  for (std::size_t i = 0; i < 3; ++i) {
    auto* box = targets_[i] = new QComboBox(page_);
    const auto id = QStringLiteral("blcad.sketch3d.constraint_target_") + QString::number(i);
    box->setObjectName(id);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(10);
    selectors_[i] = new ShellPropertySelector(
        id + ".selector", QString("Target ") + "ABC"[i], *box, *this,
        [this, i](bool active) {
          stop_picking();
          if (active) {
            pick_slot_ = i;
            selectors_[i]->set_active(true);
          }
          callbacks_.pick_changed();
        },
        page_);
    form->addRow(selectors_[i]);
  }
  hidden_ = new QCheckBox("Include Hidden Inputs", page_);
  hidden_->setObjectName("blcad.sketch3d.constraint_hidden_inputs");
  form->addRow(hidden_);
  connect(hidden_, &QCheckBox::toggled, this, [this] {
    if (picking())
      callbacks_.pick_changed();
  });
  source_ = combo("constraint_source", "Dimension Source");
  value_ = spin("constraint_value", "Value", 1e-9, 1e9, 10);
  angle_ = combo("constraint_angle", "Angle Convention");
  angle_->addItems({"Unsigned (0 … 180°)", "Oriented about Axis (−180 … 180°)"});
  for (int i = 0; i < 3; ++i)
    fixed_[i] = spin((std::string("constraint_fixed_") + "xyz"[i]).c_str(),
                     QString("Fixed ") + "XYZ"[i] + " (mm)", -1e9, 1e9, 0);
  seed_ = spin("constraint_seed", "Curve Branch Seed", 0, 1, .5);
  column->addLayout(form);
  const auto button = [&](const char* id, const QString& label) {
    auto* b = make_gui_button(label, page_);
    b->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    column->addWidget(b);
    return b;
  };
  auto* current = button("constraint_fixed_current", "Copy Current Target Position to Anchor");
  auto* inspect_button =
      button("constraint_inspect", "Inspect Actual Endpoint Tangent / Curvature");
  auto* accept = button("constraint_accept", "Accept Constraint Locally");
  auto* erase = button("constraint_delete", "Delete Constraint Locally");
  auto* discard = button("constraint_discard", "Discard Constraint Form Changes");
  differential_ = new QLabel(page_);
  differential_->setObjectName("blcad.sketch3d.constraint_differential");
  differential_->setWordWrap(true);
  column->addWidget(differential_);
  feedback_ = new QLabel(page_);
  feedback_->setObjectName("blcad.sketch3d.constraint_feedback");
  feedback_->setWordWrap(true);
  column->addWidget(feedback_);
  column->addStretch();
  connect(record_, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading_)
      load();
  });
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading_) {
      mode();
      changed();
    }
  });
  connect(angle_, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading_) {
      mode();
      changed();
    }
  });
  connect(source_, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading_) {
      value_changed_ = true;
      mode();
      changed();
    }
  });
  for (auto* box : targets_)
    connect(box, &QComboBox::currentIndexChanged, this, [this] { changed(); });
  connect(id_, &QLineEdit::textChanged, this, [this] { changed(); });
  connect(value_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (!loading_)
      value_changed_ = true;
  });
  connect(seed_, &QDoubleSpinBox::valueChanged, this, [this] {
    if (!loading_)
      seed_changed_ = true;
  });
  for (std::size_t i = 0; i < 3; ++i)
    connect(fixed_[i], &QDoubleSpinBox::valueChanged, this, [this, i] {
      if (!loading_)
        fixed_changed_[i] = true;
    });
  connect(current, &QPushButton::clicked, this, [this] {
    report([&] {
      take(callbacks_.edit.edit());
      const auto p = take(callbacks_.position(selected(0).target));
      copied_anchor_ = p;
      fixed_changed_ = {};
      const std::array<double, 3> values{p.x, p.y, p.z};
      loading_ = true;
      for (int i = 0; i < 3; ++i)
        fixed_[i]->setValue(values[i]);
      loading_ = false;
      changed();
    });
  });
  connect(inspect_button, &QPushButton::clicked, this, [this] { report([&] { inspect(); }); });
  connect(accept, &QPushButton::clicked, this, [this] { report([&] { stage(); }); });
  connect(erase, &QPushButton::clicked, this, [this] { report([&] { remove(); }); });
  connect(discard, &QPushButton::clicked, this, [this] { load(); });
}
void SpatialSketchConstraintEditor::stop_picking() {
  pick_slot_.reset();
  pick_aliases_.clear();
  for (auto* selector : selectors_)
    if (selector)
      selector->set_active(false);
}
std::vector<geometry::ViewportSceneItem> SpatialSketchConstraintEditor::pick_presentations() {
  pick_aliases_.clear();
  std::vector<geometry::ViewportSceneItem> items;
  if (!pick_slot_)
    return items;
  std::vector<std::string> sources;
  for (std::size_t i = 0; i < choices_.size(); ++i) {
    const auto& c = choices_[i];
    if (!compatible(c, *pick_slot_) || c.shape.empty() ||
        (!hidden_->isChecked() && !callbacks_.visible(c)))
      continue;
    auto found = std::find(sources.begin(), sources.end(), c.semantic_id);
    if (found != sources.end()) {
      pick_aliases_[static_cast<std::size_t>(found - sources.begin())].second.push_back(i);
      continue;
    }
    auto alias = "spatial-target/" + std::to_string(items.size());
    sources.push_back(c.semantic_id);
    pick_aliases_.push_back({alias, {i}});
    const auto kind =
        c.target.kind() == SpatialTargetKind::Face    ? geometry::ViewportSceneKind::Face
        : c.target.kind() == SpatialTargetKind::Point ? geometry::ViewportSceneKind::Vertex
        : c.target.kind() == SpatialTargetKind::Curve ? geometry::ViewportSceneKind::Edge
                                                      : geometry::ViewportSceneKind::Datum;
    items.push_back({kind, std::move(alias), c.shape});
  }
  return items;
}
bool SpatialSketchConstraintEditor::accept_pick(std::string_view alias,
                                                std::optional<QPointF> pointer) {
  if (!pick_slot_)
    return false;
  const auto found = std::find_if(pick_aliases_.begin(), pick_aliases_.end(),
                                  [&](const auto& entry) { return entry.first == alias; });
  if (found == pick_aliases_.end())
    return false;
  std::optional<std::size_t> choice;
  if (found->second.size() == 1)
    choice = found->second.front();
  else if (pointer) {
    double best = std::numeric_limits<double>::infinity(), second = best;
    for (const auto index : found->second) {
      auto p = callbacks_.position(choices_[index].target);
      auto screen = p ? callbacks_.project(p.value()) : Result<QPointF>::failure(p.error());
      if (!screen)
        continue;
      const auto d =
          std::hypot(screen.value().x() - pointer->x(), screen.value().y() - pointer->y());
      if (d < best) {
        second = best;
        best = d;
        choice = index;
      } else
        second = std::min(second, d);
    }
    if (second - best <= 1)
      choice.reset();
  }
  if (!choice) {
    feedback_->setText(
        "Endpoint selection is ambiguous. Choose Start/End explicitly or change the view.");
    return true;
  }
  const auto slot = *pick_slot_;
  const int row = targets_[slot]->findData(static_cast<int>(*choice));
  stop_picking();
  if (row >= 0)
    targets_[slot]->setCurrentIndex(row);
  callbacks_.pick_changed();
  return true;
}
void SpatialSketchConstraintEditor::report(std::function<void()> operation) {
  try {
    operation();
  } catch (const std::exception& e) {
    feedback_->setText(QString::fromUtf8(e.what()));
  }
}
void SpatialSketchConstraintEditor::changed() {
  if (loading_ || !callbacks_.edit.sketch())
    return;
  pending_ = true;
  auto ready = callbacks_.edit.edit();
  feedback_->setText(ready ? "Constraint form changed. Accept or discard it before previewing."
                           : q(ready.error().message()));
}
std::size_t SpatialSketchConstraintEditor::target_count() const {
  const auto k = static_cast<K>(kind_->currentIndex());
  if (k == K::Fixed || k == K::Radius || k == K::Length)
    return 1;
  return k == K::Angle && angle_->currentIndex() == 1 ? 3 : 2;
}
bool SpatialSketchConstraintEditor::compatible(const SpatialSketchTargetChoice& c,
                                               std::size_t slot) const {
  const auto& t = c.target;
  const auto k = static_cast<K>(kind_->currentIndex());
  switch (k) {
  case K::Coincident:
  case K::Distance:
    return point(t);
  case K::Fixed:
    return point(t) && t.local_entity().has_value();
  case K::AxisParallel:
    return slot == 0 ? whole(t) && c.straight : t.kind() == SpatialTargetKind::Axis;
  case K::PlaneParallel:
    return slot == 0 ? whole(t) && c.straight : t.kind() == SpatialTargetKind::Plane;
  case K::PointOnLine:
    return slot == 0 ? point(t) : direction(c);
  case K::PointOnPlane:
    return slot == 0 ? point(t) : t.kind() == SpatialTargetKind::Plane;
  case K::PointOnCurve:
    return slot == 0 ? point(t) : whole(t);
  case K::PointOnFace:
    return slot == 0 ? point(t) : t.kind() == SpatialTargetKind::Face;
  case K::Parallel:
  case K::Perpendicular:
    return direction(c);
  case K::Tangent:
  case K::Smooth:
    return endpoint(t);
  case K::Angle:
    return slot == 2 ? t.kind() == SpatialTargetKind::Axis : direction(c);
  case K::Radius:
    return whole(t) && c.circular;
  case K::Length:
    return whole(t);
  }
  return false;
}
const SpatialSketchTargetChoice& SpatialSketchConstraintEditor::selected(std::size_t slot) const {
  if (targets_.at(slot)->currentIndex() < 0 || !targets_[slot]->currentData().isValid())
    throw std::runtime_error("Select each compatible target in order.");
  const auto row = targets_.at(slot)->currentData().toInt();
  if (row < 0 || static_cast<std::size_t>(row) >= choices_.size())
    throw std::runtime_error("Select each compatible target in order.");
  return choices_[row];
}
void SpatialSketchConstraintEditor::fill_targets() {
  const bool was_loading = loading_;
  loading_ = true;
  for (std::size_t slot = 0; slot < 3; ++slot) {
    const auto previous = targets_[slot]->currentData();
    targets_[slot]->clear();
    targets_[slot]->addItem("Select Target");
    for (std::size_t i = 0; i < choices_.size(); ++i)
      if (compatible(choices_[i], slot))
        targets_[slot]->addItem(q(choices_[i].label), static_cast<int>(i));
    const int row = targets_[slot]->findData(previous);
    targets_[slot]->setCurrentIndex(row >= 0 ? row : 0);
    selectors_[slot]->setEnabled(slot < target_count());
  }
  loading_ = was_loading;
}
void SpatialSketchConstraintEditor::mode() {
  const bool was_picking = picking();
  stop_picking();
  if (was_picking)
    callbacks_.pick_changed();
  if (!callbacks_.edit.sketch())
    return;
  const auto k = static_cast<K>(kind_->currentIndex());
  const bool dimensional = k == K::Distance || k == K::Angle || k == K::Radius || k == K::Length;
  const bool was_loading = loading_;
  loading_ = true;
  const auto previous = source_->currentData();
  source_->clear();
  source_->addItem("Owned Literal (Explicit Rebind)", QString());
  if (dimensional)
    for (const auto& p : callbacks_.edit.part().parameters())
      if (p.type() == (k == K::Angle ? ParameterType::Angle : ParameterType::Length))
        source_->addItem(q(p.name()) +
                             (p.formula() ? " [formula, read-only]" : " [shared, read-only]"),
                         q(p.id().value()));
  source_->setCurrentIndex(std::max(0, source_->findData(previous)));
  source_->setEnabled(dimensional);
  value_->setRange(k == K::Angle ? (angle_->currentIndex() ? -180 : 0) : 1e-9,
                   k == K::Angle ? 180 : 1e9);
  value_->setEnabled(dimensional && source_->currentData().toString().isEmpty());
  value_->setSuffix(k == K::Angle ? QStringLiteral(" °") : QStringLiteral(" mm"));
  if (const auto* p = callbacks_.edit.part().find_parameter(
          ParameterId(source_->currentData().toString().toStdString())))
    value_->setValue(k == K::Angle ? p->value().degrees() : p->value().millimeters());
  angle_->setEnabled(k == K::Angle);
  seed_->setEnabled(k == K::PointOnCurve);
  for (auto* coordinate : fixed_)
    coordinate->setEnabled(k == K::Fixed);
  fill_targets();
  loading_ = was_loading;
}
void SpatialSketchConstraintEditor::reset() {
  stop_picking();
  pending_ = false;
  loading_ = true;
  record_->clear();
  kind_->setCurrentIndex(0);
  angle_->setCurrentIndex(0);
  value_->setValue(10);
  seed_->setValue(.5);
  for (auto* p : fixed_)
    p->setValue(0);
  loading_ = false;
  refresh();
}
void SpatialSketchConstraintEditor::refresh() {
  if (pending_ || !callbacks_.edit.sketch())
    return;
  const auto previous = record_->currentData();
  loading_ = true;
  record_->clear();
  record_->addItem("New Constraint", QString());
  for (const auto& r : callbacks_.edit.sketch()->constraints())
    record_->addItem(q(r.id().value()) + " / " + q(to_string(r.kind())), q(r.id().value()));
  record_->setCurrentIndex(std::max(0, record_->findData(previous)));
  loading_ = false;
  load();
}
void SpatialSketchConstraintEditor::load() {
  const bool was_picking = picking();
  stop_picking();
  if (was_picking)
    callbacks_.pick_changed();
  if (!callbacks_.edit.sketch())
    return;
  report([&] {
    loading_ = true;
    for (auto* box : targets_)
      box->clear();
    choices_ = take(callbacks_.targets());
    const auto& owner = *callbacks_.edit.sketch();
    const SketchConstraintId id(record_->currentData().toString().toStdString());
    const auto* old = owner.find_constraint(id);
    loaded_ = old ? std::optional<SpatialConstraint>(*old) : std::nullopt;
    id_->setReadOnly(old);
    id_->setText(q(id.value()));
    std::vector<int> retained;
    if (old) {
      kind_->setCurrentIndex(static_cast<int>(old->kind()));
      angle_->setCurrentIndex(static_cast<int>(old->angle_convention()));
      for (std::size_t i = 0; i < old->targets().size(); ++i) {
        const auto& t = old->targets()[i];
        retained.push_back(static_cast<int>(choices_.size()));
        choices_.push_back({"Current target " + std::to_string(i + 1) + " / " +
                                t.referenced_node_id().value_or(
                                    t.local_entity() ? t.local_entity()->value() : "local"),
                            {},
                            t,
                            straight(owner, t),
                            radius(owner, t),
                            {}});
      }
      mode();
      for (std::size_t i = 0; i < retained.size(); ++i)
        targets_[i]->setCurrentIndex(targets_[i]->findData(retained[i]));
      if (old->value()) {
        if (const auto* literal = std::get_if<Quantity>(&old->value()->source())) {
          source_->setCurrentIndex(0);
          value_->setValue(old->kind() == K::Angle ? literal->degrees() : literal->millimeters());
        } else
          source_->setCurrentIndex(
              source_->findData(q(std::get<ParameterId>(old->value()->source()).value())));
      }
      if (old->fixed_position()) {
        const auto p = *old->fixed_position();
        fixed_[0]->setValue(p.x);
        fixed_[1]->setValue(p.y);
        fixed_[2]->setValue(p.z);
      }
      if (old->curve_parameter_seed())
        seed_->setValue(*old->curve_parameter_seed());
    } else {
      for (std::size_t i = 1;; ++i) {
        const auto next = "constraint." + std::to_string(i);
        if (!owner.find_constraint(SketchConstraintId(next))) {
          id_->setText(q(next));
          break;
        }
      }
      mode();
    }
    mode();
    loading_ = false;
    pending_ = false;
    copied_anchor_.reset();
    value_changed_ = seed_changed_ = false;
    fixed_changed_ = {};
    differential_->clear();
    feedback_->setText(
        "Choose compatible ordered targets. Shared dimensions require an explicit source rebind.");
  });
  loading_ = false;
}
void SpatialSketchConstraintEditor::stage() {
  if (value_->isEnabled())
    finish_spatial_number(*value_);
  if (seed_->isEnabled())
    finish_spatial_number(*seed_);
  for (auto* field : fixed_)
    if (field->isEnabled())
      finish_spatial_number(*field);
  take(callbacks_.edit.edit());
  auto candidate = *callbacks_.edit.sketch();
  const SketchConstraintId id(id_->text().trimmed().toStdString());
  const auto kind = static_cast<K>(kind_->currentIndex());
  std::vector<SpatialConstraintTarget> targets;
  for (std::size_t i = 0; i < target_count(); ++i)
    targets.push_back(selected(i).target);
  std::optional<SpatialDimensionValue> dimension;
  std::optional<Point3> fixed;
  std::optional<double> seed;
  if (kind == K::Distance || kind == K::Angle || kind == K::Radius || kind == K::Length) {
    const auto source = source_->currentData().toString();
    if (!source.isEmpty())
      dimension = take(SpatialDimensionValue::parameter(ParameterId(source.toStdString())));
    else if (loaded_ && loaded_->kind() == kind && loaded_->value() && !value_changed_ &&
             std::holds_alternative<Quantity>(loaded_->value()->source()))
      dimension = loaded_->value();
    else
      dimension = take(SpatialDimensionValue::literal(
          take(kind == K::Angle ? Quantity::angle_deg(value_->value())
                                : Quantity::length_mm(value_->value()))));
  }
  if (kind == K::Fixed) {
    Point3 p{fixed_[0]->value(), fixed_[1]->value(), fixed_[2]->value()};
    if (copied_anchor_ || (loaded_ && loaded_->fixed_position())) {
      const auto old = copied_anchor_ ? *copied_anchor_ : *loaded_->fixed_position();
      if (!fixed_changed_[0])
        p.x = old.x;
      if (!fixed_changed_[1])
        p.y = old.y;
      if (!fixed_changed_[2])
        p.z = old.z;
    }
    fixed = p;
  }
  if (kind == K::PointOnCurve)
    seed = loaded_ && loaded_->curve_parameter_seed() && !seed_changed_
               ? *loaded_->curve_parameter_seed()
               : seed_->value();
  auto value = take(SpatialConstraint::create(
      id, kind, targets, dimension, fixed,
      kind == K::Angle ? static_cast<SpatialAngleConvention>(angle_->currentIndex())
                       : SpatialAngleConvention::Unsigned,
      seed));
  const bool existing = !record_->currentData().toString().isEmpty();
  take(existing ? candidate.replace_constraint(value) : candidate.add_constraint(value));
  take(callbacks_.edit.accept(std::move(candidate), callbacks_.edit.resources()));
  pending_ = false;
  refresh();
  record_->setCurrentIndex(record_->findData(q(id.value())));
  feedback_->setText(
      "Constraint accepted locally. Inspect DOF and check the full downstream preview.");
}
void SpatialSketchConstraintEditor::remove() {
  take(callbacks_.edit.edit());
  auto candidate = *callbacks_.edit.sketch();
  const SketchConstraintId id(record_->currentData().toString().toStdString());
  take(candidate.remove_constraint(id));
  take(callbacks_.edit.accept(std::move(candidate), callbacks_.edit.resources()));
  pending_ = false;
  refresh();
  feedback_->setText("Constraint removed locally; source parameters remain unchanged.");
}
void SpatialSketchConstraintEditor::inspect() {
  QString output;
  for (std::size_t i = 0; i < target_count(); ++i) {
    const auto& target = selected(i).target;
    if (!endpoint(target))
      continue;
    const auto jet = take(
        callbacks_.jet(target, target.curve_location() == SpatialCurveLocation::Start ? 0 : 1));
    const double speed2 = dot(jet.first, jet.first);
    if (speed2 <= 1e-24)
      throw std::runtime_error("Selected endpoint is singular.");
    const auto tangent = normalized(jet.first);
    const double tangential = dot(tangent, jet.second);
    const Vector3 curvature{(jet.second.x - tangent.x * tangential) / speed2,
                            (jet.second.y - tangent.y * tangential) / speed2,
                            (jet.second.z - tangent.z * tangential) / speed2};
    output += QString("Target %1: T (%2, %3, %4); normal curvature (%5, %6, %7) 1/mm\n")
                  .arg(i + 1)
                  .arg(tangent.x, 0, 'g', 10)
                  .arg(tangent.y, 0, 'g', 10)
                  .arg(tangent.z, 0, 'g', 10)
                  .arg(curvature.x, 0, 'g', 10)
                  .arg(curvature.y, 0, 'g', 10)
                  .arg(curvature.z, 0, 'g', 10);
  }
  differential_->setText(
      output.isEmpty() ? "Select endpoint targets to inspect actual native derivatives." : output);
}
} // namespace blcad::gui
