#pragma once

#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_commands.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

enum class GuiSketchCommand {
  CreateDatumPlane,
  CreateDatumAxis,
  CreateDerivedWorkplane,
  CreateSketch,
  AddConstructionGeometry,
  AddLine,
  AddArcCircle,
  AddSpline,
  TrimExtend,
  ProjectReference,
  AddConstraint,
  AddDimension,
  InspectProfiles,
  Diagnose,
  Repair,
};

struct GuiSketchSelectionPrompt {
  std::string text;
  GuiSelectionKind required_kind{GuiSelectionKind::Datum};
  std::string required_capability;
};

struct GuiSketchPlaneView {
  DatumPlaneId workplane;
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  [[nodiscard]] Point2 model_to_plane(Point3 point) const noexcept;
  [[nodiscard]] Point3 plane_to_model(Point2 point) const noexcept;
};

struct GuiSketchInspection {
  SketchId sketch;
  std::size_t explicit_entity_count{0};
  std::size_t projected_reference_count{0};
  std::size_t constraint_count{0};
  std::size_t dimension_count{0};
  std::size_t profile_count{0};
  std::optional<ProfileId> detected_region;
  std::string region_message;
  SketchDiagnosticReport diagnostics{SketchId("sketch.inspection")};
  SketchRepairSuggestionReport repairs{SketchId("sketch.inspection")};
};

using GuiSketchMutation = std::function<Result<std::size_t>(Sketch&)>;

class GuiSketchWorkbench {
public:
  [[nodiscard]] static GuiSketchSelectionPrompt prompt_for(GuiSketchCommand command);

  [[nodiscard]] Result<std::size_t> create_xy_datum(GuiDocumentSession& session, DatumPlaneId id,
                                                     std::string name) const;
  [[nodiscard]] Result<std::size_t> create_datum_axis(GuiDocumentSession& session, DatumAxis axis) const;
  [[nodiscard]] Result<std::size_t> create_construction_point(GuiDocumentSession& session,
                                                               ConstructionPoint point) const;
  [[nodiscard]] Result<std::size_t> create_construction_line(GuiDocumentSession& session,
                                                              ConstructionLine line) const;
  [[nodiscard]] Result<std::size_t> create_construction_plane(GuiDocumentSession& session,
                                                               ConstructionPlane plane) const;
  [[nodiscard]] Result<std::size_t> create_derived_workplane(GuiDocumentSession& session,
                                                              DerivedWorkplane workplane) const;
  [[nodiscard]] Result<std::size_t> create_sketch(GuiDocumentSession& session, Sketch sketch) const;

  [[nodiscard]] Result<std::size_t> edit_sketch(GuiDocumentSession& session, SketchId sketch,
                                                 std::string label,
                                                 const GuiSketchMutation& mutation) const;
  [[nodiscard]] Result<std::size_t> add_line(GuiDocumentSession& session, SketchId sketch,
                                             LineSegment line) const;
  [[nodiscard]] Result<std::size_t> add_arc(GuiDocumentSession& session, SketchId sketch,
                                            ArcSegment arc) const;
  [[nodiscard]] Result<std::size_t> add_spline(GuiDocumentSession& session, SketchId sketch,
                                               SplineSegment spline) const;
  [[nodiscard]] Result<std::size_t> add_trim_extend(GuiDocumentSession& session, SketchId sketch,
                                                    SketchTrimExtendOperation operation) const;
  [[nodiscard]] Result<std::size_t> add_projected_point(GuiDocumentSession& session, SketchId sketch,
                                                        ProjectedSketchPoint point) const;
  [[nodiscard]] Result<std::size_t> add_projected_line(GuiDocumentSession& session, SketchId sketch,
                                                       ProjectedSketchLine line) const;
  [[nodiscard]] Result<std::size_t> add_reference_line(GuiDocumentSession& session, SketchId sketch,
                                                       ReferenceGeneratedLine line) const;
  [[nodiscard]] Result<std::size_t> add_constraint(GuiDocumentSession& session, SketchId sketch,
                                                   SketchConstraint constraint) const;
  [[nodiscard]] Result<std::size_t> add_constraint(GuiDocumentSession& session, SketchId sketch,
                                                   SketchGeometricConstraint constraint) const;
  [[nodiscard]] Result<std::size_t> add_dimension(GuiDocumentSession& session, SketchId sketch,
                                                  SketchDrivingDimension dimension) const;
  [[nodiscard]] Result<std::size_t> add_tangent_continuity(
      GuiDocumentSession& session, SketchId sketch, SketchTangentContinuity continuity) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                ClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                ArcClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CompositeClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CircleProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                RectangleProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CircularHolePattern pattern) const;

  [[nodiscard]] Result<GuiSketchPlaneView> plane_view(const GuiDocumentSession& session,
                                                       SketchId sketch) const;
  [[nodiscard]] Result<GuiSketchInspection> inspect(const GuiDocumentSession& session,
                                                     SketchId sketch) const;
  [[nodiscard]] Result<SketchRepairCommandResult>
  preview_repair(const GuiDocumentSession& session, SketchId sketch,
                 const SketchRepairSuggestion& suggestion) const;
  [[nodiscard]] Result<std::size_t> apply_repair(GuiDocumentSession& session, SketchId sketch,
                                                 const SketchRepairSuggestion& suggestion) const;
};

enum class GuiSketchInteractionStage {
  Idle,
  Hover,
  CollectingPicks,
  NumericInput,
  Preview,
  SelectedHandle,
  DragCandidate,
};

enum class GuiSketchFocusTarget { Canvas, NumericHud, TaskPanel };

struct GuiSketchWorkspaceStatus {
  GuiSketchInteractionStage stage{GuiSketchInteractionStage::Idle};
  GuiSketchFocusTarget focus{GuiSketchFocusTarget::Canvas};
  std::string tool_hint{"Select geometry or choose a Sketch command"};
  std::optional<Point2> cursor_coordinates;
  std::string snap_inference;
  std::optional<std::size_t> remaining_dof;
  std::string solve_status{"Not evaluated"};
};

class GuiSketchWorkspace {
public:
  [[nodiscard]] static const std::vector<std::string_view>& command_groups() noexcept;
  [[nodiscard]] static const std::vector<std::string_view>& browser_sections() noexcept;
  [[nodiscard]] static constexpr std::uint32_t selection_filter_mask() noexcept {
    return selection_kind_bit(GuiSelectionKind::SketchEntity) |
           selection_kind_bit(GuiSelectionKind::Edge) |
           selection_kind_bit(GuiSelectionKind::Vertex);
  }

  [[nodiscard]] Result<GuiSketchPlaneView> enter(GuiDocumentSession& session,
                                                  const GuiSketchWorkbench& workbench,
                                                  SketchId sketch);
  [[nodiscard]] Result<GuiWorkspace> finish(GuiDocumentSession& session,
                                            const GuiSketchWorkbench& workbench);

  [[nodiscard]] bool set_hover(bool hovered) noexcept;
  [[nodiscard]] bool begin_command(GuiDocumentSession& session, std::string command_id,
                                   std::string tool_hint, bool repeatable = true);
  [[nodiscard]] bool begin_numeric_input(GuiDocumentSession& session) noexcept;
  [[nodiscard]] bool set_numeric_input(std::string text);
  [[nodiscard]] bool show_preview(GuiDocumentSession& session) noexcept;
  [[nodiscard]] bool commit_command(GuiDocumentSession& session) noexcept;
  [[nodiscard]] bool repeat_last_command(GuiDocumentSession& session);
  [[nodiscard]] bool escape(GuiDocumentSession& session) noexcept;

  [[nodiscard]] bool select_handle(GuiDocumentSession& session, std::string semantic_id);
  [[nodiscard]] bool show_drag_candidate(GuiDocumentSession& session) noexcept;
  [[nodiscard]] bool commit_drag(GuiDocumentSession& session) noexcept;

  void set_cursor_coordinates(std::optional<Point2> coordinates) noexcept;
  void set_snap_inference(std::string inference);
  void set_solve_feedback(std::optional<std::size_t> remaining_dof, std::string solve_status);
  void focus_canvas() noexcept;
  void focus_task_panel() noexcept;

  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] const std::optional<SketchId>& active_sketch() const noexcept;
  [[nodiscard]] GuiSketchInteractionStage stage() const noexcept;
  [[nodiscard]] std::string_view active_command() const noexcept;
  [[nodiscard]] std::string_view last_repeatable_command() const noexcept;
  [[nodiscard]] std::string_view numeric_input() const noexcept;
  [[nodiscard]] std::string_view selected_handle() const noexcept;
  [[nodiscard]] const GuiSketchWorkspaceStatus& status() const noexcept;

private:
  void reset_interaction() noexcept;
  void leave_workspace() noexcept;

  std::optional<SketchId> active_sketch_;
  GuiWorkspace previous_workspace_{GuiWorkspace::Part};
  std::vector<GuiSelection> previous_selection_;
  GuiSketchInteractionStage stage_{GuiSketchInteractionStage::Idle};
  GuiSketchFocusTarget focus_{GuiSketchFocusTarget::Canvas};
  std::string active_command_;
  std::string active_tool_hint_;
  bool active_command_repeatable_{false};
  std::string last_repeatable_command_;
  std::string last_repeatable_hint_;
  std::string numeric_input_;
  std::string selected_handle_;
  GuiSketchWorkspaceStatus status_;
};

} // namespace blcad::gui
