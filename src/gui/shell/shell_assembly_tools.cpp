#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace blcad::gui {
namespace {
Result<std::size_t> fail(std::string text) {
  return Result<std::size_t>::failure(Error::validation("gui.shell.assembly", std::move(text)));
}
QString title(ShellAssemblyTools::Command c) {
  using C = ShellAssemblyTools::Command;
  switch (c) {
  case C::InsertComponent:
    return QStringLiteral("Bauteil einsetzen");
  case C::InsertSubassembly:
    return QStringLiteral("Unterbaugruppe einsetzen");
  case C::Placement:
    return QStringLiteral("Platzieren");
  case C::Relationship:
    return QStringLiteral("Beziehung");
  case C::Joint:
    return QStringLiteral("Gelenk");
  case C::Motion:
    return QStringLiteral("Gelenk bewegen");
  case C::Solve:
    return QStringLiteral("Baugruppe lösen");
  case C::ValidateHierarchy:
    return QStringLiteral("Hierarchie prüfen");
  case C::CrossRelationship:
    return QStringLiteral("Hierarchie-Beziehung");
  case C::CrossJoint:
    return QStringLiteral("Hierarchie-Gelenk");
  case C::CrossSolve:
    return QStringLiteral("Hierarchie lösen");
  case C::CrossMotion:
    return QStringLiteral("Hierarchie bewegen");
  default:
    return {};
  }
}
bool same(const AssemblyConstraintTarget& a, const AssemblyConstraintTarget& b) {
  return a.component_instance() == b.component_instance() &&
         a.semantic_reference() == b.semantic_reference();
}
std::optional<geometry::AssemblyFrameTargetDescriptor>
world_frame(const geometry::AssemblyResolvedGeometricTarget& target) {
  const auto* frame = std::get_if<geometry::AssemblyFrameTargetDescriptor>(&target.descriptor);
  if (!frame)
    return {};
  geometry::AssemblyHierarchyTransformEvaluator e;
  return geometry::AssemblyFrameTargetDescriptor{
      e.evaluate_point(target.transforms_inner_to_outer, frame->origin),
      e.evaluate_vector(target.transforms_inner_to_outer, frame->x_axis),
      e.evaluate_vector(target.transforms_inner_to_outer, frame->y_axis),
      e.evaluate_vector(target.transforms_inner_to_outer, frame->z_axis)};
}
} // namespace
ShellAssemblyTools::ShellAssemblyTools(ShellWindow& window, GuiDocumentSession& session,
                                       OcctViewport& viewport, ShellRibbon& ribbon, int tab)
    : QObject(&window), window_(window), session_(session), viewport_(viewport),
      coordinator_(&session, &layer_) {
  auto* document = ribbon.add_group(tab, QStringLiteral("Baugruppe"));
  auto* create = new QAction(QStringLiteral("Neue Baugruppe"), this);
  create->setObjectName("blcad.action.new_project");
  connect(create, &QAction::triggered, this, [this] {
    bool ok = false;
    auto name =
        QInputDialog::getText(&window_, QStringLiteral("Neue Baugruppe"), QStringLiteral("Name"),
                              QLineEdit::Normal, QStringLiteral("Baugruppe"), &ok)
            .trimmed();
    if (ok && !name.isEmpty())
      (void)window_.new_project(name);
  });
  ribbon.add_button(document, create);
  import_action_ = new QAction(QStringLiteral("Definition laden"), this);
  import_action_->setObjectName("blcad.action.assembly_import");
  connect(import_action_, &QAction::triggered, this, [this] {
    auto path =
        QFileDialog::getOpenFileName(&window_, QStringLiteral("Bauteil oder Baugruppe laden"), {},
                                     QStringLiteral("BLCAD (*.blcad.json *.blcad.project.json)"));
    if (!path.isEmpty()) {
      auto result = import_definition(path);
      if (!result)
        window_.report_error(result.error());
    }
  });
  ribbon.add_small_button(document, import_action_);
  auto* tools = ribbon.add_group(tab, QStringLiteral("Anordnen und verbinden"));
  for (const auto& [command, id] : std::vector<std::pair<Command, const char*>>{
           {Command::InsertComponent, "assembly_insert_component"},
           {Command::InsertSubassembly, "assembly_insert_subassembly"},
           {Command::Placement, "assembly_placement"},
           {Command::Relationship, "assembly_relationship"},
           {Command::Joint, "assembly_joint"},
           {Command::Motion, "assembly_motion"},
           {Command::Solve, "assembly_solve"},
           {Command::ValidateHierarchy, "assembly_validate_hierarchy"},
           {Command::CrossRelationship, "assembly_cross_relationship"},
           {Command::CrossJoint, "assembly_cross_joint"},
           {Command::CrossSolve, "assembly_cross_solve"},
           {Command::CrossMotion, "assembly_cross_motion"}}) {
    auto* action = new QAction(title(command), this);
    action->setObjectName(QStringLiteral("blcad.action.") + id);
    connect(action, &QAction::triggered, this, [this, command] {
      auto result = begin(command);
      if (!result)
        window_.report_error(result.error());
    });
    ribbon.add_small_button(tools, action);
    actions_.emplace_back(command, action);
  }
  overlay_ = make_shell_manipulator_overlay(&viewport_, layer_);
  overlay_->setObjectName("blcad.assembly.overlay");
  viewport.installEventFilter(this);
  window.installEventFilter(this);
  dock_ = new QDockWidget(QStringLiteral("Baugruppe"), &window);
  dock_->setObjectName("blcad.shell.assembly_task");
  dock_->setFeatures(QDockWidget::DockWidgetMovable);
  auto* contents = new QWidget(dock_);
  auto* column = new QVBoxLayout(contents);
  auto* scroll = new QScrollArea(contents);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setMinimumWidth(400);
  scroll->setObjectName("blcad.assembly.inputs_scroll");
  auto* panel = new QWidget(scroll);
  auto* inputs = new QVBoxLayout(panel);
  summary_ = new QLabel(panel);
  summary_->setWordWrap(true);
  summary_->setObjectName("blcad.assembly.summary");
  inputs->addWidget(summary_);
  create_advanced_controls(panel);
  inputs->addWidget(advanced_controls_);
  placement_controls_ = new QWidget(panel);
  auto* placement_form = new QFormLayout(placement_controls_);
  definition_ = new QComboBox(panel);
  definition_->setObjectName("blcad.assembly.definition");
  placement_form->addRow(QStringLiteral("Definition"), definition_);
  occurrence_ = new QComboBox(panel);
  occurrence_->setObjectName("blcad.assembly.occurrence");
  placement_form->addRow(QStringLiteral("Vorkommen"), occurrence_);
  placement_drag_ = new QComboBox(panel);
  placement_drag_->addItems({QStringLiteral("Verschieben XYZ"), QStringLiteral("Drehen X"),
                             QStringLiteral("Drehen Y"), QStringLiteral("Drehen Z")});
  placement_form->addRow(QStringLiteral("Dynamischer Griff"), placement_drag_);
  connect(placement_drag_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && placement()) {
      publish_handles();
      refresh_presentation();
    }
  });
  name_ = new QLineEdit(panel);
  name_->setObjectName("blcad.assembly.name");
  placement_form->addRow(QStringLiteral("Name"), name_);
  for (std::size_t i = 0; i < 6; ++i) {
    auto* value = new QDoubleSpinBox(panel);
    value->setRange(-1e9, 1e9);
    value->setDecimals(6);
    value->setKeyboardTracking(false);
    value->setSuffix(i < 3 ? " mm" : " deg");
    placement_values_[i] = value;
    value->setObjectName(QStringLiteral("blcad.assembly.placement.%1").arg(i));
    placement_form->addRow((i < 3 ? QStringLiteral("Position ") : QStringLiteral("Rotation ")) +
                               QStringLiteral("XYZ").at(i % 3),
                           value);
    connect(value, &QDoubleSpinBox::valueChanged, this, [this] {
      if (!sync_ && placement())
        changed();
    });
  }
  grounding_ = new QCheckBox(QStringLiteral("Fixiert"), panel);
  visibility_ = new QCheckBox(QStringLiteral("Sichtbar"), panel);
  suppression_ = new QCheckBox(QStringLiteral("Unterdrückt"), panel);
  placement_form->addRow(grounding_);
  placement_form->addRow(visibility_);
  placement_form->addRow(suppression_);
  inputs->addWidget(placement_controls_);
  target_controls_ = new QWidget(panel);
  auto* target_form = new QFormLayout(target_controls_);
  relationship_type_ = new QComboBox(panel);
  relationship_type_->setObjectName("blcad.assembly.relationship_type");
  relationship_type_->addItems({"Mate", "Concentric", "Distance", "Insert", "Angle", "Coincident",
                                "Parallel", "Perpendicular"});
  target_form->addRow(QStringLiteral("Beziehung"), relationship_type_);
  joint_type_ = new QComboBox(panel);
  joint_type_->setObjectName("blcad.assembly.joint_type");
  joint_type_->addItems({"Revolute", "Prismatic", "Cylindrical", "Planar", "Spherical"});
  target_form->addRow(QStringLiteral("Gelenkart"), joint_type_);
  pick_ = new QComboBox(panel);
  pick_->addItems({QStringLiteral("Referenz A"), QStringLiteral("Referenz B")});
  target_form->addRow(QStringLiteral("Modellklick wählt"), pick_);
  first_ = new QComboBox(panel);
  second_ = new QComboBox(panel);
  first_->setObjectName("blcad.assembly.first");
  second_->setObjectName("blcad.assembly.second");
  target_form->addRow(QStringLiteral("Referenz A"), first_);
  target_form->addRow(QStringLiteral("Referenz B"), second_);
  slots_ = new QTableWidget(0, 4, panel);
  slots_->setObjectName("blcad.assembly.slots");
  slots_->setHorizontalHeaderLabels({QStringLiteral("Koordinate"), QStringLiteral("Wert"),
                                     QStringLiteral("Minimum"), QStringLiteral("Maximum")});
  slots_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  slots_->setMinimumHeight(135);
  slots_->setMaximumHeight(190);
  target_form->addRow(slots_);
  inputs->addWidget(target_controls_);
  motion_controls_ = new QWidget(panel);
  auto* motion_form = new QFormLayout(motion_controls_);
  joint_ = new QComboBox(panel);
  joint_->setObjectName("blcad.assembly.joint");
  coordinate_ = new QComboBox(panel);
  coordinate_->setObjectName("blcad.assembly.coordinate");
  motion_form->addRow(QStringLiteral("Gelenk"), joint_);
  motion_form->addRow(QStringLiteral("Koordinate"), coordinate_);
  slider_ = new QSlider(Qt::Horizontal, panel);
  slider_->setRange(0, 1000);
  motion_form->addRow(slider_);
  slider_label_ = new QLabel(panel);
  slider_label_->setWordWrap(true);
  motion_form->addRow(slider_label_);
  inputs->addWidget(motion_controls_);
  value_label_ = new QLabel(panel);
  value_ = new QDoubleSpinBox(panel);
  value_->setRange(-1e9, 1e9);
  value_->setDecimals(6);
  value_->setKeyboardTracking(false);
  value_->setObjectName("blcad.assembly.value");
  inputs->addWidget(value_label_);
  inputs->addWidget(value_);
  solve_group_ = new QListWidget(panel);
  solve_group_->setObjectName("blcad.assembly.solve_group");
  solve_group_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  solve_group_->setMaximumHeight(180);
  inputs->addWidget(solve_group_);
  drag_editor_ = new QLineEdit(panel);
  drag_editor_->setObjectName("blcad.assembly.drag_value");
  drag_editor_->setPlaceholderText(
      QStringLiteral("Während des Ziehens Maß tippen, Enter beendet den Zug"));
  inputs->addWidget(drag_editor_);
  for (auto* combo : {definition_, occurrence_, placement_drag_, relationship_type_, joint_type_,
                      first_, second_, pick_, joint_, coordinate_}) {
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(12);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(combo, &QComboBox::currentTextChanged, combo, &QWidget::setToolTip);
  }
  auto* hint = new QLabel(
      QStringLiteral(
          "Anwenden übernimmt eine gültige Vorschau als einen rückgängig machbaren "
          "Schritt. Escape setzt einen Zug zurück; erneut Escape bricht den Befehl ab. "
          "Bewegliche Gelenkkoordinaten benötigen Unter- und Obergrenzen. Freies Platzieren löst "
          "bestehende Beziehungen nicht automatisch."),
      panel);
  hint->setWordWrap(true);
  inputs->addWidget(hint);
  inputs->addStretch();
  scroll->setWidget(panel);
  column->addWidget(scroll, 1);
  auto* footer = new QHBoxLayout;
  apply_ = new QPushButton(QStringLiteral("Anwenden"), contents);
  apply_->setObjectName("blcad.assembly.apply");
  auto* cancel = new QPushButton(QStringLiteral("Abbrechen"), contents);
  cancel->setObjectName("blcad.assembly.cancel");
  footer->addWidget(apply_);
  footer->addWidget(cancel);
  column->addLayout(footer);
  dock_->setWidget(contents);
  window.addDockWidget(Qt::RightDockWidgetArea, dock_);
  dock_->hide();
  connect(apply_, &QPushButton::clicked, this, [this] {
    auto result = apply();
    if (!result)
      window_.report_error(result.error());
  });
  connect(cancel, &QPushButton::clicked, this, [this] { this->cancel(); });
  for (auto* field : {visibility_, suppression_, grounding_})
    connect(field, &QCheckBox::toggled, this, [this] {
      if (!sync_ && placement())
        changed();
    });
  connect(name_, &QLineEdit::editingFinished, this, [this] {
    if (!sync_ && active())
      changed();
  });
  connect(definition_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && placement())
      changed();
  });
  connect(occurrence_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && command_ == Command::Placement) {
      load_placement();
      changed();
    }
  });
  connect(relationship_type_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && (command_ == Command::Relationship || command_ == Command::CrossRelationship)) {
      update_second_choices();
      changed();
    }
  });
  connect(joint_type_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && (command_ == Command::Joint || command_ == Command::CrossJoint)) {
      load_slots();
      update_second_choices();
      changed();
    }
  });
  connect(first_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && active()) {
      update_second_choices();
      changed();
    }
  });
  connect(second_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && active())
      changed();
  });
  connect(pick_, &QComboBox::currentIndexChanged, this, [this] {
    if (active())
      update_pick_scope();
  });
  connect(slots_, &QTableWidget::cellChanged, this, [this] {
    if (!sync_ && (command_ == Command::Joint || command_ == Command::CrossJoint))
      changed();
  });
  connect(joint_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && (command_ == Command::Motion || command_ == Command::CrossMotion)) {
      if (command_ == Command::CrossMotion)
        load_cross_motion();
      else
        load_joint();
      changed();
    }
  });
  connect(coordinate_, &QComboBox::currentIndexChanged, this, [this] {
    if (!sync_ && command_ == Command::CrossMotion) {
      synchronize_cross_motion();
      changed();
      return;
    }
    if (!sync_ && command_ == Command::Motion) {
      auto& motion = coordinator_.motion();
      if (motion.active()) {
        (void)motion.select_coordinate(
            session_, static_cast<AssemblyJointCoordinateRole>(coordinate_->currentData().toInt()));
        synchronize_values();
      }
      changed();
    }
  });
  connect(value_, &QDoubleSpinBox::valueChanged, this, [this](double) {
    if (sync_ || !active())
      return;
    const auto old = command_ == Command::Motion && coordinator_.motion().active()
                         ? coordinator_.motion().coordinate_controls(session_)
                         : std::vector<GuiAssemblyJointCoordinateControl>{};
    changed();
    if (!valid_ && !old.empty()) {
      auto selected =
          std::find_if(old.begin(), old.end(), [](const auto& v) { return v.selected; });
      if (selected != old.end()) {
        sync_ = true;
        value_->setValue(selected->value);
        sync_ = false;
        changed();
      }
    }
  });
  connect(slider_, &QSlider::valueChanged, this, [this](int i) {
    if (!sync_ && (command_ == Command::Motion || command_ == Command::CrossMotion))
      value_->setValue(slider_lower_ + (slider_upper_ - slider_lower_) * i / 1000.0);
  });
  connect(solve_group_, &QListWidget::itemSelectionChanged, this, [this] {
    if (!sync_ && (command_ == Command::Solve || command_ == Command::CrossSolve))
      changed();
  });
  connect(drag_editor_, &QLineEdit::editingFinished, this, [this] {
    if (!layer_.active_handle_id().empty()) {
      auto result = set_drag_text(drag_editor_->text());
      if (!result)
        window_.report_error(result.error());
    }
  });
}
ShellAssemblyTools::~ShellAssemblyTools() {
  delete overlay_;
}
bool ShellAssemblyTools::placement() const noexcept {
  return command_ == Command::InsertComponent || command_ == Command::InsertSubassembly ||
         command_ == Command::Placement;
}
RigidTransform ShellAssemblyTools::transform() const {
  return {
      {placement_values_[0]->value(), placement_values_[1]->value(), placement_values_[2]->value()},
      {placement_values_[3]->value(), placement_values_[4]->value(),
       placement_values_[5]->value()}};
}
Result<std::size_t> ShellAssemblyTools::import_definition(const QString& path) {
  if (active() || session_.task().active() || !session_.project())
    return fail("Definition laden benötigt eine Baugruppe ohne aktiven Befehl");
  GuiDocumentSession source;
  auto opened = source.open_file(path.toStdString());
  if (!opened)
    return opened;
  Result<std::size_t> result = fail("Unbekannter Definitionstyp");
  if (source.part_document())
    result = GuiAssemblyWorkbench{}.add_part(session_, *source.part_document());
  else if (source.project()) {
    const Project imported = *source.project();
    if (imported.cross_hierarchy_constraint_count() || imported.cross_hierarchy_joint_count())
      return fail("Projekt mit hierarchieübergreifenden Beziehungen kann noch nicht als "
                  "Unterbaugruppe geladen werden");
    result = session_.commit_project_transaction(
        "Load Assembly definition", [imported](Project& project) -> Result<std::size_t> {
          std::size_t count = 0;
          for (const auto& part : imported.part_documents()) {
            if (const auto* existing = project.find_part_document(part.id())) {
              auto a = serialize_part_document_to_json(*existing),
                   b = serialize_part_document_to_json(part);
              if (!a || !b || a.value() != b.value())
                return fail("Bauteildefinition mit gleicher ID und anderem Inhalt vorhanden");
            } else {
              auto added = project.add_part_document(part);
              if (!added)
                return added;
              count += added.value();
            }
          }
          for (const auto& child : imported.child_assembly_documents()) {
            auto added = project.add_child_assembly_document(child);
            if (!added)
              return added;
            count += added.value();
          }
          auto added = project.add_child_assembly_document(imported.assembly());
          if (!added)
            return added;
          return Result<std::size_t>::success(count + added.value());
        });
  }
  window_.refresh();
  return result;
}
Result<std::size_t> ShellAssemblyTools::begin(Command command) {
  const auto* project = session_.project();
  if (active() || session_.task().active() || !project || !session_.derived_results_fresh() ||
      command == Command::None)
    return fail("Baugruppenbefehl benötigt ein aktuelles Projekt ohne aktiven Befehl");
  sync_ = true;
  definition_->clear();
  occurrence_->clear();
  first_->clear();
  second_->clear();
  joint_->clear();
  coordinate_->clear();
  solve_group_->clear();
  definition_->addItem(QStringLiteral("Bitte wählen"), QString());
  occurrence_->addItem(QStringLiteral("Bitte wählen"), QString());
  first_->addItem(QStringLiteral("Bitte wählen"), -1);
  second_->addItem(QStringLiteral("Zuerst Referenz A wählen"), -1);
  joint_->addItem(QStringLiteral("Bitte wählen"), QString());
  if (command == Command::InsertComponent)
    for (const auto& part : project->part_documents())
      definition_->addItem(QString::fromStdString(part.name()),
                           QString::fromStdString(part.id().value()));
  if (command == Command::InsertSubassembly)
    for (const auto& assembly : project->child_assembly_documents())
      definition_->addItem(QString::fromStdString(assembly.name()),
                           QString::fromStdString(assembly.id().value()));
  for (const auto& component : project->assembly().component_instances()) {
    occurrence_->addItem(QString::fromStdString(component.name()),
                         QStringLiteral("component:") +
                             QString::fromStdString(component.id().value()));
    auto* item = new QListWidgetItem(QString::fromStdString(component.name()), solve_group_);
    item->setData(Qt::UserRole, QString::fromStdString(component.id().value()));
    item->setSelected(component.suppression_state() == ComponentSuppressionState::Active);
  }
  for (const auto& sub : project->assembly().subassembly_instances())
    occurrence_->addItem(QString::fromStdString(sub.name()),
                         QStringLiteral("subassembly:") + QString::fromStdString(sub.id().value()));
  for (const auto& joint : project->assembly().joints())
    if (joint.state() == AssemblyJointState::Active && joint.type() != AssemblyJointType::Spherical)
      joint_->addItem(QString::fromStdString(joint.name()),
                      QString::fromStdString(joint.id().value()));
  targets_.clear();
  hierarchy_targets_.clear();
  if (command == Command::Relationship || command == Command::Joint) {
    auto targets = geometry::AssemblyInteractionGeometry::targets(*project);
    if (!targets) {
      sync_ = false;
      return Result<std::size_t>::failure(targets.error());
    }
    targets_ = std::move(targets.value());
    for (std::size_t i = 0; i < targets_.size(); ++i)
      first_->addItem(QString::fromStdString(targets_[i].label), static_cast<int>(i));
  }
  for (std::size_t n = 1;; ++n) {
    const auto suffix = std::to_string(n);
    component_id_ = ComponentInstanceId("component.gui." + suffix);
    subassembly_id_ = SubassemblyInstanceId("subassembly.gui." + suffix);
    relationship_id_ = AssemblyConstraintId("constraint.gui." + suffix);
    joint_id_ = AssemblyJointId("joint.gui." + suffix);
    if (!project->assembly().find_component_instance(component_id_) &&
        !project->assembly().find_subassembly_instance(subassembly_id_) &&
        !project->assembly().find_constraint(relationship_id_) &&
        !project->assembly().find_joint(joint_id_) &&
        !project->find_cross_hierarchy_constraint(relationship_id_) &&
        !project->find_cross_hierarchy_joint(joint_id_)) {
      name_->setText(title(command) + " " + QString::fromStdString(suffix));
      break;
    }
  }
  for (auto* value : placement_values_)
    value->setValue(0);
  grounding_->setEnabled(command != Command::InsertSubassembly);
  placement_drag_->setCurrentIndex(0);
  grounding_->setChecked(false);
  visibility_->setChecked(true);
  suppression_->setChecked(false);
  relationship_type_->setCurrentIndex(0);
  joint_type_->setCurrentIndex(0);
  pick_->setCurrentIndex(0);
  value_->setValue(0);
  command_ = command;
  auto* form = static_cast<QFormLayout*>(placement_controls_->layout());
  form->setRowVisible(definition_, command != Command::Placement);
  form->setRowVisible(occurrence_, command == Command::Placement);
  name_->setReadOnly(command == Command::Placement);
  placement_controls_->setVisible(placement());
  target_controls_->setVisible(command == Command::Relationship || command == Command::Joint ||
                               command == Command::CrossRelationship ||
                               command == Command::CrossJoint);
  motion_controls_->setVisible(command == Command::Motion || command == Command::CrossMotion);
  solve_group_->setVisible(command == Command::Solve || command == Command::CrossSolve);
  auto* target_form = static_cast<QFormLayout*>(target_controls_->layout());
  target_form->setRowVisible(relationship_type_, command == Command::Relationship ||
                                                     command == Command::CrossRelationship);
  target_form->setRowVisible(joint_type_,
                             command == Command::Joint || command == Command::CrossJoint);
  target_form->setRowVisible(slots_, command == Command::Joint || command == Command::CrossJoint);
  advanced_controls_->setVisible(command == Command::CrossRelationship ||
                                 command == Command::CrossJoint);
  if (advanced()) {
    auto initialized = begin_advanced();
    if (!initialized) {
      command_ = Command::None;
      sync_ = false;
      return initialized;
    }
  } else
    solve_group_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  drag_editor_->hide();
  dock_->setWindowTitle(title(command));
  sync_ = false;
  if (!session_.task().begin("assembly.interactive") ||
      !session_.task().begin_parameter_editing() || !session_.task().show_preview()) {
    command_ = Command::None;
    return fail("Baugruppenaufgabe konnte nicht gestartet werden");
  }
  previous_mask_ = viewport_.selection_filter_mask();
  previous_scope_ = viewport_.selection_semantic_scope();
  revision_ = session_.presentation_revision();
  source_json_ = serialize_project_to_json(*project).value();
  if (command == Command::Joint || command == Command::CrossJoint)
    load_slots();
  dock_->show();
  dock_->raise();
  window_.refresh_feature_selection_scene();
  changed();
  window_.refresh();
  return Result<std::size_t>::success(1);
}
const geometry::AssemblyInteractionTarget* ShellAssemblyTools::selected(QComboBox* combo) const {
  const int i = combo->currentData().toInt();
  return i >= 0 && static_cast<std::size_t>(i) < targets_.size() ? &targets_[i] : nullptr;
}
void ShellAssemblyTools::load_placement() {
  const auto key = occurrence_->currentData().toString();
  const auto* project = session_.project();
  if (!project)
    return;
  Result<std::size_t> begun = key.startsWith("component:")
                                  ? coordinator_.placement().begin_edit_component(
                                        session_, ComponentInstanceId(key.mid(10).toStdString()))
                                  : coordinator_.placement().begin_edit_subassembly(
                                        session_, SubassemblyInstanceId(key.mid(12).toStdString()));
  if (!begun)
    return;
  sync_ = true;
  const auto& p = coordinator_.placement();
  name_->setText(occurrence_->currentText());
  const auto t = p.transform();
  std::array<double, 6> values{t.translation_mm.x, t.translation_mm.y, t.translation_mm.z,
                               t.rotation_deg.x,   t.rotation_deg.y,   t.rotation_deg.z};
  for (std::size_t i = 0; i < 6; ++i)
    placement_values_[i]->setValue(values[i]);
  grounding_->setChecked(p.grounding() == ComponentGroundingState::Grounded);
  visibility_->setChecked(p.visibility() == ComponentVisibility::Visible);
  suppression_->setChecked(p.suppression() == ComponentSuppressionState::Suppressed);
  grounding_->setEnabled(p.kind() == GuiAssemblyPlacementKind::Component);
  sync_ = false;
}
void ShellAssemblyTools::load_slots() {
  const QSignalBlocker block(slots_);
  slots_->setRowCount(0);
  const auto type = static_cast<AssemblyJointType>(joint_type_->currentIndex());
  std::vector<AssemblyJointCoordinateRole> roles;
  if (type == AssemblyJointType::Prismatic || type == AssemblyJointType::Cylindrical)
    roles.push_back(AssemblyJointCoordinateRole::Translation);
  if (type == AssemblyJointType::Revolute || type == AssemblyJointType::Cylindrical)
    roles.push_back(AssemblyJointCoordinateRole::Rotation);
  if (type == AssemblyJointType::Planar)
    roles = {AssemblyJointCoordinateRole::TranslationU, AssemblyJointCoordinateRole::TranslationV,
             AssemblyJointCoordinateRole::RotationNormal};
  for (auto role : roles) {
    const int row = slots_->rowCount();
    slots_->insertRow(row);
    const bool angular = role == AssemblyJointCoordinateRole::Rotation ||
                         role == AssemblyJointCoordinateRole::RotationNormal;
    auto* label = new QTableWidgetItem(QString::fromStdString(std::string(to_string(role))) +
                                       (angular ? " (deg)" : " (mm)"));
    label->setFlags(label->flags() & ~Qt::ItemIsEditable);
    label->setData(Qt::UserRole, static_cast<int>(role));
    slots_->setItem(row, 0, label);
    slots_->setItem(row, 1, new QTableWidgetItem("0"));
    slots_->setItem(row, 2, new QTableWidgetItem(angular ? "-180" : "-100"));
    slots_->setItem(row, 3, new QTableWidgetItem(angular ? "180" : "100"));
  }
}
std::vector<AssemblyJointCoordinateSlot>
ShellAssemblyTools::coordinate_slots(Result<std::size_t>& status) const {
  std::vector<AssemblyJointCoordinateSlot> result;
  for (int row = 0; row < slots_->rowCount(); ++row) {
    const auto role =
        static_cast<AssemblyJointCoordinateRole>(slots_->item(row, 0)->data(Qt::UserRole).toInt());
    const bool angular = role == AssemblyJointCoordinateRole::Rotation ||
                         role == AssemblyJointCoordinateRole::RotationNormal;
    std::vector<std::optional<Quantity>> values;
    for (int column = 1; column < 4; ++column) {
      const auto text = slots_->item(row, column)->text().trimmed();
      if (column > 1 && text.isEmpty()) {
        values.push_back({});
        continue;
      }
      bool ok = false;
      double scalar = text.toDouble(&ok);
      if (!ok || !std::isfinite(scalar)) {
        status = fail("Gelenkkoordinaten benötigen gültige Zahlen");
        return {};
      }
      auto q = angular ? Quantity::angle_deg(scalar) : Quantity::linear_displacement_mm(scalar);
      if (!q) {
        status = Result<std::size_t>::failure(q.error());
        return {};
      }
      values.push_back(q.value());
    }
    auto slot = AssemblyJointCoordinateSlot::create(
        role, angular ? AssemblyJointCoordinateKind::Angular : AssemblyJointCoordinateKind::Linear,
        *values[0], values[1], values[2]);
    if (!slot) {
      status = Result<std::size_t>::failure(slot.error());
      return {};
    }
    result.push_back(std::move(slot.value()));
  }
  return result;
}
void ShellAssemblyTools::load_joint() {
  coordinator_.cancel();
  auto begun = coordinator_.motion().begin(
      session_, AssemblyJointId(joint_->currentData().toString().toStdString()));
  sync_ = true;
  coordinate_->clear();
  if (begun)
    for (const auto& c : coordinator_.motion().coordinate_controls(session_))
      coordinate_->addItem(
          QString::fromStdString(std::string(to_string(c.role))) +
              (c.kind == AssemblyJointCoordinateKind::Angular ? " (deg)" : " (mm)"),
          static_cast<int>(c.role));
  sync_ = false;
  synchronize_values();
}
void ShellAssemblyTools::synchronize_values() {
  sync_ = true;
  if (placement() && coordinator_.placement().active()) {
    const auto t = coordinator_.placement().transform();
    std::array<double, 6> values{t.translation_mm.x, t.translation_mm.y, t.translation_mm.z,
                                 t.rotation_deg.x,   t.rotation_deg.y,   t.rotation_deg.z};
    for (std::size_t i = 0; i < 6; ++i)
      placement_values_[i]->setValue(values[i]);
  }
  if (command_ == Command::CrossMotion && slider_upper_ > slider_lower_) {
    slider_->setValue(static_cast<int>(std::clamp(
        (value_->value() - slider_lower_) / (slider_upper_ - slider_lower_) * 1000, 0.0, 1000.0)));
  }
  if (command_ == Command::Motion && coordinator_.motion().active())
    for (const auto& c : coordinator_.motion().coordinate_controls(session_))
      if (c.selected) {
        value_->setValue(c.value);
        value_->setSuffix(c.kind == AssemblyJointCoordinateKind::Angular ? " deg" : " mm");
        slider_lower_ =
            c.lower_limit.value_or(c.kind == AssemblyJointCoordinateKind::Angular ? -180 : -100);
        slider_upper_ =
            c.upper_limit.value_or(c.kind == AssemblyJointCoordinateKind::Angular ? 180 : 100);
        slider_->setEnabled(slider_upper_ > slider_lower_);
        slider_->setValue(
            slider_upper_ > slider_lower_
                ? static_cast<int>(
                      std::clamp((c.value - slider_lower_) / (slider_upper_ - slider_lower_) * 1000,
                                 0.0, 1000.0))
                : 0);
        slider_label_->setText(
            QStringLiteral("Regler: %1 bis %2%3. Grenzen: %4 / %5")
                .arg(slider_lower_)
                .arg(slider_upper_)
                .arg(c.kind == AssemblyJointCoordinateKind::Angular ? " deg" : " mm",
                     c.lower_limit ? QString::number(*c.lower_limit) : QStringLiteral("offen"),
                     c.upper_limit ? QString::number(*c.upper_limit) : QStringLiteral("offen")));
      }
  sync_ = false;
}
void ShellAssemblyTools::update_second_choices() {
  if (advanced()) {
    update_advanced_second_choices();
    return;
  }
  if (command_ != Command::Relationship && command_ != Command::Joint)
    return;
  const auto* a = selected(first_);
  const int previous = second_->currentData().toInt();
  std::vector<AssemblyConstraintTarget> candidates;
  for (const auto& target : targets_)
    candidates.push_back(target.reference);
  std::vector<AssemblyConstraintTarget> compatible;
  if (a) {
    if (command_ == Command::Relationship) {
      (void)coordinator_.relationship().begin(
          session_, relationship_id_, name_->text().toStdString(),
          static_cast<AssemblyConstraintType>(relationship_type_->currentIndex()));
      if (coordinator_.relationship().select_first(session_, a->reference))
        compatible = coordinator_.relationship().compatible_second_targets(session_, candidates);
    } else {
      Result<std::size_t> status = Result<std::size_t>::success(0);
      auto coordinates = coordinate_slots(status);
      if (status) {
        (void)coordinator_.joint().begin(
            session_, joint_id_, name_->text().toStdString(),
            static_cast<AssemblyJointType>(joint_type_->currentIndex()), coordinates);
        if (coordinator_.joint().select_first(session_, a->reference))
          compatible = coordinator_.joint().compatible_second_targets(session_, candidates);
      }
    }
  }
  const QSignalBlocker block(second_);
  second_->clear();
  second_->addItem(a ? QStringLiteral("Passende Referenz B wählen")
                     : QStringLiteral("Zuerst Referenz A wählen"),
                   -1);
  for (std::size_t i = 0; i < targets_.size(); ++i)
    if (std::any_of(compatible.begin(), compatible.end(),
                    [&](const auto& candidate) { return same(candidate, targets_[i].reference); }))
      second_->addItem(QString::fromStdString(targets_[i].label), static_cast<int>(i));
  const int found = second_->findData(previous);
  if (found >= 0)
    second_->setCurrentIndex(found);
}
Result<std::size_t> ShellAssemblyTools::rebuild_controller() {
  coordinator_.cancel();
  solved_.reset();
  if (advanced())
    return Result<std::size_t>::success(1);
  if (placement()) {
    Result<std::size_t> begun = fail("Zuerst eine Definition oder ein Vorkommen wählen");
    const auto definition = definition_->currentData().toString().toStdString();
    const auto key = occurrence_->currentData().toString();
    if (command_ == Command::InsertComponent && !definition.empty())
      begun = coordinator_.placement().begin_insert_component(
          session_, component_id_, name_->text().toStdString(), DocumentId(definition));
    if (command_ == Command::InsertSubassembly && !definition.empty())
      begun = coordinator_.placement().begin_insert_subassembly(
          session_, subassembly_id_, name_->text().toStdString(), DocumentId(definition));
    if (command_ == Command::Placement && !key.isEmpty())
      begun = key.startsWith("component:")
                  ? coordinator_.placement().begin_edit_component(
                        session_, ComponentInstanceId(key.mid(10).toStdString()))
                  : coordinator_.placement().begin_edit_subassembly(
                        session_, SubassemblyInstanceId(key.mid(12).toStdString()));
    if (!begun)
      return begun;
    auto& p = coordinator_.placement();
    p.set_transform(transform());
    p.set_grounding(grounding_->isChecked() ? ComponentGroundingState::Grounded
                                            : ComponentGroundingState::Free);
    p.set_visibility(visibility_->isChecked() ? ComponentVisibility::Visible
                                              : ComponentVisibility::Hidden);
    p.set_suppression(suppression_->isChecked() ? ComponentSuppressionState::Suppressed
                                                : ComponentSuppressionState::Active);
  } else if (command_ == Command::Relationship || command_ == Command::Joint) {
    Result<std::size_t> begun = fail("Referenzen A und B wählen");
    const auto* a = selected(first_);
    const auto* b = selected(second_);
    if (command_ == Command::Relationship) {
      const auto type = static_cast<AssemblyConstraintType>(relationship_type_->currentIndex());
      std::optional<Quantity> distance, angle;
      if (type == AssemblyConstraintType::Distance) {
        auto q = Quantity::length_mm(value_->value());
        if (!q)
          return Result<std::size_t>::failure(q.error());
        distance = q.value();
      }
      if (type == AssemblyConstraintType::Angle) {
        auto q = Quantity::angle_deg(value_->value());
        if (!q)
          return Result<std::size_t>::failure(q.error());
        angle = q.value();
      }
      begun = coordinator_.relationship().begin(session_, relationship_id_,
                                                name_->text().toStdString(), type, distance, angle);
      if (!begun)
        return begun;
      if (!a || !b)
        return fail("Kompatible Referenzen A und B aus unterschiedlichen Komponenten wählen");
      begun = coordinator_.relationship().select_first(session_, a->reference);
      if (begun)
        begun = coordinator_.relationship().select_second(session_, b->reference);
    } else {
      Result<std::size_t> status = Result<std::size_t>::success(0);
      auto coordinates = coordinate_slots(status);
      if (!status)
        return status;
      begun = coordinator_.joint().begin(
          session_, joint_id_, name_->text().toStdString(),
          static_cast<AssemblyJointType>(joint_type_->currentIndex()), coordinates);
      if (!begun)
        return begun;
      if (!a || !b)
        return fail("Kompatible Gelenkreferenzen A und B wählen");
      begun = coordinator_.joint().select_first(session_, a->reference);
      if (begun)
        begun = coordinator_.joint().select_second(session_, b->reference);
    }
    if (!begun)
      return begun;
  } else if (command_ == Command::Motion) {
    auto begun = coordinator_.motion().begin(
        session_, AssemblyJointId(joint_->currentData().toString().toStdString()));
    if (!begun)
      return begun;
    begun = coordinator_.motion().select_coordinate(
        session_, static_cast<AssemblyJointCoordinateRole>(coordinate_->currentData().toInt()));
    if (!begun)
      return begun;
    begun = coordinator_.motion().set_value(session_, value_->value());
    if (!begun)
      return begun;
    const auto* joint = session_.project()->assembly().find_joint(
        AssemblyJointId(joint_->currentData().toString().toStdString()));
    auto resolved = geometry::AssemblyConstraintTargetResolver{}.resolve_geometric(
        *session_.project(), joint->target_a());
    if (!resolved)
      return Result<std::size_t>::failure(resolved.error());
    auto frame = world_frame(resolved.value());
    if (!frame)
      return fail("Gelenk benötigt einen aufgelösten orientierten Rahmen");
    coordinator_.set_motion_frame(*frame);
  }
  publish_handles();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellAssemblyTools::preview() {
  valid_ = false;
  apply_->setEnabled(false);
  Project candidate = *session_.project();
  QString status;
  if (advanced()) {
    auto checked = preview_advanced(candidate);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    status = checked.value();
    if (command_ == Command::ValidateHierarchy) {
      summary_->setText(status);
      viewport_.clear_feature_preview();
      return Result<std::size_t>::success(1);
    }
  } else if (placement()) {
    auto checked = coordinator_.placement().preview_project(session_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    candidate = std::move(checked.value());
    status = QStringLiteral("Platzierung gültig. Neue Position bleibt bis Anwenden eine Vorschau.");
  } else if (command_ == Command::Relationship) {
    auto checked = coordinator_.relationship().preview(session_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    auto added = candidate.assembly().add_constraint(checked.value().relationship);
    if (!added)
      return added;
    auto posed =
        geometry::AssemblySolveResultApplier{}.apply(candidate, checked.value().solved_pose);
    if (!posed)
      return posed;
    status = QStringLiteral("Beziehung gelöst. Rest-Freiheitsgrade: %1; RMS: %2")
                 .arg(checked.value().diagnostics.rank_summary.remaining_dof)
                 .arg(checked.value().solved_pose.residual_summary.final_rms, 0, 'g', 4);
  } else if (command_ == Command::Joint) {
    auto checked = coordinator_.joint().preview(session_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    auto added = candidate.assembly().add_joint(checked.value().joint);
    if (!added)
      return added;
    status =
        QStringLiteral("Gelenkrahmen kompatibel. Die Referenzrahmen bleiben im Modell sichtbar.");
  } else if (command_ == Command::Motion) {
    auto checked = coordinator_.motion().preview(session_);
    if (!checked)
      return Result<std::size_t>::failure(checked.error());
    auto moved =
        geometry::AssemblyJointMotionResultApplier{}.apply(candidate, checked.value().motion);
    if (!moved)
      return moved;
    status =
        QStringLiteral("Bewegung konvergiert. Gelenk-Freiheitsgrade: %1; ungetrieben: %2; RMS: %3")
            .arg(checked.value().hud.joint_dof)
            .arg(checked.value().hud.undriven_dof)
            .arg(checked.value().hud.final_rms, 0, 'g', 4);
  } else if (command_ == Command::Solve) {
    std::vector<ComponentInstanceId> group;
    for (auto* item : solve_group_->selectedItems())
      group.emplace_back(item->data(Qt::UserRole).toString().toStdString());
    auto inspected = GuiAssemblyWorkbench{}.inspect_root_group(session_, group);
    if (!inspected)
      return Result<std::size_t>::failure(inspected.error());
    auto solved = GuiAssemblyWorkbench{}.solve_root_group(session_, group);
    if (!solved)
      return Result<std::size_t>::failure(solved.error());
    if (!solved.value().converged())
      return fail("Ausgewählte Gruppe konnte nicht konvergent gelöst werden");
    auto posed = geometry::AssemblySolveResultApplier{}.apply(candidate, solved.value());
    if (!posed)
      return posed;
    solved_ = std::move(solved.value());
    status = QStringLiteral("Gruppe gelöst. Rest-Freiheitsgrade: %1; RMS: %2")
                 .arg(inspected.value().rank_summary.remaining_dof)
                 .arg(solved_->residual_summary.final_rms, 0, 'g', 4);
  }
  geometry::ViewportSceneBuilder builder;
  auto scene = builder.build_project(candidate);
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  auto source = builder.build_project(*session_.project());
  if (!source)
    return Result<std::size_t>::failure(source.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  std::set<std::string> changed_occurrences;
  if (placement()) {
    const auto key = occurrence_->currentData().toString();
    const bool component = command_ == Command::InsertComponent ||
                           (command_ == Command::Placement && key.startsWith("component:"));
    const std::string id = command_ == Command::InsertComponent ? component_id_.value()
                           : command_ == Command::InsertSubassembly
                               ? subassembly_id_.value()
                               : key.mid(component ? 10 : 12).toStdString();
    for (const auto* project : {session_.project(), &candidate}) {
      auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(*project);
      if (!leaves)
        return Result<std::size_t>::failure(leaves.error());
      for (const auto& leaf : leaves.value()) {
        const bool match =
            component
                ? leaf.subassembly_path.empty() && leaf.component_instance.value() == id
                : !leaf.subassembly_path.empty() && leaf.subassembly_path.front().value() == id;
        if (!match)
          continue;
        std::string identity = "component/" + leaf.assembly_document.value();
        for (const auto& part : leaf.subassembly_path)
          identity += "/" + part.value();
        changed_occurrences.insert(identity + "/" + leaf.component_instance.value());
      }
    }
  }
  for (auto& item : scene.value())
    if (!placement() || changed_occurrences.contains(item.semantic_id))
      shapes.push_back(std::move(item.shape));
  for (const auto& item : source.value())
    if (!placement() || changed_occurrences.contains(item.semantic_id))
      replaced.push_back(item.semantic_id);
  if (!shapes.empty() || !replaced.empty()) {
    auto published = viewport_.set_feature_previews(std::move(shapes), std::move(replaced));
    if (!published)
      return published;
  } else
    viewport_.clear_feature_preview();
  summary_->setText(status);
  window_.statusBar()->showMessage(status);
  valid_ = true;
  apply_->setEnabled(layer_.active_handle_id().empty());
  synchronize_values();
  refresh_presentation();
  return Result<std::size_t>::success(1);
}
void ShellAssemblyTools::publish_handles() {
  coordinator_.refresh_handles();
  if (placement() && coordinator_.placement().active()) {
    auto handles = coordinator_.placement().handles();
    const int mode = placement_drag_->currentIndex();
    std::erase_if(handles, [mode](const auto& handle) {
      if (mode == 0)
        return handle.id.starts_with(kAssemblyPlacementRotatePrefix);
      return handle.id !=
             std::string(kAssemblyPlacementRotatePrefix) + "." + std::string(1, "xyz"[mode - 1]);
    });
    (void)layer_.set_handles(std::move(handles));
  }
}
void ShellAssemblyTools::changed() {
  if (!active() || sync_)
    return;
  value_->setVisible(
      command_ == Command::Motion || command_ == Command::CrossMotion ||
      ((command_ == Command::Relationship || command_ == Command::CrossRelationship) &&
       (relationship_type_->currentIndex() == static_cast<int>(AssemblyConstraintType::Distance) ||
        relationship_type_->currentIndex() == static_cast<int>(AssemblyConstraintType::Angle))));
  value_label_->setVisible(value_->isVisible());
  value_label_->setText((command_ == Command::Motion || command_ == Command::CrossMotion)
                            ? QStringLiteral("Koordinatenwert")
                        : relationship_type_->currentIndex() ==
                                static_cast<int>(AssemblyConstraintType::Angle)
                            ? QStringLiteral("Winkel (deg)")
                            : QStringLiteral("Abstand (mm)"));
  if (command_ == Command::Relationship || command_ == Command::CrossRelationship) {
    const QSignalBlocker block(value_);
    value_->setSuffix(relationship_type_->currentIndex() ==
                              static_cast<int>(AssemblyConstraintType::Angle)
                          ? " deg"
                          : " mm");
  }
  auto built = rebuild_controller();
  auto result = built ? preview() : built;
  if (!result) {
    valid_ = false;
    apply_->setEnabled(false);
    viewport_.clear_feature_preview();
    summary_->setText(QString::fromStdString(result.error().message()));
    window_.statusBar()->showMessage(summary_->text());
  }
  update_pick_scope();
  refresh_presentation();
}
void ShellAssemblyTools::update_pick_scope() {
  std::vector<std::string> scope;
  if (command_ == Command::CrossRelationship || command_ == Command::CrossJoint) {
    auto* input = pick_->currentIndex() == 0 ? first_ : second_;
    for (int row = 1; row < input->count(); ++row)
      scope.push_back(
          hierarchy_targets_.at(input->itemData(row).toInt()).target.presentation.semantic_id);
    viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::AssemblyTarget));
  } else if (command_ == Command::Relationship || command_ == Command::Joint) {
    auto* input = pick_->currentIndex() == 0 ? first_ : second_;
    for (int row = 1; row < input->count(); ++row)
      scope.push_back(targets_.at(input->itemData(row).toInt()).presentation.semantic_id);
    viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::AssemblyTarget));
  } else if (command_ == Command::Placement) {
    for (const auto& c : session_.project()->assembly().component_instances())
      scope.push_back("component/" + session_.project()->assembly().id().value() + "/" +
                      c.id().value());
    viewport_.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Component));
  } else
    viewport_.set_selection_filter_mask(0);
  viewport_.set_selection_semantic_scope(std::move(scope));
  window_.refresh_feature_selection_scene();
}
bool ShellAssemblyTools::accept_selection(const GuiSelection& selection) {
  if (!active() || !layer_.active_handle_id().empty())
    return false;
  if (command_ == Command::Placement && selection.kind == GuiSelectionKind::Component) {
    for (const auto& c : session_.project()->assembly().component_instances())
      if (selection.semantic_id ==
          "component/" + session_.project()->assembly().id().value() + "/" + c.id().value()) {
        occurrence_->setCurrentIndex(occurrence_->findData(QStringLiteral("component:") +
                                                           QString::fromStdString(c.id().value())));
        return true;
      }
  }
  if (command_ == Command::CrossRelationship || command_ == Command::CrossJoint) {
    if (selection.kind != GuiSelectionKind::AssemblyTarget)
      return false;
    auto* input = pick_->currentIndex() == 0 ? first_ : second_;
    for (int row = 1; row < input->count(); ++row)
      if (hierarchy_targets_.at(input->itemData(row).toInt()).target.presentation.semantic_id ==
          selection.semantic_id) {
        input->setCurrentIndex(row);
        if (input == first_)
          pick_->setCurrentIndex(1);
        return true;
      }
    return false;
  }
  if ((command_ != Command::Relationship && command_ != Command::Joint) ||
      selection.kind != GuiSelectionKind::AssemblyTarget)
    return false;
  auto* input = pick_->currentIndex() == 0 ? first_ : second_;
  for (int row = 1; row < input->count(); ++row)
    if (targets_.at(input->itemData(row).toInt()).presentation.semantic_id ==
        selection.semantic_id) {
      input->setCurrentIndex(row);
      if (input == first_)
        pick_->setCurrentIndex(1);
      return true;
    }
  return false;
}
std::vector<geometry::ViewportSceneItem> ShellAssemblyTools::selection_items() const {
  std::vector<geometry::ViewportSceneItem> items;
  if (!active() && !session_.task().active() && session_.project() &&
      session_.derived_results_fresh() &&
      viewport_.selection_filter_mask() == selection_kind_bit(GuiSelectionKind::AssemblyTarget)) {
    auto targets = geometry::AssemblyInteractionGeometry::targets(*session_.project());
    if (targets)
      for (auto& target : targets.value())
        items.push_back(std::move(target.presentation));
    return items;
  }
  if (command_ == Command::CrossRelationship || command_ == Command::CrossJoint) {
    auto* input = pick_->currentIndex() == 0 ? first_ : second_;
    for (int row = 1; row < input->count(); ++row)
      items.push_back(hierarchy_targets_.at(input->itemData(row).toInt()).target.presentation);
    if (pick_->currentIndex() == 1)
      if (const auto* a = hierarchy_selected(first_))
        items.push_back(a->target.presentation);
  } else if (command_ == Command::Relationship || command_ == Command::Joint) {
    auto* input = pick_->currentIndex() == 0 ? first_ : second_;
    for (int row = 1; row < input->count(); ++row)
      items.push_back(targets_.at(input->itemData(row).toInt()).presentation);
    if (pick_->currentIndex() == 1)
      if (const auto* a = selected(first_))
        items.push_back(a->presentation);
  }
  return items;
}
Result<std::size_t> ShellAssemblyTools::candidate(const GuiViewportManipulatorCandidate& input) {
  if (!std::isfinite(input.scalar_value))
    return fail("Ungültiger Manipulatorwert");
  const auto previous = placement()
                            ? std::optional<RigidTransform>(coordinator_.placement().transform())
                            : std::nullopt;
  double value = value_->value();
  const bool accepted = placement() ? coordinator_.placement().apply_manipulator(input)
                                    : coordinator_.motion().apply_manipulator(session_, input);
  if (!accepted)
    return fail("Manipulator passt nicht zum aktiven Baugruppenbefehl");
  auto checked = preview();
  if (!checked) {
    if (previous)
      coordinator_.placement().set_transform(*previous);
    else
      (void)coordinator_.motion().set_value(session_, value);
    (void)preview();
    return checked;
  }
  synchronize_values();
  if (layer_.active_handle_id().empty())
    publish_handles();
  return checked;
}
Result<std::size_t> ShellAssemblyTools::set_drag_text(const QString& text) {
  if (layer_.active_handle_id().empty())
    return fail("Zuerst einen Manipulator ziehen");
  std::string value = text.trimmed().toStdString();
  if (std::none_of(value.begin(), value.end(), [](unsigned char c) { return std::isalpha(c); }))
    value += layer_.active_handle_id().starts_with(kAssemblyPlacementRotatePrefix) ||
                     (command_ == Command::Motion && value_->suffix().contains("deg"))
                 ? " deg"
                 : " mm";
  if (!layer_.set_numeric_input(value) || !layer_.candidate())
    return fail("Ungültige Maßeingabe");
  return candidate(*layer_.candidate());
}
void ShellAssemblyTools::restore_drag() {
  if (drag_transform_)
    coordinator_.placement().set_transform(*drag_transform_);
  if (drag_value_)
    (void)coordinator_.motion().set_value(session_, *drag_value_);
  drag_transform_.reset();
  drag_value_.reset();
  publish_handles();
  (void)preview();
  drag_editor_->hide();
}
void ShellAssemblyTools::refresh_presentation() {
  if (viewport_.width() > 0 && viewport_.height() > 0) {
    auto mapping = GuiViewportManipulatorMapping::create(
        [this](GuiSketchScreenPoint p) { return viewport_.screen_to_view_ray(p); },
        [this](Point3 p) { return viewport_.model_to_screen(p); });
    if (mapping)
      (void)layer_.set_mapping(std::move(mapping.value()));
  }
  overlay_->setGeometry(viewport_.rect());
  overlay_->setVisible(active() && valid_ && !layer_.handles().empty());
  if (overlay_->isVisible()) {
    overlay_->raise();
    overlay_->update();
  }
}
void ShellAssemblyTools::refresh() {
  const auto* project = session_.project();
  const bool idle =
      project && !active() && !session_.task().active() && session_.derived_results_fresh();
  import_action_->setEnabled(idle);
  for (const auto& [command, action] : actions_)
    action->setEnabled(
        idle &&
        (command == Command::ValidateHierarchy ? true
         : command == Command::CrossRelationship || command == Command::CrossJoint
             ? project->assembly().subassembly_instance_count() > 0

         : command == Command::CrossSolve ? project->cross_hierarchy_constraint_count() > 0
         : command == Command::CrossMotion
             ? std::any_of(project->cross_hierarchy_joints().begin(),
                           project->cross_hierarchy_joints().end(),
                           [](const auto& j) {
                             return j.state() == AssemblyJointState::Active &&
                                    j.type() != AssemblyJointType::Spherical;
                           })
         : command == Command::InsertComponent   ? !project->part_documents().empty()
         : command == Command::InsertSubassembly ? !project->child_assembly_documents().empty()
         : command == Command::Motion
             ? std::any_of(project->assembly().joints().begin(), project->assembly().joints().end(),
                           [](const auto& j) {
                             return j.state() == AssemblyJointState::Active &&
                                    j.type() != AssemblyJointType::Spherical;
                           })
         : command == Command::Placement
             ? project->assembly().component_instance_count() +
                       project->assembly().subassembly_instance_count() >
                   0
         : command == Command::Solve ? project->assembly().component_instance_count() > 0
                                     : project->assembly().component_instance_count() >= 2));
  if (active() && (!project || !session_.derived_results_fresh() ||
                   session_.task().command_id() != "assembly.interactive")) {
    cancel();
    return;
  }
  if (active() && revision_ != session_.presentation_revision()) {
    auto json = serialize_project_to_json(*project);
    if (!json || json.value() != source_json_) {
      cancel();
      window_.statusBar()->showMessage(
          QStringLiteral("Baugruppe geändert. Befehl erneut starten."));
      return;
    }
    revision_ = session_.presentation_revision();
  }
  refresh_presentation();
}
void ShellAssemblyTools::end(bool applied) {
  coordinator_.cancel();
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  layer_.cancel();
  drag_transform_.reset();
  drag_value_.reset();
  command_ = Command::None;
  valid_ = false;
  solved_.reset();
  targets_.clear();
  hierarchy_targets_.clear();
  cross_groups_.clear();
  cross_solved_.reset();
  cross_moved_.reset();
  viewport_.set_selection_semantic_scope(previous_scope_);
  viewport_.set_selection_filter_mask(previous_mask_);
  viewport_.clear_feature_preview();
  dock_->hide();
  overlay_->hide();
  if (session_.task().command_id() == "assembly.interactive") {
    if (applied)
      (void)session_.task().apply();
    else
      (void)session_.task().cancel();
  }
  window_.refresh_feature_selection_scene();
  window_.finish_context_task(applied);
  window_.refresh();
  window_.statusBar()->showMessage(applied ? QStringLiteral("Baugruppenbefehl angewendet")
                                           : QStringLiteral("Baugruppenbefehl abgebrochen"));
}
void ShellAssemblyTools::cancel() {
  if (active())
    end(false);
}
Result<std::size_t> ShellAssemblyTools::apply() {
  if (!active() || !session_.project() || session_.task().command_id() != "assembly.interactive" ||
      !layer_.active_handle_id().empty())
    return fail("Kein anwendbarer Baugruppenbefehl");
  auto source = serialize_project_to_json(*session_.project());
  if (!session_.derived_results_fresh() || !source || source.value() != source_json_)
    return fail("Baugruppe geändert. Befehl erneut starten.");
  auto checked = rebuild_controller();
  if (!checked)
    return checked;
  checked = preview();
  if (!checked)
    return checked;
  auto applied = advanced() ? apply_advanced()
                 : command_ == Command::Solve
                     ? GuiAssemblyWorkbench{}.apply_solve(session_, *solved_)
                     : coordinator_.apply();
  if (applied) {
    QAction* accepted_action = nullptr;
    for (const auto& [command, action] : actions_)
      if (command == command_)
        accepted_action = action;
    end(true);
    if (accepted_action)
      window_.remember_repeatable_command(*accepted_action);
  }
  return applied;
}
bool ShellAssemblyTools::eventFilter(QObject* watched, QEvent* event) {
  if (!active())
    return QObject::eventFilter(watched, event);
  if (event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      if (layer_.active_handle_id().empty())
        cancel();
      else {
        layer_.cancel();
        if (viewport_.mouseGrabber() == &viewport_)
          viewport_.release_interaction_mouse();
        restore_drag();
      }
      return true;
    }
    if (!layer_.active_handle_id().empty() && watched == &viewport_) {
      if (key->key() == Qt::Key_Backspace) {
        if (!drag_buffer_.empty())
          drag_buffer_.pop_back();
      } else if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        auto final = layer_.end_drag(last_pointer_);
        viewport_.release_interaction_mouse();
        if (final)
          (void)candidate(*final);
        drag_transform_.reset();
        drag_value_.reset();
        drag_editor_->hide();
        return true;
      } else if (!key->text().isEmpty())
        drag_buffer_ += key->text().toStdString();
      drag_editor_->setText(QString::fromStdString(drag_buffer_));
      if (!drag_buffer_.empty())
        (void)set_drag_text(QString::fromStdString(drag_buffer_));
      return true;
    }
  }
  if (watched == &viewport_) {
    if (event->type() == QEvent::Resize)
      refresh_presentation();
    if (event->type() == QEvent::MouseButtonPress) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && valid_) {
        refresh_presentation();
        last_pointer_ = {mouse->position().x(), mouse->position().y()};
        if (layer_.begin_drag(last_pointer_)) {
          if (placement())
            drag_transform_ = coordinator_.placement().transform();
          else
            drag_value_ = value_->value();
          drag_buffer_.clear();
          drag_editor_->clear();
          drag_editor_->show();
          viewport_.setFocus();
          viewport_.grab_interaction_mouse();
          apply_->setEnabled(false);
          return true;
        }
      }
    }
    if (event->type() == QEvent::MouseMove && !layer_.active_handle_id().empty()) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      last_pointer_ = {mouse->position().x(), mouse->position().y()};
      if (layer_.update_drag(last_pointer_) && layer_.candidate())
        (void)candidate(*layer_.candidate());
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && !layer_.active_handle_id().empty()) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      auto result = layer_.end_drag({mouse->position().x(), mouse->position().y()});
      viewport_.release_interaction_mouse();
      if (result) {
        (void)candidate(*result);
        drag_transform_.reset();
        drag_value_.reset();
      } else
        restore_drag();
      drag_editor_->hide();
      apply_->setEnabled(valid_);
      return true;
    }
  }
  if (event->type() == QEvent::WindowDeactivate && !layer_.active_handle_id().empty()) {
    layer_.cancel();
    if (viewport_.mouseGrabber() == &viewport_)
      viewport_.release_interaction_mouse();
    restore_drag();
  }
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
