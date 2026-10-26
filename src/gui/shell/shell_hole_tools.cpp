#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "part.hole";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
Result<double> number(const QLineEdit& field) {
  const auto source = field.text().trimmed().toStdString();
  double value{};
  const auto parsed = std::from_chars(source.data(), source.data() + source.size(), value);
  if (parsed.ec != std::errc() || parsed.ptr != source.data() + source.size() ||
      !std::isfinite(value))
    return Result<double>::failure(error("Enter finite numbers without units (decimal point)."));
  return Result<double>::success(value);
}
// The retained SubtractiveExtrude contract requires a Feature from the Extrude
// collection even when a Body context supplies the actual target geometry.
std::optional<FeatureId> target_feature(const PartDocument& part, const BodyId& body) {
  for (auto it = part.features().rbegin(); it != part.features().rend(); ++it)
    if (it->body_result_context() && it->body_result_context()->effective_produced_body() == body)
      return it->id();
  return std::nullopt;
}
bool target_available(const GuiDocumentSession& session, const Body& body) {
  return body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
         session.part_shape_cache() && session.part_shape_cache()->find_body_shape(body.id()) &&
         target_feature(*session.part_document(), body.id()).has_value();
}
struct Input {
  QComboBox* binding{};
  QLineEdit* value{};
  ParameterType type{ParameterType::Length};
  std::string suffix;
  QString local;
  bool was_local{true};
};
struct Candidate {
  std::vector<Parameter> parameters;
  Sketch sketch;
  Feature cut;
  BodyId body;
};
Result<std::size_t> add_candidate(PartDocument& part, const Candidate& candidate) {
  for (const auto& parameter : candidate.parameters) {
    const auto added = part.add_parameter(parameter);
    if (!added)
      return added;
  }
  const auto added = part.add_sketch(candidate.sketch);
  if (!added)
    return added;
  return part.add_feature(candidate.cut);
}
} // namespace

struct ShellHoleTools::Impl {
  ShellWindow& window;
  QAction* action{};
  QDockWidget* dock{};
  QLineEdit* name{};
  QComboBox* target{};
  QComboBox* plane{};
  QComboBox* placement{};
  std::array<QLineEdit*, 2> center{};
  QLineEdit* angle{};
  std::array<Input, 3> inputs{}; // diameter, radius, count
  QLabel* summary{};
  QPushButton* apply_button{};
  bool active{false};
  bool loading{false};
  bool previewed{false};
  std::size_t revision{};
  std::size_t index{};
  std::uint32_t previous_mask{0xFFFFFFFFU};
  std::optional<std::vector<std::string>> previous_scope;
  std::optional<GuiSelection> previous_selection;

  bool available() const {
    const auto& session = window.session();
    const auto* part = session.part_document();
    return part && session.derived_results_fresh() && !session.reference_repair_mode() &&
           !session.task().active() && !window.active_sketch() &&
           std::any_of(part->bodies().begin(), part->bodies().end(),
                       [&](const auto& body) { return target_available(session, body); });
  }
  bool current() const {
    const auto& session = window.session();
    return active && session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.model_revision() == revision;
  }
  std::string id(std::string_view kind, std::string_view suffix = {}) const {
    return std::string(kind) + ".hole." + std::to_string(index) + std::string(suffix);
  }
  void invalidate() {
    previewed = false;
    apply_button->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void edited() {
    if (loading || !active)
      return;
    invalidate();
    const bool circular = placement->currentData().toBool();
    for (std::size_t i = 1; i < inputs.size(); ++i) {
      inputs[i].binding->setEnabled(circular);
      inputs[i].value->setEnabled(circular);
    }
    angle->setEnabled(circular);
    summary->setText(
        QStringLiteral("Through all along the plane normal. Check the preview, then apply."));
  }
  void select_binding(Input& input) {
    if (input.was_local)
      input.local = input.value->text();
    const auto* parameter = window.session().part_document()->find_parameter(
        ParameterId(input.binding->currentData().toString().toStdString()));
    loading = true;
    input.was_local = !parameter;
    input.value->setReadOnly(parameter);
    input.value->setText(
        parameter
            ? (input.type == ParameterType::Count
                   ? QString::number(static_cast<qulonglong>(parameter->value().count_value()))
                   : QString::number(parameter->value().millimeters(), 'g', 17))
            : input.local);
    input.value->setToolTip(
        parameter && parameter->formula() ? "Formula: " + text(*parameter->formula()) : QString());
    loading = false;
    edited();
  }
  Result<ParameterId> resolve_input(const Input& input, std::vector<Parameter>& added) const {
    const auto selected = input.binding->currentData().toString();
    if (!selected.isEmpty()) {
      const auto* parameter =
          window.session().part_document()->find_parameter(ParameterId(selected.toStdString()));
      if (!parameter || parameter->type() != input.type)
        return Result<ParameterId>::failure(
            error("Parameter must exist and have the displayed type."));
      if (input.type == ParameterType::Count && parameter->value().count_value() > 1000)
        return Result<ParameterId>::failure(error("Bolt circle supports 1 to 1000 holes."));
      return Result<ParameterId>::success(parameter->id());
    }
    const auto parsed = number(*input.value);
    if (!parsed)
      return Result<ParameterId>::failure(parsed.error());
    if (input.type == ParameterType::Count && parsed.value() > 1000)
      return Result<ParameterId>::failure(error("Bolt circle supports 1 to 1000 holes."));
    const ParameterId parameter_id(id("parameter", "." + input.suffix));
    const auto quantity = input.type == ParameterType::Count ? Quantity::count(parsed.value())
                                                             : Quantity::length_mm(parsed.value());
    if (!quantity)
      return Result<ParameterId>::failure(quantity.error());
    const auto parameter_name = "hole_" + std::to_string(index) + "_" + input.suffix;
    auto parameter = input.type == ParameterType::Count
                         ? Parameter::create_count(parameter_id, parameter_name, quantity.value())
                         : Parameter::create_length(parameter_id, parameter_name, quantity.value());
    if (!parameter)
      return Result<ParameterId>::failure(parameter.error());
    added.push_back(std::move(parameter.value()));
    return Result<ParameterId>::success(parameter_id);
  }
  Result<Candidate> candidate() const {
    const auto& part = *window.session().part_document();
    const BodyId body_id(target->currentData().toString().toStdString());
    const auto* body = part.find_body(body_id);
    if (!body || !target_available(window.session(), *body) || plane->currentIndex() < 0)
      return Result<Candidate>::failure(
          error("Select a valid visible solid target body with extrude history and a workplane."));
    const auto feature = target_feature(part, body_id);
    const auto title = name->text().trimmed().toStdString();
    if (title.empty())
      return Result<Candidate>::failure(error("Enter a name."));
    const auto x = number(*center[0]), y = number(*center[1]);
    if (!x || !y)
      return Result<Candidate>::failure(!x ? x.error() : y.error());
    std::vector<Parameter> parameters;
    const auto diameter = resolve_input(inputs[0], parameters);
    if (!diameter)
      return Result<Candidate>::failure(diameter.error());
    auto sketch = Sketch::create(SketchId(id("sketch")), title + " Positions",
                                 DatumPlaneId(plane->currentData().toString().toStdString()));
    if (!sketch)
      return Result<Candidate>::failure(sketch.error());
    const ProfileId profile_id(id("profile"));
    if (placement->currentData().toBool()) {
      const auto radius = resolve_input(inputs[1], parameters),
                 count = resolve_input(inputs[2], parameters);
      const auto offset = number(*angle);
      if (!radius || !count || !offset)
        return Result<Candidate>::failure(!radius  ? radius.error()
                                          : !count ? count.error()
                                                   : offset.error());
      const auto pattern =
          CircularHolePattern::create(profile_id, radius.value(), count.value(), diameter.value(),
                                      {x.value(), y.value()}, offset.value());
      if (!pattern)
        return Result<Candidate>::failure(pattern.error());
      const auto added = sketch.value().add_profile(pattern.value());
      if (!added)
        return Result<Candidate>::failure(added.error());
    } else {
      const auto circle =
          CircleProfile::create(profile_id, diameter.value(), {x.value(), y.value()});
      if (!circle)
        return Result<Candidate>::failure(circle.error());
      const auto added = sketch.value().add_profile(circle.value());
      if (!added)
        return Result<Candidate>::failure(added.error());
    }
    auto cut = Feature::create_subtractive_extrude(FeatureId(id("feature")), title,
                                                   sketch.value().id(), *feature);
    const auto context =
        FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, body_id, std::nullopt);
    if (!cut || !context)
      return Result<Candidate>::failure(!cut ? cut.error() : context.error());
    cut = cut.value().with_body_result_context(context.value());
    if (!cut)
      return Result<Candidate>::failure(cut.error());
    // The dedicated sketch has exactly one profile, as required by the frozen
    // historical through-all circular-cut executor (no ambiguous profile mixing).
    return Result<Candidate>::success(
        {std::move(parameters), std::move(sketch.value()), std::move(cut.value()), body_id});
  }
};

ShellHoleTools::ShellHoleTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("Holes"), this);
  ui.action->setObjectName("blcad.action.hole");
  ribbon.add_button(ribbon.add_group(model_tab, QStringLiteral("Holes")), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    const auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Through Holes"), &window);
  ui.dock->setObjectName("blcad.shell.hole_task");
  auto* panel = new QWidget(ui.dock);
  auto* column = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* layout = new QVBoxLayout(content);
  auto* hint =
      new QLabel(QStringLiteral("Single hole or complete bolt circle through the target body. "
                                "Positions are local workplane X/Y coordinates. Parameters remain "
                                "bound; new values are created only on Apply."),
                 content);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto* form = new QFormLayout;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  const auto line = [&](const char* name, const QString& label) {
    auto* field = new QLineEdit(content);
    field->setObjectName(QStringLiteral("blcad.hole.") + name);
    form->addRow(label, field);
    connect(field, &QLineEdit::textChanged, this, [this] { impl_->edited(); });
    return field;
  };
  const auto combo = [&](const char* name, const QString& label) {
    auto* field = new QComboBox(content);
    field->setObjectName(QStringLiteral("blcad.hole.") + name);
    field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    field->setMinimumContentsLength(14);
    form->addRow(label, field);
    connect(field, &QComboBox::currentIndexChanged, this, [this] { impl_->edited(); });
    return field;
  };
  ui.name = line("name", QStringLiteral("Name"));
  ui.target = combo("target", QStringLiteral("Target Body"));
  ui.plane = combo("plane", QStringLiteral("Workplane"));
  ui.placement = combo("placement", QStringLiteral("Layout"));
  ui.placement->addItem(QStringLiteral("Single Hole"), false);
  ui.placement->addItem(QStringLiteral("Bolt Circle"), true);
  ui.center[0] = line("center_x", QStringLiteral("Center X (mm)"));
  ui.center[1] = line("center_y", QStringLiteral("Center Y (mm)"));
  for (std::size_t i = 0; i < ui.inputs.size(); ++i) {
    auto& input = ui.inputs[i];
    input.suffix = i == 0 ? "diameter" : i == 1 ? "radius" : "count";
    input.type = i == 2 ? ParameterType::Count : ParameterType::Length;
    const auto label = i == 0   ? QStringLiteral("Diameter")
                       : i == 1 ? QStringLiteral("Bolt Circle Radius")
                                : QStringLiteral("Count");
    input.binding = combo((input.suffix + "_binding").c_str(), label);
    input.value = line(input.suffix.c_str(), i == 0   ? QStringLiteral("Diameter (mm)")
                                             : i == 1 ? QStringLiteral("Radius (mm)")
                                                      : QStringLiteral("Count (1–1000)"));
    connect(input.binding, &QComboBox::currentIndexChanged, this, [this, i] {
      if (!impl_->loading && impl_->active) {
        if (!impl_->current()) {
          cancel();
          return;
        }
        impl_->select_binding(impl_->inputs[i]);
      }
    });
  }
  ui.angle = line("angle", QStringLiteral("Start Angle (°)"));
  layout->addLayout(form);
  ui.summary = new QLabel(content);
  ui.summary->setObjectName("blcad.hole.summary");
  ui.summary->setWordWrap(true);
  ui.summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(ui.summary);
  layout->addStretch();
  scroll->setWidget(content);
  column->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* name, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.hole.") + name);
    buttons->addWidget(result);
    return result;
  };
  auto* preview_button = button("preview", QStringLiteral("Preview"));
  ui.apply_button = button("apply", QStringLiteral("Apply"));
  auto* cancel_button = button("cancel", QStringLiteral("Cancel"));
  column->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  qApp->installEventFilter(this);
  const auto report = [this](const Result<std::size_t>& result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply_button, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
}
ShellHoleTools::~ShellHoleTools() = default;
bool ShellHoleTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellHoleTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("Holes require a current visible solid body with extrude history "
              "without an active command or sketch editor."));
  const auto& part = *ui.window.session().part_document();
  for (ui.index = 1;; ++ui.index) {
    bool used = false;
    for (const auto* kind : {"feature", "sketch", "profile"})
      used |= part.dependency_graph().has_node(ui.id(kind));
    for (const auto* suffix : {"diameter", "radius", "count"}) {
      used |= part.dependency_graph().has_node(ui.id("parameter", "." + std::string(suffix)));
      used |= part.find_parameter(
                  std::string_view("hole_" + std::to_string(ui.index) + "_" + suffix)) != nullptr;
    }
    if (!used)
      break;
  }
  (void)ui.window.session().task().begin(std::string(task_id));
  (void)ui.window.session().task().begin_parameter_editing();
  ui.previous_mask = ui.window.viewport().selection_filter_mask();
  ui.previous_scope = ui.window.viewport().selection_semantic_scope();
  ui.previous_selection = ui.window.viewport().selected_semantic();
  ui.active = true;
  ui.revision = ui.window.session().model_revision();
  ui.loading = true;
  ui.name->setText(QStringLiteral("Holes %1").arg(ui.index));
  ui.target->clear();
  for (const auto& body : part.bodies())
    if (target_available(ui.window.session(), body))
      ui.target->addItem(text(body.name()) + " / " + text(body.id().value()),
                         text(body.id().value()));
  ui.plane->clear();
  const auto add_plane = [&](const auto& plane) {
    const auto sketch = Sketch::create(SketchId("hole.plane.check"), "Plane check",
                                       DatumPlaneId(plane.id().value()));
    if (sketch && geometry::WorkplaneResolver{}.resolve_for_sketch(
                      part, sketch.value(), *ui.window.session().part_shape_cache()))
      ui.plane->addItem(text(plane.name()) + " / " + text(plane.id().value()),
                        text(plane.id().value()));
  };
  for (const auto& plane : part.datum_planes())
    add_plane(plane);
  for (const auto& plane : part.construction_planes())
    add_plane(plane);
  for (const auto& plane : part.derived_workplanes())
    add_plane(plane);
  ui.placement->setCurrentIndex(0);
  for (auto* center : ui.center)
    center->setText("0");
  ui.angle->setText("0");
  for (std::size_t i = 0; i < ui.inputs.size(); ++i) {
    auto& input = ui.inputs[i];
    input.binding->clear();
    input.binding->addItem(QStringLiteral("New Value"), QString());
    for (const auto& parameter : part.parameters())
      if (parameter.type() == input.type)
        input.binding->addItem(text(parameter.name()) + " / " + text(parameter.id().value()),
                               text(parameter.id().value()));
    input.was_local = true;
    input.local = i == 0 ? "6" : i == 1 ? "20" : "6";
    input.value->setReadOnly(false);
    input.value->setText(input.local);
    input.value->setToolTip({});
  }
  ui.loading = false;
  for (const auto& selection : ui.window.session().selection().entries())
    (void)accept_selection(selection);
  ui.window.viewport().set_selection_semantic_scope(std::nullopt);
  ui.window.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body) |
                                                 selection_kind_bit(GuiSelectionKind::Datum));
  ui.edited();
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.index);
}
Result<std::size_t> ShellHoleTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Model or command changed. Reopen holes."));
  }
  ui.invalidate();
  const auto definition = ui.candidate();
  if (!definition)
    return Result<std::size_t>::failure(definition.error());
  auto candidate = ui.window.session();
  const auto changed = candidate.commit_part_transaction(
      "Through Holes", [&](PartDocument& part) { return add_candidate(part, definition.value()); });
  if (!changed)
    return changed;
  const auto* before =
      ui.window.session().part_shape_cache()->find_body_shape(definition.value().body);
  const auto* after = candidate.part_shape_cache()->find_body_shape(definition.value().body);
  if (!before || !after)
    return Result<std::size_t>::failure(error("Target body has no current result."));
  const auto original = geometry::RectangleExtrusionAdapter{}.summarize(*before);
  const auto result = geometry::RectangleExtrusionAdapter{}.summarize(*after);
  const auto tolerance = std::max(1e-9, std::abs(original.volume_mm3) * 1e-10);
  if (result.solid_count == 0 || !std::isfinite(result.volume_mm3) ||
      result.volume_mm3 <= tolerance || result.volume_mm3 >= original.volume_mm3 - tolerance)
    return Result<std::size_t>::failure(
        error("Holes must remove material and preserve a solid body."));
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  // Validate the complete presentation, then shade the candidate Body results.
  // Keep supporting planes and sketches in their normal wire presentation so
  // an unchanged face workplane cannot fill the visible hole openings.
  const geometry::ViewportSceneBuilder builder;
  auto scene = builder.build_part(*candidate.part_document(), *candidate.part_shape_cache());
  auto old_scene = builder.build_part(*ui.window.session().part_document(),
                                      *ui.window.session().part_shape_cache());
  if (!scene || !old_scene)
    return Result<std::size_t>::failure(!scene ? scene.error() : old_scene.error());
  for (const auto& item : old_scene.value())
    if (item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody)
      replaced.push_back(item.semantic_id);
  for (auto& item : scene.value())
    if (item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody)
      shapes.push_back(std::move(item.shape));
  const auto shown =
      ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  ui.previewed = true;
  (void)ui.window.session().task().show_preview();
  ui.apply_button->setEnabled(true);
  ui.summary->setText(QStringLiteral("Preview valid. Removed: %1 mm³. Apply commits "
                                     "the sketch, new parameters, and cut together.")
                          .arg(original.volume_mm3 - result.volume_mm3, 0, 'g', 12));
  return changed;
}
Result<std::size_t> ShellHoleTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed) {
    if (!ui.current())
      cancel();
    return Result<std::size_t>::failure(error("Check a current hole preview first."));
  }
  const auto checked = preview();
  if (!checked)
    return checked;
  const auto definition = ui.candidate();
  if (!definition)
    return Result<std::size_t>::failure(definition.error());
  const auto changed = ui.window.session().commit_part_transaction(
      "Through Holes", [&](PartDocument& part) { return add_candidate(part, definition.value()); });
  if (changed) {
    cancel();
    ui.window.remember_repeatable_command(*ui.action);
  } else
    ui.invalidate();
  return changed;
}
bool ShellHoleTools::accept_selection(const GuiSelection& selection) {
  auto& ui = *impl_;
  if (!ui.current())
    return false;
  const auto select = [](QComboBox* combo, std::string_view id) {
    const int index = combo->findData(text(id));
    if (index < 0)
      return false;
    combo->setCurrentIndex(index);
    return true;
  };
  if (selection.kind == GuiSelectionKind::Body && selection.semantic_id.starts_with("body/"))
    return select(ui.target, std::string_view(selection.semantic_id).substr(5));
  if (selection.kind == GuiSelectionKind::Datum)
    for (const auto prefix : {"datum-plane/", "construction-plane/", "derived-workplane/"})
      if (selection.semantic_id.starts_with(prefix))
        return select(
            ui.plane,
            std::string_view(selection.semantic_id).substr(std::char_traits<char>::length(prefix)));
  return false;
}
void ShellHoleTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  const bool restore_selection =
      !ui.window.session().task().active() || ui.window.session().task().command_id() == task_id;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  if (restore_selection) {
    ui.window.viewport().set_selection_semantic_scope(ui.previous_scope);
    ui.window.viewport().set_selection_filter_mask(ui.previous_mask);
    if (ui.previous_selection)
      (void)ui.window.viewport().select_semantic(ui.previous_selection->semantic_id);
    else
      ui.window.viewport().clear_selection();
  }
  ui.loading = true;
  ui.name->clear();
  ui.target->clear();
  ui.plane->clear();
  for (auto* center : ui.center)
    center->clear();
  ui.angle->clear();
  for (auto& input : ui.inputs) {
    input.binding->clear();
    input.value->clear();
    input.local.clear();
  }
  ui.summary->clear();
  ui.loading = false;
  ui.dock->hide();
  ui.window.refresh();
}
void ShellHoleTools::refresh() {
  auto& ui = *impl_;
  if (ui.active && !ui.current()) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
bool ShellHoleTools::eventFilter(QObject* watched, QEvent* event) {
  auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && widget && (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
