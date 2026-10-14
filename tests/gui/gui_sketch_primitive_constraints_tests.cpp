#include "blcad/gui/gui_sketch_create.hpp"

#include "blcad/core/sketch_constraint_catalog_system.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

GuiSketchPlaneView xy_plane() {
  return {
      DatumPlaneId("datum.xy"), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
}

struct ShapeExpectation {
  GuiSketchCreateTool tool;
  std::vector<Point2> picks;
  std::size_t lines;
  std::size_t arcs;
  std::size_t closed_profiles;
  std::size_t curve_profiles;
  std::vector<SketchSolverConstraintKind> kinds;
};

std::size_t count_kind(const SketchConstraintCatalog& catalog, SketchSolverConstraintKind kind) {
  return static_cast<std::size_t>(
      std::count_if(catalog.constraints().begin(), catalog.constraints().end(),
                    [kind](const auto& constraint) { return constraint.kind() == kind; }));
}

void verify_shape(const ShapeExpectation& expected) {
  GuiDocumentSession session;
  GuiSketchWorkbench workbench;
  const SketchId sketch_id("sketch.primitive_constraints");
  REQUIRE(session.create_part(DocumentId("part.primitive_constraints"), "Primitive constraints"));
  REQUIRE(workbench.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(sketch_id, "Primitive constraints", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(workbench.create_sketch(session, std::move(sketch.value())));

  auto controller = GuiSketchCreateController::begin(expected.tool, sketch_id, xy_plane());
  REQUIRE(controller);
  for (const auto point : expected.picks)
    REQUIRE(controller.value().add_pick({point, GuiSketchSnapKind::None}));
  REQUIRE(controller.value().commit(session, workbench));

  const Sketch* committed = session.part_document()->find_sketch(sketch_id);
  REQUIRE(committed != nullptr);
  CHECK(committed->line_segments().size() == expected.lines);
  CHECK(committed->arc_segments().size() == expected.arcs);
  CHECK(committed->closed_profiles().size() == expected.closed_profiles);
  CHECK(committed->arc_closed_profiles().size() == expected.curve_profiles);
  if (expected.tool == GuiSketchCreateTool::CornerRectangle ||
      expected.tool == GuiSketchCreateTool::CenterRectangle) {
    REQUIRE(committed->line_segments().size() == 4U);
    for (std::size_t index = 0U; index < 4U; ++index) {
      const auto& first = committed->line_segments()[index];
      const auto& second = committed->line_segments()[(index + 1U) % 4U];
      const Point2 first_direction{first.end().x - first.start().x,
                                   first.end().y - first.start().y};
      const Point2 second_direction{second.end().x - second.start().x,
                                    second.end().y - second.start().y};
      CHECK(std::abs(first_direction.x * second_direction.x +
                     first_direction.y * second_direction.y) < 1.0e-8);
    }
  }

  auto catalog = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(catalog);
  REQUIRE(catalog.value().constraints().size() == expected.kinds.size());
  for (const auto& constraint : catalog.value().constraints())
    CHECK(constraint.source() == SketchConstraintIntentSource::Automatic);
  for (const auto kind : expected.kinds)
    CHECK(count_kind(catalog.value(), kind) ==
          static_cast<std::size_t>(std::count(expected.kinds.begin(), expected.kinds.end(), kind)));
  CHECK(count_kind(catalog.value(), SketchSolverConstraintKind::Coincident) == 0U);

  auto topology = SketchTopology::migrate_legacy(*committed);
  REQUIRE(topology);
  auto system = SketchConstraintCatalogSystemBuilder::build(*session.part_document(),
                                                            topology.value(), catalog.value());
  REQUIRE(system);
  auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
  REQUIRE(solved);
  CHECK((solved.value().status == SketchSolveStatus::UnderConstrained ||
         solved.value().status == SketchSolveStatus::FullyConstrained));
  CHECK(solved.value().redundant_constraint_ids.empty());
  CHECK(solved.value().conflicting_constraint_ids.empty());

  REQUIRE(session.undo());
  const Sketch* undone = session.part_document()->find_sketch(sketch_id);
  REQUIRE(undone != nullptr);
  CHECK(undone->line_segments().empty());
  CHECK(undone->arc_segments().empty());
  CHECK(undone->closed_profiles().empty());
  CHECK(undone->arc_closed_profiles().empty());
  auto undone_catalog = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(undone_catalog);
  CHECK(undone_catalog.value().constraints().empty());
}

} // namespace

TEST_CASE("Compound Sketch tools persist primitive constraints in one transaction",
          "[gui][sketch-create][sketch-primitive-constraints][undo]") {
  SECTION("corner rectangle") {
    verify_shape({GuiSketchCreateTool::CornerRectangle,
                  {{1.0, 2.0}, {11.0, 8.0}},
                  4U,
                  0U,
                  1U,
                  0U,
                  {SketchSolverConstraintKind::Horizontal, SketchSolverConstraintKind::Horizontal,
                   SketchSolverConstraintKind::Vertical, SketchSolverConstraintKind::Vertical}});
  }
  SECTION("center rectangle") {
    verify_shape({GuiSketchCreateTool::CenterRectangle,
                  {{5.0, 5.0}, {11.0, 9.0}},
                  4U,
                  0U,
                  1U,
                  0U,
                  {SketchSolverConstraintKind::Horizontal, SketchSolverConstraintKind::Horizontal,
                   SketchSolverConstraintKind::Vertical, SketchSolverConstraintKind::Vertical}});
  }
  SECTION("three point rectangle") {
    verify_shape({GuiSketchCreateTool::ThreePointRectangle,
                  {{0.0, 0.0}, {8.0, 3.0}, {5.0, 9.0}},
                  4U,
                  0U,
                  1U,
                  0U,
                  {SketchSolverConstraintKind::Perpendicular, SketchSolverConstraintKind::Parallel,
                   SketchSolverConstraintKind::Parallel}});
  }
  SECTION("parallelogram") {
    verify_shape({GuiSketchCreateTool::Parallelogram,
                  {{0.0, 0.0}, {8.0, 1.0}, {11.0, 6.0}},
                  4U,
                  0U,
                  1U,
                  0U,
                  {SketchSolverConstraintKind::Parallel, SketchSolverConstraintKind::Parallel}});
  }
  SECTION("center slot") {
    verify_shape({GuiSketchCreateTool::CenterSlot,
                  {{0.0, 0.0}, {12.0, 4.0}, {1.0, 5.0}},
                  2U,
                  2U,
                  0U,
                  1U,
                  {SketchSolverConstraintKind::Parallel, SketchSolverConstraintKind::Tangent,
                   SketchSolverConstraintKind::Tangent, SketchSolverConstraintKind::Tangent,
                   SketchSolverConstraintKind::Tangent}});
  }
  SECTION("overall slot") {
    verify_shape({GuiSketchCreateTool::OverallSlot,
                  {{0.0, 0.0}, {20.0, 0.0}, {0.0, 3.0}},
                  2U,
                  2U,
                  0U,
                  1U,
                  {SketchSolverConstraintKind::Parallel, SketchSolverConstraintKind::Tangent,
                   SketchSolverConstraintKind::Tangent, SketchSolverConstraintKind::Tangent,
                   SketchSolverConstraintKind::Tangent}});
  }
  SECTION("regular polygon does not invent an incomplete regularity relation") {
    verify_shape(
        {GuiSketchCreateTool::RegularPolygon, {{0.0, 0.0}, {10.0, 0.0}}, 6U, 0U, 1U, 0U, {}});
  }
}
