#include "blcad/gui/shell/shell_sketch_tools.hpp"

#include "blcad/core/sketch_edit_commands.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"
#include "blcad/gui/gui_presentation_visibility.hpp"
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
#include <QLineEdit>
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

// Pending spline points are not yet in the document's hit scene. Snap closure to
// the first fit point in screen space, using the same tolerance at every zoom.
GuiSketchCreatePick create_pointer_pick(const GuiSketchCreateController& controller,
                                        const OcctViewport& viewport,
                                        const GuiSketchSnapResult& snap) {
  if (controller.tool() == GuiSketchCreateTool::Spline && controller.picks().size() >= 3U) {
    const auto first = controller.picks().front().point;
    auto a = viewport.sketch_plane_to_screen(first);
    auto b = viewport.sketch_plane_to_screen(snap.raw_point);
    if (a && b &&
        std::hypot(a.value().x - b.value().x, a.value().y - b.value().y) <=
            GuiSketchInteractionConfig{}.snap_tolerance_dip)
      return {first, GuiSketchSnapKind::Endpoint};
  }
  return {snap.snapped_point, snap.kind};
}

// Tool families as Inventor-style menu buttons; covers every MVP-8 create tool.
// Inventor-style: ONE line tool that chains segments (each click continues at
// the previous end, corners are coupled coincident); Escape ends the chain.
// The headless Polyline tool stays available programmatically but has no
// separate ribbon entry.
constexpr std::array<ToolDescriptor, 3> kLineTools{{
    {"sketch.create.line", "Line", GuiSketchCreateTool::Line,
     "Pick points; each new line continues from the endpoint, Esc finishes"},
    {"sketch.create.centerline", "Centerline", GuiSketchCreateTool::Centerline,
     "Dash-dot line: pick start and end"},
    {"sketch.create.construction_line", "Construction Line", GuiSketchCreateTool::ConstructionLine,
     "Dashed construction line: pick start and end"},
}};
constexpr std::array<ToolDescriptor, 5> kRectangleTools{{
    {"sketch.create.corner_rectangle", "Rectangle (2 Points)", GuiSketchCreateTool::CornerRectangle,
     "Pick two opposite corners"},
    {"sketch.create.center_rectangle", "Rectangle (Center)", GuiSketchCreateTool::CenterRectangle,
     "Pick center and one corner"},
    {"sketch.create.three_point_rectangle", "Rectangle (3 Points)",
     GuiSketchCreateTool::ThreePointRectangle, "Pick base edge and height point"},
    {"sketch.create.parallelogram", "Parallelogram", GuiSketchCreateTool::Parallelogram,
     "Pick three corners"},
    {"sketch.create.polygon", "Polygon", GuiSketchCreateTool::RegularPolygon,
     "Enter the number of sides, then pick center and vertex"},
}};
constexpr std::array<ToolDescriptor, 6> kCircleTools{{
    {"sketch.create.center_radius_circle", "Circle (Center/Radius)",
     GuiSketchCreateTool::CenterRadiusCircle, "Pick center and radius point"},
    {"sketch.create.center_diameter_circle", "Circle (Center/Diameter)",
     GuiSketchCreateTool::CenterDiameterCircle, "Pick center and diameter point"},
    {"sketch.create.two_point_circle", "Circle (2 Points)", GuiSketchCreateTool::TwoPointCircle,
     "Pick both diameter endpoints"},
    {"sketch.create.three_point_circle", "Circle (3 Points)", GuiSketchCreateTool::ThreePointCircle,
     "Pick three circle points"},
    {"sketch.create.tangent_circle", "Circle (Tangent)", GuiSketchCreateTool::TangentCircle,
     "Pick a tangent point on a curve, then a circle point"},
    {"sketch.create.ellipse", "Ellipse", GuiSketchCreateTool::Ellipse,
     "Pick center, major axis, and minor radius"},
}};
constexpr std::array<ToolDescriptor, 4> kArcTools{{
    {"sketch.create.center_start_end_arc", "Arc (Center/Start/End)",
     GuiSketchCreateTool::CenterStartEndArc, "Pick center, start, and end direction"},
    {"sketch.create.three_point_arc", "Arc (3 Points)", GuiSketchCreateTool::ThreePointArc,
     "Pick start, intermediate point, and end"},
    {"sketch.create.tangent_arc", "Arc (Tangent)", GuiSketchCreateTool::TangentArc,
     "Pick a curve endpoint, then the arc end"},
    {"sketch.create.spline", "Spline", GuiSketchCreateTool::Spline,
     "Click fit points in order; right-click finishes the spline"},
}};
constexpr std::array<ToolDescriptor, 2> kSlotTools{{
    {"sketch.create.center_slot", "Slot (Center–Center)", GuiSketchCreateTool::CenterSlot,
     "Pick both cap centers and half-width"},
    {"sketch.create.overall_slot", "Slot (Overall Length)", GuiSketchCreateTool::OverallSlot,
     "Pick overall endpoints and half-width"},
}};
constexpr ToolDescriptor kPointTool{"sketch.create.point", "Point", GuiSketchCreateTool::Point,
                                    "Pick the construction point position"};

struct ConstraintDescriptor {
  SketchSolverConstraintKind kind;
  const char* label;
  const char* description;
};

// PointOnObject and Midpoint have no own buttons: Koinzident on a point +
// curve/circle maps to point-on-object, Koinzident with a clicked line
// midpoint maps to Midpoint (Inventor semantics).
constexpr std::array<ConstraintDescriptor, 11> kConstraints{{
    {SketchSolverConstraintKind::Coincident, "Coincident",
     "Connect points or place a point on a curve/circle"},
    {SketchSolverConstraintKind::Fixed, "Grounded", "Fix a point or curve"},
    {SketchSolverConstraintKind::Horizontal, "Horizontal", "Make a line horizontal"},
    {SketchSolverConstraintKind::Vertical, "Vertical", "Make a line vertical"},
    {SketchSolverConstraintKind::Parallel, "Parallel", "Make two lines parallel"},
    {SketchSolverConstraintKind::Perpendicular, "Perpendicular", "Make two lines perpendicular"},
    {SketchSolverConstraintKind::Collinear, "Collinear", "Make two lines collinear"},
    {SketchSolverConstraintKind::Equal, "Equal", "Make two lines/arcs equal in size"},
    {SketchSolverConstraintKind::Tangent, "Tangent", "Make two connected curves tangent"},
    {SketchSolverConstraintKind::Concentric, "Concentric", "Make two centers coincident"},
    {SketchSolverConstraintKind::Symmetric, "Symmetric",
     "Pick the symmetry axis, then select the symmetric elements"},
}};

struct DimensionDescriptor {
  SketchDimensionKind kind;
  const char* label;
};

constexpr std::array<DimensionDescriptor, 9> kDimensions{{
    {SketchDimensionKind::HorizontalDistance, "Horizontal Distance"},
    {SketchDimensionKind::VerticalDistance, "Vertical Distance"},
    {SketchDimensionKind::AlignedDistance, "Aligned Distance"},
    {SketchDimensionKind::PointToPointDistance, "Point to Point"},
    {SketchDimensionKind::Length, "Length"},
    {SketchDimensionKind::Radius, "Radius"},
    {SketchDimensionKind::Diameter, "Diameter"},
    {SketchDimensionKind::Angle, "Angle"},
    {SketchDimensionKind::ArcLength, "Arc Length"},
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
  case SketchSolveStatus::FullyConstrained:
    return "Fully Constrained";
  case SketchSolveStatus::UnderConstrained:
    return "Underconstrained";
  case SketchSolveStatus::Redundant: return "Redundant";
  case SketchSolveStatus::Conflicting:
    return "Conflicting";
  case SketchSolveStatus::NonConvergent:
    return "Not Converged";
  case SketchSolveStatus::InvalidReference:
    return "Invalid Reference";
  }
  return "Not Evaluated";
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
        Error::validation(sketch.value(), "active sketch no longer exists"));
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
          Error::validation(entity_id, "sketch geometry does not exist"));
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
        "Deletion violates referenced dimensions/constraints – remove these first"));
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
  dimension_editor_ = new QLineEdit(&viewport_);
  dimension_editor_->setObjectName(QStringLiteral("sketch.dimension.editor"));
  dimension_editor_->setPlaceholderText(QStringLiteral("Value or =Formula"));
  dimension_editor_->hide();
  dimension_editor_->installEventFilter(this);
  connect(dimension_editor_, &QLineEdit::returnPressed, this, [this] {
    if (!dimension_editor_id_) return;
    const auto edited = edit_dimension(*dimension_editor_id_,
                                      dimension_editor_->text().trimmed().toStdString());
    if (edited.has_error()) {
      report(edited.error());
      dimension_editor_->setToolTip(QString::fromStdString(edited.error().message()));
      return;
    }
    dimension_editor_->hide();
    dimension_editor_id_.reset();
    viewport_.setFocus();
    window_.refresh();
  });
  build_ribbon_groups();
  wire_viewport_callbacks();
  viewport_.installEventFilter(this);
  window_.installEventFilter(this);
}

void ShellSketchTools::build_ribbon_groups() {
  const auto make_tool_action = [this](const ToolDescriptor& descriptor) {
    auto* action = new QAction(QString::fromUtf8(descriptor.label), this);
    action->setObjectName(QString::fromUtf8(descriptor.command));
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

  auto* draw_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Draw"));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Line"), make_tool_menu(kLineTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Rectangle"), make_tool_menu(kRectangleTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Circle"), make_tool_menu(kCircleTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Arc"), make_tool_menu(kArcTools));
  ribbon_.add_menu_button(draw_group, QStringLiteral("Slot"), make_tool_menu(kSlotTools));
  ribbon_.add_button(draw_group, make_tool_action(kPointTool));

  auto* constrain_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Constraints"));
  for (const auto& descriptor : kConstraints) {
    auto* action = new QAction(QString::fromUtf8(descriptor.label), this);
    action->setToolTip(QString::fromUtf8(descriptor.description));
    connect(action, &QAction::triggered, this, [this, kind = descriptor.kind] {
      if (kind == SketchSolverConstraintKind::Symmetric) {
        show_operation_dialog(GuiSketchOperationKind::Symmetric);
        return;
      }
      const auto applied = apply_constraint(kind);
      if (applied.has_error())
        report(applied.error());
      window_.refresh();
    });
    ribbon_.add_small_button(constrain_group, action);
    constraint_actions_.push_back(action);
  }
  delete_constraint_action_ = new QAction(QStringLiteral("Delete (Del)"), this);
  delete_constraint_action_->setToolTip(
      QStringLiteral("Delete selected constraint, dimension, or sketch geometry"));
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
  auto* modify_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Modify"));
  struct ModifyToolButton {
    const char* label;
    const char* tip;
    SketchModifyKind kind;
  };
  static constexpr std::array<ModifyToolButton, 3> kModifyTools{{
      {"Trim", "Click a curve or hold the left mouse button and stroke across curves",
       SketchModifyKind::Trim},
      {"Extend", "Click a curve to extend it to the next intersection", SketchModifyKind::Extend},
      {"Split", "Split the curve at the clicked position", SketchModifyKind::Split},
  }};
  for (const auto& button : kModifyTools) {
    auto* action = new QAction(QString::fromUtf8(button.label), this);
    action->setToolTip(QString::fromUtf8(button.tip));
    connect(action, &QAction::triggered, this,
            [this, kind = button.kind] { begin_modify_tool(kind); });
    ribbon_.add_small_button(modify_group, action);
    modify_actions_.push_back(action);
  }
  fillet_action_ = new QAction(QStringLiteral("Fillet"), this);
  fillet_action_->setToolTip(QStringLiteral("Fillet two connected curves (enter radius)"));
  connect(fillet_action_, &QAction::triggered, this, [this] {
    bool accepted = false;
    const double radius =
        QInputDialog::getDouble(&window_, QStringLiteral("Fillet"), QStringLiteral("Radius (mm):"),
                                5.0, 0.001, 1.0e6, 3, &accepted);
    if (!accepted)
      return;
    const auto applied = apply_fillet(radius);
    if (applied.has_error())
      report(applied.error());
    window_.refresh();
  });
  ribbon_.add_small_button(modify_group, fillet_action_);
  chamfer_action_ = new QAction(QStringLiteral("Chamfer"), this);
  chamfer_action_->setToolTip(QStringLiteral("Chamfer two connected curves (enter distance)"));
  connect(chamfer_action_, &QAction::triggered, this, [this] {
    bool accepted = false;
    const double distance =
        QInputDialog::getDouble(&window_, QStringLiteral("Chamfer"),
                                QStringLiteral("Distance (mm):"), 5.0, 0.001, 1.0e6, 3, &accepted);
    if (!accepted)
      return;
    const auto applied = apply_chamfer(distance);
    if (applied.has_error())
      report(applied.error());
    window_.refresh();
  });
  ribbon_.add_small_button(modify_group, chamfer_action_);
  build_operation_group();

  auto* dimension_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Dimension"));
  rapid_dimension_action_ = new QAction(QStringLiteral("Rapid Dimension"), this);
  rapid_dimension_action_->setObjectName(QStringLiteral("sketch.dimension.rapid"));
  rapid_dimension_action_->setCheckable(true);
  rapid_dimension_action_->setShortcut(QKeySequence(Qt::Key_D));
  rapid_dimension_action_->setToolTip(
      QStringLiteral("Hover a curve or pick two points, then click to place. D switches "
                     "radius/diameter; Escape cancels."));
  connect(rapid_dimension_action_, &QAction::triggered, this, [this] {
    if (rapid_dimension_active_) {
      rapid_alternate_radial_ = !rapid_alternate_radial_;
      if (viewport_.sketch_snap_result())
        update_rapid_dimension(viewport_.sketch_snap_result()->raw_point,
                               viewport_.hovered_sketch_hit());
    } else
      (void)begin_rapid_dimension();
    rapid_dimension_action_->setChecked(rapid_dimension_active_);
  });
  window_.addAction(rapid_dimension_action_);
  ribbon_.add_small_button(dimension_group, rapid_dimension_action_);
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
  ribbon_.add_menu_button(dimension_group, QStringLiteral("Dimension"), dimension_menu);
  reference_dimension_action_ = new QAction(QStringLiteral("Reference"), this);
  reference_dimension_action_->setCheckable(true);
  reference_dimension_action_->setToolTip(
      QStringLiteral("Create new dimensions as reference dimensions (not driving)"));
  ribbon_.add_small_button(dimension_group, reference_dimension_action_);
  edit_dimension_action_ = new QAction(QStringLiteral("Modify"), this);
  edit_dimension_action_->setToolTip(
      QStringLiteral("Edit selected dimension value (or double-click)"));
  connect(edit_dimension_action_, &QAction::triggered, this,
          [this] { (void)edit_selected_dimension_interactive(); });
  ribbon_.add_small_button(dimension_group, edit_dimension_action_);

  auto* grid_group = ribbon_.add_group(sketch_tab_, QStringLiteral("Grid"));
  grid_action_ = new QAction(QStringLiteral("Grid"), this);
  grid_action_->setCheckable(true);
  grid_action_->setChecked(true);
  grid_snap_action_ = new QAction(QStringLiteral("Snap"), this);
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
  viewport_.set_sketch_selection_callback([this](const std::vector<GuiSelection>& selections) {
    synchronize_sketch_selection(selections);
  });
  viewport_.set_sketch_drag_pointer_callback([this](GuiSketchScreenPoint screen, Point2 raw,
                                                    const GuiSketchSnapResult& snap,
                                                    const std::optional<GuiSketchHit>&) {
    if (operation_pointer_) {
      operation_pointer_(snap.snapped_point);
      return;
    }
    if (trim_stroke_active_) {
      continue_trim_stroke(screen);
      return;
    }
    if (label_drag_) {
      // Dimension label drag: move the badge with the raw pointer.
      label_drag_delta_.x += raw.x - label_drag_last_.x;
      label_drag_delta_.y += raw.y - label_drag_last_.y;
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
      cancel_drag(true, "Sketch drag could not start live preview");
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
        if (!drag_controller_ || !drag_controller_->active() || !drag_controller_->has_pending())
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
        if (rapid_dimension_active_) {
          if (phase == GuiSketchPointerPhase::Press)
            rapid_dimension_press(raw, hit);
          return;
        }
        if (operation_phase_) {
          operation_phase_(phase, snap.snapped_point, hit);
          return;
        }
        if (tool_active())
          return; // create tools capture picks via the viewport event filter
        if (modify_tool_) {
          // Trim supports Inventor-style paint trimming while the left button
          // is held. Extend and Split intentionally stay single-press tools.
          if (*modify_tool_ == SketchModifyKind::Trim) {
            if (phase == GuiSketchPointerPhase::Press)
              begin_trim_stroke(screen);
            else
              end_trim_stroke();
          } else if (phase == GuiSketchPointerPhase::Press) {
            handle_modify_press(*modify_tool_, hit);
          }
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
              auto catalog = session_.sketch_dimension_catalog(*window_.active_sketch());
              const auto* dimension = catalog ? catalog.value().find(SketchDimensionId(*label_drag_))
                                              : nullptr;
              if (!dimension) {
                label_drag_.reset();
                return;
              }
              label_drag_delta_ = {};
              label_drag_initial_offset_ = dimension->label_offset();
              label_drag_last_ = raw;
              viewport_.set_sketch_selection_enabled(false);
              return;
            }
          }
          begin_drag(screen);
        } else if (label_drag_) {
          const Point2 offset{label_drag_initial_offset_.x + label_drag_delta_.x + raw.x - label_drag_last_.x,
                              label_drag_initial_offset_.y + label_drag_delta_.y + raw.y - label_drag_last_.y};
          const auto moved = GuiSketchDimensionController::set_label_offset(
              session_, *window_.active_sketch(), SketchDimensionId(*label_drag_), offset);
          if (moved.has_error())
            report(moved.error());
          cancel_label_drag();
          window_.refresh();
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
  if (close_operation_)
    close_operation_();
  cancel_tool();
  dimension_editor_->hide();
  dimension_editor_id_.reset();
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
  auto scene =
      session_.part_shape_cache() != nullptr
          ? GuiSketchInteractionSceneBuilder{}.build(*part, sketch, *session_.part_shape_cache())
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
  // Preview adds only the uncommitted delta to the persisted glyph offset.
  if (label_drag_) {
    for (auto& annotation : scene.value().annotations) {
      if (annotation.hit_kind == GuiSketchHitKind::Dimension &&
          annotation.candidate_id == *label_drag_) {
        annotation.point.x += label_drag_delta_.x;
        annotation.point.y += label_drag_delta_.y;
      }
    }
  }

  filter_presentation_visibility(session_, scene.value());
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
  if (rapid_dimension_action_)
    rapid_dimension_action_->setEnabled(sketch_context_ready() &&
                                        (!session_.task().active() || rapid_dimension_active_));
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
      enabled =
          std::find(compatible_constraints.begin(), compatible_constraints.end(),
                    SketchSolverConstraintKind::PointOnObject) != compatible_constraints.end();
    if (kConstraints[index].kind == SketchSolverConstraintKind::Symmetric)
      enabled = active;
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
  const auto corner_ids = selected_sketch_entity_ids();
  const auto* selected_sketch = active ? session_.part_document()->find_sketch(*window_.active_sketch()) : nullptr;
  const bool two_curves = selected_sketch && session_.selection().size() == 2U && corner_ids.size() == 2U &&
      std::all_of(corner_ids.begin(), corner_ids.end(), [&](const auto& id) {
        return selected_sketch->find_line_segment(id) || selected_sketch->find_arc_segment(id) ||
               selected_sketch->find_spline_segment(id);
      });
  if (fillet_action_ != nullptr)
    fillet_action_->setEnabled(two_curves);
  if (chamfer_action_ != nullptr)
    chamfer_action_->setEnabled(two_curves);
  const auto operation_ids = selected_operation_entities();
  const bool operation_selection = active && operation_ids && !operation_ids.value().empty();
  const auto* operation_sketch =
      active ? session_.part_document()->find_sketch(*window_.active_sketch()) : nullptr;
  for (auto* action : operation_actions_) {
    bool compatible = operation_selection;
    if (compatible && action->objectName() == QStringLiteral("sketch.operation.offset")) {
      compatible =
          std::all_of(operation_ids.value().begin(), operation_ids.value().end(),
                      [&](const auto& id) { return operation_sketch->find_line_segment(id); }) ||
          (operation_ids.value().size() == 1U &&
           operation_sketch->find_arc_segment(operation_ids.value().front()));
    }
    action->setEnabled(active && (session_.selection().empty() || compatible));
  }
  if (project_action_) project_action_->setEnabled(active);
  if (break_link_action_) {
    const auto ids = selected_sketch_entity_ids();
    const auto* sketch = active ? session_.part_document()->find_sketch(*window_.active_sketch()) : nullptr;
    break_link_action_->setEnabled(sketch && session_.selection().size() == 1U && ids.size() == 1U && sketch->find_projected_line(ids.front()));
  }
  if (delete_constraint_action_ != nullptr)
    delete_constraint_action_->setEnabled(active && (selected_accepted_constraint().has_value() ||
                                                     selected_dimension().has_value() ||
                                                     selected_entity().has_value()));
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
        Error::validation("gui.shell.sketch", "no active sketch"));
  const Sketch* sketch = session_.part_document()->find_sketch(*window_.active_sketch());
  if (sketch == nullptr)
    return Result<SketchTopology>::failure(
        Error::validation(window_.active_sketch()->value(), "active sketch no longer exists"));
  return SketchTopology::migrate_legacy(*sketch);
}

Result<std::vector<SketchConstraintIntentTarget>>
ShellSketchTools::selected_targets(const SketchTopology& topology, bool resolve_point_roles) const {
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
          if (const auto point_id = resolve_point_role(topology, *entity, selection.candidate_id)) {
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
          selection.semantic_id, "Selection is not a stable point or curve target"));
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
  const char* hint = tool == GuiSketchCreateTool::Spline
                         ? "Spline: pick fit points; click the start point to close; "
                           "right-click finishes"
                         : "Pick points in the viewport";
  if (!workspace_.begin_command(session_, "sketch.create", hint)) {
    report_message("Sketch tool could not start the workspace lifecycle");
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

bool ShellSketchTools::add_pick(Point2 point, GuiSketchSnapKind kind,
                                std::optional<SketchEntityId> tangent_reference) {
  if (!tool_active())
    return false;
  const bool tangent = create_controller_->tool() == GuiSketchCreateTool::TangentArc ||
                       create_controller_->tool() == GuiSketchCreateTool::TangentCircle;
  auto added = [&]() -> Result<std::size_t> {
    if (!tangent || !create_controller_->picks().empty()) return create_controller_->add_pick({point, kind});
    if (!tangent_reference) {
      auto screen = viewport_.sketch_plane_to_screen(point);
      auto topology = active_topology();
      if (screen && topology) {
        auto hits = viewport_.sketch_curve_hits_along(screen.value(), screen.value());
        if (hits) for (const auto& hit : hits.value()) {
          auto target = selection_target(topology.value(), hit.semantic_id);
          if (target && target->kind() == SketchConstraintIntentTargetKind::Entity &&
              target->id().starts_with("entity/")) {
            tangent_reference = SketchEntityId(target->id().substr(7));
            break;
          }
        }
      }
    }
    if (!tangent_reference)
      return Result<std::size_t>::failure(
          Error::validation("tangent", "Select a source curve at the tangent point"));
    return create_controller_->add_tangent_reference(*session_.part_document(), *tangent_reference, point);
  }();
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

void ShellSketchTools::cancel_label_drag() {
  if (!label_drag_) return;
  label_drag_.reset();
  label_drag_delta_ = {};
  viewport_.set_sketch_selection_enabled(true);
  publish_scene();
}

bool ShellSketchTools::finish_tool() {
  if (tool_active() && create_controller_->tool() == GuiSketchCreateTool::Spline &&
      create_controller_->complete_ready())
    return complete_tool();
  cancel_tool();
  return true;
}

void ShellSketchTools::cancel_tool() {
  cancel_operation_preview();
  if (rapid_dimension_active_) {
    rapid_dimension_active_ = false;
    rapid_dimension_action_->setChecked(false);
    rapid_targets_.clear();
    rapid_candidate_.reset();
    viewport_.set_sketch_dimension_preview(std::nullopt);
    (void)workspace_.escape(session_);
    refresh_enablement();
  }
  cancel_label_drag();
  chain_anchor_.reset();
  modify_tool_.reset(); // switching/cancelling any tool ends a modal modify tool
  cancel_trim_stroke();
  viewport_.set_sketch_selection_enabled(true);
  if (!create_controller_)
    return;
  create_controller_.reset();
  numeric_buffer_.clear();
  viewport_.set_sketch_preview_polyline({}, false);
  viewport_.set_sketch_inference_anchor(std::nullopt);
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
      if (std::find(corner_points.begin(), corner_points.end(), point_id) == corner_points.end())
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
    hover = create_pointer_pick(*create_controller_, viewport_, *snap);
  const auto preview = create_controller_->preview(hover);
  viewport_.set_sketch_preview_polyline(preview.rubber_band, preview.closed);
  if (!preview.constraints.empty())
    workspace_.set_snap_inference("Constraint preview: " + preview.constraints.back().kind);
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
      const std::string_view role =
          separator == std::string::npos
              ? std::string_view{}
              : std::string_view(selection.candidate_id).substr(separator + 1U);
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

  auto targets =
      explicit_targets
          ? Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(*explicit_targets))
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
        "gui.shell.sketch", "current selection is incompatible with this constraint"));
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
        "Constraint preview rejected: " + std::string(to_string(preview.solve.status));
    if (!preview.conflict_ids.empty()) {
      message += " [";
      for (std::size_t index = 0U; index < preview.conflict_ids.size(); ++index) {
        if (index > 0U)
          message += ", ";
        message += preview.conflict_ids[index];
      }
      message += "]";
    }
    // Solver detail for diagnosis: separates a stall (zero iterations) from the
    // iteration limit, and shows how far the residual fell.
    const auto& solve = preview.solve;
    message += " (iter=" + std::to_string(solve.iterations) + ", rms " +
               std::to_string(solve.residuals.initial_rms) + "→" +
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
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "select exactly one accepted constraint glyph"));
  auto removed =
      GuiSketchConstraintController::remove_accepted(session_, *window_.active_sketch(), *selected);
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
  // Point hits can carry an internal endpoint/midpoint role suffix. Normalize
  // them exactly like a real selection before resolving the owning entity.
  const GuiSelection selection(GuiSelectionKind::SketchEntity, semantic_id);
  const auto target = selection_target(topology, selection.semantic_id);
  if (!target || target->kind() != SketchConstraintIntentTargetKind::Entity)
    return std::nullopt;
  constexpr std::string_view kEntityPrefix = "entity/";
  std::string_view id = target->id();
  if (!id.starts_with(kEntityPrefix))
    return std::nullopt; // profiles etc. are not trim/fillet curves
  return SketchEntityId(std::string(id.substr(kEntityPrefix.size())));
}

// Full circles are still represented by a profile rather than a core curve
// entity. Keep that representation visible to Trim; the document-aware modify
// service explodes the picked remainder into ordinary arc geometry.
[[nodiscard]] std::optional<ProfileId> raw_profile_id(const SketchTopology& topology,
                                                      const std::string& semantic_id) {
  const GuiSelection selection(GuiSelectionKind::SketchEntity, semantic_id);
  const auto target = selection_target(topology, selection.semantic_id);
  if (!target || target->kind() != SketchConstraintIntentTargetKind::Entity)
    return std::nullopt;
  constexpr std::string_view kProfilePrefix = "profile/";
  const std::string_view id = target->id();
  if (!id.starts_with(kProfilePrefix))
    return std::nullopt;
  const SketchTopologyEntity* entity = topology.find_entity(target->id());
  if (entity == nullptr || entity->kind() != SketchTopologyEntityKind::CircleProfile)
    return std::nullopt;
  return ProfileId(std::string(id.substr(kProfilePrefix.size())));
}

struct ResolvedModifyHit {
  std::optional<SketchEntityId> entity;
  std::optional<ProfileId> profile;
  Point2 pick;
  std::string stroke_key;
};

[[nodiscard]] std::optional<ResolvedModifyHit>
resolve_modify_hit(const SketchTopology& topology, SketchModifyKind kind,
                   const std::optional<GuiSketchHit>& hit) {
  if (!hit)
    return std::nullopt;

  ResolvedModifyHit result;
  result.pick = hit->plane_point;
  // Curve points belong to the same editable entity. Accepting them for Trim
  // makes painting across endpoints/quadrants behave like painting the curve;
  // Split also accepts midpoint hits; an endpoint is rejected by the split service.
  if (hit->kind == GuiSketchHitKind::Curve ||
      ((kind == SketchModifyKind::Trim || kind == SketchModifyKind::Split) &&
       hit->kind == GuiSketchHitKind::Point))
    result.entity = raw_entity_id(topology, hit->semantic_id);
  if (kind == SketchModifyKind::Trim &&
      (hit->kind == GuiSketchHitKind::Curve || hit->kind == GuiSketchHitKind::Point) &&
      !result.entity)
    result.profile = raw_profile_id(topology, hit->semantic_id);
  if (!result.entity && !result.profile)
    return std::nullopt;

  result.stroke_key = result.entity ? "entity/" + result.entity->value()
                                    : "profile/" + result.profile->value();
  return result;
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

Result<std::vector<SketchEntityId>> ShellSketchTools::selected_operation_entities() const {
  std::vector<SketchEntityId> ids;
  auto topology = active_topology();
  if (!topology)
    return Result<std::vector<SketchEntityId>>::failure(topology.error());
  const auto reject = [](const std::string& id) {
    return Result<std::vector<SketchEntityId>>::failure(
        Error::validation(id, "The entire selection must contain editable lines, arcs, splines, "
                              "or their closed profiles"));
  };
  const auto append = [&](const SketchTopologyEntity* entity) {
    if (!entity ||
        (entity->kind() != SketchTopologyEntityKind::Line &&
         entity->kind() != SketchTopologyEntityKind::Arc &&
         entity->kind() != SketchTopologyEntityKind::Spline) ||
        !std::string_view(entity->id()).starts_with("entity/"))
      return false;
    SketchEntityId id(entity->id().substr(7));
    if (std::find(ids.begin(), ids.end(), id) == ids.end())
      ids.push_back(std::move(id));
    return true;
  };
  for (const auto& selection : session_.selection().entries()) {
    if (selection.kind != GuiSelectionKind::SketchEntity)
      return reject(selection.semantic_id);
    auto target = selection_target(topology.value(), selection.semantic_id);
    if (!target || target->kind() != SketchConstraintIntentTargetKind::Entity)
      return reject(selection.semantic_id);
    const auto* entity = topology.value().find_entity(target->id());
    if (append(entity))
      continue;
    if (!entity ||
        (entity->kind() != SketchTopologyEntityKind::ClosedProfile &&
         entity->kind() != SketchTopologyEntityKind::ArcClosedProfile &&
         entity->kind() != SketchTopologyEntityKind::CompositeClosedProfile) ||
        entity->entity_dependencies().empty())
      return reject(selection.semantic_id);
    for (const auto& dependency : entity->entity_dependencies()) {
      if (!append(topology.value().find_entity(dependency)))
        return reject(selection.semantic_id);
    }
  }
  return Result<std::vector<SketchEntityId>>::success(std::move(ids));
}

Result<std::size_t> ShellSketchTools::apply_modify_pick(SketchModifyKind kind,
                                                        const std::string& entity_id, Point2 pick) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation("gui.shell.sketch", "no active sketch"));
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  const SketchEntityId target(entity_id);
  Result<GuiSketchModifyPreview> preview = Result<GuiSketchModifyPreview>::failure(
      Error::validation("gui.shell.sketch", "unknown modification type"));
  switch (kind) {
  case SketchModifyKind::Trim:
    preview = controller.value().trim(target, pick);
    break;
  case SketchModifyKind::Extend:
    preview = controller.value().extend(target, pick);
    break;
  case SketchModifyKind::Split:
    preview = controller.value().split(target, pick);
    break;
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

Result<std::size_t> ShellSketchTools::apply_modify_profile_pick(SketchModifyKind kind,
                                                                const ProfileId& profile_id,
                                                                Point2 pick) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation("gui.shell.sketch", "no active sketch"));
  if (kind != SketchModifyKind::Trim)
    return Result<std::size_t>::failure(
        Error::validation(profile_id.value(), "circle profiles can only be trimmed"));
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  auto preview = controller.value().trim(profile_id, pick);
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

Result<std::size_t> ShellSketchTools::apply_fillet(double radius) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation("gui.shell.sketch", "no active sketch"));
  const auto ids = selected_sketch_entity_ids();
  if (ids.size() != 2U || session_.selection().size() != 2U)
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "select exactly two connected curves for fillet"));
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  auto preview = controller.value().fillet(ids[0], ids[1], radius);
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto committed = controller.value().commit(session_, true);
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

Result<std::size_t> ShellSketchTools::apply_chamfer(double distance) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation("gui.shell.sketch", "no active sketch"));
  const auto ids = selected_sketch_entity_ids();
  if (ids.size() != 2U || session_.selection().size() != 2U)
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "select exactly two connected curves for chamfer"));
  auto controller = GuiSketchModifyController::create(session_, *window_.active_sketch());
  if (controller.has_error())
    return Result<std::size_t>::failure(controller.error());
  auto preview = controller.value().chamfer(ids[0], ids[1], distance);
  if (preview.has_error())
    return Result<std::size_t>::failure(preview.error());
  auto committed = controller.value().commit(session_, true);
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

void ShellSketchTools::begin_modify_tool(SketchModifyKind kind) {
  if (!sketch_context_ready())
    return;
  cancel_tool(); // drop any armed create tool (and clears modify_tool_)
  modify_tool_ = kind;
  const char* verb = kind == SketchModifyKind::Trim     ? "Trim"
                     : kind == SketchModifyKind::Extend ? "Extend"
                                                        : "Split";
  if (kind == SketchModifyKind::Trim) {
    report_message("Modify: click a curve or circle, or hold the left mouse button to trim "
                   "(Esc finishes)");
  } else {
    report_message(std::string("Modify: click a curve to ") + verb + " (Esc finishes)");
  }
  // A modify press owns the pointer gesture. In particular, pressing empty
  // space while starting a Trim stroke must not start box selection beneath it.
  viewport_.set_sketch_selection_enabled(false);
  refresh_enablement();
}

void ShellSketchTools::handle_modify_press(SketchModifyKind kind,
                                           const std::optional<GuiSketchHit>& hit) {
  auto topology = active_topology();
  if (topology.has_error()) {
    report(topology.error());
    return;
  }
  const auto target = resolve_modify_hit(topology.value(), kind, hit);
  if (!target) {
    report_message(kind == SketchModifyKind::Trim ? "Modify: click a curve or circle"
                                                  : "Modify: click a curve");
    return;
  }
  const auto applied = target->entity
                           ? apply_modify_pick(kind, target->entity->value(), target->pick)
                           : apply_modify_profile_pick(kind, *target->profile, target->pick);
  if (applied.has_error())
    report(applied.error()); // tool stays armed for the next curve
  window_.refresh();
}

void ShellSketchTools::begin_trim_stroke(GuiSketchScreenPoint screen) {
  cancel_trim_stroke();
  trim_stroke_active_ = true;
  trim_stroke_last_screen_ = screen;

  // hits_at() deliberately prioritizes snap points for ordinary selection.
  // Trim needs the curve below such a midpoint/endpoint/quadrant instead, so
  // use the curve-only sweep API even for a zero-length click.
  auto curve_hits = viewport_.sketch_curve_hits_along(screen, screen);
  if (curve_hits.has_error()) {
    report(curve_hits.error());
    return;
  }
  if (!curve_hits.value().empty()) {
    handle_trim_stroke_hit(curve_hits.value().front(), true);
  }
}

void ShellSketchTools::continue_trim_stroke(GuiSketchScreenPoint screen) {
  if (!trim_stroke_active_ || modify_tool_ != SketchModifyKind::Trim)
    return;
  // update_sketch_pointer() also runs immediately before the Release phase.
  // Do not let that duplicate coordinate act on geometry exposed by the
  // previous trim's synchronous scene rebuild.
  if (trim_stroke_last_screen_ && *trim_stroke_last_screen_ == screen)
    return;
  const GuiSketchScreenPoint previous = *trim_stroke_last_screen_;
  trim_stroke_last_screen_ = screen;

  // Qt may coalesce mouse-move events. Test the whole travelled segment so a
  // quick stroke cannot jump over a thin curve between two delivered samples.
  auto hits = viewport_.sketch_curve_hits_along(previous, screen);
  if (hits.has_error()) {
    report(hits.error());
    return;
  }
  queue_trim_stroke_hits(std::move(hits.value()));
}

void ShellSketchTools::queue_trim_stroke_hits(std::vector<GuiSketchHit> hits) {
  if (hits.empty())
    return;

  // A trim commit republishes the interaction scene and resets the viewport's
  // current snap result. Running that synchronously from update_sketch_pointer
  // would suppress the subsequent Release phase. Defer the copied hit until
  // this pointer event has completed; generation ownership makes stale queued
  // samples harmless after cancellation or a new stroke.
  const std::size_t generation = trim_stroke_generation_;
  ++trim_stroke_pending_;
  QTimer::singleShot(0, this, [this, generation, hits = std::move(hits)] {
    if (generation != trim_stroke_generation_)
      return;
    for (const auto& hit : hits)
      handle_trim_stroke_hit(hit, false);
    if (trim_stroke_pending_ > 0U)
      --trim_stroke_pending_;
    if (!trim_stroke_active_ && trim_stroke_pending_ == 0U)
      trim_stroke_visited_.clear();
  });
}

void ShellSketchTools::handle_trim_stroke_hit(const std::optional<GuiSketchHit>& hit,
                                              bool report_miss) {
  auto topology = active_topology();
  if (topology.has_error()) {
    report(topology.error());
    return;
  }
  const auto target = resolve_modify_hit(topology.value(), SketchModifyKind::Trim, hit);
  if (!target) {
    if (report_miss)
      report_message("Modify: click a curve or circle");
    return;
  }
  // Insert before executing. A rejected trim must not emit the same error on
  // every mouse move while the pointer is still over that target.
  if (!trim_stroke_visited_.insert(target->stroke_key).second)
    return;

  const auto applied = target->entity
                           ? apply_modify_pick(SketchModifyKind::Trim, target->entity->value(),
                                               target->pick)
                           : apply_modify_profile_pick(SketchModifyKind::Trim, *target->profile,
                                                       target->pick);
  if (applied.has_error())
    report(applied.error());
  window_.refresh();
}

void ShellSketchTools::end_trim_stroke() noexcept {
  trim_stroke_active_ = false;
  trim_stroke_last_screen_.reset();
  if (trim_stroke_pending_ == 0U)
    trim_stroke_visited_.clear();
}

void ShellSketchTools::cancel_trim_stroke() noexcept {
  // Invalidate every deferred sample owned by the previous gesture.
  ++trim_stroke_generation_;
  trim_stroke_pending_ = 0U;
  trim_stroke_active_ = false;
  trim_stroke_last_screen_.reset();
  trim_stroke_visited_.clear();
}

Result<std::size_t> ShellSketchTools::delete_selection() {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation("gui.shell.sketch", "no active sketch"));
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
        "gui.shell.sketch", "nothing deletable selected (constraint, dimension, or geometry)"));

  // One transaction: drop the selected intents, then every intent referencing
  // a doomed entity (or its points), then the entities plus orphaned points.
  auto removed = session_.commit_part_sketch_intent_transaction(
      "Delete selection",
      [sketch, constraint_ids, dimension_ids,
       entity_ids](PartDocument& part, std::vector<SketchConstraintCatalog>& constraint_catalogs,
                   std::vector<SketchDimensionCatalog>& dimension_catalogs) -> Result<std::size_t> {
        const Sketch* source = part.find_sketch(sketch);
        if (source == nullptr)
          return Result<std::size_t>::failure(
              Error::validation(sketch.value(), "active sketch no longer exists"));
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
            if (std::find(entity_ids.begin(), entity_ids.end(), dependency) != entity_ids.end()) {
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
                Error::validation(entity_id, "sketch geometry does not exist"));
          doomed_points.insert(doomed_points.end(), target->points().begin(),
                               target->points().end());
        }
        const auto references_doomed = [&](const auto& intent) {
          for (const auto& intent_target : intent.targets()) {
            if (std::find(doomed_entities.begin(), doomed_entities.end(), intent_target.id()) !=
                doomed_entities.end())
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
  const auto id = selected_dimension();
  if (!id || !window_.active_sketch()) return false;
  auto catalog = session_.sketch_dimension_catalog(*window_.active_sketch());
  const auto* dimension = catalog ? catalog.value().find(*id) : nullptr;
  if (!dimension || !dimension->driving() || !dimension->parameter()) return false;
  const auto* parameter = session_.part_document()->find_parameter(*dimension->parameter());
  if (!parameter) return false;
  auto annotations = GuiSketchDimensionController::accepted_annotations(session_, *window_.active_sketch());
  if (!annotations) return false;
  for (const auto& annotation : annotations.value()) {
    if (annotation.candidate_id != id->value()) continue;
    auto screen = viewport_.sketch_plane_to_screen(annotation.point);
    if (!screen) return false;
    cancel_label_drag();
    dimension_editor_id_ = *id;
    const bool circle_radius =
        dimension->kind() == SketchDimensionKind::Radius &&
        dimension_circle_profile(*session_.part_document(), *dimension, *window_.active_sketch());
    const auto measured_value =
        SketchDimensionSolverAdapter::parameter_value(*session_.part_document(), *dimension);
    if (!measured_value)
      return false;
    dimension_editor_->setText(
        parameter->is_expression()
            ? QString::fromStdString(circle_radius ? "(" + *parameter->formula() + ") / 2"
                                                   : *parameter->formula())
            : QString::number(measured_value.value(), 'g', 12));
    dimension_editor_->setToolTip({});
    const int width = std::min(220, viewport_.width());
    dimension_editor_->setFixedWidth(width);
    dimension_editor_->move(std::clamp(static_cast<int>(screen.value().x) - width / 2, 0,
                                      std::max(0, viewport_.width() - width)),
                            std::clamp(static_cast<int>(screen.value().y) - 15, 0,
                                      std::max(0, viewport_.height() - 30)));
    dimension_editor_->show();
    dimension_editor_->raise();
    dimension_editor_->setFocus();
    dimension_editor_->selectAll();
    return true;
  }
  return false;
}

Result<std::size_t> ShellSketchTools::apply_dimension(SketchDimensionKind kind, bool reference,
                                                      std::optional<ParameterId> parameter) {
  auto topology = active_topology();
  if (topology.has_error())
    return Result<std::size_t>::failure(topology.error());
  auto targets = selected_targets(topology.value(), true);
  if (targets.has_error())
    return Result<std::size_t>::failure(targets.error());
  return commit_dimension(kind, reference, parameter, std::move(targets.value()));
}

Result<std::size_t> ShellSketchTools::commit_dimension(
    SketchDimensionKind kind, bool reference, std::optional<ParameterId> parameter,
    std::vector<SketchConstraintIntentTarget> selected, std::optional<Point2> label_position) {
  auto topology = active_topology();
  if (!topology)
    return Result<std::size_t>::failure(topology.error());
  auto targets = Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(selected));
  const auto compatible =
      GuiSketchDimensionController::compatible_kinds(topology.value(), targets.value());
  if (std::find(compatible.begin(), compatible.end(), kind) == compatible.end())
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "selection is incompatible with this dimension"));
  canonicalize_order(topology.value(), kind, targets.value());
  auto catalog = session_.sketch_dimension_catalog(topology.value().sketch());
  if (catalog.has_error())
    return Result<std::size_t>::failure(catalog.error());
  std::optional<Parameter> pending_parameter;
  if (!reference && !parameter) {
    auto probe = SketchDimensionIntent::create(next_dimension_id(catalog.value(), kind), kind,
                                               SketchDimensionMode::Reference, targets.value(),
                                               std::nullopt);
    if (probe)
      if (const auto* circle = dimension_circle_profile(*session_.part_document(), probe.value(),
                                                        topology.value().sketch()))
        parameter = circle->diameter_parameter();
  }
  if (!reference && !parameter) {
    // Inventor-style: auto-create a dN parameter carrying the CURRENT measured
    // value, so committing the driving dimension does not move geometry.
    std::vector<SketchConstraintIntentTarget> measure_targets = targets.value();
    auto measure_intent = SketchDimensionIntent::create(next_dimension_id(catalog.value(), kind),
                                                        kind, SketchDimensionMode::Reference,
                                                        std::move(measure_targets), std::nullopt);
    if (measure_intent.has_error())
      return Result<std::size_t>::failure(measure_intent.error());
    auto measured = SketchDimensionMeasurementEvaluator::measure(
        topology.value(), measure_intent.value(), session_.part_document());
    if (measured.has_error())
      return Result<std::size_t>::failure(measured.error());
    std::size_t number = 1;
    while (session_.part_document()->find_parameter(ParameterId("d" + std::to_string(number))) !=
           nullptr)
      ++number;
    const ParameterId parameter_id("d" + std::to_string(number));
    const Quantity value = measured.value().value;
    auto created = kind == SketchDimensionKind::Angle
        ? Parameter::create_angle(parameter_id, parameter_id.value(), value)
        : Parameter::create_length(parameter_id, parameter_id.value(), value);
    if (created.has_error()) return Result<std::size_t>::failure(created.error());
    pending_parameter = std::move(created.value());
    parameter = parameter_id;
  }
  auto intent = SketchDimensionIntent::create(next_dimension_id(catalog.value(), kind), kind,
                                              reference ? SketchDimensionMode::Reference
                                                        : SketchDimensionMode::Driving,
                                              std::move(targets.value()), parameter);
  if (intent.has_error())
    return Result<std::size_t>::failure(intent.error());
  if (label_position) {
    auto measurement = SketchDimensionMeasurementEvaluator::measure(
        topology.value(), intent.value(), session_.part_document());
    if (!measurement)
      return Result<std::size_t>::failure(measurement.error());
    auto glyph = geometry::SketchDimensionGlyphLayoutResolver{}.resolve(
        topology.value(), intent.value(), measurement.value());
    if (!glyph)
      return Result<std::size_t>::failure(glyph.error());
    auto placed = intent.value().with_label_offset(
        {label_position->x - glyph.value().anchor.x, label_position->y - glyph.value().anchor.y});
    if (!placed)
      return Result<std::size_t>::failure(placed.error());
    intent = std::move(placed);
  }
  auto committed = GuiSketchDimensionController::add(
      session_, topology.value().sketch(), std::move(intent.value()), std::move(pending_parameter));
  if (committed.has_error())
    return committed;
  session_.selection().clear();
  publish_scene();
  arm_drag_controller();
  return committed;
}

bool ShellSketchTools::begin_rapid_dimension() {
  if (!sketch_context_ready() || session_.task().active())
    return false;
  cancel_tool();
  if (!workspace_.begin_command(session_, "sketch.dimension.rapid",
                                "Pick geometry, then click to place a dimension"))
    return false;
  rapid_dimension_active_ = true;
  rapid_dimension_action_->setChecked(true);
  rapid_alternate_radial_ = false;
  rapid_targets_.clear();
  rapid_candidate_.reset();
  viewport_.set_sketch_selection_enabled(false);
  viewport_.set_sketch_drag_handles({});
  window_.statusBar()->showMessage(
      QStringLiteral("Rapid Dimension: hover a curve or pick two points; click empty space to "
                     "place. D switches radius/diameter."));
  return true;
}

std::optional<SketchConstraintIntentTarget>
ShellSketchTools::rapid_target(const SketchTopology& topology,
                               const std::optional<GuiSketchHit>& hit, Point2 pointer) const {
  if (!hit || hit->kind == GuiSketchHitKind::Dimension || hit->kind == GuiSketchHitKind::Glyph)
    return std::nullopt;
  const GuiSelection selected{GuiSelectionKind::SketchEntity, hit->semantic_id};
  auto target = selection_target(topology, selected.semantic_id);
  if (!target)
    return std::nullopt;
  if (target->kind() == SketchConstraintIntentTargetKind::Entity) {
    const auto* entity = topology.find_entity(target->id());
    if (entity) {
      // A large endpoint tolerance can cover an entire small arc or circle.
      // Preserve precise point picks, but prefer the curve actually under the
      // pointer when it is appreciably closer than a nearby fit point.
      if (hit->kind == GuiSketchHitKind::Point && hit->screen_distance_dip > 3.0) {
        auto screen = viewport_.sketch_plane_to_screen(pointer);
        if (screen) {
          auto curves = viewport_.sketch_curve_hits_along(screen.value(), screen.value());
          if (curves)
            for (const auto& curve : curves.value()) {
              const GuiSelection curve_selection{GuiSelectionKind::SketchEntity, curve.semantic_id};
              if (curve_selection.semantic_id == selected.semantic_id &&
                  curve.screen_distance_dip <= 3.0)
                return target;
            }
        }
      }
      if (const auto point = resolve_point_role(topology, *entity, hit->candidate_id))
        return SketchConstraintIntentTarget::point(*point).value();
      if (entity->kind() == SketchTopologyEntityKind::Point)
        return SketchConstraintIntentTarget::point(entity->points()[0]).value();
    }
  }
  return target;
}

void ShellSketchTools::update_rapid_dimension(Point2 pointer,
                                              const std::optional<GuiSketchHit>& hit) {
  auto topology = active_topology();
  if (!topology)
    return;
  auto targets = rapid_targets_;
  const auto hovered = rapid_target(topology.value(), hit, pointer);
  if (hovered && std::find(targets.begin(), targets.end(), *hovered) == targets.end() &&
      targets.size() < 2U)
    targets.push_back(*hovered);
  auto inferred =
      GuiRapidDimensionInference::infer(*session_.part_document(), topology.value(),
                                        std::move(targets), pointer, rapid_alternate_radial_);
  rapid_candidate_ = inferred ? std::move(inferred.value()) : std::nullopt;
  if (rapid_candidate_) {
    const auto& glyph = rapid_candidate_->glyph;
    viewport_.set_sketch_dimension_preview(GuiSketchAnnotationPrimitive{
        glyph.semantic_id, glyph.candidate_id, glyph.anchor, GuiSketchHitKind::Dimension,
        GuiSketchDimensionController::annotation_label(glyph), glyph.leader});
    window_.statusBar()->showMessage(
        QStringLiteral(
            "Rapid Dimension: %1 — %2. Click empty space to place; D switches radius/diameter.")
            .arg(QString::fromUtf8(to_string(rapid_candidate_->intent.kind()).data()),
                 QString::fromStdString(glyph.value_text)));
  } else {
    viewport_.set_sketch_dimension_preview(std::nullopt);
    window_.statusBar()->showMessage(QStringLiteral(
        "Rapid Dimension: pick a curve or two points, then click empty space to place."));
  }
}

void ShellSketchTools::rapid_dimension_press(Point2 pointer,
                                             const std::optional<GuiSketchHit>& hit) {
  auto topology = active_topology();
  if (!topology)
    return;
  const auto target = rapid_target(topology.value(), hit, pointer);
  if (target) {
    if (rapid_targets_.size() >= 2U)
      rapid_targets_.clear();
    if (std::find(rapid_targets_.begin(), rapid_targets_.end(), *target) == rapid_targets_.end())
      rapid_targets_.push_back(*target);
    update_rapid_dimension(pointer, std::nullopt);
    return;
  }
  if (!rapid_candidate_ || rapid_targets_.empty())
    return;
  const auto candidate = *rapid_candidate_;
  const bool reference = reference_dimension_action_ && reference_dimension_action_->isChecked();
  auto committed = commit_dimension(candidate.intent.kind(), reference, std::nullopt,
                                    candidate.intent.targets(), pointer);
  if (!committed) {
    report(committed.error());
    return;
  }
  rapid_targets_.clear();
  rapid_candidate_.reset();
  viewport_.set_sketch_dimension_preview(std::nullopt);
  viewport_.set_sketch_selection_enabled(false);
  viewport_.set_sketch_drag_handles({});
  window_.refresh();
}

Result<std::size_t> ShellSketchTools::edit_selected_dimension(const std::string& text) {
  const auto id = selected_dimension();
  if (!id || session_.part_document() == nullptr)
    return Result<std::size_t>::failure(
        Error::validation("gui.shell.sketch", "select exactly one dimension annotation"));
  return edit_dimension(*id, text);
}

Result<std::size_t> ShellSketchTools::edit_dimension(SketchDimensionId id, const std::string& text) {
  if (!sketch_context_ready())
    return Result<std::size_t>::failure(Error::validation(id.value(), "no active sketch"));
  auto catalog = session_.sketch_dimension_catalog(*window_.active_sketch());
  if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
  const auto* dimension = catalog.value().find(id);
  if (dimension == nullptr || !dimension->driving() || !dimension->parameter())
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "selected dimension has no driving value"));
  const Parameter* parameter = session_.part_document()->find_parameter(*dimension->parameter());
  if (parameter == nullptr)
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "dimension parameter does not exist"));
  Result<std::size_t> edited = [&] {
    const QString input = QString::fromStdString(text).trimmed();
    if (parameter->is_expression() || input.startsWith('=')) {
      const std::string formula = (input.startsWith('=') ? input.mid(1) : input).toStdString();
      if (parameter->is_expression())
        return GuiSketchDimensionController::edit_formula(session_, *window_.active_sketch(), id,
                                                          formula);
      std::size_t number = 1U;
      std::string name;
      do { name = "d_expr_" + std::to_string(number++); }
      while (session_.part_document()->find_parameter(ParameterId(name)) != nullptr ||
             std::any_of(session_.part_document()->parameters().begin(),
                         session_.part_document()->parameters().end(),
                         [&name](const auto& item) { return item.name() == name; }));
      return GuiSketchDimensionController::edit_formula(session_, *window_.active_sketch(), id,
                                                        formula, ParameterId(name), name);
    }
    QString literal = input;
    const QString unit = dimension->kind() == SketchDimensionKind::Angle
                             ? QStringLiteral("deg") : QStringLiteral("mm");
    if (literal.endsWith(unit)) literal.chop(unit.size());
    bool valid = false;
    const double value = literal.trimmed().toDouble(&valid);
    if (!valid || !std::isfinite(value) || value <= 0.0)
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "dimension value must be positive and finite"));
    auto quantity = dimension->kind() == SketchDimensionKind::Angle
                        ? Quantity::angle_deg(value, id.value())
                        : Quantity::length_mm(value, id.value());
    if (quantity.has_error())
      return Result<std::size_t>::failure(quantity.error());
    return GuiSketchDimensionController::edit_literal(session_, *window_.active_sketch(), id,
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
    if (!drag_handle_visible(handle))
      continue;
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
    report_message("Sketch handle could not start the drag lifecycle");
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
    cancel_drag(true, "Sketch drag could not start release preview");
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
    report_message("Drag committed, but workspace lifecycle did not close cleanly");
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

bool ShellSketchTools::drag_handle_visible(const GuiSketchDragHandle& handle) const {
  if (!window_.active_sketch())
    return false;
  const std::string prefix = "sketch/" + window_.active_sketch()->value();
  if (!session_.presentation_visible(prefix))
    return false;
  if (handle.dimension_id &&
      !session_.presentation_visible(prefix + "/dimension/" + handle.dimension_id->value()))
    return false;
  if (!handle.entity_id.empty() && !session_.presentation_visible(prefix + "/" + handle.entity_id))
    return false;
  if (handle.point_id) {
    if (handle.entity_id.empty() && drag_controller_) {
      // Endpoint handles are deduplicated across joined entities. Keep the
      // shared point draggable if at least one visible owner still uses it.
      bool owned = false;
      for (const auto& entity : drag_controller_->source_topology().entities()) {
        if (std::find(entity.points().begin(), entity.points().end(), *handle.point_id) ==
            entity.points().end())
          continue;
        owned = true;
        if (session_.presentation_visible(prefix + "/" + entity.id()))
          return true;
      }
      if (owned)
        return false;
    }
    if (!session_.presentation_visible(prefix + "/" + handle.point_id->value()))
      return false;
  }
  return true;
}

void ShellSketchTools::publish_handles(const std::vector<GuiSketchDragHandle>& handles) {
  std::vector<Point2> positions;
  positions.reserve(handles.size());
  for (const auto& handle : handles)
    if (drag_handle_visible(handle))
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
  if (rapid_dimension_active_)
    update_rapid_dimension(raw_point, hit);
  else
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
  if (operation_selection_)
    operation_selection_();
  refresh_enablement();
}

bool ShellSketchTools::eventFilter(QObject* watched, QEvent* event) {
  if (rapid_dimension_active_ && (watched == &viewport_ || watched == &window_) &&
      event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    if (!rapid_targets_.empty()) {
      rapid_targets_.clear();
      rapid_candidate_.reset();
      viewport_.set_sketch_dimension_preview(std::nullopt);
    } else
      cancel_tool();
    return true;
  }
  if (watched == dimension_editor_) {
    if (event->type() == QEvent::FocusOut ||
        (event->type() == QEvent::KeyPress &&
         static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)) {
      dimension_editor_->hide();
      dimension_editor_id_.reset();
      return event->type() == QEvent::KeyPress;
    }
    return QObject::eventFilter(watched, event);
  }
  if (watched == &viewport_ && event->type() == QEvent::MouseButtonPress &&
      static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
    suppress_tool_right_release_ = tool_active();
  if (watched == &viewport_ && event->type() == QEvent::MouseButtonRelease &&
      static_cast<QMouseEvent*>(event)->button() == Qt::RightButton &&
      suppress_tool_right_release_) {
    // The drawing tool owns the whole gesture even when its press finishes the tool.
    suppress_tool_right_release_ = false;
    return true;
  }
  if (close_operation_ && (watched == &viewport_ || watched == &window_) &&
      event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    close_operation_();
    return true;
  }
  if (label_drag_ && ((event->type() == QEvent::KeyPress &&
                      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) ||
                     event->type() == QEvent::UngrabMouse ||
                     event->type() == QEvent::WindowDeactivate)) {
    cancel_label_drag();
    return event->type() == QEvent::KeyPress;
  }
  if (watched == &viewport_ && tool_active()) {
    if (event->type() == QEvent::MouseButtonPress) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::RightButton) {
        // A spline collects any number of fit points and commits on right-click.
        (void)finish_tool();
        return true;
      }
      if (mouse->button() == Qt::LeftButton) {
        // Deferred so the pick uses the snap result computed by the viewport's
        // own press handling (the filter runs first) — sequencing, not polling.
        QTimer::singleShot(0, this, [this] {
          if (!tool_active())
            return;
          const auto& snap = viewport_.sketch_snap_result();
          if (snap) {
            const auto pick = create_pointer_pick(*create_controller_, viewport_, *snap);
            (void)add_pick(pick.point, pick.snap);
          }
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
  } else if ((watched == &viewport_ || watched == &window_) &&
             (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate)) {
    // Losing capture ends only the current paint gesture. The modal Trim tool
    // remains armed, just as after an ordinary left-button release.
    if (operation_phase_)
      operation_phase_(GuiSketchPointerPhase::Release, {}, std::nullopt);
    if (trim_stroke_active_ || trim_stroke_pending_ > 0U)
      cancel_trim_stroke();
    if (drag_controller_ && drag_controller_->active() && drag_stage(workspace_.stage()))
      cancel_drag(true, "Sketch drag cancelled: pointer capture lost");
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
        if ((create_controller_->tool() == GuiSketchCreateTool::Polyline ||
             create_controller_->tool() == GuiSketchCreateTool::Spline) &&
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
      cancel_tool();
      report_message("Modify finished");
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
    message += QStringLiteral("   |   Snap: %1").arg(QString::fromStdString(status.snap_inference));
  if (status.remaining_dof)
    message += QStringLiteral("   |   DOF: %1").arg(*status.remaining_dof);
  if (!status.solve_status.empty())
    message += QStringLiteral("   |   Solver: %1").arg(QString::fromStdString(status.solve_status));
  if (!numeric_buffer_.empty())
    message += QStringLiteral("   |   Input: %1").arg(QString::fromStdString(numeric_buffer_));
  window_.statusBar()->showMessage(message);
}

} // namespace blcad::gui
