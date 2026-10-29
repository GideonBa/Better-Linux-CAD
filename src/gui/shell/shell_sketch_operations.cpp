#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <numbers>

namespace blcad::gui {
namespace {
using Kind = GuiSketchOperationKind;
struct OperationButton {
  Kind kind;
  const char* id;
  const char* label;
};
constexpr std::array<OperationButton, 8> buttons{{
    {Kind::Move, "move", "Move"},
    {Kind::Rotate, "rotate", "Rotate"},
    {Kind::Scale, "scale", "Scale"},
    {Kind::Copy, "copy", "Copy"},
    {Kind::Mirror, "mirror", "Mirror"},
    {Kind::RectangularPattern, "rectangular_pattern", "Rectangular Pattern"},
    {Kind::CircularPattern, "circular_pattern", "Circular Pattern"},
    {Kind::Offset, "offset", "Offset"},
}};

QString copy_report(const std::vector<std::string>& ids) {
  if (ids.empty())
    return QStringLiteral("Preview valid. Apply creates one undo step.");
  QStringList names;
  for (const auto& id : ids)
    names.push_back(QString::fromStdString(id));
  return QStringLiteral("Copies have no independent constraints/dimensions for: %1")
      .arg(names.join(QStringLiteral(", ")));
}
} // namespace

void ShellSketchTools::build_operation_group() {
  auto* group = ribbon_.add_group(sketch_tab_, QStringLiteral("Transform"));
  for (const auto& button : buttons) {
    auto* action = new QAction(QString::fromUtf8(button.label), this);
    action->setObjectName(QStringLiteral("sketch.operation.") + QString::fromUtf8(button.id));
    action->setToolTip(QStringLiteral("Select multiple elements, drag with the mouse or "
                                      "enter values and apply the preview"));
    connect(action, &QAction::triggered, this,
            [this, kind = button.kind] { show_operation_dialog(kind); });
    ribbon_.add_small_button(group, action);
    operation_actions_.push_back(action);
  }
  auto* reference_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Project"));
  project_action_ = new QAction(QStringLiteral("Project Geometry"), this);
  project_action_->setObjectName(QStringLiteral("sketch.operation.project"));
  connect(project_action_, &QAction::triggered, this,
          [this] { show_operation_dialog(Kind::ProjectLine); });
  ribbon_.add_small_button(reference_group, project_action_);
  break_link_action_ = new QAction(QStringLiteral("Detach Reference"), this);
  break_link_action_->setObjectName(QStringLiteral("sketch.operation.break_link"));
  connect(break_link_action_, &QAction::triggered, this,
          [this] { show_operation_dialog(Kind::BreakLinkLine); });
  ribbon_.add_small_button(reference_group, break_link_action_);
}

Result<GuiSketchOperationRequest>
ShellSketchTools::prepare_operation(GuiSketchOperationRequest request) const {
  if (!sketch_context_ready())
    return Result<GuiSketchOperationRequest>::failure(
        Error::validation("sketch.operation", "no active sketch"));
  const auto& part = *session_.part_document();
  const auto& sketch = *part.find_sketch(*window_.active_sketch());
  if (request.axis) {
    const auto* axis = sketch.find_line_segment(*request.axis);
    if (!axis)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation.axis", "Select a line as the symmetry axis"));
    request.first = axis->start();
    request.second = axis->end();
  }
  if (request.kind == Kind::ProjectGeometry) {
    if (!request.source_sketch || !request.source_entity || *request.source_sketch == sketch.id())
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation.source", "Select geometry from another sketch"));
    const auto* source = part.find_sketch(*request.source_sketch);
    if (!source)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation.source", "Source sketch no longer exists"));
    auto from = workbench_.plane_view(session_, source->id());
    auto to = workbench_.plane_view(session_, sketch.id());
    if (!from)
      return Result<GuiSketchOperationRequest>::failure(from.error());
    if (!to)
      return Result<GuiSketchOperationRequest>::failure(to.error());
    const auto project = [&](Point2 p) {
      return to.value().model_to_plane(from.value().plane_to_model(p));
    };
    request.projected_curves.clear();
    request.projected_splines.clear();
    std::size_t number = 1;
    const auto next_id = [&]() {
      for (;;) {
        SketchEntityId id("projected.curve." + std::to_string(number++));
        if (!sketch.find_line_segment(id) && !sketch.find_arc_segment(id) &&
            !sketch.find_spline_segment(id) && !sketch.find_projected_line(id) &&
            !sketch.find_projected_point(id) && !sketch.find_point(id) &&
            !sketch.find_reference_generated_line(id))
          return id;
      }
    };
    const auto append = [&](const SplineSegment& s) -> Result<std::size_t> {
      auto curve =
          SplineSegment::create_cubic_bezier(next_id(), project(s.start()), project(s.control1()),
                                             project(s.control2()), project(s.end()), s.weights());
      if (!curve)
        return Result<std::size_t>::failure(curve.error());
      request.projected_splines.push_back(std::move(curve.value()));
      return Result<std::size_t>::success(1);
    };
    if (const auto* line = source->find_line_segment(*request.source_entity)) {
      auto curve = LineSegment::create(next_id(), project(line->start()), project(line->end()),
                                       line->role());
      if (!curve)
        return Result<GuiSketchOperationRequest>::failure(curve.error());
      request.projected_curves.push_back(std::move(curve.value()));
    } else if (const auto* spline = source->find_spline_segment(*request.source_entity)) {
      auto added = append(*spline);
      if (!added)
        return Result<GuiSketchOperationRequest>::failure(added.error());
    } else {
      std::optional<detail::ArcGeometry> arc;
      if (const auto* curve = source->find_arc_segment(*request.source_entity))
        arc = detail::arc_geometry(curve->start(), curve->mid(), curve->end());
      else if (const auto* circle =
                   source->find_circle_profile(ProfileId(request.source_entity->value()))) {
        const auto* diameter = part.find_parameter(circle->diameter_parameter());
        if (diameter)
          arc = detail::ArcGeometry{circle->center(), diameter->value().millimeters() / 2, 0,
                                    2 * std::numbers::pi};
      }
      if (!arc)
        return Result<GuiSketchOperationRequest>::failure(
            Error::validation("sketch.operation.source", "Source is not a projectable curve"));
      const int spans =
          std::max(1, static_cast<int>(std::ceil(std::abs(arc->sweep) / (std::numbers::pi / 2))));
      for (int i = 0; i < spans; ++i) {
        auto span = SplineSegment::create_elliptical_arc(
            SketchEntityId("span"), arc->center, {arc->radius, 0}, arc->radius,
            arc->start_angle + arc->sweep * i / spans, arc->sweep / spans);
        if (!span)
          return Result<GuiSketchOperationRequest>::failure(span.error());
        auto added = append(span.value());
        if (!added)
          return Result<GuiSketchOperationRequest>::failure(added.error());
      }
    }
    return Result<GuiSketchOperationRequest>::success(std::move(request));
  }
  if (request.entities.empty() && request.kind != Kind::ProjectLine &&
      request.kind != Kind::ProjectPoint) {
    if (request.kind == Kind::BreakLinkLine) {
      if (session_.selection().size() != 1U)
        return Result<GuiSketchOperationRequest>::failure(
            Error::validation("sketch.operation", "Select exactly one projected line"));
      request.entities = selected_sketch_entity_ids();
    } else {
      auto selected = selected_operation_entities();
      if (!selected)
        return Result<GuiSketchOperationRequest>::failure(selected.error());
      request.entities = std::move(selected.value());
    }
  }
  geometry::SketchReferenceProjector projector;
  const auto* cache = session_.part_shape_cache();
  if (request.kind == Kind::ProjectPoint && request.projected_point) {
    auto resolved = cache ? projector.resolve_point(part, sketch, *request.projected_point, *cache)
                          : projector.resolve_point(part, sketch, *request.projected_point);
    if (!resolved)
      return Result<GuiSketchOperationRequest>::failure(resolved.error());
  }
  if (request.kind == Kind::ProjectLine && request.projected_line) {
    auto resolved = cache ? projector.resolve_line(part, sketch, *request.projected_line, *cache)
                          : projector.resolve_line(part, sketch, *request.projected_line);
    if (!resolved)
      return Result<GuiSketchOperationRequest>::failure(resolved.error());
  }
  if (request.kind == Kind::BreakLinkLine) {
    if (request.entities.size() != 1U || !std::isfinite(request.value) || request.value <= 0)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation", "Select a projected line and a positive length"));
    const auto* reference = sketch.find_projected_line(request.entities.front());
    if (!reference)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation", "Selection is not a projected line"));
    auto resolved = cache ? projector.resolve_line(part, sketch, *reference, *cache)
                          : projector.resolve_line(part, sketch, *reference);
    if (!resolved)
      return Result<GuiSketchOperationRequest>::failure(resolved.error());
    const auto p = resolved.value().point;
    const auto d = resolved.value().direction;
    const double length = std::hypot(d.x, d.y);
    if (length <= 1.0e-12)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation", "Projected line has no direction"));
    const double half = request.value / (2.0 * length);
    request.first = {p.x - d.x * half, p.y - d.y * half};
    request.second = {p.x + d.x * half, p.y + d.y * half};
  }
  return Result<GuiSketchOperationRequest>::success(std::move(request));
}

Result<GuiSketchOperationPreview>
ShellSketchTools::preview_operation(GuiSketchOperationRequest request) {
  auto prepared = prepare_operation(std::move(request));
  if (!prepared) {
    cancel_operation_preview();
    return Result<GuiSketchOperationPreview>::failure(prepared.error());
  }
  auto preview =
      GuiSketchOperationController::preview(session_, *window_.active_sketch(), prepared.value());
  if (!preview) {
    cancel_operation_preview();
    return preview;
  }
  // Check document graph validity (including associative projection cycles) before displaying a
  // candidate.
  PartDocument document = *session_.part_document();
  auto updated = document.update_sketch(preview.value().candidate);
  if (!updated) {
    cancel_operation_preview();
    return Result<GuiSketchOperationPreview>::failure(updated.error());
  }
  operation_preview_active_ = true;
  viewport_.set_sketch_drag_handles({});
  viewport_.set_sketch_selection_enabled(false);
  publish_scene_for(preview.value().candidate, false);
  auto scene = session_.part_shape_cache()
                   ? GuiSketchInteractionSceneBuilder{}.build(document, preview.value().candidate,
                                                              *session_.part_shape_cache())
                   : GuiSketchInteractionSceneBuilder{}.build(document, preview.value().candidate);
  if (!scene) {
    cancel_operation_preview();
    return Result<GuiSketchOperationPreview>::failure(scene.error());
  }
  auto changed = preview.value().created_entities;
  if (changed.empty())
    changed = prepared.value().entities;
  std::vector<std::vector<Point2>> curves;
  for (const auto& curve : scene.value().curves) {
    const bool selected = std::any_of(changed.begin(), changed.end(), [&](const auto& id) {
      return curve.semantic_id ==
             "sketch/" + preview.value().candidate.id().value() + "/entity/" + id.value();
    });
    if (selected) {
      auto points = curve.polyline;
      if (curve.closed && !points.empty())
        points.push_back(points.front());
      curves.push_back(std::move(points));
    }
  }
  viewport_.set_sketch_operation_preview(std::move(curves));
  return preview;
}

Result<std::size_t> ShellSketchTools::apply_operation(GuiSketchOperationRequest request) {
  auto prepared = prepare_operation(std::move(request));
  if (!prepared)
    return Result<std::size_t>::failure(prepared.error());
  auto preview =
      GuiSketchOperationController::preview(session_, *window_.active_sketch(), prepared.value());
  if (!preview)
    return Result<std::size_t>::failure(preview.error());
  auto committed =
      GuiSketchOperationController::commit(session_, *window_.active_sketch(), prepared.value());
  if (!committed)
    return committed;
  operation_preview_active_ = false;
  viewport_.set_sketch_operation_preview({});
  viewport_.set_sketch_selection_enabled(true);
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  report_message(preview.value().uncopied_references.empty()
                     ? "Sketch operation applied."
                     : copy_report(preview.value().uncopied_references).toStdString());
  return committed;
}

void ShellSketchTools::cancel_operation_preview() {
  if (!operation_preview_active_)
    return;
  operation_preview_active_ = false;
  viewport_.set_sketch_operation_preview({});
  viewport_.set_sketch_selection_enabled(true);
  publish_scene();
  arm_drag_controller();
}

void ShellSketchTools::show_operation_dialog(Kind kind) {
  if (!sketch_context_ready() || session_.task().active())
    return;
  cancel_tool();
  GuiSketchOperationRequest base;
  base.kind = kind;
  const bool projection = kind == Kind::ProjectLine || kind == Kind::ProjectPoint;
  if (!projection) {
    if (kind == Kind::BreakLinkLine) {
      if (session_.selection().size() != 1U) {
        report_message("Select exactly one projected line");
        return;
      }
      base.entities = selected_sketch_entity_ids();
    } else {
      auto selected = selected_operation_entities();
      if (!selected) {
        if (kind != Kind::Symmetric) {
          report(selected.error());
          return;
        }
        session_.selection().clear();
        viewport_.clear_selection();
      } else
        base.entities = std::move(selected.value());
    }
  }

  QDialog dialog(&window_);
  dialog.setObjectName(QStringLiteral("sketch.operation.dialog"));
  QString title =
      projection ? QStringLiteral("Project Geometry") : QStringLiteral("Detach Reference");
  for (const auto& b : buttons)
    if (kind == b.kind)
      title = QString::fromUtf8(b.label);
  if (kind == Kind::Symmetric)
    title = QStringLiteral("Symmetric");
  dialog.setWindowTitle(title);
  dialog.setWindowModality(Qt::NonModal);
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  layout->addLayout(form);
  enum class PickMode { Elements, Axis, Center, Drag };
  const bool has_axis = kind == Kind::Mirror || kind == Kind::Symmetric;
  PickMode mode =
      has_axis ? PickMode::Axis : (base.entities.empty() ? PickMode::Elements : PickMode::Drag);
  QPushButton *elements_button = nullptr, *axis_button = nullptr, *center_button = nullptr,
              *drag_button = nullptr;
  if (!projection && kind != Kind::BreakLinkLine) {
    elements_button = make_gui_button(QStringLiteral("Select Elements"), &dialog);
    elements_button->setObjectName(QStringLiteral("sketch.operation.elements"));
    layout->addWidget(elements_button);
    if (has_axis) {
      axis_button = make_gui_button(QStringLiteral("Select Symmetry Axis"), &dialog);
      axis_button->setObjectName(QStringLiteral("sketch.operation.axis"));
      layout->insertWidget(0, axis_button);
      auto* note = new QLabel(
          kind == Kind::Symmetric
              ? QStringLiteral(
                    "Click the axis, then select elements. Curves are paired with their nearest "
                    "counterpart; a single line becomes symmetric about its midpoint.")
              : QStringLiteral("Click the axis, then select all elements to mirror."),
          &dialog);
      note->setWordWrap(true);
      layout->addWidget(note);
    } else {
      if (kind == Kind::Rotate || kind == Kind::Scale || kind == Kind::CircularPattern) {
        center_button = make_gui_button(QStringLiteral("Pick Center in Viewport"), &dialog);
        center_button->setObjectName(QStringLiteral("sketch.operation.center"));
        layout->addWidget(center_button);
      }
      drag_button = make_gui_button(QStringLiteral("Drag with Mouse"), &dialog);
      drag_button->setObjectName(QStringLiteral("sketch.operation.drag"));
      layout->addWidget(drag_button);
    }
  }
  std::vector<GuiSketchOperationRequest> sources;
  QComboBox* source_combo = nullptr;
  if (projection) {
    source_combo = new QComboBox(&dialog);
    source_combo->setObjectName(QStringLiteral("sketch.operation.source"));
    const auto& part = *session_.part_document();
    const auto& sketch = *part.find_sketch(*window_.active_sketch());
    std::size_t number = 1;
    while (
        sketch.find_projected_line(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_projected_point(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_line_segment(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_arc_segment(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_spline_segment(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_point(SketchEntityId("projected." + std::to_string(number))) ||
        sketch.find_reference_generated_line(SketchEntityId("projected." + std::to_string(number))))
      ++number;
    const SketchEntityId id("projected." + std::to_string(number));
    const auto add = [&](GuiSketchOperationRequest r, const QString& name) {
      if (!prepare_operation(r))
        return;
      source_combo->addItem(name);
      sources.push_back(std::move(r));
    };
    for (const auto& point : part.construction_points()) {
      GuiSketchOperationRequest r;
      r.kind = Kind::ProjectPoint;
      auto reference = ProjectedSketchPoint::create_from_construction_point(id, point.id());
      if (!reference)
        continue;
      r.projected_point = std::move(reference.value());
      add(std::move(r), QStringLiteral("Point: ") + QString::fromStdString(point.name()));
    }
    for (const auto& line : part.construction_lines()) {
      GuiSketchOperationRequest r;
      r.kind = Kind::ProjectLine;
      auto reference = ProjectedSketchLine::create_from_construction_line(id, line.id());
      if (!reference)
        continue;
      r.projected_line = std::move(reference.value());
      add(std::move(r), QStringLiteral("Line: ") + QString::fromStdString(line.name()));
    }
    for (const auto& source : part.sketches()) {
      if (source.id() == sketch.id())
        continue;
      const auto add_curve_source = [&](const auto& curve) {
        GuiSketchOperationRequest r;
        r.kind = Kind::ProjectGeometry;
        r.source_sketch = source.id();
        r.source_entity = SketchEntityId(curve.id().value());
        add(std::move(r), QString::fromStdString(source.name() + " / " + curve.id().value()));
      };
      for (const auto& curve : source.line_segments())
        add_curve_source(curve);
      for (const auto& curve : source.arc_segments())
        add_curve_source(curve);
      for (const auto& curve : source.spline_segments())
        add_curve_source(curve);
      for (const auto& curve : source.circle_profiles())
        add_curve_source(curve);
    }
    // Semantic references remain stable across recompute; only currently
    // resolvable feature edges/vertices are offered by the source chooser.
    if (const auto* cache = session_.part_shape_cache())
      for (const auto& feature : cache->feature_shapes()) {
        for (int e = 0; e < 12; ++e) {
          auto edge =
              SemanticEdgeReference::create(feature.feature_id, static_cast<SemanticEdge>(e));
          if (!edge)
            continue;
          auto reference = ProjectedSketchLine::create_from_semantic_edge(id, edge.value());
          if (!reference)
            continue;
          GuiSketchOperationRequest r;
          r.kind = Kind::ProjectLine;
          r.projected_line = std::move(reference.value());
          add(std::move(r), QString::fromStdString(edge.value().node_id()));
        }
        for (int v = 0; v < 8; ++v) {
          auto vertex =
              SemanticVertexReference::create(feature.feature_id, static_cast<SemanticVertex>(v));
          if (!vertex)
            continue;
          auto reference = ProjectedSketchPoint::create_from_semantic_vertex(id, vertex.value());
          if (!reference)
            continue;
          GuiSketchOperationRequest r;
          r.kind = Kind::ProjectPoint;
          r.projected_point = std::move(reference.value());
          add(std::move(r), QString::fromStdString(vertex.value().node_id()));
        }
      }
    form->addRow(QStringLiteral("Source"), source_combo);
  }
  const auto number = [&](const char* name, const QString& label, double initial) {
    auto* input = new QDoubleSpinBox(&dialog);
    input->setObjectName(QStringLiteral("sketch.operation.") + QString::fromUtf8(name));
    input->setDecimals(6);
    input->setRange(-1.0e6, 1.0e6);
    input->setValue(initial);
    form->addRow(label, input);
    return input;
  };
  const auto count = [&](const char* name, const QString& label, int initial) {
    auto* input = new QSpinBox(&dialog);
    input->setObjectName(QStringLiteral("sketch.operation.") + QString::fromUtf8(name));
    input->setRange(1, 10000);
    input->setValue(initial);
    form->addRow(label, input);
    return input;
  };
  QDoubleSpinBox *x = nullptr, *y = nullptr, *x2 = nullptr, *y2 = nullptr, *value = nullptr;
  QSpinBox *n1 = nullptr, *n2 = nullptr;
  if (!projection && kind != Kind::Offset && kind != Kind::BreakLinkLine &&
      kind != Kind::Symmetric) {
    const bool delta = kind == Kind::Move || kind == Kind::Copy || kind == Kind::RectangularPattern;
    const QString prefix = delta                  ? QStringLiteral("Offset")
                           : kind == Kind::Mirror ? QStringLiteral("Axis Start")
                                                  : QStringLiteral("Center");
    x = number("x", prefix + QStringLiteral(" X (mm)"), delta ? 10.0 : 0.0);
    y = number("y", prefix + QStringLiteral(" Y (mm)"), 0.0);
  }
  if (kind == Kind::Mirror || kind == Kind::RectangularPattern) {
    const QString prefix =
        kind == Kind::Mirror ? QStringLiteral("Axis End") : QStringLiteral("Second Offset");
    x2 = number("x2", prefix + QStringLiteral(" X (mm)"), 0.0);
    y2 = number("y2", prefix + QStringLiteral(" Y (mm)"), kind == Kind::Mirror ? 1.0 : 10.0);
  }
  if (kind == Kind::Rotate || kind == Kind::CircularPattern) {
    value = number("value", QStringLiteral("Angle (°)"), kind == Kind::Rotate ? 90.0 : 360.0);
    value->setRange(-360.0, 360.0);
  }
  if (kind == Kind::Scale)
    value = number("value", QStringLiteral("Factor"), 2.0);
  if (kind == Kind::Offset)
    value = number("value", QStringLiteral("Signed Distance (mm)"), 1.0);
  if (kind == Kind::BreakLinkLine)
    value = number("value", QStringLiteral("Detached Line Length (mm)"), 100.0);
  if (kind == Kind::RectangularPattern || kind == Kind::CircularPattern) {
    n1 = count("count", QStringLiteral("Count Including Source"), 3);
    if (kind == Kind::RectangularPattern)
      n2 = count("count2", QStringLiteral("Second Direction Count"), 2);
    auto* note =
        new QLabel(QStringLiteral("The pattern creates individually editable copies."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
  }
  auto* status = new QLabel(&dialog);
  status->setObjectName(QStringLiteral("sketch.operation.status"));
  status->setWordWrap(true);
  layout->addWidget(status);
  auto* controls = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  controls->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Apply"));
  controls->button(QDialogButtonBox::Ok)->setProperty("guiRole", "primary");
  controls->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancel"));
  apply_gui_dialog_icons(*controls);
  layout->addWidget(controls);
  const auto request = [&] {
    auto r = base;
    if (source_combo && source_combo->currentIndex() >= 0)
      r = sources[static_cast<std::size_t>(source_combo->currentIndex())];
    if (x)
      r.first = {x->value(), y->value()};
    if (x2)
      r.second = {x2->value(), y2->value()};
    if (value)
      r.value = value->value() * ((kind == Kind::Rotate || kind == Kind::CircularPattern)
                                      ? std::numbers::pi / 180.0
                                      : 1.0);
    if (n1)
      r.first_count = static_cast<std::size_t>(n1->value());
    if (n2)
      r.second_count = static_cast<std::size_t>(n2->value());
    return r;
  };
  const auto update = [&] {
    if (!projection && kind != Kind::BreakLinkLine &&
        (base.entities.empty() || (kind == Kind::Symmetric && !base.axis))) {
      cancel_operation_preview();
      viewport_.set_sketch_selection_enabled(mode == PickMode::Elements);
      controls->button(QDialogButtonBox::Ok)->setEnabled(false);
      status->setText(has_axis && !base.axis
                          ? QStringLiteral("Click the symmetry axis in the viewport.")
                          : QStringLiteral("Click elements individually or use a selection box."));
      return;
    }
    if (!projection && kind != Kind::BreakLinkLine) {
      // Still validate the numeric candidate while leaving the source hit scene intact.
      auto prepared = prepare_operation(request());
      auto preview = prepared ? GuiSketchOperationController::preview(
                                    session_, *window_.active_sketch(), prepared.value())
                              : Result<GuiSketchOperationPreview>::failure(prepared.error());
      cancel_operation_preview();
      viewport_.set_sketch_selection_enabled(mode == PickMode::Elements);
      if (preview) {
        // Publish only the result polylines as an overlay; preserve original picks and selection
        // box.
        auto scene = GuiSketchInteractionSceneBuilder{}.build(*session_.part_document(),
                                                              preview.value().candidate);
        std::vector<std::vector<Point2>> curves;
        const auto& changed = preview.value().created_entities.empty()
                                  ? base.entities
                                  : preview.value().created_entities;
        if (scene)
          for (const auto& curve : scene.value().curves) {
            for (const auto& id : changed)
              if (curve.semantic_id ==
                  "sketch/" + preview.value().candidate.id().value() + "/entity/" + id.value()) {
                auto points = curve.polyline;
                if (curve.closed && !points.empty())
                  points.push_back(points.front());
                curves.push_back(std::move(points));
                break;
              }
          }
        viewport_.set_sketch_operation_preview(std::move(curves));
      } else
        viewport_.set_sketch_operation_preview({});
      controls->button(QDialogButtonBox::Ok)->setEnabled(preview.has_value());
      if (preview) {
        auto message = QStringLiteral("%1 elements selected. Drag or enter values; "
                                      "Apply confirms the preview.")
                           .arg(base.entities.size());
        if (!preview.value().uncopied_references.empty())
          message += QStringLiteral("\n") + copy_report(preview.value().uncopied_references);
        status->setText(message);
      } else
        status->setText(QString::fromStdString(preview.error().message()));
      return;
    }
    if (projection && sources.empty()) {
      controls->button(QDialogButtonBox::Ok)->setEnabled(false);
      status->setText(QStringLiteral("No sources available. First create another sketch, "
                                     "construction geometry, or a model body."));
      return;
    }
    auto preview = preview_operation(request());
    controls->button(QDialogButtonBox::Ok)->setEnabled(preview.has_value());
    status->setText(preview
                        ? (projection && request().kind == Kind::ProjectGeometry
                               ? QStringLiteral("Sketch curve is copied as an editable projection "
                                                "without an associative reference.")
                               : copy_report(preview.value().uncopied_references))
                        : QString::fromStdString(preview.error().message()));
  };
  if (!session_.task().begin("sketch.operation"))
    return;
  (void)session_.task().begin_parameter_editing();
  (void)session_.task().show_preview();
  refresh_enablement();
  for (auto* input : {x, y, x2, y2, value})
    if (input)
      connect(input, &QDoubleSpinBox::valueChanged, &dialog, [&] {
        if (kind == Kind::Mirror) {
          base.axis.reset();
          mode = PickMode::Drag;
        }
        update();
      });
  for (auto* input : {n1, n2})
    if (input)
      connect(input, &QSpinBox::valueChanged, &dialog, update);
  if (source_combo)
    connect(source_combo, &QComboBox::currentIndexChanged, &dialog, update);
  connect(controls, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(controls, &QDialogButtonBox::accepted, &dialog, [&] {
    auto applied = apply_operation(request());
    if (!applied) {
      status->setText(QString::fromStdString(applied.error().message()));
      return;
    }
    (void)session_.task().apply();
    dialog.accept();
  });
  std::optional<Point2> anchor;
  bool updating_pointer = false;
  close_operation_ = [&] { dialog.reject(); };
  const auto pick_mode = [&](PickMode next) {
    mode = next;
    anchor.reset();
    cancel_operation_preview();
    viewport_.set_sketch_operation_preview({});
    viewport_.set_sketch_selection_enabled(mode == PickMode::Elements);
    viewport_.set_sketch_additive_selection(mode == PickMode::Elements);
    viewport_.set_sketch_drag_handles({});
    if (elements_button)
      elements_button->setText(QStringLiteral("Select Elements (%1)").arg(base.entities.size()));
    update();
    if (next == PickMode::Center)
      status->setText(QStringLiteral("Click the center in the viewport."));
    if (next == PickMode::Axis)
      status->setText(QStringLiteral("Click the symmetry axis in the viewport."));
    viewport_.setFocus();
  };
  if (elements_button)
    connect(elements_button, &QPushButton::clicked, &dialog,
            [&] { pick_mode(PickMode::Elements); });
  if (axis_button)
    connect(axis_button, &QPushButton::clicked, &dialog, [&] { pick_mode(PickMode::Axis); });
  if (center_button)
    connect(center_button, &QPushButton::clicked, &dialog, [&] { pick_mode(PickMode::Center); });
  if (drag_button)
    connect(drag_button, &QPushButton::clicked, &dialog, [&] { pick_mode(PickMode::Drag); });
  operation_selection_ = [&] {
    if (mode != PickMode::Elements)
      return;
    auto selected = selected_operation_entities();
    if (!selected) {
      base.entities.clear();
      viewport_.set_sketch_operation_preview({});
      controls->button(QDialogButtonBox::Ok)->setEnabled(false);
      status->setText(QString::fromStdString(selected.error().message()));
      return;
    }
    base.entities = std::move(selected.value());
    if (base.axis)
      std::erase(base.entities, *base.axis);
    if (elements_button)
      elements_button->setText(QStringLiteral("Select Elements (%1)").arg(base.entities.size()));
    update();
  };
  operation_phase_ = [&](GuiSketchPointerPhase phase, Point2 point,
                         const std::optional<GuiSketchHit>& hit) {
    if (projection || kind == Kind::BreakLinkLine)
      return;
    if (phase == GuiSketchPointerPhase::Release) {
      anchor.reset();
      return;
    }
    if (mode == PickMode::Axis) {
      if (!hit)
        return;
      const GuiSelection selection(GuiSelectionKind::SketchEntity, hit->semantic_id);
      const auto marker = selection.semantic_id.rfind("entity/");
      if (marker == std::string::npos)
        return;
      const SketchEntityId id(selection.semantic_id.substr(marker + 7));
      const auto* axis =
          session_.part_document()->find_sketch(*window_.active_sketch())->find_line_segment(id);
      if (!axis) {
        status->setText(QStringLiteral("Select a sketch line as the axis."));
        return;
      }
      base.axis = id;
      base.first = axis->start();
      base.second = axis->end();
      std::erase(base.entities, id);
      if (x) {
        const QSignalBlocker bx(x), by(y), bx2(x2), by2(y2);
        x->setValue(base.first.x);
        y->setValue(base.first.y);
        x2->setValue(base.second.x);
        y2->setValue(base.second.y);
      }
      axis_button->setText(QStringLiteral("Symmetry axis: ") + QString::fromStdString(id.value()));
      pick_mode(PickMode::Elements);
    } else if (mode == PickMode::Center && x && y) {
      {
        const QSignalBlocker bx(x), by(y);
        x->setValue(point.x);
        y->setValue(point.y);
      }
      pick_mode(PickMode::Drag);
    } else if (mode == PickMode::Drag)
      anchor = point;
  };
  operation_pointer_ = [&](Point2 point) {
    if (!anchor || mode != PickMode::Drag || updating_pointer)
      return;
    updating_pointer = true;
    const Point2 delta{point.x - anchor->x, point.y - anchor->y};
    if (kind == Kind::Move || kind == Kind::Copy || kind == Kind::RectangularPattern) {
      {
        const QSignalBlocker bx(x), by(y);
        x->setValue(delta.x);
        y->setValue(delta.y);
      }
    } else if ((kind == Kind::Rotate || kind == Kind::CircularPattern) && value) {
      const double a = std::atan2(anchor->y - y->value(), anchor->x - x->value());
      const double b = std::atan2(point.y - y->value(), point.x - x->value());
      const QSignalBlocker block(value);
      value->setValue(std::remainder(b - a, 2 * std::numbers::pi) * 180 / std::numbers::pi);
    } else if (kind == Kind::Scale && value) {
      const double before = std::hypot(anchor->x - x->value(), anchor->y - y->value());
      const QSignalBlocker block(value);
      if (before > 1e-9)
        value->setValue(std::hypot(point.x - x->value(), point.y - y->value()) / before);
    } else if (kind == Kind::Offset && value && !base.entities.empty()) {
      const auto* sketch = session_.part_document()->find_sketch(*window_.active_sketch());
      const QSignalBlocker block(value);
      if (const auto* line = sketch->find_line_segment(base.entities.front())) {
        const Point2 d{line->end().x - line->start().x, line->end().y - line->start().y};
        value->setValue((d.x * delta.y - d.y * delta.x) / std::hypot(d.x, d.y));
      } else if (const auto* arc = sketch->find_arc_segment(base.entities.front())) {
        auto circle = detail::arc_geometry(arc->start(), arc->mid(), arc->end());
        if (circle)
          value->setValue((std::hypot(point.x - circle->center.x, point.y - circle->center.y) -
                           circle->radius) *
                          (circle->sweep > 0 ? -1 : 1));
      }
    }
    update();
    updating_pointer = false;
  };
  viewport_.set_sketch_additive_selection(mode == PickMode::Elements);
  viewport_.set_sketch_selection_enabled(mode == PickMode::Elements);
  update();
  // A local event loop preserves the synchronous command/test surface while the dialog
  // remains nonmodal: the viewport continues to receive mouse and keyboard events.
  QEventLoop loop;
  connect(&dialog, &QDialog::finished, &loop, &QEventLoop::quit);
  dialog.show();
  loop.exec();
  operation_phase_ = {};
  operation_pointer_ = {};
  operation_selection_ = {};
  close_operation_ = {};
  viewport_.set_sketch_additive_selection(false);
  viewport_.set_sketch_operation_preview({});
  viewport_.set_sketch_selection_enabled(true);
  if (session_.task().active())
    (void)session_.task().cancel();
  cancel_operation_preview();
  window_.refresh();
  publish_scene();
  arm_drag_controller();
  refresh_enablement();
}

} // namespace blcad::gui
