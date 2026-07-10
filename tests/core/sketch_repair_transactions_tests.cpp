#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_transactions.hpp"

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

TEST_CASE("Sketch repair transaction undoes added fixed endpoint constraints",
          "[core][sketch-repair-transaction]") {
  auto sketch =
      Sketch::create(SketchId("sketch.transaction.fixed"), "Fixed", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion =
      find_action(suggestions, SketchRepairSuggestionAction::AddFixedEndpointConstraint);
  REQUIRE(suggestion != nullptr);

  const SketchRepairTransactionExecutor executor;
  auto transaction = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  CHECK(transaction.value().applied());
  CHECK(transaction.value().undoable());
  REQUIRE(transaction.value().added_geometric_constraints().size() == 1U);
  CHECK(sketch.value().find_geometric_constraint(
            SketchConstraintId("repair.fixed.line.free.start")) != nullptr);

  auto undo = executor.undo(sketch.value(), transaction.value());
  REQUIRE(undo);
  CHECK(undo.value().undone());
  REQUIRE(undo.value().removed_constraint_ids().size() == 1U);
  CHECK(undo.value().removed_constraint_ids().front() ==
        SketchConstraintId("repair.fixed.line.free.start"));
  CHECK(sketch.value().find_geometric_constraint(
            SketchConstraintId("repair.fixed.line.free.start")) == nullptr);
}

TEST_CASE("Sketch repair transaction undoes removed duplicate fixed endpoint constraints",
          "[core][sketch-repair-transaction]") {
  auto sketch = Sketch::create(SketchId("sketch.transaction.duplicates"), "Duplicates",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  auto fixed_a = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.a"),
                                                         line_start("line.a"));
  auto fixed_b = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.b"),
                                                         line_start("line.a"));
  REQUIRE(fixed_a);
  REQUIRE(fixed_b);
  REQUIRE(sketch.value().add_constraint(fixed_b.value()));
  REQUIRE(sketch.value().add_constraint(fixed_a.value()));

  const auto suggestions = suggestions_for(sketch.value());
  const auto* suggestion = find_action(
      suggestions, SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  REQUIRE(suggestion != nullptr);

  const SketchRepairTransactionExecutor executor;
  auto transaction = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  CHECK(transaction.value().applied());
  REQUIRE(transaction.value().removed_geometric_constraints().size() == 1U);
  CHECK(transaction.value().removed_geometric_constraints().front().id() ==
        SketchConstraintId("constraint.b"));
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.a")) != nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) == nullptr);

  auto undo = executor.undo(sketch.value(), transaction.value());
  REQUIRE(undo);
  CHECK(undo.value().undone());
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.a")) != nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) != nullptr);
  REQUIRE(undo.value().restored_constraint_ids().size() == 1U);
  CHECK(undo.value().restored_constraint_ids().front() == SketchConstraintId("constraint.b"));
}

TEST_CASE("Sketch repair transaction undoes removed duplicate driving dimensions",
          "[core][sketch-repair-transaction]") {
  auto sketch = Sketch::create(SketchId("sketch.transaction.dimensions"), "Dimensions",
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

  const SketchRepairTransactionExecutor executor;
  auto transaction = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  CHECK(transaction.value().applied());
  REQUIRE(transaction.value().removed_driving_dimensions().size() == 1U);
  CHECK(transaction.value().removed_driving_dimensions().front().id() ==
        SketchDimensionId("dim.b"));
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.a")) != nullptr);
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.b")) == nullptr);

  auto undo = executor.undo(sketch.value(), transaction.value());
  REQUIRE(undo);
  CHECK(undo.value().undone());
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.a")) != nullptr);
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.b")) != nullptr);
  REQUIRE(undo.value().restored_dimension_ids().size() == 1U);
  CHECK(undo.value().restored_dimension_ids().front() == SketchDimensionId("dim.b"));
}

TEST_CASE("Sketch repair transaction skips undo for unsupported command results",
          "[core][sketch-repair-transaction]") {
  auto sketch = Sketch::create(SketchId("sketch.transaction.unsupported"), "Unsupported",
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

  const SketchRepairTransactionExecutor executor;
  auto transaction = executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  CHECK_FALSE(transaction.value().applied());
  CHECK_FALSE(transaction.value().undoable());

  auto undo = executor.undo(sketch.value(), transaction.value());
  REQUIRE(undo);
  CHECK(undo.value().status() == SketchRepairTransactionStatus::SkippedUnsupported);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.horizontal")) !=
        nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.vertical")) !=
        nullptr);
}
