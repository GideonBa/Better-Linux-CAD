#include "advanced_loft_controls.hpp"
#include "blcad/core/surface_feature.hpp"
#include "scroll_form_panel.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
namespace blcad::gui {
namespace {
QString number(double value) {
  return QString::number(value, 'g', QLocale::FloatingPointShortest);
}
std::string key(const PathCurveId& guide, const ProfileSectionReference& section) {
  return std::to_string(guide.value().size()) + ":" + guide.value() + section.identity();
}
QComboBox* combo(QWidget* parent) {
  auto* box = new QComboBox(parent);
  box->setMinimumWidth(80);
  box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  QObject::connect(box, &QComboBox::currentTextChanged, box,
                   [box](const QString& text) { box->setToolTip(text); });
  return box;
}
Result<std::optional<LoftConstraintIntent>> fail(const char* message) {
  return Result<std::optional<LoftConstraintIntent>>::failure(
      Error::validation("loft.constraints", message));
}
QString face_key(const FaceReference& face) {
  return QString::fromStdString(loft_support_key(face));
}
} // namespace
AdvancedLoftControls::AdvancedLoftControls(QWidget* parent) {
  panel = new ScrollFormPanel(parent);
  panel->setObjectName("blcad.loft.advanced");
  auto* form = new QFormLayout(panel);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  enabled = new QCheckBox("Explicit guide anchors and boundary continuity", panel);
  enabled->setObjectName("blcad.loft.constraints");
  form->addRow(enabled);
  include_hidden = new QCheckBox("Include hidden support Bodies", panel);
  include_hidden->setObjectName("blcad.loft.include_hidden");
  form->addRow(include_hidden);
  tolerance_binding = combo(panel);
  tolerance_binding->setObjectName("blcad.loft.tolerance_binding");
  form->addRow("Linear Tolerance Binding", tolerance_binding);
  const auto field = [&](const char* id, const char* title, const char* value) {
    auto* line = new QLineEdit(value, panel);
    line->setObjectName(QString("blcad.loft.") + id);
    form->addRow(title, line);
    QObject::connect(line, &QLineEdit::textChanged, panel, [this] {
      if (!loading && changed)
        changed();
    });
    return line;
  };
  linear = field("linear_tolerance", "Linear Tolerance (mm)", "0.00001");
  angular = field("angular_tolerance", "Angular Tolerance (rad)", "0.0001");
  curvature = field("curvature_tolerance", "Curvature Tolerance (1/mm)", "0.0001");
  anchors = new QTableWidget(0, 5, panel);
  anchors->setObjectName("blcad.loft.anchors");
  anchors->setHorizontalHeaderLabels(
      {"Guide", "Section", "Boundary Entity", "Boundary Fraction", "Guide Fraction"});
  for (int column : {3, 4})
    anchors->horizontalHeaderItem(column)->setToolTip(
        "Normalized true curve length fraction in [0,1]");
  anchors->setMinimumHeight(180);
  anchors->setMaximumHeight(250);
  anchors->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  anchors->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  form->addRow(new QLabel("Explicit Incidence Anchors", panel));
  form->addRow(anchors);
  auto* hint = new QLabel("Fractions use true curve length. Choose each original "
                          "boundary entity and guide station explicitly. Preview checks "
                          "incidence; no nearest intersection is guessed.",
                          panel);
  hint->setWordWrap(true);
  form->addRow(hint);
  supports = new QTableWidget(0, 8, panel);
  supports->setObjectName("blcad.loft.supports");
  supports->setHorizontalHeaderLabels({"Scope", "Terminal Section", "Support Face", "From Guide",
                                       "To Guide", "Continuity", "Reverse Normal",
                                       "Tangent Scale"});
  supports->setMinimumHeight(160);
  supports->setMaximumHeight(230);
  supports->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  supports->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  form->addRow(new QLabel("Boundary Supports", panel));
  form->addRow(supports);
  auto* row = new QWidget(panel);
  auto* buttons = new QHBoxLayout(row);
  buttons->setContentsMargins(0, 0, 0, 0);
  add_support = new QPushButton("Add Support", row);
  add_support->setObjectName("blcad.loft.add_support");
  remove_support = new QPushButton("Remove Support", row);
  remove_support->setObjectName("blcad.loft.remove_support");
  buttons->addWidget(add_support);
  buttons->addWidget(remove_support);
  form->addRow(row);
  feedback = new QLabel(panel);
  feedback->setObjectName("blcad.loft.measurements");
  feedback->setWordWrap(true);
  feedback->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  form->addRow(feedback);
  repair = new QPushButton("Repair References", panel);
  repair->setObjectName("blcad.loft.repair");
  form->addRow(repair);
  QObject::connect(enabled, &QCheckBox::toggled, panel, [this] {
    configure();
    if (!loading && changed)
      changed();
  });
  QObject::connect(tolerance_binding, &QComboBox::currentIndexChanged, panel, [this] {
    linear->setReadOnly(tolerance_binding->currentIndex() > 0);
    if (tolerance_binding->currentIndex() > 0)
      linear->setText(tolerance_binding->currentData(Qt::UserRole + 1).toString());
    if (!loading && changed)
      changed();
  });
  QObject::connect(add_support, &QPushButton::clicked, panel, [this] {
    loading = true;
    add_boundary();
    loading = false;
    if (changed)
      changed();
  });
  QObject::connect(remove_support, &QPushButton::clicked, panel, [this] {
    supports->removeRow(supports->currentRow());
    if (changed)
      changed();
  });
  for (auto* table : {anchors, supports})
    QObject::connect(table, &QTableWidget::itemChanged, panel, [this] {
      if (!loading && changed)
        changed();
    });
}
void AdvancedLoftControls::configure() {
  for (QWidget* w : std::vector<QWidget*>{tolerance_binding, linear, angular, curvature, anchors,
                                          supports, add_support, remove_support, include_hidden})
    w->setEnabled(enabled->isChecked());
}
void AdvancedLoftControls::reset(const PartDocument& part) {
  loading = true;
  enabled->setChecked(false);
  include_hidden->setChecked(false);
  faces.clear();
  sections.clear();
  guides.clear();
  reversals.clear();
  anchor_drafts.clear();
  anchors->setRowCount(0);
  supports->setRowCount(0);
  feedback->clear();
  tolerance_binding->clear();
  tolerance_binding->addItem("New Independent Value", QString());
  for (const auto& p : part.parameters())
    if (p.type() == ParameterType::Length) {
      tolerance_binding->addItem(QString::fromStdString(p.name()),
                                 QString::fromStdString(p.id().value()));
      tolerance_binding->setItemData(tolerance_binding->count() - 1,
                                     number(p.value().millimeters()), Qt::UserRole + 1);
    }
  linear->setReadOnly(false);
  linear->setText("0.00001");
  angular->setText("0.0001");
  curvature->setText("0.0001");
  loading = false;
  configure();
}
void AdvancedLoftControls::remember_anchors() {
  for (int r = 0; r < anchors->rowCount(); ++r) {
    auto* e = qobject_cast<QComboBox*>(anchors->cellWidget(r, 2));
    anchor_drafts[anchors->item(r, 0)->data(Qt::UserRole).toString().toStdString()] = {
        e->currentData().toString(), anchors->item(r, 3)->text(), anchors->item(r, 4)->text()};
  }
}
void AdvancedLoftControls::sync(const PartDocument& part, std::vector<ProfileSectionReference> ns,
                                std::vector<PathCurveId> ng, std::vector<bool> nr) {
  const auto identities = [](const auto& values) {
    std::vector<std::string> ids;
    for (const auto& v : values)
      ids.push_back(v.identity());
    return ids;
  };
  if (identities(sections) == identities(ns) && guides == ng &&
      anchors->rowCount() == int(ns.size() * ng.size())) {
    sections = std::move(ns);
    reversals = std::move(nr);
    return;
  }
  remember_anchors();
  loading = true;
  sections = std::move(ns);
  guides = std::move(ng);
  reversals = std::move(nr);
  anchors->setRowCount(0);
  for (const auto& g : guides)
    for (std::size_t i = 0; i < sections.size(); ++i) {
      const auto& s = sections[i];
      const auto k = key(g, s);
      const auto d = anchor_drafts.contains(k) ? anchor_drafts.at(k)
                                               : AnchorDraft{"", "0",
                                                             i == 0                     ? "0"
                                                             : i + 1 == sections.size() ? "1"
                                                                                        : ""};
      const int r = anchors->rowCount();
      anchors->insertRow(r);
      const auto* p = part.find_path_curve(g);
      auto* label = new QTableWidgetItem(p ? QString::fromStdString(p->name()) : "Missing Guide");
      label->setData(Qt::UserRole, QString::fromStdString(k));
      label->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      anchors->setItem(r, 0, label);
      label = new QTableWidgetItem(QString("Section %1").arg(i + 1));
      label->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      anchors->setItem(r, 1, label);
      auto* e = combo(anchors);
      e->addItem("Whole Boundary", QString());
      if (s.kind() == LoftSectionKind::ClosedRegion) {
        const auto& ref = std::get<ProfileRegionReference>(s.source());
        const auto* sketch = part.find_sketch(ref.sketch());
        const auto* profile = sketch ? sketch->find_arc_closed_profile(ref.profile()) : nullptr;
        if (profile)
          for (const auto& id : profile->curve_segments())
            e->addItem(QString::fromStdString(id.value()), QString::fromStdString(id.value()));
        const auto* composite =
            sketch ? sketch->find_composite_closed_profile(ref.profile()) : nullptr;
        if (composite)
          for (const auto& id : composite->outer_contour())
            e->addItem(QString::fromStdString(id.value()), QString::fromStdString(id.value()));
      }
      int at = e->findData(d.entity);
      if (at < 0) {
        e->addItem("Saved Entity (repair required)", d.entity);
        at = e->count() - 1;
      }
      e->setCurrentIndex(at);
      anchors->setCellWidget(r, 2, e);
      anchors->setItem(r, 3, new QTableWidgetItem(d.boundary));
      anchors->setItem(r, 4, new QTableWidgetItem(d.guide));
      QObject::connect(e, &QComboBox::currentIndexChanged, panel, [this] {
        if (!loading && changed)
          changed();
      });
    }
  for (int r = 0; r < supports->rowCount(); ++r)
    for (int c : {1, 3, 4}) {
      auto* b = qobject_cast<QComboBox*>(supports->cellWidget(r, c));
      const auto previous = b->currentData();
      const auto label = b->currentText();
      b->clear();
      if (c == 1) {
        for (std::size_t i = 0; i < sections.size(); ++i)
          b->addItem(QString("Section %1").arg(i + 1),
                     QString::fromStdString(sections[i].identity()));
      } else
        for (const auto& g : guides) {
          const auto* p = part.find_path_curve(g);
          b->addItem(p ? QString::fromStdString(p->name()) : "Missing Guide",
                     QString::fromStdString(g.value()));
        }
      int at = b->findData(previous);
      if (at < 0 && !previous.toString().isEmpty()) {
        b->addItem(label + " (unavailable)", previous);
        at = b->count() - 1;
      }
      b->setCurrentIndex(at);
    }
  loading = false;
}
void AdvancedLoftControls::add_boundary(const LoftBoundaryCondition* condition,
                                        const LoftPatchBoundaryCondition* patch, int scope) {
  const int r = supports->rowCount();
  supports->insertRow(r);
  for (int c = 0; c < 7; ++c) {
    if (c == 6) {
      auto* check = new QTableWidgetItem;
      check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
      check->setCheckState(condition && condition->reverse_normal ? Qt::Checked : Qt::Unchecked);
      supports->setItem(r, c, check);
      continue;
    }
    auto* b = combo(supports);
    supports->setCellWidget(r, c, b);
    if (c == 0) {
      b->setMinimumContentsLength(12);
      b->addItems({"Whole Start", "Whole End", "Scoped Patch"});
      b->setCurrentIndex(patch ? 2 : scope);
    }
    if (c == 1)
      for (std::size_t i = 0; i < sections.size(); ++i)
        b->addItem(QString("Section %1").arg(i + 1),
                   QString::fromStdString(sections[i].identity()));
    if (c == 2)
      for (std::size_t i = 0; i < faces.size(); ++i)
        if (faces[i].available)
          b->addItem(QString::fromStdString(faces[i].target.label), int(i));
    if (c == 3 || c == 4)
      for (const auto& g : guides)
        b->addItem(QString::fromStdString(g.value()), QString::fromStdString(g.value()));
    if (c == 5) {
      b->addItems({"G1", "G2"});
      b->setCurrentIndex(condition && condition->continuity == LoftBoundaryContinuity::G2 ? 1 : 0);
    }
    if (condition && c == 1)
      b->setCurrentIndex(b->findData(QString::fromStdString(condition->section_identity)));
    if (condition && c == 2) {
      const auto stable = face_key(condition->support);
      int at = -1;
      for (std::size_t i = 0; i < faces.size(); ++i)
        if (face_key(std::get<FaceReference>(faces[i].target.reference)) == stable)
          at = i;
      if (at < 0) {
        faces.push_back({{"", "Saved Support (repair or replace)", condition->support, {}, {}},
                         condition->imported_source_body.value_or(BodyId(""))});
        b->addItem("Saved Support (repair or replace)", int(faces.size() - 1));
        at = faces.size() - 1;
      }
      b->setCurrentIndex(b->findData(at));
    }
    if (patch && (c == 3 || c == 4))
      b->setCurrentIndex(b->findData(
          QString::fromStdString((c == 3 ? patch->from_guide : patch->to_guide).value())));
    if (c == 2)
      QObject::connect(b, &QComboBox::currentIndexChanged, panel, [this, b] {
        if (!loading && b->currentIndex() >= 0 && face_selected)
          face_selected(std::size_t(b->currentData().toInt()));
      });
    QObject::connect(b, &QComboBox::currentIndexChanged, panel, [this] {
      if (!loading && changed)
        changed();
    });
  }
  auto* scale = new QTableWidgetItem(condition ? number(condition->tangent_scale) : "1");
  scale->setToolTip("Verified boundary construction requires unit tangent scale.");
  supports->setItem(r, 7, scale);
}
void AdvancedLoftControls::load(const PartDocument& part,
                                const std::optional<LoftConstraintIntent>& intent) {
  loading = true;
  enabled->setChecked(intent.has_value());
  if (intent) {
    tolerance_binding->setCurrentIndex(
        tolerance_binding->findData(QString::fromStdString(intent->linear_tolerance.value())));
    angular->setText(number(intent->angular_tolerance_rad));
    curvature->setText(number(intent->curvature_tolerance_per_mm));
    for (const auto& b : intent->guides)
      for (const auto& a : b.anchors) {
        const auto s = std::find_if(sections.begin(), sections.end(), [&](const auto& s) {
          return s.identity() == a.section_identity;
        });
        if (s != sections.end())
          anchor_drafts[key(b.guide, *s)] = {
              a.boundary_entity ? QString::fromStdString(a.boundary_entity->value()) : QString(),
              number(a.boundary_fraction), number(a.guide_fraction)};
      }
  }
  anchors->setRowCount(0);
  supports->setRowCount(0);
  loading = false;
  sync(part, sections, guides, reversals);
  loading = true;
  if (intent) {
    if (intent->start)
      add_boundary(&*intent->start, nullptr, 0);
    if (intent->end)
      add_boundary(&*intent->end, nullptr, 1);
    for (const auto& patch : intent->patch_boundaries)
      add_boundary(&patch.condition, &patch, 2);
  }
  loading = false;
  configure();
}
Result<std::optional<LoftConstraintIntent>>
AdvancedLoftControls::collect(ParameterId tolerance) const {
  if (!enabled->isChecked())
    return Result<std::optional<LoftConstraintIntent>>::success({});
  bool ok = false;
  const double angle = angular->text().toDouble(&ok);
  if (!ok || !std::isfinite(angle) || angle <= 0)
    return fail("Enter a positive angular tolerance in radians");
  const double curve = curvature->text().toDouble(&ok);
  if (!ok || !std::isfinite(curve) || curve <= 0)
    return fail("Enter a positive curvature tolerance in 1/mm");
  LoftConstraintIntent intent{std::move(tolerance), angle, curve, {}, {}, {}, {}};
  int r = 0;
  for (std::size_t g = 0; g < guides.size(); ++g) {
    LoftGuideBinding binding{guides[g], reversals[g], {}};
    for (const auto& section : sections) {
      const double boundary = anchors->item(r, 3)->text().toDouble(&ok);
      if (!ok || !std::isfinite(boundary) || boundary < 0 || boundary > 1)
        return fail("Each boundary anchor needs a length fraction from 0 to 1");
      const double station = anchors->item(r, 4)->text().toDouble(&ok);
      if (!ok || !std::isfinite(station) || station < 0 || station > 1)
        return fail("Each guide anchor needs an explicit length fraction from 0 to 1");
      const auto entity =
          qobject_cast<QComboBox*>(anchors->cellWidget(r, 2))->currentData().toString();
      binding.anchors.push_back(
          {section.identity(),
           entity.isEmpty() ? std::nullopt : std::optional(SketchEntityId(entity.toStdString())),
           boundary, station});
      ++r;
    }
    intent.guides.push_back(std::move(binding));
  }
  for (int row = 0; row < supports->rowCount(); ++row) {
    const auto box = [&](int c) { return qobject_cast<QComboBox*>(supports->cellWidget(row, c)); };
    if (box(1)->currentIndex() < 0 || box(2)->currentIndex() < 0)
      return fail("Choose a terminal section and owned support face");
    const int f = box(2)->currentData().toInt();
    if (f < 0 || std::size_t(f) >= faces.size())
      return fail("Support face is unavailable");
    const double scale = supports->item(row, 7)->text().toDouble(&ok);
    if (!ok || scale != 1)
      return fail("Verified Loft boundary construction requires tangent scale 1");
    const auto& face = faces[f];
    if (!face.available)
      return fail("Support face is outside the current input scope; include "
                  "hidden inputs or repair it");
    const auto& ref = std::get<FaceReference>(face.target.reference);
    const bool imported = ref.source_kind() == PartFeatureInputSourceKind::ImportedTopology;
    LoftBoundaryCondition condition{box(1)->currentData().toString().toStdString(),
                                    ref,
                                    imported ? std::optional(face.owner) : std::nullopt,
                                    box(5)->currentIndex() == 1 ? LoftBoundaryContinuity::G2
                                                                : LoftBoundaryContinuity::G1,
                                    supports->item(row, 6)->checkState() == Qt::Checked,
                                    scale};
    const int scope = box(0)->currentIndex();
    if (scope == 2) {
      if (box(3)->currentIndex() < 0 || box(4)->currentIndex() < 0)
        return fail("Choose adjacent ordered guides for the scoped patch");
      intent.patch_boundaries.push_back(
          {std::move(condition), PathCurveId(box(3)->currentData().toString().toStdString()),
           PathCurveId(box(4)->currentData().toString().toStdString())});
    } else {
      auto& destination = scope == 0 ? intent.start : intent.end;
      if (destination)
        return fail("A whole terminal boundary has only one support");
      destination = std::move(condition);
    }
  }
  return Result<std::optional<LoftConstraintIntent>>::success(std::move(intent));
}
} // namespace blcad::gui
