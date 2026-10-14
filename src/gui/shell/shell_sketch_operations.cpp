#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
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
    {Kind::Move, "move", "Verschieben"},
    {Kind::Rotate, "rotate", "Drehen"},
    {Kind::Scale, "scale", "Skalieren"},
    {Kind::Copy, "copy", "Kopieren"},
    {Kind::Mirror, "mirror", "Spiegeln"},
    {Kind::RectangularPattern, "rectangular_pattern", "Rechteckmuster"},
    {Kind::CircularPattern, "circular_pattern", "Kreismuster"},
    {Kind::Offset, "offset", "Versatz"},
}};

QString copy_report(const std::vector<std::string>& ids) {
  if (ids.empty())
    return QStringLiteral("Vorschau gültig. Übernehmen erstellt einen Undo-Schritt.");
  QStringList names;
  for (const auto& id : ids)
    names.push_back(QString::fromStdString(id));
  return QStringLiteral("Kopien erhalten keine eigenen Abhängigkeiten/Bemaßungen für: %1")
      .arg(names.join(QStringLiteral(", ")));
}
} // namespace

void ShellSketchTools::build_operation_group() {
  auto* group = ribbon_.add_group(sketch_tab_, QStringLiteral("Transformieren"));
  for (const auto& button : buttons) {
    auto* action = new QAction(QString::fromUtf8(button.label), this);
    action->setObjectName(QStringLiteral("sketch.operation.") + QString::fromUtf8(button.id));
    action->setToolTip(
        QStringLiteral("Geometrie auswählen, Werte eingeben und Vorschau übernehmen"));
    connect(action, &QAction::triggered, this,
            [this, kind = button.kind] { show_operation_dialog(kind); });
    ribbon_.add_small_button(group, action);
    operation_actions_.push_back(action);
  }
  auto* reference_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Projizieren"));
  project_action_ = new QAction(QStringLiteral("Geometrie projizieren"), this);
  project_action_->setObjectName(QStringLiteral("sketch.operation.project"));
  connect(project_action_, &QAction::triggered, this,
          [this] { show_operation_dialog(Kind::ProjectLine); });
  ribbon_.add_small_button(reference_group, project_action_);
  break_link_action_ = new QAction(QStringLiteral("Verknüpfung lösen"), this);
  break_link_action_->setObjectName(QStringLiteral("sketch.operation.break_link"));
  connect(break_link_action_, &QAction::triggered, this,
          [this] { show_operation_dialog(Kind::BreakLinkLine); });
  ribbon_.add_small_button(reference_group, break_link_action_);
}

Result<GuiSketchOperationRequest>
ShellSketchTools::prepare_operation(GuiSketchOperationRequest request) const {
  if (!sketch_context_ready())
    return Result<GuiSketchOperationRequest>::failure(
        Error::validation("sketch.operation", "keine aktive Skizze"));
  const auto& part = *session_.part_document();
  const auto& sketch = *part.find_sketch(*window_.active_sketch());
  if (request.entities.empty() && request.kind != Kind::ProjectLine &&
      request.kind != Kind::ProjectPoint) {
    if (request.kind == Kind::BreakLinkLine) {
      if (session_.selection().size() != 1U)
        return Result<GuiSketchOperationRequest>::failure(
            Error::validation("sketch.operation", "Genau eine projizierte Linie auswählen"));
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
      return Result<GuiSketchOperationRequest>::failure(Error::validation(
          "sketch.operation", "Eine projizierte Linie und eine positive Länge wählen"));
    const auto* reference = sketch.find_projected_line(request.entities.front());
    if (!reference)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation", "Auswahl ist keine projizierte Linie"));
    auto resolved = cache ? projector.resolve_line(part, sketch, *reference, *cache)
                          : projector.resolve_line(part, sketch, *reference);
    if (!resolved)
      return Result<GuiSketchOperationRequest>::failure(resolved.error());
    const auto p = resolved.value().point;
    const auto d = resolved.value().direction;
    const double length = std::hypot(d.x, d.y);
    if (length <= 1.0e-12)
      return Result<GuiSketchOperationRequest>::failure(
          Error::validation("sketch.operation", "Projizierte Linie hat keine Richtung"));
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
                     ? "Skizzenoperation übernommen."
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
        report_message("Genau eine projizierte Linie auswählen");
        return;
      }
      base.entities = selected_sketch_entity_ids();
    } else {
      auto selected = selected_operation_entities();
      if (!selected) {
        report(selected.error());
        return;
      }
      base.entities = std::move(selected.value());
    }
  }
  if (!projection && base.entities.empty()) {
    report_message("Zuerst die gewünschte Skizzengeometrie auswählen.");
    return;
  }
  QDialog dialog(&window_);
  dialog.setObjectName(QStringLiteral("sketch.operation.dialog"));
  QString title =
      projection ? QStringLiteral("Geometrie projizieren") : QStringLiteral("Verknüpfung lösen");
  for (const auto& b : buttons)
    if (kind == b.kind)
      title = QString::fromUtf8(b.label);
  dialog.setWindowTitle(title);
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  layout->addLayout(form);
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
      add(std::move(r), QStringLiteral("Punkt: ") + QString::fromStdString(point.name()));
    }
    for (const auto& line : part.construction_lines()) {
      GuiSketchOperationRequest r;
      r.kind = Kind::ProjectLine;
      auto reference = ProjectedSketchLine::create_from_construction_line(id, line.id());
      if (!reference)
        continue;
      r.projected_line = std::move(reference.value());
      add(std::move(r), QStringLiteral("Linie: ") + QString::fromStdString(line.name()));
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
    form->addRow(QStringLiteral("Quelle"), source_combo);
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
  if (!projection && kind != Kind::Offset && kind != Kind::BreakLinkLine) {
    const bool delta = kind == Kind::Move || kind == Kind::Copy || kind == Kind::RectangularPattern;
    const QString prefix = delta                  ? QStringLiteral("Versatz")
                           : kind == Kind::Mirror ? QStringLiteral("Achsenstart")
                                                  : QStringLiteral("Zentrum");
    x = number("x", prefix + QStringLiteral(" X (mm)"), delta ? 10.0 : 0.0);
    y = number("y", prefix + QStringLiteral(" Y (mm)"), 0.0);
  }
  if (kind == Kind::Mirror || kind == Kind::RectangularPattern) {
    const QString prefix =
        kind == Kind::Mirror ? QStringLiteral("Achsenende") : QStringLiteral("Zweiter Versatz");
    x2 = number("x2", prefix + QStringLiteral(" X (mm)"), 0.0);
    y2 = number("y2", prefix + QStringLiteral(" Y (mm)"), kind == Kind::Mirror ? 1.0 : 10.0);
  }
  if (kind == Kind::Rotate || kind == Kind::CircularPattern) {
    value = number("value", QStringLiteral("Winkel (°)"), kind == Kind::Rotate ? 90.0 : 360.0);
    value->setRange(-360.0, 360.0);
  }
  if (kind == Kind::Scale)
    value = number("value", QStringLiteral("Faktor"), 2.0);
  if (kind == Kind::Offset)
    value = number("value", QStringLiteral("Vorzeichenbehafteter Abstand (mm)"), 1.0);
  if (kind == Kind::BreakLinkLine)
    value = number("value", QStringLiteral("Länge der gelösten Linie (mm)"), 100.0);
  if (kind == Kind::RectangularPattern || kind == Kind::CircularPattern) {
    n1 = count("count", QStringLiteral("Anzahl einschließlich Ausgangselement"), 3);
    if (kind == Kind::RectangularPattern)
      n2 = count("count2", QStringLiteral("Anzahl zweite Richtung"), 2);
    auto* note =
        new QLabel(QStringLiteral("Das Muster erzeugt einzeln bearbeitbare Kopien."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
  }
  auto* status = new QLabel(&dialog);
  status->setObjectName(QStringLiteral("sketch.operation.status"));
  status->setWordWrap(true);
  layout->addWidget(status);
  auto* controls = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  controls->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Übernehmen"));
  controls->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
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
    if (projection && sources.empty()) {
      controls->button(QDialogButtonBox::Ok)->setEnabled(false);
      status->setText(QStringLiteral(
          "Keine projizierbaren Konstruktionspunkte, Linien oder Modellkanten vorhanden."));
      return;
    }
    auto preview = preview_operation(request());
    controls->button(QDialogButtonBox::Ok)->setEnabled(preview.has_value());
    status->setText(preview ? copy_report(preview.value().uncopied_references)
                            : QString::fromStdString(preview.error().message()));
  };
  if (!session_.task().begin("sketch.operation"))
    return;
  (void)session_.task().begin_parameter_editing();
  (void)session_.task().show_preview();
  refresh_enablement();
  for (auto* input : {x, y, x2, y2, value})
    if (input)
      connect(input, &QDoubleSpinBox::valueChanged, &dialog, update);
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
  update();
  (void)dialog.exec();
  if (session_.task().active())
    (void)session_.task().cancel();
  cancel_operation_preview();
  window_.refresh();
  publish_scene();
  arm_drag_controller();
  refresh_enablement();
}

} // namespace blcad::gui
