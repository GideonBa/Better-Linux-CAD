#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QSlider>
#include <QTableWidget>
#include <algorithm>
#include <map>

namespace blcad::gui {
namespace {
Error invalid(std::string message) {
  return Error::validation("gui.shell.assembly.hierarchy", std::move(message));
}
double scalar(const Quantity& value) {
  return value.kind() == QuantityKind::AngleDeg ? value.degrees() : value.millimeters();
}
std::string path_text(const std::vector<SubassemblyInstanceId>& path) {
  std::string text = "Root";
  for (const auto& step : path)
    text += " / " + step.value();
  return text;
}
std::string path_identity(const std::vector<SubassemblyInstanceId>& path) {
  std::string text;
  for (const auto& step : path)
    text += std::to_string(step.value().size()) + ":" + step.value() + "/";
  return text;
}
} // namespace
bool ShellAssemblyTools::advanced() const noexcept {
  return command_ == Command::ValidateHierarchy || command_ == Command::CrossRelationship ||
         command_ == Command::CrossJoint || command_ == Command::CrossSolve ||
         command_ == Command::CrossMotion;
}
void ShellAssemblyTools::create_advanced_controls(QWidget* panel) {
  advanced_controls_ = new QWidget(panel);
  auto* form = new QFormLayout(advanced_controls_);
  advanced_record_ = new QComboBox(advanced_controls_);
  advanced_record_->setObjectName("blcad.assembly.cross_record");
  advanced_record_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  advanced_record_->setMinimumContentsLength(12);
  advanced_name_ = new QLineEdit(advanced_controls_);
  advanced_name_->setObjectName("blcad.assembly.cross_name");
  advanced_active_ = new QCheckBox(QStringLiteral("Aktiv"), advanced_controls_);
  advanced_active_->setObjectName("blcad.assembly.cross_active");
  form->addRow(QStringLiteral("Neu / Bearbeiten"), advanced_record_);
  form->addRow(QStringLiteral("Name"), advanced_name_);
  form->addRow(advanced_active_);
  connect(advanced_record_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && advanced()) {
      load_advanced_record();
      changed();
    }
  });
  connect(advanced_name_, &QLineEdit::editingFinished, this, [this] {
    if (!sync_ && advanced())
      changed();
  });
  connect(advanced_active_, &QCheckBox::toggled, this, [this] {
    if (!sync_ && advanced())
      changed();
  });
}
Result<std::size_t> ShellAssemblyTools::build_hierarchy_targets() {
  const auto& project = *session_.project();
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (!hierarchy)
    return Result<std::size_t>::failure(hierarchy.error());
  std::map<std::string, std::vector<geometry::AssemblyInteractionTarget>> local_targets;
  for (const auto& occurrence : hierarchy.value().occurrences()) {
    if (!occurrence.active_path || !occurrence.visible_path)
      continue;
    const auto* assembly = project.find_assembly_document(occurrence.assembly_document);
    if (!assembly)
      return Result<std::size_t>::failure(invalid("Baugruppendefinition fehlt"));
    auto found = local_targets.find(assembly->id().value());
    if (found == local_targets.end()) {
      auto local = Project::create(DocumentId("gui.hierarchy.targets"), "Targets", *assembly);
      if (!local)
        return Result<std::size_t>::failure(local.error());
      for (const auto& part : project.part_documents()) {
        auto added = local.value().add_part_document(part);
        if (!added)
          return added;
      }
      auto targets = geometry::AssemblyInteractionGeometry::targets(local.value());
      if (!targets)
        return Result<std::size_t>::failure(targets.error());
      found = local_targets.emplace(assembly->id().value(), std::move(targets.value())).first;
    }
    for (auto target : found->second) {
      auto endpoint = AssemblyHierarchyConstraintEndpoint::create(
          occurrence.occurrence_path, target.reference.component_instance(),
          target.reference.semantic_reference());
      if (!endpoint)
        return Result<std::size_t>::failure(endpoint.error());
      auto query = geometry::AssemblyHierarchyConstraintTarget::create(
          occurrence.occurrence_path, endpoint.value().component_instance(),
          endpoint.value().semantic_reference());
      if (!query)
        return Result<std::size_t>::failure(query.error());
      auto resolved = geometry::AssemblyHierarchyConstraintTargetResolver{}.resolve_geometric(
          project, query.value());
      if (!resolved)
        continue;
      target.resolved = std::move(resolved.value());
      for (const auto& transform : occurrence.parent_transforms_inner_to_outer) {
        for (const auto& [axis, angle] :
             std::array<std::pair<Vector3, double>, 3>{{{{1, 0, 0}, transform.rotation_deg.x},
                                                        {{0, 1, 0}, transform.rotation_deg.y},
                                                        {{0, 0, 1}, transform.rotation_deg.z}}}) {
          auto rotated =
              geometry::BodyTransformAdapter{}.rotate(target.presentation.shape, {}, axis, angle);
          if (!rotated)
            return Result<std::size_t>::failure(rotated.error());
          target.presentation.shape = std::move(rotated.value());
        }
        auto shifted = geometry::BodyTransformAdapter{}.translate(target.presentation.shape,
                                                                  transform.translation_mm);
        if (!shifted)
          return Result<std::size_t>::failure(shifted.error());
        target.presentation.shape = std::move(shifted.value());
      }
      target.anchor = geometry::AssemblyHierarchyTransformEvaluator{}.evaluate_point(
          occurrence.parent_transforms_inner_to_outer, target.anchor);
      target.presentation.semantic_id = "hierarchy-target/" +
                                        path_identity(occurrence.occurrence_path) +
                                        target.presentation.semantic_id;
      target.label = path_text(occurrence.occurrence_path) + " / " + target.label;
      hierarchy_targets_.push_back({std::move(endpoint.value()), std::move(target)});
    }
  }
  for (std::size_t i = 0; i < hierarchy_targets_.size(); ++i)
    first_->addItem(QString::fromStdString(hierarchy_targets_[i].target.label),
                    static_cast<int>(i));
  return Result<std::size_t>::success(hierarchy_targets_.size());
}
Result<std::size_t> ShellAssemblyTools::begin_advanced() {
  const auto& project = *session_.project();
  cross_solved_.reset();
  cross_moved_.reset();
  cross_groups_.clear();
  advanced_record_->clear();
  advanced_record_->addItem(QStringLiteral("Neu erstellen"), QString());
  advanced_active_->setChecked(true);
  advanced_name_->setText(name_->text());
  if (command_ == Command::CrossRelationship || command_ == Command::CrossJoint) {
    auto built = build_hierarchy_targets();
    if (!built)
      return built;
    if (command_ == Command::CrossRelationship)
      for (const auto& record : project.cross_hierarchy_constraints())
        advanced_record_->addItem(QString::fromStdString(record.name()),
                                  QString::fromStdString(record.id().value()));
    else
      for (const auto& record : project.cross_hierarchy_joints())
        advanced_record_->addItem(QString::fromStdString(record.name()),
                                  QString::fromStdString(record.id().value()));
  }
  if (command_ == Command::CrossSolve) {
    solve_group_->clear();
    solve_group_->setSelectionMode(QAbstractItemView::SingleSelection);
    solve_group_->setObjectName("blcad.assembly.solve_group");
    auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
    if (!graph)
      return Result<std::size_t>::failure(graph.error());
    cross_groups_ = graph.value().solve_groups();
    for (std::size_t i = 0; i < cross_groups_.size(); ++i) {
      QString label = QStringLiteral("Gruppe %1: ").arg(i + 1);
      for (const auto& authority : cross_groups_[i].authorities)
        label += QString::fromStdString(authority.assembly_document.value() + "/" +
                                        authority.component_instance.value()) +
                 " ";
      auto* item = new QListWidgetItem(label, solve_group_);
      item->setData(Qt::UserRole, static_cast<int>(i));
      item->setToolTip(label);
    }
  }
  if (command_ == Command::CrossMotion) {
    joint_->clear();
    joint_->addItem(QStringLiteral("Bitte wählen"), QString());
    for (const auto& joint : project.cross_hierarchy_joints())
      if (joint.state() == AssemblyJointState::Active &&
          joint.type() != AssemblyJointType::Spherical)
        joint_->addItem(QString::fromStdString(joint.name()),
                        QString::fromStdString(joint.id().value()));
  }
  return Result<std::size_t>::success(1);
}
const ShellAssemblyTools::HierarchyTarget*
ShellAssemblyTools::hierarchy_selected(QComboBox* input) const {
  const int i = input->currentData().toInt();
  return i >= 0 && static_cast<std::size_t>(i) < hierarchy_targets_.size() ? &hierarchy_targets_[i]
                                                                           : nullptr;
}
Result<AssemblyHierarchyConstraint> ShellAssemblyTools::cross_relationship() const {
  const auto* a = hierarchy_selected(first_);
  const auto* b = hierarchy_selected(second_);
  if (!a || !b)
    return Result<AssemblyHierarchyConstraint>::failure(
        invalid("Zwei kompatible Vorkommensreferenzen wählen"));
  const auto type = static_cast<AssemblyConstraintType>(relationship_type_->currentIndex());
  std::optional<Quantity> distance, angle;
  if (type == AssemblyConstraintType::Distance || type == AssemblyConstraintType::Angle) {
    auto q = type == AssemblyConstraintType::Distance ? Quantity::length_mm(value_->value())
                                                      : Quantity::angle_deg(value_->value());
    if (!q)
      return Result<AssemblyHierarchyConstraint>::failure(q.error());
    if (type == AssemblyConstraintType::Distance)
      distance = q.value();
    else
      angle = q.value();
  }
  const auto id = advanced_record_->currentData().toString();
  return AssemblyHierarchyConstraint::create(
      id.isEmpty() ? relationship_id_ : AssemblyConstraintId(id.toStdString()),
      advanced_name_->text().trimmed().toStdString(), type, a->endpoint, b->endpoint,
      advanced_active_->isChecked() ? AssemblyConstraintState::Active
                                    : AssemblyConstraintState::Inactive,
      distance, angle);
}
Result<AssemblyHierarchyJoint> ShellAssemblyTools::cross_joint() const {
  const auto* a = hierarchy_selected(first_);
  const auto* b = hierarchy_selected(second_);
  if (!a || !b)
    return Result<AssemblyHierarchyJoint>::failure(
        invalid("Zwei kompatible Gelenkreferenzen wählen"));
  const auto type = static_cast<AssemblyJointType>(joint_type_->currentIndex());
  auto compatible = geometry::AssemblyJointTargetCompatibilityResolver{}.resolve(
      type, a->target.resolved, b->target.resolved);
  if (!compatible)
    return Result<AssemblyHierarchyJoint>::failure(compatible.error());
  Result<std::size_t> status = Result<std::size_t>::success(0);
  auto coordinates = coordinate_slots(status);
  if (!status)
    return Result<AssemblyHierarchyJoint>::failure(status.error());
  const auto id = advanced_record_->currentData().toString();
  return AssemblyHierarchyJoint::create(
      id.isEmpty() ? joint_id_ : AssemblyJointId(id.toStdString()),
      advanced_name_->text().trimmed().toStdString(), type, a->endpoint, b->endpoint,
      advanced_active_->isChecked() ? AssemblyJointState::Active : AssemblyJointState::Inactive,
      std::move(coordinates));
}
void ShellAssemblyTools::update_advanced_second_choices() {
  if (command_ != Command::CrossRelationship && command_ != Command::CrossJoint)
    return;
  const auto* a = hierarchy_selected(first_);
  const int previous = second_->currentData().toInt();
  const QSignalBlocker block(second_);
  second_->clear();
  second_->addItem(QStringLiteral("Passende Referenz B wählen"), -1);
  if (!a)
    return;
  for (std::size_t i = 0; i < hierarchy_targets_.size(); ++i) {
    const auto& b = hierarchy_targets_[i];
    if (a->endpoint.occurrence_path() == b.endpoint.occurrence_path() &&
        a->endpoint.component_instance() == b.endpoint.component_instance())
      continue;
    bool compatible = false;
    if (command_ == Command::CrossRelationship) {
      const auto type = static_cast<AssemblyConstraintType>(relationship_type_->currentIndex());
      auto query = geometry::AssemblyHierarchyConstraintQuery::create(
          relationship_id_, type, a->endpoint, b.endpoint,
          type == AssemblyConstraintType::Distance ? std::optional(Quantity::length_mm(1).value())
                                                   : std::nullopt,
          type == AssemblyConstraintType::Angle ? std::optional(Quantity::angle_deg(0).value())
                                                : std::nullopt);
      compatible = query && geometry::AssemblyHierarchyConstraintEquationBuilder{}.build(
                                *session_.project(), query.value());
    } else
      compatible = static_cast<bool>(geometry::AssemblyJointTargetCompatibilityResolver{}.resolve(
          static_cast<AssemblyJointType>(joint_type_->currentIndex()), a->target.resolved,
          b.target.resolved));
    if (compatible)
      second_->addItem(QString::fromStdString(b.target.label), static_cast<int>(i));
  }
  const int row = second_->findData(previous);
  if (row >= 0)
    second_->setCurrentIndex(row);
}
void ShellAssemblyTools::load_advanced_record() {
  const auto id = advanced_record_->currentData().toString().toStdString();
  sync_ = true;
  const auto choose = [this](QComboBox* input,
                             const AssemblyHierarchyConstraintEndpoint& endpoint) {
    auto found = std::find_if(hierarchy_targets_.begin(), hierarchy_targets_.end(),
                              [&](const auto& target) { return target.endpoint == endpoint; });
    const int data = found == hierarchy_targets_.end()
                         ? -1
                         : static_cast<int>(found - hierarchy_targets_.begin());
    input->setCurrentIndex(std::max(0, input->findData(data)));
  };
  if (id.empty()) {
    advanced_name_->setText(name_->text());
    advanced_active_->setChecked(true);
    first_->setCurrentIndex(0);
    update_advanced_second_choices();
    value_->setValue(0);
    if (command_ == Command::CrossJoint)
      load_slots();
  } else if (command_ == Command::CrossRelationship) {
    const auto* record =
        session_.project()->find_cross_hierarchy_constraint(AssemblyConstraintId(id));
    if (record) {
      advanced_name_->setText(QString::fromStdString(record->name()));
      advanced_active_->setChecked(record->state() == AssemblyConstraintState::Active);
      relationship_type_->setCurrentIndex(static_cast<int>(record->type()));
      value_->setValue(record->distance() ? scalar(*record->distance())
                       : record->angle()  ? scalar(*record->angle())
                                          : 0);
      choose(first_, record->target_a());
      update_advanced_second_choices();
      choose(second_, record->target_b());
    }
  } else if (command_ == Command::CrossJoint) {
    const auto* record = session_.project()->find_cross_hierarchy_joint(AssemblyJointId(id));
    if (record) {
      advanced_name_->setText(QString::fromStdString(record->name()));
      advanced_active_->setChecked(record->state() == AssemblyJointState::Active);
      joint_type_->setCurrentIndex(static_cast<int>(record->type()));
      load_slots();
      for (int row = 0; row < slots_->rowCount(); ++row) {
        const auto* slot = record->find_coordinate_slot(static_cast<AssemblyJointCoordinateRole>(
            slots_->item(row, 0)->data(Qt::UserRole).toInt()));
        if (!slot)
          continue;
        slots_->item(row, 1)->setText(QString::number(scalar(slot->value()), 'g', 17));
        slots_->item(row, 2)->setText(slot->lower_limit()
                                          ? QString::number(scalar(*slot->lower_limit()), 'g', 17)
                                          : QString());
        slots_->item(row, 3)->setText(slot->upper_limit()
                                          ? QString::number(scalar(*slot->upper_limit()), 'g', 17)
                                          : QString());
      }
      choose(first_, record->target_a());
      update_advanced_second_choices();
      choose(second_, record->target_b());
    }
  }
  sync_ = false;
}
void ShellAssemblyTools::load_cross_motion() {
  sync_ = true;
  coordinate_->clear();
  const auto* joint = session_.project()->find_cross_hierarchy_joint(
      AssemblyJointId(joint_->currentData().toString().toStdString()));
  if (joint)
    for (const auto& slot : joint->coordinate_slots())
      coordinate_->addItem(
          QString::fromStdString(std::string(to_string(slot.role()))) +
              (slot.kind() == AssemblyJointCoordinateKind::Angular ? " (deg)" : " (mm)"),
          static_cast<int>(slot.role()));
  sync_ = false;
  synchronize_cross_motion();
}
void ShellAssemblyTools::synchronize_cross_motion() {
  const auto* joint = session_.project()->find_cross_hierarchy_joint(
      AssemblyJointId(joint_->currentData().toString().toStdString()));
  if (!joint)
    return;
  const auto* slot = joint->find_coordinate_slot(
      static_cast<AssemblyJointCoordinateRole>(coordinate_->currentData().toInt()));
  if (!slot)
    return;
  sync_ = true;
  value_->setValue(scalar(slot->value()));
  value_->setSuffix(slot->kind() == AssemblyJointCoordinateKind::Angular ? " deg" : " mm");
  slider_lower_ = slot->lower_limit() ? scalar(*slot->lower_limit()) : -100;
  slider_upper_ = slot->upper_limit() ? scalar(*slot->upper_limit()) : 100;
  slider_->setEnabled(slider_lower_ < slider_upper_);
  slider_->setValue(
      slider_lower_ < slider_upper_
          ? static_cast<int>(1000 * std::clamp((scalar(slot->value()) - slider_lower_) /
                                                   (slider_upper_ - slider_lower_),
                                               0.0, 1.0))
          : 0);
  slider_label_->setText(
      QStringLiteral("Regler: %1 bis %2%3; offene Grenzen erlauben direkte Werteingabe.")
          .arg(slider_lower_)
          .arg(slider_upper_)
          .arg(value_->suffix()));
  sync_ = false;
}
Result<QString> ShellAssemblyTools::preview_advanced(Project& candidate) {
  cross_solved_.reset();
  cross_moved_.reset();
  auto structure = candidate.validate_assembly_structure();
  if (!structure)
    return Result<QString>::failure(structure.error());
  if (command_ == Command::ValidateHierarchy) {
    auto hierarchy = AssemblyHierarchyTraversal::build(candidate);
    auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(candidate);
    if (!hierarchy)
      return Result<QString>::failure(hierarchy.error());
    if (!leaves)
      return Result<QString>::failure(leaves.error());
    QString text =
        QStringLiteral(
            "Hierarchie gültig: %1 Baugruppenvorkommen, %2 sichtbare aktive Bauteilvorkommen.\n")
            .arg(hierarchy.value().occurrence_count())
            .arg(leaves.value().size());
    for (const auto& occurrence : hierarchy.value().occurrences())
      text += QString::fromStdString(path_text(occurrence.occurrence_path) + " → " +
                                     occurrence.assembly_document.value()) +
              (occurrence.active_path ? QString() : QStringLiteral(" [unterdrückt]")) +
              (occurrence.visible_path ? QString() : QStringLiteral(" [ausgeblendet]")) + "\n";
    return Result<QString>::success(text);
  }
  if (command_ == Command::CrossRelationship) {
    auto record = cross_relationship();
    if (!record)
      return Result<QString>::failure(record.error());
    auto query = geometry::AssemblyHierarchyConstraintQuery::create(record.value());
    if (!query)
      return Result<QString>::failure(query.error());
    auto equations =
        geometry::AssemblyHierarchyConstraintEquationBuilder{}.build(candidate, query.value());
    if (!equations)
      return Result<QString>::failure(equations.error());
    if (auto* existing = candidate.find_cross_hierarchy_constraint(record.value().id()))
      *existing = record.value();
    else {
      auto added = candidate.add_cross_hierarchy_constraint(record.value());
      if (!added)
        return Result<QString>::failure(added.error());
    }
  } else if (command_ == Command::CrossJoint) {
    auto record = cross_joint();
    if (!record)
      return Result<QString>::failure(record.error());
    if (auto* existing = candidate.find_cross_hierarchy_joint(record.value().id()))
      *existing = record.value();
    else {
      auto added = candidate.add_cross_hierarchy_joint(record.value());
      if (!added)
        return Result<QString>::failure(added.error());
    }
  } else if (command_ == Command::CrossSolve) {
    const auto selected = solve_group_->selectedItems();
    if (selected.size() != 1)
      return Result<QString>::failure(
          invalid("Eine vollständige verbundene Hierarchiegruppe wählen"));
    const auto& group = cross_groups_.at(selected.front()->data(Qt::UserRole).toInt());
    auto diagnostics =
        geometry::AssemblyCrossHierarchySolveDiagnosticsAnalyzer{}.analyze(candidate, group);
    if (!diagnostics)
      return Result<QString>::failure(diagnostics.error());
    auto solved = geometry::AssemblyCrossHierarchyConstraintSolver{}.solve(candidate, group);
    if (!solved)
      return Result<QString>::failure(solved.error());
    if (!solved.value().converged())
      return Result<QString>::failure(invalid("Hierarchiegruppe konvergiert nicht"));
    auto applied =
        geometry::AssemblyCrossHierarchySolveResultApplier{}.apply(candidate, solved.value());
    if (!applied)
      return Result<QString>::failure(applied.error());
    cross_solved_ = std::move(solved.value());
    return Result<QString>::success(
        QStringLiteral("Hierarchiegruppe gelöst. Rest-Freiheitsgrade: %1; RMS: %2. Wiederholte "
                       "Definitionen teilen ihre interne Platzierung.")
            .arg(diagnostics.value().rank_summary.remaining_dof)
            .arg(cross_solved_->residual_summary.final_rms, 0, 'g', 4));
  } else if (command_ == Command::CrossMotion) {
    const auto* joint = candidate.find_cross_hierarchy_joint(
        AssemblyJointId(joint_->currentData().toString().toStdString()));
    if (!joint)
      return Result<QString>::failure(invalid("Ein aktives Hierarchiegelenk wählen"));
    const auto role = static_cast<AssemblyJointCoordinateRole>(coordinate_->currentData().toInt());
    const auto* slot = joint->find_coordinate_slot(role);
    if (!slot)
      return Result<QString>::failure(invalid("Eine bewegliche Koordinate wählen"));
    auto q = slot->kind() == AssemblyJointCoordinateKind::Angular
                 ? Quantity::angle_deg(value_->value())
                 : Quantity::linear_displacement_mm(value_->value());
    if (!q)
      return Result<QString>::failure(q.error());
    auto moved = geometry::AssemblyCrossHierarchyJointMotionSolver{}.move(
        candidate, geometry::AssemblyJointDrive{joint->id(), {{role, q.value()}}});
    if (!moved)
      return Result<QString>::failure(moved.error());
    if (!moved.value().converged())
      return Result<QString>::failure(invalid("Hierarchiebewegung konvergiert nicht"));
    auto applied =
        geometry::AssemblyCrossHierarchyJointMotionResultApplier{}.apply(candidate, moved.value());
    if (!applied)
      return Result<QString>::failure(applied.error());
    cross_moved_ = std::move(moved.value());
    return Result<QString>::success(
        QStringLiteral("Hierarchiebewegung konvergiert. RMS: %1. Nur die gewählte Koordinate wird "
                       "übernommen; Unterbaugruppen-Grenzen bleiben erhalten.")
            .arg(cross_moved_->residual_summary.final_rms, 0, 'g', 4));
  }
  structure = candidate.validate_assembly_structure();
  if (!structure)
    return Result<QString>::failure(structure.error());
  return Result<QString>::success(
      QStringLiteral("Vorkommensreferenzen und Eingaben gültig. Anwenden speichert die Beziehung; "
                     "Hierarchie lösen beziehungsweise bewegen berechnet die Platzierung."));
}
Result<std::size_t> ShellAssemblyTools::apply_advanced() {
  if (command_ == Command::CrossSolve && cross_solved_) {
    const auto result = *cross_solved_;
    return session_.commit_project_transaction(
        "Solve cross-hierarchy group", [result](Project& project) {
          return geometry::AssemblyCrossHierarchySolveResultApplier{}.apply(project, result);
        });
  }
  if (command_ == Command::CrossMotion && cross_moved_) {
    const auto result = *cross_moved_;
    return session_.commit_project_transaction(
        "Move cross-hierarchy joint", [result](Project& project) {
          return geometry::AssemblyCrossHierarchyJointMotionResultApplier{}.apply(project, result);
        });
  }
  if (command_ == Command::CrossRelationship) {
    auto record = cross_relationship();
    if (!record)
      return Result<std::size_t>::failure(record.error());
    return session_.commit_project_transaction(
        "Author cross-hierarchy relationship", [record = record.value()](Project& project) {
          if (auto* existing = project.find_cross_hierarchy_constraint(record.id())) {
            *existing = record;
            return project.validate_assembly_structure();
          }
          return project.add_cross_hierarchy_constraint(record);
        });
  }
  if (command_ == Command::CrossJoint) {
    auto record = cross_joint();
    if (!record)
      return Result<std::size_t>::failure(record.error());
    return session_.commit_project_transaction(
        "Author cross-hierarchy joint", [record = record.value()](Project& project) {
          if (auto* existing = project.find_cross_hierarchy_joint(record.id())) {
            *existing = record;
            return project.validate_assembly_structure();
          }
          return project.add_cross_hierarchy_joint(record);
        });
  }
  return Result<std::size_t>::failure(invalid("Hierarchieprüfung ist schreibgeschützt"));
}
} // namespace blcad::gui
