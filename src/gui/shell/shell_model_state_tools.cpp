#include "blcad/gui/shell/shell_model_state_tools.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
#include <charconv>
#include <cmath>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "model_states.manage";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string value) {
  return Error::validation(std::string(task_id), std::move(value));
}
QString numeric(const Quantity& q) {
  return q.kind() == QuantityKind::Count
             ? QString::number(static_cast<qulonglong>(q.count_value()))
             : QString::number(q.kind() == QuantityKind::AngleDeg ? q.degrees() : q.millimeters(),
                               'g', 17);
}
QTableWidgetItem* cell(QTableWidget& table, int row, int column, QString label,
                       bool editable = false) {
  auto* item = new QTableWidgetItem(std::move(label));
  if (!editable)
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  table.setItem(row, column, item);
  return item;
}
} // namespace
struct ShellModelStateTools::Impl {
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox* owners{};
  QListWidget* states{};
  QLineEdit* name{};
  QTableWidget *parameters{}, *features{}, *materials{}, *children{};
  QLabel* summary{};
  QPushButton *apply{}, *view{}, *remove{}, *rename{}, *duplicate{}, *save_member{};
  std::optional<PartDocument> part;
  std::optional<AssemblyDocument> assembly;
  std::vector<ModelState> drafts;
  std::vector<EngineeringResourceKey> material_keys;
  DocumentId owner;
  ModelStateId selected{primary_model_state_id()};
  std::size_t revision{};
  bool active = false, loading = false, pending = false, previewed = false;
  bool available() const {
    const auto& s = window.session();
    return s.has_document() && !s.project_part_context_active() &&
           (!s.part_document() || !s.part_document()->is_reference_part()) &&
           s.derived_results_fresh() && !s.reference_repair_mode() && !s.task().active() &&
           !s.part_workflow_active() && !window.active_sketch();
  }
  bool current() const {
    const auto& s = window.session();
    return active && s.model_revision() == revision && s.task().command_id() == task_id &&
           !window.active_sketch();
  }
  ModelState* state() {
    const auto found =
        std::find_if(drafts.begin(), drafts.end(), [&](const auto& s) { return s.id == selected; });
    return found == drafts.end() ? nullptr : &*found;
  }
  void invalidate() {
    previewed = false;
    apply->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void edited() {
    if (loading || !active)
      return;
    pending = true;
    owners->setEnabled(false);
    invalidate();
    summary->setText("Accept the edited cells locally, then Preview the complete table.");
  }
  QComboBox* choice(QTableWidget& table, int row, int column, const QStringList& labels) {
    auto* combo = new QComboBox(&table);
    combo->addItems(labels);
    apply_gui_choice_icons(*combo);
    table.setCellWidget(row, column, combo);
    QObject::connect(combo, &QComboBox::currentIndexChanged, &window, [this] { edited(); });
    return combo;
  }
  void select_rows() {
    loading = true;
    pending = false;
    invalidate();
    auto* selected_state = state();
    name->setText(selected_state ? text(selected_state->name) : QStringLiteral("Primary"));
    name->setReadOnly(!selected_state);
    for (auto* button : {remove, rename, duplicate})
      button->setEnabled(selected_state);
    view->setEnabled(!window.session().project() ||
                     (assembly && owner == window.session().project()->assembly().id()));
    save_member->setEnabled(part.has_value());
    const auto& source_parameters = part ? part->parameters() : assembly->parameters();
    parameters->setRowCount(static_cast<int>(source_parameters.size()));
    for (int row = 0; row < parameters->rowCount(); ++row) {
      const auto& p = source_parameters[static_cast<std::size_t>(row)];
      auto* enabled = cell(*parameters, row, 0, {});
      enabled->setFlags(enabled->flags() | Qt::ItemIsUserCheckable);
      enabled->setCheckState(Qt::Unchecked);
      cell(*parameters, row, 1, text(p.name()));
      cell(*parameters, row, 2,
           p.formula() ? "Formula" : text(to_string(p.type())) + " / " + text(p.value().unit()));
      auto input = p.formula() ? text(*p.formula()) : numeric(p.value());
      if (selected_state)
        for (const auto& override : selected_state->parameters)
          if (override.parameter == p.id()) {
            enabled->setCheckState(Qt::Checked);
            input = std::holds_alternative<Quantity>(override.value)
                        ? numeric(std::get<Quantity>(override.value))
                        : text(std::get<std::string>(override.value));
          }
      cell(*parameters, row, 3, input, true);
    }
    features->setRowCount(0);
    if (part) {
      auto history = part->executable_history();
      if (history) {
        features->setRowCount(static_cast<int>(history.value().size()));
        for (int row = 0; row < features->rowCount(); ++row) {
          const auto& id = history.value()[static_cast<std::size_t>(row)].node_id;
          cell(*features, row, 0, text(id));
          auto* combo = choice(*features, row, 1, {"Inherit Primary", "Active", "Suppressed"});
          if (selected_state)
            for (const auto& override : selected_state->features)
              if (override.executable == id)
                combo->setCurrentIndex(override.suppressed ? 2 : 1);
        }
      }
    }
    materials->setRowCount(part ? static_cast<int>(part->bodies().size()) + 1 : 0);
    material_keys.clear();
    QStringList material_labels{"Inherit Primary", "Clear assignment"};
    if (part)
      for (const auto& entry : part->engineering_resources().entries())
        if (const auto* material = std::get_if<EngineeringMaterial>(&entry)) {
          material_keys.push_back(material->key());
          material_labels << text(material->name()) + " / " + text(material->key().revision());
        }
    for (int row = 0; row < materials->rowCount(); ++row) {
      const std::optional<BodyId> body =
          row ? std::optional(part->bodies()[static_cast<std::size_t>(row - 1)].id())
              : std::nullopt;
      cell(*materials, row, 0, body ? text(body->value()) : QStringLiteral("Part default"));
      auto* combo = choice(*materials, row, 1, material_labels);
      if (selected_state)
        for (const auto& override : selected_state->materials)
          if (override.body == body) {
            combo->setCurrentIndex(1);
            if (override.material)
              for (std::size_t i = 0; i < material_keys.size(); ++i)
                if (material_keys[i] == *override.material)
                  combo->setCurrentIndex(static_cast<int>(i) + 2);
          }
    }
    children->setRowCount(assembly ? static_cast<int>(assembly->component_instances().size() +
                                                      assembly->subassembly_instances().size())
                                   : 0);
    if (assembly) {
      int row = 0;
      const auto populate = [&](const auto& child, const auto* definition, const auto& overrides) {
        cell(*children, row, 0, text(child.id().value()));
        auto* suppression = choice(*children, row, 1, {"Inherit Primary", "Active", "Suppressed"});
        QStringList labels{"Inherit Primary", "Primary"};
        if (definition)
          for (const auto& state : definition->model_states())
            labels << text(state.name);
        auto* states = choice(*children, row, 2, labels);
        states->setItemData(0, QString());
        states->setItemData(1, QStringLiteral("primary"));
        if (definition)
          for (int index = 2; index < states->count(); ++index)
            states->setItemData(
                index,
                text(definition->model_states()[static_cast<std::size_t>(index - 2)].id.value()));
        for (const auto& override : overrides) {
          const auto id = [&] {
            if constexpr (requires { override.component; })
              return override.component.value();
            else
              return override.subassembly.value();
          }();
          if (id != child.id().value())
            continue;
          if (override.suppressed)
            suppression->setCurrentIndex(*override.suppressed ? 2 : 1);
          if (override.selected_state)
            states->setCurrentIndex(states->findData(text(override.selected_state->value())));
        }
        ++row;
      };
      const auto* root = window.session().project();
      for (const auto& component : assembly->component_instances())
        populate(component, root->find_part_document(component.referenced_part_document()),
                 selected_state ? selected_state->components
                                : std::vector<ModelStateComponentOverride>{});
      for (const auto& child : assembly->subassembly_instances())
        populate(child, root->find_assembly_document(child.referenced_assembly_document()),
                 selected_state ? selected_state->subassemblies
                                : std::vector<ModelStateSubassemblyOverride>{});
    }
    for (auto* table : {parameters, features, materials, children})
      table->setEnabled(selected_state);
    loading = false;
    summary->setText(selected_state ? "Edit sparse overrides. Unchecked cells inherit Primary."
                                    : "Primary is permanent. Create a state or choose a named "
                                      "sibling.");
  }
  void populate_states() {
    const QSignalBlocker blocker(states);
    states->clear();
    auto* primary = new QListWidgetItem("Primary", states);
    primary->setData(Qt::UserRole, "primary");
    for (const auto& state : drafts) {
      auto* item = new QListWidgetItem(text(state.name), states);
      item->setData(Qt::UserRole, text(state.id.value()));
    }
    int index = 0;
    for (int i = 0; i < states->count(); ++i)
      if (states->item(i)->data(Qt::UserRole).toString().toStdString() == selected.value())
        index = i;
    states->setCurrentRow(index);
    select_rows();
  }
  void load_owner(DocumentId id) {
    owner = std::move(id);
    part.reset();
    assembly.reset();
    const auto& s = window.session();
    if (s.part_document())
      part = *s.part_document();
    else if (const auto* p = s.project()->find_part_document(owner))
      part = *p;
    else
      assembly = s.project()->assembly();
    owners->setEnabled(true);
    drafts = part ? part->model_states() : assembly->model_states();
    selected = primary_model_state_id();
    populate_states();
  }
};

ShellModelStateTools::ShellModelStateTools(ShellWindow& window, ShellRibbon& ribbon, int tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction("Model States", this);
  ui.action->setObjectName("blcad.action.model_states");
  ribbon.add_button(ribbon.add_group(tab, "Variants"), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget("Model States", &window);
  ui.dock->setObjectName("blcad.shell.model_states_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* body = new QVBoxLayout(content);
  auto* hint = new QLabel("Named states inherit Primary. Stage typed overrides, then "
                          "Preview and Apply the table together. View selection is "
                          "temporary; Save retains Primary and state definitions.",
                          content);
  hint->setWordWrap(true);
  body->addWidget(hint);
  ui.owners = new QComboBox(content);
  ui.owners->setObjectName("blcad.model_states.owner");
  body->addWidget(ui.owners);
  ui.states = new QListWidget(content);
  ui.states->setObjectName("blcad.model_states.states");
  ui.states->setMaximumHeight(110);
  body->addWidget(ui.states);
  auto* form = new QFormLayout;
  ui.name = new QLineEdit(content);
  ui.name->setObjectName("blcad.model_states.name");
  form->addRow("State name", ui.name);
  body->addLayout(form);
  const auto button = [&](QWidget* parent, QHBoxLayout* row, const char* id, const QString& label) {
    auto* result = make_gui_button(label, parent);
    result->setObjectName(QStringLiteral("blcad.model_states.") + id);
    row->addWidget(result);
    return result;
  };
  auto* catalog = new QHBoxLayout;
  auto* create = button(content, catalog, "create", "New");
  ui.duplicate = button(content, catalog, "duplicate", "Duplicate");
  ui.rename = button(content, catalog, "rename", "Rename");
  ui.remove = button(content, catalog, "delete", "Delete");
  body->addLayout(catalog);
  const auto table = [&](QTableWidget*& target, const char* id, const QStringList& labels) {
    target = new QTableWidget(content);
    target->setObjectName(QStringLiteral("blcad.model_states.") + id);
    target->setColumnCount(labels.size());
    target->setHorizontalHeaderLabels(labels);
    target->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    target->horizontalHeader()->setStretchLastSection(true);
    target->setMinimumHeight(100);
    target->setMaximumHeight(190);
    body->addWidget(target);
    connect(target, &QTableWidget::itemChanged, this, [this] { impl_->edited(); });
  };
  table(ui.parameters, "parameters", {"Use", "Parameter", "Category", "Value / formula"});
  table(ui.features, "features", {"Feature", "Suppression"});
  table(ui.materials, "materials", {"Material scope", "Pinned material"});
  table(ui.children, "children", {"Occurrence", "Suppression", "Model state"});
  ui.summary = new QLabel(content);
  ui.summary->setObjectName("blcad.model_states.summary");
  ui.summary->setWordWrap(true);
  body->addWidget(ui.summary);
  auto* local = new QHBoxLayout;
  auto* stage_button = button(content, local, "stage", "Accept Locally");
  auto* reset = button(content, local, "reset", "Discard Local Changes");
  body->addLayout(local);
  body->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll);
  auto* commands = new QHBoxLayout;
  auto* preview_button = button(panel, commands, "preview", "Preview");
  ui.apply = button(panel, commands, "apply", "Apply");
  ui.apply->setProperty("guiRole", "primary");
  auto* cancel_button = button(panel, commands, "cancel", "Cancel");
  layout->addLayout(commands);
  auto* navigation = new QHBoxLayout;
  ui.view = button(panel, navigation, "view", "View selected state");
  auto* primary = button(panel, navigation, "primary", "Return to Primary");
  layout->addLayout(navigation);
  auto* member_buttons = new QHBoxLayout;
  ui.save_member = button(panel, member_buttons, "member", "Save Member Copy");
  member_buttons->addStretch();
  layout->addLayout(member_buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  const auto report = [this](const auto& result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(ui.owners, &QComboBox::currentIndexChanged, this, [this] {
    if (!impl_->loading && impl_->active)
      impl_->load_owner(DocumentId(impl_->owners->currentData().toString().toStdString()));
  });
  connect(ui.states, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem* item, QListWidgetItem* previous) {
            if (impl_->loading || !item)
              return;
            if (impl_->pending) {
              const QSignalBlocker blocker(impl_->states);
              impl_->states->setCurrentItem(previous);
              impl_->summary->setText("Accept the edited cells locally or discard local changes "
                                      "before selecting another state.");
              return;
            }
            impl_->selected = ModelStateId(item->data(Qt::UserRole).toString().toStdString());
            impl_->select_rows();
          });
  const auto create_named = [this, report](bool duplicate) {
    std::size_t i = 1;
    while (has_model_state(impl_->drafts, ModelStateId("state." + std::to_string(i))) ||
           std::any_of(impl_->drafts.begin(), impl_->drafts.end(), [&](const auto& state) {
             return state.name == "State " + std::to_string(i);
           }))
      ++i;
    report(create_state(ModelStateId("state." + std::to_string(i)), "State " + std::to_string(i),
                        duplicate));
  };
  connect(create, &QPushButton::clicked, this, [create_named] { create_named(false); });
  connect(ui.duplicate, &QPushButton::clicked, this, [create_named] { create_named(true); });
  connect(ui.rename, &QPushButton::clicked, this,
          [this, report] { report(rename_state(impl_->name->text().toStdString())); });
  connect(ui.remove, &QPushButton::clicked, this, [this, report] { report(delete_state()); });
  connect(stage_button, &QPushButton::clicked, this, [this, report] { report(stage()); });
  connect(preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, &ShellModelStateTools::cancel);
  connect(reset, &QPushButton::clicked, this, [this] { impl_->load_owner(impl_->owner); });
  connect(ui.view, &QPushButton::clicked, this,
          [this, report] { report(activate_state(impl_->selected)); });
  connect(primary, &QPushButton::clicked, this,
          [this, report] { report(activate_state(primary_model_state_id())); });
  connect(ui.save_member, &QPushButton::clicked, this, [this, report] {
    const auto path = QFileDialog::getSaveFileName(&impl_->window, "Save independent member", {},
                                                   "BLCAD Part (*.blcad.json)");
    if (path.isEmpty())
      return;
    const auto id = DocumentId("part.member." +
                               QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    report(export_member(path.toStdString(), id, impl_->name->text().toStdString()));
  });
  qApp->installEventFilter(this);
  ui.dock->installEventFilter(this);
}
ShellModelStateTools::~ShellModelStateTools() = default;
QAction& ShellModelStateTools::action() noexcept {
  return *impl_->action;
}
bool ShellModelStateTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellModelStateTools::begin(std::optional<DocumentId> requested) {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(error("Model States requires an idle complete document"));
  ui.loading = true;
  ui.owners->clear();
  const auto& s = ui.window.session();
  if (const auto* part = s.part_document())
    ui.owners->addItem(text(part->name()), text(part->id().value()));
  else {
    ui.owners->addItem("Root Assembly / " + text(s.project()->assembly().name()),
                       text(s.project()->assembly().id().value()));
    for (const auto& part : s.project()->part_documents())
      if (!part.is_reference_part())
        ui.owners->addItem("Part / " + text(part.name()), text(part.id().value()));
  }
  if (requested)
    ui.owners->setCurrentIndex(ui.owners->findData(text(requested->value())));
  if (ui.owners->currentIndex() < 0) {
    ui.loading = false;
    return Result<std::size_t>::failure(error("State owner is unavailable"));
  }
  ui.loading = false;
  (void)ui.window.session().task().begin(std::string(task_id));
  (void)ui.window.session().task().begin_parameter_editing();
  ui.active = true;
  ui.revision = s.model_revision();
  ui.load_owner(DocumentId(ui.owners->currentData().toString().toStdString()));
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.drafts.size());
}
Result<std::size_t> ShellModelStateTools::create_state(ModelStateId id, std::string name,
                                                       bool duplicate) {
  auto& ui = *impl_;
  if (!ui.current() || ui.pending)
    return Result<std::size_t>::failure(error("Accept or discard edited cells first"));
  auto draft = duplicate && ui.state() ? *ui.state() : ModelState{};
  draft.id = std::move(id);
  draft.name = std::move(name);
  auto candidate = ui.drafts;
  candidate.push_back(std::move(draft));
  auto valid = validate_model_state_catalog(candidate);
  if (!valid)
    return valid;
  ui.owners->setEnabled(false);
  ui.drafts = std::move(candidate);
  ui.selected = ui.drafts.back().id;
  ui.populate_states();
  return Result<std::size_t>::success(ui.drafts.size());
}
Result<std::size_t> ShellModelStateTools::rename_state(std::string name) {
  auto& ui = *impl_;
  if (!ui.current() || !ui.state() || ui.pending)
    return Result<std::size_t>::failure(error("Choose a named state without pending cells"));
  auto candidate = ui.drafts;
  for (auto& state : candidate)
    if (state.id == ui.selected)
      state.name = std::move(name);
  auto valid = validate_model_state_catalog(candidate);
  if (!valid)
    return valid;
  ui.owners->setEnabled(false);
  ui.drafts = std::move(candidate);
  ui.populate_states();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellModelStateTools::delete_state() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.state() || ui.pending)
    return Result<std::size_t>::failure(error("Choose a named state without pending cells"));
  ui.owners->setEnabled(false);
  std::erase_if(ui.drafts, [&](const auto& s) { return s.id == ui.selected; });
  ui.selected = primary_model_state_id();
  ui.populate_states();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellModelStateTools::select_state(ModelStateId state) {
  auto& ui = *impl_;
  if (!ui.current() || ui.pending || !has_model_state(ui.drafts, state))
    return Result<std::size_t>::failure(
        error("Accept edited cells before choosing a current state"));
  ui.selected = std::move(state);
  ui.populate_states();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellModelStateTools::stage() {
  auto& ui = *impl_;
  if (!ui.current())
    return Result<std::size_t>::failure(error("The model-state context changed"));
  auto* current = ui.state();
  if (!current)
    return Result<std::size_t>::success(0);
  auto updated = *current;
  updated.parameters.clear();
  updated.features.clear();
  updated.materials.clear();
  updated.components.clear();
  updated.subassemblies.clear();
  const auto& parameters = ui.part ? ui.part->parameters() : ui.assembly->parameters();
  for (int row = 0; row < ui.parameters->rowCount(); ++row) {
    if (ui.parameters->item(row, 0)->checkState() != Qt::Checked)
      continue;
    const auto& parameter = parameters[static_cast<std::size_t>(row)];
    const auto input = ui.parameters->item(row, 3)->text().trimmed().toStdString();
    if (parameter.formula())
      updated.parameters.push_back({parameter.id(), input});
    else {
      double value{};
      const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
      if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() ||
          !std::isfinite(value))
        return Result<std::size_t>::failure(
            error("Enter a finite number in the displayed parameter unit"));
      auto quantity = parameter.type() == ParameterType::Length
                          ? Quantity::length_mm(value, parameter.id().value())
                      : parameter.type() == ParameterType::Count
                          ? Quantity::count(value, parameter.id().value())
                          : Quantity::angle_deg(value, parameter.id().value());
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      updated.parameters.push_back({parameter.id(), quantity.value()});
    }
  }
  for (int row = 0; row < ui.features->rowCount(); ++row) {
    const auto index = static_cast<QComboBox*>(ui.features->cellWidget(row, 1))->currentIndex();
    if (index)
      updated.features.push_back({ui.features->item(row, 0)->text().toStdString(), index == 2});
  }
  for (int row = 0; row < ui.materials->rowCount(); ++row) {
    const auto index = static_cast<QComboBox*>(ui.materials->cellWidget(row, 1))->currentIndex();
    if (!index)
      continue;
    const std::optional<BodyId> body =
        row ? std::optional(ui.part->bodies()[static_cast<std::size_t>(row - 1)].id())
            : std::nullopt;
    const std::optional<EngineeringResourceKey> material =
        index > 1 ? std::optional(ui.material_keys[static_cast<std::size_t>(index - 2)])
                  : std::nullopt;
    updated.materials.push_back({body, material});
  }
  for (int row = 0; row < ui.children->rowCount(); ++row) {
    const auto index = static_cast<QComboBox*>(ui.children->cellWidget(row, 1))->currentIndex();
    const auto selected =
        static_cast<QComboBox*>(ui.children->cellWidget(row, 2))->currentData().toString();
    if (!index && selected.isEmpty())
      continue;
    const std::optional<bool> suppressed = index ? std::optional(index == 2) : std::nullopt;
    const std::optional<ModelStateId> state =
        selected.isEmpty() ? std::nullopt : std::optional(ModelStateId(selected.toStdString()));
    const auto id = ui.children->item(row, 0)->text().toStdString();
    if (row < static_cast<int>(ui.assembly->component_instances().size()))
      updated.components.push_back({ComponentInstanceId(id), suppressed, state});
    else
      updated.subassemblies.push_back({SubassemblyInstanceId(id), suppressed, state});
  }
  auto candidates = ui.drafts;
  for (auto& state : candidates)
    if (state.id == ui.selected)
      state = updated;
  auto valid = ui.part ? ui.part->replace_model_states(candidates)
                       : ui.assembly->replace_model_states(candidates);
  if (!valid)
    return valid;
  ui.owners->setEnabled(false);
  ui.drafts = std::move(candidates);
  ui.pending = false;
  ui.invalidate();
  ui.summary->setText("Cells accepted locally. Preview validates every state "
                      "and the complete document.");
  return valid;
}
Result<std::size_t> ShellModelStateTools::preview() {
  auto& ui = *impl_;
  if (!ui.current())
    return Result<std::size_t>::failure(error("The model-state context changed"));
  auto staged = stage();
  if (!staged)
    return staged;
  ui.invalidate();
  auto candidate = ui.window.session();
  auto checked = candidate.commit_model_state_table(ui.owner, ui.drafts);
  if (!checked)
    return checked;
  (void)candidate.task().cancel();
  if (candidate.part_document() ||
      (ui.assembly && ui.owner == candidate.project()->assembly().id())) {
    auto selected = candidate.activate_model_state(ui.selected);
    if (!selected)
      return selected;
  }
  auto context = make_step_import_context(candidate);
  if (!context)
    return Result<std::size_t>::failure(context.error());
  geometry::ViewportSceneBuilder builder(context.value().get());
  const auto scene = [&](const GuiDocumentSession& source) {
    return source.effective_part_document()
               ? builder.build_part(*source.effective_part_document(), *source.part_shape_cache())
               : builder.build_project_for_states(
                     *source.effective_project(), [&](DocumentId id, ModelStateId state) {
                       return source.project_part_shape_cache(id, state);
                     });
  };
  auto preview = scene(candidate), original = scene(ui.window.session());
  if (!preview || !original)
    return Result<std::size_t>::failure(!preview ? preview.error() : original.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  for (const auto& item : original.value())
    if (!item.semantic_id.starts_with("datum-plane/"))
      replaced.push_back(item.semantic_id);
  for (auto& item : preview.value())
    if (!item.semantic_id.starts_with("datum-plane/") &&
        item.kind != geometry::ViewportSceneKind::Face &&
        item.kind != geometry::ViewportSceneKind::Edge &&
        item.kind != geometry::ViewportSceneKind::Vertex)
      shapes.push_back(std::move(item.shape));
  if (context.value()) {
    auto fresh = context.value()->validate_sources();
    if (!fresh)
      return fresh;
  }
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  ui.previewed = true;
  ui.apply->setEnabled(true);
  (void)ui.window.session().task().show_preview();
  ui.summary->setText(ui.window.session().project() && ui.part
                          ? "All state members validated. Assembly preview retains occurrence "
                            "selections. Apply publishes one state-table transaction."
                          : "Selected state and every table member validated. Apply publishes "
                            "one state-table transaction.");
  return checked;
}
Result<std::size_t> ShellModelStateTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed || ui.pending)
    return Result<std::size_t>::failure(error("Preview the complete current table before Apply"));
  auto checked = preview();
  if (!checked)
    return checked;
  auto result = ui.window.session().commit_model_state_table(ui.owner, ui.drafts);
  if (result)
    cancel();
  else
    ui.invalidate();
  return result;
}
Result<std::size_t> ShellModelStateTools::activate_state(ModelStateId state) {
  auto& ui = *impl_;
  if (ui.active) {
    // The editor snapshots include staged cells after stage(); compare the live
    // owner so activation cannot silently discard an unapplied table.
    const auto* source_part = ui.window.session().part_document();
    if (!source_part && ui.part)
      source_part = ui.window.session().project()->find_part_document(ui.owner);
    const auto& source = source_part ? source_part->model_states()
                                     : ui.window.session().project()->assembly().model_states();
    if (ui.pending || ui.drafts != source)
      return Result<std::size_t>::failure(
          error("Apply or discard the state table before switching views"));
    if (ui.window.session().project() && ui.part && state != primary_model_state_id())
      return Result<std::size_t>::failure(
          error("Choose this Part state on an Assembly occurrence"));
    cancel();
  }
  auto result = ui.window.session().activate_model_state(std::move(state));
  ui.window.refresh();
  return result;
}
Result<std::uintmax_t> ShellModelStateTools::export_member(const std::filesystem::path& path,
                                                           DocumentId new_id,
                                                           std::string new_name) {
  auto& ui = *impl_;
  if (!ui.current() || !ui.part || ui.pending)
    return Result<std::uintmax_t>::failure(
        error("Choose a saved Part state without pending cells"));
  const auto* source = ui.window.session().part_document();
  if (!source)
    source = ui.window.session().project()->find_part_document(ui.owner);
  if (ui.drafts != source->model_states())
    return Result<std::uintmax_t>::failure(
        error("Apply the state table before exporting a member"));
  auto candidate = ui.window.session();
  (void)candidate.task().cancel();
  return candidate.save_model_state_member(
      path, ui.selected, std::move(new_id), std::move(new_name),
      candidate.project() ? std::optional(ui.owner) : std::nullopt);
}
void ShellModelStateTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.part.reset();
  ui.assembly.reset();
  ui.drafts.clear();
  ui.dock->hide();
  ui.window.refresh();
}
void ShellModelStateTools::refresh() {
  if (impl_->active && !impl_->current()) {
    cancel();
    return;
  }
  impl_->action->setEnabled(impl_->available());
}
bool ShellModelStateTools::eventFilter(QObject* watched, QEvent* event) {
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
