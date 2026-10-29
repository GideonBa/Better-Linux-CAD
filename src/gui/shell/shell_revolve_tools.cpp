#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace blcad::gui {

std::vector<ShellFeatureTools::RevolveAxisChoice>
ShellFeatureTools::revolve_axis_choices(const GuiSketchPlaneView& plane) const {
  std::vector<RevolveAxisChoice> choices;
  const auto* part = session_.part_document();
  const auto* cache = session_.part_shape_cache();
  if (!part || !cache)
    return choices;
  const auto append = [&](Result<AxisReference> reference, std::string label,
                          std::string identity) {
    if (!reference)
      return;
    auto axis = geometry::GeometryRecomputeExecutor{}.resolve_feature_axis(*part, reference.value(),
                                                                           *cache);
    if (!axis)
      return;
    const auto [origin, direction] = axis.value();
    const Vector3 offset{origin.x - plane.origin.x, origin.y - plane.origin.y,
                         origin.z - plane.origin.z};
    if (std::abs(dot(direction, plane.normal)) > 1e-6 || std::abs(dot(offset, plane.normal)) > 1e-6)
      return;
    auto radial = plane.x_axis;
    if (std::abs(dot(radial, direction)) > 0.99)
      radial = plane.y_axis;
    choices.push_back({std::move(reference.value()),
                       {origin, direction, direction, radial},
                       std::move(label),
                       std::move(identity)});
  };
  for (const auto& axis : part->datum_axes())
    append(AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis, axis.id()),
           axis.name(), "datum-axis/" + axis.id().value());
  for (const auto& line : part->construction_lines())
    append(AxisReference::create_construction_line(PartFeatureInputRole::RevolveAxis, line.id(),
                                                   PartFeatureInputCapability::Axis),
           line.name(), "construction-line/" + line.id().value());
  for (const auto& sketch : part->sketches())
    for (const auto& line : sketch.line_segments()) {
      auto reference = SketchLineReference::create(sketch.id(), line.id());
      if (!reference)
        continue;
      const auto role = line.role() == SketchLineRole::Centerline ? "Centerline"
                        : line.construction()                     ? "Construction"
                                                                  : "Geometry";
      append(
          AxisReference::create_sketch_line(PartFeatureInputRole::RevolveAxis, reference.value()),
          std::string("Sketch Line (") + role + "): " + sketch.name() + " / " + line.id().value(),
          reference.value().identity());
    }
  for (const auto& feature : part->features())
    if (feature.type() == FeatureType::SubtractiveExtrude) {
      auto semantic = SemanticAxisReference::create(feature.id());
      if (semantic)
        append(AxisReference::create_semantic_axis(PartFeatureInputRole::RevolveAxis,
                                                   semantic.value()),
               feature.name() + " / Axis", "feature-axis/" + feature.id().value());
    }
  auto sources = geometry::ViewportSceneBuilder{}.build_construction_sources(*part, *cache);
  if (sources)
    for (const auto& source : sources.value())
      if (const auto* edge = std::get_if<SemanticEdgeReference>(&source.reference)) {
        const auto* feature = part->find_feature(edge->source_feature());
        append(AxisReference::create_semantic_edge(PartFeatureInputRole::RevolveAxis, *edge,
                                                   PartFeatureInputCapability::Axis),
               feature->name() + " / " + std::string(to_string(edge->edge())),
               source.item.semantic_id);
      }
  return choices;
}

void ShellFeatureTools::build_revolve_controls(QWidget* panel) {
  revolve_controls_ = new QWidget(panel);
  revolve_controls_->setObjectName(QStringLiteral("blcad.revolve.inputs"));
  auto* column = new QVBoxLayout(revolve_controls_);
  column->setContentsMargins(0, 0, 0, 0);
  revolve_form_ = new QFormLayout();
  revolve_form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
  column->addLayout(revolve_form_);
  const auto combo = [&](const char* id, const QString& label) {
    auto* field = new QComboBox(revolve_controls_);
    field->setObjectName(QString::fromLatin1(id));
    field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    field->setMinimumContentsLength(15);
    revolve_form_->addRow(label, field);
    return field;
  };
  revolve_extent_ = combo("blcad.revolve.extent", QStringLiteral("Revolve Extent"));
  revolve_extent_->addItems(
      {QStringLiteral("Full Revolution"), QStringLiteral("Angle"), QStringLiteral("Symmetric")});
  revolve_side_ = combo("blcad.revolve.side", QStringLiteral("Rotation Direction"));
  revolve_side_->addItems({QStringLiteral("Positive"), QStringLiteral("Negative")});
  revolve_operation_ = combo("blcad.revolve.operation", QStringLiteral("Operation"));
  revolve_operation_->addItems({QStringLiteral("New Body"), QStringLiteral("Union"),
                                QStringLiteral("Cut"), QStringLiteral("Intersect")});
  revolve_target_ = combo("blcad.revolve.target", QStringLiteral("Target Body"));
  revolve_axis_ = combo("blcad.revolve.axis", QStringLiteral("Rotation Axis"));
  auto* hint = new QLabel(
      QStringLiteral("Axes must lie in the profile plane. The profile must not cross the axis. "
                     "Symmetric uses the total included angle. "
                     "All input remains local until Apply."),
      revolve_controls_);
  hint->setWordWrap(true);
  column->addWidget(hint);
  qobject_cast<QVBoxLayout*>(panel->layout())->addWidget(revolve_controls_);
  for (auto* field :
       {revolve_extent_, revolve_side_, revolve_operation_, revolve_target_, revolve_axis_})
    connect(field, &QComboBox::currentIndexChanged, this, [this] { refresh_revolve_preview(); });
  revolve_controls_->hide();
}

Result<std::size_t>
ShellFeatureTools::reset_revolve_controls(const GuiSketchPlaneView& plane,
                                          std::optional<DatumAxisId> requested) {
  const QSignalBlocker b1(revolve_extent_), b2(revolve_side_), b3(revolve_operation_),
      b4(revolve_target_), b5(revolve_axis_);
  revolve_extent_->setCurrentIndex(1);
  revolve_side_->setCurrentIndex(0);
  revolve_operation_->setCurrentIndex(0);
  revolve_target_->clear();
  revolve_axis_->clear();
  revolve_axes_ = revolve_axis_choices(plane);
  for (const auto& axis : revolve_axes_)
    revolve_axis_->addItem(QString::fromStdString(axis.label),
                           QString::fromStdString(axis.semantic_id));
  auto preferred = requested;
  if (!preferred) {
    auto fallback = default_revolve_axis(plane);
    if (fallback)
      preferred = fallback.value();
  }
  if (preferred) {
    const auto row =
        revolve_axis_->findData(QString::fromStdString("datum-axis/" + preferred->value()));
    if (row >= 0)
      revolve_axis_->setCurrentIndex(row);
    else if (requested)
      return Result<std::size_t>::failure(Error::validation(
          preferred->value(), "Rotation axis must exist and lie in the profile plane."));
  }
  if (revolve_axis_->count() == 0)
    return Result<std::size_t>::failure(
        Error::validation("revolve", "Create an axis in the profile plane first."));
  const auto& part = *session_.part_document();
  for (const auto& body : part.bodies())
    if (body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
        session_.part_shape_cache()->find_body_shape(body.id()))
      revolve_target_->addItem(QString::fromStdString(body.name()),
                               QString::fromStdString(body.id().value()));
  return Result<std::size_t>::success(1);
}

void ShellFeatureTools::update_revolve_controls() {
  const bool full = revolve_extent_->currentIndex() == 0;
  numeric_editor_->setVisible(!full);
  value_label_->setVisible(!full);
  revolve_side_->setEnabled(revolve_extent_->currentIndex() == 1);
  revolve_target_->setEnabled(revolve_operation_->currentIndex() != 0);
}

void ShellFeatureTools::refresh_revolve_preview() {
  if (command_ != Command::Revolve)
    return;
  update_revolve_controls();
  auto preview = preview_and_publish();
  if (!preview)
    report(preview.error());
}

Result<std::size_t> ShellFeatureTools::configure_revolve_candidate() {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("revolve", std::move(message)));
  };
  if (!layer_.active_handle_id().empty())
    return Result<std::size_t>::success(0);
  if (revolve_axis_->currentIndex() < 0)
    return fail("Select a suitable rotation axis.");
  const auto operation = static_cast<FeatureBodyOperationMode>(revolve_operation_->currentIndex());
  if (operation != FeatureBodyOperationMode::NewBody && revolve_target_->currentIndex() < 0)
    return fail("Select a visible solid body as the target.");
  auto& controller = coordinator_.revolve();
  if (revolve_extent_->currentIndex() != 0) {
    const auto text = numeric_editor_->text().trimmed().toStdString();
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    const auto suffix =
        QString::fromStdString(std::string(end, text.data() + text.size())).trimmed();
    if (end == text.data() || error != std::errc() || !std::isfinite(value) || value <= 0 ||
        value >= 360 || (!suffix.isEmpty() && suffix != QStringLiteral("deg")))
      return fail("Revolve angle must be between 0 and 360 degrees.");
  }
  const auto& axis = revolve_axes_.at(static_cast<std::size_t>(revolve_axis_->currentIndex()));
  controller.set_axis(axis.reference);
  controller.set_kind(operation == FeatureBodyOperationMode::Cut ? RevolveFeatureKind::RevolveCut
                                                                 : RevolveFeatureKind::Revolve);
  controller.set_operation(
      operation,
      operation == FeatureBodyOperationMode::NewBody
          ? std::nullopt
          : std::optional<BodyId>{BodyId(revolve_target_->currentData().toString().toStdString())});
  auto frame = axis.frame;
  auto resolved = geometry::GeometryRecomputeExecutor{}.resolve_feature_axis(
      *session_.part_document(), axis.reference, *session_.part_shape_cache());
  if (!resolved)
    return Result<std::size_t>::failure(resolved.error());
  frame.origin = resolved.value().first;
  frame.axis = frame.plane_normal = normalized(resolved.value().second);
  const auto projection = dot(frame.reference_direction, frame.axis);
  frame.reference_direction = {frame.reference_direction.x - projection * frame.axis.x,
                               frame.reference_direction.y - projection * frame.axis.y,
                               frame.reference_direction.z - projection * frame.axis.z};
  if (length(frame.reference_direction) <= 1e-9)
    frame.reference_direction =
        cross(std::abs(frame.axis.x) < .9 ? Vector3{1, 0, 0} : Vector3{0, 1, 0}, frame.axis);
  frame.reference_direction = normalized(frame.reference_direction);
  if (revolve_extent_->currentIndex() == 0)
    controller.set_full();
  else if (revolve_extent_->currentIndex() == 2)
    controller.set_symmetric(controller.angle());
  else {
    const bool negative = revolve_side_->currentIndex() == 1;
    controller.set_angle(controller.angle(),
                         negative ? RevolveSide::Negative : RevolveSide::Positive);
    if (negative)
      frame.plane_normal = {-frame.plane_normal.x, -frame.plane_normal.y, -frame.plane_normal.z};
  }
  controller.set_manipulator_frame(frame);
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}
} // namespace blcad::gui
