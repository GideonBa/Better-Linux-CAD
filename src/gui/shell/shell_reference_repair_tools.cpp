#include "blcad/gui/shell/shell_reference_repair_tools.hpp"

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>
#include <type_traits>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "reference.repair";
Error error(std::string message) {
  return Error::validation("gui.shell.reference_repair", std::move(message));
}
std::string source_id(const SemanticFaceReference& reference) {
  return reference.source_feature().value() + ".face." + std::string(to_string(reference.face()));
}
template <typename T> std::string source_id(const T& reference) {
  return reference.node_id();
}
QString role_label(std::string_view role) {
  return QString::fromUtf8(role.data(), static_cast<int>(role.size())).replace('_', ' ');
}
QString text(const std::string& value) {
  return QString::fromStdString(value);
}
} // namespace

struct ShellReferenceRepairTools::Impl {
  enum class Kind { Plane, Point, Line };
  struct Reference {
    Kind kind;
    std::string owner;
    std::string entity;
    std::string source;
    QString label;
    QString diagnostic;
  };
  struct Choice {
    QString label;
    std::string identity;
    GuiPartMutation mutate;
  };
  struct Pending {
    Reference reference;
    Choice choice;
  };
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox* references{};
  QComboBox* replacements{};
  QLabel* diagnostic{};
  QLabel* summary{};
  QPushButton* stage_button{};
  QPushButton* remove_button{};
  QPushButton* clear_button{};
  QListWidget* pending_list{};
  QPushButton* preview_button{};
  QPushButton* apply_button{};
  std::vector<Reference> entries;
  std::vector<Choice> choices;
  std::vector<Pending> pending;
  std::optional<PartDocument> prepared;
  std::optional<geometry::ShapeCache> selection_cache;
  std::size_t revision{};
  bool active{};
  bool previewed{};

  explicit Impl(ShellWindow& owner) : window(owner) {}

  bool current() const {
    const auto& session = window.session();
    return active && session.part_document() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.model_revision() == revision;
  }
  const PartDocument& selection_part() const {
    return prepared ? *prepared : *window.session().part_document();
  }
  Result<std::size_t> prepare_selection_cache() {
    if (const auto* cache = window.session().part_shape_cache()) {
      selection_cache = *cache;
      return Result<std::size_t>::success(1);
    }
    auto cache = geometry::ShapeCache::create(ShapeCacheId("reference.repair.sources"));
    if (!cache)
      return Result<std::size_t>::failure(cache.error());
    // These successful producer results are only provisional repair input. The
    // session never receives this partial cache or labels it fresh Geometry.
    const auto& part = selection_part();
    geometry::GeometryRecomputeExecutor executor;
    for (const auto& feature : part.features())
      if (feature.type() == FeatureType::AdditiveExtrude) {
        auto producer_cache = cache.value();
        if (executor.execute_additive_extrude(part, feature.id(), producer_cache))
          cache.value() = std::move(producer_cache);
      }
    selection_cache = std::move(cache.value());
    return Result<std::size_t>::success(selection_cache->feature_shape_count());
  }
  static bool same_reference(const Reference& a, const Reference& b) {
    return a.kind == b.kind && a.owner == b.owner && a.entity == b.entity;
  }
  Result<PartDocument> candidate(const std::vector<Pending>& changes) const {
    auto part = *window.session().part_document();
    for (const auto& change : changes) {
      auto result = change.choice.mutate(part);
      if (!result)
        return Result<PartDocument>::failure(result.error());
    }
    return Result<PartDocument>::success(std::move(part));
  }
  void update_buttons() {
    const bool valid = current();
    const bool selected = valid && replacements->currentIndex() >= 0;
    stage_button->setEnabled(selected);
    preview_button->setEnabled(valid && (selected || !pending.empty()));
    remove_button->setEnabled(valid && pending_list->currentRow() >= 0);
    clear_button->setEnabled(valid && !pending.empty());
  }
  void show_pending() {
    pending_list->clear();
    for (const auto& item : pending) {
      pending_list->addItem(item.reference.label + QStringLiteral(" → ") + item.choice.label);
      pending_list->item(pending_list->count() - 1)->setToolTip(text(item.choice.identity));
    }
    if (!pending.empty())
      pending_list->setCurrentRow(pending_list->count() - 1);
    update_buttons();
  }
  void invalidate() {
    previewed = false;
    apply_button->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
    summary->setText(
        pending.empty()
            ? QStringLiteral("Select a replacement and check the preview.")
            : QStringLiteral("%1 change(s) staged. Check the entire repair with Preview.")
                  .arg(pending.size()));
  }
  void inspect() {
    entries.clear();
    const auto& part = *window.session().part_document();
    const auto& cache = *selection_cache;
    const auto status = [](const auto& result) {
      return result ? QStringLiteral("Resolved")
                    : QStringLiteral("Missing: ") + text(result.error().message());
    };
    for (const auto& plane : part.derived_workplanes())
      entries.push_back({Kind::Plane,
                         plane.id().value(),
                         {},
                         source_id(plane.face_reference()),
                         text(plane.name()) + QStringLiteral(" / Face"),
                         status(geometry::WorkplaneResolver{}.resolve(part, plane.id()))});
    for (const auto& sketch : part.sketches()) {
      for (const auto& point : sketch.projected_points())
        entries.push_back(
            {Kind::Point, sketch.id().value(), point.id().value(), point.referenced_node_id(),
             text(sketch.name()) + " / " + text(point.id().value()),
             status(
                 geometry::SketchReferenceProjector{}.resolve_point(part, sketch, point, cache))});
      for (const auto& line : sketch.projected_lines())
        entries.push_back(
            {Kind::Line, sketch.id().value(), line.id().value(), line.referenced_node_id(),
             text(sketch.name()) + " / " + text(line.id().value()),
             status(geometry::SketchReferenceProjector{}.resolve_line(part, sketch, line, cache))});
    }
    QSignalBlocker blocker(references);
    references->clear();
    for (const auto& entry : entries) {
      references->addItem(entry.label + " — " + entry.diagnostic,
                          text(entry.owner + "/" + entry.entity));
      references->setItemData(references->count() - 1, text(entry.source), Qt::ToolTipRole);
    }
  }
  template <typename T>
  void add_projection(const Reference& entry, T replacement, QString label, std::string identity) {
    if (identity == entry.source)
      return;
    const auto& part = selection_part();
    const auto* sketch = part.find_sketch(SketchId(entry.owner));
    const auto& cache = *selection_cache;
    bool valid;
    if constexpr (std::is_same_v<T, ProjectedSketchPoint>)
      valid = bool(
          geometry::SketchReferenceProjector{}.resolve_point(part, *sketch, replacement, cache));
    else
      valid = bool(
          geometry::SketchReferenceProjector{}.resolve_line(part, *sketch, replacement, cache));
    if (!valid)
      return;
    choices.push_back({std::move(label), std::move(identity),
                       [id = sketch->id(), replacement](PartDocument& candidate) {
                         auto* original = candidate.find_sketch(id);
                         if (!original)
                           return Result<std::size_t>::failure(error("Sketch missing"));
                         auto sketch_copy = *original;
                         auto changed = sketch_copy.replace_reference(replacement);
                         if (!changed)
                           return changed;
                         return candidate.update_sketch(std::move(sketch_copy));
                       }});
  }
  void select_reference() {
    invalidate();
    choices.clear();
    QSignalBlocker blocker(replacements);
    replacements->clear();
    if (!current() || references->currentIndex() < 0) {
      diagnostic->setText(QStringLiteral("No supported references available."));
      update_buttons();
      return;
    }
    auto cached = prepare_selection_cache();
    if (!cached) {
      summary->setText(text(cached.error().message()));
      update_buttons();
      return;
    }
    const auto& entry = entries.at(references->currentIndex());
    QString detail = entry.diagnostic + QStringLiteral("\nSource: ") + text(entry.source);
    for (const auto& item : pending)
      if (same_reference(item.reference, entry))
        detail += QStringLiteral("\nStaged: ") + item.choice.label;
    diagnostic->setText(detail);
    const auto& part = selection_part();
    if (entry.kind == Kind::Point)
      for (const auto& point : part.construction_points()) {
        auto replacement = ProjectedSketchPoint::create_from_construction_point(
            SketchEntityId(entry.entity), point.id());
        if (replacement)
          add_projection(entry, replacement.value(),
                         text(point.name() + " / " + point.id().value()), point.id().value());
      }
    if (entry.kind == Kind::Line)
      for (const auto& line : part.construction_lines()) {
        auto replacement = ProjectedSketchLine::create_from_construction_line(
            SketchEntityId(entry.entity), line.id());
        if (replacement)
          add_projection(entry, replacement.value(), text(line.name() + " / " + line.id().value()),
                         line.id().value());
      }
    auto sources =
        geometry::ViewportSceneBuilder{}.build_construction_sources(part, *selection_cache);
    if (sources)
      for (const auto& source : sources.value())
        std::visit(
            [&](const auto& target) {
              using T = std::decay_t<decltype(target)>;
              const auto* producer = part.find_feature(target.source_feature());
              QString label = text(producer ? producer->name() : target.source_feature().value());
              if constexpr (std::is_same_v<T, SemanticFaceReference>) {
                label += QStringLiteral(" / Face ") + role_label(to_string(target.face()));
                if (entry.kind != Kind::Plane || source_id(target) == entry.source)
                  return;
                const auto* plane = part.find_derived_workplane(DatumPlaneId(entry.owner));
                auto replacement =
                    DerivedWorkplane::create_on_feature_face(plane->id(), plane->name(), target);
                if (replacement)
                  choices.push_back({label, source_id(target),
                                     [replacement = replacement.value()](PartDocument& candidate) {
                                       return candidate.update_derived_workplane(replacement);
                                     }});
              } else if constexpr (std::is_same_v<T, SemanticVertexReference>) {
                label += QStringLiteral(" / Vertex ") + role_label(to_string(target.vertex()));
                if (entry.kind == Kind::Point) {
                  auto replacement = ProjectedSketchPoint::create_from_semantic_vertex(
                      SketchEntityId(entry.entity), target);
                  if (replacement)
                    add_projection(entry, replacement.value(), label, source_id(target));
                }
              } else if (entry.kind == Kind::Line) {
                label += QStringLiteral(" / Edge ") + role_label(to_string(target.edge()));
                auto replacement = ProjectedSketchLine::create_from_semantic_edge(
                    SketchEntityId(entry.entity), target);
                if (replacement)
                  add_projection(entry, replacement.value(), label, source_id(target));
              }
            },
            source.reference);
    if (!sources)
      summary->setText(text(sources.error().message()));
    for (const auto& choice : choices) {
      replacements->addItem(choice.label, text(choice.identity));
      replacements->setItemData(replacements->count() - 1, text(choice.identity), Qt::ToolTipRole);
    }
    replacements->setCurrentIndex(-1);
    update_buttons();
  }
};

ShellReferenceRepairTools::ShellReferenceRepairTools(ShellWindow& window, ShellRibbon& ribbon,
                                                     int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("Repair References"), this);
  ui.action->setObjectName("blcad.action.repair_references");
  auto* group = ribbon.add_group(model_tab, QStringLiteral("References"));
  ribbon.add_small_button(group, ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Repair References"), &window);
  ui.dock->setObjectName("blcad.shell.reference_repair_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* hint = new QLabel(
      QStringLiteral("Repair points, lines, and face workplanes together. "
                     "Stage changes and check the entire list. Apply commits the repair."),
      panel);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto* form = new QFormLayout;
  const auto combo = [&](const char* id, const QString& label) {
    auto* result = new QComboBox(panel);
    result->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    result->setMinimumContentsLength(18);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    form->addRow(label, result);
    return result;
  };
  ui.references = combo("reference", QStringLiteral("Reference"));
  ui.replacements = combo("replacement", QStringLiteral("Replacement"));
  layout->addLayout(form);
  for (auto [target, id] : {std::pair{&ui.diagnostic, "diagnostic"}, {&ui.summary, "summary"}}) {
    *target = new QLabel(panel);
    (*target)->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    (*target)->setWordWrap(true);
    (*target)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*target);
  }
  auto* queue_label = new QLabel(QStringLiteral("Staged Changes"), panel);
  layout->addWidget(queue_label);
  ui.pending_list = new QListWidget(panel);
  ui.pending_list->setObjectName("blcad.reference_repair.pending");
  ui.pending_list->setMinimumHeight(72);
  ui.pending_list->setMaximumHeight(150);
  layout->addWidget(ui.pending_list);
  auto* queue_buttons = new QHBoxLayout;
  const auto queue_button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    queue_buttons->addWidget(result);
    return result;
  };
  ui.stage_button = queue_button("stage", QStringLiteral("Add"));
  ui.remove_button = queue_button("remove", QStringLiteral("Remove"));
  ui.clear_button = queue_button("clear", QStringLiteral("Clear List"));
  layout->addLayout(queue_buttons);
  layout->addStretch();
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    if (QString::fromLatin1(id) == QStringLiteral("apply"))
      result->setProperty("guiRole", "primary");
    buttons->addWidget(result);
    return result;
  };
  ui.preview_button = button("preview", QStringLiteral("Full Preview"));
  ui.apply_button = button("apply", QStringLiteral("Apply"));
  auto* close = button("cancel", QStringLiteral("Cancel"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  ui.dock->installEventFilter(this);
  window.installEventFilter(this);
  window.viewport().installEventFilter(this);
  connect(close, &QPushButton::clicked, this, [this] { cancel(); });
  connect(ui.references, &QComboBox::currentIndexChanged, this,
          [this] { impl_->select_reference(); });
  connect(ui.replacements, &QComboBox::currentIndexChanged, this, [this] {
    impl_->invalidate();
    impl_->update_buttons();
  });
  connect(ui.pending_list, &QListWidget::currentRowChanged, this,
          [this] { impl_->update_buttons(); });
  connect(ui.clear_button, &QPushButton::clicked, this, [this] { clear_staged(); });
  const auto report_result = [this](const Result<std::size_t>& result) {
    if (!result) {
      impl_->summary->setText(text(result.error().message()));
      // Candidate errors belong to the editable task. Recording a session error
      // advances its presentation revision and would expire this same candidate.
      if (!impl_->active)
        impl_->window.report_error(result.error());
    }
  };
  connect(ui.preview_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(preview()); });
  connect(ui.apply_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(apply()); });
  connect(ui.stage_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(stage()); });
  connect(ui.remove_button, &QPushButton::clicked, this,
          [this, report_result] { report_result(remove_staged()); });
}
ShellReferenceRepairTools::~ShellReferenceRepairTools() = default;
bool ShellReferenceRepairTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellReferenceRepairTools::begin() {
  auto& ui = *impl_;
  auto& session = ui.window.session();
  if (!session.part_document() ||
      (!session.part_shape_cache() && !session.reference_repair_mode()) ||
      session.task().active() || ui.window.active_sketch())
    return Result<std::size_t>::failure(
        error("Reference repair requires a part without an active command"));
  if (!session.task().begin(std::string(task_id)) || !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Could not start reference repair"));
  ui.active = true;
  ui.revision = session.model_revision();
  ui.pending.clear();
  ui.prepared.reset();
  ui.show_pending();
  auto cached = ui.prepare_selection_cache();
  if (!cached) {
    cancel();
    return cached;
  }
  ui.inspect();
  ui.select_reference();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.entries.size());
}
Result<std::size_t> ShellReferenceRepairTools::stage() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen repair."));
  }
  ui.invalidate();
  const auto index = ui.replacements->currentIndex();
  if (index < 0 || index >= static_cast<int>(ui.choices.size()))
    return Result<std::size_t>::failure(error("Select a valid replacement"));
  const auto& reference = ui.entries.at(ui.references->currentIndex());
  auto proposed = ui.pending;
  const auto found = std::find_if(proposed.begin(), proposed.end(), [&](const auto& item) {
    return Impl::same_reference(item.reference, reference);
  });
  Impl::Pending replacement{reference, ui.choices[index]};
  if (found == proposed.end())
    proposed.push_back(std::move(replacement));
  else
    *found = std::move(replacement);
  auto candidate = ui.candidate(proposed);
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  ui.pending = std::move(proposed);
  ui.prepared = std::move(candidate.value());
  ui.show_pending();
  ui.select_reference();
  return Result<std::size_t>::success(ui.pending.size());
}
Result<std::size_t> ShellReferenceRepairTools::remove_staged() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen repair."));
  }
  ui.invalidate();
  const auto row = ui.pending_list->currentRow();
  if (row < 0 || row >= static_cast<int>(ui.pending.size()))
    return Result<std::size_t>::failure(error("Select a staged change"));
  auto proposed = ui.pending;
  proposed.erase(proposed.begin() + row);
  auto candidate = ui.candidate(proposed);
  if (!candidate)
    return Result<std::size_t>::failure(candidate.error());
  ui.pending = std::move(proposed);
  ui.prepared = std::move(candidate.value());
  ui.show_pending();
  ui.select_reference();
  return Result<std::size_t>::success(ui.pending.size());
}
void ShellReferenceRepairTools::clear_staged() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return;
  }
  ui.pending.clear();
  ui.prepared.reset();
  ui.show_pending();
  ui.select_reference();
}
Result<std::size_t> ShellReferenceRepairTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen repair."));
  }
  ui.invalidate();
  if (ui.replacements->currentIndex() >= 0) {
    auto staged = stage();
    if (!staged)
      return staged;
  }
  if (ui.pending.empty())
    return Result<std::size_t>::failure(error("Stage at least one reference change"));
  auto prepared = ui.candidate(ui.pending);
  if (!prepared)
    return Result<std::size_t>::failure(prepared.error());
  auto candidate = std::move(prepared.value());
  auto cache = geometry::ShapeCache::create(ShapeCacheId("reference.repair.preview"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto computed = geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
  if (!computed)
    return Result<std::size_t>::failure(computed.error());
  auto scene = geometry::ViewportSceneBuilder{}.build_part(candidate, cache.value());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  std::vector<std::string> affected;
  for (const auto& item : ui.pending) {
    auto downstream = candidate.dependency_graph().transitive_dependents(item.reference.owner);
    affected.insert(affected.end(), downstream.begin(), downstream.end());
    affected.push_back(item.reference.owner);
  }
  const auto included = [&](const auto& item) {
    if (item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody)
      return true;
    return std::any_of(affected.begin(), affected.end(), [&](const auto& id) {
      return item.semantic_id == "derived-workplane/" + id ||
             item.semantic_id.starts_with("sketch/" + id + "/");
    });
  };
  for (auto& item : scene.value())
    if (included(item)) {
      replaced.push_back(item.semantic_id);
      shapes.push_back(std::move(item.shape));
    }
  if (shapes.empty())
    return Result<std::size_t>::failure(error("Repair has no displayable geometry"));
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  (void)ui.window.session().task().show_preview();
  ui.previewed = true;
  ui.apply_button->setEnabled(true);
  ui.summary->setText(QStringLiteral("Full preview valid. Apply commits %1 change(s) together.")
                          .arg(ui.pending.size()));
  return Result<std::size_t>::success(ui.pending.size());
}
Result<std::size_t> ShellReferenceRepairTools::apply() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen repair."));
  }
  if (!ui.previewed)
    return Result<std::size_t>::failure(error("Check a current repair preview first"));
  auto valid = preview();
  if (!valid)
    return valid;
  const auto changes = ui.pending;
  auto result = ui.window.session().commit_part_repair_transaction(
      changes.size() == 1 ? "Repair reference" : "Repair references",
      [changes](PartDocument& part) {
        for (const auto& change : changes) {
          auto changed = change.choice.mutate(part);
          if (!changed)
            return changed;
        }
        return Result<std::size_t>::success(changes.size());
      });
  if (!result) {
    cancel();
    return result;
  }
  (void)ui.window.session().task().apply();
  cancel();
  return Result<std::size_t>::success(changes.size());
}
void ShellReferenceRepairTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.dock->hide();
  ui.entries.clear();
  ui.choices.clear();
  ui.pending.clear();
  ui.prepared.reset();
  ui.selection_cache.reset();
  ui.show_pending();
  ui.window.refresh();
}
void ShellReferenceRepairTools::refresh() {
  auto& ui = *impl_;
  if (ui.active && !ui.current()) {
    cancel();
    return;
  }
  const auto& session = ui.window.session();
  ui.action->setEnabled(session.part_document() &&
                        (session.part_shape_cache() || session.reference_repair_mode()) &&
                        !session.task().active() && !ui.window.active_sketch());
}
bool ShellReferenceRepairTools::eventFilter(QObject* watched, QEvent* event) {
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
