#pragma once

#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_interaction_binder.hpp"
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

inline QMenu* dimension_sketch_menu(MainWindow& window) {
  for (QAction* action : window.menuBar()->actions()) {
    QMenu* menu = action->menu();
    if (menu == nullptr) continue;
    QString title = menu->title();
    title.remove(QLatin1Char('&'));
    if (title == QStringLiteral("Sketch")) return menu;
  }
  return nullptr;
}

inline void append_dimension_diagnostic(MainWindow& window, const Error& error) {
  if (auto* diagnostics = window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics")))
    diagnostics->append(
        QStringLiteral("[Sketch Dimension] %1: %2")
            .arg(QString::fromStdString(error.object_id()),
                 QString::fromStdString(error.message())));
}

inline Result<SketchTopology> dimension_active_topology(const MainWindow& window) {
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

inline std::optional<std::string>
dimension_entity_id_from_semantic(const SketchTopology& topology, std::string semantic) {
  if (topology.find_entity(semantic) != nullptr) return semantic;
  const std::string prefix = "sketch/" + topology.sketch().value() + "/";
  if (std::string_view(semantic).starts_with(prefix)) semantic.erase(0U, prefix.size());
  if (topology.find_entity(semantic) != nullptr) return semantic;
  if (std::string_view(semantic).starts_with("entity/") ||
      std::string_view(semantic).starts_with("profile/")) {
    if (topology.find_entity(semantic) != nullptr) return semantic;
  } else if (topology.find_entity("entity/" + semantic) != nullptr) {
    return "entity/" + semantic;
  } else if (topology.find_entity("profile/" + semantic) != nullptr) {
    return "profile/" + semantic;
  }
  return std::nullopt;
}

inline std::optional<SketchPointId>
dimension_point_id_from_candidate(const SketchTopology& topology,
                                  const SketchTopologyEntity& entity,
                                  std::string_view candidate) {
  if (topology.find_point(SketchPointId(std::string(candidate))) != nullptr)
    return SketchPointId(std::string(candidate));
  const auto suffix_position = candidate.rfind(':');
  if (suffix_position == std::string_view::npos) return std::nullopt;
  const std::string_view role = candidate.substr(suffix_position + 1U);
  if (entity.kind() == SketchTopologyEntityKind::Line) {
    if (role == "start") return entity.points()[0];
    if (role == "end") return entity.points()[1];
  }
  if (entity.kind() == SketchTopologyEntityKind::Arc) {
    if (role == "start") return entity.points()[0];
    if (role == "midpoint" || role == "mid") return entity.points()[1];
    if (role == "end") return entity.points()[2];
  }
  if (entity.kind() == SketchTopologyEntityKind::Spline) {
    if (role == "start") return entity.points()[0];
    if (role == "end") return entity.points()[3];
  }
  return std::nullopt;
}

inline Result<std::vector<SketchConstraintIntentTarget>>
dimension_selected_targets(const MainWindow& window, const SketchTopology& topology) {
  std::vector<SketchConstraintIntentTarget> targets;
  for (const auto& selection : window.session().selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity) continue;
    const std::string dimension_prefix =
        "sketch/" + topology.sketch().value() + "/dimension/";
    if (std::string_view(selection.semantic_id).starts_with(dimension_prefix)) continue;
    const auto entity_id = dimension_entity_id_from_semantic(topology, selection.semantic_id);
    if (!entity_id)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "selected Sketch item is not a persistent topology entity"));
    const auto* entity = topology.find_entity(*entity_id);
    if (entity == nullptr)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "selected Sketch entity disappeared"));
    if (!selection.candidate_id.empty()) {
      const auto point_id = dimension_point_id_from_candidate(
          topology, *entity, selection.candidate_id);
      if (point_id) {
        auto point = SketchConstraintIntentTarget::point(*point_id);
        if (point.has_error())
          return Result<std::vector<SketchConstraintIntentTarget>>::failure(point.error());
        if (std::find(targets.begin(), targets.end(), point.value()) == targets.end())
          targets.push_back(std::move(point.value()));
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

inline std::optional<SketchDimensionId>
dimension_selected_annotation(const MainWindow& window, const SketchId& sketch) {
  if (window.session().selection().entries().size() != 1U) return std::nullopt;
  const auto& selection = window.session().selection().entries().front();
  const std::string prefix = "sketch/" + sketch.value() + "/dimension/";
  if (!std::string_view(selection.semantic_id).starts_with(prefix)) return std::nullopt;
  return SketchDimensionId(selection.semantic_id.substr(prefix.size()));
}

inline void dimension_normalize_target_order(
    const SketchTopology& topology, SketchDimensionKind kind,
    std::vector<SketchConstraintIntentTarget>& targets) {
  if (targets.size() != 2U) return;
  if (kind == SketchDimensionKind::HorizontalDistance ||
      kind == SketchDimensionKind::VerticalDistance) {
    if (targets[0].kind() != SketchConstraintIntentTargetKind::Point ||
        targets[1].kind() != SketchConstraintIntentTargetKind::Point)
      return;
    const Point2 first = topology.find_point(SketchPointId(targets[0].id()))->position();
    const Point2 second = topology.find_point(SketchPointId(targets[1].id()))->position();
    if ((kind == SketchDimensionKind::HorizontalDistance && second.x < first.x) ||
        (kind == SketchDimensionKind::VerticalDistance && second.y < first.y))
      std::swap(targets[0], targets[1]);
  }
  if (kind == SketchDimensionKind::Angle &&
      targets[0].kind() == SketchConstraintIntentTargetKind::Entity &&
      targets[1].kind() == SketchConstraintIntentTargetKind::Entity) {
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
}

inline SketchDimensionId dimension_next_id(const SketchDimensionCatalog& catalog,
                                            SketchDimensionKind kind) {
  const std::string base = "dimension.gui." + std::string(to_string(kind)) + ".";
  std::size_t index = 1U;
  while (catalog.find(SketchDimensionId(base + std::to_string(index))) != nullptr) ++index;
  return SketchDimensionId(base + std::to_string(index));
}

inline std::optional<ParameterId> dimension_select_parameter(MainWindow& window,
                                                             SketchDimensionKind kind) {
  const auto parameters = GuiSketchDimensionController::compatible_parameters(
      *window.session().part_document(), kind);
  if (parameters.empty()) {
    append_dimension_diagnostic(window, Error::validation(
        "sketch_dimension_gui",
        "driving dimension requires an existing compatible Length/Angle parameter"));
    return std::nullopt;
  }
  QStringList items;
  for (const auto& parameter : parameters)
    items.push_back(QString::fromStdString(parameter.value()));
  bool accepted = false;
  const QString selected = QInputDialog::getItem(
      &window, QStringLiteral("Sketch dimension parameter"),
      QStringLiteral("Bind to parameter:"), items, 0, false, &accepted);
  if (!accepted || selected.isEmpty()) return std::nullopt;
  return ParameterId(selected.toStdString());
}

inline Result<std::size_t> dimension_execute_add(MainWindow& window,
                                                 SketchDimensionKind kind,
                                                 bool reference_mode) {
  auto topology = dimension_active_topology(window);
  if (topology.has_error()) return Result<std::size_t>::failure(topology.error());
  auto targets = dimension_selected_targets(window, topology.value());
  if (targets.has_error()) return Result<std::size_t>::failure(targets.error());
  const auto compatible = GuiSketchDimensionController::compatible_kinds(
      topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "current Sketch selection is incompatible with this dimension"));
  dimension_normalize_target_order(topology.value(), kind, targets.value());
  auto catalog = window.session().sketch_dimension_catalog(topology.value().sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  std::optional<ParameterId> parameter;
  const SketchDimensionMode mode = reference_mode ? SketchDimensionMode::Reference
                                                   : SketchDimensionMode::Driving;
  if (mode == SketchDimensionMode::Driving) {
    parameter = dimension_select_parameter(window, kind);
    if (!parameter) return Result<std::size_t>::success(0U);
  }
  auto intent = SketchDimensionIntent::create(
      dimension_next_id(catalog.value(), kind), kind, mode,
      std::move(targets.value()), parameter);
  if (intent.has_error()) return Result<std::size_t>::failure(intent.error());
  auto controller = GuiSketchDimensionController::create(
      window.session(), topology.value().sketch(), std::move(intent.value()));
  if (controller.has_error()) return Result<std::size_t>::failure(controller.error());
  if (!controller.value().preview().authoring.accepted)
    return Result<std::size_t>::failure(Error::validation(
        controller.value().preview().authoring.candidate.id().value(),
        "dimension preview is conflicting, redundant, invalid, or non-convergent"));
  return controller.value().commit(window.session());
}

inline Result<std::size_t> dimension_execute_edit(MainWindow& window) {
  if (!window.active_sketch() || window.session().part_document() == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "dimension edit requires an active Sketch"));
  const auto selected = dimension_selected_annotation(window, *window.active_sketch());
  if (!selected)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*selected);
  if (dimension == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "selected dimension does not exist"));
  if (!dimension->driving() || !dimension->parameter())
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "reference dimension has no editable driving value"));
  const Parameter* parameter =
      window.session().part_document()->find_parameter(*dimension->parameter());
  if (parameter == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "dimension parameter does not exist"));
  const QString initial = parameter->is_expression()
                              ? QString::fromStdString(*parameter->formula())
                              : QString::number(dimension->kind() == SketchDimensionKind::Angle
                                                    ? parameter->value().degrees()
                                                    : parameter->value().millimeters(),
                                                'g', 12);
  bool accepted = false;
  const QString edited = QInputDialog::getText(
      &window, QStringLiteral("Edit Sketch dimension"),
      parameter->is_expression() ? QStringLiteral("Formula:")
                                 : QStringLiteral("Value:"),
      QLineEdit::Normal, initial, &accepted);
  if (!accepted || edited.trimmed().isEmpty()) return Result<std::size_t>::success(0U);
  if (parameter->is_expression())
    return GuiSketchDimensionController::edit_formula(
        window.session(), *window.active_sketch(), *selected,
        edited.trimmed().toStdString());
  bool numeric = false;
  const double value = edited.trimmed().toDouble(&numeric);
  if (!numeric || !std::isfinite(value) || value <= 0.0)
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "direct dimension edit requires a positive numeric value"));
  auto quantity = dimension->kind() == SketchDimensionKind::Angle
                      ? Quantity::angle_deg(value, selected->value())
                      : Quantity::length_mm(value, selected->value());
  if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
  return GuiSketchDimensionController::edit_literal(
      window.session(), *window.active_sketch(), *selected, quantity.value());
}

inline Result<std::size_t> dimension_execute_rebind(MainWindow& window) {
  if (!window.active_sketch())
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "dimension rebind requires an active Sketch"));
  const auto selected = dimension_selected_annotation(window, *window.active_sketch());
  if (!selected)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*selected);
  if (dimension == nullptr || !dimension->driving())
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "only a driving dimension can be rebound"));
  auto parameter = dimension_select_parameter(window, dimension->kind());
  if (!parameter) return Result<std::size_t>::success(0U);
  return GuiSketchDimensionController::rebind_parameter(
      window.session(), *window.active_sketch(), *selected, *parameter);
}

inline Result<std::size_t> dimension_execute_toggle_mode(MainWindow& window) {
  if (!window.active_sketch())
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "dimension mode change requires an active Sketch"));
  const auto selected = dimension_selected_annotation(window, *window.active_sketch());
  if (!selected)
    return Result<std::size_t>::failure(Error::validation(
        "sketch_dimension_gui", "select exactly one dimension annotation"));
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*selected);
  if (dimension == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        selected->value(), "selected dimension does not exist"));
  if (dimension->driving())
    return GuiSketchDimensionController::set_mode(
        window.session(), *window.active_sketch(), *selected,
        SketchDimensionMode::Reference);
  auto parameter = dimension_select_parameter(window, dimension->kind());
  if (!parameter) return Result<std::size_t>::success(0U);
  return GuiSketchDimensionController::set_mode(
      window.session(), *window.active_sketch(), *selected,
      SketchDimensionMode::Driving, *parameter);
}

class SketchDimensionBinder final : public QObject {
public:
  explicit SketchDimensionBinder(MainWindow& window) : QObject(&window), window_(window) {
    setObjectName(QStringLiteral("blcad.sketch.dimension_binder"));
    QMenu* root = dimension_sketch_menu(window_);
    menu_ = root == nullptr ? nullptr : root->addMenu(QStringLiteral("Dimension"));
    if (menu_ != nullptr) menu_->setObjectName(QStringLiteral("blcad.menu.sketch_dimension"));

    reference_mode_ = new QAction(QStringLiteral("Create reference dimensions"), &window_);
    reference_mode_->setObjectName(QStringLiteral("blcad.action.sketch_dimension.reference_mode"));
    reference_mode_->setCheckable(true);
    if (menu_ != nullptr) menu_->addAction(reference_mode_);
    if (menu_ != nullptr) menu_->addSeparator();

    for (const auto& descriptor : kDimensionActions) {
      (void)window_.command_registry().register_command(
          {descriptor.command, descriptor.label,
           [](const GuiCommandContext& context) {
             return context.document_kind == GuiDocumentKind::Part &&
                    context.workspace == GuiWorkspace::Sketch &&
                    !context.task_active();
           }});
      auto* action = new QAction(QString::fromUtf8(descriptor.label), &window_);
      action->setObjectName(QString::fromUtf8(descriptor.object_name));
      action->setProperty("blcad.dimension_kind", static_cast<int>(descriptor.kind));
      connect(action, &QAction::triggered, this, [this, kind = descriptor.kind] {
        run([this, kind] {
          return dimension_execute_add(window_, kind, reference_mode_->isChecked());
        });
      });
      if (menu_ != nullptr) menu_->addAction(action);
      family_actions_.push_back(action);
    }

    if (menu_ != nullptr) menu_->addSeparator();
    edit_action_ = new QAction(QStringLiteral("Edit selected dimension…"), &window_);
    edit_action_->setObjectName(QStringLiteral("blcad.action.sketch_dimension.edit"));
    rebind_action_ = new QAction(QStringLiteral("Rebind selected dimension…"), &window_);
    rebind_action_->setObjectName(QStringLiteral("blcad.action.sketch_dimension.rebind"));
    mode_action_ = new QAction(QStringLiteral("Toggle driving/reference"), &window_);
    mode_action_->setObjectName(QStringLiteral("blcad.action.sketch_dimension.toggle_mode"));
    connect(edit_action_, &QAction::triggered, this,
            [this] { run([this] { return dimension_execute_edit(window_); }); });
    connect(rebind_action_, &QAction::triggered, this,
            [this] { run([this] { return dimension_execute_rebind(window_); }); });
    connect(mode_action_, &QAction::triggered, this,
            [this] { run([this] { return dimension_execute_toggle_mode(window_); }); });
    if (menu_ != nullptr) {
      menu_->addAction(edit_action_);
      menu_->addAction(rebind_action_);
      menu_->addAction(mode_action_);
    }
    refresh();
  }

  void refresh() {
    const bool active = window_.sketch_workspace().active() &&
                        window_.active_sketch().has_value() &&
                        window_.session().part_document() != nullptr &&
                        !window_.session().task().active();
    if (menu_ != nullptr) menu_->menuAction()->setVisible(active);
    if (reference_mode_ != nullptr) reference_mode_->setEnabled(active);
    std::vector<SketchDimensionKind> compatible;
    if (active) {
      auto topology = dimension_active_topology(window_);
      if (!topology.has_error()) {
        auto targets = dimension_selected_targets(window_, topology.value());
        if (!targets.has_error())
          compatible = GuiSketchDimensionController::compatible_kinds(
              topology.value(), targets.value());
      }
    }
    for (auto* action : family_actions_) {
      const auto kind = static_cast<SketchDimensionKind>(
          action->property("blcad.dimension_kind").toInt());
      action->setEnabled(active &&
                         std::find(compatible.begin(), compatible.end(), kind) != compatible.end());
    }
    const bool dimension_selected =
        active && dimension_selected_annotation(window_, *window_.active_sketch()).has_value();
    edit_action_->setEnabled(dimension_selected);
    rebind_action_->setEnabled(dimension_selected);
    mode_action_->setEnabled(dimension_selected);
  }

private:
  template <typename Operation> void run(Operation operation) {
    auto result = operation();
    if (result.has_error()) append_dimension_diagnostic(window_, result.error());
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
  if (auto* binder = window.findChild<detail::SketchDimensionBinder*>(
          QStringLiteral("blcad.sketch.dimension_binder")))
    binder->refresh();
}

} // namespace blcad::gui
