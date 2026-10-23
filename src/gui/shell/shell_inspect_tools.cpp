#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "inspect.assembly_analysis";
Error error(std::string message) {
  return Error::validation("gui.shell.inspect", std::move(message));
}
QString number(double value) {
  return QString::number(value, 'g', 12);
}
QTableWidgetItem* occurrence_item(const Project& project,
                                  const std::vector<SubassemblyInstanceId>& path,
                                  const ComponentInstanceId& id, const QString& identity) {
  const AssemblyDocument* assembly = &project.assembly();
  QString label;
  for (const auto& step : path) {
    const auto* instance = assembly ? assembly->find_subassembly_instance(step) : nullptr;
    label += (instance ? QString::fromStdString(instance->name())
                       : QString::fromStdString(step.value())) +
             " / ";
    assembly = instance ? project.find_assembly_document(instance->referenced_assembly_document())
                        : nullptr;
  }
  const auto* component = assembly ? assembly->find_component_instance(id) : nullptr;
  label +=
      component ? QString::fromStdString(component->name()) : QString::fromStdString(id.value());
  auto* item = new QTableWidgetItem(label);
  item->setToolTip(identity);
  item->setData(Qt::UserRole, identity);
  return item;
}
QTableWidgetItem* leaf_item(const Project& project, const geometry::AssemblyLeafIdentity& leaf) {
  return occurrence_item(project, leaf.subassembly_path, leaf.component_instance,
                         QString::fromStdString(leaf.occurrence_key));
}
QTableWidgetItem* contact_item(const Project& project,
                               const AssemblyExchangeComponentOccurrenceIdentity& identity) {
  QString path = QStringLiteral("Wurzel");
  for (const auto& item : identity.containing_assembly_occurrence.occurrence_path)
    path += QStringLiteral(" / ") + QString::fromStdString(item.value());
  return occurrence_item(
      project, identity.containing_assembly_occurrence.occurrence_path, identity.component_instance,
      path + " / " + QString::fromStdString(identity.component_instance.value()));
}
} // namespace

ShellInspectTools::ShellInspectTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab,
                                     int assembly_tab)
    : QObject(&window), window_(window) {
  auto* part_export = ribbon.add_group(model_tab, QStringLiteral("Austausch"));
  auto* assembly_export = ribbon.add_group(assembly_tab, QStringLiteral("Prüfen und exportieren"));
  for (auto [mode, id, label] :
       {std::tuple{GuiStepExportMode::PartMultiBody, "export_part_step", "Bauteil als STEP"},
        std::tuple{GuiStepExportMode::AssemblyFlattened, "export_assembly_flat_step", "STEP flach"},
        std::tuple{GuiStepExportMode::AssemblyStructured, "export_assembly_structured_step",
                   "STEP strukturiert"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + id);
    connect(action, &QAction::triggered, this, [this, mode] { choose_export(mode); });
    ribbon.add_small_button(
        mode == GuiStepExportMode::PartMultiBody ? part_export : assembly_export, action);
    export_actions_.emplace_back(mode, action);
  }
  analysis_action_ = new QAction(QStringLiteral("Baugruppe analysieren"), this);
  analysis_action_->setObjectName("blcad.action.assembly_analysis");
  connect(analysis_action_, &QAction::triggered, this, [this] {
    auto result = begin_analysis();
    if (!result)
      window_.report_error(result.error());
  });
  ribbon.add_small_button(assembly_export, analysis_action_);

  dock_ = new QDockWidget(QStringLiteral("Baugruppenanalyse"), &window);
  dock_->setObjectName("blcad.shell.inspect_task");
  dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
  auto* panel = new QWidget(dock_);
  auto* column = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  scroll->setMinimumWidth(400);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* inputs = new QWidget(scroll);
  auto* layout = new QVBoxLayout(inputs);
  auto* form = new QFormLayout;
  kind_ = new QComboBox(inputs);
  kind_->setObjectName("blcad.inspect.kind");
  kind_->addItems({QStringLiteral("Kollisionen"), QStringLiteral("Mindestabstand"),
                   QStringLiteral("Kontakte"), QStringLiteral("Freiheitsgrade (Wurzelgruppe)")});
  form->addRow(QStringLiteral("Analyse"), kind_);
  clearance_ = new QDoubleSpinBox(inputs);
  clearance_->setObjectName("blcad.inspect.clearance");
  clearance_->setRange(0.000001, 1e6);
  clearance_->setDecimals(6);
  clearance_->setValue(1);
  clearance_->setSuffix(QStringLiteral(" mm"));
  clearance_->setKeyboardTracking(false);
  form->addRow(QStringLiteral("Mindestabstand"), clearance_);
  layout->addLayout(form);
  auto* hint = new QLabel(
      QStringLiteral("Kollisionen, Abstände und Kontakte prüfen sichtbare, aktive Vorkommen in "
                     "ihrer aktuellen "
                     "Position. Für Freiheitsgrade eine zusammenhängende Wurzelgruppe auswählen. "
                     "Die Analyse verändert keine Positionen."),
      inputs);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  group_ = new QListWidget(inputs);
  group_->setObjectName("blcad.inspect.group");
  group_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  group_->setMaximumHeight(140);
  layout->addWidget(group_);
  summary_ = new QLabel(inputs);
  summary_->setObjectName("blcad.inspect.summary");
  summary_->setWordWrap(true);
  summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(summary_);
  results_ = new QTableWidget(0, 3, inputs);
  results_->setObjectName("blcad.inspect.results");
  results_->setHorizontalHeaderLabels(
      {QStringLiteral("Vorkommen A"), QStringLiteral("Vorkommen B"), QStringLiteral("Ergebnis")});
  results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  results_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  results_->setMinimumHeight(180);
  layout->addWidget(results_);
  layout->addStretch();
  scroll->setWidget(inputs);
  column->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  auto* run = new QPushButton(QStringLiteral("Analysieren"), panel);
  run->setObjectName("blcad.inspect.run");
  auto* close = new QPushButton(QStringLiteral("Schließen"), panel);
  close->setObjectName("blcad.inspect.close");
  buttons->addWidget(run);
  buttons->addWidget(close);
  column->addLayout(buttons);
  dock_->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, dock_);
  dock_->hide();
  dock_->installEventFilter(this);
  window.viewport().installEventFilter(this);
  window.installEventFilter(this);
  connect(run, &QPushButton::clicked, this, [this] {
    auto result = run_analysis();
    if (!result) {
      summary_->setText(QString::fromStdString(result.error().message()));
      window_.report_error(result.error());
    }
  });
  connect(close, &QPushButton::clicked, this, [this] { cancel(); });
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] { update_inputs(); });
  connect(clearance_, &QDoubleSpinBox::valueChanged, this, [this] { clear_result(); });
  connect(group_, &QListWidget::itemSelectionChanged, this, [this] { clear_result(); });
  update_inputs();
}

bool ShellInspectTools::idle_and_fresh() const {
  return window_.session().has_document() && window_.session().derived_results_fresh() &&
         !window_.session().task().active() && !window_.active_sketch();
}
Result<std::size_t> ShellInspectTools::begin_analysis() {
  auto& session = window_.session();
  if (!idle_and_fresh() || !session.project())
    return Result<std::size_t>::failure(
        error("Analyse benötigt eine aktuelle Baugruppe ohne aktiven Befehl"));
  group_->clear();
  for (const auto& component : session.project()->assembly().component_instances()) {
    auto* item = new QListWidgetItem(QString::fromStdString(component.name()), group_);
    item->setData(Qt::UserRole, QString::fromStdString(component.id().value()));
  }
  if (!session.task().begin(std::string(task_id)) || !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Analyse konnte nicht gestartet werden"));
  active_ = true;
  revision_ = session.presentation_revision();
  clear_result();
  dock_->show();
  dock_->raise();
  window_.refresh();
  return Result<std::size_t>::success(1);
}
void ShellInspectTools::clear_result() {
  results_->setRowCount(0);
  summary_->setText(QStringLiteral("Analyse wählen und Analysieren klicken."));
}
void ShellInspectTools::update_inputs() {
  clearance_->setEnabled(kind_->currentIndex() == 1);
  group_->setVisible(kind_->currentIndex() == 3);
  clear_result();
}
void ShellInspectTools::add_row(QTableWidgetItem* first, QTableWidgetItem* second, QString result) {
  const int row = results_->rowCount();
  results_->insertRow(row);
  results_->setItem(row, 0, first);
  results_->setItem(row, 1, second);
  auto* value = new QTableWidgetItem(result);
  value->setToolTip(result);
  results_->setItem(row, 2, value);
  results_->resizeRowToContents(row);
}
Result<std::size_t> ShellInspectTools::run_analysis() {
  clear_result();
  auto& session = window_.session();
  if (!active_ || session.task().command_id() != task_id || !session.project() ||
      !session.derived_results_fresh() || session.presentation_revision() != revision_)
    return Result<std::size_t>::failure(
        error("Analyse ist nicht aktiv oder das Modell wurde geändert. Neu öffnen."));
  const auto overlap = [this, &session](const auto& records) {
    for (const auto& pair : records)
      add_row(leaf_item(*session.project(), pair.leaf_a),
              leaf_item(*session.project(), pair.leaf_b),
              QStringLiteral("Kollision: %1 mm³").arg(number(pair.overlap_volume_mm3)));
  };
  const auto summary = [this](std::size_t leaves, std::size_t pairs) {
    summary_->setText(QStringLiteral("%1 Vorkommen, %2 geprüfte Paare, %3 Einträge.")
                          .arg(leaves)
                          .arg(pairs)
                          .arg(results_->rowCount()));
  };
  switch (kind_->currentIndex()) {
  case 0: {
    auto result = workbench_.analyze_interference(session);
    if (!result)
      return Result<std::size_t>::failure(result.error());
    overlap(result.value().interferences);
    summary(result.value().leaf_count, result.value().evaluated_pair_count);
    break;
  }
  case 1: {
    geometry::AssemblyClearanceAnalysisOptions options;
    options.clearance_threshold_mm = clearance_->value();
    auto result = workbench_.analyze_clearance(session, options);
    if (!result)
      return Result<std::size_t>::failure(result.error());
    overlap(result.value().interferences);
    for (const auto& pair : result.value().clearance_violations)
      add_row(leaf_item(*session.project(), pair.leaf_a),
              leaf_item(*session.project(), pair.leaf_b),
              QStringLiteral("Abstand: %1 mm").arg(number(pair.minimum_distance_mm)));
    summary(result.value().leaf_count, result.value().evaluated_pair_count);
    break;
  }
  case 2: {
    auto result = workbench_.analyze_contact(session);
    if (!result)
      return Result<std::size_t>::failure(result.error());
    for (const auto& pair : result.value().records) {
      QString text;
      switch (pair.classification) {
      case geometry::AssemblyContactClassification::Interfering:
        text = QStringLiteral("Kollision: %1 mm³").arg(number(pair.overlap_volume_mm3));
        break;
      case geometry::AssemblyContactClassification::Touching:
        text = QStringLiteral("Berührung: %1 mm").arg(number(pair.minimum_distance_mm.value_or(0)));
        break;
      case geometry::AssemblyContactClassification::Separated:
        text = QStringLiteral("Getrennt: %1 mm").arg(number(pair.minimum_distance_mm.value_or(0)));
        break;
      }
      add_row(contact_item(*session.project(), pair.pair.occurrence_a),
              contact_item(*session.project(), pair.pair.occurrence_b), text);
    }
    summary(result.value().component_occurrence_count, result.value().evaluated_pair_count);
    break;
  }
  case 3: {
    std::vector<ComponentInstanceId> group;
    for (auto* item : group_->selectedItems())
      group.emplace_back(item->data(Qt::UserRole).toString().toStdString());
    auto result = workbench_.inspect_dof(session, std::move(group));
    if (!result)
      return Result<std::size_t>::failure(result.error());
    const auto& diagnostics = result.value();
    QString state;
    switch (diagnostics.dof_classification) {
    case geometry::AssemblyDofClassification::NotEvaluated:
      state = QStringLiteral("Freiheitsgrade nicht ermittelt");
      break;
    case geometry::AssemblyDofClassification::NoVariableDof:
      state = QStringLiteral("Keine beweglichen Komponenten");
      break;
    case geometry::AssemblyDofClassification::Underconstrained:
      state = QStringLiteral("Unterbestimmt");
      break;
    case geometry::AssemblyDofClassification::LocallyFullyConstrained:
      state = QStringLiteral("Lokal vollständig bestimmt");
      break;
    }
    QString consistency;
    switch (diagnostics.consistency_classification) {
    case geometry::AssemblyConstraintConsistencyClassification::LocallyConsistent:
      consistency = QStringLiteral("Beziehungen lokal konsistent");
      break;
    case geometry::AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent:
      consistency = QStringLiteral("Fixierte Geometrie widerspricht den Beziehungen");
      break;
    case geometry::AssemblyConstraintConsistencyClassification::SolverDidNotConverge:
      consistency = QStringLiteral("Lösung nicht konvergiert");
      break;
    }
    const auto remaining = diagnostics.rank_summary.rank_evaluated
                               ? QString::number(diagnostics.rank_summary.remaining_dof)
                               : QStringLiteral("nicht ermittelt");
    summary_->setText(QStringLiteral("%1; %2\nVerbleibende Freiheitsgrade: %3")
                          .arg(state, consistency, remaining));
    break;
  }
  default:
    return Result<std::size_t>::failure(error("Unbekannte Analyse"));
  }
  return Result<std::size_t>::success(static_cast<std::size_t>(results_->rowCount()));
}
void ShellInspectTools::choose_export(GuiStepExportMode mode) {
  if (!idle_and_fresh())
    return;
  const auto path = QFileDialog::getSaveFileName(&window_, QStringLiteral("STEP exportieren"), {},
                                                 QStringLiteral("STEP (*.step *.stp)"));
  if (path.isEmpty())
    return;
  auto result = export_step({mode, path.toStdString()});
  if (!result)
    window_.report_error(result.error());
  else
    window_.statusBar()->showMessage(QStringLiteral("STEP exportiert: %1 Elemente, %2 Bytes")
                                         .arg(result.value().exported_item_count)
                                         .arg(result.value().written_bytes));
}
Result<GuiStepExportResult> ShellInspectTools::export_step(const GuiStepExportRequest& request) {
  if (!idle_and_fresh())
    return Result<GuiStepExportResult>::failure(
        error("STEP benötigt aktuelle Geometrie ohne aktiven Befehl"));
  auto preflight = workbench_.preflight_step_export(window_.session(), request);
  if (!preflight)
    return Result<GuiStepExportResult>::failure(preflight.error());
  const auto target = QString::fromStdString(request.output.string());
  const auto document_path = QString::fromStdString(window_.session().file_path().string());
  const QFileInfo target_info(target), document_info(document_path);
  const auto canonical_source = document_info.canonicalFilePath();
  const bool same_path = QDir::cleanPath(target_info.absoluteFilePath()) ==
                         QDir::cleanPath(document_info.absoluteFilePath());
  const bool same_existing_file =
      !canonical_source.isEmpty() && target_info.canonicalFilePath() == canonical_source;
  if (!document_path.isEmpty() && (same_path || same_existing_file))
    return Result<GuiStepExportResult>::failure(
        error("STEP darf die BLCAD-Quelldatei nicht ersetzen"));
  // Kernel writers write synchronously. Stage their output beside the target,
  // then publish with QSaveFile so a failed export preserves an existing file.
  QTemporaryDir staging(QFileInfo(target).absolutePath() + QStringLiteral("/.blcad-step-XXXXXX"));
  if (!staging.isValid())
    return Result<GuiStepExportResult>::failure(error("STEP-Zielordner ist nicht beschreibbar"));
  auto result = workbench_.export_step(
      window_.session(), {request.mode, staging.filePath("export.step").toStdString()});
  if (!result)
    return result;
  QFile source(staging.filePath("export.step"));
  QSaveFile output(target);
  if (!source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return Result<GuiStepExportResult>::failure(error("STEP-Datei konnte nicht geöffnet werden"));
  while (!source.atEnd()) {
    const auto bytes = source.read(64 * 1024);
    if (source.error() != QFileDevice::NoError || output.write(bytes) != bytes.size())
      return Result<GuiStepExportResult>::failure(
          error("STEP-Datei konnte nicht geschrieben werden"));
  }
  if (!output.commit())
    return Result<GuiStepExportResult>::failure(
        error("STEP-Datei konnte nicht veröffentlicht werden"));
  return result;
}
void ShellInspectTools::cancel() {
  if (!active_)
    return;
  active_ = false;
  if (window_.session().task().command_id() == task_id)
    (void)window_.session().task().cancel();
  clear_result();
  dock_->hide();
  window_.refresh();
}
void ShellInspectTools::refresh() {
  const bool idle = idle_and_fresh();
  analysis_action_->setEnabled(idle && window_.session().project());
  for (auto [mode, action] : export_actions_)
    action->setEnabled(
        idle && (mode == GuiStepExportMode::PartMultiBody
                     ? window_.session().part_document() && window_.session().part_shape_cache() &&
                           window_.session().part_shape_cache()->body_shape_count() > 0
                     : window_.session().project() != nullptr));
  if (active_ && (!window_.session().project() || !window_.session().derived_results_fresh() ||
                  revision_ != window_.session().presentation_revision() ||
                  window_.session().task().command_id() != task_id))
    cancel();
}
bool ShellInspectTools::eventFilter(QObject* watched, QEvent* event) {
  if (watched == dock_ && event->type() == QEvent::Close)
    cancel();
  if (active_ && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
