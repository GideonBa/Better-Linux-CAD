#include "blcad/gui/gui_sketch_workbench.hpp"

#include <utility>

namespace blcad::gui {
namespace {

Result<std::size_t> part_only_error(std::string_view operation) {
  return Result<std::size_t>::failure(
      Error::validation("gui.sketch_workbench", std::string(operation) + " requires an active Part document"));
}

Error workspace_error(std::string message) {
  return Error::validation("gui.sketch_workspace", std::move(message));
}

const PartDocument* active_part(const GuiDocumentSession& session) { return session.part_document(); }

template <typename Object, typename Add>
Result<std::size_t> create_part_object(GuiDocumentSession& session, std::string label,
                                      Object object, Add add) {
  if (!session.part_document())
    return part_only_error(label);
  return session.commit_part_transaction(std::move(label),
      [object = std::move(object), add](PartDocument& part) mutable {
        return (part.*add)(std::move(object));
      });
}

const Sketch* find_sketch(const GuiDocumentSession& session, SketchId id) {
  const auto* part = active_part(session);
  return part ? part->find_sketch(std::move(id)) : nullptr;
}

double dot(Vector3 a, Vector3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 displacement(Point3 from, Point3 to) noexcept {
  return {to.x - from.x, to.y - from.y, to.z - from.z};
}

} // namespace

Point2 GuiSketchPlaneView::model_to_plane(Point3 point) const noexcept {
  const Vector3 local = displacement(origin, point);
  return {dot(local, x_axis), dot(local, y_axis)};
}

Point3 GuiSketchPlaneView::plane_to_model(Point2 point) const noexcept {
  return {origin.x + point.x * x_axis.x + point.y * y_axis.x,
          origin.y + point.x * x_axis.y + point.y * y_axis.y,
          origin.z + point.x * x_axis.z + point.y * y_axis.z};
}

GuiSketchSelectionPrompt GuiSketchWorkbench::prompt_for(GuiSketchCommand command) {
  switch (command) {
  case GuiSketchCommand::CreateDerivedWorkplane:
    return {"Select a planar generated face", GuiSelectionKind::Face, "planar_face"};
  case GuiSketchCommand::CreateSketch:
    return {"Select a datum plane, construction plane, or derived workplane", GuiSelectionKind::Datum,
            "planar_workplane"};
  case GuiSketchCommand::ProjectReference:
    return {"Select a construction point/line or stable semantic vertex/edge", GuiSelectionKind::Edge,
            "projectable_reference"};
  case GuiSketchCommand::TrimExtend:
    return {"Select an editable planar sketch curve", GuiSelectionKind::SketchEntity,
            "editable_planar_curve"};
  case GuiSketchCommand::AddConstraint:
    return {"Select sketch entities compatible with the chosen constraint", GuiSelectionKind::SketchEntity,
            "constraint_compatible"};
  case GuiSketchCommand::AddDimension:
    return {"Select two compatible sketch points or endpoints", GuiSelectionKind::SketchEntity,
            "dimensionable_points"};
  case GuiSketchCommand::Repair:
  case GuiSketchCommand::Diagnose:
  case GuiSketchCommand::InspectProfiles:
    return {"Select one planar sketch", GuiSelectionKind::SketchEntity, "planar_sketch"};
  case GuiSketchCommand::CreateDatumPlane:
  case GuiSketchCommand::CreateDatumAxis:
  case GuiSketchCommand::AddConstructionGeometry:
  case GuiSketchCommand::AddLine:
  case GuiSketchCommand::AddArcCircle:
  case GuiSketchCommand::AddSpline:
    return {"Enter deterministic numeric intent in sketch-plane coordinates",
            GuiSelectionKind::SketchEntity, "active_sketch"};
  }
  return {"Select compatible geometry", GuiSelectionKind::SketchEntity, "compatible_geometry"};
}

Result<std::size_t> GuiSketchWorkbench::create_xy_datum(GuiDocumentSession& session, DatumPlaneId id,
                                                        std::string name) const {
  auto datum = DatumPlane::xy(std::move(id), std::move(name));
  if (datum.has_error()) return Result<std::size_t>::failure(datum.error());
  return create_part_object(session, "Create datum plane", std::move(datum.value()),
                            &PartDocument::add_datum_plane);
}

Result<std::size_t> GuiSketchWorkbench::create_datum_axis(GuiDocumentSession& session,
                                                          DatumAxis axis) const {
  return create_part_object(session, "Create datum axis", std::move(axis), &PartDocument::add_datum_axis);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_point(GuiDocumentSession& session,
                                                                  ConstructionPoint point) const {
  return create_part_object(session, "Create construction point", std::move(point), &PartDocument::add_construction_point);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_line(GuiDocumentSession& session,
                                                                 ConstructionLine line) const {
  return create_part_object(session, "Create construction line", std::move(line), &PartDocument::add_construction_line);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_plane(GuiDocumentSession& session,
                                                                  ConstructionPlane plane) const {
  return create_part_object(session, "Create construction plane", std::move(plane), &PartDocument::add_construction_plane);
}
Result<std::size_t> GuiSketchWorkbench::create_derived_workplane(GuiDocumentSession& session,
                                                                 DerivedWorkplane workplane) const {
  return create_part_object(session, "Create derived workplane", std::move(workplane), &PartDocument::add_derived_workplane);
}
Result<std::size_t> GuiSketchWorkbench::create_sketch(GuiDocumentSession& session, Sketch sketch) const {
  return create_part_object(session, "Create sketch", std::move(sketch), &PartDocument::add_sketch);
}

Result<std::size_t> GuiSketchWorkbench::edit_sketch(GuiDocumentSession& session, SketchId sketch,
                                                    std::string label,
                                                    const GuiSketchMutation& mutation) const {
  if (!session.part_document()) return part_only_error(label);
  return session.commit_part_transaction(std::move(label), [sketch, &mutation](PartDocument& part) {
    const auto* existing = part.find_sketch(sketch);
    if (!existing)
      return Result<std::size_t>::failure(Error::validation(sketch.value(), "sketch must exist"));
    Sketch candidate = *existing;
    auto edited = mutation(candidate);
    if (edited.has_error()) return Result<std::size_t>::failure(edited.error());
    return part.update_sketch(std::move(candidate));
  });
}

#define BLCAD_SKETCH_ADD_METHOD(NAME, TYPE, METHOD, LABEL)                                      \
  Result<std::size_t> GuiSketchWorkbench::NAME(GuiDocumentSession& session, SketchId sketch,     \
                                                TYPE object) const {                              \
    return edit_sketch(session, std::move(sketch), LABEL,                                        \
                       [object = std::move(object)](Sketch& candidate) mutable {                   \
                         return candidate.METHOD(std::move(object));                              \
                       });                                                                        \
  }

BLCAD_SKETCH_ADD_METHOD(add_line, LineSegment, add_entity, "Add sketch line")
BLCAD_SKETCH_ADD_METHOD(add_arc, ArcSegment, add_entity, "Add sketch arc")
BLCAD_SKETCH_ADD_METHOD(add_spline, SplineSegment, add_entity, "Add sketch spline")
BLCAD_SKETCH_ADD_METHOD(add_trim_extend, SketchTrimExtendOperation, add_trim_extend_operation,
                        "Trim or extend sketch curve")
BLCAD_SKETCH_ADD_METHOD(add_projected_point, ProjectedSketchPoint, add_reference,
                        "Project sketch point reference")
BLCAD_SKETCH_ADD_METHOD(add_projected_line, ProjectedSketchLine, add_reference,
                        "Project sketch line reference")
BLCAD_SKETCH_ADD_METHOD(add_reference_line, ReferenceGeneratedLine, add_reference,
                        "Create reference-driven sketch line")
BLCAD_SKETCH_ADD_METHOD(add_constraint, SketchConstraint, add_constraint, "Add sketch reference constraint")
BLCAD_SKETCH_ADD_METHOD(add_constraint, SketchGeometricConstraint, add_constraint, "Add sketch geometric constraint")
BLCAD_SKETCH_ADD_METHOD(add_dimension, SketchDrivingDimension, add_dimension, "Add sketch dimension")
BLCAD_SKETCH_ADD_METHOD(add_tangent_continuity, SketchTangentContinuity, add_tangent_continuity,
                        "Add sketch tangent continuity")
BLCAD_SKETCH_ADD_METHOD(add_profile, ClosedProfile, add_profile, "Add closed sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, ArcClosedProfile, add_profile, "Add curved sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CompositeClosedProfile, add_profile, "Add composite sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CircleProfile, add_profile, "Add circle sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, RectangleProfile, add_profile, "Add rectangle sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CircularHolePattern, add_profile, "Add circular hole pattern")

#undef BLCAD_SKETCH_ADD_METHOD

Result<GuiSketchPlaneView> GuiSketchWorkbench::plane_view(const GuiDocumentSession& session,
                                                          SketchId sketch) const {
  const auto* part = active_part(session);
  const auto* target = part ? part->find_sketch(std::move(sketch)) : nullptr;
  if (!target)
    return Result<GuiSketchPlaneView>::failure(Error::validation("gui.sketch_workbench", "sketch must exist"));
  geometry::WorkplaneResolver resolver;
  auto plane = session.part_shape_cache()
                   ? resolver.resolve_for_sketch(*part, *target, *session.part_shape_cache())
                   : resolver.resolve_for_sketch(*part, *target);
  if (plane.has_error()) return Result<GuiSketchPlaneView>::failure(plane.error());
  return Result<GuiSketchPlaneView>::success({plane.value().id, plane.value().origin,
                                              plane.value().x_axis, plane.value().y_axis,
                                              plane.value().normal});
}

Result<GuiSketchInspection> GuiSketchWorkbench::inspect(const GuiDocumentSession& session,
                                                        SketchId sketch) const {
  const auto* target = find_sketch(session, sketch);
  if (!target)
    return Result<GuiSketchInspection>::failure(Error::validation(sketch.value(), "sketch must exist"));
  SketchConstraintDiagnostics analyzer;
  auto diagnostics = analyzer.analyze(*target);
  SketchRepairSuggester suggester;
  auto repairs = suggester.suggest(diagnostics);
  std::optional<ProfileId> detected_region;
  std::string region_message = "no explicit closed region requested";
  if (target->line_segments().size() >= 3U) {
    auto region = geometry::SketchRegionFinder{}.find_single_region(*active_part(session), *target);
    if (region.has_error())
      region_message = region.error().message();
    else {
      detected_region = region.value().id;
      region_message = "one deterministic closed region detected";
    }
  }
  GuiSketchInspection result{target->id(),
      target->line_segments().size() + target->arc_segments().size() + target->spline_segments().size(),
      target->projected_points().size() + target->projected_lines().size() + target->reference_generated_lines().size(),
      target->constraints().size() + target->geometric_constraints().size() + target->tangent_continuities().size(),
      target->driving_dimensions().size(), target->profile_count(), std::move(detected_region),
      std::move(region_message), std::move(diagnostics), std::move(repairs)};
  return Result<GuiSketchInspection>::success(std::move(result));
}

Result<SketchRepairCommandResult>
GuiSketchWorkbench::preview_repair(const GuiDocumentSession& session, SketchId sketch,
                                   const SketchRepairSuggestion& suggestion) const {
  const auto* target = find_sketch(session, std::move(sketch));
  if (!target)
    return Result<SketchRepairCommandResult>::failure(Error::validation("gui.sketch_repair", "sketch must exist"));
  Sketch candidate = *target;
  return SketchRepairCommandExecutor{}.apply(candidate, SketchRepairCommand(suggestion));
}

Result<std::size_t> GuiSketchWorkbench::apply_repair(GuiDocumentSession& session, SketchId sketch,
                                                     const SketchRepairSuggestion& suggestion) const {
  const auto preview = preview_repair(session, sketch, suggestion);
  if (preview.has_error()) return Result<std::size_t>::failure(preview.error());
  if (!preview.value().applied())
    return Result<std::size_t>::failure(Error::validation(sketch.value(), preview.value().message()));
  return edit_sketch(session, std::move(sketch), "Repair sketch",
      [suggestion](Sketch& candidate) {
        auto applied = SketchRepairCommandExecutor{}.apply(candidate, SketchRepairCommand(suggestion));
        if (applied.has_error()) return Result<std::size_t>::failure(applied.error());
        return applied.value().applied()
                   ? Result<std::size_t>::success(applied.value().changed_constraint_ids().size() +
                                                  applied.value().changed_dimension_ids().size())
                   : Result<std::size_t>::failure(Error::validation(candidate.id().value(), applied.value().message()));
      });
}

const std::vector<std::string_view>& GuiSketchWorkspace::command_groups() noexcept {
  static const std::vector<std::string_view> groups{
      "Create", "Constrain", "Dimension", "Modify", "Project"};
  return groups;
}

const std::vector<std::string_view>& GuiSketchWorkspace::browser_sections() noexcept {
  static const std::vector<std::string_view> sections{
      "Entities", "Constraints", "Dimensions", "Diagnostics"};
  return sections;
}

Result<GuiSketchPlaneView> GuiSketchWorkspace::enter(GuiDocumentSession& session,
                                                     const GuiSketchWorkbench& workbench,
                                                     SketchId sketch) {
  if (active())
    return Result<GuiSketchPlaneView>::failure(workspace_error("a Sketch workspace is already active"));
  if (session.document_kind() != GuiDocumentKind::Part)
    return Result<GuiSketchPlaneView>::failure(workspace_error("Enter Sketch requires an active Part document"));
  if (session.task().active())
    return Result<GuiSketchPlaneView>::failure(
        workspace_error("active task must be applied or cancelled before Enter Sketch"));
  if (session.part_document()->find_sketch(sketch) == nullptr)
    return Result<GuiSketchPlaneView>::failure(workspace_error("selected sketch must exist in the active Part"));

  auto plane = workbench.plane_view(session, sketch);
  if (plane.has_error())
    return plane;

  previous_workspace_ = session.workspace();
  previous_selection_ = session.selection().entries();
  if (!session.set_workspace(GuiWorkspace::Sketch))
    return Result<GuiSketchPlaneView>::failure(workspace_error("could not activate Sketch workspace"));

  session.selection().clear();
  active_sketch_ = std::move(sketch);
  last_repeatable_command_.clear();
  last_repeatable_hint_.clear();
  reset_interaction();
  status_.tool_hint = "Choose a Sketch command or select sketch geometry";
  return plane;
}

Result<GuiWorkspace> GuiSketchWorkspace::finish(GuiDocumentSession& session,
                                                const GuiSketchWorkbench& workbench) {
  if (!active())
    return Result<GuiWorkspace>::failure(workspace_error("Finish Sketch requires an active Sketch workspace"));
  if (session.task().active() || (stage_ != GuiSketchInteractionStage::Idle &&
                                  stage_ != GuiSketchInteractionStage::Hover))
    return Result<GuiWorkspace>::failure(
        workspace_error("active Sketch command must be committed or cancelled before Finish Sketch"));

  auto inspection = workbench.inspect(session, *active_sketch_);
  if (inspection.has_error())
    return Result<GuiWorkspace>::failure(inspection.error());
  if (inspection.value().diagnostics.has_errors())
    return Result<GuiWorkspace>::failure(
        workspace_error("Sketch diagnostics contain errors; repair them before Finish Sketch"));

  const GuiWorkspace restored_workspace = previous_workspace_;
  const auto restored_selection = previous_selection_;
  if (!session.set_workspace(restored_workspace))
    return Result<GuiWorkspace>::failure(workspace_error("could not restore previous workspace"));

  session.selection().clear();
  for (const auto& selection : restored_selection)
    (void)session.selection().add(selection);
  leave_workspace();
  return Result<GuiWorkspace>::success(restored_workspace);
}

bool GuiSketchWorkspace::set_hover(bool hovered) noexcept {
  if (!active() || (stage_ != GuiSketchInteractionStage::Idle &&
                    stage_ != GuiSketchInteractionStage::Hover))
    return false;
  stage_ = hovered ? GuiSketchInteractionStage::Hover : GuiSketchInteractionStage::Idle;
  status_.stage = stage_;
  status_.tool_hint = hovered ? "Hovering sketch geometry" :
                                "Choose a Sketch command or select sketch geometry";
  return true;
}

bool GuiSketchWorkspace::begin_command(GuiDocumentSession& session, std::string command_id,
                                       std::string tool_hint, bool repeatable) {
  if (!active() || command_id.empty() ||
      (stage_ != GuiSketchInteractionStage::Idle && stage_ != GuiSketchInteractionStage::Hover) ||
      !session.task().begin(command_id))
    return false;
  active_command_ = std::move(command_id);
  active_tool_hint_ = std::move(tool_hint);
  active_command_repeatable_ = repeatable;
  stage_ = GuiSketchInteractionStage::CollectingPicks;
  focus_ = GuiSketchFocusTarget::Canvas;
  status_.stage = stage_;
  status_.focus = focus_;
  status_.tool_hint = active_tool_hint_;
  numeric_input_.clear();
  selected_handle_.clear();
  return true;
}

bool GuiSketchWorkspace::begin_numeric_input(GuiDocumentSession& session) noexcept {
  if (!active() || stage_ != GuiSketchInteractionStage::CollectingPicks ||
      !session.task().begin_parameter_editing())
    return false;
  stage_ = GuiSketchInteractionStage::NumericInput;
  focus_ = GuiSketchFocusTarget::NumericHud;
  status_.stage = stage_;
  status_.focus = focus_;
  status_.tool_hint = "Enter a numeric value; Enter previews and Esc returns to picking";
  return true;
}

bool GuiSketchWorkspace::set_numeric_input(std::string text) {
  if (!active() || stage_ != GuiSketchInteractionStage::NumericInput)
    return false;
  numeric_input_ = std::move(text);
  return true;
}

bool GuiSketchWorkspace::show_preview(GuiDocumentSession& session) noexcept {
  if (!active() || stage_ != GuiSketchInteractionStage::NumericInput ||
      !session.task().show_preview())
    return false;
  stage_ = GuiSketchInteractionStage::Preview;
  status_.stage = stage_;
  status_.tool_hint = "Preview ready — Apply commits; Esc returns to numeric input";
  return true;
}

bool GuiSketchWorkspace::commit_command(GuiDocumentSession& session) noexcept {
  if (!active() || stage_ != GuiSketchInteractionStage::Preview || !session.task().apply())
    return false;
  if (active_command_repeatable_) {
    last_repeatable_command_ = active_command_;
    last_repeatable_hint_ = active_tool_hint_;
  }
  reset_interaction();
  return true;
}

bool GuiSketchWorkspace::repeat_last_command(GuiDocumentSession& session) {
  if (!active() || last_repeatable_command_.empty() ||
      (stage_ != GuiSketchInteractionStage::Idle && stage_ != GuiSketchInteractionStage::Hover))
    return false;
  const std::string command = last_repeatable_command_;
  const std::string hint = last_repeatable_hint_;
  return begin_command(session, command, hint, true);
}

bool GuiSketchWorkspace::escape(GuiDocumentSession& session) noexcept {
  if (!active())
    return false;
  switch (stage_) {
  case GuiSketchInteractionStage::Hover:
    stage_ = GuiSketchInteractionStage::Idle;
    status_.stage = stage_;
    status_.tool_hint = "Choose a Sketch command or select sketch geometry";
    return true;
  case GuiSketchInteractionStage::Preview:
    if (!session.task().return_to_editing())
      return false;
    stage_ = GuiSketchInteractionStage::NumericInput;
    focus_ = GuiSketchFocusTarget::NumericHud;
    status_.stage = stage_;
    status_.focus = focus_;
    status_.tool_hint = "Edit the numeric value or press Esc to return to picking";
    return true;
  case GuiSketchInteractionStage::NumericInput:
    if (!session.task().return_to_selection())
      return false;
    stage_ = GuiSketchInteractionStage::CollectingPicks;
    focus_ = GuiSketchFocusTarget::Canvas;
    numeric_input_.clear();
    status_.stage = stage_;
    status_.focus = focus_;
    status_.tool_hint = active_tool_hint_;
    return true;
  case GuiSketchInteractionStage::CollectingPicks:
  case GuiSketchInteractionStage::SelectedHandle:
  case GuiSketchInteractionStage::DragCandidate:
    if (!session.task().cancel())
      return false;
    reset_interaction();
    return true;
  case GuiSketchInteractionStage::Idle:
    return false;
  }
  return false;
}

bool GuiSketchWorkspace::select_handle(GuiDocumentSession& session, std::string semantic_id) {
  if (!active() || semantic_id.empty() ||
      (stage_ != GuiSketchInteractionStage::Idle && stage_ != GuiSketchInteractionStage::Hover) ||
      !session.task().begin("sketch.drag"))
    return false;
  selected_handle_ = std::move(semantic_id);
  stage_ = GuiSketchInteractionStage::SelectedHandle;
  focus_ = GuiSketchFocusTarget::Canvas;
  status_.stage = stage_;
  status_.focus = focus_;
  status_.tool_hint = "Drag the selected handle or press Esc to cancel";
  return true;
}

bool GuiSketchWorkspace::show_drag_candidate(GuiDocumentSession& session) noexcept {
  if (!active() || stage_ != GuiSketchInteractionStage::SelectedHandle ||
      !session.task().begin_parameter_editing() || !session.task().show_preview())
    return false;
  stage_ = GuiSketchInteractionStage::DragCandidate;
  status_.stage = stage_;
  status_.tool_hint = "Drag candidate preview — release commits or Esc cancels";
  return true;
}

bool GuiSketchWorkspace::commit_drag(GuiDocumentSession& session) noexcept {
  if (!active() || stage_ != GuiSketchInteractionStage::DragCandidate || !session.task().apply())
    return false;
  reset_interaction();
  return true;
}

void GuiSketchWorkspace::set_cursor_coordinates(std::optional<Point2> coordinates) noexcept {
  status_.cursor_coordinates = std::move(coordinates);
}

void GuiSketchWorkspace::set_snap_inference(std::string inference) {
  status_.snap_inference = std::move(inference);
}

void GuiSketchWorkspace::set_solve_feedback(std::optional<std::size_t> remaining_dof,
                                            std::string solve_status) {
  status_.remaining_dof = remaining_dof;
  status_.solve_status = solve_status.empty() ? "Not evaluated" : std::move(solve_status);
}

void GuiSketchWorkspace::focus_canvas() noexcept {
  if (!active())
    return;
  focus_ = GuiSketchFocusTarget::Canvas;
  status_.focus = focus_;
}

void GuiSketchWorkspace::focus_task_panel() noexcept {
  if (!active())
    return;
  focus_ = GuiSketchFocusTarget::TaskPanel;
  status_.focus = focus_;
}

bool GuiSketchWorkspace::active() const noexcept { return active_sketch_.has_value(); }

const std::optional<SketchId>& GuiSketchWorkspace::active_sketch() const noexcept {
  return active_sketch_;
}

GuiSketchInteractionStage GuiSketchWorkspace::stage() const noexcept { return stage_; }

std::string_view GuiSketchWorkspace::active_command() const noexcept { return active_command_; }

std::string_view GuiSketchWorkspace::last_repeatable_command() const noexcept {
  return last_repeatable_command_;
}

std::string_view GuiSketchWorkspace::numeric_input() const noexcept { return numeric_input_; }

std::string_view GuiSketchWorkspace::selected_handle() const noexcept { return selected_handle_; }

const GuiSketchWorkspaceStatus& GuiSketchWorkspace::status() const noexcept { return status_; }

void GuiSketchWorkspace::reset_interaction() noexcept {
  stage_ = GuiSketchInteractionStage::Idle;
  focus_ = GuiSketchFocusTarget::Canvas;
  active_command_.clear();
  active_tool_hint_.clear();
  active_command_repeatable_ = false;
  numeric_input_.clear();
  selected_handle_.clear();
  status_ = {};
  status_.stage = stage_;
  status_.focus = focus_;
  status_.tool_hint = active() ? "Choose a Sketch command or select sketch geometry" :
                                "Select geometry or choose a Sketch command";
}

void GuiSketchWorkspace::leave_workspace() noexcept {
  reset_interaction();
  active_sketch_.reset();
  previous_workspace_ = GuiWorkspace::Part;
  previous_selection_.clear();
  last_repeatable_command_.clear();
  last_repeatable_hint_.clear();
  status_.tool_hint = "Select geometry or choose a Sketch command";
}

} // namespace blcad::gui
