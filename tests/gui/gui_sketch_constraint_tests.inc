#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

void seed_constraint_sketch(GuiDocumentSession& session, GuiSketchWorkbench& workbench,
                            const char* part_id, Point2 end) {
  REQUIRE(session.create_part(DocumentId(part_id), "Constraint Part"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.constraints"), "Constraints",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, end).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));
}

bool contains_kind(const std::vector<SketchSolverConstraintKind>& kinds,
                   SketchSolverConstraintKind kind) {
  return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

} // namespace

TEST_CASE("Block 114 GUI constraint preview commits geometry and catalog with exact undo redo",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraints", {10.0, 3.0});

  auto catalog = SketchConstraintCatalog::create(
      session.part_document()->id(), SketchId("sketch.constraints"));
  REQUIRE(catalog);
  auto target = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(target);
  auto automatic = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.horizontal"),
      GuiSketchSnapKind::HorizontalInference, target.value());
  REQUIRE(automatic);
  REQUIRE(automatic.value().has_value());
  CHECK(automatic.value()->source() == SketchConstraintIntentSource::Automatic);

  const Point2 original_end = session.part_document()
                                  ->find_sketch(SketchId("sketch.constraints"))
                                  ->find_line_segment(SketchEntityId("line.a"))->end();
  auto controller = GuiSketchConstraintController::create(
      *session.part_document(), catalog.value(), *automatic.value());
  REQUIRE(controller);
  CHECK(controller.value().preview().authoring.accepted);
  CHECK(controller.value().preview().glyph.state ==
        geometry::SketchConstraintGlyphState::Preview);
  CHECK(controller.value().preview().annotation.hit_kind == GuiSketchHitKind::Glyph);
  CHECK(controller.value().preview().annotation.candidate_id ==
        "constraint.auto.horizontal");
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);

  REQUIRE(controller.value().commit(session, catalog.value()));
  CHECK(session.undo_label() == "Add sketch constraint");
  REQUIRE(catalog.value().constraints().size() == 1U);
  const auto* solved = session.part_document()
                           ->find_sketch(SketchId("sketch.constraints"))
                           ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(solved != nullptr);
  CHECK(solved->end().y == Catch::Approx(solved->start().y).margin(1.0e-7));
  const Point2 solved_end = solved->end();

  REQUIRE(controller.value().undo(session, catalog.value()));
  CHECK(catalog.value().constraints().empty());
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);
  REQUIRE(controller.value().redo(session, catalog.value()));
  REQUIRE(catalog.value().constraints().size() == 1U);
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == solved_end);
}

TEST_CASE("Block 114 selection compatibility exposes every non-dimensional solver family",
          "[gui][sketch-constraints]") {
  auto sketch = Sketch::create(SketchId("sketch.compatibility"), "Compatibility",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 2.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), {0.0, 5.0}, {10.0, 8.0}).value()));
  auto topology = SketchTopology::migrate_legacy(sketch.value());
  REQUIRE(topology);
  const auto* first_entity = topology.value().find_entity("entity/line.a");
  const auto* second_entity = topology.value().find_entity("entity/line.b");
  REQUIRE(first_entity != nullptr);
  REQUIRE(second_entity != nullptr);

  auto line_a = SketchConstraintIntentTarget::entity("entity/line.a");
  auto line_b = SketchConstraintIntentTarget::entity("entity/line.b");
  auto point_a = SketchConstraintIntentTarget::point(first_entity->points().front());
  auto point_b = SketchConstraintIntentTarget::point(second_entity->points().front());
  REQUIRE(line_a);
  REQUIRE(line_b);
  REQUIRE(point_a);
  REQUIRE(point_b);

  const auto line_pair = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {line_a.value(), line_b.value()});
  CHECK(contains_kind(line_pair, SketchSolverConstraintKind::Parallel));
  CHECK(contains_kind(line_pair, SketchSolverConstraintKind::Perpendicular));
  CHECK(contains_kind(line_pair, SketchSolverConstraintKind::Collinear));
  CHECK(contains_kind(line_pair, SketchSolverConstraintKind::Equal));
  CHECK(contains_kind(line_pair, SketchSolverConstraintKind::Tangent));

  const auto point_pair = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), point_b.value()});
  CHECK(contains_kind(point_pair, SketchSolverConstraintKind::Coincident));
  const auto point_line = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), line_b.value()});
  CHECK(contains_kind(point_line, SketchSolverConstraintKind::Midpoint));
  CHECK(contains_kind(point_line, SketchSolverConstraintKind::PointOnObject));
  const auto symmetric = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), point_b.value(), line_a.value()});
  CHECK(contains_kind(symmetric, SketchSolverConstraintKind::Symmetric));

  auto coincident = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.coincident"), GuiSketchSnapKind::Endpoint,
      point_a.value(), point_b.value());
  REQUIRE(coincident);
  REQUIRE(coincident.value().has_value());
  CHECK(coincident.value()->kind() == SketchSolverConstraintKind::Coincident);
  CHECK(coincident.value()->source() == SketchConstraintIntentSource::Automatic);
}

TEST_CASE("Block 114 conflict glyph refuses commit without document or catalog mutation",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraint_conflict", {10.0, 0.0});

  auto line = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(line);
  auto fixed = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed"), SketchSolverConstraintKind::Fixed,
      {line.value()});
  auto vertical = SketchConstraintIntent::create(
      SketchConstraintId("constraint.vertical"), SketchSolverConstraintKind::Vertical,
      {line.value()}, SketchConstraintIntentSource::Automatic);
  REQUIRE(fixed);
  REQUIRE(vertical);
  auto catalog = SketchConstraintCatalog::create(
      session.part_document()->id(), SketchId("sketch.constraints"), {fixed.value()});
  REQUIRE(catalog);
  const Point2 before = session.part_document()
                            ->find_sketch(SketchId("sketch.constraints"))
                            ->find_line_segment(SketchEntityId("line.a"))->end();

  auto controller = GuiSketchConstraintController::create(
      *session.part_document(), catalog.value(), vertical.value());
  REQUIRE(controller);
  CHECK_FALSE(controller.value().preview().authoring.accepted);
  CHECK(controller.value().preview().glyph.state ==
        geometry::SketchConstraintGlyphState::Conflict);
  CHECK(std::find(controller.value().preview().authoring.conflict_ids.begin(),
                  controller.value().preview().authoring.conflict_ids.end(),
                  "constraint.vertical") !=
        controller.value().preview().authoring.conflict_ids.end());
  CHECK_FALSE(controller.value().commit(session, catalog.value()));
  REQUIRE(catalog.value().constraints().size() == 1U);
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == before);
}
