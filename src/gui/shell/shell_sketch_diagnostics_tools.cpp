#include "blcad/gui/shell/shell_sketch_diagnostics_tools.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/core/sketch_repair_commands.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "sketch.diagnostics";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
QString action_label(SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return QStringLiteral("Fix Endpoint");
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return QStringLiteral("Remove Duplicate Fixes");
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return QStringLiteral("Remove Duplicate Driving Dimensions");
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return QStringLiteral("Edit Conflicting Alignment Manually");
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return QStringLiteral("Add Driving Dimension Manually");
  }
  return QStringLiteral("Edit Manually");
}
QString severity_label(SketchDiagnosticSeverity severity) {
  switch (severity) {
  case SketchDiagnosticSeverity::Info:
    return QStringLiteral("Information");
  case SketchDiagnosticSeverity::Warning:
    return QStringLiteral("Warning");
  case SketchDiagnosticSeverity::Error:
    return QStringLiteral("Error");
  }
  return {};
}
} // namespace

struct ShellSketchDiagnosticsTools::Impl {
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox* sketches{};
  QListWidget* diagnostics{};
  QComboBox* suggestions{};
  QLabel* detail{};
  QLabel* summary{};
  QPushButton* preview_button{};
  QPushButton* apply_button{};
  bool active{false};
  bool previewed{false};
  std::size_t revision{};
  std::optional<SketchId> editing_sketch;
  std::vector<SketchConstraintDiagnostic> report;
  std::vector<SketchRepairSuggestion> choices;

  bool available() const {
    const auto& session = window.session();
    return session.part_document() && !session.part_document()->sketches().empty() &&
           session.derived_results_fresh() && !session.reference_repair_mode() &&
           !session.task().active();
  }
  bool current() const {
    const auto& session = window.session();
    return active && session.part_document() && session.derived_results_fresh() &&
           session.task().command_id() == task_id && session.model_revision() == revision &&
           editing_sketch == window.active_sketch() &&
           (!editing_sketch || sketch_id() == *editing_sketch);
  }
  SketchId sketch_id() const {
    return SketchId(sketches->currentData().toString().toStdString());
  }
  const SketchRepairSuggestion* choice() const {
    const int index = suggestions->currentIndex();
    return index >= 0 && index < static_cast<int>(choices.size()) ? &choices[index] : nullptr;
  }
  void invalidate() {
    previewed = false;
    apply_button->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void select_suggestion() {
    invalidate();
    preview_button->setEnabled(false);
    summary->setText(QStringLiteral("No automatic repair is available for this diagnostic."));
    const auto* selected = choice();
    if (!current() || !selected)
      return;
    auto candidate = *window.session().part_document()->find_sketch(sketch_id());
    const auto checked =
        SketchRepairCommandExecutor{}.apply(candidate, SketchRepairCommand(*selected));
    if (!checked) {
      summary->setText(text(checked.error().message()));
      return;
    }
    if (!checked.value().applied()) {
      summary->setText(QStringLiteral("Manual editing required: ") +
                       text(checked.value().message()));
      return;
    }
    summary->setText(
        QStringLiteral("Suggestion selected. Check the preview, then apply explicitly."));
    preview_button->setEnabled(true);
  }
  void select_diagnostic() {
    QSignalBlocker blocker(suggestions);
    suggestions->clear();
    choices.clear();
    detail->clear();
    const int index = diagnostics->currentRow();
    if (current() && index >= 0 && index < static_cast<int>(report.size())) {
      const auto& diagnostic = report[index];
      detail->setText(text(diagnostic.target()) + QStringLiteral("\n") +
                      text(diagnostic.message()));
      SketchDiagnosticReport selected(sketch_id());
      selected.add(diagnostic);
      choices = SketchRepairSuggester{}.suggest(selected).suggestions();
      for (const auto& suggestion : choices)
        suggestions->addItem(action_label(suggestion.action()),
                             text(to_string(suggestion.action())));
    }
    select_suggestion();
  }
  void inspect() {
    QSignalBlocker blocker(diagnostics);
    diagnostics->clear();
    report.clear();
    if (current()) {
      const auto* sketch = window.session().part_document()->find_sketch(sketch_id());
      if (sketch) {
        report = SketchConstraintDiagnostics{}.analyze(*sketch).diagnostics();
        for (const auto& diagnostic : report) {
          auto* item = new QListWidgetItem(severity_label(diagnostic.severity()) +
                                               QStringLiteral(" / ") + text(diagnostic.target()),
                                           diagnostics);
          item->setData(Qt::UserRole,
                        text(to_string(diagnostic.kind())) + "/" + text(diagnostic.target()));
          item->setToolTip(text(diagnostic.message()));
        }
      }
    }
    diagnostics->setCurrentRow(report.empty() ? -1 : 0);
    select_diagnostic();
    if (report.empty())
      summary->setText(QStringLiteral("No diagnostics for this sketch."));
  }
};

ShellSketchDiagnosticsTools::ShellSketchDiagnosticsTools(ShellWindow& window, ShellRibbon& ribbon,
                                                         int model_tab, int sketch_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("Inspect Sketch"), this);
  ui.action->setObjectName("blcad.action.sketch_diagnostics");
  for (const int tab : {model_tab, sketch_tab})
    ribbon.add_button(ribbon.add_group(tab, QStringLiteral("Sketch Diagnostics")), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Inspect and Repair Sketch"), &window);
  ui.dock->setObjectName("blcad.shell.sketch_diagnostics_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* hint = new QLabel(
      QStringLiteral(
          "Check fixes, alignments, and driving dimensions. Sketch editor degrees of freedom "
          "are displayed separately. Select a diagnostic and suggestion, "
          "check the preview, then apply. Other diagnostics may remain."),
      panel);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto* form = new QFormLayout;
  ui.sketches = new QComboBox(panel);
  ui.sketches->setObjectName("blcad.sketch_diagnostics.sketch");
  form->addRow(QStringLiteral("Sketch"), ui.sketches);
  layout->addLayout(form);
  ui.diagnostics = new QListWidget(panel);
  ui.diagnostics->setObjectName("blcad.sketch_diagnostics.diagnostics");
  ui.diagnostics->setMinimumHeight(90);
  ui.diagnostics->setMaximumHeight(170);
  layout->addWidget(ui.diagnostics);
  for (auto [target, name] : {std::pair{&ui.detail, "detail"}, {&ui.summary, "summary"}}) {
    *target = new QLabel(panel);
    (*target)->setObjectName(QStringLiteral("blcad.sketch_diagnostics.") + name);
    (*target)->setWordWrap(true);
    (*target)->setTextInteractionFlags(Qt::TextSelectableByMouse);
  }
  layout->addWidget(ui.detail);
  ui.suggestions = new QComboBox(panel);
  ui.suggestions->setObjectName("blcad.sketch_diagnostics.suggestion");
  layout->addWidget(ui.suggestions);
  layout->addWidget(ui.summary);
  layout->addStretch();
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* name, const QString& label) {
    auto* result = make_gui_button(label, panel);
    result->setObjectName(QStringLiteral("blcad.sketch_diagnostics.") + name);
    if (QString::fromLatin1(name) == QStringLiteral("apply"))
      result->setProperty("guiRole", "primary");
    buttons->addWidget(result);
    return result;
  };
  ui.preview_button = button("preview", QStringLiteral("Preview"));
  ui.apply_button = button("apply", QStringLiteral("Apply"));
  auto* cancel_button = button("cancel", QStringLiteral("Cancel"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  for (auto* widget :
       {static_cast<QWidget*>(&window), static_cast<QWidget*>(&window.viewport()),
        static_cast<QWidget*>(ui.dock), static_cast<QWidget*>(ui.sketches),
        static_cast<QWidget*>(ui.diagnostics), static_cast<QWidget*>(ui.suggestions),
        static_cast<QWidget*>(ui.preview_button), static_cast<QWidget*>(ui.apply_button),
        static_cast<QWidget*>(cancel_button)})
    widget->installEventFilter(this);
  connect(ui.sketches, &QComboBox::currentIndexChanged, this, [this] { impl_->inspect(); });
  connect(ui.diagnostics, &QListWidget::currentRowChanged, this,
          [this] { impl_->select_diagnostic(); });
  connect(ui.suggestions, &QComboBox::currentIndexChanged, this,
          [this] { impl_->select_suggestion(); });
  const auto report_result = [this](const Result<std::size_t>& result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(ui.preview_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(preview()); });
  connect(ui.apply_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
}
ShellSketchDiagnosticsTools::~ShellSketchDiagnosticsTools() = default;
bool ShellSketchDiagnosticsTools::active() const noexcept {
  return impl_->active;
}

Result<std::size_t> ShellSketchDiagnosticsTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("Sketch diagnostics require a current part without an active command"));
  ui.editing_sketch = ui.window.active_sketch();
  if (ui.editing_sketch) {
    ui.window.sketch_tools().deactivate();
    ui.window.viewport().clear_sketch_interaction();
  }
  auto& session = ui.window.session();
  (void)session.task().begin(std::string(task_id));
  (void)session.task().begin_parameter_editing();
  ui.active = true;
  ui.revision = session.model_revision();
  {
    QSignalBlocker blocker(ui.sketches);
    ui.sketches->clear();
    for (const auto& sketch : session.part_document()->sketches())
      ui.sketches->addItem(text(sketch.name()) + " / " + text(sketch.id().value()),
                           text(sketch.id().value()));
    if (ui.editing_sketch)
      ui.sketches->setCurrentIndex(ui.sketches->findData(text(ui.editing_sketch->value())));
    ui.sketches->setEnabled(!ui.editing_sketch);
  }
  ui.inspect();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.report.size());
}

Result<std::size_t> ShellSketchDiagnosticsTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen diagnostics."));
  }
  ui.invalidate();
  const auto* suggestion = ui.choice();
  if (!suggestion)
    return Result<std::size_t>::failure(error("Select a repair suggestion"));
  auto sketch = *ui.window.session().part_document()->find_sketch(ui.sketch_id());
  auto changed = SketchRepairCommandExecutor{}.apply(sketch, SketchRepairCommand(*suggestion));
  if (!changed)
    return Result<std::size_t>::failure(changed.error());
  if (!changed.value().applied())
    return Result<std::size_t>::failure(error(changed.value().message()));
  // Copy the complete value session: candidate catalog validation, Geometry and
  // history changes remain disposable, and no preview operation performs I/O.
  auto candidate = ui.window.session();
  auto applied = GuiSketchWorkbench{}.apply_repair(candidate, ui.sketch_id(), *suggestion);
  if (!applied)
    return applied;
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*candidate.part_document(),
                                                           *candidate.part_shape_cache());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  const auto prefix = "sketch/" + ui.sketch_id().value() + "/";
  for (auto& item : scene.value())
    if (item.semantic_id.starts_with(prefix) ||
        item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody) {
      replaced.push_back(item.semantic_id);
      shapes.push_back(std::move(item.shape));
    }
  if (shapes.empty())
    return Result<std::size_t>::failure(error("Suggestion has no displayable geometry"));
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  QStringList ids;
  for (const auto& id : changed.value().changed_constraint_ids())
    ids << text(id.value());
  for (const auto& id : changed.value().changed_dimension_ids())
    ids << text(id.value());
  const auto remaining = SketchConstraintDiagnostics{}.analyze(
      *candidate.part_document()->find_sketch(ui.sketch_id()));
  ui.summary->setText(
      QStringLiteral("Preview valid. Changed constraints/dimensions: %1\nRemaining diagnostics: %2 "
                     "errors, %3 warnings. Apply commits one repair.")
          .arg(ids.join(", "))
          .arg(remaining.error_count())
          .arg(remaining.warning_count()));
  (void)ui.window.session().task().show_preview();
  ui.previewed = true;
  ui.apply_button->setEnabled(true);
  return Result<std::size_t>::success(ids.size());
}

Result<std::size_t> ShellSketchDiagnosticsTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed) {
    if (!ui.current())
      cancel();
    return Result<std::size_t>::failure(error("Check a current repair preview first"));
  }
  auto checked = preview();
  if (!checked)
    return checked;
  const auto suggestion = *ui.choice();
  auto result = GuiSketchWorkbench{}.apply_repair(ui.window.session(), ui.sketch_id(), suggestion);
  cancel();
  return result;
}
void ShellSketchDiagnosticsTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.dock->hide();
  if (ui.editing_sketch == ui.window.active_sketch() && ui.editing_sketch &&
      ui.window.session().part_document() &&
      ui.window.session().part_document()->find_sketch(*ui.editing_sketch))
    ui.window.sketch_tools().activate();
  ui.editing_sketch.reset();
  ui.window.refresh();
}
void ShellSketchDiagnosticsTools::refresh() {
  auto& ui = *impl_;
  if (ui.active && !ui.current()) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
bool ShellSketchDiagnosticsTools::eventFilter(QObject* watched, QEvent* event) {
  if (active() && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
