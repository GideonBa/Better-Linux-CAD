#include "blcad/gui/shell/shell_sketch_tools.hpp"

#include "blcad/core/sketch_edit_commands.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"
#include "blcad/gui/gui_sketch_modify.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <utility>

namespace blcad::gui {
namespace {

constexpr double kHandleToleranceDip = 9.0;

struct ToolDescriptor {
  const char* command;
  const char* label;
  GuiSketchCreateTool tool;
  const char* hint;
};

// Tool families as Inventor-style menu buttons; covers every MVP-8 create tool.
// Inventor-style: ONE line tool that chains segments (each click continues at
// the previous end, corners are coupled coincident); Escape ends the chain.
// The headless Polyline tool stays available programmatically but has no
// separate ribbon entry.
constexpr std::array<ToolDescriptor, 2> kLineTools{{
    {"sketch.create.line", "Linie", GuiSketchCreateTool::Line,
     "Punkte wählen; jede weitere Linie setzt am Ende an, Esc beendet"},
    {"sketch.create.centerline", "Mittellinie", GuiSketchCreateTool::Centerline,
     "Start- und Endpunkt der Mittellinie wählen"},
}};
constexpr std::array<ToolDescriptor, 5> kRectangleTools{{
    {"sketch.create.corner_rectangle", "Rechteck (2 Punkte)",
     GuiSketchCreateTool::CornerRectangle, "Zwei gegenüberliegende Ecken wählen"},
    {"sketch.create.center_rectangle", "Rechteck (Mitte)", GuiSketchCreateTool::CenterRectangle,
     "Mittelpunkt und eine Ecke wählen"},
    {"sketch.create.three_point_rectangle", "Rechteck (3 Punkte)",
     GuiSketchCreateTool::ThreePointRectangle, "Basiskante und Höhenpunkt wählen"},
    {"sketch.create.parallelogram", "Parallelogramm", GuiSketchCreateTool::Parallelogram,
     "Drei Ecken wählen"},
    {"sketch.create.polygon", "Polygon", GuiSketchCreateTool::RegularPolygon,
     "Seitenzahl tippen, dann Mitte und Eckpunkt wählen"},
}};
constexpr std::array<ToolDescriptor, 6> kCircleTools{{
    {"sketch.create.center_radius_circle", "Kreis (Mitte/Radius)",
     GuiSketchCreateTool::CenterRadiusCircle, "Mittelpunkt und Radiuspunkt wählen"},
    {"sketch.create.center_diameter_circle", "Kreis (Mitte/Durchmesser)",
     GuiSketchCreateTool::CenterDiameterCircle, "Mittelpunkt und Durchmesserpunkt wählen"},
    {"sketch.create.two_point_circle", "Kreis (2 Punkte)", GuiSketchCreateTool::TwoPointCircle,
     "Beide Durchmesser-Endpunkte wählen"},
    {"sketch.create.three_point_circle", "Kreis (3 Punkte)",
     GuiSketchCreateTool::ThreePointCircle, "Drei Kreispunkte wählen"},
    {"sketch.create.tangent_circle", "Kreis (tangential)", GuiSketchCreateTool::TangentCircle,
     "Drei Tangentenkandidaten wählen"},
    {"sketch.create.ellipse", "Ellipse", GuiSketchCreateTool::Ellipse,
     "Mitte, Hauptachse und Nebenradius wählen"},
}};
constexpr std::array<ToolDescriptor, 4> kArcTools{{
    {"sketch.create.center_start_end_arc", "Bogen (Mitte/Start/Ende)",
     GuiSketchCreateTool::CenterStartEndArc, "Mitte, Start und Endrichtung wählen"},
    {"sketch.create.three_point_arc", "Bogen (3 Punkte)", GuiSketchCreateTool::ThreePointArc,
     "Start, Zwischenpunkt und Ende wählen"},
    {"sketch.create.tangent_arc", "Bogen (tangential)", GuiSketchCreateTool::TangentArc,
     "Start, Zwischenpunkt und Ende wählen"},
    {"sketch.create.elliptical_arc", "Elliptischer Bogen", GuiSketchCreateTool::EllipticalArc,
     "Mitte, Hauptachse, Nebenradius, Start und Ende wählen"},
}};
constexpr std::array<ToolDescriptor, 2> kSlotTools{{
    {"sketch.create.center_slot", "Langloch (Mitte–Mitte)", GuiSketchCreateTool::CenterSlot,
     "Beide Kappenmitten und halbe Breite wählen"},
    {"sketch.create.overall_slot", "Langloch (Gesamtlänge)", GuiSketchCreateTool::OverallSlot,
     "Gesamt-Endpunkte und halbe Breite wählen"},
}};
constexpr ToolDescriptor kPointTool{"sketch.create.point", "Punkt", GuiSketchCreateTool::Point,
                                    "Position des Konstruktionspunkts wählen"};

struct ConstraintDescriptor {
  SketchSolverConstraintKind kind;
  const char* label;
  const char* description;
};

// PointOnObject and Midpoint have no own buttons: Koinzident on a point +
// curve/circle maps to point-on-object, Koinzident with a clicked line
// midpoint maps to Midpoint (Inventor semantics).
constexpr std::array<ConstraintDescriptor, 11> kConstraints{{
    {SketchSolverConstraintKind::Coincident, "Koinzident",
     "Punkte verbinden oder Punkt auf Kurve/Kreis setzen"},
    {SketchSolverConstraintKind::Fixed, "Fixiert", "Punkt oder Kurve fixieren"},
    {SketchSolverConstraintKind::Horizontal, "Horizontal", "Linie horizontal ausrichten"},
    {SketchSolverConstraintKind::Vertical, "Vertikal", "Linie vertikal ausrichten"},
    {SketchSolverConstraintKind::Parallel, "Parallel", "Zwei Linien parallel setzen"},
    {SketchSolverConstraintKind::Perpendicular, "Senkrecht", "Zwei Linien senkrecht setzen"},
    {SketchSolverConstraintKind::Collinear, "Kollinear", "Zwei Linien kollinear setzen"},
    {SketchSolverConstraintKind::Equal, "Gleich", "Zwei Linien/Bögen gleich groß setzen"},
    {SketchSolverConstraintKind::Tangent, "Tangential", "Zwei verbundene Kurven tangential"},
    {SketchSolverConstraintKind::Concentric, "Konzentrisch", "Zwei Mittelpunkte vereinen"},
    {SketchSolverConstraintKind::Symmetric, "Symmetrisch",
     "Zwei Punkte symmetrisch zu einer Linie"},
}};

struct DimensionDescriptor {
  SketchDimensionKind kind;
  const char* label;
};

constexpr std::array<DimensionDescriptor, 9> kDimensions{{
    {SketchDimensionKind::HorizontalDistance, "Horizontaler Abstand"},
    {SketchDimensionKind::VerticalDistance, "Vertikaler Abstand"},
    {SketchDimensionKind::AlignedDistance, "Ausgerichteter Abstand"},
    {SketchDimensionKind::PointToPointDistance, "Punkt-zu-Punkt"},
    {SketchDimensionKind::Length, "Länge"},
    {SketchDimensionKind::Radius, "Radius"},
    {SketchDimensionKind::Diameter, "Durchmesser"},
    {SketchDimensionKind::Angle, "Winkel"},
    {SketchDimensionKind::ArcLength, "Bogenlänge"},
}};

[[nodiscard]] bool sketch_idle_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::Idle || stage == GuiSketchInteractionStage::Hover;
}

[[nodiscard]] bool collecting_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::CollectingPicks ||
         stage == GuiSketchInteractionStage::NumericInput ||
         stage == GuiSketchInteractionStage::Preview;
}

[[nodiscard]] bool drag_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::SelectedHandle ||
         stage == GuiSketchInteractionStage::DragCandidate;
}

[[nodiscard]] std::string solve_status_text(SketchSolveStatus status) {
  switch (status) {
  case SketchSolveStatus::FullyConstrained: return "Voll bestimmt";
  case SketchSolveStatus::UnderConstrained: return "Unterbestimmt";
  case SketchSolveStatus::Redundant: return "Redundant";
  case SketchSolveStatus::Conflicting: return "Widersprüchlich";
  case SketchSolveStatus::NonConvergent: return "Nicht konvergent";
  case SketchSolveStatus::InvalidReference: return "Ungültige Referenz";
  }
  return "Nicht ausgewertet";
}

[[nodiscard]] bool numeric_character(QChar character) noexcept {
  return character.isDigit() || character == QChar('.') || character == QChar('-') ||
         character == QChar('<') || character == QChar(';');
}

[[nodiscard]] double screen_distance(GuiSketchScreenPoint first,
                                     GuiSketchScreenPoint second) noexcept {
  return std::hypot(second.x - first.x, second.y - first.y);
}

[[nodiscard]] std::optional<SketchConstraintIntentTarget>
selection_target(const SketchTopology& topology, std::string semantic_id) {
  const auto entity =
      [&topology](const std::string& id) -> std::optional<SketchConstraintIntentTarget> {
    if (topology.find_entity(id) == nullptr)
      return std::nullopt;
    auto target = SketchConstraintIntentTarget::entity(id);
    return target.has_error() ? std::nullopt
                              : std::optional<SketchConstraintIntentTarget>(target.value());
  };
  const auto point =
      [&topology](const std::string& id) -> std::optional<SketchConstraintIntentTarget> {
    if (topology.find_point(SketchPointId(id)) == nullptr)
      return std::nullopt;
    auto target = SketchConstraintIntentTarget::point(SketchPointId(id));
    return target.has_error() ? std::nullopt
                              : std::optional<SketchConstraintIntentTarget>(target.value());
  };

  if (auto target = entity(semantic_id))
    return target;
  if (auto target = point(semantic_id))
    return target;

  const std::string sketch_prefix = "sketch/" + topology.sketch().value() + "/";
  if (std::string_view(semantic_id).starts_with(sketch_prefix))
    semantic_id.erase(0U, sketch_prefix.size());
  if (auto target = entity(semantic_id))
    return target;
  if (auto target = point(semantic_id))
    return target;

  constexpr std::array<std::string_view, 3U> curve_prefixes{"line/", "arc/", "spline/"};
  for (const auto prefix : curve_prefixes)
    if (std::string_view(semantic_id).starts_with(prefix))
      if (auto target = entity("entity/" + semantic_id.substr(prefix.size())))
        return target;

  if (std::string_view(semantic_id).starts_with("point/")) {
    if (auto target = point(semantic_id))
      return target;
    if (auto target = point(semantic_id.substr(std::string_view("point/").size())))
      return target;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<SketchPointId>
resolve_point_role(const SketchTopology& topology, const SketchTopologyEntity& entity,
                   std::string_view candidate) {
  const SketchPointId direct{std::string(candidate)};
  if (topology.find_point(direct) != nullptr)
    return direct;
  const auto separator = candidate.rfind(':');
  if (separator == std::string_view::npos)
    return std::nullopt;
  const std::string_view role = candidate.substr(separator + 1U);
  if (entity.kind() == SketchTopologyEntityKind::Line) {
    if (role == "start")
      return entity.points()[0];
    if (role == "end")
      return entity.points()[1];
  }
  if (entity.kind() == SketchTopologyEntityKind::Arc) {
    if (role == "start")
      return entity.points()[0];
    if (role == "mid" || role == "midpoint")
      return entity.points()[1];
    if (role == "end")
      return entity.points()[2];
  }
  if (entity.kind() == SketchTopologyEntityKind::Spline) {
    if (role == "start")
      return entity.points()[0];
    if (role == "end")
      return entity.points()[3];
  }
  if ((entity.kind() == SketchTopologyEntityKind::CircleProfile ||
       entity.kind() == SketchTopologyEntityKind::RectangleProfile) &&
      role == "center" && !entity.points().empty())
    return entity.points()[0];
  return std::nullopt;
}

[[nodiscard]] SketchConstraintId next_constraint_id(const SketchConstraintCatalog& catalog,
                                                    SketchSolverConstraintKind kind) {
  const std::string base = "constraint.gui." + std::string(to_string(kind)) + ".";
  std::size_t index = 1U;
  while (catalog.find(SketchConstraintId(base + std::to_string(index))) != nullptr)
    ++index;
  return SketchConstraintId(base + std::to_string(index));
}

[[nodiscard]] SketchDimensionId next_dimension_id(const SketchDimensionCatalog& catalog,
                                                  SketchDimensionKind kind) {
  const std::string prefix = "dimension.gui." + std::string(to_string(kind)) + ".";
  std::size_t index = 1U;
  while (catalog.find(SketchDimensionId(prefix + std::to_string(index))) != nullptr)
    ++index;
  return SketchDimensionId(prefix + std::to_string(index));
}

// Removes a sketch entity and the points it leaves orphaned, in one topology
// edit. Removing only the entity strands its endpoints, which the legacy
// PartDocument JSON cannot represent (identity-loss), so the points must go
// too. Points still referenced by another entity are kept.
[[nodiscard]] Result<std::size_t>
delete_sketch_entities(PartDocument& document, const SketchId& sketch,
                       const std::vector<std::string>& entity_ids) {
  const Sketch* source = document.find_sketch(sketch);
  if (source == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(sketch.value(), "aktive Skizze existiert nicht mehr"));
  const Sketch snapshot = *source;
  auto topology = SketchTopology::migrate_legacy(snapshot);
  if (topology.has_error())
    return Result<std::size_t>::failure(topology.error());
  SketchTopology current = topology.value();

  std::vector<SketchPointId> owned_points;
  for (const auto& entity_id : entity_ids) {
    const SketchTopologyEntity* target = current.find_entity(entity_id);
    if (target == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(entity_id, "Skizzengeometrie existiert nicht"));
    owned_points.insert(owned_points.end(), target->points().begin(), target->points().end());
    auto remove_entity = SketchEditCommand::remove_entity(entity_id);
    if (remove_entity.has_error())
      return Result<std::size_t>::failure(remove_entity.error());
    auto after_entity = SketchEditCommandExecutor{}.apply(current, remove_entity.value());
    if (after_entity.has_error())
      return Result<std::size_t>::failure(after_entity.error());
    current = after_entity.value().after();
  }

  for (const SketchPointId& point : owned_points) {
    const bool still_referenced =
        std::any_of(current.entities().begin(), current.entities().end(), [&point](const auto& e) {
          return std::find(e.points().begin(), e.points().end(), point) != e.points().end();
        });
    if (still_referenced || current.find_point(point) == nullptr)
      continue;
    auto remove_point = SketchEditCommand::remove_point(point);
    if (remove_point.has_error())
      return Result<std::size_t>::failure(remove_point.error());
    auto after_point = SketchEditCommandExecutor{}.apply(current, remove_point.value());
    if (after_point.has_error())
      return Result<std::size_t>::failure(after_point.error());
    current = after_point.value().after();
  }

  auto materialized = SketchTopologyLegacyMaterializer{}.materialize(snapshot, current);
  if (materialized.has_error())
    return Result<std::size_t>::failure(materialized.error());
  auto representable = SketchTopology::migrate_legacy(materialized.value());
  if (representable.has_error())
    return Result<std::size_t>::failure(representable.error());
  if (representable.value() != current)
    return Result<std::size_t>::failure(Error::validation(
        sketch.value(),
        "Löschen verletzt referenzierte Bemaßungen/Abhängigkeiten – diese zuerst entfernen"));
  return document.update_sketch(std::move(materialized.value()));
}

void canonicalize_order(const SketchTopology& topology, SketchDimensionKind kind,
                        std::vector<SketchConstraintIntentTarget>& targets) {
  if (targets.size() != 2U)
    return;
  if ((kind == SketchDimensionKind::HorizontalDistance ||
       kind == SketchDimensionKind::VerticalDistance) &&
      targets[0].kind() == SketchConstraintIntentTargetKind::Point &&
      targets[1].kind() == SketchConstraintIntentTargetKind::Point) {
    const Point2 first = topology.find_point(SketchPointId(targets[0].id()))->position();
    const Point2 second = topology.find_point(SketchPointId(targets[1].id()))->position();
    if ((kind == SketchDimensionKind::HorizontalDistance && second.x < first.x) ||
        (kind == SketchDimensionKind::VerticalDistance && second.y < first.y))
      std::swap(targets[0], targets[1]);
  }
  if (kind != SketchDimensionKind::Angle ||
      targets[0].kind() != SketchConstraintIntentTargetKind::Entity ||
      targets[1].kind() != SketchConstraintIntentTargetKind::Entity)
    return;
  const auto* first = topology.find_entity(targets[0].id());
  const auto* second = topology.find_entity(targets[1].id());
  if (first == nullptr || second == nullptr || first->points().size() != 2U ||
      second->points().size() != 2U)
    return;
  const Point2 a0 = topology.find_point(first->points()[0])->position();
  const Point2 a1 = topology.find_point(first->points()[1])->position();
  const Point2 b0 = topology.find_point(second->points()[0])->position();
  const Point2 b1 = topology.find_point(second->points()[1])->position();
  const double ax = a1.x - a0.x;
  const double ay = a1.y - a0.y;
  const double bx = b1.x - b0.x;
  const double by = b1.y - b0.y;
  if (std::atan2(ax * by - ay * bx, ax * bx + ay * by) < 0.0)
    std::swap(targets[0], targets[1]);
}

} // namespace

ShellSketchTools::ShellSketchTools(ShellWindow& window, GuiDocumentSession& session,
                                   GuiSketchWorkbench& workbench, GuiSketchWorkspace& workspace,
                                   OcctViewport& viewport, ShellRibbon& ribbon, int sketch_tab)
    : QObject(&window), window_(window), session_(session), workbench_(workbench),
      workspace_(workspace), viewport_(viewport), ribbon_(ribbon), sketch_tab_(sketch_tab) {
  build_ribbon_groups();
  wire_viewport_callbacks();
  viewport_.installEventFilter(this);
  window_.installEventFilter(this);
}

void ShellSketchTools::build_ribbon_groups() {
  const auto make_tool_action = [this](const ToolDescriptor& descriptor) {
    auto* action = new QAction(QString::fromUtf8(descriptor.label), this);
    action->setToolTip(QString::fromUtf8(descriptor.hint));
    connect(action, &QAction::triggered, this,
            [this, tool = descriptor.tool] { (void)begin_tool(tool); });
    tool_actions_.push_back(action);
    return action;
  };
  const auto make_tool_menu = [&](auto& descriptors) {
    QList<QAction*> actions;
    for (const auto& descriptor : descriptors)
      actions.push_back(make_tool_action(descriptor));
    return actions;
  };

  auto* draw_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Zeichnen"));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Linie"), make_tool_menu(kLineTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Rechteck"),
                          make_tool_menu(kRectangleTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Kreis"), make_tool_menu(kCircleTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Bogen"), make_tool_menu(kArcTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Langloch"), make_tool_menu(kSlotTools));
  ribbon_.add_button(draw_group, make_tool_action(kPointTool));

  auto* constrain_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Abhängigkeiten"));
  for (const auto& descriptor : kConstraints) {
    auto* action = new QAction(QString::fromUtf8(descriptor.label), this);
    action->setToolTip(QString::fromUtf8(descriptor.description));
    connect(action, &QAction::triggered, this, [this, kind = descriptor.kind] {
      const auto applied = apply_constraint(kind);
      if (applied.has_error())
        report(applied.error());
      window_.refresh();
    });
    ribbon_.add_small_button(constrain_group, action);
    constraint_actions_.push_back(action);
  }
  delete_constraint_action_ = new QAction(QStringLiteral("Löschen (Entf)"), this);
  delete_constraint_action_->setToolTip(QStringLiteral(
      "Ausgewählte Abhängigkeit, Bemaßung oder Skizzengeometrie löschen"));
  delete_constraint_action_->setShortcuts(
      {QKeySequence(QKeySequence::Delete), QKeySequence(Qt::Key_Backspace)});
  connect(delete_constraint_action_, &QAction::triggered, this, [this] {
    const auto removed = delete_selection();
    if (removed.has_error())
      report(removed.error());
    window_.refresh();
  });
  ribbon_.add_small_button(constrain_group, delete_constraint_action_);

  // Ändern-Gruppe (Block 116): Trim/Extend/Split are modal pick tools; Fillet
  // and Chamfer act on exactly two selected curves with a numeric prompt.
  auto* modify_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Ändern"));
  struct ModifyToolButton {
    const char* label;
    const char* tip;
    SketchModifyKind kind;
  };
  static constexpr std::array<ModifyToolButton, 3> kModifyTools{{
      {"Trimmen", "Kurve zum Wegschneiden anklicken (bis zur nächsten Kreuzung)",
       SketchModifyKind::Trim},
      {"Verlängern", "Kurve zum Verlängern anklicken (bis zur nächsten Kreuzung)",
       SketchModifyKind::Extend},
      {"Teilen", "Kurve an der Klickstelle teilen", SketchModifyKind::Split},
  }};
  for (const auto& button : kModifyTools) {
    auto* action = new QAction(QString::fromUtf8(button.label), this);
    action->setToolTip(QString::fromUtf8(button.tip));
    connect(action, &QAction::triggered, this,
            [this, kind = button.kind] { begin_modify_tool(kind); });
    ribbon_.add_small_button(modify_group, action);
    modify_actions_.push_back(action);
  }
  fillet_action_ = new QAction(QStringLiteral("Verrundung"), this);
  fillet_action_->setToolTip(
      QStringLiteral("Zwei verbundene Kurven verrunden (Radius abfragen)"));
  connect(fillet_action_, &QAction::triggered, this, [this] {
    bool accepted = false;
    const double radius = QInputDialog::getDouble(
        &window_, QStringLiteral("Verrundung"), QStringLiteral("Radius (mm):"), 5.0, 0.0,
        1.0e6, 3, &accepted);
    if (!accepted)
      return;
    const auto applied = apply_fillet(radius);
    if (applied.has_error())
      report(applied.error());
    window_.refresh();
  });
  ribbon_.add_small_button(modify_group, fillet_action_);
  chamfer_action_ = new QAction(QStringLiteral("Fase"), this);
  chamfer_action_->setToolTip(QStringLiteral("Zwei verbundene Kurven fasen (Abstand abfragen)"));
  connect(chamfer_action_, &QAction::triggered, this, [this] {
    bool accepted = false;
    const double distance = QInputDialog::getDouble(
        &window_, QStringLiteral("Fase"), QStringLiteral("Abstand (mm):"), 5.0, 0.0, 1.0e6, 3,
        &accepted);
    if (!accepted)
      return;
    const auto applied = apply_chamfer(distance);
    if (applied.has_error())
      report(applied.error());
    window_.refresh();
  });
  ribbon_.add_small_button(modify_group, chamfer_action_);

  auto* dimension_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Bemaßung"));
  QList<QAction*> dimension_menu;
  for (const auto& descriptor : kDimensions) {
    auto* action = new QAction(QString::fromUtf8(descriptor.label), this);
    connect(action, &QAction::triggered, this, [this, kind = descriptor.kind] {
      // Driving dimensions auto-create their parameter (Inventor-style dN);
      // no dialog needed.
      const bool reference =
          reference_dimension_action_ != nullptr && reference_dimension_action_->isChecked();
      const auto applied = apply_dimension(kind, reference);
      if (applied.has_error())
        report(applied.error());
      window_.refresh();
    });
    dimension_menu.push_back(action);
    dimension_actions_.push_back(action);
  }
  ribbon_.add_menu_button(dimension_group, QStringLiteral("Bemaßung"), dimension_menu);
  reference_dimension_action_ = new QAction(QStringLiteral("Referenz"), this);
  reference_dimension_action_->setCheckable(true);
  reference_dimension_action_->setToolTip(
      QStringLiteral("Neue Bemaßungen als Referenz (nicht treibend) anlegen"));
  ribbon_.add_small_button(dimension_group, reference_dimension_action_);
  edit_dimension_action_ = new QAction(QStringLiteral("Bearbeiten"), this);
  edit_dimension_action_->setToolTip(
      QStringLiteral("Wert der ausgewählten Bemaßung ändern (oder Doppelklick)"));
  connect(edit_dimension_action_, &QAction::triggered, this,
          [this] { (void)edit_selected_dimension_interactive(); });
  ribbon_.add_small_button(dimension_group, edit_dimension_action_);

  auto* grid_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Raster"));
  grid_action_ = new QAction(QStringLiteral("Raster"), this);
  grid_action_->setCheckable(true);
  grid_action_->setChecked(true);
  grid_snap_action_ = new QAction(QStringLiteral("Fang"), this);
  grid_snap_action_->setCheckable(true);
  grid_snap_action_->setChecked(true);
  connect(grid_action_, &QAction::toggled, this, [this] { apply_grid_config(); });
  connect(grid_snap_action_, &QAction::toggled, this, [this] { apply_grid_config(); });
  ribbon_.add_small_button(grid_group, grid_action_);
  ribbon_.add_small_button(grid_group, grid_snap_action_);
}

void ShellSketchTools::wire_viewport_callbacks() {
  viewport_.set_sketch_pointer_callback(
      [this](Point2 raw_point, const GuiSketchSnapResult& snap,
             const std::optional<GuiSketchHit>& hit) { handle_pointer(raw_point, snap, hit); });
  viewport_.set_sketch_selection_callback(
      [this](const std::vector<GuiSelection>& selections) {
        synchronize_sketch_selection(selections);
      });
  viewport_.set_sketch_drag_pointer_callback(
      [this](GuiSketchScreenPoint, Point2 raw, const GuiSketchSnapResult& snap,
             const std::optional<GuiSketchHit>&) {
        if (label_drag_) {
          // Dimension label drag: move the badge with the raw pointer.
          auto& offset = dimension_label_offsets_[*label_drag_];
          offset.x += raw.x - label_drag_last_.x;
          offset.y += raw.y - label_drag_last_.y;
          label_drag_last_ = raw;
          if (!label_publish_scheduled_) {
            label_publish_scheduled_ = true;
            QTimer::singleShot(0, this, [this] {
              label_publish_scheduled_ = false;
              publish_scene();
            });
          }
          return;
        }
        if (!drag_controller_ || !drag_controller_->active() || !drag_stage(workspace_.stage()))
          return;
        if (workspace_.stage() == GuiSketchInteractionStage::SelectedHandle &&
            !workspace_.show_drag_candidate(session_)) {
          cancel_drag(true, "Sketch-Drag konnte die Live-Vorschau nicht starten");
          return;
        }
        auto queued = drag_controller_->queue_pointer(snap.snapped_point);
        if (queued.has_error()) {
          cancel_drag(true, queued.error().message());
          return;
        }
        // Deferred so the solve runs after the pointer event finished; this is
        // event sequencing, not a polling timer.
        if (!solve_scheduled_) {
          solve_scheduled_ = true;
          QTimer::singleShot(0, this, [this] {
            solve_scheduled_ = false;
            if (!drag_controller_ || !drag_controller_->active() ||
                !drag_controller_->has_pending())
              return;
            auto preview = drag_controller_->process_pending();
            if (preview.has_error()) {
              cancel_drag(true, preview.error().message());
              return;
            }
            publish_drag_preview(preview.value());
          });
        }
      });
  viewport_.set_sketch_pointer_phase_callback(
      [this](GuiSketchPointerPhase phase, GuiSketchScreenPoint screen, Point2 raw,
             const GuiSketchSnapResult& snap, const std::optional<GuiSketchHit>& hit) {
        if (tool_active())
          return; // create tools capture picks via the viewport event filter
        if (modify_tool_) {
          // Modal Trim/Extend/Split: the press picks the target curve; other
          // phases are ignored so no drag begins.
          if (phase == GuiSketchPointerPhase::Press)
            handle_modify_press(*modify_tool_, hit);
          return;
        }
        if (phase == GuiSketchPointerPhase::Press) {
          // Pressing a dimension badge starts a label drag instead of a
          // geometry drag.
          if (hit && hit->kind == GuiSketchHitKind::Dimension) {
            constexpr std::string_view kDimensionMarker = "/dimension/";
            const auto marker = hit->semantic_id.rfind(kDimensionMarker);
            if (marker != std::string::npos) {
              label_drag_ = hit->semantic_id.substr(marker + kDimensionMarker.size());
              label_drag_last_ = raw;
              viewport_.set_sketch_selection_enabled(false);
              return;
            }
          }
          begin_drag(screen);
        } else if (label_drag_) {
          label_drag_.reset();
          viewport_.set_sketch_selection_enabled(true);
          publish_scene();
        } else {
          release_drag(snap.snapped_point);
        }
      });
}

void ShellSketchTools::activate() {
  candidate_by_semantic_.clear();
  publish_scene();
  arm_drag_controller();
  refresh_enablement();
}

void ShellSketchTools::deactivate() {
  cancel_tool();
  label_drag_.reset();
  if (drag_controller_ && drag_controller_->active())
    drag_controller_->cancel();
  drag_controller_.reset();
  candidate_by_semantic_.clear();
  viewport_.set_sketch_drag_handles({});
  viewport_.set_sketch_preview_polyline({}, false);
  viewport_.set_sketch_inference_anchor(std::nullopt);
  refresh_enablement();
}

GuiSketchGridConfig ShellSketchTools::grid_config() const {
  GuiSketchGridConfig config;
  config.visible = grid_action_ == nullptr || grid_action_->isChecked();
  config.snap_enabled = grid_snap_action_ == nullptr || grid_snap_action_->isChecked();
  config.adaptive = true;
  return config;
}

void ShellSketchTools::apply_grid_config() {
  if (viewport_.sketch_interaction_active())
    viewport_.set_sketch_grid_config(grid_config());
}

bool ShellSketchTools::sketch_context_ready() const {
  return workspace_.active() && window_.active_sketch().has_value() &&
         session_.part_document() != nullptr;
}

void ShellSketchTools::publish_scene() {
  if (!sketch_context_ready())
    return;
  // Hover-cached candidate roles reference the previous topology revision.
  candidate_by_semantic_.clear();
  const Sketch* sketch = session_.part_document()->find_sketch(*window_.active_sketch());
  if (sketch == nullptr)
    return;
  publish_scene_for(*sketch);
  if (drag_controller_)
    publish_handles(drag_controller_->handles());
}

void ShellSketchTools::publish_scene_for(const Sketch& sketch, bool include_annotations) {
  const PartDocument* part = session_.part_document();
  if (part == nullptr)
    return;
  auto scene = session_.part_shape_cache() != nullptr
                   ? GuiSketchInteractionSceneBuilder{}.build(*part, sketch,
                                                              *session_.part_shape_cache())
                   : GuiSketchInteractionSceneBuilder{}.build(*part, sketch);
  if (scene.has_error()) {
    report(scene.error());
    return;
  }
  if (include_annotations) {
    auto constraint_annotations =
        GuiSketchConstraintController::accepted_annotations(session_, sketch.id());
    if (constraint_annotations.has_error()) {
      report(constraint_annotations.error());
      return;
    }
    scene.value().annotations.insert(scene.value().annotations.end(),
                                     constraint_annotations.value().begin(),
                                     constraint_annotations.value().end());
    auto dimension_annotations =
        GuiSketchDimensionController::accepted_annotations(session_, sketch.id());
    if (dimension_annotations.has_error()) {
      report(dimension_annotations.error());
      return;
    }
    scene.value().annotations.insert(scene.value().annotations.end(),
                                     dimension_annotations.value().begin(),
                                     dimension_annotations.value().end());
  }
  // Apply the user's dragged label placement (hit test and drawing both use
  // the shifted anchor).
  if (!dimension_label_offsets_.empty()) {
    constexpr std::string_view kDimensionMarker = "/dimension/";
    for (auto& annotation : scene.value().annotations) {
      if (annotation.hit_kind != GuiSketchHitKind::Dimension)
        continue;
      const auto marker = annotation.semantic_id.rfind(kDimensionMarker);
      if (marker == std::string::npos)
        continue;
      const auto offset = dimension_label_offsets_.find(
          annotation.semantic_id.substr(marker + kDimensionMarker.size()));
      if (offset == dimension_label_offsets_.end())
        continue;
      annotation.point.x += offset->second.x;
      annotation.point.y += offset->second.y;
    }
  }

  auto plane = workbench_.plane_view(session_, sketch.id());
  if (plane.has_error()) {
    report(plane.error());
    return;
  }
  GuiSketchInteractionConfig config;
  config.grid = grid_config();
  // Constraint tokens and dimension badges should be easy to click.
  config.annotation_hit_tolerance_dip = 12.0;
  const auto published =
      viewport_.set_sketch_interaction(plane.value(), std::move(scene.value()), config);
  if (published.has_error())
    report(published.error());
}

void ShellSketchTools::refresh_enablement() {
  const bool active = sketch_context_ready() && !session_.task().active();
  std::vector<SketchSolverConstraintKind> compatible_constraints;
  std::vector<SketchDimensionKind> compatible_dimensions;
  if (active && sketch_idle_stage(workspace_.stage())) {
    auto topology = active_topology();
    if (!topology.has_error()) {
      auto constraint_targets = selected_targets(topology.value(), true);
      if (!constraint_targets.has_error())
        compatible_constraints = GuiSketchConstraintController::compatible_kinds(
            topology.value(), constraint_targets.value());
      auto dimension_targets = selected_targets(topology.value(), true);
      if (!dimension_targets.has_error())
        compatible_dimensions = GuiSketchDimensionController::compatible_kinds(
            topology.value(), dimension_targets.value());
    }
  }
  for (std::size_t index = 0; index < constraint_actions_.size(); ++index) {
    bool enabled = std::find(compatible_constraints.begin(), compatible_constraints.end(),
                             kConstraints[index].kind) != compatible_constraints.end();
    // Koinzident also covers point-on-curve (mapped to PointOnObject).
    if (!enabled && kConstraints[index].kind == SketchSolverConstraintKind::Coincident)
      enabled = std::find(compatible_constraints.begin(), compatible_constraints.end(),
                          SketchSolverConstraintKind::PointOnObject) !=
                compatible_constraints.end();
    constraint_actions_[index]->setEnabled(enabled);
  }
  for (std::size_t index = 0; index < dimension_actions_.size(); ++index)
    dimension_actions_[index]->setEnabled(
        std::find(compatible_dimensions.begin(), compatible_dimensions.end(),
                  kDimensions[index].kind) != compatible_dimensions.end());
  for (auto* action : tool_actions_)
    action->setEnabled(active);
  // Ändern: Trim/Extend/Split are modal (armed anytime); Fillet/Chamfer need
  // exactly two selected curves.
  for (auto* action : modify_actions_)
    action->setEnabled(active);
  const bool two_curves = active && selected_sketch_entity_ids().size() == 2U;
  if (fillet_action_ != nullptr)
    fillet_action_->setEnabled(two_curves);
  if (chamfer_action_ != nullptr)
    chamfer_action_->setEnabled(two_curves);
  if (delete_constraint_action_ != nullptr)
    delete_constraint_action_->setEnabled(
        active && (selected_accepted_constraint().has_value() ||
                   selected_dimension().has_value() || selected_entity().has_value()));
  if (edit_dimension_action_ != nullptr)
    edit_dimension_action_->setEnabled(active && selected_dimension().has_value());
  if (grid_action_ != nullptr)
    grid_action_->setEnabled(active);
  if (grid_snap_action_ != nullptr)
    grid_snap_action_->setEnabled(active);
}

Result<SketchTopology> ShellSketchTools::active_topology() const {
  if (!window_.active_sketch() || session_.part_document() == nullptr)
    return Result<SketchTopology>::failure(
        Error::validation("gui.shell.sketch", "keine aktive Skizze"));
  const Sketch* sketch = session_.part_document()->find_sketch(*window_.active_sketch());
  if (sketch == nullptr)
    return Result<SketchTopology>::failure(
        Error::validation(window_.active_sketch()->value(), "aktive Skizze existiert nicht mehr"));
  return SketchTopology::migrate_legacy(*sketch);
}

Result<std::vector<SketchConstraintIntentTarget>>
ShellSketchTools::selected_targets(const SketchTopology& topology,
                                   bool resolve_point_roles) const {
  std::vector<SketchConstraintIntentTarget> targets;
  const std::string dimension_prefix = "sketch/" + topology.sketch().value() + "/dimension/";
  const std::string constraint_prefix = "sketch/" + topology.sketch().value() + "/constraint/";
  for (const auto& selection : session_.selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity ||
        std::string_view(selection.semantic_id).starts_with(dimension_prefix) ||
        std::string_view(selection.semantic_id).starts_with(constraint_prefix))
      continue;
    if (resolve_point_roles && !selection.candidate_id.empty()) {
      const auto entity_id = selection_target(topology, selection.semantic_id);
      if (entity_id && entity_id->kind() == SketchConstraintIntentTargetKind::Entity) {
        const auto* entity = topology.find_entity(entity_id->id());
        if (entity != nullptr)
          if (const auto point_id =
                  resolve_point_role(topology, *entity, selection.candidate_id)) {
            auto target = SketchConstraintIntentTarget::point(*point_id);
            if (target.has_error())
              return Result<std::vector<SketchConstraintIntentTarget>>::failure(target.error());
            if (std::find(targets.begin(), targets.end(), target.value()) == targets.end())
              targets.push_back(std::move(target.value()));
            continue;
          }
      }
    }
    auto target = selection_target(topology, selection.semantic_id);
    if (!target)
      return Result<std::vector<SketchConstraintIntentTarget>>::failure(Error::validation(
          selection.semantic_id, "Auswahl ist kein stabiles Punkt- oder Kurvenziel"));
    // A free-standing sketch point selects as an entity but constrains and
    // dimensions through its single underlying topology point.
    if (target->kind() == SketchConstraintIntentTargetKind::Entity) {
      const auto* entity = topology.find_entity(target->id());
      if (entity != nullptr && entity->kind() == SketchTopologyEntityKind::Point &&
          !entity->points().empty()) {
        auto point = SketchConstraintIntentTarget::point(entity->points()[0]);
        if (point.has_error())
          return Result<std::vector<SketchConstraintIntentTarget>>::failure(point.error());
        target = std::move(point.value());
      }
    }
    if (std::find(targets.begin(), targets.end(), *target) == targets.end())
      targets.push_back(std::move(*target));
  }
  return Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(targets));
}

std::optional<SketchConstraintId> ShellSketchTools::selected_accepted_constraint() const {
  if (!window_.active_sketch() || session_.part_document() == nullptr)
    return std::nullopt;
  const std::string prefix = "sketch/" + window_.active_sketch()->value() + "/constraint/";
  std::optional<SketchConstraintId> selected;
  for (const auto& selection : session_.selection().entries()) {
    if (!std::string_view(selection.semantic_id).starts_with(prefix))
      continue;
    if (selected.has_value())
      return std::nullopt;
    selected = SketchConstraintId(selection.semantic_id.substr(prefix.size()));
  }
  if (!selected.has_value())
    return std::nullopt;
  auto catalog = session_.sketch_constraint_catalog(*window_.active_sketch());
  if (catalog.has_error() || catalog.value().find(*selected) == nullptr)
    return std::nullopt;
  return selected;
}

std::optional<SketchDimensionId> ShellSketchTools::selected_dimension() const {
  if (!window_.active_sketch() || session_.selection().entries().size() != 1U)
    return std::nullopt;
  const auto& selection = session_.selection().entries().front();
  const std::string prefix = "sketch/" + window_.active_sketch()->value() + "/dimension/";
  if (!std::string_view(selection.semantic_id).starts_with(prefix))
    return std::nullopt;
  return SketchDimensionId(selection.semantic_id.substr(prefix.size()));
}

std::optional<std::string> ShellSketchTools::selected_entity() const {
  auto topology = active_topology();
  if (topology.has_error())
    return std::nullopt;
  for (const auto& selection : session_.selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity)
      continue;
    auto target = selection_target(topology.value(), selection.semantic_id);
    if (target && target->kind() == SketchConstraintIntentTargetKind::Entity)
      return target->id();
  }
  return std::nullopt;
}

// --- create tools ---------------------------------------------------------

bool ShellSketchTools::tool_active() const {
  return create_controller_.has_value() && workspace_.active() &&
         collecting_stage(workspace_.stage());
}

bool ShellSketchTools::begin_tool(GuiSketchCreateTool tool) {
  if (!sketch_context_ready() || session_.task().active())
    return false;
  cancel_tool();
  auto plane = workbench_.plane_view(session_, *window_.active_sketch());
  if (plane.has_error()) {
    report(plane.error());
    return false;
  }
  if (!workspace_.begin_command(session_, "sketch.create", "Punkte im Viewport wählen")) {
    report_message("Skizzenwerkzeug konnte den Workspace-Lebenszyklus nicht starten");
    return false;
  }
  auto controller =
      GuiSketchCreateController::begin(tool, *window_.active_sketch(), std::move(plane.value()));
  if (controller.has_error()) {
    report(controller.error());
    (void)workspace_.escape(session_);
    return false;
  }
  create_controller_.emplace(std::move(controller.value()));
  numeric_buffer_.clear();
  viewport_.set_sketch_selection_enabled(false);
  show_workspace_status();
  return true;
}

bool ShellSketchTools::add_pick(Point2 point, GuiSketchSnapKind kind) {
  if (!tool_active())
    return false;
  auto added = create_controller_->add_pick({point, kind});
  if (added.has_error()) {
    report(added.error());
    return false;
  }
  viewport_.set_sketch_inference_anchor(point);
  after_pick();
  return true;
}

void ShellSketchTools::after_pick() {
  clear_numeric_buffer();
  update_preview();
  if (create_controller_ && !create_controller_->accepts_more_picks() &&
      create_controller_->complete_ready())
    (void)complete_tool();
}

bool ShellSketchTools::complete_tool() {
  if (!create_controller_)
    return false;
  const bool chained_line = create_controller_->tool() == GuiSketchCreateTool::Line;
  // The Point tool re-arms after each placement so repeated clicks drop more
  // points (Inventor-style), until Escape. Without this the tool deactivated
  // after one point and further clicks fell through to the drag layer.
  const bool repeatable_point = create_controller_->tool() == GuiSketchCreateTool::Point;
  std::optional<Point2> next_anchor;
  if (chained_line && !create_controller_->picks().empty())
    next_anchor = create_controller_->picks().back().point;
  const bool couple_previous = chained_line && chain_anchor_.has_value();
  if (workspace_.stage() == GuiSketchInteractionStage::CollectingPicks)
    (void)workspace_.begin_numeric_input(session_);
  if (workspace_.stage() == GuiSketchInteractionStage::NumericInput)
    (void)workspace_.show_preview(session_);
  auto committed = create_controller_->commit(session_, workbench_);
  if (committed.has_error()) {
    report(committed.error());
    cancel_tool();
    return false;
  }
  (void)workspace_.commit_command(session_);
  create_controller_.reset();
  numeric_buffer_.clear();
  viewport_.set_sketch_preview_polyline({}, false);
  viewport_.set_sketch_inference_anchor(std::nullopt);
  viewport_.set_sketch_selection_enabled(true);
  if (couple_previous)
    apply_chain_coincidence();
  publish_scene();
  arm_drag_controller();
  window_.refresh();
  if (chained_line && next_anchor && begin_tool(GuiSketchCreateTool::Line)) {
    // Inventor-style chaining: the next segment starts at the committed end;
    // the shared corner is coupled coincident on the next commit.
    chain_anchor_ = next_anchor;
    (void)add_pick(*next_anchor, GuiSketchSnapKind::Endpoint);
  } else if (repeatable_point) {
    (void)begin_tool(GuiSketchCreateTool::Point); // stay armed for more points
  }
  return true;
}

void ShellSketchTools::cancel_tool() {
  chain_anchor_.reset();
  modify_tool_.reset(); // switching/cancelling any tool ends a modal modify tool
  if (!create_controller_)
    return;
  create_controller_.reset();
  numeric_buffer_.clear();
  viewport_.set_sketch_preview_polyline({}, false);
  viewport_.set_sketch_inference_anchor(std::nullopt);
  viewport_.set_sketch_selection_enabled(true);
  while (collecting_stage(workspace_.stage()) && workspace_.escape(session_)) {
  }
}

void ShellSketchTools::apply_chain_coincidence() {
  if (!chain_anchor_)
    return;
  auto topology = active_topology();
  if (topology.has_error())
    return;
  // Find the two DISTINCT line endpoints sitting exactly on the chain anchor.
  // (Positional lookup — entity ordering is not reliable: profiles and other
  // entities interleave with the chained lines.)
  std::vector<SketchPointId> corner_points;
  for (const auto& entity : topology.value().entities()) {
    if (entity.kind() != SketchTopologyEntityKind::Line || entity.points().size() < 2U)
      continue;
    for (const auto& point_id : entity.points()) {
      const auto* point = topology.value().find_point(point_id);
      if (point == nullptr)
        continue;
      if (std::hypot(point->position().x - chain_anchor_->x,
                     point->position().y - chain_anchor_->y) > 1.0e-9)
        continue;
      if (std::find(corner_points.begin(), corner_points.end(), point_id) ==
          corner_points.end())
        corner_points.push_back(point_id);
    }
  }
  if (corner_points.size() < 2U)
    return;
  auto first = SketchConstraintIntentTarget::point(corner_points[corner_points.size() - 2U]);
  auto second = SketchConstraintIntentTarget::point(corner_points.back());
  if (first.has_error() || second.has_error())
    return;
  auto catalog = session_.sketch_constraint_catalog(topology.value().sketch());
  if (catalog.has_error())
    return;
  auto intent = SketchConstraintIntent::create(
      next_constraint_id(catalog.value(), SketchSolverConstraintKind::Coincident),
      SketchSolverConstraintKind::Coincident, {first.value(), second.value()});
  if (intent.has_error())
    return;
  auto controller = GuiSketchConstraintController::create(session_, topology.value().sketch(),
                                                          std::move(intent.value()));
  if (controller.has_error()) {
    report(controller.error());
    return;
  }
  // Redundant preview (corner already constrained) is fine and stays silent;
  // real rejections surface in the diagnostics.
  if (!controller.value().preview().authoring.accepted)
    return;
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    report(committed.error());
}

void ShellSketchTools::update_preview() {
  if (!tool_active())
    return;
  std::optional<GuiSketchCreatePick> hover;
  if (const auto& snap = viewport_.sketch_snap_result())
    hover = GuiSketchCreatePick{snap->snapped_point, snap->kind};
  const auto preview = create_controller_->preview(hover);
  viewport_.set_sketch_preview_polyline(preview.rubber_band, preview.closed);
  if (!preview.constraints.empty())
    workspace_.set_snap_inference("Abhängigkeits-Vorschau: " + preview.constraints.back().kind);
}

void ShellSketchTools::apply_numeric_buffer() {
  if (!tool_active())
    return;
  const std::string text = numeric_buffer_;
  std::optional<Point2> hover;
  if (const auto& snap = viewport_.sketch_snap_result())
    hover = snap->snapped_point;
  auto numeric = create_controller_->apply_numeric(text, hover);
  if (numeric.has_error()) {
    report(numeric.error());
    return;
  }
  if (!numeric.value().has_value()) {
    std::size_t sides{};
    const auto [parsed, error] = std::from_chars(text.data(), text.data() + text.size(), sides);
    if (error == std::errc{} && parsed == text.data() + text.size()) {
      auto updated = create_controller_->set_polygon_sides(sides);
      if (updated.has_error())
        report(updated.error());
    }
    clear_numeric_buffer();
    return;
  }
  auto added = create_controller_->add_pick(*numeric.value());
  if (added.has_error()) {
    report(added.error());
    return;
  }
  after_pick();
}

void ShellSketchTools::mirror_numeric_buffer() {
  (void)workspace_.set_numeric_input(numeric_buffer_);
  show_workspace_status();
}

void ShellSketchTools::clear_numeric_buffer() {
  numeric_buffer_.clear();
  (void)workspace_.set_numeric_input({});
}

// --- constraints and dimensions -------------------------------------------

Result<std::size_t> ShellSketchTools::apply_constraint(SketchSolverConstraintKind kind) {
  auto topology = active_topology();
  if (topology.has_error())
    return Result<std::size_t>::failure(topology.error());
  // Inventor semantics: a clicked LINE MIDPOINT is not a topology point;
  // Koinzident with it means "constrain the other point to the line's middle"
  // (the Midpoint solver kind — it has no own button).
  std::optional<std::vector<SketchConstraintIntentTarget>> explicit_targets;
  if (kind == SketchSolverConstraintKind::Coincident &&
      session_.selection().entries().size() == 2U) {
    std::optional<std::string> midpoint_line;
    std::optional<SketchConstraintIntentTarget> other_point;
    for (const auto& selection : session_.selection().entries()) {
      const auto separator = selection.candidate_id.rfind(':');
      const std::string_view role = separator == std::string::npos
                                        ? std::string_view{}
                                        : std::string_view(selection.candidate_id)
                                              .substr(separator + 1U);
      auto target = selection_target(topology.value(), selection.semantic_id);
      if (!target)
        continue;
      const auto* entity = topology.value().find_entity(target->id());
      if (role == "midpoint" && entity != nullptr &&
          entity->kind() == SketchTopologyEntityKind::Line) {
        midpoint_line = target->id();
      } else if (entity != nullptr && !selection.candidate_id.empty()) {
        if (const auto point_id =
                resolve_point_role(topology.value(), *entity, selection.candidate_id)) {
          auto point_target = SketchConstraintIntentTarget::point(*point_id);
          if (!point_target.has_error())
            other_point = std::move(point_target.value());
        }
      } else if (target->kind() == SketchConstraintIntentTargetKind::Point) {
        other_point = *target;
      }
    }
    if (midpoint_line && other_point) {
      auto line_target = SketchConstraintIntentTarget::entity(*midpoint_line);
      if (line_target.has_error())
        return Result<std::size_t>::failure(line_target.error());
      kind = SketchSolverConstraintKind::Midpoint;
      explicit_targets = {{std::move(*other_point), std::move(line_target.value())}};
    }
  }

  auto targets = explicit_targets
                     ? Result<std::vector<SketchConstraintIntentTarget>>::success(
                           std::move(*explicit_targets))
                     : selected_targets(topology.value(), true);
  if (targets.has_error())
    return Result<std::size_t>::failure(targets.error());
  // Selection order must not matter: intents expect points before entities.
  std::stable_partition(targets.value().begin(), targets.value().end(),
                        [](const SketchConstraintIntentTarget& target) {
                          return target.kind() == SketchConstraintIntentTargetKind::Point;
                        });
  // Inventor semantics: Koinzident between a point and a curve is
  // point-on-object under the hood.
  if (kind == SketchSolverConstraintKind::Coincident && targets.value().size() == 2U &&
      targets.value()[0].kind() == SketchConstraintIntentTargetKind::Point &&
      targets.value()[1].kind() == SketchConstraintIntentTargetKind::Entity)
    kind = SketchSolverConstraintKind::PointOnObject;
  const auto compatible =
      GuiSketchConstraintController::compatible_kinds(topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "aktuelle Auswahl ist mit dieser Abhängigkeit unvereinbar"));
  auto catalog = session_.sketch_constraint_catalog(topology.value().sketch());
  if (catalog.has_error())
    return Result<std::size_t>::failure(catalog.error());
  auto intent = SketchConstraintIntent::create(next_constraint_id(catalog.value(), kind), kind,
                                               std::move(targets.value()));
  if (intent.has_error())
    return Result<std::size_t>::failure(intent.error());
  auto controller = GuiSketchConstraintController::create(session_, topology.value().sketch(),
                                                          std::move(intent.value()));
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  if (!controller.value().preview().authoring.accepted) {
    const auto& preview = controller.value().preview().authoring;
    std::string message =
        "Abhängigkeits-Vorschau abgelehnt: " + std::string(to_string(preview.solve.status));
    if (!preview.conflict_ids.empty()) {
      message += " [";
      for (std::size_t index = 0U; index < preview.conflict_ids.size(); ++index) {
        if (index > 0U)
          message += ", ";
        message += preview.conflict_ids[index];
      }
      message += "]";
    }
    // Solver-Detail für die Diagnose: unterscheidet Stall (0 Iterationen) von
    // Iterationslimit und zeigt, wie weit das Residuum gefallen ist.
    const auto& solve = preview.solve;
    message += " (iter=" + std::to_string(solve.iterations) +
               ", rms " + std::to_string(solve.residuals.initial_rms) + "→" +
               std::to_string(solve.residuals.final_rms) + ")";
    if (!solve.diagnostics.empty() && !solve.diagnostics.front().message.empty())
      message += ": " + solve.diagnostics.front().message;
    return Result<std::size_t>::failure(
        Error::validation(preview.candidate.id().value(), std::move(message)));
  }
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

Result<std::size_t> ShellSketchTools::remove_selected_constraint() {
  const auto selected = selected_accepted_constraint();
  if (!selected)
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "genau eine akzeptierte Abhängigkeits-Glyphe auswählen"));
  auto removed = GuiSketchConstraintController::remove_accepted(
      session_, *window_.active_sketch(), *selected);
  if (removed.has_error())
    return removed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return removed;
}

// --- Ändern-Gruppe (Block 116) --------------------------------------------

namespace {
// The modify service addresses raw core entity ids ("line.a"), while selections
// resolve to topology target ids ("entity/line.a"). Strip the topology prefix.
[[nodiscard]] std::optional<SketchEntityId> raw_entity_id(const SketchTopology& topology,
                                                          const std::string& semantic_id) {
  const auto target = selection_target(topology, semantic_id);
  if (!target || target->kind() != SketchConstraintIntentTargetKind::Entity)
    return std::nullopt;
  constexpr std::string_view kEntityPrefix = "entity/";
  std::string_view id = target->id();
  if (!id.starts_with(kEntityPrefix))
    return std::nullopt; // profiles etc. are not trim/fillet curves
  return SketchEntityId(std::string(id.substr(kEntityPrefix.size())));
}
} // namespace

std::vector<SketchEntityId> ShellSketchTools::selected_sketch_entity_ids() const {
  std::vector<SketchEntityId> ids;
  auto topology = active_topology();
  if (topology.has_error())
    return ids;
  for (const auto& selection : session_.selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity)
      continue;
    auto id = raw_entity_id(topology.value(), selection.semantic_id);
    if (id && std::find(ids.begin(), ids.end(), *id) == ids.end())
      ids.push_back(std::move(*id));
  }
  return ids;
}

Result<std::size_t> ShellSketchTools::apply_modify_pick(SketchModifyKind kind,
                                                        const std::string& entity_id,
                                                        Point2 pick) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "keine aktive Skizze"));
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  const SketchEntityId target(entity_id);
  Result<GuiSketchModifyPreview> preview = Result<GuiSketchModifyPreview>::failure(
      Error::validation("gui.shell.sketch", "unbekannte Änderungsart"));
  switch (kind) {
  case SketchModifyKind::Trim: preview = controller.value().trim(target, pick); break;
  case SketchModifyKind::Extend: preview = controller.value().extend(target, pick); break;
  case SketchModifyKind::Split: preview = controller.value().split(target, pick); break;
  }
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

namespace {
// Topology entity ids present before a modify commit, so the newly inserted
// fillet arc / chamfer line can be told apart afterwards.
[[nodiscard]] std::vector<std::string> entity_ids_of(const SketchTopology& topology) {
  std::vector<std::string> ids;
  ids.reserve(topology.entities().size());
  for (const auto& entity : topology.entities())
    ids.push_back(entity.id());
  return ids;
}
} // namespace

Result<std::size_t> ShellSketchTools::apply_fillet(double radius) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "keine aktive Skizze"));
  const auto ids = selected_sketch_entity_ids();
  if (ids.size() != 2U)
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "genau zwei verbundene Kurven für die Verrundung wählen"));
  auto before = active_topology();
  if (before.has_error())
    return Result<std::size_t>::failure(before.error());
  const std::vector<std::string> before_ids = entity_ids_of(before.value());
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  auto preview = controller.value().fillet(ids[0], ids[1], radius);
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    return committed;
  prune_broken_coincidences(); // drop the stale corner coincidence the trim split
  auto_constrain_corner(before_ids, ids[0], ids[1], /*add_tangent=*/true);
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

Result<std::size_t> ShellSketchTools::apply_chamfer(double distance) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "keine aktive Skizze"));
  const auto ids = selected_sketch_entity_ids();
  if (ids.size() != 2U)
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "genau zwei verbundene Kurven für die Fase wählen"));
  auto before = active_topology();
  if (before.has_error())
    return Result<std::size_t>::failure(before.error());
  const std::vector<std::string> before_ids = entity_ids_of(before.value());
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  auto preview = controller.value().chamfer(ids[0], ids[1], distance);
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    return committed;
  prune_broken_coincidences(); // drop the stale corner coincidence the trim split
  auto_constrain_corner(before_ids, ids[0], ids[1], /*add_tangent=*/false);
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

bool ShellSketchTools::author_catalog_constraint(
    SketchSolverConstraintKind kind, std::vector<SketchConstraintIntentTarget> targets) {
  if (!window_.active_sketch())
    return false;
  const SketchId sketch = *window_.active_sketch();
  auto catalog = session_.sketch_constraint_catalog(sketch);
  if (catalog.has_error())
    return false;
  auto intent = SketchConstraintIntent::create(next_constraint_id(catalog.value(), kind), kind,
                                               std::move(targets));
  if (intent.has_error())
    return false;
  auto controller = GuiSketchConstraintController::create(session_, sketch, std::move(intent.value()));
  if (controller.has_error())
    return false;
  // A rejected preview (redundant/conflicting/non-convergent) is skipped
  // silently: the corner just gets fewer auto-constraints, never a hard error.
  if (!controller.value().preview().authoring.accepted)
    return false;
  return !controller.value().commit(session_).has_error();
}

void ShellSketchTools::prune_broken_coincidences() {
  auto topology = active_topology();
  if (topology.has_error())
    return;
  const SketchTopology& topo = topology.value();
  const SketchId sketch = *window_.active_sketch();
  auto catalog = session_.sketch_constraint_catalog(sketch);
  if (catalog.has_error())
    return;
  std::vector<SketchConstraintId> doomed;
  for (const auto& intent : catalog.value().constraints()) {
    if (intent.kind() != SketchSolverConstraintKind::Coincident || intent.targets().size() != 2U)
      continue;
    const auto& targets = intent.targets();
    if (targets[0].kind() != SketchConstraintIntentTargetKind::Point ||
        targets[1].kind() != SketchConstraintIntentTargetKind::Point)
      continue;
    const auto* first = topo.find_point(SketchPointId(targets[0].id()));
    const auto* second = topo.find_point(SketchPointId(targets[1].id()));
    if (first == nullptr || second == nullptr) {
      doomed.push_back(intent.id()); // references a point the edit removed
      continue;
    }
    if (std::hypot(first->position().x - second->position().x,
                   first->position().y - second->position().y) > 1.0e-4)
      doomed.push_back(intent.id()); // the two points were pulled apart
  }
  for (const auto& id : doomed)
    (void)GuiSketchConstraintController::remove_accepted(session_, sketch, id);
}

void ShellSketchTools::auto_constrain_corner(const std::vector<std::string>& before_entity_ids,
                                             SketchEntityId first_line, SketchEntityId second_line,
                                             bool add_tangent) {
  auto topology = active_topology();
  if (topology.has_error())
    return;
  const SketchTopology& topo = topology.value();

  // The inserted curve is the one entity whose id is new and whose kind is a
  // curve (Arc for a fillet, Line for a chamfer).
  const SketchTopologyEntity* inserted = nullptr;
  for (const auto& entity : topo.entities()) {
    if (std::find(before_entity_ids.begin(), before_entity_ids.end(), entity.id()) !=
        before_entity_ids.end())
      continue;
    if (entity.kind() == SketchTopologyEntityKind::Arc ||
        entity.kind() == SketchTopologyEntityKind::Line) {
      inserted = &entity;
      break;
    }
  }
  if (inserted == nullptr)
    return;

  const auto endpoint_ids = [](const SketchTopologyEntity& entity) -> std::vector<SketchPointId> {
    if (entity.kind() == SketchTopologyEntityKind::Arc)
      return {entity.points().front(), entity.points().back()};
    return {entity.points().front(), entity.points().back()};
  };
  const auto position = [&topo](const SketchPointId& id) -> std::optional<Point2> {
    const auto* point = topo.find_point(id);
    return point == nullptr ? std::nullopt : std::optional(point->position());
  };

  // For each trimmed input line, find the line endpoint that coincides with an
  // inserted-curve endpoint (the corner touch point), pin them Coincident, and
  // for fillets add a Tangent between the line and the arc.
  for (const SketchEntityId& line : {first_line, second_line}) {
    const std::string line_topology_id = "entity/" + line.value();
    const auto* line_entity = topo.find_entity(line_topology_id);
    if (line_entity == nullptr)
      continue;
    std::optional<SketchPointId> line_point;
    std::optional<SketchPointId> inserted_point;
    for (const auto& line_id : endpoint_ids(*line_entity)) {
      const auto line_pos = position(line_id);
      if (!line_pos)
        continue;
      for (const auto& inserted_id : endpoint_ids(*inserted)) {
        const auto inserted_pos = position(inserted_id);
        if (!inserted_pos)
          continue;
        if (std::hypot(line_pos->x - inserted_pos->x, line_pos->y - inserted_pos->y) <= 1.0e-6) {
          line_point = line_id;
          inserted_point = inserted_id;
          break;
        }
      }
      if (line_point)
        break;
    }
    if (!line_point || !inserted_point)
      continue;
    auto point_a = SketchConstraintIntentTarget::point(*line_point);
    auto point_b = SketchConstraintIntentTarget::point(*inserted_point);
    if (!point_a.has_error() && !point_b.has_error())
      (void)author_catalog_constraint(SketchSolverConstraintKind::Coincident,
                                      {point_a.value(), point_b.value()});
    if (add_tangent) {
      auto line_target = SketchConstraintIntentTarget::entity(line_topology_id);
      auto arc_target = SketchConstraintIntentTarget::entity(inserted->id());
      if (!line_target.has_error() && !arc_target.has_error())
        (void)author_catalog_constraint(SketchSolverConstraintKind::Tangent,
                                        {line_target.value(), arc_target.value()});
    }
  }

  // Pin the corner SIZE so it stays constant on drag. Coincident + Tangent keep
  // the corner attached and tangent but leave the radius / chamfer length as a
  // free DOF (it floated when a line was dragged). A driving Radius (fillet) or
  // Length (chamfer) dimension on the inserted curve removes exactly that DOF;
  // apply_dimension bakes the CURRENT measured value into an auto dN parameter,
  // so the commit does not move geometry. Skipped silently if not accepted.
  session_.selection().clear();
  if (session_.selection().add({GuiSelectionKind::SketchEntity, inserted->id()}))
    (void)apply_dimension(add_tangent ? SketchDimensionKind::Radius : SketchDimensionKind::Length,
                          /*reference=*/false);
}

void ShellSketchTools::begin_modify_tool(SketchModifyKind kind) {
  if (!sketch_context_ready())
    return;
  cancel_tool(); // drop any armed create tool (and clears modify_tool_)
  modify_tool_ = kind;
  const char* verb = kind == SketchModifyKind::Trim       ? "Trimmen"
                     : kind == SketchModifyKind::Extend   ? "Verlängern"
                                                          : "Teilen";
  report_message(std::string("Ändern: Kurve zum ") + verb + " anklicken (Esc beendet)");
  refresh_enablement();
}

void ShellSketchTools::handle_modify_press(SketchModifyKind kind,
                                           const std::optional<GuiSketchHit>& hit) {
  auto topology = active_topology();
  if (topology.has_error())
    return;
  std::optional<SketchEntityId> entity_id;
  if (hit && hit->kind == GuiSketchHitKind::Curve)
    entity_id = raw_entity_id(topology.value(), hit->semantic_id);
  if (!entity_id) {
    report_message("Ändern: bitte eine Linie oder einen Bogen anklicken");
    return;
  }
  const auto applied = apply_modify_pick(kind, entity_id->value(), hit->plane_point);
  if (applied.has_error())
    report(applied.error()); // tool stays armed for the next curve
  window_.refresh();
}

Result<std::size_t> ShellSketchTools::delete_selection() {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "keine aktive Skizze"));
  const SketchId sketch = *window_.active_sketch();

  // Collect EVERYTHING selected: constraint glyphs, dimensions, entities.
  const std::string constraint_prefix = "sketch/" + sketch.value() + "/constraint/";
  const std::string dimension_prefix = "sketch/" + sketch.value() + "/dimension/";
  std::vector<SketchConstraintId> constraint_ids;
  std::vector<SketchDimensionId> dimension_ids;
  std::vector<std::string> entity_ids;
  auto topology = active_topology();
  if (topology.has_error())
    return Result<std::size_t>::failure(topology.error());
  for (const auto& selection : session_.selection().entries()) {
    if (std::string_view(selection.semantic_id).starts_with(constraint_prefix)) {
      constraint_ids.emplace_back(selection.semantic_id.substr(constraint_prefix.size()));
      continue;
    }
    if (std::string_view(selection.semantic_id).starts_with(dimension_prefix)) {
      dimension_ids.emplace_back(selection.semantic_id.substr(dimension_prefix.size()));
      continue;
    }
    auto target = selection_target(topology.value(), selection.semantic_id);
    if (target && target->kind() == SketchConstraintIntentTargetKind::Entity &&
        std::find(entity_ids.begin(), entity_ids.end(), target->id()) == entity_ids.end())
      entity_ids.push_back(target->id());
  }
  if (constraint_ids.empty() && dimension_ids.empty() && entity_ids.empty())
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch",
        "nichts Löschbares ausgewählt (Abhängigkeit, Bemaßung oder Geometrie)"));

  // One transaction: drop the selected intents, then every intent referencing
  // a doomed entity (or its points), then the entities plus orphaned points.
  auto removed = session_.commit_part_sketch_intent_transaction(
      "Auswahl löschen",
      [sketch, constraint_ids, dimension_ids,
       entity_ids](PartDocument& part, std::vector<SketchConstraintCatalog>& constraint_catalogs,
                   std::vector<SketchDimensionCatalog>& dimension_catalogs)
          -> Result<std::size_t> {
        const Sketch* source = part.find_sketch(sketch);
        if (source == nullptr)
          return Result<std::size_t>::failure(
              Error::validation(sketch.value(), "aktive Skizze existiert nicht mehr"));
        auto topology = SketchTopology::migrate_legacy(*source);
        if (topology.has_error())
          return Result<std::size_t>::failure(topology.error());
        // Closed-profile groups: deleting any member deletes the WHOLE group
        // (container profile + all member curves). The legacy Sketch JSON can
        // only represent the shared corner points THROUGH the profile, so a
        // partial group cannot round-trip without identity loss.
        std::vector<std::string> doomed_entities;
        const auto add_doomed = [&doomed_entities](const std::string& id) {
          if (std::find(doomed_entities.begin(), doomed_entities.end(), id) ==
              doomed_entities.end())
            doomed_entities.push_back(id);
        };
        for (const auto& entity : topology.value().entities()) {
          if (std::find(entity_ids.begin(), entity_ids.end(), entity.id()) != entity_ids.end())
            continue;
          for (const auto& dependency : entity.entity_dependencies()) {
            if (std::find(entity_ids.begin(), entity_ids.end(), dependency) !=
                entity_ids.end()) {
              add_doomed(entity.id()); // the container goes first …
              for (const auto& member : entity.entity_dependencies())
                add_doomed(member); // … together with every member
              break;
            }
          }
        }
        for (const auto& id : entity_ids)
          add_doomed(id);

        std::vector<SketchPointId> doomed_points;
        for (const auto& entity_id : doomed_entities) {
          const auto* target = topology.value().find_entity(entity_id);
          if (target == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(entity_id, "Skizzengeometrie existiert nicht"));
          doomed_points.insert(doomed_points.end(), target->points().begin(),
                               target->points().end());
        }
        const auto references_doomed = [&](const auto& intent) {
          for (const auto& intent_target : intent.targets()) {
            if (std::find(doomed_entities.begin(), doomed_entities.end(),
                          intent_target.id()) != doomed_entities.end())
              return true;
            for (const auto& point_id : doomed_points)
              if (intent_target.id() == point_id.value())
                return true;
          }
          return false;
        };
        std::size_t removed_count = 0;
        for (auto& catalog : constraint_catalogs) {
          if (catalog.sketch() != sketch)
            continue;
          std::vector<SketchConstraintId> doomed = constraint_ids;
          for (const auto& intent : catalog.constraints())
            if (references_doomed(intent) &&
                std::find(doomed.begin(), doomed.end(), intent.id()) == doomed.end())
              doomed.push_back(intent.id());
          for (const auto& id : doomed) {
            if (catalog.find(id) == nullptr)
              continue;
            if (auto dropped = catalog.remove(id); dropped.has_error())
              return Result<std::size_t>::failure(dropped.error());
            ++removed_count;
          }
        }
        for (auto& catalog : dimension_catalogs) {
          if (catalog.sketch() != sketch)
            continue;
          std::vector<SketchDimensionId> doomed = dimension_ids;
          for (const auto& intent : catalog.dimensions())
            if (references_doomed(intent) &&
                std::find(doomed.begin(), doomed.end(), intent.id()) == doomed.end())
              doomed.push_back(intent.id());
          for (const auto& id : doomed) {
            if (catalog.find(id) == nullptr)
              continue;
            if (auto dropped = catalog.remove(id); dropped.has_error())
              return Result<std::size_t>::failure(dropped.error());
            ++removed_count;
          }
        }
        if (!doomed_entities.empty()) {
          auto deleted = delete_sketch_entities(part, sketch, doomed_entities);
          if (deleted.has_error())
            return deleted;
          removed_count += doomed_entities.size();
        }
        return Result<std::size_t>::success(removed_count);
      });
  if (!removed.has_error()) {
    session_.selection().clear();
    publish_scene();
    arm_drag_controller();
  }
  return removed;
}

bool ShellSketchTools::edit_selected_dimension_interactive() {
  if (!selected_dimension())
    return false;
  bool accepted = false;
  const QString text = QInputDialog::getText(&window_, QStringLiteral("Bemaßung bearbeiten"),
                                             QStringLiteral("Wert oder Formel:"),
                                             QLineEdit::Normal, {}, &accepted);
  if (!accepted || text.trimmed().isEmpty())
    return false;
  const auto edited = edit_selected_dimension(text.trimmed().toStdString());
  if (edited.has_error())
    report(edited.error());
  window_.refresh();
  return !edited.has_error();
}

Result<std::size_t> ShellSketchTools::apply_dimension(SketchDimensionKind kind, bool reference,
                                                      std::optional<ParameterId> parameter) {
  auto topology = active_topology();
  if (topology.has_error())
    return Result<std::size_t>::failure(topology.error());
  auto targets = selected_targets(topology.value(), true);
  if (targets.has_error())
    return Result<std::size_t>::failure(targets.error());
  const auto compatible =
      GuiSketchDimensionController::compatible_kinds(topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "Auswahl ist mit dieser Bemaßung unvereinbar"));
  canonicalize_order(topology.value(), kind, targets.value());
  auto catalog = session_.sketch_dimension_catalog(topology.value().sketch());
  if (catalog.has_error())
    return Result<std::size_t>::failure(catalog.error());
  if (!reference && !parameter) {
    // Inventor-style: auto-create a dN parameter carrying the CURRENT measured
    // value, so committing the driving dimension does not move geometry.
    std::vector<SketchConstraintIntentTarget> measure_targets = targets.value();
    auto measure_intent = SketchDimensionIntent::create(
        next_dimension_id(catalog.value(), kind), kind, SketchDimensionMode::Reference,
        std::move(measure_targets), std::nullopt);
    if (measure_intent.has_error())
      return Result<std::size_t>::failure(measure_intent.error());
    auto measured =
        SketchDimensionMeasurementEvaluator::measure(topology.value(), measure_intent.value());
    if (measured.has_error())
      return Result<std::size_t>::failure(measured.error());
    std::size_t number = 1;
    while (session_.part_document()->find_parameter(
               ParameterId("d" + std::to_string(number))) != nullptr)
      ++number;
    const ParameterId parameter_id("d" + std::to_string(number));
    const Quantity value = measured.value().value;
    auto created = session_.commit_part_transaction(
        "Bemaßungsparameter anlegen", [&parameter_id, &value, kind](PartDocument& part) {
          auto created_parameter =
              kind == SketchDimensionKind::Angle
                  ? Parameter::create_angle(parameter_id, parameter_id.value(), value)
                  : Parameter::create_length(parameter_id, parameter_id.value(), value);
          if (created_parameter.has_error())
            return Result<std::size_t>::failure(created_parameter.error());
          return part.add_parameter(std::move(created_parameter.value()));
        });
    if (created.has_error())
      return Result<std::size_t>::failure(created.error());
    parameter = parameter_id;
  }
  auto intent = SketchDimensionIntent::create(
      next_dimension_id(catalog.value(), kind), kind,
      reference ? SketchDimensionMode::Reference : SketchDimensionMode::Driving,
      std::move(targets.value()), parameter);
  if (intent.has_error())
    return Result<std::size_t>::failure(intent.error());
  auto controller = GuiSketchDimensionController::create(session_, topology.value().sketch(),
                                                         std::move(intent.value()));
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  if (!controller.value().preview().authoring.accepted)
    return Result<std::size_t>::failure(
        Error::validation(controller.value().preview().authoring.candidate.id().value(),
                          "Bemaßungs-Vorschau wurde abgelehnt"));
  auto committed = controller.value().commit(session_);
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

Result<std::size_t> ShellSketchTools::edit_selected_dimension(const std::string& text) {
  const auto id = selected_dimension();
  if (!id || session_.part_document() == nullptr)
    return Result<std::size_t>::failure(Error::validation(
        "gui.shell.sketch", "genau eine Bemaßungs-Annotation auswählen"));
  auto catalog = session_.sketch_dimension_catalog(*window_.active_sketch());
  if (catalog.has_error())
    return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(*id);
  if (dimension == nullptr || !dimension->driving() || !dimension->parameter())
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "ausgewählte Bemaßung hat keinen treibenden Wert"));
  const Parameter* parameter = session_.part_document()->find_parameter(*dimension->parameter());
  if (parameter == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(id->value(), "Bemaßungsparameter existiert nicht"));
  Result<std::size_t> edited = [&] {
    if (parameter->is_expression())
      return GuiSketchDimensionController::edit_formula(session_, *window_.active_sketch(), *id,
                                                        text);
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value) || value <= 0.0)
      return Result<std::size_t>::failure(
          Error::validation(id->value(), "Bemaßungswert muss positiv und endlich sein"));
    auto quantity = dimension->kind() == SketchDimensionKind::Angle
                        ? Quantity::angle_deg(value, id->value())
                        : Quantity::length_mm(value, id->value());
    if (quantity.has_error())
      return Result<std::size_t>::failure(quantity.error());
    return GuiSketchDimensionController::edit_literal(session_, *window_.active_sketch(), *id,
                                                      quantity.value());
  }();
  if (!edited.has_error()) {
    publish_scene();
    arm_drag_controller();
  }
  return edited;
}

// --- drag ------------------------------------------------------------------

void ShellSketchTools::arm_drag_controller() {
  if (!sketch_context_ready()) {
    drag_controller_.reset();
    viewport_.set_sketch_drag_handles({});
    return;
  }
  if (drag_controller_ && drag_controller_->active())
    return;
  auto controller = GuiSketchDragController::create(session_, *window_.active_sketch());
  if (controller.has_error()) {
    report(controller.error());
    drag_controller_.reset();
    viewport_.set_sketch_drag_handles({});
    return;
  }
  drag_controller_.emplace(std::move(controller.value()));
  const auto& solve = drag_controller_->baseline_solve();
  workspace_.set_solve_feedback(solve.remaining_dof, solve_status_text(solve.status));
  publish_handles(drag_controller_->handles());
}

void ShellSketchTools::begin_drag(GuiSketchScreenPoint screen) {
  if (!drag_controller_ || drag_controller_->active() || !workspace_.active() ||
      session_.task().active())
    return;
  const auto handles = drag_controller_->handles();
  std::optional<GuiSketchDragHandle> nearest;
  double nearest_distance = kHandleToleranceDip;
  for (const auto& handle : handles) {
    auto handle_screen = viewport_.sketch_plane_to_screen(handle.position);
    if (handle_screen.has_error())
      continue;
    const double distance = screen_distance(screen, handle_screen.value());
    if (distance <= nearest_distance) {
      nearest_distance = distance;
      nearest = handle;
    }
  }
  if (!nearest)
    return;
  auto begun = drag_controller_->begin(nearest->id);
  if (begun.has_error()) {
    report(begun.error());
    return;
  }
  if (!workspace_.select_handle(session_, nearest->id)) {
    drag_controller_->cancel();
    report_message("Skizzen-Handle konnte den Drag-Lebenszyklus nicht starten");
    return;
  }
  viewport_.set_sketch_selection_enabled(false);
  viewport_.set_sketch_inference_anchor(nearest->position);
}

void ShellSketchTools::release_drag(Point2 final_pointer) {
  if (!drag_controller_ || !drag_controller_->active() || !drag_stage(workspace_.stage()))
    return;
  if (workspace_.stage() == GuiSketchInteractionStage::SelectedHandle &&
      !drag_controller_->has_pending() && !drag_controller_->processed_pointer()) {
    // Plain click on a handle (press+release without movement): nothing was
    // previewed, so nothing needs reverting. Republishing the scene here would
    // wipe the viewport's sketch selection mid multi-select (Ctrl+click).
    drag_controller_->cancel();
    (void)workspace_.escape(session_);
    viewport_.set_sketch_inference_anchor(std::nullopt);
    viewport_.set_sketch_selection_enabled(true);
    return;
  }
  if (workspace_.stage() == GuiSketchInteractionStage::SelectedHandle &&
      !workspace_.show_drag_candidate(session_)) {
    cancel_drag(true, "Sketch-Drag konnte die Freigabe-Vorschau nicht starten");
    return;
  }
  auto preview = drag_controller_->flush(final_pointer);
  solve_scheduled_ = false;
  if (preview.has_error()) {
    cancel_drag(true, preview.error().message());
    return;
  }
  publish_drag_preview(preview.value());
  auto committed = drag_controller_->commit_session_constraints(session_);
  if (committed.has_error()) {
    cancel_drag(true, committed.error().message());
    return;
  }
  if (!workspace_.commit_drag(session_))
    report_message("Drag übernommen, aber der Workspace-Lebenszyklus schloss nicht sauber");
  viewport_.set_sketch_inference_anchor(std::nullopt);
  viewport_.set_sketch_selection_enabled(true);
  drag_controller_.reset();
  arm_drag_controller();
  publish_scene();
  window_.refresh();
}

void ShellSketchTools::cancel_drag(bool cancel_workspace, const std::string& message) {
  if (!message.empty())
    report_message(message);
  if (drag_controller_) {
    publish_scene_for(drag_controller_->source_sketch());
    drag_controller_->cancel();
    publish_handles(drag_controller_->handles());
    const auto& solve = drag_controller_->baseline_solve();
    workspace_.set_solve_feedback(solve.remaining_dof, solve_status_text(solve.status));
  }
  if (cancel_workspace && drag_stage(workspace_.stage()))
    (void)workspace_.escape(session_);
  viewport_.set_sketch_inference_anchor(std::nullopt);
  viewport_.set_sketch_selection_enabled(true);
}

void ShellSketchTools::publish_drag_preview(const GuiSketchDragPreview& preview) {
  // Live drag preview: skip the annotation/glyph re-resolve (measure + layout
  // per pointer move made dense sketches laggy); the release republishes fully.
  publish_scene_for(preview.preview_sketch(), false);
  publish_handles(drag_controller_->handles_for_topology(preview.topology()));
  const auto& solve = preview.solve();
  workspace_.set_solve_feedback(solve.remaining_dof, solve_status_text(solve.status));
  show_workspace_status();
}

void ShellSketchTools::publish_handles(const std::vector<GuiSketchDragHandle>& handles) {
  std::vector<Point2> positions;
  positions.reserve(handles.size());
  for (const auto& handle : handles)
    positions.push_back(handle.position);
  viewport_.set_sketch_drag_handles(std::move(positions));
}

// --- pointer, selection, events -------------------------------------------

void ShellSketchTools::handle_pointer(Point2 raw_point, const GuiSketchSnapResult& snap,
                                      const std::optional<GuiSketchHit>& hit) {
  if (!workspace_.active())
    return;
  workspace_.set_cursor_coordinates(raw_point);
  workspace_.set_snap_inference(snap.inference);
  if (hit)
    candidate_by_semantic_[hit->semantic_id] = hit->candidate_id;
  if (sketch_idle_stage(workspace_.stage()))
    (void)workspace_.set_hover(hit.has_value());
  show_workspace_status();
}

void ShellSketchTools::synchronize_sketch_selection(const std::vector<GuiSelection>& selections) {
  if (synchronizing_selection_ || !workspace_.active())
    return;
  synchronizing_selection_ = true;
  session_.selection().clear();
  for (auto selection : selections) {
    if (selection.candidate_id.empty()) {
      const auto candidate = candidate_by_semantic_.find(selection.semantic_id);
      if (candidate != candidate_by_semantic_.end())
        selection.candidate_id = candidate->second;
    }
    (void)session_.selection().add(std::move(selection));
  }
  synchronizing_selection_ = false;
  refresh_enablement();
}

bool ShellSketchTools::eventFilter(QObject* watched, QEvent* event) {
  if (watched == &viewport_ && tool_active()) {
    if (event->type() == QEvent::MouseButtonPress) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::RightButton) {
        // Inventor-style: right-click finishes the active tool/chain.
        cancel_tool();
        return true;
      }
      if (mouse->button() == Qt::LeftButton) {
        // Deferred so the pick uses the snap result computed by the viewport's
        // own press handling (the filter runs first) — sequencing, not polling.
        QTimer::singleShot(0, this, [this] {
          if (!tool_active())
            return;
          const auto& snap = viewport_.sketch_snap_result();
          if (snap)
            (void)add_pick(snap->snapped_point, snap->kind);
        });
      }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton &&
          create_controller_->tool() == GuiSketchCreateTool::Polyline &&
          create_controller_->complete_ready()) {
        QTimer::singleShot(0, this, [this] {
          if (tool_active())
            (void)complete_tool();
        });
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      if (!preview_scheduled_) {
        preview_scheduled_ = true;
        QTimer::singleShot(0, this, [this] {
          preview_scheduled_ = false;
          update_preview();
        });
      }
    }
  } else if (watched == &viewport_ && event->type() == QEvent::MouseButtonDblClick &&
             !tool_active()) {
    // Double-click a dimension glyph to edit it (Inventor behavior). The
    // preceding press already selected it via the viewport hit path.
    const auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() == Qt::LeftButton && selected_dimension()) {
      QTimer::singleShot(0, this, [this] { (void)edit_selected_dimension_interactive(); });
      return true;
    }
  } else if (watched == &viewport_ &&
             (event->type() == QEvent::UngrabMouse ||
              event->type() == QEvent::WindowDeactivate) &&
             drag_controller_ && drag_controller_->active() && drag_stage(workspace_.stage())) {
    cancel_drag(true, "Sketch-Drag abgebrochen: Zeigererfassung verloren");
  } else if (watched == &window_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (tool_active()) {
      if (key->key() == Qt::Key_Escape) {
        if (!numeric_buffer_.empty()) {
          clear_numeric_buffer();
          return true;
        }
        if (!create_controller_->picks().empty()) {
          (void)create_controller_->pop_pick();
          update_preview();
          return true;
        }
        cancel_tool();
        return true;
      }
      if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (!numeric_buffer_.empty()) {
          apply_numeric_buffer();
          return true;
        }
        if (create_controller_->tool() == GuiSketchCreateTool::Polyline &&
            create_controller_->complete_ready()) {
          (void)complete_tool();
          return true;
        }
        return true;
      }
      if (key->key() == Qt::Key_Backspace && !numeric_buffer_.empty()) {
        numeric_buffer_.pop_back();
        mirror_numeric_buffer();
        return true;
      }
      const QString text = key->text();
      if (text.size() == 1 && numeric_character(text.front())) {
        if (numeric_buffer_.empty())
          (void)workspace_.begin_numeric_input(session_);
        numeric_buffer_ += text.toStdString();
        mirror_numeric_buffer();
        return true;
      }
    } else if (modify_tool_ && key->key() == Qt::Key_Escape) {
      modify_tool_.reset();
      report_message("Ändern beendet");
      refresh_enablement();
      return true;
    } else if (key->key() == Qt::Key_Escape && drag_controller_ && drag_controller_->active() &&
               drag_stage(workspace_.stage())) {
      cancel_drag(true, {});
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}

void ShellSketchTools::report(const Error& error) const {
  window_.report_error(error);
}

void ShellSketchTools::report_message(const std::string& message) const {
  window_.report_error(Error::validation("gui.shell.sketch", message));
}

void ShellSketchTools::show_workspace_status() const {
  if (!workspace_.active())
    return;
  const auto& status = workspace_.status();
  QString message = QString::fromStdString(status.tool_hint);
  if (status.cursor_coordinates)
    message += QStringLiteral("   |   %1, %2 mm")
                   .arg(status.cursor_coordinates->x, 0, 'f', 2)
                   .arg(status.cursor_coordinates->y, 0, 'f', 2);
  if (!status.snap_inference.empty())
    message += QStringLiteral("   |   Fang: %1").arg(QString::fromStdString(status.snap_inference));
  if (status.remaining_dof)
    message += QStringLiteral("   |   DOF: %1").arg(*status.remaining_dof);
  if (!status.solve_status.empty())
    message +=
        QStringLiteral("   |   Solver: %1").arg(QString::fromStdString(status.solve_status));
  if (!numeric_buffer_.empty())
    message += QStringLiteral("   |   Eingabe: %1").arg(QString::fromStdString(numeric_buffer_));
  window_.statusBar()->showMessage(message);
}

} // namespace blcad::gui
