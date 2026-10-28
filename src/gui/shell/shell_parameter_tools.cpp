#include "blcad/gui/shell/shell_parameter_tools.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include <charconv>
#include <cmath>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "parameters.manage";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
QString value_text(const Parameter& parameter) {
  if (parameter.type() == ParameterType::Count)
    return QString::number(static_cast<qulonglong>(parameter.value().count_value()));
  return QString::number(parameter.type() == ParameterType::Angle ? parameter.value().degrees()
                                                                  : parameter.value().millimeters(),
                         'g', 17);
}
enum class ChangeKind { AddLiteral, AddExpression, SetLiteral, SetFormula };
struct Change {
  ChangeKind kind;
  ParameterId id;
  std::string name;
  ParameterType type;
  std::string formula;
  std::optional<Quantity> value;
};
Result<std::size_t> change_part(PartDocument& part, const Change& change) {
  if (change.kind == ChangeKind::AddExpression)
    return part.add_expression_parameter(change.id, change.name, change.type, change.formula);
  if (change.kind == ChangeKind::SetFormula || change.kind == ChangeKind::SetLiteral) {
    const auto changed = change.kind == ChangeKind::SetFormula
                             ? part.set_parameter_formula(change.id, change.formula)
                             : part.set_parameter_value(change.id, *change.value);
    if (!changed)
      return Result<std::size_t>::failure(changed.error());
    return Result<std::size_t>::success(changed.value().size());
  }
  auto parameter = change.type == ParameterType::Length
                       ? Parameter::create_length(change.id, change.name, *change.value)
                   : change.type == ParameterType::Count
                       ? Parameter::create_count(change.id, change.name, *change.value)
                       : Parameter::create_angle(change.id, change.name, *change.value);
  if (!parameter)
    return Result<std::size_t>::failure(parameter.error());
  return part.add_parameter(std::move(parameter.value()));
}
} // namespace

struct ShellParameterTools::Impl {
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QTableWidget* catalog{};
  QComboBox* parameters{};
  QLineEdit* name{};
  QComboBox* type{};
  QComboBox* mode{};
  QLineEdit* input{};
  QLabel* input_label{};
  QLabel* usage{};
  QLabel* summary{};
  QPushButton* stage_button{};
  QPushButton* preview_button{};
  QPushButton* apply_button{};
  bool active{false};
  bool loading{false};
  bool pending{false};
  bool previewed{false};
  std::size_t revision{};
  std::optional<PartDocument> draft;
  std::vector<Change> changes;

  bool available() const {
    const auto& session = window.session();
    return session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !session.task().active() && !window.active_sketch();
  }
  bool current() const {
    const auto& session = window.session();
    return active && session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.model_revision() == revision;
  }
  const Parameter* selected() const {
    return draft ? draft->find_parameter(
                       ParameterId(parameters->currentData().toString().toStdString()))
                 : nullptr;
  }
  void invalidate() {
    previewed = false;
    apply_button->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void update_controls() {
    const auto parameter_type = static_cast<ParameterType>(type->currentData().toInt());
    input_label->setText(mode->currentData().toBool()              ? QStringLiteral("Formula")
                         : parameter_type == ParameterType::Length ? QStringLiteral("Value (mm)")
                         : parameter_type == ParameterType::Angle  ? QStringLiteral("Value (°)")
                                                                   : QStringLiteral("Count"));
    stage_button->setEnabled(active && pending);
    preview_button->setEnabled(active && !pending && !changes.empty());
  }
  void edited() {
    if (loading || !active)
      return;
    invalidate();
    pending = true;
    update_controls();
    summary->setText(QStringLiteral("Accept input locally first or discard by selecting again."));
  }
  void select() {
    loading = true;
    invalidate();
    const QSignalBlocker table_blocker(catalog);
    catalog->clearSelection();
    if (parameters->currentIndex() > 0)
      catalog->selectRow(parameters->currentIndex() - 1);
    const auto* parameter = selected();
    name->setText(parameter ? text(parameter->name()) : QString());
    name->setReadOnly(parameter);
    type->setCurrentIndex(
        type->findData(static_cast<int>(parameter ? parameter->type() : ParameterType::Length)));
    type->setEnabled(!parameter);
    mode->setCurrentIndex(parameter && parameter->is_expression() ? 1 : 0);
    mode->setEnabled(!parameter);
    input->setText(
        parameter ? (parameter->formula() ? text(*parameter->formula()) : value_text(*parameter))
                  : QString());
    QStringList consumers;
    if (parameter)
      for (const auto& id : draft->dependency_graph().direct_dependents(parameter->id().value()))
        consumers << text(id);
    usage->setText(parameter
                       ? QStringLiteral("Direct Consumers: %1")
                             .arg(consumers.empty() ? QStringLiteral("none") : consumers.join(", "))
                       : QStringLiteral("New parameter in the current part."));
    pending = false;
    loading = false;
    update_controls();
    summary->setText(
        QStringLiteral("%1 local changes. Preview checks the complete model.").arg(changes.size()));
  }
  void populate(const QString& selected_id = {}) {
    QSignalBlocker combo_blocker(parameters);
    QSignalBlocker table_blocker(catalog);
    parameters->clear();
    parameters->addItem(QStringLiteral("New Parameter"), QString());
    catalog->setRowCount(draft ? static_cast<int>(draft->parameters().size()) : 0);
    if (draft) {
      int row = 0;
      for (const auto& parameter : draft->parameters()) {
        parameters->addItem(text(parameter.name()) + " / " + text(parameter.id().value()),
                            text(parameter.id().value()));
        const QStringList cells{text(parameter.name()),
                                text(to_string(parameter.type())),
                                value_text(parameter) + " " + text(parameter.value().unit()),
                                parameter.formula() ? text(*parameter.formula()) : QString(),
                                text(parameter.id().value()),
                                text(to_string(parameter.scope()))};
        for (int column = 0; column < cells.size(); ++column) {
          auto* item = new QTableWidgetItem(cells[column]);
          item->setData(Qt::UserRole, text(parameter.id().value()));
          item->setToolTip(cells[column]);
          catalog->setItem(row, column, item);
        }
        ++row;
      }
    }
    const int index = parameters->findData(selected_id);
    parameters->setCurrentIndex(index >= 0 ? index : 0);
    catalog->clearSelection();
    if (index > 0)
      catalog->selectRow(index - 1);
    select();
  }
  Result<std::size_t> commit(GuiDocumentSession& session) const {
    return session.commit_part_transaction("Manage Parameters", [&](PartDocument& part) {
      for (const auto& change : changes) {
        const auto result = change_part(part, change);
        if (!result)
          return result;
      }
      return Result<std::size_t>::success(changes.size());
    });
  }
};

ShellParameterTools::ShellParameterTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("Manage Parameters"), this);
  ui.action->setObjectName("blcad.action.parameters");
  ribbon.add_button(ribbon.add_group(model_tab, QStringLiteral("Parameter")), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    const auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Manage Parameters"), &window);
  ui.dock->setObjectName("blcad.shell.parameters_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* content_layout = new QVBoxLayout(content);
  auto* hint = new QLabel(
      QStringLiteral(
          "Stage values and formulas locally, then check and apply together. Formulas use "
          "parameter names, + − * /, parentheses, and mm. Angle formulas, renaming, deleting, and "
          "switching between value and formula are unavailable."),
      content);
  hint->setWordWrap(true);
  content_layout->addWidget(hint);
  ui.catalog = new QTableWidget(content);
  ui.catalog->setObjectName("blcad.parameters.catalog");
  ui.catalog->setColumnCount(6);
  ui.catalog->setHorizontalHeaderLabels({"Name", "Type", "Value", "Formula", "ID", "Scope"});
  ui.catalog->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui.catalog->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui.catalog->setSelectionMode(QAbstractItemView::SingleSelection);
  ui.catalog->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  ui.catalog->setMinimumHeight(120);
  ui.catalog->setMaximumHeight(180);
  content_layout->addWidget(ui.catalog);
  auto* form = new QFormLayout;
  ui.parameters = new QComboBox(content);
  ui.parameters->setObjectName("blcad.parameters.parameter");
  ui.parameters->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  ui.parameters->setMinimumContentsLength(14);
  form->addRow(QStringLiteral("Parameter"), ui.parameters);
  ui.name = new QLineEdit(content);
  ui.name->setObjectName("blcad.parameters.name");
  ui.name->setPlaceholderText(QStringLiteral("e.g. wall_thickness"));
  form->addRow(QStringLiteral("Name"), ui.name);
  ui.type = new QComboBox(content);
  ui.type->setObjectName("blcad.parameters.type");
  for (const auto& [label, type] : {std::pair{"Length", ParameterType::Length},
                                    {"Count", ParameterType::Count},
                                    {"Angle", ParameterType::Angle}})
    ui.type->addItem(QString::fromUtf8(label), static_cast<int>(type));
  form->addRow(QStringLiteral("Type"), ui.type);
  apply_gui_choice_icons(*ui.type);
  ui.mode = new QComboBox(content);
  ui.mode->setObjectName("blcad.parameters.mode");
  ui.mode->addItem(QStringLiteral("Value"), false);
  ui.mode->addItem(QStringLiteral("Formula"), true);
  apply_gui_choice_icons(*ui.mode);
  form->addRow(QStringLiteral("Input"), ui.mode);
  ui.input = new QLineEdit(content);
  ui.input->setObjectName("blcad.parameters.input");
  ui.input_label = new QLabel(content);
  form->addRow(ui.input_label, ui.input);
  content_layout->addLayout(form);
  for (auto [target, name] : {std::pair{&ui.usage, "usage"}, {&ui.summary, "summary"}}) {
    *target = new QLabel(content);
    (*target)->setObjectName(QStringLiteral("blcad.parameters.") + name);
    (*target)->setWordWrap(true);
    (*target)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content_layout->addWidget(*target);
  }
  const auto button = [](QWidget* parent, QHBoxLayout* row, const char* name,
                         const QString& label) {
    auto* result = make_gui_button(label, parent);
    result->setObjectName(QStringLiteral("blcad.parameters.") + name);
    if (QString::fromLatin1(name) == QStringLiteral("apply"))
      result->setProperty("guiRole", "primary");
    row->addWidget(result);
    return result;
  };
  auto* local_buttons = new QHBoxLayout;
  ui.stage_button = button(content, local_buttons, "stage", QStringLiteral("Accept Locally"));
  auto* reset = button(content, local_buttons, "reset", QStringLiteral("Discard Local Changes"));
  content_layout->addLayout(local_buttons);
  content_layout->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  ui.preview_button = button(panel, buttons, "preview", QStringLiteral("Preview"));
  ui.apply_button = button(panel, buttons, "apply", QStringLiteral("Apply"));
  auto* cancel_button = button(panel, buttons, "cancel", QStringLiteral("Cancel"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  qApp->installEventFilter(this);
  ui.dock->installEventFilter(this);
  connect(ui.parameters, &QComboBox::currentIndexChanged, this, [this] { impl_->select(); });
  connect(ui.catalog, &QTableWidget::itemSelectionChanged, this, [this] {
    const auto items = impl_->catalog->selectedItems();
    if (!items.empty())
      impl_->parameters->setCurrentIndex(
          impl_->parameters->findData(items.front()->data(Qt::UserRole)));
  });
  for (auto* field : {ui.name, ui.input})
    connect(field, &QLineEdit::textChanged, this, [this] { impl_->edited(); });
  for (auto* field : {ui.type, ui.mode})
    connect(field, &QComboBox::currentIndexChanged, this, [this] { impl_->edited(); });
  const auto report = [this](const Result<std::size_t>& result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(ui.stage_button, &QPushButton::clicked, this, [this, report] { report(stage()); });
  connect(ui.preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply_button, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(reset, &QPushButton::clicked, this, [this] {
    if (!impl_->current()) {
      cancel();
      return;
    }
    impl_->changes.clear();
    impl_->draft = *impl_->window.session().part_document();
    impl_->populate();
  });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
}
ShellParameterTools::~ShellParameterTools() = default;
bool ShellParameterTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellParameterTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("Parameters require a current part without an active command or sketch editor"));
  (void)ui.window.session().task().begin(std::string(task_id));
  (void)ui.window.session().task().begin_parameter_editing();
  ui.active = true;
  ui.revision = ui.window.session().model_revision();
  ui.draft = *ui.window.session().part_document();
  ui.changes.clear();
  ui.populate();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.draft->parameters().size());
}
Result<std::size_t> ShellParameterTools::stage() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen parameters."));
  }
  ui.invalidate();
  if (!ui.pending)
    return Result<std::size_t>::failure(error("Enter a value or formula first"));
  const auto* existing = ui.selected();
  const auto name = ui.name->text().toStdString();
  if (!existing && !QRegularExpression(QStringLiteral("\\A[A-Za-z_][A-Za-z0-9_]*\\z"))
                        .match(ui.name->text())
                        .hasMatch())
    return Result<std::size_t>::failure(error("Name must start with a letter or _ and contain only "
                                              "ASCII letters, digits, and _"));
  const auto type = static_cast<ParameterType>(ui.type->currentData().toInt());
  const bool formula = ui.mode->currentData().toBool();
  if (formula && type == ParameterType::Angle)
    return Result<std::size_t>::failure(error("Angle formulas are not supported"));
  std::string id = existing ? existing->id().value() : std::string();
  if (!existing) {
    for (std::size_t index = 1;; ++index) {
      id = "parameter.user." + std::to_string(index);
      if (!ui.draft->dependency_graph().has_node(id))
        break;
    }
  }
  Change change{existing ? (formula ? ChangeKind::SetFormula : ChangeKind::SetLiteral)
                         : (formula ? ChangeKind::AddExpression : ChangeKind::AddLiteral),
                ParameterId(id),
                name,
                type,
                ui.input->text().toStdString(),
                std::nullopt};
  if (!formula) {
    auto numeric = ui.input->text().trimmed().toStdString();
    std::string_view parsed(numeric);
    if (parsed.starts_with('+') && !parsed.starts_with("+-"))
      parsed.remove_prefix(1);
    double value{};
    const auto conversion = std::from_chars(parsed.data(), parsed.data() + parsed.size(), value);
    if (conversion.ec != std::errc() || conversion.ptr != parsed.data() + parsed.size() ||
        !std::isfinite(value))
      return Result<std::size_t>::failure(error("Enter a finite numeric value without a unit"));
    auto quantity = type == ParameterType::Length  ? Quantity::length_mm(value, id)
                    : type == ParameterType::Count ? Quantity::count(value, id)
                                                   : Quantity::angle_deg(value, id);
    if (!quantity)
      return Result<std::size_t>::failure(quantity.error());
    change.value = quantity.value();
  }
  if (existing && (formula ? existing->formula() == std::optional<std::string>(change.formula)
                           : existing->value() == *change.value))
    return Result<std::size_t>::failure(error("Parameter is unchanged"));
  auto candidate = *ui.draft;
  const auto result = change_part(candidate, change);
  if (!result)
    return result;
  ui.draft = std::move(candidate);
  ui.changes.push_back(std::move(change));
  ui.populate(text(id));
  return Result<std::size_t>::success(ui.changes.size());
}
Result<std::size_t> ShellParameterTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen parameters."));
  }
  ui.invalidate();
  if (ui.pending || ui.changes.empty())
    return Result<std::size_t>::failure(
        error("Accept changes locally first or discard the open input by selecting again"));
  auto candidate = ui.window.session();
  const auto result = ui.commit(candidate);
  if (!result)
    return result;
  const geometry::ViewportSceneBuilder builder;
  auto scene = builder.build_part(*candidate.part_document(), *candidate.part_shape_cache());
  auto original = builder.build_part(*ui.window.session().part_document(),
                                     *ui.window.session().part_shape_cache());
  if (!scene || !original)
    return Result<std::size_t>::failure(!scene ? scene.error() : original.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  // Principal/fixed DatumPlane records have no parameter bindings. Retain their
  // normal wire presentation instead of covering the body with shaded preview planes.
  for (const auto& item : original.value())
    if (!item.semantic_id.starts_with("datum-plane/"))
      replaced.push_back(item.semantic_id);
  for (auto& item : scene.value())
    if (!item.semantic_id.starts_with("datum-plane/") &&
        item.kind != geometry::ViewportSceneKind::Face &&
        item.kind != geometry::ViewportSceneKind::Edge &&
        item.kind != geometry::ViewportSceneKind::Vertex)
      shapes.push_back(std::move(item.shape));
  if (!shapes.empty() || !replaced.empty()) {
    const auto shown =
        ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
    if (!shown)
      return shown;
  }
  ui.previewed = true;
  (void)ui.window.session().task().show_preview();
  ui.apply_button->setEnabled(true);
  ui.summary->setText(QStringLiteral("Full model preview valid. Apply commits %1 changes together.")
                          .arg(ui.changes.size()));
  return result;
}
Result<std::size_t> ShellParameterTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed || ui.pending) {
    if (!ui.current())
      cancel();
    return Result<std::size_t>::failure(error("Check a current complete preview first"));
  }
  const auto checked = preview();
  if (!checked)
    return checked;
  const auto result = ui.commit(ui.window.session());
  if (result)
    cancel();
  else
    ui.invalidate();
  return result;
}
void ShellParameterTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.draft.reset();
  ui.changes.clear();
  ui.populate();
  ui.dock->hide();
  ui.window.refresh();
}
void ShellParameterTools::refresh() {
  auto& ui = *impl_;
  if (ui.active && !ui.current()) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
bool ShellParameterTools::eventFilter(QObject* watched, QEvent* event) {
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
