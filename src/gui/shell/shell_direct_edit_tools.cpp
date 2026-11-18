#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "direct_edit_tools_impl.hpp"
#include <QApplication>
#include <algorithm>
#include <utility>
namespace blcad::gui {
namespace {
constexpr auto task = "part.direct_edit";
Error error(const std::string& text) {
  return Error::validation(task, text);
}
} // namespace
ShellDirectEditTools::ShellDirectEditTools(ShellWindow& w, ShellRibbon& ribbon, int tab)
    : QObject(&w), impl_(std::make_unique<Impl>(w, *this)) {
  auto& p = *impl_;
  auto* group = ribbon.add_group(tab, "Direct Editing");
  p.action = new QAction("Direct Edit", this);
  p.action->setObjectName("blcad.action.direct_edit");
  ribbon.add_small_button(group, p.action);
  connect(p.action, &QAction::triggered, this, [this] {
    auto r = begin();
    if (!r)
      impl_->window.report_error(r.error());
  });
  p.dock = new QDockWidget("Direct Edit", &w);
  p.dock->setObjectName("blcad.shell.direct_edit_task");
  p.controls();
  w.addDockWidget(Qt::RightDockWidgetArea, p.dock);
  p.dock->hide();
  p.overlay = make_shell_manipulator_overlay(&w.viewport(), p.layer);
  p.overlay->setObjectName("blcad.direct_edit.manipulator_overlay");
  p.overlay->hide();
  for (const auto& [key, widget] : p.fields) {
    if (auto* line = qobject_cast<QLineEdit*>(widget); line && widget != p.numeric)
      connect(line, &QLineEdit::textChanged, this, [this] { impl_->changed(); });
    if (auto* box = qobject_cast<QComboBox*>(widget))
      connect(box, &QComboBox::currentIndexChanged, this, [this] { impl_->changed(); });
  }
  for (auto* b : {&p.move[0], &p.move[1], &p.move[2], &p.angle, &p.size})
    connect(b->parameter, &QComboBox::currentIndexChanged, this, [this, b] {
      if (impl_->loading)
        return;
      impl_->loading = true;
      impl_->select_binding(*b);
      impl_->loading = false;
      impl_->changed();
    });
  connect(p.body, &QComboBox::currentIndexChanged, this, [this] {
    if (impl_->loading)
      return;
    impl_->selected->clear();
    impl_->reload_faces();
    impl_->changed();
  });
  connect(p.add, &QPushButton::clicked, this, [this] { impl_->add_face(); });
  connect(p.remove, &QPushButton::clicked, this, [this] {
    delete impl_->selected->takeItem(impl_->selected->currentRow());
    impl_->changed();
  });
  connect(p.align, &QPushButton::clicked, this, [this] { impl_->frame_from_face(false); });
  connect(p.world, &QPushButton::clicked, this, [this] { impl_->frame_from_face(true); });
  connect(p.preview, &QPushButton::clicked, this, [this] { (void)preview(); });
  connect(p.apply, &QPushButton::clicked, this, [this] { (void)apply(); });
  connect(p.cancel, &QPushButton::clicked, this, [this] { cancel(); });
  connect(p.numeric, &QLineEdit::textEdited, this,
          [this](const QString& text) { (void)set_drag_text(text); });
  connect(p.dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    if (!visible && impl_->active)
      cancel();
  });
  qApp->installEventFilter(this);
  refresh();
}
ShellDirectEditTools::~ShellDirectEditTools() {
  cancel();
  delete impl_->overlay;
}
bool ShellDirectEditTools::Impl::current() const {
  return active && window.session().part_document() &&
         window.session().model_revision() == revision &&
         window.session().task().command_id() == task;
}
void ShellDirectEditTools::Impl::changed() {
  if (loading || !active)
    return;
  valid = false;
  apply->setEnabled(false);
  render();
  if (layer.active_handle_id().empty())
    (void)layer.set_handles({});
  (void)show_inputs();
  overlay_refresh();
  status->setText("Select faces, set the operation and preview the complete history.");
}
Result<std::size_t> ShellDirectEditTools::Impl::start(std::optional<FeatureId> id, Mode initial) {
  auto& session = window.session();
  const auto* p = session.part_document();
  if (p && p->is_reference_part())
    return Result<std::size_t>::failure(
        error("Reference Part is immutable. Open STEP Sources and choose "
              "Convert to New Editable Part before Direct Edit."));
  if (active || window.active_sketch() || session.task().active() || !p ||
      !session.part_modeling_ready())
    return Result<std::size_t>::failure(
        error("Choose a current editable Part with no active command."));
  auto history = p->executable_history();
  if (!history)
    return Result<std::size_t>::failure(history.error());
  prefix = session.history_prefix_count().value_or(history.value().size());
  const DirectEditFeature* record = nullptr;
  editing = id.has_value();
  if (id) {
    record = p->find_direct_edit_feature(*id);
    auto at = std::find_if(history.value().begin(), history.value().end(),
                           [&](const auto& row) { return row.node_id == id->value(); });
    if (!record || at == history.value().end() ||
        std::size_t(at - history.value().begin()) >= prefix)
      return Result<std::size_t>::failure(
          error("Move End of Part after the Direct Edit before editing it."));
    prefix = std::size_t(at - history.value().begin());
    feature = *id;
  } else {
    if (prefix != history.value().size())
      return Result<std::size_t>::failure(
          error("Move End of Part to the end before creating Direct Edit."));
    for (std::size_t index = 1;; ++index) {
      feature = FeatureId("direct_edit." + std::to_string(index));
      bool used = p->dependency_graph().has_node(feature.value());
      for (const auto* suffix : {"x", "y", "z", "angle", "size", "offset"})
        used |= p->find_parameter(ParameterId(feature.value() + "." + suffix)) != nullptr;
      if (!used)
        break;
    }
  }
  auto prepared = geometry::ShapeCache::create(ShapeCacheId("direct_edit.inputs"));
  if (!prepared)
    return Result<std::size_t>::failure(prepared.error());
  auto built = query_with_step_import_context(session, [&](auto* context) {
    return geometry::GeometryRecomputeExecutor(context).execute_history_prefix(*p, prefix,
                                                                               prepared.value());
  });
  if (!built)
    return Result<std::size_t>::failure(built.error());
  inputs = *p;
  cache = std::move(prepared.value());
  if (cache->body_shapes().empty())
    return Result<std::size_t>::failure(error("Create an editable solid Body first."));
  old_model_selection = session.selection();
  old_selection = window.viewport().selected_semantic();
  old_mask = window.viewport().selection_filter_mask();
  old_scope = window.viewport().selection_semantic_scope();
  auto begun = session.history_prefix_count() ? session.begin_part_history_command(task)
                                              : session.begin_part_workflow();
  if (!begun)
    return begun;
  history_command = session.history_prefix_count().has_value();
  workflow = !history_command;
  active = true;
  revision = session.model_revision();
  loading = true;
  mode->setCurrentIndex(int(initial));
  load(record);
  loading = false;
  render();
  (void)session.task().begin(task);
  (void)session.task().begin_parameter_editing();
  valid = false;
  apply->setEnabled(false);
  scroll->setEnabled(true);
  numeric->setEnabled(false);
  window.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Face));
  window.viewport().set_selection_semantic_scope(std::nullopt);
  std::vector<geometry::ViewportSceneItem> picks;
  for (const auto& target : targets)
    picks.push_back(
        {geometry::ViewportSceneKind::Face, target.selection.semantic_id, target.selection.shape});
  (void)window.viewport().set_preview_selection_targets(std::move(picks));
  const auto previous = session.selection().entries();
  for (const auto& selection : previous)
    (void)owner.accept_selection(selection);
  (void)show_inputs();
  dock->show();
  dock->raise();
  status->setText("Select semantic faces. Preview checks the complete downstream history.");
  window.refresh();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellDirectEditTools::begin(Mode mode) {
  return impl_->start(std::nullopt, mode);
}
Result<std::size_t> ShellDirectEditTools::begin_edit(FeatureId id) {
  return impl_->start(id, Mode::Move);
}
bool ShellDirectEditTools::active() const noexcept {
  return impl_->active;
}
QWidget* ShellDirectEditTools::field(const QString& key) const {
  auto at = impl_->fields.find(key);
  return at == impl_->fields.end() ? nullptr : at->second;
}
GuiViewportManipulatorLayer& ShellDirectEditTools::manipulators() {
  return impl_->layer;
}
bool ShellDirectEditTools::accept_selection(const GuiSelection& selection) {
  auto& p = *impl_;
  if (!p.current() || selection.kind != GuiSelectionKind::Face)
    return false;
  for (const auto& target : p.targets)
    if (target.selection.semantic_id == selection.semantic_id) {
      const auto key =
          interactive_face_identity(std::get<FaceReference>(target.selection.reference));
      p.face->setCurrentIndex(p.face->findData(QString::fromStdString(key)));
      p.add_face();
      return true;
    }
  return false;
}
void ShellDirectEditTools::refresh() {
  auto& p = *impl_;
  if (p.active && !p.current()) {
    cancel();
    return;
  }
  p.action->setEnabled(!p.active && !p.window.active_sketch() &&
                       !p.window.session().task().active() && p.window.session().part_document());
  if (p.active)
    p.overlay_refresh();
}
Result<std::size_t> ShellDirectEditTools::apply() {
  auto& p = *impl_;
  if (!p.current() || !p.valid || !p.layer.active_handle_id().empty())
    return Result<std::size_t>::failure(
        error("Finish the active grip and check a valid current preview."));
  auto checked = preview();
  if (!checked)
    return checked;
  auto candidate = p.candidate();
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  auto staged = p.window.session();
  const auto label = p.editing ? "Edit direct faces" : "Create Direct Edit";
  auto committed = staged.commit_part_transaction(label, [&](PartDocument& part) {
    part = candidate.value();
    return Result<std::size_t>::success(1);
  });
  if (committed && p.workflow)
    committed = staged.publish_part_workflow(label);
  if (committed && p.history_command)
    committed = staged.publish_part_history_command(label);
  if (!committed) {
    p.fail(committed.error());
    return committed;
  }
  p.window.session() = std::move(staged);
  p.workflow = p.history_command = false;
  const bool repeat = !p.editing;
  cancel();
  if (repeat)
    p.window.remember_repeatable_command(*p.action);
  return committed;
}
void ShellDirectEditTools::cancel() {
  auto& p = *impl_;
  if (!p.active)
    return;
  p.active = false;
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
  p.window.session().selection() = p.old_model_selection;
  if (p.old_selection)
    (void)p.window.viewport().select_semantic(p.old_selection->semantic_id);
  else
    p.window.viewport().clear_selection();
  p.window.refresh();
}
} // namespace blcad::gui
