#include "blcad/gui/shell/shell_construction_tools.hpp"

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"
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
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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
Vector3 unit(Vector3 value) {
  const double magnitude = length(value);
  return magnitude > 1e-12 ? Vector3{value.x / magnitude, value.y / magnitude, value.z / magnitude}
                           : Vector3{};
}
} // namespace

ShellConstructionTools::ShellConstructionTools(ShellWindow& window, ShellRibbon& ribbon,
                                               int model_tab)
    : QObject(&window), window_(window) {
  auto* group = ribbon.add_group(model_tab, QStringLiteral("Konstruktion"));
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
  construction_action_ = action("create_construction", "Konstruktionsgeometrie", Kind::Point);
  workplane_action_ = action("create_workplane", "Arbeitsebene", Kind::OffsetPlane);

  dock_ = new QDockWidget(QStringLiteral("Konstruktionsgeometrie"), &window);
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
  kind_->addItems(
      {QStringLiteral("Punkt (XYZ)"), QStringLiteral("Linie (Punkt und Richtung)"),
       QStringLiteral("Linie durch zwei Punkte"), QStringLiteral("Achse (Punkt und Richtung)"),
       QStringLiteral("Achse aus Konstruktionslinie"),
       QStringLiteral("Ebene (Ursprung und Richtungen)"), QStringLiteral("Versatzebene"),
       QStringLiteral("Ebene durch drei Punkte"), QStringLiteral("Ebene auf Modellfläche")});
  form_->addRow(QStringLiteral("Typ"), kind_);
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
  form_->addRow(QStringLiteral("Ursprung"), origin_row_);
  form_->addRow(QStringLiteral("X-Richtung"), direction_row_);
  form_->addRow(QStringLiteral("Y-Richtung (senkrecht)"), y_direction_row_);
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
  plane_ = combo("plane", QStringLiteral("Bezugsebene"));
  line_ = combo("line", QStringLiteral("Konstruktionslinie"));
  for (int i = 0; i < 3; ++i)
    points_[i] =
        combo(("point" + std::to_string(i + 1)).c_str(), QStringLiteral("Punkt %1").arg(i + 1));
  parameter_ = combo("parameter", QStringLiteral("Versatzparameter"));
  offset_ = new QDoubleSpinBox(inputs);
  offset_->setObjectName("blcad.construction.offset");
  offset_->setDecimals(6);
  offset_->setRange(0.000001, 1e6);
  offset_->setValue(10);
  offset_->setSuffix(QStringLiteral(" mm"));
  offset_->setKeyboardTracking(false);
  form_->addRow(QStringLiteral("Versatz (+Normalrichtung)"), offset_);
  face_ = combo("face", QStringLiteral("Modellfläche"));
  layout->addLayout(form_);
  summary_ = new QLabel(inputs);
  summary_->setObjectName("blcad.construction.summary");
  summary_->setWordWrap(true);
  layout->addWidget(summary_);
  layout->addStretch();
  scroll->setWidget(inputs);
  column->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  apply_ = new QPushButton(QStringLiteral("Anwenden"), panel);
  apply_->setObjectName("blcad.construction.apply");
  auto* cancel = new QPushButton(QStringLiteral("Abbrechen"), panel);
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
  // Children added to the panel are explicit task controls, not discovered widgets.
  for (auto* control : {static_cast<QWidget*>(kind_), static_cast<QWidget*>(name_),
                        static_cast<QWidget*>(apply_), static_cast<QWidget*>(cancel)})
    control->installEventFilter(this);
  for (auto* combo_control : {plane_, line_, parameter_, face_, points_[0], points_[1], points_[2]})
    combo_control->installEventFilter(this);
  for (const auto& controls : {origin_, direction_, y_direction_})
    for (auto* control : controls)
      control->installEventFilter(this);
  offset_->installEventFilter(this);
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
    update_inputs();
    update_preview();
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
  for (auto* control : {plane_, line_, parameter_, face_, points_[0], points_[1], points_[2]})
    control->clear();
  const auto add = [](QComboBox* control, const auto& object) {
    control->addItem(QString::fromStdString(object.name()),
                     QString::fromStdString(object.id().value()));
    control->setItemData(control->count() - 1, QString::fromStdString(object.id().value()),
                         Qt::ToolTipRole);
  };
  for (const auto& plane : part.datum_planes())
    add(plane_, plane);
  for (const auto& plane : part.construction_planes())
    if (!editing_ || plane.id().value() != id_)
      add(plane_, plane);
  for (const auto& plane : part.derived_workplanes())
    if (!editing_ || plane.id().value() != id_)
      add(plane_, plane);
  for (const auto& line : part.construction_lines())
    if (!editing_ || line.id().value() != id_)
      add(line_, line);
  for (const auto& point : part.construction_points())
    for (auto* control : points_)
      add(control, point);
  for (int i = 0; i < 3; ++i)
    points_[i]->setCurrentIndex(std::min(i, points_[i]->count() - 1));
  parameter_->addItem(QStringLiteral("Neuer Parameter"), QString{});
  for (const auto& parameter : part.parameters())
    if (parameter.type() == ParameterType::Length)
      add(parameter_, parameter);
  const geometry::WorkplaneResolver resolver;
  for (const auto& feature : part.features())
    for (auto face : {SemanticFace::Top, SemanticFace::Bottom, SemanticFace::Right,
                      SemanticFace::Left, SemanticFace::Front, SemanticFace::Back}) {
      auto reference = SemanticFaceReference::create(feature.id(), face);
      if (!reference || !resolver.resolve_generated_face(part, reference.value()))
        continue;
      const auto label =
          QString::fromStdString(feature.name() + " / " + std::string(to_string(face)));
      face_->addItem(label, QString::fromStdString(feature.id().value()));
      face_->setItemData(face_->count() - 1, static_cast<int>(face), Qt::UserRole + 1);
    }
}

Result<std::size_t> ShellConstructionTools::begin(Kind kind) {
  auto& session = window_.session();
  if (!session.part_document() || !session.derived_results_fresh() || window_.active_sketch() ||
      session.task().active())
    return Result<std::size_t>::failure(
        error("Konstruktion benötigt ein aktuelles Bauteil ohne aktiven Befehl"));
  updating_ = true;
  editing_ = false;
  dependencies_.clear();
  kind_->setEnabled(true);
  dock_->setWindowTitle(QStringLiteral("Konstruktion erstellen"));
  fill_references();
  std::size_t number = 1;
  const auto& graph = session.part_document()->dependency_graph();
  do {
    id_ = "construction.shell." + std::to_string(number++);
  } while (graph.has_node(id_) || graph.has_node(id_ + ".offset") ||
           graph.has_node(id_ + ".relation"));
  relation_id_ = id_ + ".relation";
  name_->setText(QStringLiteral("Konstruktion %1").arg(number - 1));
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
        error("Bearbeiten benötigt ein aktuelles Bauteil ohne aktiven Befehl"));
  // Principal origin geometry stays fixed in the shell.
  if (object_id == "datum.axis.x" || object_id == "datum.axis.y" || object_id == "datum.axis.z")
    return Result<std::size_t>::failure(error("Ursprungsgeometrie ist fest definiert"));
  Kind kind;
  std::string name;
  std::optional<ConstructionRelation> relation;
  Vector3 origin{}, direction{1, 0, 0}, y_direction{0, 1, 0};
  Vector3 normal{0, 0, 1};
  std::vector<ParameterId> dependencies;
  std::string source_line;
  const auto id = std::string(object_id);
  const auto position = [](Point3 value) { return Vector3{value.x, value.y, value.z}; };
  const auto unsupported = [] {
    return Result<std::size_t>::failure(
        error("Dieser Konstruktionstyp kann noch nicht im Dialog bearbeitet werden"));
  };
  if (const auto* point = part->find_construction_point(ConstructionPointId(id))) {
    if (point->kind() != ConstructionPointKind::Explicit)
      return unsupported();
    kind = Kind::Point;
    name = point->name();
    origin = position(point->position());
    dependencies = point->parameter_dependencies();
  } else if (const auto* line = part->find_construction_line(ConstructionLineId(id))) {
    if (line->kind() != ConstructionLineKind::Explicit &&
        line->kind() != ConstructionLineKind::ThroughTwoPoints)
      return unsupported();
    kind = line->kind() == ConstructionLineKind::Explicit ? Kind::Line : Kind::LineThroughPoints;
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
    default:
      return unsupported();
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
    return Result<std::size_t>::failure(error("Kein bearbeitbares Konstruktionselement gewählt"));
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
  }
  if (kind == Kind::AxisFromLine)
    select(line_, source_line);
  if (kind == Kind::FacePlane) {
    const auto& reference = part->find_derived_workplane(DatumPlaneId(id))->face_reference();
    face_->setCurrentIndex(-1);
    for (int i = 0; i < face_->count(); ++i)
      if (face_->itemData(i).toString().toStdString() == reference.source_feature().value() &&
          face_->itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(reference.face()))
        face_->setCurrentIndex(i);
  }
  dock_->setWindowTitle(QStringLiteral("Konstruktion bearbeiten"));
  updating_ = false;
  return open_task();
}

Result<std::size_t> ShellConstructionTools::open_task() {
  auto& session = window_.session();
  if (!session.task().begin(std::string(editing_ ? edit_task_id : task_id)) ||
      !session.task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Konstruktion konnte nicht gestartet werden"));
  revision_ = session.presentation_revision();
  active_ = true;
  update_inputs();
  dock_->show();
  dock_->raise();
  update_preview();
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
  show_row(plane_, kind == Kind::OffsetPlane);
  show_row(line_, kind == Kind::AxisFromLine);
  for (int i = 0; i < 3; ++i)
    show_row(points_[i],
             kind == Kind::PlaneThroughPoints || (kind == Kind::LineThroughPoints && i < 2));
  show_row(parameter_, kind == Kind::OffsetPlane);
  show_row(offset_, kind == Kind::OffsetPlane);
  offset_->setEnabled(parameter_->currentData().toString().isEmpty());
  if (const auto* part = window_.session().part_document())
    if (const auto* parameter =
            part->find_parameter(ParameterId(parameter_->currentData().toString().toStdString()))) {
      const bool previous = updating_;
      updating_ = true;
      offset_->setValue(parameter->value().millimeters());
      updating_ = previous;
    }
  show_row(face_, kind == Kind::FacePlane);
}

Result<std::size_t> ShellConstructionTools::populate(PartDocument& part) const {
  const auto kind = static_cast<Kind>(kind_->currentIndex());
  if (editing_ && kind != edit_kind_)
    return Result<std::size_t>::failure(
        error("Der Konstruktionstyp bleibt beim Bearbeiten erhalten"));
  const auto name = editing_ && name_->text().toStdString() == original_name_
                        ? original_name_
                        : name_->text().trimmed().toStdString();
  if (name.empty())
    return Result<std::size_t>::failure(error("Name darf nicht leer sein"));
  const auto coordinates = [&] {
    auto value = vector(origin_);
    if (editing_) {
      if (value.x == initial_values_[0].x)
        value.x = original_values_[0].x;
      if (value.y == initial_values_[0].y)
        value.y = original_values_[0].y;
      if (value.z == initial_values_[0].z)
        value.z = original_values_[0].z;
    }
    return value;
  }();
  const Point3 origin{coordinates.x, coordinates.y, coordinates.z};
  const bool direction_unchanged = editing_ && vector(direction_) == initial_values_[1];
  const bool y_unchanged = editing_ && vector(y_direction_) == initial_values_[2];
  const auto direction = direction_unchanged ? original_values_[1] : unit(vector(direction_));
  const ConstructionRelationId relation_id(relation_id_);
  switch (kind) {
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
    const auto y = y_unchanged ? original_values_[2] : unit(vector(y_direction_));
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
      auto quantity = Quantity::length_mm(offset_->value(), parameter.value());
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      auto parameter_name = "Versatz " + name;
      for (std::size_t suffix = 2;
           std::any_of(part.parameters().begin(), part.parameters().end(),
                       [&](const Parameter& item) { return item.name() == parameter_name; });
           ++suffix)
        parameter_name = "Versatz " + name + " " + std::to_string(suffix);
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
  return Result<std::size_t>::failure(error("Unbekannter Konstruktionstyp"));
}

bool ShellConstructionTools::current() const {
  const auto& session = window_.session();
  return active_ && session.part_document() && session.part_shape_cache() &&
         session.task().command_id() == (editing_ ? edit_task_id : task_id) &&
         session.derived_results_fresh() && session.presentation_revision() == revision_;
}
Result<std::size_t> ShellConstructionTools::preview() {
  if (!current())
    return Result<std::size_t>::failure(
        error("Modell oder Befehl geändert. Konstruktion neu öffnen."));
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
  const std::string prefix = kind == Kind::Point ? "construction-point/"
                             : kind == Kind::Line || kind == Kind::LineThroughPoints
                                 ? "construction-line/"
                             : kind == Kind::Axis || kind == Kind::AxisFromLine ? "datum-axis/"
                             : kind == Kind::FacePlane ? "derived-workplane/"
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
    return Result<std::size_t>::failure(
        error("Konstruktionsgeometrie konnte nicht dargestellt werden"));
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
  summary_->setText(
      editing_ ? QStringLiteral("Vorschau einschließlich nachgelagerter Geometrie gültig. Anwenden "
                                "übernimmt die Änderung.")
               : QStringLiteral("Vorschau gültig. Anwenden erstellt ein Konstruktionselement."));
  apply_->setEnabled(true);
  return Result<std::size_t>::success(1);
}
void ShellConstructionTools::update_preview() {
  if (!active_ || updating_)
    return;
  auto result = preview();
  if (!result)
    summary_->setText(QString::fromStdString(result.error().message()));
}
Result<std::size_t> ShellConstructionTools::apply() {
  if (!current())
    return Result<std::size_t>::failure(
        error("Modell oder Befehl geändert. Konstruktion neu öffnen."));
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
  if (window_.session().task().command_id() == (editing_ ? edit_task_id : task_id))
    (void)window_.session().task().cancel();
  window_.viewport().clear_feature_preview();
  apply_->setEnabled(false);
  dock_->hide();
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
  if (active_ && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active_ && watched == dock_ && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
