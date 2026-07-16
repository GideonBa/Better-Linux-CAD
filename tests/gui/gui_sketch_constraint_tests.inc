#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
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

SketchConstraintIntent automatic_horizontal_constraint(const char* id) {
  auto target = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(target);
  auto automatic = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId(id), GuiSketchSnapKind::HorizontalInference, target.value());
  REQUIRE(automatic);
  REQUIRE(automatic.value().has_value());
  return *automatic.value();
}

} // namespace

TEST_CASE("Block 114 GUI constraint preview commits session geometry and catalog with exact undo redo",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraints", {10.0, 3.0});

  auto automatic = automatic_horizontal_constraint("constraint.auto.horizontal");
  CHECK(automatic.source() == SketchConstraintIntentSource::Automatic);
  const Point2 original_end = session.part_document()
                                  ->find_sketch(SketchId("sketch.constraints"))
                                  ->find_line_segment(SketchEntityId("line.a"))->end();
  auto controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.constraints"), automatic);
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

  REQUIRE(controller.value().commit(session));
  CHECK(session.undo_label() == "Add sketch constraint");
  auto committed_catalog = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(committed_catalog);
  REQUIRE(committed_catalog.value().constraints().size() == 1U);
  const auto* solved = session.part_document()
                           ->find_sketch(SketchId("sketch.constraints"))
                           ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(solved != nullptr);
  CHECK(solved->end().y == Catch::Approx(solved->start().y).margin(1.0e-7));
  const Point2 solved_end = solved->end();

  REQUIRE(controller.value().undo(session));
  auto undone_catalog = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(undone_catalog);
  CHECK(undone_catalog.value().constraints().empty());
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == original_end);
  REQUIRE(controller.value().redo(session));
  auto redone_catalog = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(redone_catalog);
  REQUIRE(redone_catalog.value().constraints().size() == 1U);
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == solved_end);
}

TEST_CASE("Block 114 session save open roundtrips document-level constraint sidecars",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraint_save", {12.0, 4.0});
  auto controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.constraints"),
      automatic_horizontal_constraint("constraint.auto.saved"));
  REQUIRE(controller);
  REQUIRE(controller.value().commit(session));

  const auto path = std::filesystem::temp_directory_path() /
                    "blcad_block114_constraint_session.blcad.json";
  const auto sidecar = std::filesystem::path(path.string() + ".sketch-constraints.json");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  ignored.clear();
  std::filesystem::remove(sidecar, ignored);
  REQUIRE(session.save_as(path));
  CHECK(std::filesystem::exists(path));
  CHECK(std::filesystem::exists(sidecar));
  CHECK_FALSE(session.dirty());

  GuiDocumentSession restored;
  REQUIRE(restored.open_file(path));
  auto restored_catalog = restored.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(restored_catalog);
  REQUIRE(restored_catalog.value().constraints().size() == 1U);
  CHECK(restored_catalog.value().constraints().front().id().value() ==
        "constraint.auto.saved");
  CHECK(restored_catalog.value().constraints().front().source() ==
        SketchConstraintIntentSource::Automatic);
  const auto* restored_line = restored.part_document()
                                  ->find_sketch(SketchId("sketch.constraints"))
                                  ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(restored_line != nullptr);
  CHECK(restored_line->end().y == Catch::Approx(restored_line->start().y).margin(1.0e-7));
  CHECK_FALSE(restored.dirty());

  ignored.clear();
  std::filesystem::remove(path, ignored);
  ignored.clear();
  std::filesystem::remove(sidecar, ignored);
}

TEST_CASE("Block 114 accepted constraints remain active in later solver-backed drags",
          "[gui][sketch-constraints][integration][sketch-auto-constraint][integration][sketch-live-solve]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraint_drag", {10.0, 3.0});
  auto authoring = GuiSketchConstraintController::create(
      session, SketchId("sketch.constraints"),
      automatic_horizontal_constraint("constraint.auto.horizontal.drag"));
  REQUIRE(authoring);
  REQUIRE(authoring.value().commit(session));

  auto drag = GuiSketchDragController::create(session, SketchId("sketch.constraints"));
  REQUIRE(drag);
  CHECK(std::any_of(drag.value().source_system().constraints().begin(),
                    drag.value().source_system().constraints().end(), [](const auto& constraint) {
                      return constraint.id() == "intent/constraint.auto.horizontal.drag";
                    }));
  const auto* line = session.part_document()
                         ->find_sketch(SketchId("sketch.constraints"))
                         ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(line != nullptr);
  const Point2 end = line->end();
  const auto handle = std::find_if(
      drag.value().handles().begin(), drag.value().handles().end(), [end](const auto& current) {
        return current.kind == GuiSketchDragHandleKind::Endpoint && current.position == end;
      });
  REQUIRE(handle != drag.value().handles().end());
  REQUIRE(drag.value().begin(handle->id));
  auto preview = drag.value().flush({20.0, 7.0});
  REQUIRE(preview);
  const auto* preview_line =
      preview.value().preview_sketch().find_line_segment(SketchEntityId("line.a"));
  REQUIRE(preview_line != nullptr);
  CHECK(preview_line->end().y == Catch::Approx(preview_line->start().y).margin(1.0e-7));
  REQUIRE(drag.value().commit_session_constraints(session));

  auto catalog = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(catalog);
  REQUIRE(catalog.value().constraints().size() == 1U);
  CHECK(catalog.value().constraints().front().id().value() ==
        "constraint.auto.horizontal.drag");
  const auto* committed = session.part_document()
                              ->find_sketch(SketchId("sketch.constraints"))
                              ->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(committed != nullptr);
  CHECK(committed->end().y == Catch::Approx(committed->start().y).margin(1.0e-7));
}

TEST_CASE("Block 114 selection compatibility uses semantic target capabilities",
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
  CHECK_FALSE(contains_kind(line_pair, SketchSolverConstraintKind::Tangent));

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

TEST_CASE("Block 114 conflict glyph refuses session commit without document or catalog mutation",
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
  auto fixed_catalog = SketchConstraintCatalog::create(
      session.part_document()->id(), SketchId("sketch.constraints"), {fixed.value()});
  REQUIRE(fixed_catalog);
  REQUIRE(session.commit_part_constraint_transaction(
      "Seed fixed constraint",
      [fixed_catalog = fixed_catalog.value()](PartDocument&,
                                               std::vector<SketchConstraintCatalog>& catalogs) {
        catalogs.push_back(fixed_catalog);
        return Result<std::size_t>::success(1U);
      }));
  const Point2 before = session.part_document()
                            ->find_sketch(SketchId("sketch.constraints"))
                            ->find_line_segment(SketchEntityId("line.a"))->end();

  auto controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.constraints"), vertical.value());
  REQUIRE(controller);
  CHECK_FALSE(controller.value().preview().authoring.accepted);
  auto current_catalog = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(current_catalog);
  CHECK(controller.value().preview().authoring.catalog_after == current_catalog.value());
  CHECK(controller.value().preview().glyph.state ==
        geometry::SketchConstraintGlyphState::Conflict);
  CHECK(std::find(controller.value().preview().authoring.conflict_ids.begin(),
                  controller.value().preview().authoring.conflict_ids.end(),
                  "constraint.vertical") !=
        controller.value().preview().authoring.conflict_ids.end());
  CHECK_FALSE(controller.value().commit(session));
  auto unchanged = session.sketch_constraint_catalog(SketchId("sketch.constraints"));
  REQUIRE(unchanged);
  REQUIRE(unchanged.value().constraints().size() == 1U);
  CHECK(session.part_document()
            ->find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == before);
}

TEST_CASE("Block 114 accepted constraints can be removed atomically with exact undo redo",
          "[gui][sketch-constraints]") {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  seed_constraint_sketch(session, workbench, "part.gui_constraint_remove", {10.0, 3.0});
  auto controller = GuiSketchConstraintController::create(
      session, SketchId("sketch.constraints"),
      automatic_horizontal_constraint("constraint.auto.removable"));
  REQUIRE(controller);
  REQUIRE(controller.value().commit(session));
  REQUIRE(session.sketch_constraint_catalog(SketchId("sketch.constraints"))
              .value().constraints().size() == 1U);

  SECTION("removal deletes the catalog record as one history entry") {
    REQUIRE(GuiSketchConstraintController::remove_accepted(
        session, SketchId("sketch.constraints"),
        SketchConstraintId("constraint.auto.removable")));
    CHECK(session.undo_label() == "Remove sketch constraint");
    CHECK(session.sketch_constraint_catalog(SketchId("sketch.constraints"))
              .value().constraints().empty());

    REQUIRE(session.undo());
    CHECK(session.sketch_constraint_catalog(SketchId("sketch.constraints"))
              .value().constraints().size() == 1U);
    REQUIRE(session.redo());
    CHECK(session.sketch_constraint_catalog(SketchId("sketch.constraints"))
              .value().constraints().empty());
  }

  SECTION("removal rejects unknown and already-removed constraint ids") {
    CHECK_FALSE(GuiSketchConstraintController::remove_accepted(
        session, SketchId("sketch.constraints"), SketchConstraintId("constraint.missing")));
    REQUIRE(GuiSketchConstraintController::remove_accepted(
        session, SketchId("sketch.constraints"),
        SketchConstraintId("constraint.auto.removable")));
    CHECK_FALSE(GuiSketchConstraintController::remove_accepted(
        session, SketchId("sketch.constraints"),
        SketchConstraintId("constraint.auto.removable")));
  }
}
