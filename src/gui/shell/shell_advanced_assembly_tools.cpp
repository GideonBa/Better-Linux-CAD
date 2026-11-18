#include "advanced_assembly_tools_impl.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "assembly.advanced";
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
QString text(std::string_view s) {
  return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}
QString title(ShellAdvancedAssemblyTools::Mode mode) {
  using M = ShellAdvancedAssemblyTools::Mode;
  switch (mode) {
  case M::Flexible:
    return "Flexible Occurrence";
  case M::Adaptive:
    return "Adaptive Reference";
  case M::AssemblyHole:
    return "Assembly Hole";
  case M::Representations:
    return "Positions and Design Views";
  case M::Context:
    return "Edit Part in Context";
  }
  return {};
}
const char* action_id(ShellAdvancedAssemblyTools::Mode mode) {
  using M = ShellAdvancedAssemblyTools::Mode;
  switch (mode) {
  case M::Flexible:
    return "assembly_flexible_occurrence";
  case M::Adaptive:
    return "assembly_adaptive_reference";
  case M::AssemblyHole:
    return "assembly_hole";
  case M::Representations:
    return "assembly_representations";
  case M::Context:
    return "assembly_edit_part_context";
  }
  return "assembly_advanced";
}
QPushButton* button(QWidget* parent, QVBoxLayout* layout, const char* id, const char* label) {
  auto* result = new QPushButton(label, parent);
  result->setObjectName(QStringLiteral("blcad.advanced_assembly.") + id);
  layout->addWidget(result);
  return result;
}
QComboBox* choice(QWidget* parent, QFormLayout* form, const char* id, const char* label) {
  auto* result = new QComboBox(parent);
  result->setObjectName(QStringLiteral("blcad.advanced_assembly.") + id);
  result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  result->setMinimumContentsLength(14);
  form->addRow(label, result);
  return result;
}
} // namespace
ShellAdvancedAssemblyTools::ShellAdvancedAssemblyTools(ShellWindow& window, ShellRibbon& ribbon,
                                                       int tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window, *this)) {
  auto& p = *impl_;
  auto* group = ribbon.add_group(tab, "Design Context");
  for (auto mode :
       {Mode::Flexible, Mode::Adaptive, Mode::AssemblyHole, Mode::Representations, Mode::Context}) {
    auto* action = new QAction(title(mode), this);
    action->setObjectName(QStringLiteral("blcad.action.") + action_id(mode));
    connect(action, &QAction::triggered, this, [this, mode] {
      auto result = begin(mode);
      if (!result)
        impl_->window.report_error(result.error());
    });
    ribbon.add_small_button(group, action);
    p.actions.emplace_back(mode, action);
  }
  p.dock = new QDockWidget("Assembly Design", &window);
  p.dock->setObjectName("blcad.shell.advanced_assembly_task");
  p.dock->setFeatures(QDockWidget::DockWidgetMovable);
  auto* contents = new QWidget(p.dock);
  auto* column = new QVBoxLayout(contents);
  column->setContentsMargins(6, 6, 6, 6);
  p.breadcrumb = new QWidget(contents);
  auto* trail_column = new QVBoxLayout(p.breadcrumb);
  trail_column->setContentsMargins(0, 0, 0, 0);
  p.trail = new QLabel(p.breadcrumb);
  p.trail->setWordWrap(true);
  p.trail->setObjectName("blcad.advanced_assembly.breadcrumb");
  trail_column->addWidget(p.trail);
  auto* finish = button(p.breadcrumb, trail_column, "finish_part", "Finish Part → Assembly");
  auto* cancel_part =
      button(p.breadcrumb, trail_column, "cancel_part", "Cancel Part edits → Assembly");
  connect(finish, &QPushButton::clicked, this,
          [this] { (void)impl_->window.finish_project_part_context(true); });
  connect(cancel_part, &QPushButton::clicked, this,
          [this] { (void)impl_->window.finish_project_part_context(false); });
  column->addWidget(p.breadcrumb);
  auto* scroll = new QScrollArea(contents);
  scroll->setObjectName("blcad.advanced_assembly.scroll");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* panel = new QWidget(scroll);
  auto* inputs = new QVBoxLayout(panel);
  p.summary = new QLabel(panel);
  p.summary->setWordWrap(true);
  p.summary->setObjectName("blcad.advanced_assembly.summary");
  inputs->addWidget(p.summary);
  p.record = new QComboBox(panel);
  p.record->setObjectName("blcad.advanced_assembly.record");
  inputs->addWidget(p.record);
  p.name = new QLineEdit(panel);
  p.name->setObjectName("blcad.advanced_assembly.name");
  p.name->setPlaceholderText("Reference or representation name");
  inputs->addWidget(p.name);
  p.pages = new QStackedWidget(panel);
  inputs->addWidget(p.pages);
  auto* flex = new QWidget(panel);
  auto* flex_form = new QFormLayout(flex);
  p.occurrence = choice(flex, flex_form, "occurrence", "Rooted subassembly");
  p.flexibility = choice(flex, flex_form, "flexibility", "Internal pose");
  p.flexibility->addItems({"Rigid definition pose", "Flexible occurrence pose"});
  p.joint = choice(flex, flex_form, "joint", "Local joint");
  p.coordinate = choice(flex, flex_form, "coordinate", "Coordinate");
  p.value = new QDoubleSpinBox(flex);
  p.value->setObjectName("blcad.advanced_assembly.coordinate_value");
  p.value->setRange(-1e9, 1e9);
  p.value->setDecimals(8);
  p.value->setKeyboardTracking(false);
  flex_form->addRow("Value", p.value);
  p.pages->addWidget(flex);
  auto* adaptive = new QWidget(panel);
  auto* adaptive_form = new QFormLayout(adaptive);
  p.source = choice(adaptive, adaptive_form, "source", "Source occurrence");
  p.source_state = choice(adaptive, adaptive_form, "source_state", "Source model state");
  p.geometry_editor = choice(adaptive, adaptive_form, "source_geometry", "Source geometry");
  p.target = choice(adaptive, adaptive_form, "target", "Driving target occurrence");
  p.construction = choice(adaptive, adaptive_form, "construction", "Target reference");
  p.pages->addWidget(adaptive);
  auto* machining = new QWidget(panel);
  auto* machining_column = new QVBoxLayout(machining);
  auto* machining_form = new QFormLayout;
  p.layout = choice(machining, machining_form, "layout", "Source Hole layout");
  p.layout_state = choice(machining, machining_form, "layout_state", "Layout model state");
  p.pattern = choice(machining, machining_form, "pattern", "Shared placement pattern");
  machining_column->addLayout(machining_form);
  auto* hint = new QLabel("Select existing Hole recipes below. Create a recipe "
                          "with Edit Part in Context, "
                          "then return here. Shared Part changes require an "
                          "explicit participant policy.",
                          machining);
  hint->setWordWrap(true);
  machining_column->addWidget(hint);
  p.participants = new QTableWidget(machining);
  p.participants->setObjectName("blcad.advanced_assembly.participants");
  p.participants->setColumnCount(6);
  p.participants->setHorizontalHeaderLabels(
      {"Use", "Occurrence", "Hole recipe", "Model state", "Role", "All occurrences"});
  p.participants->setMinimumHeight(200);
  p.participants->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  machining_column->addWidget(p.participants);
  p.pages->addWidget(machining);
  auto* reps = new QWidget(panel);
  auto* reps_column = new QVBoxLayout(reps);
  auto* reps_form = new QFormLayout;
  p.positions = choice(reps, reps_form, "position", "Position");
  p.views = choice(reps, reps_form, "design_view", "Design view");
  reps_column->addLayout(reps_form);
  auto* recall = button(reps, reps_column, "recall_position", "Recall selected position");
  auto* store = button(reps, reps_column, "store_position", "Store current position");
  auto* recall_view = button(reps, reps_column, "recall_view", "Recall selected design view");
  auto* rename_position = button(reps, reps_column, "rename_position", "Rename selected position");
  auto* delete_position = button(reps, reps_column, "delete_position", "Delete selected position");
  auto* delete_view = button(reps, reps_column, "delete_view", "Delete selected design view");
  const auto change_position = [this](bool remove) {
    auto& p = *impl_;
    if (!p.current())
      return;
    const AssemblyPositionId id(p.positions->currentData().toString().toStdString());
    auto positions = p.window.session().project()->assembly_positions();
    auto found = std::find_if(positions.begin(), positions.end(),
                              [&](const auto& value) { return value.id == id; });
    if (found == positions.end()) {
      p.summary->setText("Master is the authoritative root pose and cannot be "
                         "renamed or deleted.");
      return;
    }
    if (remove)
      positions.erase(found);
    else
      found->name = p.name->text().trimmed().toStdString();
    auto changed = p.window.session().commit_assembly_representations(
        std::move(positions), p.window.session().project()->assembly_design_views(),
        remove ? "Delete Assembly Position" : "Rename Assembly Position");
    if (!changed) {
      p.summary->setText(text(changed.error().message()));
      return;
    }
    cancel();
    p.window.refresh();
  };
  connect(rename_position, &QPushButton::clicked, this,
          [change_position] { change_position(false); });
  connect(delete_position, &QPushButton::clicked, this,
          [change_position] { change_position(true); });
  connect(delete_view, &QPushButton::clicked, this, [this] {
    auto& p = *impl_;
    if (!p.current())
      return;
    const AssemblyDesignViewId id(p.views->currentData().toString().toStdString());
    auto views = p.window.session().project()->assembly_design_views();
    const auto removed = std::erase_if(views, [&](const auto& value) { return value.id == id; });
    if (!removed) {
      p.summary->setText("Default retains authored component visibility and "
                         "cannot be deleted.");
      return;
    }
    auto changed = p.window.session().commit_assembly_representations(
        p.window.session().project()->assembly_positions(), std::move(views),
        "Delete Assembly Design View");
    if (!changed) {
      p.summary->setText(text(changed.error().message()));
      return;
    }
    cancel();
    p.window.refresh();
  });
  p.display = new QTableWidget(reps);
  p.display->setObjectName("blcad.advanced_assembly.display");
  p.display->setColumnCount(2);
  p.display->setHorizontalHeaderLabels({"Visible", "Rooted component or Assembly"});
  p.display->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  p.display->setMinimumHeight(180);
  reps_column->addWidget(p.display);
  p.pages->addWidget(reps);
  connect(recall, &QPushButton::clicked, this, [this] {
    auto id = AssemblyPositionId(impl_->positions->currentData().toString().toStdString());
    auto result = recall_position(id);
    if (!result)
      impl_->window.report_error(result.error());
  });
  connect(recall_view, &QPushButton::clicked, this, [this] {
    auto id = AssemblyDesignViewId(impl_->views->currentData().toString().toStdString());
    auto result = recall_design_view(id);
    if (!result)
      impl_->window.report_error(result.error());
  });
  connect(store, &QPushButton::clicked, this, [this] {
    auto result = store_position(
        AssemblyPositionId("position." +
                           QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()),
        impl_->name->text().trimmed().toStdString());
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  auto* context = new QWidget(panel);
  auto* context_column = new QVBoxLayout(context);
  auto* context_form = new QFormLayout;
  p.context_part = choice(context, context_form, "context_part", "Rooted Part occurrence");
  context_column->addLayout(context_form);
  auto* edit = button(context, context_column, "edit_part", "Edit Part in context");
  connect(edit, &QPushButton::clicked, this, [this] {
    auto result = impl_->edit_part();
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  p.pages->addWidget(context);
  p.dependencies = new QLabel(panel);
  p.dependencies->setObjectName("blcad.advanced_assembly.dependencies");
  p.dependencies->setWordWrap(true);
  inputs->addWidget(p.dependencies);
  inputs->addStretch();
  scroll->setWidget(panel);
  column->addWidget(scroll, 1);
  p.detach = button(contents, column, "detach", "Detach selected coordination");
  auto* footer = new QHBoxLayout;
  p.preview = new QPushButton("Preview", contents);
  p.preview->setObjectName("blcad.advanced_assembly.preview");
  p.apply = new QPushButton("Apply", contents);
  p.apply->setObjectName("blcad.advanced_assembly.apply");
  auto* cancel_button = new QPushButton("Cancel", contents);
  cancel_button->setObjectName("blcad.advanced_assembly.cancel");
  footer->addWidget(p.preview);
  footer->addWidget(p.apply);
  footer->addWidget(cancel_button);
  column->addLayout(footer);
  p.dock->setWidget(contents);
  window.addDockWidget(Qt::RightDockWidgetArea, p.dock);
  p.dock->hide();
  connect(p.preview, &QPushButton::clicked, this, [this] {
    auto result = preview();
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  connect(p.apply, &QPushButton::clicked, this, [this] {
    auto result = apply();
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  connect(cancel_button, &QPushButton::clicked, this, [this] {
    if (impl_->window.session().project_part_context_active()) {
      (void)impl_->window.finish_project_part_context(false);
      return;
    }
    cancel();
    impl_->window.refresh();
  });
  connect(p.detach, &QPushButton::clicked, this, [this] {
    auto result = impl_->detach_record();
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  connect(p.occurrence, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_flexible();
      impl_->invalidate();
    }
  });
  connect(p.joint, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_coordinate();
      impl_->invalidate();
    }
  });
  connect(p.coordinate, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_coordinate();
      impl_->invalidate();
    }
  });
  connect(p.source, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_source();
      impl_->invalidate();
    }
  });
  connect(p.target, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_target();
      impl_->invalidate();
    }
  });
  connect(p.geometry_editor, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_target();
      impl_->invalidate();
    }
  });
  connect(p.layout, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_layout();
      impl_->invalidate();
    }
  });
  connect(p.views, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_view();
      impl_->invalidate();
    }
  });
  connect(p.record, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_record();
      impl_->invalidate();
    }
  });
  connect(p.flexibility, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->active && !impl_->loading) {
      impl_->load_coordinate();
      impl_->invalidate();
    }
  });
  for (auto* input : {p.source_state, p.construction, p.layout_state, p.pattern})
    connect(input, &QComboBox::currentIndexChanged, this, [this] { impl_->invalidate(); });
  connect(p.value, &QDoubleSpinBox::valueChanged, this, [this] { impl_->invalidate(); });
  connect(p.name, &QLineEdit::textChanged, this, [this] { impl_->invalidate(); });
  connect(p.display, &QTableWidget::itemChanged, this, [this] { impl_->invalidate(); });
  connect(p.participants, &QTableWidget::itemChanged, this, [this] { impl_->invalidate(); });
  window.installEventFilter(this);
}
ShellAdvancedAssemblyTools::~ShellAdvancedAssemblyTools() {
  cancel();
}
bool ShellAdvancedAssemblyTools::Impl::available(Mode requested) const {
  const auto& s = window.session();
  return s.project() && !s.reference_repair_mode() && s.derived_results_fresh() &&
         !s.project_part_context_active() && !window.active_sketch() && !s.part_workflow_active() &&
         !s.task().active() &&
         (requested == Mode::Representations ||
          (s.active_model_state() == primary_model_state_id() &&
           s.active_position() == master_position_id() &&
           (requested != Mode::Context || s.active_design_view() == default_design_view_id())));
}
bool ShellAdvancedAssemblyTools::Impl::current() const {
  return active && window.session().project() && window.session().model_revision() == revision &&
         window.session().task().command_id() == task_id;
}
void ShellAdvancedAssemblyTools::Impl::invalidate() {
  if (loading || !active)
    return;
  candidate.reset();
  apply->setEnabled(false);
  window.viewport().clear_feature_preview();
  if (window.session().task().command_id() == task_id)
    (void)window.session().task().return_to_editing();
  update_dependencies();
}
Result<std::size_t> ShellAdvancedAssemblyTools::begin(Mode mode) {
  auto& p = *impl_;
  if (p.active)
    cancel();
  if (!p.available(mode))
    return Result<std::size_t>::failure(
        error("Finish the active command and choose Primary / Master at the "
              "root Assembly; "
              "Edit Part in Context also requires the Default design view"));
  p.mode = mode;
  p.active = true;
  p.revision = p.window.session().model_revision();
  p.prior_mask = p.window.viewport().selection_filter_mask();
  p.prior_scope = p.window.viewport().selection_semantic_scope();
  if (!p.window.session().task().begin(std::string(task_id))) {
    p.active = false;
    return Result<std::size_t>::failure(error("Another task owns the document"));
  }
  p.window.viewport().set_selection_filter_mask(0);
  p.window.viewport().set_selection_semantic_scope(std::vector<std::string>{});
  p.window.session().selection().clear();
  p.link_id = ProjectDesignReferenceId(
      "reference." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
  p.hole_id = FeatureId("assembly.hole." +
                        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
  p.load();
  p.pages->setCurrentIndex(int(mode));
  p.record->setVisible(mode == Mode::Adaptive || mode == Mode::AssemblyHole);
  p.name->setVisible(mode != Mode::Flexible && mode != Mode::Context);
  p.detach->setVisible(mode == Mode::Adaptive || mode == Mode::AssemblyHole);
  p.preview->setVisible(mode != Mode::Context);
  p.apply->setVisible(mode != Mode::Context);
  p.apply->setEnabled(false);
  p.dock->setWindowTitle(title(mode));
  p.dock->show();
  p.dock->raise();
  p.window.refresh();
  return Result<std::size_t>::success(1);
}
Result<std::size_t>
ShellAdvancedAssemblyTools::Impl::publish_preview(GuiDocumentSession& proposed) {
  auto imports = make_step_import_context(proposed);
  if (!imports)
    return Result<std::size_t>::failure(imports.error());
  geometry::ViewportSceneBuilder builder(imports.value().get());
  auto scene = builder.build_project_for_states(
      *proposed.effective_project(), [&](DocumentId id, ModelStateId state) {
        return proposed.project_part_shape_cache(id, state);
      });
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  auto original = builder.build_project_for_states(
      *window.session().effective_project(), [&](DocumentId id, ModelStateId state) {
        return window.session().project_part_shape_cache(id, state);
      });
  if (!original)
    return Result<std::size_t>::failure(original.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  for (auto& item : scene.value())
    shapes.push_back(std::move(item.shape));
  for (const auto& item : original.value())
    replaced.push_back(item.semantic_id);
  return window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
}
Result<std::size_t> ShellAdvancedAssemblyTools::preview() {
  auto& p = *impl_;
  if (!p.current())
    return Result<std::size_t>::failure(error("The Assembly task is stale"));
  p.invalidate();
  auto proposed = p.window.session();
  (void)proposed.task().cancel();
  auto changed = p.mode == Mode::Representations
                     ? [&] {
                         auto result = p.mutate(*proposed.project());
                         if (!result)
                           return result;
                         auto stored = proposed.commit_assembly_representations(
                             p.draft_positions, p.draft_views, "Preview design view");
                         if (!stored)
                           return stored;
                         return proposed.activate_assembly_design_view(p.preview_view);
                       }()
                     : proposed.commit_project_transaction(p.label(), [&](Project& candidate) {
                         return p.mutate(candidate);
                       });
  if (!changed) {
    p.summary->setText(text(changed.error().message()));
    return changed;
  }
  auto shown = p.publish_preview(proposed);
  if (!shown)
    return shown;
  p.candidate = *proposed.project();
  p.apply->setEnabled(true);
  (void)p.window.session().task().show_preview();
  p.summary->setText("Complete Assembly preview checked. Apply publishes one "
                     "root transaction.");
  return shown;
}
Result<std::size_t> ShellAdvancedAssemblyTools::apply() {
  auto& p = *impl_;
  if (!p.current() || !p.candidate)
    return Result<std::size_t>::failure(
        error("Preview the current complete Assembly before Apply"));
  auto changed = p.mode == Mode::Representations
                     ? p.window.session().commit_assembly_representations(
                           p.draft_positions, p.draft_views, "Edit Assembly Design View")
                     : p.window.session().commit_project_transaction(p.label(), [&](Project& root) {
                         root = *p.candidate;
                         return Result<std::size_t>::success(1);
                       });
  if (!changed) {
    p.revision = p.window.session().model_revision();
    p.invalidate();
    p.summary->setText(text(changed.error().message()));
    return changed;
  }
  const auto view = p.preview_view;
  const bool activate = p.mode == Mode::Representations;
  cancel();
  if (activate) {
    auto selected = p.window.session().activate_assembly_design_view(view);
    if (!selected)
      return selected;
  }
  p.window.refresh();
  return changed;
}
Result<std::size_t> ShellAdvancedAssemblyTools::store_position(AssemblyPositionId id,
                                                               std::string name) {
  auto& p = *impl_;
  if (!p.current() || p.mode != Mode::Representations)
    return Result<std::size_t>::failure(error("Open Positions and Design Views first"));
  auto captured = capture_assembly_position(*p.window.session().effective_project(), std::move(id),
                                            std::move(name));
  if (!captured)
    return Result<std::size_t>::failure(captured.error());
  auto catalog = p.window.session().project()->assembly_positions();
  catalog.push_back(std::move(captured.value()));
  auto stored = p.window.session().commit_assembly_representations(
      std::move(catalog), p.window.session().project()->assembly_design_views(),
      "Store Assembly Position");
  if (stored) {
    cancel();
    p.window.refresh();
  }
  return stored;
}
Result<std::size_t> ShellAdvancedAssemblyTools::recall_position(AssemblyPositionId id) {
  cancel();
  auto result = impl_->window.session().activate_assembly_position(std::move(id));
  impl_->window.refresh();
  return result;
}
Result<std::size_t> ShellAdvancedAssemblyTools::recall_design_view(AssemblyDesignViewId id) {
  cancel();
  auto result = impl_->window.session().activate_assembly_design_view(std::move(id));
  impl_->window.refresh();
  return result;
}
void ShellAdvancedAssemblyTools::cancel() {
  auto& p = *impl_;
  if (!p.active)
    return;
  p.active = false;
  p.candidate.reset();
  p.window.viewport().clear_feature_preview();
  p.window.viewport().set_selection_filter_mask(p.prior_mask);
  p.window.viewport().set_selection_semantic_scope(p.prior_scope);
  if (p.window.session().task().command_id() == task_id)
    (void)p.window.session().task().cancel();
  p.dock->hide();
}
void ShellAdvancedAssemblyTools::refresh() {
  auto& p = *impl_;
  if (p.active && !p.current())
    cancel();
  for (const auto& [mode, action] : p.actions)
    action->setEnabled(p.active || p.available(mode));
  const auto* root = p.window.session().project_part_context_root();
  p.breadcrumb->setVisible(root);
  if (root) {
    p.pages->hide();
    p.record->hide();
    p.name->hide();
    p.preview->hide();
    p.apply->hide();
    p.detach->hide();
    p.dependencies->hide();
    p.trail->setText(text(root->name()) + " / " + text(p.window.session().display_name()) +
                     "\nPart coordinates · Finish publishes one root edit; "
                     "Cancel restores the Assembly.");
    p.summary->setText("Editing the shared Primary Part definition. Finish updates every "
                       "occurrence in one checked Assembly transaction.");
    p.dock->setWindowTitle("Part in Assembly");
    p.dock->show();
  } else {
    p.pages->show();
    p.dependencies->show();
    if (!p.active)
      p.dock->hide();
  }
}
bool ShellAdvancedAssemblyTools::active() const noexcept {
  return impl_->active;
}
bool ShellAdvancedAssemblyTools::eventFilter(QObject*, QEvent* event) {
  if (active() && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    impl_->window.refresh();
    return true;
  }
  return false;
}
QAction& ShellAdvancedAssemblyTools::action(Mode mode) noexcept {
  for (const auto& value : impl_->actions)
    if (value.first == mode)
      return *value.second;
  return *impl_->actions.front().second;
}
QComboBox& ShellAdvancedAssemblyTools::occurrence_editor() noexcept {
  return *impl_->occurrence;
}
QComboBox& ShellAdvancedAssemblyTools::flexibility_editor() noexcept {
  return *impl_->flexibility;
}
QComboBox& ShellAdvancedAssemblyTools::joint_editor() noexcept {
  return *impl_->joint;
}
QComboBox& ShellAdvancedAssemblyTools::coordinate_editor() noexcept {
  return *impl_->coordinate;
}
QDoubleSpinBox& ShellAdvancedAssemblyTools::coordinate_value_editor() noexcept {
  return *impl_->value;
}
QComboBox& ShellAdvancedAssemblyTools::source_editor() noexcept {
  return *impl_->source;
}
QComboBox& ShellAdvancedAssemblyTools::source_geometry_editor() noexcept {
  return *impl_->geometry_editor;
}
QComboBox& ShellAdvancedAssemblyTools::target_editor() noexcept {
  return *impl_->target;
}
QComboBox& ShellAdvancedAssemblyTools::target_construction_editor() noexcept {
  return *impl_->construction;
}
QComboBox& ShellAdvancedAssemblyTools::layout_editor() noexcept {
  return *impl_->layout;
}
QTableWidget& ShellAdvancedAssemblyTools::participant_editor() noexcept {
  return *impl_->participants;
}
QComboBox& ShellAdvancedAssemblyTools::position_editor() noexcept {
  return *impl_->positions;
}
QComboBox& ShellAdvancedAssemblyTools::design_view_editor() noexcept {
  return *impl_->views;
}
QTableWidget& ShellAdvancedAssemblyTools::display_editor() noexcept {
  return *impl_->display;
}
QLineEdit& ShellAdvancedAssemblyTools::name_editor() noexcept {
  return *impl_->name;
}
} // namespace blcad::gui
