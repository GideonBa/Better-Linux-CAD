#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_modify.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace block116_modify_tests {

void seed_modify_session(GuiDocumentSession& session, GuiSketchWorkbench& workbench) {
  REQUIRE(session.create_part(DocumentId("part.gui_modify"), "GUI Modify"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.modify"), "Modify", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.h"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.v"), {4.0, -5.0}, {4.0, 5.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));
}

bool near(Point2 first, Point2 second) {
  return std::hypot(first.x - second.x, first.y - second.y) <= 1.0e-6;
}

} // namespace block116_modify_tests

TEST_CASE("Block 116 GUI trim previews and commits one atomic undoable transaction",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_modify_session(session, workbench);

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.modify"));
  REQUIRE(controller);
  auto preview = controller.value().trim(SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(preview);
  CHECK(near(preview.value().candidate.find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));
  CHECK(preview.value().invalidated_ids.empty());
  // Preview never mutates the document.
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {10.0, 0.0}));

  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Trim sketch geometry");
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));

  REQUIRE(session.undo());
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {10.0, 0.0}));
  REQUIRE(session.redo());
  CHECK(near(session.part_document()->find_sketch(SketchId("sketch.modify"))
                 ->find_line_segment(SketchEntityId("line.h"))->end(),
             {4.0, 0.0}));
}

TEST_CASE("Block 116 GUI fillet inserts a tangent arc through one transaction",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_fillet"), "GUI Fillet"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.fillet"), "Fillet", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.h"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.v"), {0.0, 0.0}, {0.0, 10.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.fillet"));
  REQUIRE(controller);
  auto preview = controller.value().fillet(SketchEntityId("line.h"), SketchEntityId("line.v"), 2.0);
  REQUIRE(preview);
  CHECK(preview.value().candidate.arc_segments().size() == 1U);
  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Fillet sketch corner");
  const Sketch* committed = session.part_document()->find_sketch(SketchId("sketch.fillet"));
  REQUIRE(committed != nullptr);
  CHECK(committed->arc_segments().size() == 1U);
  CHECK(near(committed->find_line_segment(SketchEntityId("line.h"))->start(), {2.0, 0.0}));
}

TEST_CASE("Block 116 GUI modify fails closed when it would drop a Block-114 catalog reference",
          "[gui][sketch-trim-extend]") {
  using namespace block116_modify_tests;
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  REQUIRE(session.create_part(DocumentId("part.gui_modify_catalog"), "GUI Modify Catalog"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.solo"), "Solo", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.solo"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  // Accept a Block-114 Horizontal constraint that references entity/line.solo.
  auto target = SketchConstraintIntentTarget::entity("entity/line.solo");
  REQUIRE(target);
  auto candidate = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.horizontal.solo"), GuiSketchSnapKind::HorizontalInference,
      target.value());
  REQUIRE(candidate);
  REQUIRE(candidate.value().has_value());
  auto constraint_controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.solo"), *candidate.value());
  REQUIRE(constraint_controller);
  REQUIRE(constraint_controller.value().commit(session));

  auto controller = GuiSketchModifyController::create(session, SketchId("sketch.solo"));
  REQUIRE(controller);
  // The line has no intersections, so trim removes it entirely; the Block-114
  // catalog still references entity/line.solo, so the atomic commit must fail
  // closed and leave the document and catalog unchanged.
  auto preview = controller.value().trim(SketchEntityId("line.solo"), {5.0, 0.0});
  REQUIRE(preview);
  CHECK(preview.value().candidate.line_segments().empty());
  auto committed = controller.value().commit(session);
  REQUIRE_FALSE(committed);
  CHECK(session.part_document()->find_sketch(SketchId("sketch.solo"))
            ->find_line_segment(SketchEntityId("line.solo")) != nullptr);
  CHECK(session.sketch_constraint_catalog(SketchId("sketch.solo"))
            .value().constraints().size() == 1U);
}
