#include "blcad/gui/gui_sketch_constraints.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

bool block114_has_kind(const std::vector<SketchSolverConstraintKind>& kinds,
                       SketchSolverConstraintKind kind) {
  return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

} // namespace

TEST_CASE("Block 114 selection and inference matrices cover every geometric solver family",
          "[gui][sketch-constraints][integration][sketch-auto-constraint]") {
  auto sketch = Sketch::create(SketchId("sketch.family_matrix"), "Family Matrix",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 2.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), {0.0, 5.0}, {10.0, 8.0}).value()));
  REQUIRE(sketch.value().add_entity(ArcSegment::create_three_point(
      SketchEntityId("arc.a"), {20.0, 0.0}, {25.0, 5.0}, {30.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(ArcSegment::create_three_point(
      SketchEntityId("arc.b"), {20.0, 10.0}, {25.0, 15.0}, {30.0, 10.0}).value()));

  auto topology = SketchTopology::migrate_legacy(sketch.value());
  REQUIRE(topology);
  const auto* line_a_entity = topology.value().find_entity("entity/line.a");
  const auto* line_b_entity = topology.value().find_entity("entity/line.b");
  REQUIRE(line_a_entity != nullptr);
  REQUIRE(line_b_entity != nullptr);

  auto line_a = SketchConstraintIntentTarget::entity("entity/line.a");
  auto line_b = SketchConstraintIntentTarget::entity("entity/line.b");
  auto arc_a = SketchConstraintIntentTarget::entity("entity/arc.a");
  auto arc_b = SketchConstraintIntentTarget::entity("entity/arc.b");
  auto point_a = SketchConstraintIntentTarget::point(line_a_entity->points().front());
  auto point_b = SketchConstraintIntentTarget::point(line_b_entity->points().front());
  REQUIRE(line_a);
  REQUIRE(line_b);
  REQUIRE(arc_a);
  REQUIRE(arc_b);
  REQUIRE(point_a);
  REQUIRE(point_b);

  const auto one_line = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {line_a.value()});
  CHECK(block114_has_kind(one_line, SketchSolverConstraintKind::Fixed));
  CHECK(block114_has_kind(one_line, SketchSolverConstraintKind::Horizontal));
  CHECK(block114_has_kind(one_line, SketchSolverConstraintKind::Vertical));

  const auto one_point = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value()});
  CHECK(block114_has_kind(one_point, SketchSolverConstraintKind::Fixed));

  const auto two_points = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), point_b.value()});
  CHECK(block114_has_kind(two_points, SketchSolverConstraintKind::Coincident));

  const auto two_lines = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {line_a.value(), line_b.value()});
  CHECK(block114_has_kind(two_lines, SketchSolverConstraintKind::Parallel));
  CHECK(block114_has_kind(two_lines, SketchSolverConstraintKind::Perpendicular));
  CHECK(block114_has_kind(two_lines, SketchSolverConstraintKind::Collinear));
  CHECK(block114_has_kind(two_lines, SketchSolverConstraintKind::Equal));
  CHECK(block114_has_kind(two_lines, SketchSolverConstraintKind::Tangent));

  const auto two_arcs = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {arc_a.value(), arc_b.value()});
  CHECK(block114_has_kind(two_arcs, SketchSolverConstraintKind::Equal));
  CHECK(block114_has_kind(two_arcs, SketchSolverConstraintKind::Tangent));
  CHECK(block114_has_kind(two_arcs, SketchSolverConstraintKind::Concentric));

  const auto point_line = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), line_b.value()});
  CHECK(block114_has_kind(point_line, SketchSolverConstraintKind::Midpoint));
  CHECK(block114_has_kind(point_line, SketchSolverConstraintKind::PointOnObject));

  const auto point_arc = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), arc_a.value()});
  CHECK(block114_has_kind(point_arc, SketchSolverConstraintKind::PointOnObject));

  const auto symmetric = GuiSketchConstraintController::compatible_kinds(
      topology.value(), {point_a.value(), point_b.value(), line_a.value()});
  CHECK(block114_has_kind(symmetric, SketchSolverConstraintKind::Symmetric));

  auto vertical = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.vertical"), GuiSketchSnapKind::VerticalInference,
      line_a.value());
  auto midpoint = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.midpoint"), GuiSketchSnapKind::Midpoint,
      point_a.value(), line_b.value());
  auto center = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.center"), GuiSketchSnapKind::Center,
      arc_a.value(), arc_b.value());
  auto nearest = GuiSketchConstraintController::automatic_candidate(
      SketchConstraintId("constraint.auto.nearest"), GuiSketchSnapKind::Nearest,
      point_a.value(), arc_a.value());
  REQUIRE(vertical);
  REQUIRE(midpoint);
  REQUIRE(center);
  REQUIRE(nearest);
  REQUIRE(vertical.value().has_value());
  REQUIRE(midpoint.value().has_value());
  REQUIRE(center.value().has_value());
  REQUIRE(nearest.value().has_value());
  CHECK(vertical.value()->kind() == SketchSolverConstraintKind::Vertical);
  CHECK(midpoint.value()->kind() == SketchSolverConstraintKind::Midpoint);
  CHECK(center.value()->kind() == SketchSolverConstraintKind::Concentric);
  CHECK(nearest.value()->kind() == SketchSolverConstraintKind::PointOnObject);
  CHECK(vertical.value()->source() == SketchConstraintIntentSource::Automatic);
  CHECK(midpoint.value()->source() == SketchConstraintIntentSource::Automatic);
  CHECK(center.value()->source() == SketchConstraintIntentSource::Automatic);
  CHECK(nearest.value()->source() == SketchConstraintIntentSource::Automatic);
}
