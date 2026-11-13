#include "blcad/gui/shell/shell_sketch_curve_tools.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/planar_curve_differential.hpp"
#include "blcad/geometry/equation_curve_presentation.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_presentation_visibility.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace blcad::gui {
namespace {
constexpr const char* equation_task = "sketch.equation";
constexpr const char* smooth_task = "sketch.smooth";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation("sketch.curve.authoring", std::move(message));
}
std::string input(const QLineEdit* field) {
  return field->text().trimmed().toStdString();
}
QString vector(Point2 p) {
  return QStringLiteral("(%1, %2)").arg(p.x, 0, 'g', 8).arg(p.y, 0, 'g', 8);
}
double magnitude(Point2 p) {
  return std::hypot(p.x, p.y);
}
} // namespace
struct ShellSketchCurveTools::Impl {
  ShellWindow& window;
  ShellSketchTools& parent;
  GuiSketchWorkspace& workspace;
  QAction *equation{}, *edit{}, *smooth{};
  QDockWidget* dock{};
  QScrollArea* scroll{};
  QWidget *equation_fields{}, *smooth_fields{};
  QComboBox *mode{}, *unit{}, *parameter{}, *first{}, *second{}, *first_end{}, *second_end{};
  QLineEdit *variable{}, *lower{}, *upper{}, *x{}, *y{};
  QLabel *x_label{}, *y_label{}, *summary{}, *differential{};
  QLineEdit* tolerance{};
  QCheckBox *closed{}, *construction{};
  QPushButton *apply{}, *preview{}, *fit{};
  QLineEdit* insertion_target{};
  std::optional<SketchId> sketch;
  std::optional<SketchEntityId> source;
  std::optional<SketchConstraintId> constraint;
  std::optional<GuiViewportCameraBookmark> camera;
  mutable std::size_t revision{};
  std::string source_snapshot;
  std::vector<SketchConstraintCatalog> source_constraints;
  std::vector<SketchDimensionCatalog> source_dimensions;
  std::optional<std::size_t> source_prefix;
  void capture_source() {
    const auto& session = window.session();
    source_snapshot = serialize_part_document_to_json(*session.part_document()).value();
    source_constraints = session.sketch_constraint_catalogs();
    source_dimensions = session.sketch_dimension_catalogs();
    source_prefix = session.history_prefix_count();
    revision = session.model_revision();
  }
  bool smooth_mode{}, editing{}, previewed{}, filling{}, selection_enabled{true};
  Impl(ShellWindow& w, ShellSketchTools& p, GuiSketchWorkspace& ws)
      : window(w), parent(p), workspace(ws) {}
  const char* command() const {
    return smooth_mode ? smooth_task : equation_task;
  }
  const Sketch* current_sketch() const {
    return sketch && window.session().part_document()
               ? window.session().part_document()->find_sketch(*sketch)
               : nullptr;
  }
  bool current() const {
    const auto& session = window.session();
    if (!sketch || window.active_sketch() != sketch || !current_sketch() ||
        session.task().command_id() != command())
      return false;
    if (session.model_revision() == revision)
      return true;
    // Diagnostics share the session revision. Only a complete authoritative
    // definition/catalog change invalidates this editor, not its own errors.
    const auto snapshot = serialize_part_document_to_json(*session.part_document());
    if (!snapshot || snapshot.value() != source_snapshot ||
        source_constraints != session.sketch_constraint_catalogs() ||
        source_dimensions != session.sketch_dimension_catalogs() ||
        source_prefix != session.history_prefix_count())
      return false;
    revision = session.model_revision();
    return true;
  }
  bool available() const {
    const auto& session = window.session();
    return window.active_sketch() && session.part_document() &&
           !session.part_document()->is_reference_part() && !session.reference_repair_mode() &&
           workspace.active() && !session.task().active() &&
           (workspace.stage() == GuiSketchInteractionStage::Idle ||
            workspace.stage() == GuiSketchInteractionStage::Hover);
  }
  bool can_smooth() const {
    const auto* document = window.session().part_document();
    const auto* selected = window.active_sketch() && document
                               ? document->find_sketch(*window.active_sketch())
                               : nullptr;
    return selected && !selected->spline_segments().empty() &&
           selected->spline_segments().size() + selected->line_segments().size() +
                   selected->arc_segments().size() >=
               2;
  }
  std::optional<SketchEntityId> selected_equation() const {
    if (!window.active_sketch() || !window.session().part_document())
      return {};
    const auto* selected = window.session().part_document()->find_sketch(*window.active_sketch());
    if (!selected)
      return {};
    const auto prefix = "sketch/" + selected->id().value() + "/entity/";
    for (const auto& item : window.session().selection().entries())
      if (item.kind == GuiSelectionKind::SketchEntity && item.semantic_id.starts_with(prefix)) {
        SketchEntityId id(item.semantic_id.substr(prefix.size()));
        if (selected->find_equation_curve(id))
          return id;
      }
    return {};
  }
  void invalidate() {
    if (filling || !sketch)
      return;
    if (previewed)
      parent.publish_scene();
    previewed = false;
    apply->setEnabled(false);
    fit->setEnabled(false);
    window.viewport().clear_feature_preview();
    window.viewport().set_sketch_dimension_preview(std::nullopt);
    if (workspace.stage() == GuiSketchInteractionStage::Preview &&
        window.session().task().command_id() == command())
      (void)workspace.escape(window.session());
    summary->setText(QStringLiteral("Preview the current definition."));
    differential->clear();
  }
  void mode_changed(bool defaults) {
    const int kind = mode->currentIndex();
    x_label->setText(kind == 1 ? QStringLiteral("Radius (mm)") : QStringLiteral("X (mm)"));
    y_label->setText(kind == 0 ? QStringLiteral("Y (mm)") : QStringLiteral("Y (mm)"));
    x_label->setVisible(kind != 0);
    x->setVisible(kind != 0);
    y_label->setVisible(kind != 1);
    y->setVisible(kind != 1);
    unit->setEnabled(kind == 2);
    closed->setEnabled(kind != 0);
    if (defaults) {
      filling = true;
      variable->setText("u");
      unit->setCurrentIndex(kind == 0 ? 1 : kind == 1 ? 2 : 0);
      lower->setText(kind == 0 ? "0mm" : kind == 1 ? "0deg" : "0");
      upper->setText(kind == 0 ? "10mm" : kind == 1 ? "360deg" : "2*pi");
      x->setText(kind == 1 ? "10mm" : "10mm*cos(u)");
      y->setText(kind == 0 ? "u*u/10mm" : "10mm*sin(u)");
      closed->setChecked(kind != 0);
      filling = false;
    }
    insertion_target = kind == 0 ? y : x;
    invalidate();
  }
  Result<EquationCurve> definition() {
    bool tolerance_ok = false;
    const double tolerance_value = tolerance->text().trimmed().toDouble(&tolerance_ok);
    if (!tolerance_ok || !std::isfinite(tolerance_value) || tolerance_value <= 0)
      return Result<EquationCurve>::failure(
          error("Modeling tolerance must be a positive finite value in mm."));
    const auto kind = mode->currentIndex() == 0   ? EquationCurveMode::Cartesian2D
                      : mode->currentIndex() == 1 ? EquationCurveMode::Polar2D
                                                  : EquationCurveMode::Parametric2D;
    const auto variable_unit = unit->currentIndex() == 0   ? CurveUnit::Scalar
                               : unit->currentIndex() == 1 ? CurveUnit::Length
                                                           : CurveUnit::Angle;
    std::array<std::string, 3> coordinates;
    if (kind == EquationCurveMode::Cartesian2D)
      coordinates[1] = input(y);
    else {
      coordinates[0] = input(x);
      if (kind == EquationCurveMode::Parametric2D)
        coordinates[1] = input(y);
    }
    return EquationCurve::create(*source, kind, {input(variable), variable_unit}, input(lower),
                                 input(upper), std::move(coordinates), tolerance_value,
                                 closed->isChecked() ? EquationCurveClosure::Periodic
                                                     : EquationCurveClosure::Open,
                                 construction->isChecked());
  }
  Result<SketchConstraintIntent> smooth_intent(const GuiDocumentSession& session) const {
    const auto* selected = session.part_document()->find_sketch(*sketch);
    if (!selected)
      return Result<SketchConstraintIntent>::failure(
          error("Smooth source sketch no longer exists."));
    auto topology = SketchTopology::migrate_legacy(*selected);
    if (!topology)
      return Result<SketchConstraintIntent>::failure(topology.error());
    std::vector<SketchConstraintIntentTarget> targets;
    for (const auto& [curve, endpoint] :
         {std::pair{first, first_end}, std::pair{second, second_end}}) {
      const std::string id = "entity/" + curve->currentData().toString().toStdString();
      const auto* entity = topology.value().find_entity(id);
      if (!entity || entity->points().size() < 2)
        return Result<SketchConstraintIntent>::failure(
            error("Select a supported curve and its endpoint."));
      auto curve_target = SketchConstraintIntentTarget::entity(id);
      auto point_target = SketchConstraintIntentTarget::point(
          endpoint->currentIndex() == 1 ? entity->points().back() : entity->points().front());
      if (!curve_target || !point_target)
        return Result<SketchConstraintIntent>::failure(
            error("Smooth requires stable curve endpoint roles."));
      targets.push_back(curve_target.value());
      targets.push_back(point_target.value());
    }
    return SketchConstraintIntent::create(*constraint, SketchSolverConstraintKind::Smooth,
                                          std::move(targets));
  }
  Result<std::size_t> commit(GuiDocumentSession& session) {
    if (smooth_mode) {
      auto intent = smooth_intent(session);
      if (!intent)
        return Result<std::size_t>::failure(intent.error());
      auto controller = GuiSketchConstraintController::create(session, *sketch, intent.value());
      if (!controller)
        return Result<std::size_t>::failure(controller.error());
      if (!controller.value().preview().authoring.accepted) {
        const auto& rejected = controller.value().preview().authoring;
        std::string message = "Smooth preview: " + std::string(to_string(rejected.solve.status));
        for (const auto& id : rejected.conflict_ids)
          message += "; conflict: " + id;
        for (const auto& id : rejected.redundant_ids)
          message += "; redundant: " + id;
        return Result<std::size_t>::failure(error(std::move(message)));
      }
      return controller.value().commit(session);
    }
    auto raw = definition();
    if (!raw)
      return Result<std::size_t>::failure(raw.error());
    const auto converted =
        geometry::EquationCurveGeometryAdapter{}.convert(*session.part_document(), raw.value());
    if (!converted)
      return Result<std::size_t>::failure(converted.error());
    auto display = geometry::sample_equation_curve_for_presentation(converted.value());
    if (!display)
      return Result<std::size_t>::failure(display.error());
    return session.commit_part_transaction(
        editing ? "Edit equation curve" : "Create equation curve",
        [id = *sketch, raw = raw.value(), replace = editing](PartDocument& part) {
          const auto* existing = part.find_sketch(id);
          if (!existing)
            return Result<std::size_t>::failure(error("Equation source sketch no longer exists."));
          auto candidate = *existing;
          const auto changed =
              replace ? candidate.replace_equation_curve(raw) : candidate.add_entity(raw);
          if (!changed)
            return changed;
          return part.update_sketch(std::move(candidate));
        });
  }
  Result<PlanarEndpointDifferential> endpoint_differential(const Sketch& candidate,
                                                           const QComboBox* curve,
                                                           const QComboBox* endpoint) const {
    const SketchEntityId id(curve->currentData().toString().toStdString());
    const bool end = endpoint->currentIndex() == 1;
    if (const auto* spline = candidate.find_spline_segment(id))
      return PlanarCurveDifferential::spline(
          spline->curve(), end,
          std::max(1e-12, std::hypot(spline->end().x - spline->start().x,
                                     spline->end().y - spline->start().y)),
          id.value());
    if (const auto* line = candidate.find_line_segment(id))
      return PlanarCurveDifferential::line(end ? line->end() : line->start(),
                                           end ? line->start() : line->end(), 1, id.value());
    if (const auto* arc = candidate.find_arc_segment(id))
      return PlanarCurveDifferential::arc({arc->start(), arc->mid(), arc->end()}, end, 1,
                                          id.value());
    return Result<PlanarEndpointDifferential>::failure(error("Unsupported Smooth endpoint."));
  }
  void diagnostics(const GuiDocumentSession& candidate) {
    const auto& selected = *candidate.part_document()->find_sketch(*sketch);
    Point2 anchor{};
    QString canvas;
    if (smooth_mode) {
      auto a = endpoint_differential(selected, first, first_end),
           b = endpoint_differential(selected, second, second_end);
      if (!a || !b) {
        differential->setText(text(!a ? a.error().message() : b.error().message()));
        return;
      }
      const double tangent = magnitude({a.value().inward_tangent.x + b.value().inward_tangent.x,
                                        a.value().inward_tangent.y + b.value().inward_tangent.y});
      const double curvature =
          magnitude({a.value().curvature_vector.x - b.value().curvature_vector.x,
                     a.value().curvature_vector.y - b.value().curvature_vector.y});
      differential->setText(
          QStringLiteral("Inward tangents A %1, B %2\nCurvature A %3, B %4 mm⁻¹\nTangent mismatch: "
                         "%5\nCurvature mismatch: %6 mm⁻¹")
              .arg(vector(a.value().inward_tangent), vector(b.value().inward_tangent),
                   vector(a.value().curvature_vector), vector(b.value().curvature_vector))
              .arg(tangent, 0, 'g', 6)
              .arg(curvature, 0, 'g', 6));
      canvas =
          QStringLiteral("G2: ΔT %1; Δκ %2 mm⁻¹").arg(tangent, 0, 'g', 3).arg(curvature, 0, 'g', 3);
      const auto topology = SketchTopology::migrate_legacy(selected);
      const auto* entity =
          topology.value().find_entity("entity/" + first->currentData().toString().toStdString());
      anchor = topology.value()
                   .find_point(first_end->currentIndex() == 1 ? entity->points().back()
                                                              : entity->points().front())
                   ->position();
    } else {
      const auto* raw = selected.find_equation_curve(*source);
      auto program = EquationCurveProgram::compile(*candidate.part_document(), *raw);
      const auto bounds = program.value().domain(*candidate.part_document());
      const auto start = program.value().evaluate(*candidate.part_document(), bounds.value().first),
                 end = program.value().evaluate(*candidate.part_document(), bounds.value().second);
      if (!start || !end)
        return;
      const auto format = [](const EquationCurveJet& jet) {
        const double speed = std::hypot(jet.first.x, jet.first.y);
        const double curvature =
            (jet.first.x * jet.second.y - jet.first.y * jet.second.x) / (speed * speed * speed);
        return QStringLiteral("Position %1 mm; tangent %2; curvature %3 mm⁻¹")
            .arg(vector({jet.value.x, jet.value.y}),
                 vector({jet.first.x / speed, jet.first.y / speed}))
            .arg(curvature, 0, 'g', 8);
      };
      differential->setText(QStringLiteral("Derived start — %1\nDerived end — %2\nEdit the domain "
                                           "or parameters to move these endpoints.")
                                .arg(format(start.value()), format(end.value())));
      anchor = {start.value().value.x, start.value().value.y};
      canvas = "Equation: derived endpoint";
    }
    GuiSketchAnnotationPrimitive annotation;
    annotation.semantic_id = "sketch/" + sketch->value() + "/curve-preview";
    annotation.candidate_id = "curve-preview";
    annotation.point = anchor;
    annotation.hit_kind = GuiSketchHitKind::Glyph;
    annotation.label = canvas.toStdString();
    window.viewport().set_sketch_dimension_preview(std::move(annotation));
  }
};

ShellSketchCurveTools::ShellSketchCurveTools(ShellWindow& window, ShellSketchTools& parent,
                                             GuiSketchWorkspace& workspace)
    : QObject(&parent), impl_(std::make_unique<Impl>(window, parent, workspace)) {
  auto& ui = *impl_;
  const auto action = [&](const char* name, const char* label, auto callback) {
    auto* value = new QAction(QString::fromUtf8(label), this);
    value->setObjectName(name);
    connect(value, &QAction::triggered, this, callback);
    return value;
  };
  ui.equation = action("blcad.action.sketch_equation", "Equation Curve", [this] {
    const auto r = begin_equation();
    if (!r)
      impl_->window.report_error(r.error());
  });
  ui.edit = action("blcad.action.sketch_equation_edit", "Edit Equation Curve", [this] {
    const auto r = begin_equation(true);
    if (!r)
      impl_->window.report_error(r.error());
  });
  ui.smooth = action("blcad.action.sketch_smooth", "Smooth (G2)", [this] {
    const auto r = begin_smooth();
    if (!r)
      impl_->window.report_error(r.error());
  });
  ui.dock = new QDockWidget("Equation Curve", &window);
  ui.dock->setObjectName("blcad.sketch.curve.task");
  ui.dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  ui.scroll = scroll;
  scroll->setObjectName("blcad.sketch.curve.scroll");
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* contents = new QWidget(scroll);
  auto* inputs = new QVBoxLayout(contents);
  ui.equation_fields = new QWidget(contents);
  auto* form = new QFormLayout(ui.equation_fields);
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  const auto field = [&](const char* name, const char* label, QLineEdit*& value) {
    value = new QLineEdit(ui.equation_fields);
    value->setObjectName(QStringLiteral("blcad.equation.") + name);
    value->installEventFilter(this);
    form->addRow(QString::fromUtf8(label), value);
    connect(value, &QLineEdit::textChanged, this, [this] { impl_->invalidate(); });
  };
  ui.mode = new QComboBox(ui.equation_fields);
  ui.mode->setObjectName("blcad.equation.mode");
  ui.mode->addItems({"Cartesian: y(x)", "Polar: r(θ)", "Parametric: x(u), y(u)"});
  form->addRow("Mode", ui.mode);
  field("variable", "Independent variable", ui.variable);
  ui.unit = new QComboBox(ui.equation_fields);
  ui.unit->setObjectName("blcad.equation.unit");
  ui.unit->addItems({"Scalar (trig in radians)", "Length (mm)", "Angle"});
  form->addRow("Domain unit", ui.unit);
  field("lower", "Lower domain", ui.lower);
  field("upper", "Upper domain", ui.upper);
  ui.x = new QLineEdit(ui.equation_fields);
  ui.x->setObjectName("blcad.equation.x");
  ui.x_label = new QLabel("X (mm)", ui.equation_fields);
  form->addRow(ui.x_label, ui.x);
  ui.y = new QLineEdit(ui.equation_fields);
  ui.y->setObjectName("blcad.equation.y");
  ui.y_label = new QLabel("Y (mm)", ui.equation_fields);
  form->addRow(ui.y_label, ui.y);
  for (auto* value : {ui.x, ui.y}) {
    value->installEventFilter(this);
    connect(value, &QLineEdit::textChanged, this, [this] { impl_->invalidate(); });
  }
  ui.parameter = new QComboBox(ui.equation_fields);
  ui.parameter->setObjectName("blcad.equation.parameter");
  auto* insert = new QPushButton("Insert parameter", ui.equation_fields);
  insert->setObjectName("blcad.equation.insert");
  auto* bindings = new QWidget(ui.equation_fields);
  auto* bind_layout = new QHBoxLayout(bindings);
  bind_layout->setContentsMargins(0, 0, 0, 0);
  bind_layout->addWidget(ui.parameter, 1);
  bind_layout->addWidget(insert);
  form->addRow("Parameter reference", bindings);
  connect(insert, &QPushButton::clicked, this, [this] {
    auto& u = *impl_;
    if (u.insertion_target && u.parameter->currentIndex() >= 0) {
      auto* target = u.insertion_target;
      const int cursor = target->cursorPosition(), start = target->selectionStart(),
                length = target->selectedText().size();
      target->setFocus(Qt::OtherFocusReason);
      if (start >= 0)
        target->setSelection(start, length);
      else
        target->setCursorPosition(cursor);
      target->insert("@{" + u.parameter->currentData().toString() + "}");
      target->deselect();
    }
  });
  field("tolerance", "Modeling tolerance (mm)", ui.tolerance);
  ui.tolerance->setText("0.001");
  ui.closed = new QCheckBox("Closed domain (requires proof)", ui.equation_fields);
  ui.closed->setObjectName("blcad.equation.closed");
  form->addRow(ui.closed);
  ui.construction = new QCheckBox("Construction geometry", ui.equation_fields);
  ui.construction->setObjectName("blcad.equation.construction");
  form->addRow(ui.construction);
  inputs->addWidget(ui.equation_fields);
  ui.smooth_fields = new QWidget(contents);
  auto* smooth_form = new QFormLayout(ui.smooth_fields);
  const auto endpoint = [&](const char* suffix, QComboBox*& curve, QComboBox*& end) {
    curve = new QComboBox(ui.smooth_fields);
    curve->setObjectName(QStringLiteral("blcad.smooth.") + suffix);
    end = new QComboBox(ui.smooth_fields);
    end->setObjectName(QStringLiteral("blcad.smooth.") + suffix + "_end");
    end->addItems({"Start", "End"});
    smooth_form->addRow(QStringLiteral("Curve ") + suffix, curve);
    smooth_form->addRow(QStringLiteral("Endpoint ") + suffix, end);
    connect(curve, &QComboBox::currentIndexChanged, this, [this] { impl_->invalidate(); });
    connect(end, &QComboBox::currentIndexChanged, this, [this] { impl_->invalidate(); });
  };
  endpoint("a", ui.first, ui.first_end);
  endpoint("b", ui.second, ui.second_end);
  inputs->addWidget(ui.smooth_fields);
  ui.differential = new QLabel(contents);
  ui.differential->setObjectName("blcad.sketch.curve.differential");
  ui.differential->setWordWrap(true);
  ui.differential->setTextInteractionFlags(Qt::TextSelectableByMouse);
  inputs->addWidget(ui.differential);
  ui.summary = new QLabel(contents);
  ui.summary->setObjectName("blcad.sketch.curve.summary");
  ui.summary->setWordWrap(true);
  inputs->addWidget(ui.summary);
  inputs->addStretch();
  scroll->setWidget(contents);
  layout->addWidget(scroll, 1);
  auto* footer = new QHBoxLayout;
  ui.preview = new QPushButton("Preview", panel);
  ui.preview->setObjectName("blcad.sketch.curve.preview");
  ui.apply = new QPushButton("Apply", panel);
  ui.apply->setObjectName("blcad.sketch.curve.apply");
  auto* cancel = new QPushButton("Cancel", panel);
  cancel->setObjectName("blcad.sketch.curve.cancel");
  ui.fit = new QPushButton("Fit", panel);
  ui.fit->setObjectName("blcad.sketch.curve.fit");
  ui.fit->setToolTip("Fit preview in the viewport");
  ui.fit->setEnabled(false);
  connect(ui.fit, &QPushButton::clicked, this, [this] {
    if (impl_->previewed)
      impl_->window.viewport().fit_feature_preview();
  });
  footer->addWidget(ui.preview);
  footer->addWidget(ui.fit);
  footer->addWidget(ui.apply);
  footer->addWidget(cancel);
  layout->addLayout(footer);
  const auto invoke = [this](bool apply_now) {
    const auto result = apply_now ? apply() : preview();
    if (!result) {
      impl_->summary->setText(text(result.error().message()));
      impl_->window.report_error(result.error());
    }
  };
  connect(ui.preview, &QPushButton::clicked, this, [invoke] { invoke(false); });
  connect(ui.apply, &QPushButton::clicked, this, [invoke] { invoke(true); });
  connect(cancel, &QPushButton::clicked, this, [this] { this->cancel(); });
  for (const auto& key :
       {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
    auto* shortcut = new QShortcut(key, panel);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this, invoke] {
      auto ready = preview();
      if (ready)
        invoke(true);
      else {
        impl_->summary->setText(text(ready.error().message()));
        impl_->window.report_error(ready.error());
      }
    });
  }
  auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), panel);
  escape->setContext(Qt::WidgetWithChildrenShortcut);
  connect(escape, &QShortcut::activated, this, [this] { this->cancel(); });
  for (auto* field : {ui.variable, ui.lower, ui.upper, ui.x, ui.y, ui.tolerance})
    connect(field, &QLineEdit::returnPressed, this, [invoke] { invoke(false); });
  for (auto* field :
       {ui.first, ui.second, ui.first_end, ui.second_end, ui.mode, ui.unit, ui.parameter})
    field->installEventFilter(this);
  connect(ui.mode, &QComboBox::currentIndexChanged, this, [this] {
    if (!impl_->filling)
      impl_->mode_changed(true);
  });
  connect(ui.unit, &QComboBox::currentIndexChanged, this, [this] { impl_->invalidate(); });
  connect(ui.closed, &QCheckBox::toggled, this, [this] { impl_->invalidate(); });
  connect(ui.construction, &QCheckBox::toggled, this, [this] { impl_->invalidate(); });
  connect(ui.dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    if (!visible && active())
      this->cancel();
  });
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  ui.mode_changed(true);
  window.viewport().installEventFilter(this);
}
ShellSketchCurveTools::~ShellSketchCurveTools() {
  cancel();
}
QAction& ShellSketchCurveTools::equation_action() const {
  return *impl_->equation;
}
QAction& ShellSketchCurveTools::edit_action() const {
  return *impl_->edit;
}
QAction& ShellSketchCurveTools::smooth_action() const {
  return *impl_->smooth;
}
bool ShellSketchCurveTools::active() const noexcept {
  return impl_->sketch.has_value();
}
Result<std::size_t> ShellSketchCurveTools::begin_equation(bool editing) {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(error("Finish or cancel the current command first."));
  const auto selected = editing ? ui.selected_equation() : std::nullopt;
  if (editing && !selected)
    return Result<std::size_t>::failure(error("Select an equation curve to edit."));
  ui.parent.cancel_tool();
  ui.smooth_mode = false;
  if (!ui.workspace.begin_command(ui.window.session(), equation_task, "Edit equation definition",
                                  false))
    return Result<std::size_t>::failure(error("Equation task could not start."));
  (void)ui.workspace.begin_numeric_input(ui.window.session());
  ui.workspace.focus_task_panel();
  ui.sketch = ui.window.active_sketch();
  ui.capture_source();
  ui.editing = editing;
  ui.camera = ui.window.viewport().camera_bookmark();
  ui.selection_enabled = ui.window.viewport().sketch_selection_enabled();
  ui.window.viewport().set_sketch_selection_enabled(false);
  const auto* sketch = ui.current_sketch();
  if (selected)
    ui.source = selected;
  else
    for (std::size_t i = 1;; ++i) {
      SketchEntityId id("equation." + std::to_string(i));
      const auto topology = SketchTopology::migrate_legacy(*sketch);
      if (!topology.value().find_entity("entity/" + id.value()) &&
          !topology.value().find_entity("profile/" + id.value())) {
        ui.source = id;
        break;
      }
    }
  ui.filling = true;
  ui.parameter->clear();
  for (const auto& p : ui.window.session().part_document()->parameters())
    ui.parameter->addItem(text(p.name()) + " (" + text(p.id().value()) + ")", text(p.id().value()));
  if (selected) {
    const auto& raw = *sketch->find_equation_curve(*selected);
    ui.mode->setCurrentIndex(raw.mode() == EquationCurveMode::Cartesian2D ? 0
                             : raw.mode() == EquationCurveMode::Polar2D   ? 1
                                                                          : 2);
    ui.variable->setText(text(raw.variable().name));
    ui.unit->setCurrentIndex(raw.variable().unit == CurveUnit::Scalar   ? 0
                             : raw.variable().unit == CurveUnit::Length ? 1
                                                                        : 2);
    ui.lower->setText(text(raw.lower()));
    ui.upper->setText(text(raw.upper()));
    ui.x->setText(text(raw.coordinates()[0]));
    ui.y->setText(text(raw.coordinates()[1]));
    ui.tolerance->setText(QString::number(raw.tolerance_mm(), 'g', 17));
    ui.closed->setChecked(raw.closure() == EquationCurveClosure::Periodic);
    ui.construction->setChecked(raw.construction());
  } else {
    ui.mode->setCurrentIndex(2);
    ui.construction->setChecked(false);
    ui.tolerance->setText("0.001");
  }
  ui.filling = false;
  ui.mode_changed(!editing);
  ui.equation_fields->show();
  ui.smooth_fields->hide();
  ui.dock->setWindowTitle(editing ? "Edit Equation Curve" : "Equation Curve");
  ui.dock->show();
  ui.dock->raise();
  ui.scroll->verticalScrollBar()->setValue(0);
  (ui.mode->currentIndex() == 0 ? ui.y : ui.x)->setFocus();
  ui.parent.refresh_enablement();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellSketchCurveTools::begin_smooth() {
  auto& ui = *impl_;
  if (!ui.can_smooth())
    return Result<std::size_t>::failure(
        error("Smooth requires two supported curves, including a spline."));
  if (!ui.available())
    return Result<std::size_t>::failure(error("Finish or cancel the current command first."));
  ui.parent.cancel_tool();
  ui.smooth_mode = true;
  if (!ui.workspace.begin_command(ui.window.session(), smooth_task, "Choose Smooth curve endpoints",
                                  false))
    return Result<std::size_t>::failure(error("Smooth task could not start."));
  (void)ui.workspace.begin_numeric_input(ui.window.session());
  ui.workspace.focus_task_panel();
  ui.sketch = ui.window.active_sketch();
  ui.capture_source();
  ui.camera = ui.window.viewport().camera_bookmark();
  ui.selection_enabled = ui.window.viewport().sketch_selection_enabled();
  ui.window.viewport().set_sketch_selection_enabled(false);
  auto catalog = ui.window.session().sketch_constraint_catalog(*ui.sketch);
  for (std::size_t i = 1;; ++i) {
    SketchConstraintId id("constraint.gui.smooth." + std::to_string(i));
    if (!catalog.value().find(id)) {
      ui.constraint = id;
      break;
    }
  }
  ui.filling = true;
  ui.first->clear();
  ui.second->clear();
  const auto* sketch = ui.current_sketch();
  const auto add = [&](const auto& curves, const char* kind) {
    for (const auto& curve : curves)
      for (auto* combo : {ui.first, ui.second})
        combo->addItem(QString::fromUtf8(kind) + " / " + text(curve.id().value()),
                       text(curve.id().value()));
  };
  add(sketch->spline_segments(), "Spline");
  add(sketch->line_segments(), "Line");
  add(sketch->arc_segments(), "Arc");
  ui.second->setCurrentIndex(std::min(1, ui.second->count() - 1));
  ui.first_end->setCurrentIndex(0);
  ui.second_end->setCurrentIndex(0);
  const auto prefix = "sketch/" + ui.sketch->value() + "/entity/";
  int target = 0;
  for (const auto& selection : ui.window.session().selection().entries())
    if (selection.semantic_id.starts_with(prefix) && target < 2) {
      const bool a = target++ == 0;
      auto* combo = a ? ui.first : ui.second;
      const int index = combo->findData(text(selection.semantic_id.substr(prefix.size())));
      if (index >= 0) {
        combo->setCurrentIndex(index);
        (a ? ui.first_end : ui.second_end)
            ->setCurrentIndex(selection.candidate_id.ends_with(":end") ? 1 : 0);
      }
    }
  ui.filling = false;
  ui.invalidate();
  ui.equation_fields->hide();
  ui.smooth_fields->show();
  ui.dock->setWindowTitle("Smooth (G2)");
  ui.dock->show();
  ui.dock->raise();
  ui.scroll->verticalScrollBar()->setValue(0);
  ui.first->setFocus();
  ui.parent.refresh_enablement();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellSketchCurveTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model changed. Reopen the curve task."));
  }
  ui.invalidate();
  auto candidate = ui.window.session();
  const auto committed = ui.commit(candidate);
  if (!committed) {
    if (ui.smooth_mode)
      ui.diagnostics(ui.window.session());
    return committed;
  }
  auto scene = geometry::ViewportSceneBuilder{}.build_part(
      *candidate.part_document(), *candidate.part_shape_cache(), candidate.history_prefix_count());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  filter_presentation_visibility(candidate, scene.value());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  const auto prefix = "sketch/" + ui.sketch->value() + "/";
  for (auto& item : scene.value())
    if (item.semantic_id.starts_with(prefix) ||
        item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody ||
        item.kind == geometry::ViewportSceneKind::Path) {
      replaced.push_back(item.semantic_id);
      shapes.push_back(std::move(item.shape));
    }
  const auto shown =
      ui.window.viewport().set_sketch_previews(*ui.sketch, std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  ui.parent.publish_curve_candidate(*candidate.part_document()->find_sketch(*ui.sketch));
  ui.diagnostics(candidate);
  ui.previewed = true;
  ui.apply->setEnabled(true);
  ui.fit->setEnabled(true);
  (void)ui.workspace.show_preview(ui.window.session());
  ui.summary->setText("Valid preview. Dependent geometry updated.");
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellSketchCurveTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed)
    return Result<std::size_t>::failure(error("Preview the current definition before applying."));
  const auto checked = preview();
  if (!checked)
    return checked;
  const auto committed = ui.commit(ui.window.session());
  if (!committed)
    return committed;
  (void)ui.workspace.commit_command(ui.window.session());
  ui.camera.reset();
  cancel();
  ui.parent.activate();
  ui.window.refresh();
  return committed;
}
void ShellSketchCurveTools::cancel() {
  auto& ui = *impl_;
  if (!ui.sketch)
    return;
  while (ui.window.session().task().command_id() == ui.command())
    if (!ui.workspace.escape(ui.window.session())) {
      (void)ui.window.session().task().cancel();
      break;
    }
  ui.sketch.reset();
  ui.source.reset();
  ui.constraint.reset();
  ui.source_snapshot.clear();
  ui.source_constraints.clear();
  ui.source_dimensions.clear();
  ui.source_prefix.reset();
  ui.previewed = false;
  ui.window.viewport().clear_feature_preview();
  ui.window.viewport().set_sketch_dimension_preview(std::nullopt);
  ui.window.viewport().set_sketch_selection_enabled(ui.selection_enabled);
  if (ui.camera)
    (void)ui.window.viewport().restore_camera_bookmark(*ui.camera);
  ui.camera.reset();
  ui.dock->hide();
  ui.apply->setEnabled(false);
  ui.fit->setEnabled(false);
  ui.parent.publish_scene();
  ui.parent.refresh_enablement();
}
void ShellSketchCurveTools::refresh() {
  auto& ui = *impl_;
  if (active() && !ui.current())
    cancel();
  const bool ready = ui.available();
  ui.equation->setEnabled(ready);
  ui.edit->setEnabled(ready && ui.selected_equation().has_value());
  const auto* sketch =
      ui.window.active_sketch() && ui.window.session().part_document()
          ? ui.window.session().part_document()->find_sketch(*ui.window.active_sketch())
          : nullptr;
  ui.smooth->setEnabled(ready && sketch && !sketch->spline_segments().empty() &&
                        sketch->spline_segments().size() + sketch->line_segments().size() +
                                sketch->arc_segments().size() >=
                            2);
}
bool ShellSketchCurveTools::eventFilter(QObject* object, QEvent* event) {
  auto& ui = *impl_;
  if (active() && event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      cancel();
      return true;
    }
    if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
        key->modifiers().testFlag(Qt::ControlModifier)) {
      auto checked = preview();
      if (checked)
        checked = apply();
      if (!checked) {
        impl_->summary->setText(text(checked.error().message()));
        impl_->window.report_error(checked.error());
      }
      return true;
    }
  }
  if (event->type() == QEvent::FocusIn)
    for (auto* field : {ui.lower, ui.upper, ui.x, ui.y})
      if (object == field)
        ui.insertion_target = field;
  if (object == &ui.window.viewport() && event->type() == QEvent::KeyPress && active()) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      cancel();
      return true;
    }
  }
  if (object == &ui.window.viewport() && event->type() == QEvent::MouseButtonDblClick &&
      !active() && ui.available()) {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    const auto& hit = ui.window.viewport().hovered_sketch_hit();
    const auto selected = ui.selected_equation();
    if (mouse->button() == Qt::LeftButton && selected && hit &&
        hit->semantic_id ==
            "sketch/" + ui.window.active_sketch()->value() + "/entity/" + selected->value()) {
      const auto opened = begin_equation(true);
      if (!opened)
        ui.window.report_error(opened.error());
      return true;
    }
  }
  return QObject::eventFilter(object, event);
}
} // namespace blcad::gui
