#include "blcad/gui/gui_sketch_spline.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include "blcad/core/parameter.hpp"

#include <catch2/catch_test_macros.hpp>


using namespace blcad;
using namespace blcad::gui;

namespace {

SketchReferenceTarget line_target(const char* id) {
  return SketchReferenceTarget::create_line_segment(SketchEntityId(id)).value();
}

Parameter length_parameter(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

Parameter count_parameter(const char* id, double value) {
  return Parameter::create_count(ParameterId(id), id, Quantity::count(value, id).value()).value();
}

} // namespace

TEST_CASE("Block 99 workbench creates datums construction geometry and editable planar sketches",
          "[gui][datum-workplane][gui][sketch-workbench]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_sketch"), "GUI Sketch"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(session.commit_part_transaction("Seed sketch parameters", [](PartDocument& part) {
    auto radius = part.add_parameter(length_parameter("pattern.radius", 20.0));
    if (radius.has_error()) return radius;
    auto count = part.add_parameter(count_parameter("pattern.count", 6.0));
    if (count.has_error()) return count;
    return part.add_parameter(length_parameter("pattern.diameter", 4.0));
  }));

  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.z"), "Z Axis", {0.0, 0.0, 0.0},
                                         {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(workbench.create_datum_axis(session, std::move(axis.value())));
  auto point = ConstructionPoint::create_explicit(ConstructionPointId("point.origin"), "Origin",
                                                   {0.0, 0.0, 0.0});
  auto construction_line = ConstructionLine::create_explicit(
      ConstructionLineId("construction.x"), "X Axis", {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0});
  REQUIRE(point);
  REQUIRE(construction_line);
  REQUIRE(workbench.create_construction_point(session, std::move(point.value())));
  REQUIRE(workbench.create_construction_line(session, std::move(construction_line.value())));

  auto sketch = Sketch::create(SketchId("sketch.gui"), "GUI Sketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));
  REQUIRE(workbench.add_line(session, SketchId("sketch.gui"),
                             LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {20.0, 0.0}).value()));
  REQUIRE(workbench.add_arc(session, SketchId("sketch.gui"),
                            ArcSegment::create_three_point(SketchEntityId("arc.a"), {0.0, 0.0},
                                                           {5.0, 5.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.add_spline(session, SketchId("sketch.gui"),
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.a"), {0.0, 0.0}, {2.0, 4.0},
                                         {8.0, 4.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.add_trim_extend(session, SketchId("sketch.gui"),
      SketchTrimExtendOperation::create_extend(SketchTrimOperationId("extend.a"),
                                                SketchEntityId("line.a"), {25.0, 0.0}).value()));
  REQUIRE(workbench.add_projected_point(session, SketchId("sketch.gui"),
      ProjectedSketchPoint::create_from_construction_point(SketchEntityId("projected.origin"),
                                                            ConstructionPointId("point.origin")).value()));
  REQUIRE(workbench.add_projected_line(session, SketchId("sketch.gui"),
      ProjectedSketchLine::create_from_construction_line(SketchEntityId("projected.x"),
                                                          ConstructionLineId("construction.x")).value()));
  REQUIRE(workbench.add_constraint(session, SketchId("sketch.gui"),
      SketchGeometricConstraint::create_horizontal(SketchConstraintId("constraint.horizontal"),
                                                    line_target("line.a")).value()));
  REQUIRE(workbench.add_tangent_continuity(session, SketchId("sketch.gui"),
      SketchTangentContinuity::create_tangent(SketchConstraintId("constraint.tangent"),
                                               SketchEntityId("line.a"),
                                               SketchEntityId("spline.a")).value()));
  REQUIRE(workbench.add_profile(session, SketchId("sketch.gui"),
      CircularHolePattern::create(ProfileId("pattern.holes"), ParameterId("pattern.radius"),
                                  ParameterId("pattern.count"),
                                  ParameterId("pattern.diameter")).value()));

  const auto plane = workbench.plane_view(session, SketchId("sketch.gui"));
  REQUIRE(plane);
  CHECK(plane.value().model_to_plane({4.0, 7.0, 0.0}) == Point2{4.0, 7.0});
  CHECK(plane.value().plane_to_model({4.0, 7.0}) == Point3{4.0, 7.0, 0.0});
  const auto* edited = session.part_document()->find_sketch(SketchId("sketch.gui"));
  REQUIRE(edited != nullptr);
  CHECK(edited->tangent_continuities().size() == 1);
  CHECK(edited->circular_hole_patterns().size() == 1);
  CHECK(session.can_undo());
}

TEST_CASE("Block 99 diagnostics and explicit repair preview remain atomic",
          "[gui][sketch-workbench][gui][sketch-repair]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_repair"), "GUI Repair"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(session,
      Sketch::create(SketchId("sketch.repair"), "Repair", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.add_line(session, SketchId("sketch.repair"),
      LineSegment::create(SketchEntityId("line.free"), {0.0, 0.0}, {10.0, 0.0}).value()));

  auto inspection = workbench.inspect(session, SketchId("sketch.repair"));
  REQUIRE(inspection);
  CHECK(inspection.value().diagnostics.warning_count() == 2);
  REQUIRE_FALSE(inspection.value().repairs.empty());
  const auto suggestion = inspection.value().repairs.suggestions().front();
  const auto before = session.part_document()->find_sketch(SketchId("sketch.repair"))
                          ->geometric_constraints().size();
  auto preview = workbench.preview_repair(session, SketchId("sketch.repair"), suggestion);
  REQUIRE(preview);
  CHECK(preview.value().applied());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before);
  REQUIRE(workbench.apply_repair(session, SketchId("sketch.repair"), suggestion));
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before + 1);
  REQUIRE(session.undo());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.repair"))
            ->geometric_constraints().size() == before);
}

TEST_CASE("Block 99 selection prompts name semantic capabilities",
          "[gui][sketch-workbench]") {
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateSketch).required_capability ==
        "planar_workplane");
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::ProjectReference).required_capability ==
        "projectable_reference");
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::AddDimension).text.find("two compatible") !=
        std::string::npos);
}

TEST_CASE("Block 106 Sketch workspace preserves context and publishes the frozen surface",
          "[gui][sketch-workspace]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  GuiSketchWorkspace workspace;
  REQUIRE(session.create_part(DocumentId("part.workspace"), "Workspace"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(session,
      Sketch::create(SketchId("sketch.workspace"), "Workspace", DatumPlaneId("datum.xy")).value()));
  REQUIRE(session.set_workspace(GuiWorkspace::Inspect));
  REQUIRE(session.selection().add({GuiSelectionKind::Datum, "datum.xy"}));

  const auto plane = workspace.enter(session, workbench, SketchId("sketch.workspace"));
  REQUIRE(plane);
  CHECK(session.workspace() == GuiWorkspace::Sketch);
  CHECK(session.selection().empty());
  CHECK(workspace.active());
  CHECK(workspace.active_sketch()->value() == "sketch.workspace");
  CHECK(workspace.stage() == GuiSketchInteractionStage::Idle);
  CHECK(GuiSketchWorkspace::selection_filter_mask() ==
        (selection_kind_bit(GuiSelectionKind::SketchEntity) |
         selection_kind_bit(GuiSelectionKind::Edge) |
         selection_kind_bit(GuiSelectionKind::Vertex)));
  CHECK(GuiSketchWorkspace::command_groups().size() == 5);
  CHECK(GuiSketchWorkspace::command_groups().front() == "Create");
  CHECK(GuiSketchWorkspace::browser_sections().size() == 4);
  CHECK(GuiSketchWorkspace::browser_sections().back() == "Diagnostics");
  CHECK_FALSE(workspace.status().remaining_dof.has_value());
  CHECK(workspace.status().solve_status == "Not evaluated");

  workspace.set_cursor_coordinates(Point2{12.0, -3.0});
  workspace.set_snap_inference("Horizontal");
  workspace.set_solve_feedback(3U, "Under constrained");
  REQUIRE(workspace.status().cursor_coordinates.has_value());
  CHECK(workspace.status().cursor_coordinates.value() == Point2{12.0, -3.0});
  CHECK(workspace.status().snap_inference == "Horizontal");
  CHECK(workspace.status().remaining_dof == 3U);
  CHECK(workspace.status().solve_status == "Under constrained");

  const auto restored = workspace.finish(session, workbench);
  REQUIRE(restored);
  CHECK(restored.value() == GuiWorkspace::Inspect);
  CHECK(session.workspace() == GuiWorkspace::Inspect);
  CHECK(session.selection().contains(GuiSelectionKind::Datum, "datum.xy"));
  CHECK_FALSE(workspace.active());
}

TEST_CASE("Block 106 Sketch command lifecycle backtracks one stage with Esc and supports repeat",
          "[gui][sketch-workspace][gui][sketch-command-lifecycle]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  GuiSketchWorkspace workspace;
  REQUIRE(session.create_part(DocumentId("part.command"), "Command"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(session,
      Sketch::create(SketchId("sketch.command"), "Command", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workspace.enter(session, workbench, SketchId("sketch.command")));

  REQUIRE(workspace.set_hover(true));
  CHECK(workspace.stage() == GuiSketchInteractionStage::Hover);
  REQUIRE(workspace.begin_command(session, "sketch.line", "Pick line start"));
  CHECK(workspace.stage() == GuiSketchInteractionStage::CollectingPicks);
  CHECK(session.task().stage() == GuiTaskStage::CollectingSelection);
  REQUIRE(workspace.begin_numeric_input(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::NumericInput);
  CHECK(workspace.status().focus == GuiSketchFocusTarget::NumericHud);
  REQUIRE(workspace.set_numeric_input("25 mm"));
  REQUIRE(workspace.show_preview(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::Preview);
  CHECK(session.task().stage() == GuiTaskStage::Preview);

  REQUIRE(workspace.escape(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::NumericInput);
  CHECK(session.task().stage() == GuiTaskStage::EditingParameters);
  REQUIRE(workspace.escape(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::CollectingPicks);
  CHECK(session.task().stage() == GuiTaskStage::CollectingSelection);
  REQUIRE(workspace.escape(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(session.task().active());

  REQUIRE(workspace.begin_command(session, "sketch.line", "Pick line start"));
  REQUIRE(workspace.begin_numeric_input(session));
  REQUIRE(workspace.set_numeric_input("10 mm"));
  REQUIRE(workspace.show_preview(session));
  REQUIRE(workspace.commit_command(session));
  CHECK(workspace.last_repeatable_command() == "sketch.line");
  REQUIRE(workspace.repeat_last_command(session));
  CHECK(workspace.active_command() == "sketch.line");
  CHECK(workspace.stage() == GuiSketchInteractionStage::CollectingPicks);
  REQUIRE(workspace.escape(session));
  CHECK_FALSE(session.task().active());
}

TEST_CASE("Block 106 drag candidates remain transient until commit or cancel",
          "[gui][sketch-workspace][gui][sketch-command-lifecycle]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  GuiSketchWorkspace workspace;
  REQUIRE(session.create_part(DocumentId("part.drag"), "Drag"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(workbench.create_sketch(session,
      Sketch::create(SketchId("sketch.drag"), "Drag", DatumPlaneId("datum.xy")).value()));
  REQUIRE(workspace.enter(session, workbench, SketchId("sketch.drag")));

  REQUIRE(workspace.select_handle(session, "line.1:end"));
  CHECK(workspace.stage() == GuiSketchInteractionStage::SelectedHandle);
  REQUIRE(workspace.show_drag_candidate(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::DragCandidate);
  CHECK(session.task().stage() == GuiTaskStage::Preview);
  REQUIRE(workspace.escape(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(session.task().active());

  REQUIRE(workspace.select_handle(session, "line.1:end"));
  REQUIRE(workspace.show_drag_candidate(session));
  REQUIRE(workspace.commit_drag(session));
  CHECK(workspace.stage() == GuiSketchInteractionStage::Idle);
  CHECK_FALSE(session.task().active());
}

TEST_CASE("Block 113 GUI spline editing previews without mutation and commits one transaction",
          "[gui][sketch-spline]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_spline"), "GUI Spline"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));

  auto sketch = Sketch::create(SketchId("sketch.gui_spline"), "GUI Spline",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {0.0, 0.0}, {3.0, 0.0}, {7.0, 5.0}, {10.0, 5.0}).value()));
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.b"), {10.0, 5.0}, {10.0, 8.0}, {17.0, 0.0}, {20.0, 0.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  const Point2 original_control = session.part_document()
                                      ->find_sketch(SketchId("sketch.gui_spline"))
                                      ->find_spline_segment(SketchEntityId("spline.b"))
                                      ->control1();
  auto controller = GuiSketchSplineController::create(
      *session.part_document(), SketchId("sketch.gui_spline"),
      {SketchEntityId("spline.a"), SketchEntityId("spline.b")});
  REQUIRE(controller);
  REQUIRE(controller.value().align_tangent(0U));

  auto preview = controller.value().preview();
  REQUIRE(preview);
  REQUIRE(preview.value().authoring.continuity_handles.size() == 1U);
  CHECK(preview.value().authoring.continuity_handles.front().tangent_continuous);
  REQUIRE(preview.value().geometry.continuity.size() == 1U);
  CHECK(preview.value().geometry.continuity.front().tangent_continuous);
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.gui_spline"))
            ->find_spline_segment(SketchEntityId("spline.b"))
            ->control1() == original_control);

  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Edit sketch spline");
  const Point2 committed_control = session.part_document()
                                       ->find_sketch(SketchId("sketch.gui_spline"))
                                       ->find_spline_segment(SketchEntityId("spline.b"))
                                       ->control1();
  CHECK_FALSE(committed_control == original_control);
  REQUIRE(session.undo());
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.gui_spline"))
            ->find_spline_segment(SketchEntityId("spline.b"))
            ->control1() == original_control);
  REQUIRE(session.redo());
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.gui_spline"))
            ->find_spline_segment(SketchEntityId("spline.b"))
            ->control1() == committed_control);
}
