#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <utility>

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
  if (assembly && component) {
    std::string semantic = "component/" + assembly->id().value();
    for (const auto& step : path)
      semantic += "/" + step.value();
    semantic += "/" + id.value();
    item->setData(Qt::UserRole + 1, QString::fromStdString(semantic));
  }
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
QString contact_text(const geometry::AssemblyContactRecord& record) {
  switch (record.classification) {
  case geometry::AssemblyContactClassification::Interfering:
    return QStringLiteral("Kollision: %1 mm³").arg(number(record.overlap_volume_mm3));
  case geometry::AssemblyContactClassification::Touching:
    return QStringLiteral("Berührung: %1 mm").arg(number(record.minimum_distance_mm.value_or(0)));
  case geometry::AssemblyContactClassification::Separated:
    return QStringLiteral("Getrennt: %1 mm").arg(number(record.minimum_distance_mm.value_or(0)));
  }
  return {};
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
  form_ = new QFormLayout;
  kind_ = new QComboBox(inputs);
  kind_->setObjectName("blcad.inspect.kind");
  kind_->addItems({QStringLiteral("Kollisionen"), QStringLiteral("Mindestabstand"),
                   QStringLiteral("Kontakte"), QStringLiteral("Freiheitsgrade (Wurzelgruppe)"),
                   QStringLiteral("Drehgelenk-Abtastung")});
  form_->addRow(QStringLiteral("Analyse"), kind_);
  clearance_ = new QDoubleSpinBox(inputs);
  clearance_->setObjectName("blcad.inspect.clearance");
  clearance_->setRange(0.000001, 1e6);
  clearance_->setDecimals(6);
  clearance_->setValue(1);
  clearance_->setSuffix(QStringLiteral(" mm"));
  clearance_->setKeyboardTracking(false);
  form_->addRow(QStringLiteral("Mindestabstand"), clearance_);
  sweep_joint_ = new QComboBox(inputs);
  sweep_joint_->setObjectName("blcad.inspect.sweep_joint");
  form_->addRow(QStringLiteral("Drehgelenk"), sweep_joint_);
  const auto angle_editor = [inputs](const char* name) {
    auto* editor = new QDoubleSpinBox(inputs);
    editor->setObjectName(QStringLiteral("blcad.inspect.") + name);
    editor->setRange(-1e6, 1e6);
    editor->setDecimals(6);
    editor->setSuffix(QStringLiteral(" °"));
    editor->setKeyboardTracking(false);
    return editor;
  };
  sweep_start_ = angle_editor("sweep_start");
  sweep_end_ = angle_editor("sweep_end");
  form_->addRow(QStringLiteral("Startwinkel"), sweep_start_);
  form_->addRow(QStringLiteral("Endwinkel"), sweep_end_);
  sweep_samples_ = new QSpinBox(inputs);
  sweep_samples_->setObjectName("blcad.inspect.sweep_samples");
  sweep_samples_->setRange(2, static_cast<int>(geometry::kAssemblyRevoluteSweepMaximumSampleCount));
  sweep_samples_->setValue(5);
  sweep_samples_->setKeyboardTracking(false);
  form_->addRow(QStringLiteral("Stützstellen"), sweep_samples_);
  layout->addLayout(form_);
  sweep_hint_ = new QLabel(inputs);
  sweep_hint_->setObjectName("blcad.inspect.sweep_hint");
  sweep_hint_->setWordWrap(true);
  layout->addWidget(sweep_hint_);
  auto* hint = new QLabel(
      QStringLiteral("Kollisionen, Abstände und Kontakte prüfen sichtbare, aktive Vorkommen in "
                     "ihrer aktuellen "
                     "Position. Für Freiheitsgrade eine zusammenhängende Wurzelgruppe auswählen. "
                     "Ergebniszelle A oder B anklicken, um das genaue Vorkommen in seiner "
                     "aktuellen Position hervorzuheben. Die Analyse verändert keine Positionen."),
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
  connect(sweep_joint_, &QComboBox::currentIndexChanged, this, [this] { update_sweep_joint(); });
  connect(sweep_start_, &QDoubleSpinBox::valueChanged, this, [this] { clear_result(); });
  connect(sweep_end_, &QDoubleSpinBox::valueChanged, this, [this] { clear_result(); });
  connect(sweep_samples_, &QSpinBox::valueChanged, this, [this] { clear_result(); });
  connect(results_, &QTableWidget::currentItemChanged, this,
          [this](QTableWidgetItem* item, QTableWidgetItem*) { highlight_result(item); });
  connect(results_, &QTableWidget::itemClicked, this,
          [this](QTableWidgetItem* item) { highlight_result(item); });
  update_inputs();
}

bool ShellInspectTools::idle_and_fresh() const {
  return window_.session().has_document() && window_.session().derived_results_fresh() &&
         !window_.session().task().active() && !window_.active_sketch();
}
bool ShellInspectTools::source_unchanged() {
  const auto& session = window_.session();
  if (!session.project() || !session.derived_results_fresh())
    return false;
  if (revision_ == session.presentation_revision())
    return true;
  // Diagnostics also advance the presentation revision. Keep the editable
  // analysis task when its canonical source intent is still identical.
  auto source = serialize_project_to_json(*session.project());
  if (!source || source.value() != source_json_)
    return false;
  revision_ = session.presentation_revision();
  return true;
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
  {
    const QSignalBlocker blocker(sweep_joint_);
    sweep_joint_->clear();
    sweep_joints_.clear();
    const auto add_joint = [this](const auto& joint,
                                  geometry::AssemblyRevoluteSweepJointScope scope,
                                  const QString& prefix) {
      if (joint.type() != AssemblyJointType::Revolute ||
          joint.state() != AssemblyJointState::Active)
        return;
      sweep_joint_->addItem(prefix + QString::fromStdString(joint.name()),
                            static_cast<int>(sweep_joints_.size()));
      const int index = sweep_joint_->count() - 1;
      sweep_joint_->setItemData(index, QString::fromStdString(joint.id().value()),
                                Qt::UserRole + 1);
      sweep_joint_->setItemData(index, static_cast<int>(scope), Qt::UserRole + 2);
      sweep_joints_.push_back({scope, joint.id()});
    };
    for (const auto& joint : session.project()->assembly().joints())
      add_joint(joint, geometry::AssemblyRevoluteSweepJointScope::RootAssemblyLocal,
                QStringLiteral("Wurzel / "));
    for (const auto& joint : session.project()->cross_hierarchy_joints())
      add_joint(joint, geometry::AssemblyRevoluteSweepJointScope::ProjectCrossHierarchy,
                QStringLiteral("Hierarchie / "));
  }
  auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(*session.project());
  if (!leaves)
    return Result<std::size_t>::failure(leaves.error());
  auto source = serialize_project_to_json(*session.project());
  if (!source)
    return Result<std::size_t>::failure(source.error());
  if (!session.task().begin(std::string(task_id)) || !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Analyse konnte nicht gestartet werden"));
  active_ = true;
  revision_ = session.presentation_revision();
  source_json_ = std::move(source.value());
  previous_mask_ = window_.viewport().selection_filter_mask();
  previous_scope_ = window_.viewport().selection_semantic_scope();
  previous_selection_ = window_.viewport().selected_semantic();
  std::vector<std::string> scope;
  for (const auto& leaf : leaves.value()) {
    std::string semantic = "component/" + leaf.assembly_document.value();
    for (const auto& step : leaf.subassembly_path)
      semantic += "/" + step.value();
    scope.push_back(semantic + "/" + leaf.component_instance.value());
  }
  window_.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Component));
  window_.viewport().set_selection_semantic_scope(std::move(scope));
  update_sweep_joint();
  clear_result();
  dock_->show();
  dock_->raise();
  window_.refresh();
  return Result<std::size_t>::success(1);
}
void ShellInspectTools::clear_result() {
  const auto highlighted = std::exchange(highlighted_occurrence_, {});
  if (!highlighted.empty() && window_.viewport().selected_semantic() &&
      window_.viewport().selected_semantic()->semantic_id == highlighted)
    window_.viewport().clear_selection();
  results_->setRowCount(0);
  summary_->setText(QStringLiteral("Analyse wählen und Analysieren klicken."));
}
void ShellInspectTools::update_inputs() {
  clearance_->setEnabled(kind_->currentIndex() == 1);
  group_->setVisible(kind_->currentIndex() == 3);
  const bool sweep = kind_->currentIndex() == 4;
  form_->setRowVisible(sweep_joint_, sweep);
  form_->setRowVisible(sweep_start_, sweep);
  form_->setRowVisible(sweep_end_, sweep);
  form_->setRowVisible(sweep_samples_, sweep);
  sweep_hint_->setVisible(sweep);
  clear_result();
}
void ShellInspectTools::update_sweep_joint() {
  clear_result();
  const auto* project = window_.session().project();
  const int index = sweep_joint_->currentData().toInt();
  const bool selected = project && sweep_joint_->currentIndex() >= 0 && index >= 0 &&
                        static_cast<std::size_t>(index) < sweep_joints_.size();
  if (!selected) {
    sweep_hint_->setText(QStringLiteral("Kein aktives Drehgelenk vorhanden."));
    return;
  }
  const auto& reference = sweep_joints_[static_cast<std::size_t>(index)];
  std::optional<AssemblyJointLimits> limits;
  if (reference.scope == geometry::AssemblyRevoluteSweepJointScope::RootAssemblyLocal) {
    if (const auto* joint = project->assembly().find_joint(reference.joint))
      limits = joint->limits();
  } else if (const auto* joint = project->find_cross_hierarchy_joint(reference.joint)) {
    limits = joint->limits();
  }
  if (!limits)
    return;
  sweep_start_->setValue(limits->lower_deg);
  sweep_end_->setValue(limits->upper_deg);
  sweep_hint_->setText(
      QStringLiteral("Grenzen: %1° bis %2°. Gleichmäßig verteilte Stützstellen einschließlich "
                     "Start und Ende; Zwischenpositionen werden nicht geprüft. Hervorhebung zeigt "
                     "das Vorkommen in der aktuellen Modellposition.")
          .arg(number(limits->lower_deg), number(limits->upper_deg)));
}
void ShellInspectTools::highlight_result(QTableWidgetItem* item) {
  if (!item || !active_ || !window_.session().derived_results_fresh() ||
      window_.session().task().command_id() != task_id || !source_unchanged())
    return;
  const auto semantic = item->data(Qt::UserRole + 1).toString().toStdString();
  if (!semantic.empty() && window_.viewport().select_semantic(semantic))
    highlighted_occurrence_ = semantic;
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
      !source_unchanged())
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
      add_row(contact_item(*session.project(), pair.pair.occurrence_a),
              contact_item(*session.project(), pair.pair.occurrence_b), contact_text(pair));
    }
    summary(result.value().component_occurrence_count, result.value().evaluated_pair_count);
    break;
  }
  case 4: {
    const int index = sweep_joint_->currentData().toInt();
    if (sweep_joint_->currentIndex() < 0 || index < 0 ||
        static_cast<std::size_t>(index) >= sweep_joints_.size())
      return Result<std::size_t>::failure(error("Aktives Drehgelenk wählen"));
    const auto start = Quantity::angle_deg(sweep_start_->value(), "gui.shell.inspect.sweep");
    const auto end = Quantity::angle_deg(sweep_end_->value(), "gui.shell.inspect.sweep");
    if (!start)
      return Result<std::size_t>::failure(start.error());
    if (!end)
      return Result<std::size_t>::failure(end.error());
    auto result = workbench_.analyze_revolute_sweep(
        session, {sweep_joints_[static_cast<std::size_t>(index)], start.value(), end.value(),
                  static_cast<std::size_t>(sweep_samples_->value())});
    if (!result)
      return Result<std::size_t>::failure(result.error());
    std::size_t evaluated_pairs = 0;
    for (const auto& sample : result.value().samples) {
      evaluated_pairs += sample.contact_analysis.evaluated_pair_count;
      for (const auto& pair : sample.contact_analysis.records) {
        add_row(contact_item(*session.project(), pair.pair.occurrence_a),
                contact_item(*session.project(), pair.pair.occurrence_b),
                QStringLiteral("%1 / %2°: %3")
                    .arg(sample.sample_index + 1)
                    .arg(number(sample.coordinate_deg), contact_text(pair)));
        results_->item(results_->rowCount() - 1, 2)->setData(Qt::UserRole, sample.coordinate_deg);
      }
    }
    summary_->setText(QStringLiteral("%1 Stützstellen, %2 geprüfte Paare. "
                                     "Zwischenpositionen wurden nicht geprüft.")
                          .arg(result.value().samples.size())
                          .arg(evaluated_pairs));
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
  auto protected_source = validate_step_import_output_path(window_.session(), request.output);
  if (!protected_source)
    return Result<GuiStepExportResult>::failure(protected_source.error());
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
  window_.viewport().set_selection_semantic_scope(previous_scope_);
  window_.viewport().set_selection_filter_mask(previous_mask_);
  window_.refresh_feature_selection_scene();
  if (previous_selection_ && source_unchanged())
    (void)window_.viewport().select_semantic(previous_selection_->semantic_id);
  previous_selection_.reset();
  source_json_.clear();
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
                  !source_unchanged() || window_.session().task().command_id() != task_id))
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
