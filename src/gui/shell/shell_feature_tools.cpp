#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/datum_axis.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/gui/gui_interactive_profile_materialization.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace blcad::gui {
namespace {

std::optional<std::pair<SketchId, ProfileId>> parse_profile_semantic(std::string_view id) {
  constexpr std::string_view kRegionPrefix = "profile-region:";
  if (id.starts_with(kRegionPrefix)) {
    id.remove_prefix(kRegionPrefix.size());
    const auto separator = id.find(':');
    if (separator != std::string_view::npos && separator > 0U && separator + 1U < id.size())
      return std::pair{SketchId(std::string(id.substr(0U, separator))),
                       ProfileId(std::string(id.substr(separator + 1U)))};
    return std::nullopt;
  }

  constexpr std::string_view kSketchPrefix = "sketch/";
  if (!id.starts_with(kSketchPrefix))
    return std::nullopt;
  id.remove_prefix(kSketchPrefix.size());
  const auto sketch_end = id.find('/');
  if (sketch_end == std::string_view::npos || sketch_end == 0U)
    return std::nullopt;
  const std::string sketch(id.substr(0U, sketch_end));
  id.remove_prefix(sketch_end + 1U);
  constexpr std::string_view families[]{
      "profile/",     "rectangle-profile/", "circle-profile/",       "closed-profile/",
      "arc-profile/", "composite-profile/", "circular-hole-pattern/"};
  for (const auto family : families) {
    if (id.starts_with(family) && id.size() > family.size())
      return std::pair{SketchId(sketch), ProfileId(std::string(id.substr(family.size())))};
  }
  return std::nullopt;
}

bool shell_feature_profile_available(const PartDocument& part, const SketchId& sketch_id,
                                     const ProfileId& profile_id) {
  const Sketch* sketch = part.find_sketch(sketch_id);
  if (sketch == nullptr)
    return false;
  // CircularHolePattern is subtractive pattern intent, not a region supported
  // by the shell's additive Extrude/Revolve commands.
  if (GuiModelingWorkspace::has_materialized_profile(*sketch, profile_id))
    return sketch->find_rectangle_profile(profile_id) != nullptr ||
           sketch->find_circle_profile(profile_id) != nullptr ||
           sketch->find_closed_profile(profile_id) != nullptr ||
           sketch->find_arc_closed_profile(profile_id) != nullptr ||
           sketch->find_composite_closed_profile(profile_id) != nullptr;
  auto pending = prepare_interactive_profile(part, sketch_id, profile_id);
  return !pending.has_error() && pending.value().has_value();
}

bool parameter_name_exists(const PartDocument& part, std::string_view name) {
  return std::any_of(part.parameters().begin(), part.parameters().end(),
                     [name](const Parameter& parameter) { return parameter.name() == name; });
}

std::size_t next_extrude_index(const PartDocument& part) {
  for (std::size_t index = 1U;; ++index) {
    const std::string suffix = std::to_string(index);
    if (!part.dependency_graph().has_node("feature.extrusion." + suffix) &&
        !part.dependency_graph().has_node("body." + suffix) &&
        !part.dependency_graph().has_node("body:body." + suffix) &&
        part.find_body(BodyId("body." + suffix)) == nullptr &&
        !part.dependency_graph().has_node("parameter.extrusion." + suffix + ".depth") &&
        !part.dependency_graph().has_node("parameter.extrusion." + suffix + ".depth.second") &&
        !part.dependency_graph().has_node("parameter.extrusion." + suffix + ".depth.wall") &&
        !part.dependency_graph().has_node("parameter.extrusion." + suffix + ".depth.wall.second") &&
        !parameter_name_exists(part, "Extrusion " + suffix + " Tiefe") &&
        !parameter_name_exists(part, "feature.extrusion." + suffix + " zweite Tiefe") &&
        !parameter_name_exists(part, "feature.extrusion." + suffix + " Wandstärke") &&
        !parameter_name_exists(part, "feature.extrusion." + suffix + " zweite Wandstärke"))
      return index;
  }
}

std::size_t next_revolve_index(const PartDocument& part) {
  for (std::size_t index = 1U;; ++index) {
    const std::string suffix = std::to_string(index);
    if (!part.dependency_graph().has_node("feature.drehung." + suffix) &&
        !part.dependency_graph().has_node("body." + suffix) &&
        !part.dependency_graph().has_node("body:body." + suffix) &&
        part.find_body(BodyId("body." + suffix)) == nullptr)
      return index;
  }
}

QString number_text(double value) {
  return QString::number(value, 'g', 12);
}

std::optional<double> parse_numeric(const QString& source, std::string_view expected_unit) {
  std::string text = source.toStdString();
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return std::nullopt;
  text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1U);

  double value = 0.0;
  const char* const begin = text.data();
  const char* const stop = begin + text.size();
  auto [end, error] = std::from_chars(begin, stop, value);
  if (end == begin || error != std::errc() || !std::isfinite(value))
    return std::nullopt;
  while (end != stop && std::isspace(static_cast<unsigned char>(*end)))
    ++end;
  if (std::string_view(end, static_cast<std::size_t>(stop - end)) != expected_unit && end != stop)
    return std::nullopt;
  return value;
}

} // namespace

ShellFeatureTools::ShellFeatureTools(ShellWindow& window, GuiDocumentSession& session,
                                     const GuiSketchWorkbench& sketch_workbench,
                                     OcctViewport& viewport, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), window_(window), session_(session), sketch_workbench_(sketch_workbench),
      viewport_(viewport), coordinator_(&session_, &layer_) {
  auto* feature_group = ribbon.add_group(model_tab, QStringLiteral("Erstellen"));
  action_extrude_ = new QAction(QStringLiteral("Extrusion"), this);
  action_extrude_->setObjectName(QStringLiteral("blcad.action.extrude"));
  action_extrude_->setToolTip(
      QStringLiteral("Profil wählen und eine Extrusion mit Vorschau starten"));
  connect(action_extrude_, &QAction::triggered, this, [this] {
    const auto begun = begin_extrude();
    if (begun.has_error())
      report(begun.error());
  });
  action_revolve_ = new QAction(QStringLiteral("Drehung"), this);
  action_revolve_->setObjectName(QStringLiteral("blcad.action.revolve"));
  action_revolve_->setToolTip(
      QStringLiteral("Profil wählen und eine Drehung um eine passende Ursprungsachse starten"));
  connect(action_revolve_, &QAction::triggered, this, [this] {
    const auto begun = begin_revolve();
    if (begun.has_error())
      report(begun.error());
  });
  ribbon.add_button(feature_group, action_extrude_);
  ribbon.add_button(feature_group, action_revolve_);

  auto* finishing_group = ribbon.add_group(model_tab, QStringLiteral("Bearbeiten"));
  for (const auto& [command, label, id] : {std::tuple{Command::Fillet, "Verrunden", "fillet"},
                                           std::tuple{Command::Chamfer, "Fase", "chamfer"},
                                           std::tuple{Command::Shell, "Aushöhlen", "shell"},
                                           std::tuple{Command::Draft, "Formschräge", "draft"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + QString::fromUtf8(id));
    connect(action, &QAction::triggered, this, [this, command] {
      auto begun = begin_finishing(command);
      if (!begun)
        report(begun.error());
    });
    ribbon.add_small_button(finishing_group, action);
    finishing_actions_.emplace_back(command, action);
  }

  auto* pattern_group = ribbon.add_group(model_tab, QStringLiteral("Muster"));
  for (const auto& [command, label, id] :
       {std::tuple{Command::LinearPattern, "Lineares Muster", "linear_pattern"},
        std::tuple{Command::CircularPattern, "Kreismuster", "circular_pattern"},
        std::tuple{Command::BodyMirror, "Körper spiegeln", "body_mirror"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + QString::fromUtf8(id));
    connect(action, &QAction::triggered, this, [this, command] {
      auto begun = begin_pattern(command);
      if (!begun)
        report(begun.error());
    });
    ribbon.add_small_button(pattern_group, action);
    pattern_actions_.emplace_back(command, action);
  }

  auto* body_group = ribbon.add_group(model_tab, QStringLiteral("Körper"));
  for (const auto& [command, label, id] :
       {std::tuple{Command::BodyBoolean, "Kombinieren", "body_boolean"},
        std::tuple{Command::BodyTranslate, "Verschieben", "body_translate"},
        std::tuple{Command::BodyRotate, "Drehen", "body_rotate"},
        std::tuple{Command::BodyScale, "Skalieren", "body_scale"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + QString::fromUtf8(id));
    connect(action, &QAction::triggered, this, [this, command] {
      auto begun = begin_body_operation(command);
      if (!begun)
        report(begun.error());
    });
    ribbon.add_small_button(body_group, action);
    body_actions_.emplace_back(command, action);
  }

  auto* spatial_group = ribbon.add_group(model_tab, QStringLiteral("Sweep / Loft"));
  for (const auto& [command, label, id] :
       {std::tuple{Command::PathCurve, "Pfad erstellen", "path_curve"},
        std::tuple{Command::Sweep, "Sweep", "sweep"}, std::tuple{Command::Loft, "Loft", "loft"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + QString::fromUtf8(id));
    connect(action, &QAction::triggered, this, [this, command] {
      auto begun = begin_spatial(command);
      if (!begun)
        report(begun.error());
    });
    ribbon.add_small_button(spatial_group, action);
    spatial_actions_.emplace_back(command, action);
  }

  auto* surface_group = ribbon.add_group(model_tab, QStringLiteral("Flächen"));
  for (const auto& [command, label, id] :
       {std::tuple{Command::BoundarySurface, "Begrenzungsfläche", "surface_boundary"},
        std::tuple{Command::FillSurface, "Füllfläche", "surface_fill"},
        std::tuple{Command::TrimSurface, "Fläche trimmen", "surface_trim"},
        std::tuple{Command::ExtendSurface, "Fläche erweitern", "surface_extend"},
        std::tuple{Command::SurfaceStitch, "Flächen vernähen", "surface_stitch"},
        std::tuple{Command::ClosedShellToSolid, "Hülle zu Volumen", "surface_to_solid"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + QString::fromUtf8(id));
    connect(action, &QAction::triggered, this, [this, command] {
      auto begun = begin_surface(command);
      if (!begun)
        report(begun.error());
    });
    ribbon.add_small_button(surface_group, action);
    surface_actions_.emplace_back(command, action);
  }

  overlay_ = make_shell_manipulator_overlay(&viewport_, layer_);
  overlay_->setGeometry(viewport_.rect());
  viewport_.installEventFilter(this);
  window_.installEventFilter(this);

  task_dock_ = new QDockWidget(QStringLiteral("Feature"), &window_);
  task_dock_->setObjectName(QStringLiteral("blcad.shell.feature_task"));
  task_dock_->setFeatures(QDockWidget::DockWidgetMovable);
  auto* dock_contents = new QWidget(task_dock_);
  auto* dock_column = new QVBoxLayout(dock_contents);
  auto* panel = new QWidget(dock_contents);
  auto* column = new QVBoxLayout(panel);
  task_summary_ = new QLabel(panel);
  task_summary_->setWordWrap(true);
  column->addWidget(task_summary_);
  auto* form = new QFormLayout();
  numeric_editor_ = new QLineEdit(panel);
  numeric_editor_->setObjectName(QStringLiteral("blcad.manipulator_numeric_hud"));
  value_label_ = new QLabel(QStringLiteral("Wert"), panel);
  form->addRow(value_label_, numeric_editor_);
  column->addLayout(form);
  build_extrude_controls(panel);
  build_revolve_controls(panel);
  build_finishing_controls(panel, *form);
  build_pattern_controls(panel);
  build_body_controls(panel);
  build_spatial_controls(panel);
  build_surface_controls(panel);
  auto* buttons = new QHBoxLayout();
  apply_button_ = new QPushButton(QStringLiteral("Anwenden"), dock_contents);
  apply_button_->setObjectName(QStringLiteral("blcad.feature.apply"));
  cancel_button_ = new QPushButton(QStringLiteral("Abbrechen"), dock_contents);
  cancel_button_->setObjectName(QStringLiteral("blcad.feature.cancel"));
  buttons->addWidget(apply_button_);
  buttons->addWidget(cancel_button_);
  column->addStretch(1);
  auto* scroll = new QScrollArea(dock_contents);
  scroll->setObjectName(QStringLiteral("blcad.feature.inputs_scroll"));
  scroll->setMinimumWidth(380);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(panel);
  dock_column->addWidget(scroll, 1);
  dock_column->addLayout(buttons);
  task_dock_->setWidget(dock_contents);
  window_.addDockWidget(Qt::RightDockWidgetArea, task_dock_);
  task_dock_->hide();

  // Escape must work even when focus is in the task panel rather than the
  // viewport. The WindowShortcut covers all child widgets; the event filters
  // keep direct synthetic/test events deterministic as well.
  action_escape_ = new QAction(this);
  action_escape_->setObjectName(QStringLiteral("blcad.action.cancel_feature"));
  action_escape_->setShortcut(Qt::Key_Escape);
  action_escape_->setShortcutContext(Qt::WindowShortcut);
  action_escape_->setEnabled(false);
  window_.addAction(action_escape_);
  connect(action_escape_, &QAction::triggered, this, [this] { handle_escape(); });
  numeric_editor_->installEventFilter(this);
  apply_button_->installEventFilter(this);
  cancel_button_->installEventFilter(this);

  connect(numeric_editor_, &QLineEdit::editingFinished, this, [this] {
    if (synchronizing_numeric_ || !active())
      return;
    const auto preview = set_numeric_text(numeric_editor_->text());
    if (preview.has_error())
      report(preview.error());
  });
  connect(apply_button_, &QPushButton::clicked, this, [this] {
    const auto applied = apply();
    if (applied.has_error())
      report(applied.error());
  });
  connect(cancel_button_, &QPushButton::clicked, this, [this] { cancel(); });

  (void)update_mapping();
  refresh_enablement();
}

ShellFeatureTools::~ShellFeatureTools() = default;

bool ShellFeatureTools::active() const noexcept {
  return command_ != Command::None && (finishing() || pattern() || body_operation() || spatial() ||
                                       surface() || coordinator_.active());
}

std::optional<ShellFeatureTools::ProfileContext> ShellFeatureTools::profile_context() const {
  if (active() || session_.task().active() || window_.active_sketch() ||
      session_.reference_repair_mode() ||
      !session_.selection().allows(GuiSelectionKind::SketchEntity))
    return std::nullopt;
  const auto selected = profile_from_selection();
  if (selected)
    return ProfileContext{selected.value().first, selected.value().second};
  if (const auto line = open_line_from_selection())
    return ProfileContext{*line, std::nullopt};
  return std::nullopt;
}

bool ShellFeatureTools::select_profile(SketchId sketch, ProfileId profile) {
  selected_open_line_.reset();
  const auto* part = session_.part_document();
  if (!session_.selection().allows(GuiSelectionKind::SketchEntity) || part == nullptr ||
      !shell_feature_profile_available(*part, sketch, profile)) {
    selected_profile_.reset();
    refresh_enablement();
    return false;
  }
  selected_profile_ = std::pair{std::move(sketch), std::move(profile)};
  // Keep the authoritative semantic selection untouched. The explicit
  // handoff is only a fallback for Finish Sketch or browser nodes that have no
  // viewport selection; Cancel therefore preserves the exact prior selection.
  refresh_enablement();
  return true;
}

void ShellFeatureTools::clear_profile_selection() noexcept {
  selected_profile_.reset();
  selected_open_line_.reset();
  refresh_enablement();
}

bool ShellFeatureTools::select_open_line(SketchId sketch) {
  selected_profile_.reset();
  selected_open_line_.reset();
  const auto* part = session_.part_document();
  const auto* source = part ? part->find_sketch(sketch) : nullptr;
  if (session_.selection().allows(GuiSelectionKind::SketchEntity) && source &&
      interactive_thin_line(*source))
    selected_open_line_ = std::move(sketch);
  refresh_enablement();
  return selected_open_line_.has_value();
}

std::optional<SketchId> ShellFeatureTools::open_line_from_selection() const {
  const auto* part = session_.part_document();
  if (!part)
    return std::nullopt;
  for (const auto& selection : session_.selection().entries())
    if (const auto sketch = interactive_thin_selection(*part, selection.semantic_id))
      return sketch;
  const auto* source = selected_open_line_ ? part->find_sketch(*selected_open_line_) : nullptr;
  return source && interactive_thin_line(*source) ? selected_open_line_ : std::nullopt;
}

Result<std::pair<SketchId, ProfileId>> ShellFeatureTools::profile_from_selection() const {
  const auto* part = session_.part_document();
  const auto validate = [part](const std::pair<SketchId, ProfileId>& selected)
      -> std::optional<std::pair<SketchId, ProfileId>> {
    return part != nullptr &&
                   shell_feature_profile_available(*part, selected.first, selected.second)
               ? std::optional(selected)
               : std::nullopt;
  };
  for (const auto& selection : session_.selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity)
      continue;
    if (const auto parsed = parse_profile_semantic(selection.semantic_id))
      if (const auto valid = validate(*parsed))
        return Result<std::pair<SketchId, ProfileId>>::success(*valid);
  }
  if (selected_profile_.has_value())
    if (const auto valid = validate(*selected_profile_))
      return Result<std::pair<SketchId, ProfileId>>::success(*valid);
  return Result<std::pair<SketchId, ProfileId>>::failure(
      Error::validation("gui.shell.feature", "zuerst ein materialisiertes Skizzenprofil wählen"));
}

Result<GuiSketchPlaneView> ShellFeatureTools::selected_profile_plane() const {
  auto selected = profile_from_selection();
  if (selected.has_error())
    return Result<GuiSketchPlaneView>::failure(selected.error());
  return sketch_workbench_.plane_view(session_, selected.value().first);
}

Result<DatumAxisId> ShellFeatureTools::default_revolve_axis(const GuiSketchPlaneView& plane) const {
  const auto* part = session_.part_document();
  if (part == nullptr)
    return Result<DatumAxisId>::failure(
        Error::validation("gui.shell.feature", "Drehung benötigt ein aktives Bauteil"));
  for (const auto& axis : part->datum_axes()) {
    if (axis.kind() != DatumAxisKind::Explicit)
      continue;
    const double direction_offset = std::abs(dot(axis.direction(), plane.normal));
    const double origin_offset =
        std::abs(dot(displacement(plane.origin, axis.origin()), plane.normal));
    if (direction_offset <= 1.0e-6 && origin_offset <= 1.0e-6)
      return Result<DatumAxisId>::success(axis.id());
  }
  return Result<DatumAxisId>::failure(Error::validation(
      "gui.shell.feature", "keine zur Profilebene passende Bezugsachse gefunden"));
}

Result<std::size_t> ShellFeatureTools::begin_extrude() {
  if (active() || session_.task().active())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "zuerst den aktiven Befehl beenden"));
  auto selected = profile_from_selection();
  const auto open_line = open_line_from_selection();
  if (selected.has_error() && !open_line)
    return Result<std::size_t>::failure(selected.error());
  const auto sketch = open_line ? *open_line : selected.value().first;
  auto plane = sketch_workbench_.plane_view(session_, sketch);
  if (plane.has_error())
    return Result<std::size_t>::failure(plane.error());
  const auto* part = session_.part_document();
  if (part == nullptr)
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "Extrusion benötigt ein aktives Bauteil"));

  const std::size_t index = next_extrude_index(*part);
  const std::string suffix = std::to_string(index);
  const FeatureId feature("feature.extrusion." + suffix);
  const ParameterId parameter_id("parameter.extrusion." + suffix + ".depth");
  const BodyId body_id("body." + suffix);
  auto quantity = Quantity::length_mm(10.0, parameter_id.value());
  if (quantity.has_error())
    return Result<std::size_t>::failure(quantity.error());
  auto parameter =
      Parameter::create_length(parameter_id, "Extrusion " + suffix + " Tiefe", quantity.value());
  if (parameter.has_error())
    return Result<std::size_t>::failure(parameter.error());
  auto body = Body::create(body_id, "Körper " + suffix, BodyKind::Solid);
  if (body.has_error())
    return Result<std::size_t>::failure(body.error());

  preview_valid_ = false;
  if (!session_.task().begin("part.extrude") || !session_.task().begin_parameter_editing())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "Extrusionsaufgabe konnte nicht gestartet werden"));
  GuiFeatureManipulatorFrame frame{plane.value().origin, plane.value().normal, plane.value().normal,
                                   plane.value().x_axis};
  auto begun = coordinator_.begin_extrude_with_new_inputs(
      sketch, open_line ? std::nullopt : std::optional(selected.value().second), feature,
      "Extrusion " + suffix, std::move(parameter.value()), std::move(body.value()), frame);
  if (begun.has_error()) {
    (void)session_.task().cancel();
    return begun;
  }
  if (open_line) {
    const auto& line = part->find_sketch(sketch)->line_segments().front();
    const auto& view = plane.value();
    const double dx = line.end().x - line.start().x, dy = line.end().y - line.start().y;
    const double length = std::hypot(dx, dy);
    const Point2 middle{(line.start().x + line.end().x) / 2, (line.start().y + line.end().y) / 2};
    const Point3 origin{view.origin.x + middle.x * view.x_axis.x + middle.y * view.y_axis.x,
                        view.origin.y + middle.x * view.x_axis.y + middle.y * view.y_axis.y,
                        view.origin.z + middle.x * view.x_axis.z + middle.y * view.y_axis.z};
    const Vector3 left{(-dy * view.x_axis.x + dx * view.y_axis.x) / length,
                       (-dy * view.x_axis.y + dx * view.y_axis.y) / length,
                       (-dy * view.x_axis.z + dx * view.y_axis.z) / length};
    coordinator_.extrude().set_thin_manipulator_frame({origin, left, view.normal, view.x_axis});
  }
  command_ = Command::Extrude;
  previous_selection_mask_ = viewport_.selection_filter_mask();
  previous_semantic_scope_ = viewport_.selection_semantic_scope();
  extrude_previous_selection_ = viewport_.selected_semantic();
  extrude_previous_profile_ = selected_profile_;
  extrude_previous_open_line_ = selected_open_line_;
  reset_extrude_controls(frame);
  if (!session_.task().show_preview()) {
    cancel();
    return Result<std::size_t>::failure(
        Error::internal("gui.shell.feature", "Extrusionsvorschau konnte nicht aktiviert werden"));
  }
  show_task_panel(QStringLiteral("Extrusion"), QStringLiteral("mm"), 10.0);
  auto preview = preview_and_publish();
  if (preview.has_error()) {
    const Error error = preview.error();
    cancel();
    return Result<std::size_t>::failure(error);
  }
  window_.refresh();
  return begun;
}

Result<std::size_t> ShellFeatureTools::begin_revolve(std::optional<DatumAxisId> requested_axis) {
  if (active() || session_.task().active())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "zuerst den aktiven Befehl beenden"));
  auto selected = profile_from_selection();
  if (selected.has_error())
    return Result<std::size_t>::failure(selected.error());
  auto plane = sketch_workbench_.plane_view(session_, selected.value().first);
  if (plane.has_error())
    return Result<std::size_t>::failure(plane.error());
  auto controls = reset_revolve_controls(plane.value(), requested_axis);
  if (!controls)
    return controls;
  const auto* part = session_.part_document();
  const auto& axis = revolve_axes_.at(static_cast<std::size_t>(revolve_axis_->currentIndex()));

  const std::size_t index = next_revolve_index(*part);
  const std::string suffix = std::to_string(index);
  const FeatureId feature("feature.drehung." + suffix);
  const BodyId body_id("body." + suffix);
  auto body = Body::create(body_id, "Körper " + suffix, BodyKind::Solid);
  if (body.has_error())
    return Result<std::size_t>::failure(body.error());

  preview_valid_ = false;
  if (!session_.task().begin("part.revolve") || !session_.task().begin_parameter_editing())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "Drehungsaufgabe konnte nicht gestartet werden"));
  auto begun = coordinator_.begin_revolve_with_new_body(
      selected.value().first, selected.value().second, feature, "Drehung " + suffix, axis.reference,
      std::move(body.value()), axis.frame);
  if (begun.has_error()) {
    (void)session_.task().cancel();
    return begun;
  }
  command_ = Command::Revolve;
  // The shell opens with a useful partial revolve so both the angle wheel and
  // the numeric field are immediately available. The headless default remains
  // the canonical Full extent.
  coordinator_.revolve().set_angle(180.0);
  coordinator_.refresh_handles();
  if (!session_.task().show_preview()) {
    cancel();
    return Result<std::size_t>::failure(
        Error::internal("gui.shell.feature", "Drehungsvorschau konnte nicht aktiviert werden"));
  }
  show_task_panel(QStringLiteral("Drehung"), QStringLiteral("deg"), 180.0);
  auto preview = preview_and_publish();
  // Keep an invalid initial profile/axis combination correctable in the task.
  if (!preview)
    report(preview.error());
  window_.refresh();
  return begun;
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_numeric_text(const QString& text) {
  if (!active())
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.shell.feature", "kein aktiver Feature-Wert"));
  const std::string_view unit = command_ == Command::BodyScale ? ""
                                : angular_value()              ? "deg"
                                                               : "mm";
  const auto value = parse_numeric(text, unit);
  if (!value.has_value()) {
    synchronize_numeric_editor(current_value());
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.shell.feature", "numerischer Wert oder Einheit ist ungültig"));
  }
  auto preview = set_numeric_value(*value);
  if (preview.has_error())
    synchronize_numeric_editor(current_value());
  return preview;
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_translation_text(std::size_t component,
                                                                      const QString& text) {
  const auto value = parse_numeric(text, "mm");
  if (command_ != Command::BodyTranslate || component > 2 || !value) {
    synchronize_numeric_editor(current_value());
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("body", "Verschiebung oder Einheit ist ungültig"));
  }
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(kTransformTranslatePrefix) + "." + "xyz"[component];
  candidate.value_kind = GuiViewportManipulatorValueKind::Translation;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = *value;
  if (!layer_.active_handle_id().empty()) {
    if (layer_.active_handle_id() != candidate.handle_id ||
        !layer_.set_numeric_input(number_text(*value).toStdString() + " mm") ||
        !layer_.candidate()) {
      synchronize_numeric_editor(current_value());
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("body", "zuerst den aktiven Manipulatorzug abschließen"));
    }
    candidate = *layer_.candidate();
  }
  return accept_manipulator_candidate(candidate);
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_scale_center_text(std::size_t component,
                                                                       const QString& text) {
  const auto value = parse_numeric(text, "mm");
  if (command_ != Command::BodyScale || component > 2 || !value ||
      !layer_.active_handle_id().empty()) {
    synchronize_body_editors();
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("body", "Skalierungszentrum oder Einheit ist ungültig"));
  }
  const auto previous = body_scale_center_;
  if (component == 0)
    body_scale_center_.x = *value;
  else if (component == 1)
    body_scale_center_.y = *value;
  else
    body_scale_center_.z = *value;
  auto rebuilt = rebuild_body_controller();
  auto preview =
      rebuilt ? preview_and_publish() : Result<GuiPartFeaturePreview>::failure(rebuilt.error());
  if (!preview) {
    body_scale_center_ = previous;
    refresh_body_preview();
  }
  synchronize_body_editors();
  return preview;
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_secondary_numeric_text(const QString& text) {
  if (command_ != Command::Chamfer || finishing_mode_->currentIndex() <= 0)
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("finishing", "kein zweites Fasenmaß aktiv"));
  const bool angular = finishing_mode_->currentIndex() == 2;
  auto value = parse_numeric(text, angular ? "deg" : "mm");
  if (!value) {
    secondary_numeric_editor_->setText(number_text(finishing_second_value_));
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("finishing", "zweites Maß oder Einheit ist ungültig"));
  }
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = std::string(angular ? kChamferAngleHandleId : kChamferDistance2HandleId);
  candidate.value_kind =
      angular ? GuiViewportManipulatorValueKind::Angle : GuiViewportManipulatorValueKind::Distance;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = *value;
  if (!layer_.active_handle_id().empty()) {
    if (layer_.active_handle_id() != candidate.handle_id ||
        !layer_.set_numeric_input(number_text(*value).toStdString() + (angular ? " deg" : " mm")) ||
        !layer_.candidate())
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("finishing", "aktiver Manipulatorzug passt nicht zum zweiten Maß"));
    candidate = *layer_.candidate();
  }
  auto preview = accept_manipulator_candidate(candidate);
  finishing_second_value_ = coordinator_.finishing().second_value();
  secondary_numeric_editor_->setText(number_text(finishing_second_value_));
  return preview;
}

Result<GuiPartFeaturePreview> ShellFeatureTools::set_numeric_value(double value) {
  if (!active() || !std::isfinite(value))
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.shell.feature", "kein aktiver Feature-Wert"));
  GuiViewportManipulatorCandidate candidate;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = value;
  if (command_ == Command::Extrude) {
    if (value <= 0.0)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("gui.shell.feature", "Extrusionstiefe muss positiv sein"));
    candidate.handle_id = std::string(kExtrudeExtentHandleId);
    candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
  } else if (command_ == Command::Revolve) {
    if (value <= 0.0 || value >= 360.0)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("gui.shell.feature", "Drehwinkel muss zwischen 0 und 360 Grad liegen"));
    candidate.handle_id = std::string(kRevolveAngleHandleId);
    candidate.value_kind = GuiViewportManipulatorValueKind::Angle;
  } else if (surface()) {
    if (!surface_scalar() || !coordinator_.surface().active())
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("surface", "Zuerst gültige Flächeneingaben auswählen"));
    candidate.handle_id = std::string(
        command_ == Command::ExtendSurface ? kSurfaceExtendHandleId : kSurfaceToleranceValueId);
    candidate.value_kind = GuiViewportManipulatorValueKind::Distance;
  } else if (spatial()) {
    if (!spatial_scalar())
      return Result<GuiPartFeaturePreview>::failure(Error::validation(
          "spatial", "nur die aktive Sweep-Verdrehung besitzt einen numerischen Winkel"));
    candidate.handle_id = std::string(kSweepTwistHandleId);
    candidate.value_kind = GuiViewportManipulatorValueKind::Angle;
  } else if (body_operation()) {
    if (command_ == Command::BodyBoolean)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("body", "Boolean hat kein numerisches Maß"));
    candidate.handle_id = command_ == Command::BodyTranslate
                              ? std::string(kTransformTranslatePrefix) + ".x"
                          : command_ == Command::BodyRotate ? std::string(kTransformAngleHandleId)
                                                            : std::string(kTransformScaleHandleId);
    candidate.value_kind =
        command_ == Command::BodyTranslate ? GuiViewportManipulatorValueKind::Translation
        : command_ == Command::BodyRotate  ? GuiViewportManipulatorValueKind::Angle
                                           : GuiViewportManipulatorValueKind::ScaleFactor;
  } else if (pattern()) {
    if (command_ == Command::BodyMirror)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("pattern", "Spiegeln hat keinen numerischen Abstand"));
    candidate.handle_id = std::string(command_ == Command::LinearPattern ? kPatternSpacingHandleId
                                                                         : kPatternAngleHandleId);
    candidate.value_kind = angular_value() ? GuiViewportManipulatorValueKind::Angle
                                           : GuiViewportManipulatorValueKind::Distance;
  } else if (finishing()) {
    const auto handles = command_ == Command::Fillet || command_ == Command::Chamfer
                             ? coordinator_.finishing().handles()
                             : coordinator_.shell_draft().handles();
    if (handles.empty())
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("finishing", "zuerst gültige Eingaben und Elemente auswählen"));
    candidate.handle_id = handles.front().id;
    candidate.value_kind = angular_value() ? GuiViewportManipulatorValueKind::Angle
                                           : GuiViewportManipulatorValueKind::Distance;
  }
  if (!layer_.active_handle_id().empty()) {
    if (layer_.active_handle_id() != candidate.handle_id)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation(candidate.handle_id, "zuerst den aktiven Manipulatorzug abschließen"));
    const std::string unit = command_ == Command::BodyScale ? "" : angular_value() ? " deg" : " mm";
    if (!layer_.set_numeric_input(number_text(value).toStdString() + unit) ||
        !layer_.candidate().has_value())
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation(candidate.handle_id, "numerische Manipulatoreingabe ist ungültig"));
    candidate = *layer_.candidate();
  }
  return accept_manipulator_candidate(candidate);
}

Result<GuiPartFeaturePreview>
ShellFeatureTools::accept_manipulator_candidate(const GuiViewportManipulatorCandidate& candidate) {
  if (!active())
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.shell.feature", "kein aktives interaktives Feature"));
  const bool secondary =
      command_ == Command::Chamfer && (candidate.handle_id == kChamferDistance2HandleId ||
                                       candidate.handle_id == kChamferAngleHandleId);
  const double previous_value = value_for_handle(candidate.handle_id);
  if (!std::isfinite(candidate.scalar_value) || !coordinator_.on_candidate(candidate))
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation(candidate.handle_id.empty() ? "gui.shell.feature" : candidate.handle_id,
                          "Manipulator-Kandidat passt nicht zum aktiven Feature"));
  synchronize_manipulator_editor(candidate.handle_id);
  if (secondary) {
    finishing_second_value_ = coordinator_.finishing().second_value();
    secondary_numeric_editor_->setText(number_text(finishing_second_value_));
  }
  auto preview = preview_and_publish();
  if (!preview.has_error())
    return preview;

  // The controller is candidate-only, but it still owns the value used by the
  // next Apply. A failed geometry preview must therefore be transactional too:
  // restore the last value whose preview was visible, then republish that
  // preview instead of leaving a broken value and an empty viewport behind.
  const Error candidate_error = preview.error();
  GuiViewportManipulatorCandidate rollback = candidate;
  rollback.source = GuiViewportManipulatorInputSource::Numeric;
  rollback.scalar_value = previous_value;
  if (candidate.handle_id == kPatternCountHandleId)
    rollback.count_value = static_cast<int>(previous_value);
  if (!coordinator_.on_candidate(rollback)) {
    return Result<GuiPartFeaturePreview>::failure(Error::internal(
        candidate.handle_id.empty() ? "gui.shell.feature" : candidate.handle_id,
        "gültiger Manipulatorwert konnte nach Vorschaufehler nicht wiederhergestellt werden"));
  }
  synchronize_manipulator_editor(candidate.handle_id);
  if (secondary) {
    finishing_second_value_ = coordinator_.finishing().second_value();
    secondary_numeric_editor_->setText(number_text(finishing_second_value_));
  }
  auto restored = preview_and_publish();
  if (restored.has_error())
    report(restored.error());
  return Result<GuiPartFeaturePreview>::failure(candidate_error);
}

Result<GuiPartFeaturePreview> ShellFeatureTools::preview_and_publish() {
  preview_valid_ = false;
  if (command_ == Command::Extrude || command_ == Command::Revolve) {
    auto configured = command_ == Command::Extrude ? configure_extrude_candidate()
                                                   : configure_revolve_candidate();
    if (!configured) {
      viewport_.clear_feature_preview();
      task_summary_->setText(QString::fromStdString(configured.error().message()));
      refresh_enablement();
      synchronize_overlay();
      return Result<GuiPartFeaturePreview>::failure(configured.error());
    }
  }
  auto preview =
      pattern() && pattern_sources_->selectedItems().empty()
          ? Result<GuiPartFeaturePreview>::failure(
                Error::validation("pattern", "Mindestens einen Körper auswählen."))
      : finishing() && finishing_targets_list_->selectedItems().empty()
          ? Result<GuiPartFeaturePreview>::failure(Error::validation(
                "gui.shell.finishing", command_ == Command::Fillet || command_ == Command::Chamfer
                                           ? "Mindestens eine Kante auswählen."
                                           : "Mindestens eine Fläche auswählen."))
          : coordinator_.preview();
  if (preview.has_error()) {
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(preview.error().message()));
    refresh_enablement();
    synchronize_overlay();
    return preview;
  }
  Result<std::vector<geometry::GeometryShape>> shapes =
      Result<std::vector<geometry::GeometryShape>>::success({});
  if (body_operation())
    shapes = coordinator_.body_operation().preview_shapes(session_);
  else {
    auto shape = surface() ? coordinator_.surface().preview_shape(session_)
                 : command_ == Command::PathCurve || command_ == Command::Sweep
                     ? coordinator_.path_sweep().preview_shape(session_)
                 : command_ == Command::Loft    ? coordinator_.loft().preview_shape(session_)
                 : command_ == Command::Extrude ? coordinator_.extrude().preview_shape(session_)
                 : command_ == Command::Revolve ? coordinator_.revolve().preview_shape(session_)
                 : pattern() ? coordinator_.pattern_mirror().preview_shape(session_)
                 : command_ == Command::Fillet || command_ == Command::Chamfer
                     ? coordinator_.finishing().preview_shape(session_)
                     : coordinator_.shell_draft().preview_shape(session_);
    shapes = shape
                 ? Result<std::vector<geometry::GeometryShape>>::success({std::move(shape.value())})
                 : Result<std::vector<geometry::GeometryShape>>::failure(shape.error());
  }
  if (!shapes) {
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(shapes.error().message()));
    refresh_enablement();
    synchronize_overlay();
    return Result<GuiPartFeaturePreview>::failure(shapes.error());
  }
  std::vector<std::string> replacements;
  if (body_operation())
    replacements = body_preview_replacements();
  else if (command_ == Command::Extrude && extrude_operation_->currentIndex() != 0)
    replacements.push_back("body/" + extrude_target_->currentData().toString().toStdString());
  else if (command_ == Command::Revolve && revolve_operation_->currentIndex() != 0)
    replacements.push_back("body/" + revolve_target_->currentData().toString().toStdString());
  else if (spatial() && command_ != Command::PathCurve && spatial_mode_->currentIndex() == 1)
    replacements.push_back("body/" + spatial_target_->currentData().toString().toStdString());
  else if (finishing())
    replacements.push_back("body/" + finishing_body_->currentData().toString().toStdString());
  auto published =
      viewport_.set_feature_previews(std::move(shapes.value()), std::move(replacements));
  if (published.has_error()) {
    viewport_.clear_feature_preview();
    task_summary_->setText(QString::fromStdString(published.error().message()));
    refresh_enablement();
    synchronize_overlay();
    return Result<GuiPartFeaturePreview>::failure(published.error());
  }
  const auto summary =
      surface() ? task_dock_->windowTitle() + QStringLiteral(": Vorschau gültig.")
      : command_ == Command::Extrude
          ? QStringLiteral("%1 · %2: Vorschau gültig.")
                .arg(extrude_operation_->currentText(), extrude_direction_->currentIndex() == 2
                                                            ? extrude_path_->currentText()
                                                            : extrude_extent_->currentText())
      : command_ == Command::Revolve
          ? QStringLiteral("%1 · %2: Vorschau gültig.")
                .arg(revolve_operation_->currentText(), revolve_extent_->currentText())
      : spatial()        ? spatial_summary()
      : body_operation() ? body_summary()
      : finishing()      ? finishing_summary()
      : pattern()        ? pattern_summary()
                         : QString::fromStdString(preview.value().semantic_summary);
  task_summary_->setText(summary);
  preview_valid_ = true;
  refresh_enablement();
  window_.statusBar()->showMessage(QStringLiteral("Vorschau: %1").arg(summary));
  synchronize_overlay();
  return preview;
}

Result<std::size_t> ShellFeatureTools::apply() {
  if (!active())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.feature", "kein aktives Feature zum Anwenden"));
  if (!layer_.active_handle_id().empty())
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.feature", "aktiven Manipulatorzug vor dem Anwenden abschließen"));
  const std::string_view expected_command = command_id();
  if (session_.task().stage() != GuiTaskStage::Preview ||
      session_.task().command_id() != expected_command) {
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.feature", "Feature-Aufgabe ist nicht in einem anwendbaren Vorschauzustand"));
  }
  auto preview = preview_and_publish();
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto applied = coordinator_.apply();
  if (applied.has_error())
    return applied;
  QAction* accepted_action = command_ == Command::Extrude   ? action_extrude_
                             : command_ == Command::Revolve ? action_revolve_
                                                            : nullptr;
  for (const auto* actions : {&finishing_actions_, &pattern_actions_, &body_actions_,
                              &spatial_actions_, &surface_actions_})
    for (const auto& [command, action] : *actions)
      if (command == command_)
        accepted_action = action;
  // The stage and command id were checked immediately before the synchronous
  // controller commit, and that commit does not mutate GuiTaskState.  Applying
  // the task is therefore deterministic here.  Normalize an unexpected future
  // invariant break instead of reporting a failed Apply after the document was
  // already committed successfully.
  const bool task_applied = session_.task().apply();
  if (!task_applied && session_.task().active())
    (void)session_.task().cancel();
  window_.finish_profile_context(true);
  const bool was_finishing = finishing();
  const bool was_extrude = command_ == Command::Extrude;
  const bool restore_selection = pattern() || body_operation() || spatial() || surface();
  const bool restore_scope = spatial() || surface();
  command_ = Command::None;
  if (restore_scope)
    viewport_.set_selection_semantic_scope(previous_semantic_scope_);
  if (restore_selection) {
    pattern_reference_picking_ = false;
    body_reference_picking_ = false;
    viewport_.set_selection_filter_mask(previous_selection_mask_);
  }
  if (was_finishing) {
    finishing_targets_.clear();
    viewport_.set_selection_filter_mask(previous_selection_mask_);
    window_.refresh_feature_selection_scene();
  }
  surface_targets_.clear();
  preview_valid_ = false;
  drag_start_value_.reset();
  selected_profile_.reset();
  viewport_.clear_feature_preview();
  if (was_extrude)
    restore_extrude_selection();
  hide_task_panel();
  synchronize_overlay();
  window_.refresh();
  window_.statusBar()->showMessage(QStringLiteral("Feature angewendet"));
  if (accepted_action)
    window_.remember_repeatable_command(*accepted_action);
  return applied;
}

void ShellFeatureTools::cancel() noexcept {
  const bool was_active = command_ != Command::None || coordinator_.active();
  if (coordinator_.active())
    coordinator_.cancel();
  if (viewport_.mouseGrabber() == &viewport_)
    viewport_.release_interaction_mouse();
  viewport_.clear_feature_preview();
  layer_.cancel();
  drag_start_value_.reset();
  if (command_ != Command::None && session_.task().active())
    (void)session_.task().cancel();
  const bool was_finishing = finishing();
  const bool was_extrude = command_ == Command::Extrude;
  const bool restore_selection = pattern() || body_operation() || spatial() || surface();
  const bool restore_scope = spatial() || surface();
  command_ = Command::None;
  if (restore_scope)
    viewport_.set_selection_semantic_scope(previous_semantic_scope_);
  if (restore_selection) {
    pattern_reference_picking_ = false;
    body_reference_picking_ = false;
    viewport_.set_selection_filter_mask(previous_selection_mask_);
  }
  if (was_finishing) {
    finishing_targets_.clear();
    viewport_.set_selection_filter_mask(previous_selection_mask_);
    window_.refresh_feature_selection_scene();
  }
  surface_targets_.clear();
  preview_valid_ = false;
  if (was_extrude)
    restore_extrude_selection();
  hide_task_panel();
  synchronize_overlay();
  window_.finish_profile_context(false);
  window_.refresh();
  if (was_active)
    window_.statusBar()->showMessage(QStringLiteral("Feature-Befehl abgebrochen"));
}

void ShellFeatureTools::reset() noexcept {
  cancel();
  selected_profile_.reset();
  selected_open_line_.reset();
  window_.refresh();
}

void ShellFeatureTools::refresh_enablement() {
  const bool idle_part =
      !active() && !session_.task().active() && !window_.active_sketch().has_value() &&
      session_.document_kind() == GuiDocumentKind::Part &&
      session_.workspace() == GuiWorkspace::Part && !session_.reference_repair_mode();
  const bool profile_available = idle_part && !profile_from_selection().has_error();
  action_extrude_->setEnabled(profile_available ||
                              (idle_part && open_line_from_selection().has_value()));
  bool revolve_available = false;
  if (profile_available) {
    const auto plane = selected_profile_plane();
    revolve_available = !plane.has_error() && !revolve_axis_choices(plane.value()).empty();
  }
  action_revolve_->setEnabled(revolve_available);
  for (const auto& [command, action] : finishing_actions_) {
    bool available = false;
    if (idle_part && session_.part_shape_cache())
      for (const auto& body : session_.part_document()->bodies()) {
        const auto role = command == Command::Fillet    ? PartFeatureInputRole::FilletEdge
                          : command == Command::Chamfer ? PartFeatureInputRole::ChamferEdge
                          : command == Command::Shell   ? PartFeatureInputRole::ShellRemovalFace
                                                        : PartFeatureInputRole::DraftFace;
        auto targets = geometry::FinishingSelectionBuilder::build(
            *session_.part_document(), *session_.part_shape_cache(), body.id(), role);
        if (targets && !targets.value().empty()) {
          available = true;
          break;
        }
      }
    action->setEnabled(available);
  }
  const bool source_available =
      idle_part && session_.part_shape_cache() &&
      std::any_of(session_.part_document()->bodies().begin(),
                  session_.part_document()->bodies().end(), [&](const auto& body) {
                    return body.kind() == BodyKind::Solid &&
                           body.visibility() == BodyVisibility::Visible &&
                           session_.part_shape_cache()->find_body_shape(body.id());
                  });
  for (const auto& [command, action] : pattern_actions_) {
    const auto* part = session_.part_document();
    const bool reference_available =
        source_available &&
        (command == Command::BodyMirror
             ? !part->datum_planes().empty() || !part->construction_planes().empty()
             : !part->datum_axes().empty() || !part->construction_lines().empty());
    action->setEnabled(source_available && reference_available);
  }
  refresh_body_enablement(idle_part);
  refresh_spatial_enablement(idle_part);
  refresh_surface_enablement(idle_part);
  action_escape_->setEnabled(active());
  const std::string_view expected_command = command_id();
  const bool task_ready = session_.task().stage() == GuiTaskStage::Preview &&
                          session_.task().command_id() == expected_command;
  apply_button_->setEnabled(active() && preview_valid_ && task_ready);
  cancel_button_->setEnabled(active());
  window_.refresh_profile_context();
}

double ShellFeatureTools::current_value() const noexcept {
  if (command_ == Command::Extrude)
    return coordinator_.extrude().distance();
  if (command_ == Command::Revolve)
    return coordinator_.revolve().angle();
  if (command_ == Command::Fillet || command_ == Command::Chamfer)
    return coordinator_.finishing().active() ? coordinator_.finishing().first_value()
                                             : finishing_first_value_;
  if (command_ == Command::Shell)
    return coordinator_.shell_draft().thickness();
  if (command_ == Command::Draft)
    return coordinator_.shell_draft().angle();
  if (command_ == Command::Sweep)
    return coordinator_.path_sweep().active() ? coordinator_.path_sweep().twist() : spatial_twist_;
  if (surface_scalar())
    return coordinator_.surface().active()
               ? (command_ == Command::ExtendSurface ? coordinator_.surface().distance()
                                                     : coordinator_.surface().tolerance())
               : surface_value_;
  if (body_transform()) {
    if (!coordinator_.body_operation().active())
      return command_ == Command::BodyTranslate ? body_translation_.x : body_value_;
    const auto& body = coordinator_.body_operation();
    return command_ == Command::BodyTranslate ? body.translation().x
           : command_ == Command::BodyRotate  ? body.angle()
                                              : body.scale_factor();
  }
  if (pattern())
    return coordinator_.pattern_mirror().active()
               ? (command_ == Command::LinearPattern ? coordinator_.pattern_mirror().spacing()
                                                     : coordinator_.pattern_mirror().angle())
               : pattern_value_;
  return 0.0;
}

double ShellFeatureTools::value_for_handle(std::string_view handle) const noexcept {
  if (command_ == Command::Extrude && handle == kExtrudeTaperHandleId)
    return coordinator_.extrude().taper().value_or(0);
  if (command_ == Command::Extrude && handle == kExtrudeThinHandleId)
    return coordinator_.extrude().thin_thickness();
  if (command_ == Command::BodyTranslate) {
    const auto value = coordinator_.body_operation().translation();
    return handle.ends_with(".y") ? value.y : handle.ends_with(".z") ? value.z : value.x;
  }
  if (pattern() && handle == kPatternCountHandleId)
    return coordinator_.pattern_mirror().count();
  if (command_ == Command::Chamfer &&
      (handle == kChamferDistance2HandleId || handle == kChamferAngleHandleId))
    return coordinator_.finishing().second_value();
  return current_value();
}

bool ShellFeatureTools::update_mapping() {
  if (viewport_.width() <= 0 || viewport_.height() <= 0)
    return false;
  auto mapping = GuiViewportManipulatorMapping::create(
      [this](GuiSketchScreenPoint point) { return viewport_.screen_to_view_ray(point); },
      [this](Point3 point) { return viewport_.model_to_screen(point); });
  if (mapping.has_error()) {
    report(mapping.error());
    return false;
  }
  return !layer_.set_mapping(std::move(mapping.value())).has_error();
}

void ShellFeatureTools::synchronize_numeric_editor(double value) {
  const QSignalBlocker blocker(numeric_editor_);
  synchronizing_numeric_ = true;
  numeric_editor_->setText(number_text(value));
  if (command_ == Command::Extrude && coordinator_.extrude().taper()) {
    const QSignalBlocker taper_blocker(extrude_taper_);
    extrude_taper_->setText(QString::number(*coordinator_.extrude().taper(), 'g', 17));
  }
  if (pattern())
    synchronize_pattern_editors();
  if (body_operation())
    synchronize_body_editors();
  if (command_ == Command::Sweep && coordinator_.path_sweep().active())
    spatial_twist_ = coordinator_.path_sweep().twist();
  if (surface_scalar() && coordinator_.surface().active())
    surface_value_ = current_value();
  synchronizing_numeric_ = false;
}

void ShellFeatureTools::synchronize_manipulator_editor(std::string_view handle) {
  synchronize_numeric_editor(current_value());
  if (command_ == Command::Extrude && handle == kExtrudeThinHandleId) {
    const QSignalBlocker blocker(extrude_thickness_);
    extrude_thickness_->setText(QString::number(coordinator_.extrude().thin_thickness(), 'g', 17));
  }
}

void ShellFeatureTools::synchronize_overlay() {
  if (command_ == Command::Extrude)
    extrude_controls_->setEnabled(layer_.active_handle_id().empty());
  if (command_ == Command::Revolve)
    revolve_controls_->setEnabled(layer_.active_handle_id().empty());
  const bool visible = active() && preview_valid_ && !layer_.handles().empty() &&
                       !pattern_reference_picking_ && !body_reference_picking_ &&
                       !extrude_reference_picking_;
  overlay_->setVisible(visible);
  if (visible) {
    overlay_->setGeometry(viewport_.rect());
    overlay_->raise();
    overlay_->update();
  }
}

void ShellFeatureTools::refresh_presentation() {
  // Reporting a failed candidate advances the presentation revision. Scene
  // rebuilding then clears OCCT selection, but the active local reference role
  // still owns its highlight, including invalid limits.
  if (extrude_reference_picking_ && !viewport_.selected_semantic())
    update_extrude_pick_mode();
  (void)update_mapping();
  synchronize_overlay();
}

void ShellFeatureTools::restore_drag_start_value() {
  if (!drag_start_value_.has_value())
    return;
  const double value = *drag_start_value_;
  drag_start_value_.reset();
  GuiViewportManipulatorCandidate candidate;
  candidate.handle_id = drag_start_handle_;
  candidate.value_kind = angular_value() || drag_start_handle_ == kChamferAngleHandleId ||
                                 drag_start_handle_ == kExtrudeTaperHandleId
                             ? GuiViewportManipulatorValueKind::Angle
                             : GuiViewportManipulatorValueKind::Distance;
  candidate.source = GuiViewportManipulatorInputSource::Numeric;
  candidate.scalar_value = value;
  if (command_ == Command::BodyTranslate)
    candidate.value_kind = GuiViewportManipulatorValueKind::Translation;
  if (command_ == Command::BodyScale)
    candidate.value_kind = GuiViewportManipulatorValueKind::ScaleFactor;
  if (drag_start_handle_ == kPatternCountHandleId) {
    candidate.value_kind = GuiViewportManipulatorValueKind::Count;
    candidate.count_value = static_cast<int>(value);
  }
  const auto restored = accept_manipulator_candidate(candidate);
  if (restored.has_error())
    report(restored.error());
}

void ShellFeatureTools::handle_escape() {
  if (!active())
    return;
  if (!layer_.active_handle_id().empty()) {
    layer_.cancel();
    if (viewport_.mouseGrabber() == &viewport_)
      viewport_.release_interaction_mouse();
    restore_drag_start_value();
    synchronize_overlay();
    return;
  }
  cancel();
}

void ShellFeatureTools::show_task_panel(QString title, QString unit, double value) {
  extrude_controls_->setVisible(command_ == Command::Extrude);
  revolve_controls_->setVisible(command_ == Command::Revolve);
  finishing_controls_->setVisible(finishing());
  pattern_controls_->setVisible(pattern());
  body_controls_->setVisible(body_operation());
  spatial_controls_->setVisible(spatial());
  surface_controls_->setVisible(surface());
  value_label_->setVisible(command_ != Command::BodyMirror && command_ != Command::BodyBoolean &&
                           (!spatial() || spatial_scalar()) && (!surface() || surface_scalar()));
  numeric_editor_->setVisible(command_ != Command::BodyMirror && command_ != Command::BodyBoolean &&
                              (!spatial() || spatial_scalar()) && (!surface() || surface_scalar()));
  task_dock_->setWindowTitle(std::move(title));
  const QString label = command_ == Command::SurfaceStitch   ? QStringLiteral("Toleranz")
                        : command_ == Command::ExtendSurface ? QStringLiteral("Abstand")
                        : command_ == Command::Sweep         ? QStringLiteral("Verdrehung")
                        : command_ == Command::BodyTranslate ? QStringLiteral("X")
                        : command_ == Command::BodyScale     ? QStringLiteral("Faktor")
                        : command_ == Command::Fillet        ? QStringLiteral("Radius")
                        : command_ == Command::Chamfer       ? QStringLiteral("Abstand")
                        : command_ == Command::Shell         ? QStringLiteral("Wandstärke")
                        : command_ == Command::LinearPattern ? QStringLiteral("Abstand / Länge")
                        : angular_value()                    ? QStringLiteral("Winkel")
                                                             : QStringLiteral("Tiefe");
  value_label_->setText(unit.isEmpty() ? label
                                       : label + QStringLiteral(" (") + unit + QStringLiteral(")"));
  numeric_editor_->setPlaceholderText(std::move(unit));
  synchronize_numeric_editor(value);
  if (command_ == Command::Revolve)
    update_revolve_controls();
  task_dock_->show();
  task_dock_->raise();
  refresh_presentation();
}

void ShellFeatureTools::hide_task_panel() noexcept {
  task_dock_->hide();
  task_summary_->clear();
  numeric_editor_->clear();
}

void ShellFeatureTools::report(const Error& error) {
  window_.report_error(error);
}

bool ShellFeatureTools::eventFilter(QObject* watched, QEvent* event) {
  if (watched == &viewport_) {
    switch (event->type()) {
    case QEvent::Resize:
      overlay_->setGeometry(viewport_.rect());
      refresh_presentation();
      break;
    case QEvent::Wheel:
      QMetaObject::invokeMethod(this, [this] { refresh_presentation(); }, Qt::QueuedConnection);
      break;
    case QEvent::MouseButtonPress: {
      if (!active())
        break;
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && preview_valid_ && !pattern_reference_picking_ &&
          !body_reference_picking_ && !extrude_reference_picking_) {
        (void)update_mapping();
        if (layer_.begin_drag({mouse->position().x(), mouse->position().y()})) {
          drag_start_handle_ = std::string(layer_.active_handle_id());
          drag_start_value_ = value_for_handle(drag_start_handle_);
          viewport_.grab_interaction_mouse();
          synchronize_overlay();
          return true;
        }
      }
      break;
    }
    case QEvent::MouseMove: {
      if (layer_.active_handle_id().empty())
        break;
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (layer_.update_drag({mouse->position().x(), mouse->position().y()})) {
        if (layer_.candidate().has_value())
          (void)accept_manipulator_candidate(*layer_.candidate());
        synchronize_overlay();
        return true;
      }
      layer_.cancel();
      if (viewport_.mouseGrabber() == &viewport_)
        viewport_.release_interaction_mouse();
      restore_drag_start_value();
      synchronize_overlay();
      return true;
    }
    case QEvent::MouseButtonRelease: {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && !layer_.active_handle_id().empty()) {
        auto candidate = layer_.end_drag({mouse->position().x(), mouse->position().y()});
        viewport_.release_interaction_mouse();
        if (candidate.has_value()) {
          drag_start_value_.reset();
          auto accepted = accept_manipulator_candidate(*candidate);
          if (accepted.has_error())
            report(accepted.error());
        } else {
          restore_drag_start_value();
        }
        synchronize_overlay();
        return true;
      }
      QMetaObject::invokeMethod(this, [this] { refresh_presentation(); }, Qt::QueuedConnection);
      break;
    }
    default:
      break;
    }
  }

  if (event->type() == QEvent::KeyPress && active()) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      handle_escape();
      return true;
    }
  } else if (event->type() == QEvent::WindowDeactivate && !layer_.active_handle_id().empty()) {
    layer_.cancel();
    if (viewport_.mouseGrabber() == &viewport_)
      viewport_.release_interaction_mouse();
    restore_drag_start_value();
    synchronize_overlay();
  }
  return QObject::eventFilter(watched, event);
}

} // namespace blcad::gui
