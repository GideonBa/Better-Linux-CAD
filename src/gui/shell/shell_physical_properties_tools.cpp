#include "blcad/gui/shell/shell_physical_properties_tools.hpp"
#include "blcad/gui/gui_physical_properties.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace blcad::gui {
namespace {
QString text(const std::string& s) {
  return QString::fromStdString(s);
}
QString number(double value) {
  return QString::number(value, 'g', 12);
}
QString point(Point3 p) {
  return "(" + number(p.x) + ", " + number(p.y) + ", " + number(p.z) + ")";
}
QString vector(Vector3 p) {
  return point({p.x, p.y, p.z});
}
Result<std::size_t> fail(std::string value) {
  return Result<std::size_t>::failure(
      Error::validation("gui.shell.physical_properties", std::move(value)));
}
bool same_scope(const GuiPhysicalScope& a, const GuiPhysicalScope& b) {
  if (a.body != b.body || a.occurrence.has_value() != b.occurrence.has_value())
    return false;
  if (!a.occurrence)
    return true;
  const auto& x = *a.occurrence;
  const auto& y = *b.occurrence;
  return x.assembly_document == y.assembly_document && x.subassembly_path == y.subassembly_path &&
         x.component_instance == y.component_instance &&
         x.referenced_part_document == y.referenced_part_document;
}
class CentroidOverlay final : public QWidget {
public:
  explicit CentroidOverlay(OcctViewport& viewport) : QWidget(&viewport), viewport_(viewport) {
    setObjectName("blcad.physical.overlay");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
  }
  std::optional<Point3> centroid;
  QString label;
  std::function<bool()> fresh;

protected:
  void paintEvent(QPaintEvent*) override {
    if (!centroid || !fresh || !fresh())
      return;
    const auto screen = viewport_.model_to_screen(*centroid);
    if (!screen)
      return;
    const QPointF center(screen.value().x, screen.value().y);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const std::array colors{QColor(240, 80, 80), QColor(90, 220, 120), QColor(100, 160, 255)};
    const std::array names{"X", "Y", "Z"};
    for (std::size_t i = 0; i < 3; ++i) {
      auto end = *centroid;
      if (i == 0)
        end.x += 1;
      else if (i == 1)
        end.y += 1;
      else
        end.z += 1;
      const auto projected = viewport_.model_to_screen(end);
      if (!projected)
        continue;
      QPointF direction(projected.value().x - center.x(), projected.value().y - center.y());
      const double length = std::hypot(direction.x(), direction.y());
      p.setPen(QPen(colors[i], 2));
      if (length > 1e-6) {
        direction *= 30.0 / length;
        p.drawLine(center, center + direction);
        p.drawText(center + direction + QPointF(3, -3), names[i]);
      } else {
        p.drawEllipse(center, 8, 8);
        p.drawText(center + QPointF(10, -10), names[i]);
      }
    }
    p.setPen(QPen(QColor(255, 196, 64), 2));
    p.setBrush(QColor(255, 196, 64));
    p.drawEllipse(center, 3, 3);
    p.setPen(Qt::white);
    p.drawText(center + QPointF(10, 22), label);
  }

private:
  OcctViewport& viewport_;
};
} // namespace

struct ShellPhysicalPropertiesTools::Impl {
  explicit Impl(ShellWindow& owner) : window(owner) {}
  ShellWindow& window;
  GuiPhysicalPropertiesController controller;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox *scope{}, *length_unit{}, *mass_unit{}, *centroid_kind{};
  QCheckBox* show_centroid{};
  QLabel *status{}, *frame{}, *area{}, *volume{}, *mass{}, *volume_centroid{}, *mass_centroid{},
      *inertia_unit{}, *principals{}, *directions{}, *materials{}, *precision{}, *centroid_status{};
  QTableWidget* inertia{};
  QPushButton *copy{}, *refresh{};
  CentroidOverlay* overlay{};
  std::vector<GuiPhysicalScopeChoice> choices;
  GuiPhysicalScope selected;

  bool ready() const {
    const auto& s = window.session();
    return s.has_document() && s.derived_results_fresh() && !s.part_workflow_active() &&
           !window.active_sketch() &&
           (!s.task().active() ||
            s.task().command_id() == GuiPhysicalPropertiesController::task_id);
  }
  void clear_values(QString message) {
    status->setText(message);
    status->setToolTip(message);
    for (auto* label : {frame, area, volume, mass, volume_centroid, mass_centroid, inertia_unit,
                        principals, directions, materials, precision, centroid_status})
      label->clear();
    inertia->clearContents();
    copy->setEnabled(false);
    overlay->centroid.reset();
    overlay->hide();
  }
  void fill_scopes() {
    const auto available = controller.scopes(window.session());
    const QSignalBlocker block(scope);
    scope->clear();
    choices.clear();
    if (!available)
      return;
    choices = available.value();
    int match = -1;
    for (std::size_t i = 0; i < choices.size(); ++i) {
      scope->addItem(text(choices[i].label));
      scope->setItemData(static_cast<int>(i), text(choices[i].label), Qt::ToolTipRole);
      if (same_scope(choices[i].scope, selected))
        match = static_cast<int>(i);
    }
    scope->setCurrentIndex(match);
  }
  QString material_details(const GuiPhysicalPropertiesView& v) const {
    QString result;
    const auto* catalog = window.session().part_document()
                              ? &window.session().part_document()->engineering_resources()
                          : window.session().project()
                              ? &window.session().project()->engineering_resources()
                              : nullptr;
    for (const auto& material : v.materials) {
      result += text(material.name) + " — " + text(material.key.id().value()) + " @ " +
                text(material.key.revision()) + "\n" +
                (material.density ? number(*material.density) + " " + text(v.density_unit)
                                  : QString("Density unavailable")) +
                "; Bodies: " + QString::number(material.body_count);
      const auto* snapshot = catalog ? catalog->find_material(material.key) : nullptr;
      if (snapshot) {
        const auto& p = snapshot->properties().density ? snapshot->properties().density->provenance
                                                       : snapshot->provenance();
        const char* origin = p.origin() == EngineeringDataOrigin::Normative      ? "Normative"
                             : p.origin() == EngineeringDataOrigin::Manufacturer ? "Manufacturer"
                                                                                 : "User-defined";
        result += "\n" + QString::fromLatin1(origin) + ": " + text(p.source());
        if (p.reference())
          result += " | " + text(*p.reference());
        if (p.uri())
          result += " | " + text(*p.uri());
        if (p.license())
          result += " | " + text(*p.license());
      }
      result += "\n";
    }
    result += "Unassigned Bodies: " + QString::number(v.unassigned_body_count) +
              "; density unavailable: " + QString::number(v.missing_density_body_count);
    return result;
  }
  void present() {
    if (!controller.active())
      return;
    const auto result = controller.view(window.session());
    if (!result) {
      clear_values("Refresh required: " + text(result.error().message()));
      return;
    }
    const auto& v = result.value();
    const bool partial = v.mass_status == geometry::PhysicalMassStatus::Partial;
    status->setText(v.mass_status == geometry::PhysicalMassStatus::Complete ? "Complete mass"
                    : partial ? "Partial mass — known-density subset only"
                    : v.mass_status == geometry::PhysicalMassStatus::Unavailable
                        ? "Mass unavailable — assign material density"
                        : "Mass not applicable — no solid Bodies");
    if (v.known_mass)
      status->setText(status->text() + ": " + number(v.known_mass->mass) + " " + text(v.mass_unit));
    status->setText(status->text() +
                    "\nHidden active results included.\nSuppressed results excluded.");
    status->setToolTip(status->text());
    frame->setText(text(v.scope_label) + "\n" + text(v.coordinate_frame));
    frame->setToolTip(frame->text());
    area->setText(number(v.area) + " " + text(v.area_unit));
    volume->setText(v.solid_body_count ? number(v.volume) + " " + text(v.volume_unit)
                                       : "Not applicable");
    volume_centroid->setText(v.volume_centroid
                                 ? point(*v.volume_centroid) + " " + text(v.length_unit)
                                 : "Not applicable");
    const auto subset = partial ? QString("Known subset: ") : QString{};
    const auto absent = v.mass_status == geometry::PhysicalMassStatus::NotApplicable
                            ? QString("Not applicable")
                            : QString("Unavailable");
    mass->setText(v.known_mass ? subset + number(v.known_mass->mass) + " " + text(v.mass_unit)
                               : absent);
    mass_centroid->setText(
        v.known_mass ? subset + point(v.known_mass->centroid) + " " + text(v.length_unit) : absent);
    inertia_unit->setText((partial ? "Known-subset central inertia (" : "Central inertia (") +
                          text(v.inertia_unit) + ")");
    inertia->clearContents();
    for (int row = 0; row < 3; ++row)
      for (int col = 0; col < 3; ++col) {
        const auto value =
            v.known_mass ? number(v.known_mass->central_inertia[row * 3 + col]) : absent;
        auto* cell = new QTableWidgetItem(value);
        cell->setToolTip(value);
        inertia->setItem(row, col, cell);
      }
    if (v.known_mass) {
      const auto& m = *v.known_mass;
      principals->setText(subset + number(m.principal_moments[0]) + ", " +
                          number(m.principal_moments[1]) + ", " + number(m.principal_moments[2]) +
                          " " + text(v.inertia_unit));
      directions->setText(
          m.principal_axes_unique
              ? subset + "Principal unit directions:\n" + vector(m.principal_directions[0]) + "\n" +
                    vector(m.principal_directions[1]) + "\n" + vector(m.principal_directions[2])
              : subset + "Principal axes non-unique (repeated moments)");
    } else {
      principals->setText(absent);
      directions->setText(absent);
    }
    principals->setToolTip(principals->text());
    directions->setToolTip(directions->text());
    materials->setText(material_details(v));
    materials->setToolTip(materials->text());
    precision->setText(
        "Requested relative integration tolerance: " + number(v.requested_relative_precision) +
        "\nEstimated relative area / volume error: " + number(v.estimated_relative_area_error) +
        " / " + number(v.estimated_relative_volume_error));
    precision->setToolTip(precision->text());
    copy->setEnabled(true);
    overlay->centroid.reset();
    if (centroid_kind->currentIndex() == 0) {
      overlay->centroid = v.canonical.volume_centroid_mm;
      overlay->label = "Volume centroid";
    } else if (v.canonical.known_mass) {
      overlay->centroid = v.canonical.known_mass->centroid_mm;
      overlay->label = partial ? "Known-subset mass centroid" : "Mass centroid";
    }
    centroid_status->setText(overlay->centroid ? overlay->label + "; frame-aligned XYZ"
                                               : "Selected centroid unavailable");
    overlay->setGeometry(window.viewport().rect());
    overlay->setVisible(show_centroid->isChecked() && overlay->centroid.has_value());
    overlay->raise();
    overlay->update();
  }
};

ShellPhysicalPropertiesTools::ShellPhysicalPropertiesTools(ShellWindow& window, ShellRibbon& ribbon,
                                                           int inspect_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction("Physical Properties", this);
  ui.action->setObjectName("blcad.action.physical_properties");
  ribbon.add_button(ribbon.add_group(inspect_tab, "Properties"), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    const auto r = begin();
    if (!r)
      impl_->status->setText(text(r.error().message()));
  });
  ui.dock = new QDockWidget("Physical Properties", &window);
  ui.dock->setObjectName("blcad.shell.physical_properties_task");
  ui.dock->setMinimumWidth(300);
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* column = new QVBoxLayout(content);
  auto label = [&](QWidget* parent, const char* name) {
    auto* result = new QLabel(parent);
    result->setObjectName(QString("blcad.physical.") + name);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    result->setMinimumWidth(0);
    result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result->setMaximumHeight(result->fontMetrics().lineSpacing() * 6);
    return result;
  };
  auto group = [&](const char* name, const char* title) {
    auto* result = new ShellPropertyGroup(QString("blcad.physical.") + name, title, content);
    column->addWidget(result);
    return result;
  };
  auto combo = [&](QFormLayout* form, QWidget* parent, const char* name, const char* title) {
    auto* result = new QComboBox(parent);
    result->setObjectName(QString("blcad.physical.") + name);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    result->setMinimumContentsLength(8);
    form->addRow(title, result);
    return result;
  };
  auto* scope_group = group("scope_group", "Scope and units");
  auto* form = new QFormLayout;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  scope_group->content_layout().addLayout(form);
  ui.scope = combo(form, &scope_group->content(), "scope", "Scope");
  ui.length_unit = combo(form, &scope_group->content(), "length_unit", "Length");
  for (const auto* name : {"mm", "cm", "m"})
    ui.length_unit->addItem(name);
  ui.mass_unit = combo(form, &scope_group->content(), "mass_unit", "Mass");
  ui.mass_unit->addItems({"kg", "g"});
  ui.frame = label(&scope_group->content(), "frame");
  scope_group->content_layout().addWidget(ui.frame);
  auto* totals = group("totals", "Physical totals");
  auto* totals_form = new QFormLayout;
  totals_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  totals->content_layout().addLayout(totals_form);
  auto row = [&](const char* name, const char* title) {
    auto* result = label(&totals->content(), name);
    totals_form->addRow(title, result);
    return result;
  };
  ui.area = row("area", "Surface area");
  ui.volume = row("volume", "Solid volume");
  ui.mass = row("mass", "Mass");
  ui.volume_centroid = row("volume_centroid", "Volume centroid");
  ui.mass_centroid = row("mass_centroid", "Mass centroid");
  auto* moments = group("moments", "Inertia and principal axes");
  ui.inertia_unit = label(&moments->content(), "inertia_unit");
  moments->content_layout().addWidget(ui.inertia_unit);
  ui.inertia = new QTableWidget(3, 3, &moments->content());
  ui.inertia->setObjectName("blcad.physical.inertia");
  ui.inertia->setHorizontalHeaderLabels({"X", "Y", "Z"});
  ui.inertia->setVerticalHeaderLabels({"X", "Y", "Z"});
  ui.inertia->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  ui.inertia->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui.inertia->setMaximumHeight(150);
  ui.inertia->setMinimumWidth(0);
  moments->content_layout().addWidget(ui.inertia);
  ui.principals = label(&moments->content(), "principals");
  moments->content_layout().addWidget(ui.principals);
  ui.directions = label(&moments->content(), "directions");
  moments->content_layout().addWidget(ui.directions);
  auto* sources = group("sources", "Materials and precision");
  ui.materials = label(&sources->content(), "materials");
  sources->content_layout().addWidget(ui.materials);
  ui.precision = label(&sources->content(), "precision");
  sources->content_layout().addWidget(ui.precision);
  auto* centroid = group("centroid", "Centroid presentation");
  auto* centroid_form = new QFormLayout;
  centroid_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  centroid->content_layout().addLayout(centroid_form);
  ui.centroid_kind = combo(centroid_form, &centroid->content(), "centroid_kind", "Centroid");
  ui.centroid_kind->addItems({"Volume centroid", "Mass centroid"});
  ui.centroid_kind->setCurrentIndex(1);
  ui.show_centroid = new QCheckBox("Show centroid triad", &centroid->content());
  ui.show_centroid->setObjectName("blcad.physical.show_centroid");
  centroid->content_layout().addWidget(ui.show_centroid);
  ui.centroid_status = label(&centroid->content(), "centroid_status");
  centroid->content_layout().addWidget(ui.centroid_status);
  column->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll);
  ui.status = label(panel, "status");
  ui.status->setMaximumHeight(ui.status->fontMetrics().lineSpacing() * 4);
  layout->addWidget(ui.status);
  auto* footer = new QHBoxLayout;
  auto button = [&](const char* name, const char* title) {
    auto* b = new QPushButton(title, panel);
    b->setObjectName(QString("blcad.physical.") + name);
    footer->addWidget(b);
    return b;
  };
  ui.refresh = button("refresh", "Refresh");
  ui.copy = button("copy", "Copy");
  auto* close = button("close", "Close");
  layout->addLayout(footer);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  ui.overlay = new CentroidOverlay(window.viewport());
  ui.overlay->fresh = [this] {
    return active() && bool(impl_->controller.view(impl_->window.session()));
  };
  connect(ui.refresh, &QPushButton::clicked, this, [this] { (void)update_query(); });
  connect(ui.copy, &QPushButton::clicked, this, [this] { (void)copy(); });
  connect(close, &QPushButton::clicked, this, [this] { cancel(); });
  connect(ui.scope, &QComboBox::currentIndexChanged, this, [this](int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= impl_->choices.size() || !active())
      return;
    impl_->selected = impl_->choices[i].scope;
    const auto r = impl_->controller.select_scope(impl_->window.session(), impl_->selected);
    if (!r)
      impl_->clear_values(text(r.error().message()));
    else
      impl_->present();
  });
  const auto units = [this] {
    if (!active())
      return;
    const auto r = impl_->controller.set_units(
        {static_cast<GuiPhysicalLengthUnit>(impl_->length_unit->currentIndex()),
         static_cast<GuiPhysicalMassUnit>(impl_->mass_unit->currentIndex())});
    if (!r)
      impl_->clear_values(text(r.error().message()));
    else
      impl_->present();
  };
  connect(ui.length_unit, &QComboBox::currentIndexChanged, this, units);
  connect(ui.mass_unit, &QComboBox::currentIndexChanged, this, units);
  connect(ui.centroid_kind, &QComboBox::currentIndexChanged, this,
          [this] { refresh_presentation(); });
  connect(ui.show_centroid, &QCheckBox::toggled, this, [this] { refresh_presentation(); });
  ui.clear_values({});
  qApp->installEventFilter(this);
  ui.dock->installEventFilter(this);
  window.viewport().installEventFilter(this);
}
ShellPhysicalPropertiesTools::~ShellPhysicalPropertiesTools() {
  delete impl_->overlay;
}
bool ShellPhysicalPropertiesTools::active() const noexcept {
  return impl_->controller.active();
}
Result<std::size_t> ShellPhysicalPropertiesTools::begin() {
  auto& ui = *impl_;
  auto& s = ui.window.session();
  if (active() || !ui.ready() || s.task().active())
    return fail("Finish the current workflow and recompute before Physical Properties");
  if (!s.task().begin(std::string(GuiPhysicalPropertiesController::task_id)))
    return fail("Could not reserve Physical Properties");
  const auto r = ui.controller.begin(s);
  if (!r) {
    (void)s.task().cancel();
    return r;
  }
  (void)s.task().begin_parameter_editing();
  ui.selected = {};
  const QSignalBlocker length_block(ui.length_unit), mass_block(ui.mass_unit);
  ui.length_unit->setCurrentIndex(static_cast<int>(ui.controller.units().length));
  ui.mass_unit->setCurrentIndex(static_cast<int>(ui.controller.units().mass));
  const auto query = update_query();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return query;
}
Result<std::size_t> ShellPhysicalPropertiesTools::update_query() {
  auto& ui = *impl_;
  const auto result = ui.controller.refresh(ui.window.session());
  ui.fill_scopes();
  if (!result)
    ui.clear_values(text(result.error().message()));
  else
    ui.present();
  return result;
}
Result<std::size_t> ShellPhysicalPropertiesTools::copy() {
  auto& ui = *impl_;
  const auto result = ui.controller.clipboard_text(ui.window.session());
  if (!result) {
    ui.clear_values(text(result.error().message()));
    return Result<std::size_t>::failure(result.error());
  }
  QApplication::clipboard()->setText(text(result.value()));
  return Result<std::size_t>::success(result.value().size());
}
void ShellPhysicalPropertiesTools::cancel() {
  auto& ui = *impl_;
  if (!active())
    return;
  ui.controller.cancel();
  if (ui.window.session().task().command_id() == GuiPhysicalPropertiesController::task_id)
    (void)ui.window.session().task().cancel();
  ui.clear_values({});
  ui.choices.clear();
  ui.scope->clear();
  ui.dock->hide();
  ui.window.refresh();
}
void ShellPhysicalPropertiesTools::refresh() {
  auto& ui = *impl_;
  if (active() &&
      ui.window.session().task().command_id() != GuiPhysicalPropertiesController::task_id) {
    cancel();
    return;
  }
  ui.action->setEnabled(!active() && ui.ready() && !ui.window.session().task().active());
  ui.action->setToolTip(
      ui.window.session().history_prefix_count()
          ? "Apply or cancel End of Part before inspecting complete physical properties."
          : "Inspect complete physical properties without editing the model.");
  ui.refresh->setEnabled(active() && ui.ready());
  if (active())
    ui.present();
}
void ShellPhysicalPropertiesTools::refresh_presentation() {
  if (active())
    impl_->present();
}
bool ShellPhysicalPropertiesTools::eventFilter(QObject* watched, QEvent* event) {
  auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && event->type() == QEvent::KeyPress && widget &&
      (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  if (active() && watched == &impl_->window.viewport() && event->type() == QEvent::Resize)
    refresh_presentation();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
