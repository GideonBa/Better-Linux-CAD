#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include "blcad/gui/gui_step_import_context.hpp"

#include <QAction>
#include <QComboBox>
#include <QListWidget>
#include <QSignalBlocker>

#include <algorithm>

namespace blcad::gui {
QAction* ShellAssemblyTools::context_action(Command command) const noexcept {
  if (command != Command::Placement && command != Command::Relationship &&
      command != Command::Joint && command != Command::Motion && command != Command::Solve)
    return nullptr;
  for (const auto& [kind, action] : actions_)
    if (kind == command)
      return action;
  return nullptr;
}

std::optional<ShellAssemblyTools::Context>
ShellAssemblyTools::context_for(Command command, const GuiSelection& selection) const {
  const auto* project = session_.project();
  if (!project || !session_.derived_results_fresh() || !context_action(command))
    return std::nullopt;
  if (selection.kind == GuiSelectionKind::Component) {
    for (const auto& component : project->assembly().component_instances()) {
      if (selection.semantic_id !=
              "component/" + project->assembly().id().value() + "/" + component.id().value() ||
          component.visibility() != ComponentVisibility::Visible ||
          component.suppression_state() != ComponentSuppressionState::Active)
        continue;
      if (command == Command::Placement || command == Command::Solve)
        return Context{command, selection, component.id().value()};
      if (command == Command::Motion) {
        std::optional<std::string> joint;
        for (const auto& candidate : project->assembly().joints()) {
          if (candidate.state() != AssemblyJointState::Active ||
              candidate.type() == AssemblyJointType::Spherical ||
              (candidate.target_a().component_instance() != component.id() &&
               candidate.target_b().component_instance() != component.id()))
            continue;
          if (joint)
            return std::nullopt;
          joint = candidate.id().value();
        }
        if (joint)
          return Context{command, selection, *joint};
      }
    }
  }
  if (selection.kind == GuiSelectionKind::AssemblyTarget &&
      (command == Command::Relationship || command == Command::Joint)) {
    const auto targets = query_with_step_import_context(session_, [&](auto* context) {
      return geometry::AssemblyInteractionGeometry::targets(*project, context);
    });
    if (targets)
      for (const auto& target : targets.value())
        if (target.presentation.semantic_id == selection.semantic_id)
          return Context{command, selection, target.presentation.semantic_id};
  }
  return std::nullopt;
}

std::optional<ShellAssemblyTools::Context> ShellAssemblyTools::context(Command command) const {
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.reference_repair_mode() || session_.selection().size() != 1)
    return std::nullopt;
  const auto& selection = session_.selection().entries().front();
  if (!session_.selection().allows(selection.kind) ||
      !(viewport_.selection_filter_mask() & selection_kind_bit(selection.kind)))
    return std::nullopt;
  const auto& scope = viewport_.selection_semantic_scope();
  if (scope && std::find(scope->begin(), scope->end(), selection.semantic_id) == scope->end())
    return std::nullopt;
  return context_for(command, selection);
}

bool ShellAssemblyTools::context_available(const Context& source) const {
  const auto current = context_for(source.command, source.selection);
  return current && current->source == source.source;
}

bool ShellAssemblyTools::select_context(const Context& source) {
  if (!active() || command_ != source.command ||
      session_.task().command_id() != "assembly.interactive" || !context_available(source))
    return false;
  if (command_ == Command::Placement || command_ == Command::Motion) {
    auto* combo = command_ == Command::Placement ? occurrence_ : joint_;
    const auto key = QString::fromStdString(
        command_ == Command::Placement ? "component:" + source.source : source.source);
    const int row = combo->findData(key);
    if (row <= 0)
      return false;
    combo->setCurrentIndex(row);
    return true;
  }
  if (command_ == Command::Solve) {
    auto graph = AssemblyConstraintGraph::build(session_.project()->assembly());
    if (!graph)
      return false;
    for (const auto& group : graph.value().connected_components()) {
      if (std::find(group.begin(), group.end(), ComponentInstanceId(source.source)) == group.end())
        continue;
      {
        const QSignalBlocker blocker(solve_group_);
        for (int row = 0; row < solve_group_->count(); ++row) {
          auto* item = solve_group_->item(row);
          const ComponentInstanceId id(item->data(Qt::UserRole).toString().toStdString());
          const auto* component = session_.project()->assembly().find_component_instance(id);
          item->setSelected(std::find(group.begin(), group.end(), id) != group.end() && component &&
                            component->suppression_state() == ComponentSuppressionState::Active);
        }
      }
      changed();
      return true;
    }
    return false;
  }
  for (int row = 1; row < first_->count(); ++row) {
    const auto index = first_->itemData(row).toInt();
    if (index < 0 || static_cast<std::size_t>(index) >= targets_.size() ||
        targets_[index].presentation.semantic_id != source.source)
      continue;
    first_->setCurrentIndex(row);
    second_->setCurrentIndex(0);
    pick_->setCurrentIndex(1);
    changed();
    return true;
  }
  return false;
}
} // namespace blcad::gui
