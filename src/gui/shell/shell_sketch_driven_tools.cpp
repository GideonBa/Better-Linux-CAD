#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "sketch_driven_tools_impl.hpp"
#include <QApplication>
#include <QSignalBlocker>
namespace blcad::gui {
namespace {
constexpr auto task = "part.sketch_driven";
Error error(std::string message) {
  return Error::validation(task, std::move(message));
}
} // namespace
ShellSketchDrivenTools::ShellSketchDrivenTools(ShellWindow& window, ShellRibbon& ribbon, int tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window, *this)) {
  auto& p = *impl_;
  auto* group = ribbon.add_group(tab, "Ribs and Labels");
  const char* labels[]{"Rib", "Web", "Emboss", "Engrave"};
  const char* ids[]{"rib", "web", "emboss", "engrave"};
  for (int i = 0; i < 4; ++i) {
    p.actions[i] = new QAction(labels[i], this);
    p.actions[i]->setObjectName(QString("blcad.action.") + ids[i]);
    ribbon.add_small_button(group, p.actions[i]);
    connect(p.actions[i], &QAction::triggered, this, [this, i] {
      auto result = begin(static_cast<Mode>(i));
      if (!result)
        impl_->window.report_error(result.error());
    });
  }
  p.dock = new QDockWidget("Sketch-driven Feature", &window);
  p.dock->setObjectName("blcad.shell.sketch_driven_task");
  p.ui = std::make_unique<detail::SketchDrivenControls>(p.dock);
  p.dock->setWidget(p.ui->panel);
  window.addDockWidget(Qt::RightDockWidgetArea, p.dock);
  p.dock->hide();
  p.overlay = make_shell_manipulator_overlay(&window.viewport(), p.layer);
  p.overlay->hide();
  for (const auto& [key, field] : p.ui->fields) {
    if (auto* line = qobject_cast<QLineEdit*>(field); line && key != "grip_numeric")
      connect(line, &QLineEdit::textChanged, this, [this] { impl_->edited(); });
    if (auto* combo = qobject_cast<QComboBox*>(field))
      connect(combo, &QComboBox::currentIndexChanged, this, [this] { impl_->edited(); });
  }
  for (auto* binding : p.ui->bindings())
    connect(binding->parameter, &QComboBox::currentIndexChanged, this, [this, binding] {
      auto& p = *impl_;
      if (p.loading || !p.current())
        return;
      p.loading = true;
      p.ui->select_binding(*p.window.session().part_document(), *binding);
      p.loading = false;
      p.edited();
    });
  connect(p.ui->grip_numeric, &QLineEdit::textEdited, this,
          [this](const QString& text) { (void)set_drag_text(text); });
  connect(p.ui->mode, &QComboBox::currentIndexChanged, this, [this] {
    auto& p = *impl_;
    if (p.loading)
      return;
    p.loading = true;
    if (p.ui->mode->currentIndex() >= 2 && p.ui->source_kind->currentIndex() == 0)
      p.ui->source_kind->setCurrentIndex(1);
    if (p.ui->mode->currentIndex() < 2 && p.ui->source_kind->currentIndex() == 2)
      p.ui->source_kind->setCurrentIndex(0);
    p.loading = false;
    p.edited();
  });
  connect(p.ui->sketch, &QComboBox::currentIndexChanged, this, [this] {
    auto& p = *impl_;
    if (p.loading)
      return;
    p.loading = true;
    p.reload_sources(true);
    p.loading = false;
    p.edited();
  });
  connect(p.ui->mapping, &QComboBox::currentIndexChanged, this, [this] {
    auto& p = *impl_;
    if (p.loading || !p.current())
      return;
    p.reload_targets();
  });
  connect(p.ui->body, &QComboBox::currentIndexChanged, this, [this] {
    auto& p = *impl_;
    if (p.loading)
      return;
    p.loading = true;
    p.ui->supports->clear();
    p.reload_targets();
    p.loading = false;
    p.edited();
  });
  for (auto* checkbox : {p.ui->draft_enabled, p.ui->orientation_enabled})
    connect(checkbox, &QCheckBox::toggled, this, [this] { impl_->edited(); });
  for (auto* list : {p.ui->curves, p.ui->supports}) {
    connect(list->model(), &QAbstractItemModel::rowsRemoved, this, [this] { impl_->edited(); });
    connect(list->model(), &QAbstractItemModel::rowsInserted, this, [this] { impl_->edited(); });
  }
  connect(p.ui->tokens, &QTableWidget::itemChanged, this, [this] { impl_->edited(); });
  connect(p.ui->tokens->model(), &QAbstractItemModel::rowsRemoved, this,
          [this] { impl_->edited(); });
  connect(p.ui->curve_add, &QPushButton::clicked, this, [this] { impl_->add_curve(); });
  connect(p.ui->face_add, &QPushButton::clicked, this, [this] { impl_->add_face(); });
  connect(p.ui->center_axis, &QPushButton::clicked, this, [this] {
    auto& p = *impl_;
    if (!p.current())
      return;
    const auto key = (p.ui->supports->count() ? p.ui->supports->item(0)->data(Qt::UserRole)
                                              : p.ui->face->currentData())
                         .toString()
                         .toStdString();
    for (const auto& target : p.targets)
      if (interactive_face_identity(std::get<FaceReference>(target.selection.reference)) == key &&
          target.axial_center_mm) {
        p.ui->anchor_y->setText(QString::number(*target.axial_center_mm, 'g', 17));
        return;
      }
    p.fail(error("Select an analytic cylinder or cone support first."));
  });
  connect(p.ui->token_add, &QPushButton::clicked, this, [this] { impl_->add_token(); });
  connect(p.ui->preview, &QPushButton::clicked, this, [this] { (void)preview(); });
  connect(p.ui->apply, &QPushButton::clicked, this, [this] { (void)apply(); });
  connect(p.ui->cancel, &QPushButton::clicked, this, [this] { cancel(); });
  connect(p.ui->source_edit, &QPushButton::clicked, this, [this] {
    auto result = edit_source_sketch();
    if (!result)
      impl_->fail(result.error());
  });
  connect(p.ui->breadcrumb, &QPushButton::clicked, this,
          [this] { (void)impl_->window.finish_active_sketch(); });
  connect(p.dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    if (!visible && impl_->active && !impl_->source_edit)
      cancel();
  });
  qApp->installEventFilter(this);
  refresh();
}
ShellSketchDrivenTools::~ShellSketchDrivenTools() {
  cancel();
  delete impl_->overlay;
}
bool ShellSketchDrivenTools::Impl::current() const {
  return active && window.session().part_document() &&
         window.session().model_revision() == revision &&
         window.session().task().command_id() == task && !source_edit;
}
void ShellSketchDrivenTools::Impl::edited() {
  if (loading || !active || source_edit)
    return;
  valid = false;
  ui->apply->setEnabled(false);
  (void)show_inputs();
  if (layer.active_handle_id().empty())
    (void)layer.set_handles({});
  ui->render();
  ui->status->setText("Check the source, support and bindings, then preview.");
  overlay_refresh();
}
Result<std::size_t> ShellSketchDrivenTools::Impl::prepare_inputs(const PartDocument& p,
                                                                 std::size_t count) {
  auto cache = geometry::ShapeCache::create(ShapeCacheId("sketch_driven.inputs"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto computed = query_with_step_import_context(window.session(), [&](auto* context) {
    return geometry::GeometryRecomputeExecutor(context).execute_history_prefix(p, count,
                                                                               cache.value());
  });
  if (!computed)
    return Result<std::size_t>::failure(computed.error());
  inputs = p;
  input_cache = std::move(cache.value());
  prefix = count;
  return Result<std::size_t>::success(count);
}
Result<std::size_t> ShellSketchDrivenTools::Impl::start(std::optional<FeatureId> id, Mode mode) {
  auto& session = window.session();
  const auto* part = session.part_document();
  if (active || window.active_sketch() || session.task().active() || !part ||
      part->is_reference_part() ||
      (!session.part_modeling_ready() && !(id && session.reference_repair_mode())))
    return Result<std::size_t>::failure(error("Choose an editable Part with no active command. "
                                              "Repair mode permits an existing feature."));
  const auto history = part->executable_history();
  if (!history)
    return Result<std::size_t>::failure(history.error());
  editing = id.has_value();
  repair = session.reference_repair_mode();
  feature = id.value_or(FeatureId(""));
  const SketchDrivenFeature* record = nullptr;
  std::size_t count = session.history_prefix_count().value_or(history.value().size());
  if (id) {
    record = part->find_sketch_driven_feature(*id);
    if (!record)
      return Result<std::size_t>::failure(error("Sketch-driven feature is unavailable."));
    auto found = std::find_if(history.value().begin(), history.value().end(),
                              [&](const auto& row) { return row.node_id == id->value(); });
    if (found == history.value().end())
      return Result<std::size_t>::failure(error("Feature history is unavailable."));
    count = std::size_t(found - history.value().begin());
    if (session.history_prefix_count() && count >= *session.history_prefix_count())
      return Result<std::size_t>::failure(
          error("Move End of Part after this feature before editing it."));
  } else {
    for (index = 1;; ++index) {
      feature = FeatureId("sketch_driven." + std::to_string(index));
      bool used = part->dependency_graph().has_node(feature.value());
      for (const auto* suffix : {"size", "second", "distance", "draft", "height", "orientation"})
        used |= part->find_parameter(ParameterId(feature.value() + "." + suffix)) != nullptr;
      if (!used)
        break;
    }
  }
  auto prepared = prepare_inputs(*part, count);
  if (!prepared)
    return prepared;
  if (!std::any_of(part->bodies().begin(), part->bodies().end(),
                   [&](const auto& body) {
                     return body.kind() == BodyKind::Solid &&
                            input_cache->find_body_shape(body.id());
                   }) ||
      part->sketches().empty())
    return Result<std::size_t>::failure(error("Create a Solid target and a source Sketch first."));
  old_selection = window.viewport().selected_semantic();
  old_mask = window.viewport().selection_filter_mask();
  old_scope = window.viewport().selection_semantic_scope();
  workflow = false;
  history_command = false;
  if (!repair) {
    auto begun = session.history_prefix_count() ? session.begin_part_history_command(task)
                                                : session.begin_part_workflow();
    if (!begun)
      return begun;
    history_command = session.history_prefix_count().has_value();
    workflow = !history_command;
  }
  active = true;
  revision = session.model_revision();
  loading = true;
  ui->mode->setCurrentIndex(int(mode));
  if (!record) {
    ui->source_kind->setCurrentIndex(int(mode) < 2 ? 0 : 1);
    ui->side->setCurrentIndex(0);
    ui->plane->setCurrentIndex(0);
    ui->extent->setCurrentIndex(0);
    ui->draft_enabled->setChecked(false);
    ui->orientation_enabled->setChecked(false);
    ui->mapping->setCurrentIndex(0);
    ui->anchor_x->setText("0");
    ui->anchor_y->setText("0");
    ui->seam->setText("0");
    for (int i = 0; i < 3; ++i)
      ui->direction[i]->setText(i == 2 ? "1" : "0");
  }
  load(record);
  loading = false;
  (void)session.task().begin(task);
  (void)session.task().begin_parameter_editing();
  valid = false;
  ui->apply->setEnabled(false);
  ui->breadcrumb->hide();
  ui->scroll->setEnabled(true);
  ui->render();
  ui->grip_numeric->setEnabled(false);
  window.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body) |
                                              selection_kind_bit(GuiSelectionKind::Face) |
                                              selection_kind_bit(GuiSelectionKind::SketchEntity));
  window.viewport().set_selection_semantic_scope(std::nullopt);
  for (const auto& selection : session.selection().entries())
    (void)owner.accept_selection(selection);
  (void)show_inputs();
  std::vector<geometry::ViewportSceneItem> picks;
  for (const auto& target : targets)
    picks.push_back(
        {geometry::ViewportSceneKind::Face, target.selection.semantic_id, target.selection.shape});
  (void)window.viewport().set_preview_selection_targets(std::move(picks));
  dock->show();
  dock->raise();
  ui->status->setText(
      QString("Input history: %1 feature(s). Select sources and preview.").arg(prefix));
  window.refresh();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellSketchDrivenTools::begin(Mode mode) {
  return impl_->start(std::nullopt, mode);
}
Result<std::size_t> ShellSketchDrivenTools::begin_edit(FeatureId id) {
  return impl_->start(id, Mode::Rib);
}
bool ShellSketchDrivenTools::active() const noexcept {
  return impl_->active;
}
QWidget* ShellSketchDrivenTools::field(const QString& name) const {
  auto found = impl_->ui->fields.find(name);
  return found == impl_->ui->fields.end() ? nullptr : found->second;
}
GuiViewportManipulatorLayer& ShellSketchDrivenTools::manipulators() {
  return impl_->layer;
}
void ShellSketchDrivenTools::refresh() {
  auto& p = *impl_;
  if (p.active && !p.source_edit && !p.current()) {
    cancel();
    return;
  }
  const auto* part = p.window.session().part_document();
  const bool enabled =
      !p.active && !p.window.active_sketch() && !p.window.session().task().active() &&
      p.window.session().part_modeling_ready() && part && !part->is_reference_part();
  for (auto* action : p.actions)
    action->setEnabled(enabled);
  if (p.active)
    p.overlay_refresh();
}
Result<std::size_t> ShellSketchDrivenTools::apply() {
  auto& p = *impl_;
  if (!p.current() || !p.valid || !p.layer.active_handle_id().empty())
    return Result<std::size_t>::failure(
        error("Check a valid current preview and finish the active grip first."));
  auto checked = preview();
  if (!checked)
    return checked;
  auto candidate = p.candidate();
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  auto staged = p.window.session();
  const auto mutation = [&](PartDocument& part) {
    part = candidate.value();
    return Result<std::size_t>::success(1);
  };
  auto committed =
      p.repair ? staged.commit_part_repair_transaction("Edit sketch-driven feature", mutation)
               : staged.commit_part_transaction(p.editing ? "Edit sketch-driven feature"
                                                          : "Create sketch-driven feature",
                                                mutation);
  if (!committed) {
    p.fail(committed.error());
    return committed;
  }
  if (p.workflow) {
    committed = staged.publish_part_workflow(p.editing ? "Edit sketch-driven feature"
                                                       : "Create sketch-driven feature");
    if (!committed) {
      p.fail(committed.error());
      return committed;
    }
  }
  if (p.history_command) {
    committed = staged.publish_part_history_command("Sketch-driven feature");
    if (!committed) {
      p.fail(committed.error());
      return committed;
    }
  }
  p.window.session() = std::move(staged);
  p.workflow = p.history_command = false;
  auto* repeat = !p.editing ? p.actions[p.ui->mode->currentIndex()] : nullptr;
  cancel();
  if (repeat)
    p.window.remember_repeatable_command(*repeat);
  return committed;
}
void ShellSketchDrivenTools::cancel() {
  auto& p = *impl_;
  if (!p.active)
    return;
  if (p.source_edit)
    p.window.abort_feature_source_sketch_view();
  p.active = false;
  p.source_edit.reset();
  p.layer.cancel();
  if (p.window.viewport().mouseGrabber() == &p.window.viewport())
    p.window.viewport().release_interaction_mouse();
  if (p.window.session().task().command_id() == task)
    (void)p.window.session().task().cancel();
  if (p.workflow)
    p.window.session().cancel_part_workflow();
  if (p.history_command)
    p.window.session().cancel_part_history_command();
  p.workflow = p.history_command = false;
  p.window.viewport().clear_feature_preview();
  p.window.viewport().clear_preview_selection_targets();
  (void)p.layer.set_handles({});
  p.overlay->hide();
  p.dock->hide();
  p.window.viewport().set_selection_filter_mask(p.old_mask);
  p.window.viewport().set_selection_semantic_scope(p.old_scope);
  p.window.refresh_feature_selection_scene();
  if (p.old_selection)
    (void)p.window.viewport().select_semantic(p.old_selection->semantic_id);
  else
    p.window.viewport().clear_selection();
  p.window.refresh();
}
Result<std::size_t> ShellSketchDrivenTools::edit_source_sketch() {
  auto& p = *impl_;
  if (!p.current() || p.repair || !p.layer.active_handle_id().empty())
    return Result<std::size_t>::failure(
        error("Finish the current grip; repair the feature before entering its source Sketch."));
  const SketchId sketch(p.ui->sketch->currentData().toString().toStdString());
  if (!p.window.session().part_document()->find_sketch(sketch))
    return Result<std::size_t>::failure(error("Source Sketch is unavailable."));
  (void)p.window.session().task().cancel();
  p.source_edit = sketch;
  p.valid = false;
  p.ui->apply->setEnabled(false);
  p.ui->scroll->setEnabled(false);
  p.ui->breadcrumb->show();
  p.window.viewport().clear_feature_preview();
  p.window.viewport().clear_preview_selection_targets();
  (void)p.layer.set_handles({});
  p.overlay->hide();
  if (!p.window.enter_feature_source_sketch(sketch)) {
    p.source_edit.reset();
    p.ui->scroll->setEnabled(true);
    p.ui->breadcrumb->hide();
    (void)p.window.session().task().begin(task);
    (void)p.window.session().task().begin_parameter_editing();
    return Result<std::size_t>::failure(error("The source Sketch cannot be entered."));
  }
  return Result<std::size_t>::success(1);
}
bool ShellSketchDrivenTools::editing_source_sketch() const noexcept {
  return impl_->source_edit.has_value();
}
void ShellSketchDrivenTools::resume_source_sketch(SketchId sketch) {
  auto& p = *impl_;
  if (!p.source_edit || *p.source_edit != sketch)
    return;
  p.source_edit.reset();
  p.revision = p.window.session().model_revision();
  p.loading = true;
  p.reload_sources(false);
  p.loading = false;
  p.ui->scroll->setEnabled(true);
  p.ui->breadcrumb->hide();
  (void)p.window.session().task().begin(task);
  (void)p.window.session().task().begin_parameter_editing();
  p.edited();
  (void)preview();
}
} // namespace blcad::gui
