#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_commands.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

SketchReferenceTarget line_target(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

SketchReferenceTarget line_start(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_start(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

SketchReferenceTarget line_end(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_end(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

const SketchRepairSuggestion* find_action(const SketchRepairSuggestionReport& report,
                                          SketchRepairSuggestionAction action) {
  const auto found = std::find_if(
      report.suggestions().begin(), report.suggestions().end(),
      [action](const SketchRepairSuggestion& suggestion) { return suggestion.action() == action; });
  return found == report.suggestions().end() ? nullptr : &*found;
}

SketchRepairSuggestionReport suggestions_for(const Sketch& sketch) {
  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch);
  const SketchRepairSuggester suggester;
  return suggester.suggest(diagnostic_report);
}

} // namespace

TEST_CASE("Sketch repair command adds a deterministic fixed endpoint constraint",
          "[core][sketch-repair-command]") {
  auto sketch = Sketch::create(SketchId("sketch.command.free"), "Free", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion =
      find_action(suggestions, SketchRepairSuggestionAction::AddFixedEndpointConstraint);
  REQUIRE(suggestion != nullptr);

  const SketchRepairCommandExecutor executor;
  auto result = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(result);
  CHECK(result.value().applied());
  REQUIRE(result.value().changed_constraint_ids().size() == 1U);
  CHECK(result.value().changed_constraint_ids().front() ==
        SketchConstraintId("repair.fixed.line.free.start"));
  REQUIRE(sketch.value().find_geometric_constraint(
              SketchConstraintId("repair.fixed.line.free.start")) != nullptr);
}

TEST_CASE("Sketch repair command removes duplicate fixed endpoint constraints deterministically",
          "[core][sketch-repair-command]") {
  auto sketch = Sketch::create(SketchId("sketch.command.fixed_duplicates"), "Duplicates",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  auto fixed_a = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.a"),
                                                         line_start("line.a"));
  auto fixed_b = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.b"),
                                                         line_start("line.a"));
  auto fixed_c = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.c"),
                                                         line_end("line.a"));
  REQUIRE(fixed_a);
  REQUIRE(fixed_b);
  REQUIRE(fixed_c);
  REQUIRE(sketch.value().add_constraint(fixed_b.value()));
  REQUIRE(sketch.value().add_constraint(fixed_a.value()));
  REQUIRE(sketch.value().add_constraint(fixed_c.value()));

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion = find_action(
      suggestions, SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  REQUIRE(suggestion != nullptr);

  const SketchRepairCommandExecutor executor;
  auto result = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(result);
  CHECK(result.value().applied());
  REQUIRE(result.value().changed_constraint_ids().size() == 1U);
  CHECK(result.value().changed_constraint_ids().front() == SketchConstraintId("constraint.b"));
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.a")) != nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) == nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.c")) != nullptr);
}

TEST_CASE("Sketch repair command removes duplicate driving dimensions deterministically",
          "[core][sketch-repair-command]") {
  auto sketch = Sketch::create(SketchId("sketch.command.dimension_duplicates"), "Duplicates",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  auto dim_a = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.a"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width"));
  auto dim_b = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.b"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width.b"));
  REQUIRE(dim_a);
  REQUIRE(dim_b);
  REQUIRE(sketch.value().add_dimension(dim_b.value()));
  REQUIRE(sketch.value().add_dimension(dim_a.value()));

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion =
      find_action(suggestions, SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  REQUIRE(suggestion != nullptr);

  const SketchRepairCommandExecutor executor;
  auto result = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(result);
  CHECK(result.value().applied());
  REQUIRE(result.value().changed_dimension_ids().size() == 1U);
  CHECK(result.value().changed_dimension_ids().front() == SketchDimensionId("dim.b"));
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.a")) != nullptr);
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.b")) == nullptr);
}

TEST_CASE("Sketch repair command skips unsupported explicit suggestions",
          "[core][sketch-repair-command]") {
  auto sketch = Sketch::create(SketchId("sketch.command.unsupported"), "Unsupported",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.locked", Point2{0.0, 0.0}, Point2{10.0, 0.0});
  auto horizontal = SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("constraint.horizontal"), line_target("line.locked"));
  auto vertical = SketchGeometricConstraint::create_vertical(
      SketchConstraintId("constraint.vertical"), line_target("line.locked"));
  REQUIRE(horizontal);
  REQUIRE(vertical);
  REQUIRE(sketch.value().add_constraint(horizontal.value()));
  REQUIRE(sketch.value().add_constraint(vertical.value()));

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion = find_action(
      suggestions, SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint);
  REQUIRE(suggestion != nullptr);

  const SketchRepairCommandExecutor executor;
  auto result = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(result);
  CHECK(result.value().status() == SketchRepairCommandStatus::SkippedUnsupported);
  CHECK(result.value().changed_constraint_ids().empty());
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.horizontal")) !=
        nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.vertical")) !=
        nullptr);
}
