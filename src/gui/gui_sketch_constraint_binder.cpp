#include "blcad/gui/gui_sketch_constraint_binder.hpp"

#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

struct ConstraintActionDescriptor {
  SketchSolverConstraintKind kind;
  const char* command;
  const char* object_name;
  const char* label;
  const char* description;
};

constexpr std::array<ConstraintActionDescriptor, 13U> kConstraintActions{{
    {SketchSolverConstraintKind::Coincident, "sketch.constraint.coincident",
     "blcad.action.sketch_constraint.coincident", "Coincident",
     "Make two selected Sketch points coincident"},
    {SketchSolverConstraintKind::Fixed, "sketch.constraint.fixed",
     "blcad.action.sketch_constraint.fixed", "Fixed",
     "Fix one selected Sketch point or curve"},
    {SketchSolverConstraintKind::Horizontal, "sketch.constraint.horizontal",
     "blcad.action.sketch_constraint.horizontal", "Horizontal",
     "Constrain one selected line horizontally"},
    {SketchSolverConstraintKind::Vertical, "sketch.constraint.vertical",
     "blcad.action.sketch_constraint.vertical", "Vertical",
     "Constrain one selected line vertically"},
    {SketchSolverConstraintKind::Parallel, "sketch.constraint.parallel",
     "blcad.action.sketch_constraint.parallel", "Parallel",
     "Constrain two selected lines parallel"},
    {SketchSolverConstraintKind::Perpendicular, "sketch.constraint.perpendicular",
     "blcad.action.sketch_constraint.perpendicular", "Perpendicular",
     "Constrain two selected lines perpendicular"},
    {SketchSolverConstraintKind::Collinear, "sketch.constraint.collinear",
     "blcad.action.sketch_constraint.collinear", "Collinear",
     "Constrain two selected lines collinear"},
    {SketchSolverConstraintKind::Equal, "sketch.constraint.equal",
     "blcad.action.sketch_constraint.equal", "Equal",
     "Constrain two selected lines or arcs to equal measure"},
    {SketchSolverConstraintKind::Tangent, "sketch.constraint.tangent",
     "blcad.action.sketch_constraint.tangent", "Tangent",
     "Constrain two connected selected curves tangent"},
    {SketchSolverConstraintKind::Concentric, "sketch.constraint.concentric",
     "blcad.action.sketch_constraint.concentric", "Concentric",
     "Constrain two selected center-bearing entities concentric"},
    {SketchSolverConstraintKind::Midpoint, "sketch.constraint.midpoint",
     "blcad.action.sketch_constraint.midpoint", "Midpoint",
     "Constrain a selected point to a selected line midpoint"},
    {SketchSolverConstraintKind::Symmetric, "sketch.constraint.symmetric",
     "blcad.action.sketch_constraint.symmetric", "Symmetric",
     "Constrain two selected points symmetrically about a selected line"},
    {SketchSolverConstraintKind::PointOnObject, "sketch.constraint.point_on_object",
     "blcad.action.sketch_constraint.point_on_object", "Point on Object",
     "Constrain a selected point onto a selected line or arc"},
}};

[[nodiscard]] const ConstraintActionDescriptor*
descriptor(SketchSolverConstraintKind kind) noexcept {
  const auto found = std::find_if(kConstraintActions.begin(), kConstraintActions.end(),
                                  [kind](const auto& value) { return value.kind == kind; });
  return found == kConstraintActions.end() ? nullptr : &*found;
}

[[nodiscard]] std::optional<SketchConstraintIntentTarget>
selection_target(const SketchTopology& topology, std::string semantic_id) {
  const auto entity = [&topology](const std::string& id)
      -> std::optional<SketchConstraintIntentTarget> {
    if (topology.find_entity(id) == nullptr) return std::nullopt;
    auto target = SketchConstraintIntentTarget::entity(id);
    return target.has_error() ? std::nullopt
                              : std::optional<SketchConstraintIntentTarget>(target.value());
  };
  const auto point = [&topology](const std::string& id)
      -> std::optional<SketchConstraintIntentTarget> {
    if (topology.find_point(SketchPointId(id)) == nullptr) return std::nullopt;
    auto target = SketchConstraintIntentTarget::point(SketchPointId(id));
    return target.has_error() ? std::nullopt
                              : std::optional<SketchConstraintIntentTarget>(target.value());
  };

  if (auto target = entity(semantic_id)) return target;
  if (auto target = point(semantic_id)) return target;

  const std::string sketch_prefix = "sketch/" + topology.sketch().value() + "/";
  if (std::string_view(semantic_id).starts_with(sketch_prefix))
    semantic_id.erase(0U, sketch_prefix.size());
  if (auto target = entity(semantic_id)) return target;
  if (auto target = point(semantic_id)) return target;

  constexpr std::array<std::string_view, 3U> curve_prefixes{"line/", "arc/", "spline/"};
  for (const auto prefix : curve_prefixes)
    if (std::string_view(semantic_id).starts_with(prefix))
      if (auto target = entity("entity/" + semantic_id.substr(prefix.size()))) return target;

  if (std::string_view(semantic_id).starts_with("point/")) {
    if (auto target = point(semantic_id)) return target;
    if (auto target = point(semantic_id.substr(std::string_view("point/").size()))) return target;
  }
  return std::nullopt;
}

[[nodiscard]] Result<std::vector<SketchConstraintIntentTarget>>
selected_targets(const MainWindow& window, const SketchTopology& topology) {
  std::vector<SketchConstraintIntentTarget> targets;
  for (const auto& selection : window.session().selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity) continue;
    auto target = selection_target(topology, selection.semantic_id);
    if (!target)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "selected Sketch item is not a stable point or curve target"));
    if (std::find(targets.begin(), targets.end(), *target) == targets.end())
      targets.push_back(std::move(*target));
  }
  return Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(targets));
}

[[nodiscard]] Result<SketchTopology> active_topology(const MainWindow& window) {
  if (!window.active_sketch() || window.session().part_document() == nullptr)
    return Result<SketchTopology>::failure(
        Error::validation("sketch_constraint_gui", "no active Sketch constraint workspace"));
  const Sketch* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  if (sketch == nullptr)
    return Result<SketchTopology>::failure(Error::validation(
        window.active_sketch()->value(), "active Sketch no longer exists"));
  return SketchTopology::migrate_legacy(*sketch);
}

[[nodiscard]] SketchConstraintId next_constraint_id(
    const SketchConstraintCatalog& catalog, SketchSolverConstraintKind kind) {
  const std::string base = "constraint.gui." + std::string(to_string(kind)) + ".";
  std::size_t index = 1U;
  while (catalog.find(SketchConstraintId(base + std::to_string(index))) != nullptr) ++index;
  return SketchConstraintId(base + std::to_string(index));
}

void append_diagnostic(MainWindow& window, std::string message) {
  if (auto* diagnostics = window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics")))
    diagnostics->append(QStringLiteral("[Sketch Constraint] %1")
                            .arg(QString::fromStdString(std::move(message))));
}

// Resolves exactly one selected accepted-catalog constraint id from the
// current semantic selection ("sketch/<sketch>/constraint/<id>" glyphs).
[[nodiscard]] std::optional<SketchConstraintId>
selected_accepted_constraint(const MainWindow& window) {
  if (!window.active_sketch() || window.session().part_document() == nullptr)
    return std::nullopt;
  const std::string prefix =
      "sketch/" + window.active_sketch()->value() + "/constraint/";
  std::optional<SketchConstraintId> selected;
  for (const auto& selection : window.session().selection().entries()) {
    if (!std::string_view(selection.semantic_id).starts_with(prefix)) continue;
    if (selected.has_value()) return std::nullopt;
    selected = SketchConstraintId(selection.semantic_id.substr(prefix.size()));
  }
  if (!selected.has_value()) return std::nullopt;
  auto catalog = window.session().sketch_constraint_catalog(*window.active_sketch());
  if (catalog.has_error() || catalog.value().find(*selected) == nullptr) return std::nullopt;
  return selected;
}

Result<std::size_t> execute_constraint_removal(MainWindow& window) {
  const auto selected = selected_accepted_constraint(window);
  if (!selected)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_constraint_gui",
        "removal requires exactly one selected accepted constraint glyph"));
  const SketchId sketch = *window.active_sketch();
  auto removed = GuiSketchConstraintController::remove_accepted(window.session(), sketch, *selected);
  if (removed.has_error()) return removed;
  window.session().selection().clear();
  window.render_sketch(sketch);
  return removed;
}

Result<std::size_t> execute_constraint(MainWindow& window,
                                       SketchSolverConstraintKind kind) {
  auto topology = active_topology(window);
  if (topology.has_error()) return Result<std::size_t>::failure(topology.error());
  auto targets = selected_targets(window, topology.value());
  if (targets.has_error()) return Result<std::size_t>::failure(targets.error());
  const auto compatible =
      GuiSketchConstraintController::compatible_kinds(topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(Error::validation(
        "sketch_constraint_gui", "current Sketch selection is incompatible with this constraint"));
  auto catalog = window.session().sketch_constraint_catalog(topology.value().sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  auto intent = SketchConstraintIntent::create(next_constraint_id(catalog.value(), kind), kind,
                                                std::move(targets.value()));
  if (intent.has_error()) return Result<std::size_t>::failure(intent.error());
  auto controller = GuiSketchConstraintController::create(
      window.session(), topology.value().sketch(), std::move(intent.value()));
  if (controller.has_error()) return Result<std::size_t>::failure(controller.error());
  if (!controller.value().preview().authoring.accepted) {
    const auto& preview = controller.value().preview().authoring;
    std::string message = "constraint preview refused: " +
                          std::string(to_string(preview.solve.status));
    if (!preview.conflict_ids.empty()) {
      message += " [";
      for (std::size_t index = 0U; index < preview.conflict_ids.size(); ++index) {
        if (index > 0U) message += ", ";
        message += preview.conflict_ids[index];
      }
      message += "]";
    }
    return Result<std::size_t>::failure(
        Error::validation(preview.candidate.id().value(), std::move(message)));
  }
  auto committed = controller.value().commit(window.session());
  if (committed.has_error()) return committed;
  window.session().selection().clear();
  window.render_sketch(topology.value().sketch());
  return committed;
}

class SketchConstraintBinder final : public QObject {
public:
  explicit SketchConstraintBinder(MainWindow& window) : QObject(&window), window_(window) {
    setObjectName(QStringLiteral("blcad.sketch.constraint_binder"));
    auto* menu = window_.findChild<QMenu*>(QStringLiteral("blcad.menu.sketch"));
    if (menu != nullptr) menu->addSeparator();
    for (const auto& current : kConstraintActions) {
      GuiCommandSpec spec;
      spec.id = current.command;
      spec.label = current.label;
      spec.description = current.description;
      spec.workspace = GuiWorkspace::Sketch;
      spec.allowed_documents = {GuiDocumentKind::Part};
      spec.requires_idle_task = true;
      spec.minimum_selection_count = 1U;
      spec.required_selection_mask = selection_kind_bit(GuiSelectionKind::SketchEntity);
      spec.enabled_when = [this](const GuiCommandContext& context) {
        return context.workspace == GuiWorkspace::Sketch && window_.active_sketch().has_value();
      };
      auto added = window_.command_registry().add(
          std::move(spec), [this, kind = current.kind]() {
            auto result = execute_constraint(window_, kind);
            if (result.has_error()) append_diagnostic(window_, result.error().message());
            window_.refresh_command_state();
            return result;
          });
      if (added.has_error()) continue;
      auto* action = new QAction(QString::fromUtf8(current.label), &window_);
      action->setObjectName(QString::fromUtf8(current.object_name));
      action->setToolTip(QString::fromUtf8(current.description));
      action->setProperty("blcad.constraint_kind", static_cast<int>(current.kind));
      connect(action, &QAction::triggered, this, [this, command = std::string(current.command)] {
        auto executed = window_.command_registry().execute(command, window_.command_context());
        if (executed.has_error()) append_diagnostic(window_, executed.error().message());
        window_.refresh_command_state();
      });
      if (menu != nullptr) menu->addAction(action);
      actions_.push_back(action);
    }

    GuiCommandSpec delete_spec;
    delete_spec.id = "sketch.constraint.delete";
    delete_spec.label = "Delete constraint";
    delete_spec.description = "Remove the selected accepted sketch constraint";
    delete_spec.workspace = GuiWorkspace::Sketch;
    delete_spec.allowed_documents = {GuiDocumentKind::Part};
    delete_spec.requires_idle_task = true;
    delete_spec.minimum_selection_count = 1U;
    delete_spec.required_selection_mask = selection_kind_bit(GuiSelectionKind::SketchEntity);
    delete_spec.enabled_when = [this](const GuiCommandContext& context) {
      return context.workspace == GuiWorkspace::Sketch && window_.active_sketch().has_value();
    };
    auto delete_added = window_.command_registry().add(std::move(delete_spec), [this]() {
      auto result = execute_constraint_removal(window_);
      if (result.has_error()) append_diagnostic(window_, result.error().message());
      window_.refresh_command_state();
      return result;
    });
    if (!delete_added.has_error()) {
      delete_action_ = new QAction(QStringLiteral("Delete constraint"), &window_);
      delete_action_->setObjectName(QStringLiteral("blcad.action.sketch_constraint.delete"));
      delete_action_->setToolTip(
          QStringLiteral("Remove the selected accepted sketch constraint"));
      connect(delete_action_, &QAction::triggered, this, [this] {
        auto executed = window_.command_registry().execute("sketch.constraint.delete",
                                                           window_.command_context());
        if (executed.has_error()) append_diagnostic(window_, executed.error().message());
        window_.refresh_command_state();
      });
      if (menu != nullptr) menu->addAction(delete_action_);
    }

    if (auto* viewport = window_.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport")))
      viewport->installEventFilter(this);
    refresh();
  }

  void refresh() {
    std::vector<SketchSolverConstraintKind> compatible;
    if (window_.active_sketch() && window_.session().part_document() != nullptr &&
        window_.session().workspace() == GuiWorkspace::Sketch && !window_.session().task().active()) {
      auto topology = active_topology(window_);
      if (!topology.has_error()) {
        auto targets = selected_targets(window_, topology.value());
        if (!targets.has_error())
          compatible = GuiSketchConstraintController::compatible_kinds(
              topology.value(), targets.value());
      }
    }
    for (auto* action : actions_) {
      const auto kind = static_cast<SketchSolverConstraintKind>(
          action->property("blcad.constraint_kind").toInt());
      action->setEnabled(std::find(compatible.begin(), compatible.end(), kind) != compatible.end());
    }
    if (delete_action_ != nullptr)
      delete_action_->setEnabled(selected_accepted_constraint(window_).has_value() &&
                                 !window_.session().task().active());
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    (void)watched;
    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyRelease ||
        event->type() == QEvent::FocusIn)
      QTimer::singleShot(0, this, [this] { refresh(); });
    return QObject::eventFilter(watched, event);
  }

private:
  MainWindow& window_;
  std::vector<QAction*> actions_;
  QAction* delete_action_{nullptr};
};

} // namespace

void install_sketch_constraint_binder(MainWindow& window) {
  if (window.findChild<QObject*>(QStringLiteral("blcad.sketch.constraint_binder")) != nullptr)
    return;
  (void)new SketchConstraintBinder(window);
}

void refresh_sketch_constraint_actions(MainWindow& window) {
  if (auto* binder =
          window.findChild<SketchConstraintBinder*>(QStringLiteral("blcad.sketch.constraint_binder")))
    binder->refresh();
}

} // namespace blcad::gui
