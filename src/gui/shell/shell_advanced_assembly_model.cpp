#include "advanced_assembly_tools_impl.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_hole_coordinator.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QHeaderView>
#include <QUuid>
#include <algorithm>
#include <set>
#include <type_traits>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("assembly.advanced", std::move(message));
}
QString text(std::string_view s) {
  return QString::fromUtf8(s.data(), int(s.size()));
}
std::string unique(const char* prefix) {
  return std::string(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
ProjectDesignOccurrence design_occurrence(const AssemblyLeafOccurrenceDescriptor& leaf) {
  return {leaf.subassembly_path, leaf.component_instance, leaf.referenced_part_document};
}
QString path_label(const Project* project, const std::vector<SubassemblyInstanceId>& path) {
  QStringList value{"Root"};
  const AssemblyDocument* assembly = project ? &project->assembly() : nullptr;
  for (const auto& id : path) {
    const auto* instance = assembly ? assembly->find_subassembly_instance(id) : nullptr;
    value.push_back(instance ? text(instance->name()) : text(id.value()));
    assembly = instance && project
                   ? project->find_assembly_document(instance->referenced_assembly_document())
                   : nullptr;
  }
  return value.join(" / ");
}
void states(QComboBox& editor, const PartDocument& part, ModelStateId selected) {
  editor.clear();
  editor.addItem("Primary", text(primary_model_state_id().value()));
  for (const auto& state : part.model_states())
    editor.addItem(text(state.name), text(state.id.value()));
  editor.setCurrentIndex(std::max(0, editor.findData(text(selected.value()))));
}
void select(QComboBox& editor, const std::string& id) {
  const int index = editor.findData(text(id));
  if (index >= 0)
    editor.setCurrentIndex(index);
}
QTableWidgetItem* item(QTableWidget& table, int row, int col, QString label) {
  auto* value = new QTableWidgetItem(std::move(label));
  value->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  table.setItem(row, col, value);
  return value;
}
} // namespace
QString ShellAdvancedAssemblyTools::Impl::occurrence_label(
    const AssemblyLeafOccurrenceDescriptor& leaf) const {
  const auto* project = window.session().project();
  if (!project)
    project = window.session().project_part_context_root();
  const auto* assembly =
      project ? project->find_assembly_document(leaf.assembly_document) : nullptr;
  const auto* component =
      assembly ? assembly->find_component_instance(leaf.component_instance) : nullptr;
  return path_label(project, leaf.subassembly_path) + " / " +
         (component ? text(component->name()) : text(leaf.component_instance.value()));
}
const AssemblyLeafOccurrenceDescriptor*
ShellAdvancedAssemblyTools::Impl::selected(QComboBox* input) const {
  const int i = input->currentData().toInt();
  return i >= 0 && std::size_t(i) < leaves.size() ? &leaves[std::size_t(i)] : nullptr;
}
std::string ShellAdvancedAssemblyTools::Impl::label() const {
  switch (mode) {
  case Mode::Flexible:
    return "Set Flexible Occurrence Pose";
  case Mode::Adaptive:
    return "Edit Adaptive Assembly Reference";
  case Mode::AssemblyHole:
    return "Coordinate Assembly Hole Participants";
  case Mode::Representations:
    return "Edit Assembly Design View";
  case Mode::Context:
    return "Edit Part in Assembly";
  }
  return {};
}
void ShellAdvancedAssemblyTools::Impl::load() {
  loading = true;
  candidate.reset();
  const auto& project = *window.session().project();
  record->clear();
  record->addItem("Create new", QString());
  name->setText(mode == Mode::Adaptive       ? "Adaptive reference"
                : mode == Mode::AssemblyHole ? "Assembly hole"
                                             : "Named representation");
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  subassemblies.clear();
  occurrence->clear();
  occurrence->addItem("Select rooted subassembly", -1);
  if (hierarchy)
    for (const auto& value : hierarchy.value().occurrences())
      if (value.active_path && !value.occurrence_path.empty()) {
        occurrence->addItem(path_label(&project, value.occurrence_path), int(subassemblies.size()));
        subassemblies.push_back(value);
      }
  auto resolved =
      AssemblyLeafOccurrenceResolver{}.resolve(project, AssemblyLeafParticipation::PhysicalActive);
  leaves = resolved ? std::move(resolved.value()) : std::vector<AssemblyLeafOccurrenceDescriptor>{};
  for (auto* input : {source, target, context_part}) {
    input->clear();
    input->addItem("Select rooted Part", -1);
    for (std::size_t i = 0; i < leaves.size(); ++i)
      input->addItem(occurrence_label(leaves[i]), int(i));
  }
  layouts.clear();
  layout->clear();
  layout->addItem("Select source Hole", -1);
  for (const auto& leaf : leaves) {
    const auto* part = project.find_part_document(leaf.referenced_part_document);
    for (const auto& hole : part->hole_features()) {
      layout->addItem(occurrence_label(leaf) + " / " + text(hole.name()), int(layouts.size()));
      layouts.push_back({design_occurrence(leaf), hole.id()});
    }
  }
  participants->setRowCount(int(leaves.size()));
  for (int row = 0; row < participants->rowCount(); ++row) {
    const auto& leaf = leaves[std::size_t(row)];
    const auto* part = project.find_part_document(leaf.referenced_part_document);
    auto* use = item(*participants, row, 0, {});
    use->setFlags(use->flags() | Qt::ItemIsUserCheckable);
    use->setCheckState(Qt::Unchecked);
    item(*participants, row, 1, occurrence_label(leaf));
    auto* recipes = new QComboBox(participants);
    recipes->addItem("Choose Hole recipe", QString());
    for (const auto& hole : part->hole_features())
      recipes->addItem(text(hole.name()), text(hole.id().value()));
    apply_gui_choice_icons(*recipes);
    participants->setCellWidget(row, 2, recipes);
    auto* state = new QComboBox(participants);
    states(*state, *part, leaf.selected_model_state);
    participants->setCellWidget(row, 3, state);
    auto* role = new QComboBox(participants);
    role->addItems({"Clearance", "Threaded", "Seat"});
    participants->setCellWidget(row, 4, role);
    auto* shared = item(*participants, row, 5, {});
    shared->setFlags(shared->flags() | Qt::ItemIsUserCheckable);
    shared->setCheckState(Qt::Unchecked);
    shared->setToolTip("Explicitly change the shared Part definition used by "
                       "every occurrence.");
    QObject::connect(
        recipes, &QComboBox::currentIndexChanged, &owner, [this, row, recipes, role, part] {
          if (!loading && current()) {
            const auto* recipe =
                part->find_hole_feature(FeatureId(recipes->currentData().toString().toStdString()));
            if (recipe)
              role->setCurrentIndex(
                  recipe->thread()                                                  ? 1
                  : std::holds_alternative<std::monostate>(recipe->manufacturing()) ? 0
                                                                                    : 2);
            invalidate();
          }
        });
    for (auto* input : {state, role})
      QObject::connect(input, &QComboBox::currentIndexChanged, &owner, [this] { invalidate(); });
  }
  if (mode == Mode::Adaptive)
    for (const auto& ref : project.design_references())
      record->addItem(text(ref.name()), text(ref.id().value()));
  if (mode == Mode::AssemblyHole)
    for (const auto& feature : project.assembly_hole_features())
      record->addItem(text(feature.name()), text(feature.id().value()));
  positions->clear();
  positions->addItem("Master", text(master_position_id().value()));
  for (const auto& position : project.assembly_positions())
    positions->addItem(text(position.name), text(position.id.value()));
  select(*positions, window.session().active_position().value());
  views->clear();
  views->addItem("Create named design view", text(default_design_view_id().value()));
  for (const auto& view : project.assembly_design_views())
    views->addItem(text(view.name), text(view.id.value()));
  select(*views, window.session().active_design_view().value());
  draft_positions = project.assembly_positions();
  draft_views = project.assembly_design_views();
  loading = false;
  load_flexible();
  load_source();
  load_layout();
  load_view();
  update_dependencies();
  summary->setText("Choose the rooted occurrence and inputs, then Preview the "
                   "complete Assembly.");
}
void ShellAdvancedAssemblyTools::Impl::load_flexible() {
  loading = true;
  joint->clear();
  joint->addItem("Keep current pose", QString());
  coordinate->clear();
  const int i = occurrence->currentData().toInt();
  if (i >= 0 && std::size_t(i) < subassemblies.size()) {
    const auto& selected = subassemblies[std::size_t(i)];
    const auto* project = window.session().project();
    flexibility->setCurrentIndex(project->flexible_occurrence(selected.occurrence_path) ? 1 : 0);
    const auto* child = project->find_assembly_document(selected.assembly_document);
    for (const auto& value : child->joints())
      if (value.state() == AssemblyJointState::Active && !value.coordinate_slots().empty())
        joint->addItem(text(value.name()), text(value.id().value()));
  }
  loading = false;
  load_coordinate();
}
void ShellAdvancedAssemblyTools::Impl::load_coordinate() {
  if (loading)
    return;
  loading = true;
  const auto previous = coordinate->currentData();
  coordinate->clear();
  const int i = occurrence->currentData().toInt();
  const AssemblyJoint* selected_joint = nullptr;
  const AssemblyOccurrencePose* pose = nullptr;
  if (i >= 0 && std::size_t(i) < subassemblies.size()) {
    const auto& selected = subassemblies[std::size_t(i)];
    const auto* project = window.session().project();
    const auto* child = project->find_assembly_document(selected.assembly_document);
    selected_joint =
        child->find_joint(AssemblyJointId(joint->currentData().toString().toStdString()));
    pose = project->find_occurrence_pose(selected.occurrence_path);
  }
  if (selected_joint) {
    for (const auto& slot : selected_joint->coordinate_slots())
      coordinate->addItem(text(to_string(slot.role())), int(slot.role()));
    const int index = coordinate->findData(previous);
    if (index >= 0)
      coordinate->setCurrentIndex(index);
    const auto role = static_cast<AssemblyJointCoordinateRole>(coordinate->currentData().toInt());
    const auto* slot = selected_joint->find_coordinate_slot(role);
    if (slot) {
      auto number = slot->value();
      if (pose)
        for (const auto& c : pose->joints())
          if (c.joint == selected_joint->id() && c.role == role)
            number = c.value;
      value->setSuffix(slot->kind() == AssemblyJointCoordinateKind::Angular ? " deg" : " mm");
      value->setRange(slot->lower_limit() ? (slot->kind() == AssemblyJointCoordinateKind::Angular
                                                 ? slot->lower_limit()->degrees()
                                                 : slot->lower_limit()->millimeters())
                                          : -1e9,
                      slot->upper_limit() ? (slot->kind() == AssemblyJointCoordinateKind::Angular
                                                 ? slot->upper_limit()->degrees()
                                                 : slot->upper_limit()->millimeters())
                                          : 1e9);
      value->setValue(slot->kind() == AssemblyJointCoordinateKind::Angular ? number.degrees()
                                                                           : number.millimeters());
    }
  }
  value->setEnabled(selected_joint && flexibility->currentIndex() == 1);
  coordinate->setEnabled(selected_joint && flexibility->currentIndex() == 1);
  loading = false;
}
void ShellAdvancedAssemblyTools::Impl::load_source() {
  loading = true;
  source_choices.clear();
  geometry_editor->clear();
  source_state->clear();
  const auto* leaf = selected(source);
  if (leaf) {
    const auto* part =
        window.session().project()->find_part_document(leaf->referenced_part_document);
    states(*source_state, *part, leaf->selected_model_state);
    auto add = [&](ProjectDesignSource value, int kind, QString label) {
      geometry_editor->addItem(label, int(source_choices.size()));
      source_choices.push_back({std::move(value), kind, std::move(label)});
    };
    for (const auto& plane : part->datum_planes()) {
      auto spelling = make_assembly_reference_target_spelling(plane.id());
      if (spelling)
        add(ProjectDesignSemanticSource{spelling.value()}, 2, text(plane.name()) + " · plane");
    }
    for (const auto& plane : part->construction_planes()) {
      auto spelling = make_assembly_reference_target_spelling(plane.workplane_id());
      if (spelling)
        add(ProjectDesignSemanticSource{spelling.value()}, 2, text(plane.name()) + " · plane");
    }
    for (const auto& axis : part->datum_axes()) {
      auto spelling = make_assembly_reference_target_spelling(axis.id());
      if (spelling)
        add(ProjectDesignSemanticSource{spelling.value()}, 1, text(axis.name()) + " · axis");
    }
    for (const auto& line : part->construction_lines()) {
      auto spelling = make_assembly_reference_target_spelling(line.id());
      if (spelling)
        add(ProjectDesignSemanticSource{spelling.value()}, 1, text(line.name()) + " · line");
    }
    for (const auto& point : part->construction_points()) {
      auto spelling = make_assembly_reference_target_spelling(point.id());
      if (spelling)
        add(ProjectDesignSemanticSource{spelling.value()}, 0, text(point.name()) + " · point");
    }
    for (const auto& feature : part->features())
      if (feature.type() == FeatureType::AdditiveExtrude)
        for (const auto* role : {"top", "bottom", "left", "right", "front", "back"})
          add(ProjectDesignSemanticSource{feature.id().value() + "." + role}, 2,
              text(feature.name()) + " / " + role);
    for (const auto& path : part->path_curves())
      add(ProjectDesignPathSource{path.id()}, 3, text(path.name()) + " · native path");
    for (const auto& catalog : part->imported_topology_catalogs())
      for (const auto& entry : catalog.entries()) {
        std::optional<std::pair<ImportedTopologyCapability, int>> capability;
        if (entry.descriptor.geometry == ImportedGeometryClass::Plane)
          capability = std::pair{ImportedTopologyCapability::Plane, 2};
        else if (entry.descriptor.geometry == ImportedGeometryClass::Cylinder)
          capability = std::pair{ImportedTopologyCapability::Axis, 1};
        else if (entry.descriptor.geometry == ImportedGeometryClass::Line)
          capability = std::pair{ImportedTopologyCapability::Line, 1};
        else if (entry.descriptor.geometry == ImportedGeometryClass::Point)
          capability = std::pair{ImportedTopologyCapability::Point, 0};
        if (capability) {
          auto reference =
              ImportedTopologyReference::create(catalog.id(), entry.id, capability->first);
          if (reference) {
            auto spelling = make_imported_topology_target_spelling(reference.value());
            if (spelling)
              add(ProjectDesignSemanticSource{spelling.value()}, capability->second,
                  "Imported " + text(to_string(entry.descriptor.geometry)) + " / " +
                      text(entry.id.value()));
          }
        }
      }
  }
  loading = false;
  load_target();
}
void ShellAdvancedAssemblyTools::Impl::load_target() {
  loading = true;
  construction->clear();
  construction->addItem("Create new linked reference", QString());
  const auto* leaf = selected(target);
  const int i = geometry_editor->currentData().toInt();
  if (leaf && i >= 0 && std::size_t(i) < source_choices.size()) {
    const auto* part =
        window.session().project()->find_part_document(leaf->referenced_part_document);
    switch (source_choices[std::size_t(i)].kind) {
    case 0:
      for (const auto& point : part->construction_points())
        construction->addItem(text(point.name()), text(point.id().value()));
      break;
    case 1:
      for (const auto& line : part->construction_lines())
        construction->addItem(text(line.name()), text(line.id().value()));
      break;
    case 2:
      for (const auto& plane : part->construction_planes())
        construction->addItem(text(plane.name()), text(plane.id().value()));
      break;
    case 3:
      for (const auto& path : part->path_curves())
        construction->addItem(text(path.name()), text(path.id().value()));
      break;
    }
  }
  loading = false;
}
void ShellAdvancedAssemblyTools::Impl::load_layout() {
  loading = true;
  layout_state->clear();
  pattern->clear();
  pattern->addItem("Source Hole placements", QString());
  const int i = layout->currentData().toInt();
  if (i >= 0 && std::size_t(i) < layouts.size()) {
    const auto& choice = layouts[std::size_t(i)];
    const auto* part = window.session().project()->find_part_document(choice.occurrence.part);
    states(*layout_state, *part, primary_model_state_id());
    auto add = [&](const auto& value) {
      if (std::any_of(value.sources().begin(), value.sources().end(), [&](const auto& source) {
            const auto* id = std::get_if<FeatureId>(&source.source());
            return id && *id == choice.hole;
          }))
        pattern->addItem(text(value.name()), text(value.id().value()));
    };
    for (const auto& value : part->linear_pattern_features())
      add(value);
    for (const auto& value : part->circular_pattern_features())
      add(value);
  }
  loading = false;
}
void ShellAdvancedAssemblyTools::Impl::load_record() {
  const auto id = record->currentData().toString().toStdString();
  if (id.empty()) {
    if (mode == Mode::Adaptive) {
      link_id = ProjectDesignReferenceId(unique("reference."));
      name->setText("Adaptive reference");
      load_target();
    } else if (mode == Mode::AssemblyHole) {
      hole_id = FeatureId(unique("assembly.hole."));
      name->setText("Assembly hole");
    }
    return;
  }
  const auto& project = *window.session().project();
  if (mode == Mode::Adaptive) {
    const auto* ref = project.find_design_reference(ProjectDesignReferenceId(id));
    if (!ref)
      return;
    link_id = ref->id();
    loading = true;
    name->setText(text(ref->name()));
    for (std::size_t i = 0; i < leaves.size(); ++i) {
      if (design_occurrence(leaves[i]) == ref->source())
        source->setCurrentIndex(source->findData(int(i)));
      if (design_occurrence(leaves[i]) == ref->driver())
        target->setCurrentIndex(target->findData(int(i)));
    }
    loading = false;
    load_source();
    loading = true;
    select(*source_state, ref->source_state().value());
    for (std::size_t i = 0; i < source_choices.size(); ++i)
      if (source_choices[i].source == ref->source_geometry())
        geometry_editor->setCurrentIndex(geometry_editor->findData(int(i)));
    loading = false;
    load_target();
    const auto target_id = std::visit(
        [](const auto& value) -> std::string {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ProjectDerivedPathTarget>)
            return value.path.value();
          else
            return value.value();
        },
        ref->target());
    select(*construction, target_id);
  } else if (mode == Mode::AssemblyHole) {
    const auto found = std::find_if(
        project.assembly_hole_features().begin(), project.assembly_hole_features().end(),
        [&](const auto& feature) { return feature.id().value() == id; });
    if (found == project.assembly_hole_features().end())
      return;
    hole_id = found->id();
    loading = true;
    name->setText(text(found->name()));
    for (std::size_t i = 0; i < layouts.size(); ++i)
      if (layouts[i].occurrence == found->layout().source &&
          layouts[i].hole == found->layout().hole)
        layout->setCurrentIndex(layout->findData(int(i)));
    loading = false;
    load_layout();
    loading = true;
    select(*layout_state, found->layout().state.value());
    if (found->layout().pattern)
      select(*pattern, found->layout().pattern->value());
    for (int row = 0; row < participants->rowCount(); ++row) {
      participants->item(row, 0)->setCheckState(Qt::Unchecked);
      for (const auto& participant : found->participants())
        if (design_occurrence(leaves[std::size_t(row)]) == participant.occurrence) {
          participants->item(row, 0)->setCheckState(Qt::Checked);
          select(*static_cast<QComboBox*>(participants->cellWidget(row, 2)),
                 participant.recipe.id().value());
          select(*static_cast<QComboBox*>(participants->cellWidget(row, 3)),
                 participant.state.value());
          static_cast<QComboBox*>(participants->cellWidget(row, 4))
              ->setCurrentIndex(int(participant.role));
          participants->item(row, 5)->setCheckState(
              participant.definition_policy == AssemblyHoleDefinitionPolicy::AffectSharedDefinition
                  ? Qt::Checked
                  : Qt::Unchecked);
        }
    }
    loading = false;
  }
}
void ShellAdvancedAssemblyTools::Impl::load_view() {
  loading = true;
  const auto& project = *window.session().project();
  const auto chosen = AssemblyDesignViewId(views->currentData().toString().toStdString());
  const auto* view = project.find_assembly_design_view(chosen);
  if (view && mode == Mode::Representations)
    name->setText(text(view->name));
  auto selected_view = project.evaluate_assembly_design_view(chosen);
  auto captured =
      selected_view
          ? capture_assembly_design_view(selected_view.value(),
                                         AssemblyDesignViewId("display.draft"), "Display draft")
          : Result<AssemblyDesignView>::failure(selected_view.error());
  display_components =
      captured ? captured.value().components : std::vector<AssemblyComponentDisplay>{};
  display_subassemblies =
      captured ? captured.value().subassemblies : std::vector<AssemblySubassemblyDisplay>{};
  display->setRowCount(int(display_components.size() + display_subassemblies.size()));
  auto add = [&](int row, bool visible, QString label) {
    auto* check = item(*display, row, 0, {});
    check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
    check->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
    item(*display, row, 1, std::move(label));
  };
  int row = 0;
  for (const auto& component : display_components) {
    const AssemblyDocument* assembly = &project.assembly();
    for (const auto& id : component.occurrence.path) {
      const auto* instance = assembly ? assembly->find_subassembly_instance(id) : nullptr;
      assembly = instance ? project.find_assembly_document(instance->referenced_assembly_document())
                          : nullptr;
    }
    const auto* instance =
        assembly ? assembly->find_component_instance(component.occurrence.component) : nullptr;
    add(row++, component.visible,
        path_label(&project, component.occurrence.path) + " / " +
            text(instance ? instance->name() : component.occurrence.component.value()));
  }
  for (const auto& subassembly : display_subassemblies)
    add(row++, subassembly.visible, path_label(&project, subassembly.path) + " (Assembly)");
  loading = false;
}
void ShellAdvancedAssemblyTools::Impl::update_dependencies() {
  if (!active)
    return;
  if (mode == Mode::Adaptive) {
    const auto* a = selected(source);
    const auto* b = selected(target);
    if (!a || !b)
      dependencies->setText("Select a source occurrence and one visible driving target context.");
    else {
      QStringList lines{"Source: " + occurrence_label(*a),
                        "Visible driver: " + occurrence_label(*b)};
      const auto& project = *window.session().project();
      std::set<std::string> reachable{a->referenced_part_document.value()}, shown;
      bool added = true;
      while (added) {
        added = false;
        for (const auto& ref : project.design_references())
          if (reachable.contains(ref.source().part.value()) &&
              shown.insert("reference/" + ref.id().value()).second) {
            lines.push_back(text(project.find_part_document(ref.source().part)->name()) + " → " +
                            text(project.find_part_document(ref.driver().part)->name()) + " · " +
                            text(ref.name()));
            added = reachable.insert(ref.driver().part.value()).second || added;
          }
        for (const auto& feature : project.assembly_hole_features())
          if (reachable.contains(feature.layout().source.part.value()))
            for (const auto& participant : feature.participants())
              if (participant.occurrence.part != feature.layout().source.part &&
                  shown.insert("hole/" + feature.id().value() + "/" + participant.id).second) {
                lines.push_back(
                    text(project.find_part_document(feature.layout().source.part)->name()) + " → " +
                    text(project.find_part_document(participant.occurrence.part)->name()) + " · " +
                    text(feature.name()));
                added = reachable.insert(participant.occurrence.part.value()).second || added;
              }
      }
      lines.push_back("One shared target definition; Preview checks the "
                      "complete downstream Assembly.");
      dependencies->setText(lines.join('\n'));
    }
  } else if (mode == Mode::Flexible)
    dependencies->setText("Internal component poses belong to this exact rooted occurrence. The "
                          "shared child definition stays unchanged.");
  else if (mode == Mode::AssemblyHole)
    dependencies->setText("One shared layout drives only checked participants. Preview checks "
                          "every recipe before one root Apply.");
  else if (mode == Mode::Representations)
    dependencies->setText("Positions: pose and joint settings. Design views: "
                          "display only. Model states: "
                          "manufacturing choices. Recall changes session "
                          "context without baking Master.");
  else
    dependencies->setText("Edit the Primary Part through this explicit rooted context. The "
                          "breadcrumb returns one checked edit to the root Assembly.");
}
Result<std::size_t> ShellAdvancedAssemblyTools::Impl::mutate(Project& project) {
  if (mode == Mode::Flexible) {
    const int i = occurrence->currentData().toInt();
    if (i < 0 || std::size_t(i) >= subassemblies.size())
      return Result<std::size_t>::failure(error("Choose an active rooted subassembly"));
    const auto& selected = subassemblies[std::size_t(i)];
    const auto* previous = project.find_occurrence_pose(selected.occurrence_path);
    auto pose = AssemblyOccurrencePose::create(
        selected.occurrence_path, selected.assembly_document,
        flexibility->currentIndex() == 1 ? AssemblyOccurrenceRigidity::Flexible
                                         : AssemblyOccurrenceRigidity::Rigid,
        flexibility->currentIndex() == 1 && previous ? previous->components()
                                                     : std::vector<AssemblyComponentPoseOverride>{},
        flexibility->currentIndex() == 1 && previous
            ? previous->joints()
            : std::vector<AssemblyOccurrenceJointCoordinate>{});
    if (!pose)
      return Result<std::size_t>::failure(pose.error());
    auto stored = project.set_occurrence_pose(pose.value());
    if (!stored)
      return stored;
    if (flexibility->currentIndex() == 0 || joint->currentData().toString().isEmpty())
      return stored;
    const auto* child = project.find_assembly_document(selected.assembly_document);
    const auto* selected_joint =
        child->find_joint(AssemblyJointId(joint->currentData().toString().toStdString()));
    if (!selected_joint)
      return Result<std::size_t>::failure(error("Local joint is unavailable"));
    const auto role = static_cast<AssemblyJointCoordinateRole>(coordinate->currentData().toInt());
    const auto* slot = selected_joint->find_coordinate_slot(role);
    if (!slot)
      return Result<std::size_t>::failure(error("Choose an explicit joint coordinate"));
    auto q = slot->kind() == AssemblyJointCoordinateKind::Angular
                 ? Quantity::angle_deg(value->value())
                 : Quantity::linear_displacement_mm(value->value());
    if (!q)
      return Result<std::size_t>::failure(q.error());
    return query_with_step_import_context(
        window.session(), [&](geometry::StepImportTopologyContext* imports) {
          geometry::AssemblyOccurrenceSolver solver(imports);
          auto result = solver.move(project, selected.occurrence_path,
                                    {selected_joint->id(), {{role, q.value()}}});
          if (!result)
            return Result<std::size_t>::failure(result.error());
          return solver.apply(project, result.value());
        });
  }
  if (mode == Mode::Adaptive) {
    const auto* a = selected(source);
    const auto* b = selected(target);
    const int i = geometry_editor->currentData().toInt();
    if (!a || !b || i < 0 || std::size_t(i) >= source_choices.size())
      return Result<std::size_t>::failure(
          error("Choose a source geometry and one target driving occurrence"));
    const auto& choice = source_choices[std::size_t(i)];
    auto target_id = construction->currentData().toString().toStdString();
    if (target_id.empty())
      target_id = link_id.value() + ".target";
    ProjectDesignTarget destination = ConstructionPointId(target_id);
    if (choice.kind == 1)
      destination = ConstructionLineId(target_id);
    else if (choice.kind == 2)
      destination = ConstructionPlaneId(target_id);
    else if (choice.kind == 3) {
      const auto* existing = project.find_design_reference(link_id);
      const auto* old =
          existing ? std::get_if<ProjectDerivedPathTarget>(&existing->target()) : nullptr;
      destination = ProjectDerivedPathTarget{
          PathCurveId(target_id), old ? old->sketch : Sketch3DId(link_id.value() + ".curve")};
    }
    auto ref = ProjectDesignReference::create(
        link_id, name->text().trimmed().toStdString(), design_occurrence(*a),
        ModelStateId(source_state->currentData().toString().toStdString()), choice.source,
        design_occurrence(*b), destination);
    if (!ref)
      return Result<std::size_t>::failure(ref.error());
    return query_with_step_import_context(
        window.session(), [&](geometry::StepImportTopologyContext* imports) {
          auto result = geometry::AdaptiveProjectEvaluator{imports}.bind(project, ref.value());
          if (!result)
            return Result<std::size_t>::failure(result.error());
          project = std::move(result.value().project);
          return Result<std::size_t>::success(1);
        });
  }
  if (mode == Mode::AssemblyHole) {
    const int i = layout->currentData().toInt();
    if (i < 0 || std::size_t(i) >= layouts.size())
      return Result<std::size_t>::failure(error("Select an authored source Hole layout"));
    std::vector<AssemblyHoleParticipant> affected;
    for (int row = 0; row < participants->rowCount(); ++row)
      if (participants->item(row, 0)->checkState() == Qt::Checked) {
        const auto& leaf = leaves[std::size_t(row)];
        const auto* part = project.find_part_document(leaf.referenced_part_document);
        const auto* recipe = part->find_hole_feature(
            FeatureId(static_cast<QComboBox*>(participants->cellWidget(row, 2))
                          ->currentData()
                          .toString()
                          .toStdString()));
        if (!recipe)
          return Result<std::size_t>::failure(
              error("Each checked participant requires an existing semantic "
                    "Hole recipe"));
        const auto type = static_cast<AssemblyHoleParticipantRole>(
            static_cast<QComboBox*>(participants->cellWidget(row, 4))->currentIndex());
        affected.push_back({"participant." + std::to_string(row), design_occurrence(leaf),
                            ModelStateId(static_cast<QComboBox*>(participants->cellWidget(row, 3))
                                             ->currentData()
                                             .toString()
                                             .toStdString()),
                            type,
                            participants->item(row, 5)->checkState() == Qt::Checked
                                ? AssemblyHoleDefinitionPolicy::AffectSharedDefinition
                                : AssemblyHoleDefinitionPolicy::SingleOccurrence,
                            *recipe});
      }
    const auto& choice = layouts[std::size_t(i)];
    const auto pattern_id = pattern->currentData().toString();
    auto feature = AssemblyHoleFeature::create(
        hole_id, name->text().trimmed().toStdString(),
        {choice.occurrence, ModelStateId(layout_state->currentData().toString().toStdString()),
         choice.hole,
         pattern_id.isEmpty() ? std::nullopt
                              : std::optional<FeatureId>(FeatureId(pattern_id.toStdString()))},
        std::move(affected));
    if (!feature)
      return Result<std::size_t>::failure(feature.error());
    return query_with_step_import_context(
        window.session(), [&](geometry::StepImportTopologyContext* imports) {
          auto result = geometry::AssemblyHoleCoordinator{imports}.bind(project, feature.value());
          if (!result)
            return Result<std::size_t>::failure(result.error());
          project = std::move(result.value().project);
          return Result<std::size_t>::success(1);
        });
  }
  if (mode == Mode::Representations) {
    const auto chosen = AssemblyDesignViewId(views->currentData().toString().toStdString());
    preview_view =
        chosen == default_design_view_id() ? AssemblyDesignViewId(unique("view.")) : chosen;
    auto captured = capture_assembly_design_view(
        *window.session().effective_project(), preview_view, name->text().trimmed().toStdString());
    if (!captured)
      return Result<std::size_t>::failure(captured.error());
    for (std::size_t row = 0; row < display_components.size(); ++row)
      for (auto& component : captured.value().components)
        if (component.occurrence == display_components[row].occurrence)
          component.visible = display->item(int(row), 0)->checkState() == Qt::Checked;
    for (std::size_t row = 0; row < display_subassemblies.size(); ++row)
      for (auto& subassembly : captured.value().subassemblies)
        if (subassembly.path == display_subassemblies[row].path)
          subassembly.visible =
              display->item(int(display_components.size() + row), 0)->checkState() == Qt::Checked;
    draft_positions = project.assembly_positions();
    draft_views = project.assembly_design_views();
    auto existing = std::find_if(draft_views.begin(), draft_views.end(),
                                 [&](const auto& value) { return value.id == preview_view; });
    if (existing == draft_views.end())
      draft_views.push_back(std::move(captured.value()));
    else
      *existing = std::move(captured.value());
    return project.replace_assembly_design_views(draft_views);
  }
  return Result<std::size_t>::failure(error("Choose a checked Assembly operation"));
}
Result<std::size_t> ShellAdvancedAssemblyTools::Impl::edit_part() {
  const auto* leaf = selected(context_part);
  if (!leaf)
    return Result<std::size_t>::failure(error("Choose an explicit rooted Part occurrence"));
  const auto selected = design_occurrence(*leaf);
  owner.cancel();
  return window.enter_project_part_context(selected)
             ? Result<std::size_t>::success(1)
             : Result<std::size_t>::failure(error("Part context could not be opened"));
}
Result<std::size_t> ShellAdvancedAssemblyTools::Impl::detach_record() {
  const auto id = record->currentData().toString().toStdString();
  if (!current() || id.empty())
    return Result<std::size_t>::failure(error("Select an existing reference or coordinated Hole"));
  auto detached = commit_project_with_step_import_context(
      window.session(), "Detach Assembly Coordination",
      [&](Project& project, geometry::StepImportTopologyContext* imports) {
        auto result =
            mode == Mode::Adaptive
                ? geometry::AdaptiveProjectEvaluator{imports}.detach(project,
                                                                     ProjectDesignReferenceId(id))
                : geometry::AssemblyHoleCoordinator{imports}.detach(project, FeatureId(id));
        if (!result)
          return Result<std::size_t>::failure(result.error());
        project = std::move(result.value());
        return Result<std::size_t>::success(1);
      });
  if (detached) {
    owner.cancel();
    window.refresh();
  } else {
    revision = window.session().model_revision();
    invalidate();
  }
  return detached;
}
} // namespace blcad::gui
