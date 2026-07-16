#pragma once

#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QStringList>
#include <QTextEdit>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

void refresh_sketch_interaction_binder(MainWindow& window);

namespace detail {

struct DimensionActionDescriptor {
  SketchDimensionKind kind;
  const char* command;
  const char* object_name;
  const char* label;
};

inline constexpr std::array<DimensionActionDescriptor, 9U> kDimensionActions{{
    {SketchDimensionKind::HorizontalDistance, "sketch.dimension.horizontal",
     "blcad.action.sketch_dimension.horizontal", "Horizontal distance"},
    {SketchDimensionKind::VerticalDistance, "sketch.dimension.vertical",
     "blcad.action.sketch_dimension.vertical", "Vertical distance"},
    {SketchDimensionKind::AlignedDistance, "sketch.dimension.aligned",
     "blcad.action.sketch_dimension.aligned", "Aligned distance"},
    {SketchDimensionKind::PointToPointDistance, "sketch.dimension.point_to_point",
     "blcad.action.sketch_dimension.point_to_point", "Point-to-point distance"},
    {SketchDimensionKind::Length, "sketch.dimension.length",
     "blcad.action.sketch_dimension.length", "Length"},
    {SketchDimensionKind::Radius, "sketch.dimension.radius",
     "blcad.action.sketch_dimension.radius", "Radius"},
    {SketchDimensionKind::Diameter, "sketch.dimension.diameter",
     "blcad.action.sketch_dimension.diameter", "Diameter"},
    {SketchDimensionKind::Angle, "sketch.dimension.angle",
     "blcad.action.sketch_dimension.angle", "Angle"},
    {SketchDimensionKind::ArcLength, "sketch.dimension.arc_length",
     "blcad.action.sketch_dimension.arc_length", "Arc length"},
}};

inline QMenu* find_sketch_menu(MainWindow& window) {
  for (QAction* action : window.menuBar()->actions()) {
    QMenu* menu = action->menu();
    if (menu == nullptr) continue;
    QString title = menu->title();
    title.remove(QLatin1Char('&'));
    if (title == QStringLiteral("Sketch")) return menu;
  }
  return nullptr;
}

inline void append_error(MainWindow& window, const Error& error) {
  if (auto* output = window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics")))
    output->append(QStringLiteral("[Sketch Dimension] %1: %2")
                       .arg(QString::fromStdString(error.object_id()),
                            QString::fromStdString(error.message())));
}

inline Result<SketchTopology> active_topology(const MainWindow& window) {
  if (!window.active_sketch() || window.session().part_document() == nullptr)
    return Result<SketchTopology>::failure(Error::validation(
        "sketch_dimension_gui", "no active Sketch dimension workspace"));
  const Sketch* sketch =
      window.session().part_document()->find_sketch(*window.active_sketch());
  if (sketch == nullptr)
    return Result<SketchTopology>::failure(Error::validation(
        window.active_sketch()->value(), "active Sketch no longer exists"));
  return SketchTopology::migrate_legacy(*sketch);
}

inline std::optional<std::string> resolve_entity(const SketchTopology& topology,
                                                 std::string semantic) {
  if (topology.find_entity(semantic) != nullptr) return semantic;
  const std::string prefix = "sketch/" + topology.sketch().value() + "/";
  if (std::string_view(semantic).starts_with(prefix)) semantic.erase(0U, prefix.size());
  if (topology.find_entity(semantic) != nullptr) return semantic;
  for (const std::string_view kind : {std::string_view("entity/"),
                                      std::string_view("profile/")}) {
    const std::string candidate = std::string(kind) + semantic;
    if (topology.find_entity(candidate) != nullptr) return candidate;
  }
  return std::nullopt;
}

inline std::optional<SketchPointId> resolve_point_role(
    const SketchTopology& topology, const SketchTopologyEntity& entity,
    std::string_view candidate) {
  const SketchPointId direct{std::string(candidate)};
  if (topology.find_point(direct) != nullptr) return direct;
  const auto separator = candidate.rfind(':');
  if (separator == std::string_view::npos) return std::nullopt;
  const std::string_view role = candidate.substr(separator + 1U);
  if (entity.kind() == SketchTopologyEntityKind::Line) {
    if (role == "start") return entity.points()[0];
    if (role == "end") return entity.points()[1];
  }
  if (entity.kind() == SketchTopologyEntityKind::Arc) {
    if (role == "start") return entity.points()[0];
    if (role == "mid" || role == "midpoint") return entity.points()[1];
    if (role == "end") return entity.points()[2];
  }
  if (entity.kind() == SketchTopologyEntityKind::Spline) {
    if (role == "start") return entity.points()[0];
    if (role == "end") return entity.points()[3];
  }
  return std::nullopt;
}

inline Result<std::vector<SketchConstraintIntentTarget>> selected_targets(
    const MainWindow& window, const SketchTopology& topology) {
  std::vector<SketchConstraintIntentTarget> targets;
  const std::string dimension_prefix =
      "sketch/" + topology.sketch().value() + "/dimension/";
  for (const auto& selection : window.session().selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity ||
        std::string_view(selection.semantic_id).starts_with(dimension_prefix))
      continue;
    const auto entity_id = resolve_entity(topology, selection.semantic_id);
    if (!entity_id)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "selected Sketch item is not stable topology"));
    const auto* entity = topology.find_entity(*entity_id);
    if (entity == nullptr)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "selected Sketch entity disappeared"));
    if (!selection.candidate_id.empty()) {
      if (const auto point_id =
              resolve_point_role(topology, *entity, selection.candidate_id)) {
        auto target = SketchConstraintIntentTarget::point(*point_id);
        if (target.has_error())
          return Result<std::vector<SketchConstraintIntentTarget>>::failure(target.error());
        if (std::find(targets.begin(), targets.end(), target.value()) == targets.end())
          targets.push_back(std::move(target.value()));
        continue;
      }
    }
    auto target = SketchConstraintIntentTarget::entity(*entity_id);
    if (target.has_error())
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(target.error());
    if (std::find(targets.begin(), targets.end(), target.value()) == targets.end())
      targets.push_back(std::move(target.value()));
  }
  return Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(targets));
}

inline std::optional<SketchDimensionId> selected_dimension(const MainWindow& window,
                                                           const SketchId& sketch) {
  if (window.session().selection().entries().size() != 1U) return std::nullopt;
  const auto& selection = window.session().selection().entries().front();
  const std::string prefix = "sketch/" + sketch.value() + "/dimension/";
  if (!std::string_view(selection.semantic_id).starts_with(prefix)) return std::nullopt;
  return SketchDimensionId(selection.semantic_id.substr(prefix.size()));
}

inline void canonicalize_order(const SketchTopology& topology, SketchDimensionKind kind,
                               std::vector<SketchConstraintIntentTarget>& targets) {
  if (targets.size() != 2U) return;
  if ((kind == SketchDimensionKind::HorizontalDistance ||
       kind == SketchDimensionKind::VerticalDistance) &&
      targets[0].kind() == SketchConstraintIntentTargetKind::Point &&
      targets[1].kind() == SketchConstraintIntentTargetKind::Point) {
    const Point2 first = topology.find_point(SketchPointId(targets[0].id()))->position();
    const Point2 second = topology.find_point(SketchPointId(targets[1].id()))->position();
    if ((kind == SketchDimensionKind::HorizontalDistance && second.x < first.x) ||
        (kind == SketchDimensionKind::VerticalDistance && second.y < first.y))
      std::swap(targets[0], targets[1]);
  }
  if (kind != SketchDimensionKind::Angle ||
      targets[0].kind() != SketchConstraintIntentTargetKind::Entity ||
      targets[1].kind() != SketchConstraintIntentTargetKind::Entity)
    return;
  const auto* first = topology.find_entity(targets[0].id());
  const auto* second = topology.find_entity(targets[1].id());
  if (first == nullptr || second == nullptr || first->points().size() != 2U ||
      second->points().size() != 2U)
    return;
  const Point2 a0 = topology.find_point(first->points()[0])->position();
  const Point2 a1 = topology.find_point(first->points()[1])->position();
  const Point2 b0 = topology.find_point(second->points()[0])->position();
  const Point2 b1 = topology.find_point(second->points()[1])->position();
  const double ax = a1.x - a0.x;
  const double ay = a1.y - a0.y;
  const double bx = b1.x - b0.x;
  const double by = b1.y - b0.y;
  if (std::atan2(ax * by - ay * bx, ax * bx + ay * by) < 0.0)
    std::swap(targets[0], targets[1]);
}

inline SketchDimensionId next_id(const SketchDimensionCatalog& catalog,
                                 SketchDimensionKind kind) {
  const std::string prefix = "dimension.gui." + std::string(to_string(kind)) + ".";
  std::size_t index = 1U;
  while (catalog.find(SketchDimensionId(prefix + std::to_string(index))) != nullptr) ++index;
  return SketchDimensionId(prefix + std::to_string(index));
}

inline std::optional<ParameterId> choose_parameter(MainWindow& window,
                                                   SketchDimensionKind kind) {
  const auto parameters = GuiSketchDimensionController::compatible_parameters(
      *window.session().part_document(), kind);
  if (parameters.empty()) {
    append_error(window, Error::validation(
        "sketch_dimension_gui", "no compatible Length/Angle parameter exists"));
    return std::nullopt;
  }
  QStringList items;
  for (const auto& parameter : parameters)
    items.push_back(QString::fromStdString(parameter.value()));
  bool accepted = false;
  const QString selected = QInputDialog::getItem(
      &window, QStringLiteral("Sketch dimension parameter"),
      QStringLiteral("Bind to parameter:"), items, 0, false, &accepted);
  return accepted && !selected.isEmpty()
             ? std::optional<ParameterId>(ParameterId(selected.toStdString()))
             : std::nullopt;
}

inline Result<std::size_t> add_dimension(MainWindow& window, SketchDimensionKind kind,
                                         bool reference) {
  auto topology = active_topology(window);
  if (topology.has_error()) return Result<std::size_t>::failure(topology.error());
  auto targets = selected_targets(window, topology.value());
  if (targets.has_error()) return Result<std::size_t>::failure(targets.error());
  const auto compatible = GuiSketchDimensionController::compatible_kinds(
      topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "selection is incompatible with this dimension"));
  canonicalize_order(topology.value(), kind, targets.value());
  auto catalog = window.session().sketch_dimension_catalog(topology.value().sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  std::optional<ParameterId> parameter;
  if (!reference) {
    parameter = choose_parameter(window, kind);
    if (!parameter) return Result<std::size_t>::success(0U);
  }
  auto intent = SketchDimensionIntent::create(
      next_id(catalog.value(), kind), kind,
      reference ? SketchDimensionMode::Reference : SketchDimensionMode::Driving,
      std::move(targets.value()), parameter);
  if (intent.has_error()) return Result<std::size_t>::failure(intent.error());
  auto controller = GuiSketchDimensionController::create(
      window.session(), topology.value().sketch(), std::move(intent.value()));
  if (controller.has_error()) return Result<std::size_t>::failure(controller.error());
  if (!controller.value().preview().authoring.accepted)
    return Result<std::size_t>::failure(Error::validation(
        controller.value().preview().authoring.candidate.id().value(),
        "dimension preview was rejected"));
  return controller.value().commit(window.session());
}

inline Result<std::size_t> edit_dimension(MainWindow& window) {
  if (!window.active_sketch() || window.session().part_document() == nullptr)
    return Result<std::size_t>::failure(
        Error::validation("sketch_dimension_gui", "no active Sketch"));
  const auto id = selected_dimension(window, *window.active_sketch());
  if (!id)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*id);
  if (dimension == nullptr || !dimension->driving() || !dimension->parameter())
    return Result<std::size_t>::failure(Error::validation(
        id->value(), "selected dimension has no editable driving value"));
  const Parameter* parameter =
      window.session().part_document()->find_parameter(*dimension->parameter());
  if (parameter == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "dimension parameter does not exist"));
  const QString initial = parameter->is_expression()
                              ? QString::fromStdString(*parameter->formula())
                              : QString::number(dimension->kind() == SketchDimensionKind::Angle
                                                    ? parameter->value().degrees()
                                                    : parameter->value().millimeters(),
                                                'g', 12);
  bool accepted = false;
  const QString text = QInputDialog::getText(
      &window, QStringLiteral("Edit Sketch dimension"),
      parameter->is_expression() ? QStringLiteral("Formula:")
                                 : QStringLiteral("Value:"),
      QLineEdit::Normal, initial, &accepted);
  if (!accepted || text.trimmed().isEmpty()) return Result<std::size_t>::success(0U);
  if (parameter->is_expression())
    return GuiSketchDimensionController::edit_formula(
        window.session(), *window.active_sketch(), *id, text.trimmed().toStdString());
  bool numeric = false;
  const double value = text.trimmed().toDouble(&numeric);
  if (!numeric || !std::isfinite(value) || value <= 0.0)
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "dimension value must be positive and finite"));
  auto quantity = dimension->kind() == SketchDimensionKind::Angle
                      ? Quantity::angle_deg(value, id->value())
                      : Quantity::length_mm(value, id->value());
  if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
  return GuiSketchDimensionController::edit_literal(
      window.session(), *window.active_sketch(), *id, quantity.value());
}

inline Result<std::size_t> rebind_dimension(MainWindow& window) {
  if (!window.active_sketch())
    return Result<std::size_t>::failure(
        Error::validation("sketch_dimension_gui", "no active Sketch"));
  const auto id = selected_dimension(window, *window.active_sketch());
  if (!id)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*id);
  if (dimension == nullptr || !dimension->driving())
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "only driving dimensions can be rebound"));
  const auto parameter = choose_parameter(window, dimension->kind());
  return parameter ? GuiSketchDimensionController::rebind_parameter(
                         window.session(), *window.active_sketch(), *id, *parameter)
                   : Result<std::size_t>::success(0U);
}

inline Result<std::size_t> toggle_mode(MainWindow& window) {
  if (!window.active_sketch())
    return Result<std::size_t>::failure(
        Error::validation("sketch_dimension_gui", "no active Sketch"));
  const auto id = selected_dimension(window, *window.active_sketch());
  if (!id)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*id);
  if (dimension == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "dimension does not exist"));
  if (dimension->driving())
    return GuiSketchDimensionController::set_mode(
        window.session(), *window.active_sketch(), *id,
        SketchDimensionMode::Reference);
  const auto parameter = choose_parameter(window, dimension->kind());
  return parameter ? GuiSketchDimensionController::set_mode(
                         window.session(), *window.active_sketch(), *id,
                         SketchDimensionMode::Driving, *parameter)
                   : Result<std::size_t>::success(0U);
}

class SketchDimensionBinder final : public QObject {
public:
  explicit SketchDimensionBinder(MainWindow& window) : QObject(&window), window_(window) {
    setObjectName(QStringLiteral("blcad.sketch.dimension_binder"));
    if (QMenu* root = find_sketch_menu(window_)) {
      menu_ = root->addMenu(QStringLiteral("Dimension"));
      menu_->setObjectName(QStringLiteral("blcad.menu.sketch_dimension"));
    }
    reference_mode_ = add_action("blcad.action.sketch_dimension.reference_mode",
                                 "Create reference dimensions");
    reference_mode_->setCheckable(true);
    if (menu_) menu_->addSeparator();
    for (const auto& descriptor : kDimensionActions) {
      (void)window_.command_registry().register_command(
          {descriptor.command, descriptor.label, [](const GuiCommandContext& context) {
             return context.document_kind == GuiDocumentKind::Part &&
                    context.workspace == GuiWorkspace::Sketch && !context.task_active();
           }});
      QAction* action = add_action(descriptor.object_name, descriptor.label);
      action->setProperty("blcad.dimension_kind", static_cast<int>(descriptor.kind));
      connect(action, &QAction::triggered, this, [this, kind = descriptor.kind] {
        run([this, kind] { return add_dimension(window_, kind, reference_mode_->isChecked()); });
      });
      family_actions_.push_back(action);
    }
    if (menu_) menu_->addSeparator();
    edit_action_ = add_action("blcad.action.sketch_dimension.edit",
                              "Edit selected dimension…");
    rebind_action_ = add_action("blcad.action.sketch_dimension.rebind",
                                "Rebind selected dimension…");
    mode_action_ = add_action("blcad.action.sketch_dimension.toggle_mode",
                              "Toggle driving/reference");
    connect(edit_action_, &QAction::triggered, this,
            [this] { run([this] { return edit_dimension(window_); }); });
    connect(rebind_action_, &QAction::triggered, this,
            [this] { run([this] { return rebind_dimension(window_); }); });
    connect(mode_action_, &QAction::triggered, this,
            [this] { run([this] { return toggle_mode(window_); }); });
    refresh();
  }

  void refresh() {
    const bool active = window_.sketch_workspace().active() &&
                        window_.active_sketch().has_value() &&
                        window_.session().part_document() != nullptr &&
                        !window_.session().task().active();
    if (menu_) menu_->menuAction()->setVisible(active);
    reference_mode_->setEnabled(active);
    std::vector<SketchDimensionKind> compatible;
    if (active) {
      auto topology = active_topology(window_);
      if (!topology.has_error()) {
        auto targets = selected_targets(window_, topology.value());
        if (!targets.has_error())
          compatible = GuiSketchDimensionController::compatible_kinds(
              topology.value(), targets.value());
      }
    }
    for (QAction* action : family_actions_) {
      const auto kind = static_cast<SketchDimensionKind>(
          action->property("blcad.dimension_kind").toInt());
      action->setEnabled(active &&
                         std::find(compatible.begin(), compatible.end(), kind) != compatible.end());
    }
    const bool selected = active &&
                          selected_dimension(window_, *window_.active_sketch()).has_value();
    edit_action_->setEnabled(selected);
    rebind_action_->setEnabled(selected);
    mode_action_->setEnabled(selected);
  }

private:
  QAction* add_action(const char* object_name, const char* label) {
    auto* action = new QAction(QString::fromUtf8(label), &window_);
    action->setObjectName(QString::fromUtf8(object_name));
    if (menu_) menu_->addAction(action);
    return action;
  }

  template <typename Operation> void run(Operation operation) {
    auto result = operation();
    if (result.has_error()) append_error(window_, result.error());
    refresh_sketch_interaction_binder(window_);
    window_.refresh_command_state();
    refresh();
  }

  MainWindow& window_;
  QMenu* menu_{nullptr};
  QAction* reference_mode_{nullptr};
  std::vector<QAction*> family_actions_;
  QAction* edit_action_{nullptr};
  QAction* rebind_action_{nullptr};
  QAction* mode_action_{nullptr};
};

} // namespace detail

inline void install_sketch_dimension_binder(MainWindow& window) {
  if (window.findChild<QObject*>(QStringLiteral("blcad.sketch.dimension_binder")) != nullptr)
    return;
  (void)new detail::SketchDimensionBinder(window);
}

inline void refresh_sketch_dimension_actions(MainWindow& window) {
  QObject* object =
      window.findChild<QObject*>(QStringLiteral("blcad.sketch.dimension_binder"));
  if (object != nullptr) static_cast<detail::SketchDimensionBinder*>(object)->refresh();
}

} // namespace blcad::gui
