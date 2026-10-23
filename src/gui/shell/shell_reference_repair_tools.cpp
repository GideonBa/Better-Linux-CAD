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
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
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
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox* references{};
  QComboBox* replacements{};
  QLabel* diagnostic{};
  QLabel* summary{};
  QPushButton* preview_button{};
  QPushButton* apply_button{};
  std::vector<Reference> entries;
  std::vector<Choice> choices;
  std::size_t revision{};
  bool active{};
  bool previewed{};

  explicit Impl(ShellWindow& owner) : window(owner) {}

  bool current() const {
    const auto& session = window.session();
    return active && session.part_document() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.presentation_revision() == revision;
  }
  void invalidate() {
    previewed = false;
    apply_button->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
    summary->setText(QStringLiteral("Ersatz auswählen und Vorschau prüfen."));
  }
  void inspect() {
    entries.clear();
    const auto& part = *window.session().part_document();
    const auto& cache = *window.session().part_shape_cache();
    const auto status = [](const auto& result) {
      return result ? QStringLiteral("Aufgelöst")
                    : QStringLiteral("Verloren: ") + text(result.error().message());
    };
    for (const auto& plane : part.derived_workplanes())
      entries.push_back({Kind::Plane,
                         plane.id().value(),
                         {},
                         source_id(plane.face_reference()),
                         text(plane.name()) + QStringLiteral(" / Fläche"),
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
    const auto& part = *window.session().part_document();
    const auto* sketch = part.find_sketch(SketchId(entry.owner));
    const auto& cache = *window.session().part_shape_cache();
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
                           return Result<std::size_t>::failure(error("Skizze fehlt"));
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
      diagnostic->setText(QStringLiteral("Keine unterstützten Referenzen vorhanden."));
      preview_button->setEnabled(false);
      return;
    }
    const auto& entry = entries.at(references->currentIndex());
    diagnostic->setText(entry.diagnostic + QStringLiteral("\nQuelle: ") + text(entry.source));
    const auto& part = *window.session().part_document();
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
    auto sources = geometry::ViewportSceneBuilder{}.build_construction_sources(
        part, *window.session().part_shape_cache());
    if (sources)
      for (const auto& source : sources.value())
        std::visit(
            [&](const auto& target) {
              using T = std::decay_t<decltype(target)>;
              const auto label = text(source_id(target));
              if constexpr (std::is_same_v<T, SemanticFaceReference>) {
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
                if (entry.kind == Kind::Point) {
                  auto replacement = ProjectedSketchPoint::create_from_semantic_vertex(
                      SketchEntityId(entry.entity), target);
                  if (replacement)
                    add_projection(entry, replacement.value(), label, source_id(target));
                }
              } else if (entry.kind == Kind::Line) {
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
    preview_button->setEnabled(false);
  }
};

ShellReferenceRepairTools::ShellReferenceRepairTools(ShellWindow& window, ShellRibbon& ribbon,
                                                     int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("Referenzen reparieren"), this);
  ui.action->setObjectName("blcad.action.repair_references");
  auto* group = ribbon.add_group(model_tab, QStringLiteral("Referenzen"));
  ribbon.add_small_button(group, ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Referenzen reparieren"), &window);
  ui.dock->setObjectName("blcad.shell.reference_repair_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* hint =
      new QLabel(QStringLiteral("Projizierte Punkte/Linien und Flächenarbeitsebenen prüfen. "
                                "Ersatz wird erst nach Vorschau und Anwenden übernommen."),
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
  ui.references = combo("reference", QStringLiteral("Referenz"));
  ui.replacements = combo("replacement", QStringLiteral("Ersatz"));
  layout->addLayout(form);
  for (auto [target, id] : {std::pair{&ui.diagnostic, "diagnostic"}, {&ui.summary, "summary"}}) {
    *target = new QLabel(panel);
    (*target)->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    (*target)->setWordWrap(true);
    (*target)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*target);
  }
  layout->addStretch();
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.reference_repair.") + id);
    buttons->addWidget(result);
    return result;
  };
  ui.preview_button = button("preview", QStringLiteral("Vorschau"));
  ui.apply_button = button("apply", QStringLiteral("Anwenden"));
  auto* close = button("cancel", QStringLiteral("Abbrechen"));
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
    impl_->preview_button->setEnabled(impl_->current() && impl_->replacements->currentIndex() >= 0);
  });
  const auto run = [this](bool applying) {
    auto result = applying ? apply() : preview();
    if (!result) {
      impl_->summary->setText(text(result.error().message()));
      // Candidate errors belong to the editable task. Recording a session error
      // advances its presentation revision and would expire this same candidate.
      if (!impl_->active)
        impl_->window.report_error(result.error());
    }
  };
  connect(ui.preview_button, &QPushButton::clicked, this, [run] { run(false); });
  connect(ui.apply_button, &QPushButton::clicked, this, [run] { run(true); });
}
ShellReferenceRepairTools::~ShellReferenceRepairTools() = default;
bool ShellReferenceRepairTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellReferenceRepairTools::begin() {
  auto& ui = *impl_;
  auto& session = ui.window.session();
  if (!session.part_document() || !session.part_shape_cache() || session.task().active() ||
      ui.window.active_sketch())
    return Result<std::size_t>::failure(
        error("Referenzreparatur benötigt ein Bauteil ohne aktiven Befehl"));
  if (!session.task().begin(std::string(task_id)) || !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Referenzreparatur konnte nicht gestartet werden"));
  ui.active = true;
  ui.revision = session.presentation_revision();
  ui.inspect();
  ui.select_reference();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.entries.size());
}
Result<std::size_t> ShellReferenceRepairTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(
        error("Modell oder Befehl geändert. Reparatur neu öffnen."));
  }
  ui.invalidate();
  const auto index = ui.replacements->currentIndex();
  if (index < 0 || index >= static_cast<int>(ui.choices.size()))
    return Result<std::size_t>::failure(error("Einen gültigen Ersatz auswählen"));
  auto candidate = *ui.window.session().part_document();
  auto changed = ui.choices[index].mutate(candidate);
  if (!changed)
    return changed;
  auto cache = geometry::ShapeCache::create(ShapeCacheId("reference.repair.preview"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto computed = geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
  if (!computed)
    return Result<std::size_t>::failure(computed.error());
  auto scene = geometry::ViewportSceneBuilder{}.build_part(candidate, cache.value());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  const auto& entry = ui.entries.at(ui.references->currentIndex());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  auto affected = candidate.dependency_graph().transitive_dependents(entry.owner);
  affected.push_back(entry.owner);
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
    return Result<std::size_t>::failure(error("Reparatur hat keine darstellbare Geometrie"));
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  (void)ui.window.session().task().show_preview();
  ui.previewed = true;
  ui.apply_button->setEnabled(true);
  ui.summary->setText(QStringLiteral("Vorschau gültig, nachgelagerte Geometrie neu berechnet. "
                                     "Anwenden übernimmt genau diese Referenz."));
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellReferenceRepairTools::apply() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(
        error("Modell oder Befehl geändert. Reparatur neu öffnen."));
  }
  if (!ui.previewed)
    return Result<std::size_t>::failure(error("Zuerst eine aktuelle Reparaturvorschau prüfen"));
  auto valid = preview();
  if (!valid)
    return valid;
  auto result = ui.window.session().commit_part_transaction(
      "Repair reference", ui.choices.at(ui.replacements->currentIndex()).mutate);
  if (!result) {
    cancel();
    return result;
  }
  (void)ui.window.session().task().apply();
  cancel();
  return Result<std::size_t>::success(1);
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
  ui.window.refresh();
}
void ShellReferenceRepairTools::refresh() {
  auto& ui = *impl_;
  if (ui.active && !ui.current()) {
    cancel();
    return;
  }
  const auto& session = ui.window.session();
  ui.action->setEnabled(session.part_document() && session.part_shape_cache() &&
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
