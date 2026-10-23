#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <array>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "sketch3d.edit";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
bool has_entity(const Sketch3D& sketch, const SketchEntityId& id) {
  return sketch.find_point(id) || sketch.find_line(id) || sketch.find_polyline(id) ||
         sketch.find_arc(id) || sketch.find_spline(id) || sketch.find_helix(id) ||
         sketch.find_guide_curve(id);
}
SketchEntityId next_entity(const Sketch3D& sketch, const char* prefix) {
  for (std::size_t index = 1;; ++index) {
    SketchEntityId id(std::string(prefix) + "." + std::to_string(index));
    if (!has_entity(sketch, id))
      return id;
  }
}
} // namespace

struct ShellSketch3DTools::Impl {
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QLabel* title{};
  QComboBox* point{};
  std::array<QDoubleSpinBox*, 3> coordinates{};
  QCheckBox* detach{};
  QComboBox* start{};
  QComboBox* end{};
  QLabel* summary{};
  QPushButton* apply{};
  std::optional<Sketch3D> draft;
  bool editing{false};
  bool changed{false};
  bool pending_point{false};
  bool previewed{false};
  std::size_t revision{};

  bool available() const {
    const auto& session = window.session();
    return session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() && !session.task().active();
  }
  bool current() const {
    const auto& session = window.session();
    return draft && session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.presentation_revision() == revision;
  }
  void invalidate() {
    previewed = false;
    apply->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void select_point() {
    if (!current())
      return;
    invalidate();
    const auto* selected =
        draft->find_point(SketchEntityId(point->currentData().toString().toStdString()));
    const std::array<const SketchCoordinate3D*, 3> values{selected ? &selected->x() : nullptr,
                                                          selected ? &selected->y() : nullptr,
                                                          selected ? &selected->z() : nullptr};
    for (int axis = 0; axis < 3; ++axis) {
      QSignalBlocker blocker(coordinates[axis]);
      double value = 0;
      if (values[axis])
        value = values[axis]->explicit_coordinate()
                    ? values[axis]->explicit_coordinate()->millimeters()
                    : window.session()
                          .part_document()
                          ->find_parameter(*values[axis]->parameter())
                          ->value()
                          .millimeters();
      coordinates[axis]->setValue(value);
    }
    QSignalBlocker blocker(detach);
    detach->setChecked(false);
    detach->setVisible(selected && !selected->parameter_dependencies().empty());
    pending_point = false;
    summary->setText(
        QStringLiteral("Punkte/Linien lokal übernehmen, dann die gesamte Vorschau prüfen."));
  }
  void fill_points(const SketchEntityId& selected) {
    const auto previous_start = start->currentData();
    const auto previous_end = end->currentData();
    QSignalBlocker point_blocker(point), start_blocker(start), end_blocker(end);
    point->clear();
    start->clear();
    end->clear();
    point->addItem(QStringLiteral("Neuer Punkt"), QString());
    for (const auto& item : draft->points()) {
      const auto id = text(item.id().value());
      point->addItem(id, id);
      start->addItem(id, id);
      end->addItem(id, id);
    }
    point->setCurrentIndex(std::max(0, point->findData(text(selected.value()))));
    const auto restore = [](QComboBox* combo, const QVariant& previous, int fallback) {
      const int row = combo->findData(previous);
      combo->setCurrentIndex(row >= 0 ? row : fallback);
    };
    restore(start, previous_start, start->count() ? 0 : -1);
    restore(end, previous_end, end->count() - 1);
    select_point();
  }
  Result<std::size_t> stage_point() {
    if (!current())
      return Result<std::size_t>::failure(
          error("3D-Skizze neu öffnen: Modell oder Befehl geändert."));
    for (auto* spin : coordinates)
      spin->interpretText();
    const SketchPoint3DValue value{coordinates[0]->value(), coordinates[1]->value(),
                                   coordinates[2]->value()};
    auto id = SketchEntityId(point->currentData().toString().toStdString());
    const auto* selected = draft->find_point(id);
    if (selected && !selected->parameter_dependencies().empty() && !detach->isChecked())
      return Result<std::size_t>::failure(
          error("Parameterbindung nur mit ausdrücklicher Zustimmung durch XYZ-Werte ersetzen."));
    if (!selected)
      id = next_entity(*draft, "point");
    auto candidate = selected ? Sketch3DInteractionService::move_point(*draft, id, value)
                              : Sketch3DInteractionService::add_point(*draft, id, value);
    if (!candidate)
      return Result<std::size_t>::failure(candidate.error());
    draft = std::move(candidate.value().sketch);
    changed = true;
    fill_points(id);
    summary->setText(
        QStringLiteral("Punkt lokal übernommen. %1 Punkte, %2 Linien. Noch nicht gespeichert.")
            .arg(draft->points().size())
            .arg(draft->lines().size()));
    return Result<std::size_t>::success(draft->entity_count());
  }
  Result<std::size_t> stage_line() {
    if (!current() || pending_point)
      return Result<std::size_t>::failure(
          error("Punktänderung zuerst lokal übernehmen oder neu auswählen."));
    invalidate();
    auto line = SketchLine3D::create(next_entity(*draft, "line"),
                                     SketchEntityId(start->currentData().toString().toStdString()),
                                     SketchEntityId(end->currentData().toString().toStdString()));
    if (!line)
      return Result<std::size_t>::failure(line.error());
    auto candidate = *draft;
    auto added = candidate.add_line(std::move(line.value()));
    if (!added)
      return added;
    draft = std::move(candidate);
    changed = true;
    invalidate();
    summary->setText(
        QStringLiteral("Linie lokal übernommen. %1 Punkte, %2 Linien. Vorschau prüfen.")
            .arg(draft->points().size())
            .arg(draft->lines().size()));
    return added;
  }
  Result<std::size_t> commit(GuiDocumentSession& session) const {
    return session.commit_part_transaction(editing ? "Edit 3D sketch" : "Create 3D sketch",
                                           [sketch = *draft, update = editing](PartDocument& part) {
                                             return update ? part.update_sketch_3d(sketch)
                                                           : part.add_sketch_3d(sketch);
                                           });
  }
};

ShellSketch3DTools::ShellSketch3DTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("3D-Skizze"), this);
  ui.action->setObjectName("blcad.action.sketch_3d");
  ribbon.add_button(ribbon.add_group(model_tab, QStringLiteral("Räumliche Skizze")), ui.action);
  ui.dock = new QDockWidget(QStringLiteral("3D-Skizze: Punkte und Linien"), &window);
  ui.dock->setObjectName("blcad.shell.sketch3d_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* inputs = new QWidget(scroll);
  auto* column = new QVBoxLayout(inputs);
  ui.title = new QLabel(inputs);
  ui.title->setWordWrap(true);
  column->addWidget(ui.title);
  auto* hint = new QLabel(
      QStringLiteral("XYZ in Modellkoordinaten (mm). Punkte und Linien werden zunächst nur lokal "
                     "übernommen. Anwenden speichert die geprüfte 3D-Skizze in einem Schritt."),
      inputs);
  hint->setWordWrap(true);
  column->addWidget(hint);
  auto* form = new QFormLayout;
  const auto combo = [&](const char* id, const QString& label) {
    auto* result = new QComboBox(inputs);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    result->setMinimumContentsLength(10);
    form->addRow(label, result);
    return result;
  };
  ui.point = combo("point", QStringLiteral("Punkt bearbeiten"));
  for (int axis = 0; axis < 3; ++axis) {
    auto* spin = new QDoubleSpinBox(inputs);
    spin->setObjectName(QStringLiteral("blcad.sketch3d.") + "xyz"[axis]);
    spin->setDecimals(9);
    spin->setRange(-1e9, 1e9);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    ui.coordinates[axis] = spin;
    form->addRow(QString("XYZ"[axis]), spin);
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this] {
      if (!impl_->current())
        return;
      impl_->pending_point = true;
      impl_->invalidate();
      impl_->summary->setText(
          QStringLiteral("Punktänderung zuerst mit „Punkt übernehmen“ bestätigen."));
    });
  }
  ui.detach = new QCheckBox(QStringLiteral("Parameterbindungen durch XYZ-Werte ersetzen"), inputs);
  ui.detach->setObjectName("blcad.sketch3d.detach");
  form->addRow(ui.detach);
  const auto stage_button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, inputs);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    form->addRow(result);
    return result;
  };
  auto* stage_point = stage_button("stage_point", QStringLiteral("Punkt übernehmen"));
  ui.start = combo("start", QStringLiteral("Linienstart"));
  ui.end = combo("end", QStringLiteral("Linienende"));
  auto* stage_line = stage_button("stage_line", QStringLiteral("Linie ergänzen"));
  column->addLayout(form);
  column->addStretch();
  scroll->setWidget(inputs);
  layout->addWidget(scroll, 1);
  ui.summary = new QLabel(panel);
  ui.summary->setObjectName("blcad.sketch3d.summary");
  ui.summary->setWordWrap(true);
  layout->addWidget(ui.summary);
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    buttons->addWidget(result);
    return result;
  };
  auto* preview_button = button("preview", QStringLiteral("Vorschau"));
  ui.apply = button("apply", QStringLiteral("Anwenden"));
  auto* cancel_button = button("cancel", QStringLiteral("Abbrechen"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  const auto report = [this](Result<std::size_t> result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(ui.action, &QAction::triggered, this, [this, report] { report(begin()); });
  connect(stage_point, &QPushButton::clicked, this,
          [this, report] { report(impl_->stage_point()); });
  connect(stage_line, &QPushButton::clicked, this, [this, report] { report(impl_->stage_line()); });
  connect(preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
  connect(ui.point, &QComboBox::currentIndexChanged, this, [this] { impl_->select_point(); });
  connect(ui.detach, &QCheckBox::toggled, this, [this] { impl_->invalidate(); });
  qApp->installEventFilter(this);
}
ShellSketch3DTools::~ShellSketch3DTools() {
  qApp->removeEventFilter(this);
  cancel();
}
bool ShellSketch3DTools::active() const noexcept {
  return impl_->draft.has_value();
}
Result<std::size_t> ShellSketch3DTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("3D-Skizze benötigt ein aktuelles Bauteil ohne aktiven Befehl."));
  const auto& part = *ui.window.session().part_document();
  for (std::size_t index = 1;; ++index) {
    const std::string id = "sketch3d." + std::to_string(index);
    if (part.dependency_graph().has_node(id))
      continue;
    auto created = Sketch3D::create(Sketch3DId(id), "3D-Skizze " + std::to_string(index));
    if (!created)
      return Result<std::size_t>::failure(created.error());
    return open(std::move(created.value()), false);
  }
}
Result<std::size_t> ShellSketch3DTools::begin_edit(Sketch3DId id) {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("3D-Skizze benötigt ein aktuelles Bauteil ohne aktiven Befehl."));
  const auto* source = ui.window.session().part_document()->find_sketch_3d(id);
  if (!source)
    return Result<std::size_t>::failure(error("3D-Skizze nicht gefunden."));
  return open(*source, true);
}
Result<std::size_t> ShellSketch3DTools::open(Sketch3D sketch, bool editing) {
  auto& ui = *impl_;
  auto& session = ui.window.session();
  (void)session.task().begin(std::string(task_id));
  (void)session.task().begin_parameter_editing();
  ui.revision = session.presentation_revision();
  ui.draft = std::move(sketch);
  ui.editing = editing;
  ui.changed = false;
  ui.title->setText(text(ui.draft->name()) + " / " + text(ui.draft->id().value()));
  ui.fill_points(editing && !ui.draft->points().empty() ? ui.draft->points().front().id()
                                                        : SketchEntityId{});
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.draft->entity_count());
}
Result<std::size_t> ShellSketch3DTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(
        error("3D-Skizze neu öffnen: Modell oder Befehl geändert."));
  }
  for (auto* spin : ui.coordinates)
    spin->interpretText();
  ui.invalidate();
  if (!ui.changed || ui.pending_point || ui.draft->points().empty())
    return Result<std::size_t>::failure(error("Punkte/Linien zuerst lokal übernehmen."));
  // The complete session copy validates catalogs, reference connectivity and
  // downstream Geometry without publishing history, dirty state or file I/O.
  auto candidate = ui.window.session();
  auto checked = ui.commit(candidate);
  if (!checked)
    return checked;
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*candidate.part_document(),
                                                           *candidate.part_shape_cache());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  const auto prefix = "sketch3d/" + ui.draft->id().value() + "/";
  for (auto& item : scene.value())
    if (item.semantic_id.starts_with(prefix) || item.kind == geometry::ViewportSceneKind::Path ||
        item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody) {
      replaced.push_back(item.semantic_id);
      shapes.push_back(std::move(item.shape));
    }
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  ui.previewed = true;
  ui.apply->setEnabled(true);
  (void)ui.window.session().task().show_preview();
  ui.summary->setText(QStringLiteral("Vorschau gültig: %1 Punkte, %2 Linien. Abhängige Pfade und "
                                     "Körper neu berechnet. Anwenden übernimmt einen Schritt.")
                          .arg(ui.draft->points().size())
                          .arg(ui.draft->lines().size()));
  return Result<std::size_t>::success(ui.draft->entity_count());
}
Result<std::size_t> ShellSketch3DTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed) {
    if (!ui.current())
      cancel();
    return Result<std::size_t>::failure(error("Zuerst eine aktuelle Vorschau prüfen."));
  }
  auto checked = preview();
  if (!checked)
    return checked;
  auto result = ui.commit(ui.window.session());
  cancel();
  return result;
}
void ShellSketch3DTools::cancel() {
  auto& ui = *impl_;
  if (!ui.draft)
    return;
  ui.draft.reset();
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.dock->hide();
  ui.window.refresh();
}
void ShellSketch3DTools::refresh() {
  auto& ui = *impl_;
  if (ui.draft && !ui.current()) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
bool ShellSketch3DTools::eventFilter(QObject* watched, QEvent* event) {
  auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && event->type() == QEvent::KeyPress && widget &&
      (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
