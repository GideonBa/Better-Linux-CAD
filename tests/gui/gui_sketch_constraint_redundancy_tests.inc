#include "blcad/gui/gui_sketch_constraints.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;

TEST_CASE("Block 114 redundant candidates remain preview-only and accepted catalogs republish glyphs",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  auto document = PartDocument::create(DocumentId("part.constraint_redundancy"),
                                       "Constraint Redundancy");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  auto sketch = Sketch::create(SketchId("sketch.constraints"), "Constraints",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(document.value().add_sketch(std::move(sketch.value())));

  auto line = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(line);
  auto accepted = SketchConstraintIntent::create(
      SketchConstraintId("constraint.horizontal.1"),
      SketchSolverConstraintKind::Horizontal, {line.value()});
  auto redundant = SketchConstraintIntent::create(
      SketchConstraintId("constraint.zz.horizontal.2"),
      SketchSolverConstraintKind::Horizontal, {line.value()},
      SketchConstraintIntentSource::Automatic);
  REQUIRE(accepted);
  REQUIRE(redundant);
  auto catalog = SketchConstraintCatalog::create(
      document.value().id(), SketchId("sketch.constraints"), {accepted.value()});
  REQUIRE(catalog);

  auto annotations = GuiSketchConstraintController::accepted_annotations(
      document.value(), catalog.value());
  REQUIRE(annotations);
  REQUIRE(annotations.value().size() == 1U);
  CHECK(annotations.value().front().hit_kind == GuiSketchHitKind::Glyph);
  CHECK(annotations.value().front().candidate_id == "constraint.horizontal.1");

  auto controller = GuiSketchConstraintController::create(
      document.value(), catalog.value(), redundant.value());
  REQUIRE(controller);
  CHECK_FALSE(controller.value().preview().authoring.accepted);
  CHECK(controller.value().preview().authoring.solve.status ==
        SketchSolveStatus::Redundant);
  CHECK(controller.value().preview().glyph.state ==
        geometry::SketchConstraintGlyphState::Redundant);
  CHECK(std::find(controller.value().preview().authoring.redundant_ids.begin(),
                  controller.value().preview().authoring.redundant_ids.end(),
                  "constraint.zz.horizontal.2") !=
        controller.value().preview().authoring.redundant_ids.end());
}
