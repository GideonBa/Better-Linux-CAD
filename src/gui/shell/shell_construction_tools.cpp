#include "blcad/gui/shell/shell_construction_tools.hpp"

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "construction.create";
constexpr std::string_view edit_task_id = "construction.edit";
Error error(std::string message) {
  return Error::validation("gui.shell.construction", std::move(message));
}
template <typename T, typename Add>
Result<std::size_t> insert(PartDocument& part, Result<T> object, Add add) {
  if (!object)
    return Result<std::size_t>::failure(object.error());
  return (part.*add)(std::move(object.value()));
}
Vector3 vector(const std::array<QDoubleSpinBox*, 3>& controls) {
  return {controls[0]->value(), controls[1]->value(), controls[2]->value()};
}
void select_generated(QComboBox* combo, const FeatureId& feature, int element) {
  combo->setCurrentIndex(-1);
  for (int i = 0; i < combo->count(); ++i)
    if (combo->itemData(i).toString().toStdString() == feature.value() &&
        combo->itemData(i, Qt::UserRole + 1).toInt() == element)
      combo->setCurrentIndex(i);
}
} // namespace

ShellConstructionTools::ShellConstructionTools(ShellWindow& window, ShellRibbon& ribbon,
                                               int model_tab)
    : QObject(&window), window_(window) {
  auto* group = ribbon.add_group(model_tab, QStringLiteral("Construction"));
  const auto action = [&](const char* id, const char* label, Kind kind) {
    auto* result = new QAction(QString::fromUtf8(label), this);
    result->setObjectName(QStringLiteral("blcad.action.") + id);
    connect(result, &QAction::triggered, this, [this, kind] {
      auto started = begin(kind);
      if (!started)
        window_.report_error(started.error());
    });
    ribbon.add_small_button(group, result);
    return result;
  };
  construction_action_ = action("create_construction", "Construction Geometry", Kind::Point);
  workplane_action_ = action("create_workplane", "Workplane", Kind::OffsetPlane);

  dock_ = new QDockWidget(QStringLiteral("Construction Geometry"), &window);
  dock_->setObjectName("blcad.shell.construction_task");
  dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
  auto* panel = new QWidget(dock_);
  auto* column = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  scroll->setMinimumWidth(380);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* inputs = new QWidget(scroll);
  auto* layout = new QVBoxLayout(inputs);
  form_ = new QFormLayout;
  form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  kind_ = new QComboBox(inputs);
  kind_->setObjectName("blcad.construction.kind");
  kind_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  kind_->setMinimumContentsLength(12);
  kind_->addItems(
      {QStringLiteral("Point (XYZ)"), QStringLiteral("Line (Point and Direction)"),
       QStringLiteral("Line Through Two Points"), QStringLiteral("Axis (Point and Direction)"),
       QStringLiteral("Axis from Construction Line"),
       QStringLiteral("Plane (Origin and Directions)"), QStringLiteral("Offset Plane"),
       QStringLiteral("Plane Through Three Points"), QStringLiteral("Plane on Model Face"),
       QStringLiteral("Point at Model Edge Midpoint"), QStringLiteral("Point on Model Vertex"),
       QStringLiteral("Line Parallel to Line Through Point"),
       QStringLiteral("Line Parallel to Model Edge Through Point"),
       QStringLiteral("Plane Parallel to Plane Through Point")});
  form_->addRow(QStringLiteral("Type"), kind_);
  name_ = new QLineEdit(inputs);
  name_->setObjectName("blcad.construction.name");
  form_->addRow(QStringLiteral("Name"), name_);
  const auto triple = [&](const char* id, std::array<QDoubleSpinBox*, 3>& values, bool position) {
    auto* row = new QWidget(inputs);
    auto* row_layout = new QVBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
      auto* spin = new QDoubleSpinBox(row);
      spin->setObjectName(QStringLiteral("blcad.construction.") + id + "." + "xyz"[i]);
      spin->setDecimals(6);
      spin->setRange(-1e6, 1e6);
      spin->setPrefix(QString("xyz"[i]).toUpper() + ": ");
      if (position)
        spin->setSuffix(QStringLiteral(" mm"));
      spin->setKeyboardTracking(false);
      values[i] = spin;
      row_layout->addWidget(spin);
      connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { update_preview(); });
    }
    return row;
  };
  origin_row_ = triple("origin", origin_, true);
  direction_row_ = triple("direction", direction_, false);
  y_direction_row_ = triple("y_direction", y_direction_, false);
  direction_[0]->setValue(1);
  y_direction_[1]->setValue(1);
  form_->addRow(QStringLiteral("Origin"), origin_row_);
  form_->addRow(QStringLiteral("X Direction"), direction_row_);
  form_->addRow(QStringLiteral("Y Direction (Perpendicular)"), y_direction_row_);
  const auto combo = [&](const char* id, const QString& label) {
    auto* result = new QComboBox(inputs);
    result->setObjectName(QStringLiteral("blcad.construction.") + id);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    result->setMinimumContentsLength(12);
    form_->addRow(label, result);
    connect(result, &QComboBox::currentIndexChanged, this, [this] {
      update_inputs();
      update_preview();
    });
    return result;
  };
  plane_ = combo("plane", QStringLiteral("Reference Plane"));
  line_ = combo("line", QStringLiteral("Construction Line"));
  for (int i = 0; i < 3; ++i)
    points_[i] =
        combo(("point" + std::to_string(i + 1)).c_str(), QStringLiteral("Point %1").arg(i + 1));
  parameter_ = combo("parameter", QStringLiteral("Offset Parameter"));
  offset_ = new QDoubleSpinBox(inputs);
  offset_->setObjectName("blcad.construction.offset");
  offset_->setDecimals(6);
  offset_->setRange(0.000001, 1e6);
  offset_->setValue(10);
  offset_->setSuffix(QStringLiteral(" mm"));
  offset_->setKeyboardTracking(false);
  form_->addRow(QStringLiteral("Offset (+Normal Direction)"), offset_);
  face_ = combo("face", QStringLiteral("Model Face"));
  edge_ = combo("edge", QStringLiteral("Model Edge"));
  vertex_ = combo("vertex", QStringLiteral("Model Vertex"));
  pick_role_ = new QComboBox(inputs);
  pick_role_->setObjectName("blcad.construction.pick_role");
  form_->addRow(QStringLiteral("Viewport Click Selects"), pick_role_);
  connect(pick_role_, &QComboBox::currentIndexChanged, this, [this] { update_pick_scope(); });
  manipulation_ = new QComboBox(inputs);
  manipulation_->setObjectName("blcad.construction.manipulation");
  form_->addRow(QStringLiteral("Handles"), manipulation_);
  grip_numeric_ = new QLineEdit(inputs);
  grip_numeric_->setObjectName("blcad.construction.grip_numeric");
  grip_numeric_->setPlaceholderText(QStringLiteral("Drag a handle, then enter mm or deg"));
  grip_numeric_->setReadOnly(true);
  form_->addRow(QStringLiteral("Handle Value"), grip_numeric_);
  connect(manipulation_, &QComboBox::currentIndexChanged, this, [this] {
    cancel_drag();
    refresh_handles();
  });
  connect(grip_numeric_, &QLineEdit::textEdited, this, [this] { edit_grip_numeric(); });
  layout->addLayout(form_);
  summary_ = new QLabel(inputs);
  summary_->setObjectName("blcad.construction.summary");
  summary_->setWordWrap(true);
  layout->addWidget(summary_);
  layout->addStretch();
  scroll->setWidget(inputs);
  column->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  apply_ = new QPushButton(QStringLiteral("Apply"), panel);
  apply_->setObjectName("blcad.construction.apply");
  auto* cancel = new QPushButton(QStringLiteral("Cancel"), panel);
  cancel->setObjectName("blcad.construction.cancel");
  buttons->addWidget(apply_);
  buttons->addWidget(cancel);
  column->addLayout(buttons);
  dock_->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, dock_);
  dock_->hide();
  dock_->installEventFilter(this);
  window.installEventFilter(this);
  window.viewport().installEventFilter(this);
  overlay_ = make_shell_manipulator_overlay(&window.viewport(), layer_);
  overlay_->setObjectName("blcad.construction.manipulator_overlay");
  manipulation_->installEventFilter(this);
  grip_numeric_->installEventFilter(this);
  // Children added to the panel are explicit task controls, not discovered widgets.
  for (auto* control : {static_cast<QWidget*>(kind_), static_cast<QWidget*>(name_),
                        static_cast<QWidget*>(apply_), static_cast<QWidget*>(cancel)})
    control->installEventFilter(this);
  for (auto* combo_control : {plane_, line_, parameter_, face_, edge_, vertex_, points_[0],
                              points_[1], points_[2], pick_role_})
    combo_control->installEventFilter(this);
  for (const auto& controls : {origin_, direction_, y_direction_})
    for (auto* control : controls)
      control->installEventFilter(this);
  offset_->installEventFilter(this);
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
    cancel_drag();
    update_inputs();
    update_preview();
    if (active_)
      window_.refresh_feature_selection_scene();
  });
  connect(name_, &QLineEdit::textChanged, this, [this] { update_preview(); });
  connect(offset_, &QDoubleSpinBox::valueChanged, this, [this] { update_preview(); });
  connect(cancel, &QPushButton::clicked, this, [this] { this->cancel(); });
  connect(apply_, &QPushButton::clicked, this, [this] {
    auto result = apply();
    if (!result)
      window_.report_error(result.error());
  });
}

void ShellConstructionTools::fill_references() {
  const auto& part = *window_.session().part_document();
  for (auto* control :
       {plane_, line_, parameter_, face_, edge_, vertex_, points_[0], points_[1], points_[2]})
    control->clear();
  const auto add = [](QComboBox* control, const auto& object, const char* prefix = "") {
    control->addItem(QString::fromStdString(object.name()),
                     QString::fromStdString(object.id().value()));
    control->setItemData(control->count() - 1, QString::fromStdString(object.id().value()),
                         Qt::ToolTipRole);
    control->setItemData(control->count() - 1, QString::fromStdString(prefix + object.id().value()),
                         Qt::UserRole + 2);
  };
  for (const auto& plane : part.datum_planes())
    add(plane_, plane, "datum-plane/");
  for (const auto& plane : part.construction_planes())
    if (!editing_ || plane.id().value() != id_)
      add(plane_, plane, "construction-plane/");
  for (const auto& plane : part.derived_workplanes())
    if (!editing_ || plane.id().value() != id_)
      add(plane_, plane, "derived-workplane/");
  for (const auto& line : part.construction_lines())
    if (!editing_ || line.id().value() != id_)
      add(line_, line, "construction-line/");
  for (const auto& point : part.construction_points())
    for (auto* control : points_)
      add(control, point, "construction-point/");
  for (int i = 0; i < 3; ++i)
    points_[i]->setCurrentIndex(std::min(i, points_[i]->count() - 1));
  parameter_->addItem(QStringLiteral("New Parameter"), QString{});
  for (const auto& parameter : part.parameters())
    if (parameter.type() == ParameterType::Length)
      add(parameter_, parameter);
  selection_targets_.clear();
  auto targets = geometry::ViewportSceneBuilder{}.build_construction_sources(
      part, *window_.session().part_shape_cache());
  if (!targets) {
    window_.report_error(targets.error());
    return;
  }
  selection_targets_ = std::move(targets.value());
  for (const auto& target : selection_targets_)
    std::visit(
        [&](const auto& reference) {
          using T = std::decay_t<decltype(reference)>;
          QComboBox* control;
          int element;
          std::string token;
          if constexpr (std::is_same_v<T, SemanticFaceReference>) {
            control = face_;
            element = static_cast<int>(reference.face());
            token = to_string(reference.face());
          } else if constexpr (std::is_same_v<T, SemanticEdgeReference>) {
            control = edge_;
            element = static_cast<int>(reference.edge());
            token = to_string(reference.edge());
          } else {
            control = vertex_;
            element = static_cast<int>(reference.vertex());
            token = to_string(reference.vertex());
          }
          const auto* feature = part.find_feature(reference.source_feature());
          control->addItem(QString::fromStdString(feature->name() + " / " + token),
                           QString::fromStdString(feature->id().value()));
          const int row = control->count() - 1;
          control->setItemData(row, element, Qt::UserRole + 1);
          control->setItemData(row, QString::fromStdString(target.item.semantic_id),
                               Qt::UserRole + 2);
          control->setItemData(row, QString::fromStdString(feature->id().value() + " / " + token),
                               Qt::ToolTipRole);
        },
        target.reference);
}

Result<std::size_t> ShellConstructionTools::begin(Kind kind) {
  auto& session = window_.session();
  if (!session.part_document() || !session.derived_results_fresh() || window_.active_sketch() ||
      session.task().active())
    return Result<std::size_t>::failure(
        error("Construction requires a current part without an active command"));
  updating_ = true;
  editing_ = false;
  dependencies_.clear();
  kind_->setEnabled(true);
  dock_->setWindowTitle(QStringLiteral("Create Construction"));
  fill_references();
  std::size_t number = 1;
  const auto& graph = session.part_document()->dependency_graph();
  do {
    id_ = "construction.shell." + std::to_string(number++);
  } while (graph.has_node(id_) || graph.has_node(id_ + ".offset") ||
           graph.has_node(id_ + ".relation"));
  relation_id_ = id_ + ".relation";
  name_->setText(QStringLiteral("Construction %1").arg(number - 1));
  kind_->setCurrentIndex(static_cast<int>(kind));
  for (auto* coordinate : origin_)
    coordinate->setValue(0);
  for (int i = 0; i < 3; ++i) {
    direction_[i]->setValue(i == 0 ? 1 : 0);
    y_direction_[i]->setValue(i == 1 ? 1 : 0);
  }
  offset_->setValue(10);
  updating_ = false;
  return open_task();
}

Result<std::size_t> ShellConstructionTools::begin_edit(std::string_view object_id) {
  auto& session = window_.session();
  const auto* part = session.part_document();
  if (!part || !session.derived_results_fresh() || window_.active_sketch() ||
      session.task().active())
    return Result<std::size_t>::failure(
        error("Editing requires a current part without an active command"));
  // Principal origin geometry stays fixed in the shell.
  if (object_id == "datum.axis.x" || object_id == "datum.axis.y" || object_id == "datum.axis.z")
    return Result<std::size_t>::failure(error("Origin geometry is fixed"));
  Kind kind;
  std::string name;
  std::optional<ConstructionRelation> relation;
  Vector3 origin{}, direction{1, 0, 0}, y_direction{0, 1, 0};
  Vector3 normal{0, 0, 1};
  std::vector<ParameterId> dependencies;
  std::string source_line;
  const auto id = std::string(object_id);
  const auto position = [](Point3 value) { return Vector3{value.x, value.y, value.z}; };
  if (const auto* point = part->find_construction_point(ConstructionPointId(id))) {
    switch (point->kind()) {
    case ConstructionPointKind::Explicit:
      kind = Kind::Point;
      break;
    case ConstructionPointKind::OnGeneratedEdge:
      kind = Kind::PointOnEdge;
      break;
    case ConstructionPointKind::OnGeneratedVertex:
      kind = Kind::PointOnVertex;
      break;
    }
    name = point->name();
    origin = position(point->position());
    dependencies = point->parameter_dependencies();
    relation = point->relation();
  } else if (const auto* line = part->find_construction_line(ConstructionLineId(id))) {
    switch (line->kind()) {
    case ConstructionLineKind::Explicit:
      kind = Kind::Line;
      break;
    case ConstructionLineKind::ThroughTwoPoints:
      kind = Kind::LineThroughPoints;
      break;
    case ConstructionLineKind::ParallelToLineThroughPoint:
      kind = Kind::ParallelLineThroughPoint;
      break;
    case ConstructionLineKind::ParallelToGeneratedEdgeThroughPoint:
      kind = Kind::ParallelEdgeThroughPoint;
      break;
    }
    name = line->name();
    origin = position(line->point());
    direction = line->direction();
    relation = line->relation();
    dependencies = line->parameter_dependencies();
  } else if (const auto* axis = part->find_datum_axis(DatumAxisId(id))) {
    kind = axis->kind() == DatumAxisKind::Explicit ? Kind::Axis : Kind::AxisFromLine;
    name = axis->name();
    origin = position(axis->origin());
    direction = axis->direction();
    source_line = axis->source_construction_line().value();
    dependencies = axis->parameter_dependencies();
  } else if (const auto* plane = part->find_construction_plane(ConstructionPlaneId(id))) {
    switch (plane->kind()) {
    case ConstructionPlaneKind::Explicit:
      kind = Kind::Plane;
      break;
    case ConstructionPlaneKind::OffsetFromPlane:
      kind = Kind::OffsetPlane;
      break;
    case ConstructionPlaneKind::ThroughThreePoints:
      kind = Kind::PlaneThroughPoints;
      break;
    case ConstructionPlaneKind::ParallelToPlaneThroughPoint:
      kind = Kind::ParallelPlaneThroughPoint;
      break;
    }
    name = plane->name();
    origin = position(plane->origin());
    direction = plane->x_axis();
    y_direction = plane->y_axis();
    normal = plane->normal();
    relation = plane->relation();
    dependencies = plane->parameter_dependencies();
  } else if (const auto* plane = part->find_derived_workplane(DatumPlaneId(id))) {
    kind = Kind::FacePlane;
    name = plane->name();
  } else {
    return Result<std::size_t>::failure(error("No editable construction element selected"));
  }
  updating_ = true;
  editing_ = true;
  edit_kind_ = kind;
  id_ = id;
  dependencies_ = std::move(dependencies);
  relation_id_ = relation ? relation->id().value() : id + ".relation";
  fill_references();
  kind_->setCurrentIndex(static_cast<int>(kind));
  kind_->setEnabled(false);
  name_->setText(QString::fromStdString(name));
  original_name_ = name;
  original_values_ = {origin, direction, y_direction};
  const std::array controls{origin_, direction_, y_direction_};
  for (std::size_t i = 0; i < controls.size(); ++i) {
    controls[i][0]->setValue(original_values_[i].x);
    controls[i][1]->setValue(original_values_[i].y);
    controls[i][2]->setValue(original_values_[i].z);
    initial_values_[i] = vector(controls[i]);
  }
  original_normal_ = normal;
  const auto select = [](QComboBox* combo, const std::string& value) {
    combo->setCurrentIndex(combo->findData(QString::fromStdString(value)));
  };
  if (relation) {
    select(plane_, relation->source_plane().value());
    select(parameter_, relation->offset_parameter().value());
    select(points_[0], relation->first_point().value());
    select(points_[1], relation->second_point().value());
    select(points_[2], relation->third_point().value());
    select(line_, relation->source_line().value());
    if (relation->generated_edge())
      select_generated(edge_, relation->generated_edge()->source_feature(),
                       static_cast<int>(relation->generated_edge()->edge()));
    if (relation->generated_vertex())
      select_generated(vertex_, relation->generated_vertex()->source_feature(),
                       static_cast<int>(relation->generated_vertex()->vertex()));
  }
  if (kind == Kind::AxisFromLine)
    select(line_, source_line);
  if (kind == Kind::FacePlane) {
    const auto& reference = part->find_derived_workplane(DatumPlaneId(id))->face_reference();
    select_generated(face_, reference.source_feature(), static_cast<int>(reference.face()));
  }
  dock_->setWindowTitle(QStringLiteral("Edit Construction"));
  updating_ = false;
  return open_task();
}

Result<std::size_t> ShellConstructionTools::open_task() {
  auto& session = window_.session();
  if (!session.task().begin(std::string(editing_ ? edit_task_id : task_id)) ||
      !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Could not start construction"));
  revision_ = session.model_revision();
  previous_filter_ = window_.viewport().selection_filter_mask();
  previous_scope_ = window_.viewport().selection_semantic_scope();
  precise_values_.reset();
  precise_offset_.reset();
  numeric_invalid_ = false;
  {
    const QSignalBlocker blocker(pick_role_);
    pick_role_->clear();
  }
  active_ = true;
  update_inputs();
  dock_->show();
  dock_->raise();
  window_.refresh_feature_selection_scene();
  update_preview();
  refresh_presentation();
  window_.refresh();
  return Result<std::size_t>::success(1);
}

void ShellConstructionTools::show_row(QWidget* widget, bool visible) {
  widget->setVisible(visible);
  if (auto* label = form_->labelForField(widget))
    label->setVisible(visible);
}
void ShellConstructionTools::update_inputs() {
  if (updating_)
    return;
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  const bool explicit_position =
      kind == Kind::Point || kind == Kind::Line || kind == Kind::Axis || kind == Kind::Plane;
  show_row(origin_row_, explicit_position);
  show_row(direction_row_, kind == Kind::Line || kind == Kind::Axis || kind == Kind::Plane);
  show_row(y_direction_row_, kind == Kind::Plane);
  show_row(plane_, kind == Kind::OffsetPlane || kind == Kind::ParallelPlaneThroughPoint);
  show_row(line_, kind == Kind::AxisFromLine || kind == Kind::ParallelLineThroughPoint);
  for (int i = 0; i < 3; ++i)
    show_row(points_[i], kind == Kind::PlaneThroughPoints ||
                             (kind == Kind::LineThroughPoints && i < 2) ||
                             (i == 0 && (kind == Kind::ParallelLineThroughPoint ||
                                         kind == Kind::ParallelEdgeThroughPoint ||
                                         kind == Kind::ParallelPlaneThroughPoint)));
  show_row(parameter_, kind == Kind::OffsetPlane);
  show_row(offset_, kind == Kind::OffsetPlane);
  offset_->setEnabled(parameter_->currentData().toString().isEmpty());
  if (const auto* part = window_.session().part_document())
    if (const auto* parameter =
            part->find_parameter(ParameterId(parameter_->currentData().toString().toStdString()))) {
      const bool previous = updating_;
      updating_ = true;
      offset_->setValue(parameter->value().millimeters());
      precise_offset_ = parameter->value().millimeters();
      precise_offset_display_ = offset_->value();
      updating_ = previous;
    }
  show_row(face_, kind == Kind::FacePlane);
  show_row(edge_, kind == Kind::PointOnEdge || kind == Kind::ParallelEdgeThroughPoint);
  show_row(vertex_, kind == Kind::PointOnVertex);
  update_pick_roles();
  update_manipulator_mode();
}

QComboBox* ShellConstructionTools::pick_input() const {
  const auto key = pick_role_->currentData().toString();
  for (auto* input : {plane_, line_, points_[0], points_[1], points_[2], face_, edge_, vertex_})
    if (input->objectName() == key)
      return input;
  return nullptr;
}

void ShellConstructionTools::update_pick_roles() {
  const auto previous = pick_role_->currentData();
  {
    const QSignalBlocker blocker(pick_role_);
    pick_role_->clear();
    for (auto* input : {plane_, line_, points_[0], points_[1], points_[2], face_, edge_, vertex_})
      if (!input->isHidden()) {
        const auto* label = qobject_cast<QLabel*>(form_->labelForField(input));
        pick_role_->addItem(label->text(), input->objectName());
      }
    const int retained = pick_role_->findData(previous);
    if (retained >= 0)
      pick_role_->setCurrentIndex(retained);
  }
  show_row(pick_role_, pick_role_->count() != 0);
  update_pick_scope();
}

void ShellConstructionTools::update_pick_scope() {
  if (!current())
    return;
  std::vector<std::string> scope;
  if (const auto* input = pick_input())
    for (int row = 0; row < input->count(); ++row)
      scope.push_back(input->itemData(row, Qt::UserRole + 2).toString().toStdString());
  auto& viewport = window_.viewport();
  viewport.set_selection_filter_mask(
      selection_kind_bit(GuiSelectionKind::Datum) | selection_kind_bit(GuiSelectionKind::Face) |
      selection_kind_bit(GuiSelectionKind::Edge) | selection_kind_bit(GuiSelectionKind::Vertex));
  viewport.set_selection_semantic_scope(std::move(scope));
}

bool ShellConstructionTools::accept_selection(const GuiSelection& selection) {
  if (!current())
    return false;
  auto* input = pick_input();
  if (!input)
    return false;
  const int row = input->findData(QString::fromStdString(selection.semantic_id), Qt::UserRole + 2);
  if (row < 0)
    return false;
  input->setCurrentIndex(row);
  // Defining points are collected in order; any role can also be chosen explicitly.
  if (input == points_[0] || input == points_[1]) {
    const int next = pick_role_->currentIndex() + 1;
    if (next < pick_role_->count())
      pick_role_->setCurrentIndex(next);
  }
  return true;
}

std::vector<geometry::ViewportSceneItem> ShellConstructionTools::selection_items() const {
  std::vector<geometry::ViewportSceneItem> items;
  if (current())
    for (const auto& target : selection_targets_)
      for (const auto* input : {face_, edge_, vertex_})
        if (!input->isHidden() &&
            input->findData(QString::fromStdString(target.item.semantic_id), Qt::UserRole + 2) >= 0)
          items.push_back(target.item);
  return items;
}

Result<std::size_t> ShellConstructionTools::populate(PartDocument& part) const {
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  if (editing_ && kind != edit_kind_)
    return Result<std::size_t>::failure(error("The construction type is preserved when editing"));
  const auto name = editing_ && name_->text().toStdString() == original_name_
                        ? original_name_
                        : name_->text().trimmed().toStdString();
  if (name.empty())
    return Result<std::size_t>::failure(error("Name must not be empty"));
  const auto values = definition_values();
  const auto coordinates = values[0];
  const Point3 origin{coordinates.x, coordinates.y, coordinates.z};
  const bool direction_unchanged = editing_ && values[1] == original_values_[1];
  const bool y_unchanged = editing_ && values[2] == original_values_[2];
  const auto direction = values[1];
  const ConstructionRelationId relation_id(relation_id_);
  switch (kind) {
  case Kind::PointOnEdge:
  case Kind::PointOnVertex: {
    const ConstructionPointId point(id_);
    Result<ConstructionRelation> relation = [&] {
      if (kind == Kind::PointOnEdge) {
        auto reference = SemanticEdgeReference::create(
            FeatureId(edge_->currentData().toString().toStdString()),
            static_cast<SemanticEdge>(edge_->currentData(Qt::UserRole + 1).toInt()));
        if (!reference)
          return Result<ConstructionRelation>::failure(reference.error());
        return ConstructionRelation::create_point_on_generated_edge(relation_id, point,
                                                                    reference.value());
      }
      auto reference = SemanticVertexReference::create(
          FeatureId(vertex_->currentData().toString().toStdString()),
          static_cast<SemanticVertex>(vertex_->currentData(Qt::UserRole + 1).toInt()));
      if (!reference)
        return Result<ConstructionRelation>::failure(reference.error());
      return ConstructionRelation::create_point_on_generated_vertex(relation_id, point,
                                                                    reference.value());
    }();
    if (!relation)
      return Result<std::size_t>::failure(relation.error());
    auto object =
        kind == Kind::PointOnEdge
            ? ConstructionPoint::create_on_generated_edge(point, name, relation.value())
            : ConstructionPoint::create_on_generated_vertex(point, name, relation.value());
    return insert(part, std::move(object),
                  editing_ ? &PartDocument::update_construction_point
                           : &PartDocument::add_construction_point);
  }
  case Kind::ParallelLineThroughPoint:
  case Kind::ParallelEdgeThroughPoint: {
    const ConstructionPointId through(points_[0]->currentData().toString().toStdString());
    auto relation = [&] {
      if (kind == Kind::ParallelLineThroughPoint)
        return ConstructionRelation::create_line_parallel_to_line_through_point(
            relation_id, ConstructionLineId(line_->currentData().toString().toStdString()),
            through);
      auto reference = SemanticEdgeReference::create(
          FeatureId(edge_->currentData().toString().toStdString()),
          static_cast<SemanticEdge>(edge_->currentData(Qt::UserRole + 1).toInt()));
      if (!reference)
        return Result<ConstructionRelation>::failure(reference.error());
      return ConstructionRelation::create_line_parallel_to_generated_edge_through_point(
          relation_id, reference.value(), through);
    }();
    if (!relation)
      return Result<std::size_t>::failure(relation.error());
    auto object = kind == Kind::ParallelLineThroughPoint
                      ? ConstructionLine::create_parallel_to_line_through_point(
                            ConstructionLineId(id_), name, relation.value())
                      : ConstructionLine::create_parallel_to_generated_edge_through_point(
                            ConstructionLineId(id_), name, relation.value());
    return insert(part, std::move(object),
                  editing_ ? &PartDocument::update_construction_line
                           : &PartDocument::add_construction_line);
  }
  case Kind::ParallelPlaneThroughPoint: {
    auto relation = ConstructionRelation::create_plane_parallel_to_plane_through_point(
        relation_id, DatumPlaneId(plane_->currentData().toString().toStdString()),
        ConstructionPointId(points_[0]->currentData().toString().toStdString()));
    if (!relation)
      return Result<std::size_t>::failure(relation.error());
    return insert(part,
                  ConstructionPlane::create_parallel_to_plane_through_point(
                      ConstructionPlaneId(id_), name, relation.value()),
                  editing_ ? &PartDocument::update_construction_plane
                           : &PartDocument::add_construction_plane);
  }
  case Kind::Point:
    return insert(
        part,
        ConstructionPoint::create_explicit(ConstructionPointId(id_), name, origin, dependencies_),
        editing_ ? &PartDocument::update_construction_point
                 : &PartDocument::add_construction_point);
  case Kind::Line:
    return insert(part,
                  ConstructionLine::create_explicit(ConstructionLineId(id_), name, origin,
                                                    direction, dependencies_),
                  editing_ ? &PartDocument::update_construction_line
                           : &PartDocument::add_construction_line);
  case Kind::Axis:
    return insert(
        part, DatumAxis::create_explicit(DatumAxisId(id_), name, origin, direction, dependencies_),
        editing_ ? &PartDocument::update_datum_axis : &PartDocument::add_datum_axis);
  case Kind::AxisFromLine:
    return insert(part,
                  DatumAxis::create_from_construction_line(
                      DatumAxisId(id_), name,
                      ConstructionLineId(line_->currentData().toString().toStdString())),
                  editing_ ? &PartDocument::update_datum_axis : &PartDocument::add_datum_axis);
  case Kind::Plane: {
    const auto y = values[2];
    return insert(part,
                  ConstructionPlane::create_explicit(
                      ConstructionPlaneId(id_), name, origin, direction, y,
                      direction_unchanged && y_unchanged ? original_normal_ : cross(direction, y),
                      dependencies_),
                  editing_ ? &PartDocument::update_construction_plane
                           : &PartDocument::add_construction_plane);
  }
  case Kind::LineThroughPoints:
  case Kind::PlaneThroughPoints: {
    const auto point = [this](int i) {
      return ConstructionPointId(points_[i]->currentData().toString().toStdString());
    };
    auto relation =
        kind == Kind::LineThroughPoints
            ? ConstructionRelation::create_line_through_two_points(relation_id, point(0), point(1))
            : ConstructionRelation::create_plane_through_three_points(relation_id, point(0),
                                                                      point(1), point(2));
    if (!relation)
      return Result<std::size_t>::failure(relation.error());
    if (kind == Kind::LineThroughPoints)
      return insert(part,
                    ConstructionLine::create_through_two_points(ConstructionLineId(id_), name,
                                                                relation.value()),
                    editing_ ? &PartDocument::update_construction_line
                             : &PartDocument::add_construction_line);
    return insert(part,
                  ConstructionPlane::create_through_three_points(ConstructionPlaneId(id_), name,
                                                                 relation.value()),
                  editing_ ? &PartDocument::update_construction_plane
                           : &PartDocument::add_construction_plane);
  }
  case Kind::OffsetPlane: {
    ParameterId parameter(parameter_->currentData().toString().toStdString());
    if (parameter.empty()) {
      parameter = ParameterId(id_ + ".offset");
      for (std::size_t suffix = 2; part.dependency_graph().has_node(parameter.value()); ++suffix)
        parameter = ParameterId(id_ + ".offset." + std::to_string(suffix));
      auto quantity = Quantity::length_mm(offset_value(), parameter.value());
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      auto parameter_name = "Offset " + name;
      for (std::size_t suffix = 2;
           std::any_of(part.parameters().begin(), part.parameters().end(),
                       [&](const Parameter& item) { return item.name() == parameter_name; });
           ++suffix)
        parameter_name = "Offset " + name + " " + std::to_string(suffix);
      auto added =
          insert(part, Parameter::create_length(parameter, parameter_name, quantity.value()),
                 &PartDocument::add_parameter);
      if (!added)
        return added;
    }
    auto relation = ConstructionRelation::create_plane_offset_from_plane(
        relation_id, DatumPlaneId(plane_->currentData().toString().toStdString()), parameter);
    if (!relation)
      return Result<std::size_t>::failure(relation.error());
    return insert(part,
                  ConstructionPlane::create_offset_from_plane(ConstructionPlaneId(id_), name,
                                                              relation.value()),
                  editing_ ? &PartDocument::update_construction_plane
                           : &PartDocument::add_construction_plane);
  }
  case Kind::FacePlane: {
    auto reference = SemanticFaceReference::create(
        FeatureId(face_->currentData().toString().toStdString()),
        static_cast<SemanticFace>(face_->currentData(Qt::UserRole + 1).toInt()));
    if (!reference)
      return Result<std::size_t>::failure(reference.error());
    return insert(
        part, DerivedWorkplane::create_on_feature_face(DatumPlaneId(id_), name, reference.value()),
        editing_ ? &PartDocument::update_derived_workplane : &PartDocument::add_derived_workplane);
  }
  }
  return Result<std::size_t>::failure(error("Unknown construction type"));
}

bool ShellConstructionTools::current() const {
  const auto& session = window_.session();
  return active_ && session.part_document() && session.part_shape_cache() &&
         session.task().command_id() == (editing_ ? edit_task_id : task_id) &&
         session.derived_results_fresh() && session.model_revision() == revision_;
}
Result<std::size_t> ShellConstructionTools::preview() {
  if (!current())
    return Result<std::size_t>::failure(error("Model or command changed. Reopen construction."));
  apply_->setEnabled(false);
  window_.viewport().clear_feature_preview();
  (void)window_.session().task().return_to_editing();
  PartDocument candidate = *window_.session().part_document();
  auto added = populate(candidate);
  if (!added)
    return added;
  std::optional<geometry::ShapeCache> preview_cache;
  if (editing_) {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("construction.edit.preview"));
    if (!cache)
      return Result<std::size_t>::failure(cache.error());
    auto executed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
    if (!executed)
      return Result<std::size_t>::failure(executed.error());
    preview_cache = std::move(cache.value());
  }
  auto scene = geometry::ViewportSceneBuilder{}.build_part(
      candidate, preview_cache ? *preview_cache : *window_.session().part_shape_cache());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  const std::string prefix =
      kind == Kind::Point || kind == Kind::PointOnEdge || kind == Kind::PointOnVertex
          ? "construction-point/"
      : kind == Kind::Line || kind == Kind::LineThroughPoints ||
              kind == Kind::ParallelLineThroughPoint || kind == Kind::ParallelEdgeThroughPoint
          ? "construction-line/"
      : kind == Kind::Axis || kind == Kind::AxisFromLine ? "datum-axis/"
      : kind == Kind::FacePlane                          ? "derived-workplane/"
                                                         : "construction-plane/";
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  auto affected = candidate.dependency_graph().transitive_dependents(id_);
  affected.push_back(id_);
  const auto included = [&](const geometry::ViewportSceneItem& item) {
    if (!editing_)
      return item.semantic_id == prefix + id_;
    if (item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody)
      return true;
    return std::any_of(affected.begin(), affected.end(), [&](const std::string& id) {
      const auto slash = item.semantic_id.find('/');
      return (slash != std::string::npos && item.semantic_id.substr(slash + 1) == id) ||
             item.semantic_id.starts_with("sketch/" + id + "/");
    });
  };
  for (auto& item : scene.value())
    if (included(item))
      shapes.push_back(std::move(item.shape));
  if (shapes.empty() || (!editing_ && shapes.size() != 1))
    return Result<std::size_t>::failure(error("Could not display construction geometry"));
  if (editing_) {
    auto original = geometry::ViewportSceneBuilder{}.build_part(
        *window_.session().part_document(), *window_.session().part_shape_cache());
    if (!original)
      return Result<std::size_t>::failure(original.error());
    for (const auto& item : original.value())
      if (included(item))
        replaced.push_back(item.semantic_id);
  }
  auto shown = window_.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  (void)window_.session().task().show_preview();
  summary_->setText(editing_
                        ? QStringLiteral("Preview including downstream geometry valid. Apply "
                                         "commits the change.")
                        : QStringLiteral("Preview valid. Apply creates a construction element."));
  apply_->setEnabled(true);
  return Result<std::size_t>::success(1);
}
void ShellConstructionTools::update_preview() {
  if (!active_ || updating_)
    return;
  auto result = preview();
  if (!result)
    summary_->setText(QString::fromStdString(result.error().message()));
  if (numeric_invalid_) {
    apply_->setEnabled(false);
    summary_->setText(
        QStringLiteral("Invalid handle value. Enter a number in mm or deg, or press Escape."));
  }
  refresh_handles();
}
Result<std::size_t> ShellConstructionTools::apply() {
  if (!current())
    return Result<std::size_t>::failure(error("Model or command changed. Reopen construction."));
  if (numeric_invalid_)
    return Result<std::size_t>::failure(
        error("Invalid handle value. Correct the value or press Escape."));
  drag_.reset();
  layer_.cancel();
  if (window_.viewport().mouseGrabber() == &window_.viewport())
    window_.viewport().release_interaction_mouse();
  // Keyboard tracking is disabled for numeric refinement. A click on Apply
  // must consume pending editor text even before native focus-out is delivered.
  updating_ = true;
  for (const auto& controls : {origin_, direction_, y_direction_})
    for (auto* control : controls)
      control->interpretText();
  offset_->interpretText();
  updating_ = false;
  auto checked = preview();
  if (!checked)
    return checked;
  auto result = window_.session().commit_part_transaction(
      editing_ ? "Edit construction geometry" : "Create construction geometry",
      [this](PartDocument& part) { return populate(part); });
  if (!result)
    return result;
  (void)window_.session().task().apply();
  cancel();
  return result;
}
void ShellConstructionTools::cancel() {
  if (!active_)
    return;
  active_ = false;
  drag_.reset();
  layer_.clear_handles();
  if (window_.viewport().mouseGrabber() == &window_.viewport())
    window_.viewport().release_interaction_mouse();
  grip_numeric_->clear();
  grip_numeric_->setReadOnly(true);
  overlay_->hide();
  if (window_.session().task().command_id() == (editing_ ? edit_task_id : task_id))
    (void)window_.session().task().cancel();
  window_.viewport().clear_feature_preview();
  window_.viewport().set_selection_semantic_scope(previous_scope_);
  window_.viewport().set_selection_filter_mask(previous_filter_);
  selection_targets_.clear();
  apply_->setEnabled(false);
  dock_->hide();
  window_.refresh_feature_selection_scene();
  window_.refresh();
}
void ShellConstructionTools::refresh() {
  if (active_ && !current()) {
    cancel();
    return;
  }
  const auto& session = window_.session();
  const bool enabled = session.part_document() && session.derived_results_fresh() &&
                       !window_.active_sketch() && !session.task().active();
  construction_action_->setEnabled(enabled);
  workplane_action_->setEnabled(enabled);
}
bool ShellConstructionTools::eventFilter(QObject* watched, QEvent* event) {
  if (manipulator_event(watched, event))
    return true;
  if (active_ && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    if (drag_)
      cancel_drag();
    else
      cancel();
    return true;
  }
  if (active_ && watched == dock_ && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
