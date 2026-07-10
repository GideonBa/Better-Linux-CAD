#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_transactions.hpp"
#include "blcad/core/sketch_repair_undo_stack.hpp"

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
  const auto found = std::find_if(report.suggestions().begin(), report.suggestions().end(),
                                  [action](const SketchRepairSuggestion& suggestion) {
                                    return suggestion.action() == action;
                                  });
  return found == report.suggestions().end() ? nullptr : &*found;
}

SketchRepairSuggestionReport suggestions_for(const Sketch& sketch) {
  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch);
  const SketchRepairSuggester suggester;
  return suggester.suggest(diagnostic_report);
}

void add_duplicate_fixed_constraints(Sketch& sketch) {
  auto fixed_a = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.a"),
                                                         line_start("line.a"));
  auto fixed_b = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.b"),
                                                         line_start("line.a"));
  REQUIRE(fixed_a);
  REQUIRE(fixed_b);
  REQUIRE(sketch.add_constraint(fixed_b.value()));
  REQUIRE(sketch.add_constraint(fixed_a.value()));
}

void add_duplicate_dimensions(Sketch& sketch) {
  auto dim_a = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.a"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width"));
  auto dim_b = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.b"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width.b"));
  REQUIRE(dim_a);
  REQUIRE(dim_b);
  REQUIRE(sketch.add_dimension(dim_b.value()));
  REQUIRE(sketch.add_dimension(dim_a.value()));
}

} // namespace

TEST_CASE("Sketch repair undo stack rejects non-undoable transactions",
          "[core][sketch-repair-undo-stack]") {
  auto sketch = Sketch::create(SketchId("sketch.undo_stack.unsupported"), "Unsupported",
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

  const SketchRepairTransactionExecutor transaction_executor;
  auto transaction = transaction_executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  CHECK_FALSE(transaction.value().undoable());

  SketchRepairUndoStack stack;
  auto pushed = stack.push(transaction.value());
  REQUIRE(pushed);
  CHECK(pushed.value().status() == SketchRepairUndoStackStatus::RejectedNotUndoable);
  CHECK(pushed.value().remaining_stack_size() == 0U);
  CHECK(stack.empty());
}

TEST_CASE("Sketch repair undo stack undoes transactions in strict LIFO order",
          "[core][sketch-repair-undo-stack]") {
  auto sketch = Sketch::create(SketchId("sketch.undo_stack.lifo"), "Lifo",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});
  add_duplicate_fixed_constraints(sketch.value());
  add_duplicate_dimensions(sketch.value());

  const SketchRepairTransactionExecutor transaction_executor;
  SketchRepairUndoStack stack;

  auto duplicate_fixed_suggestions = suggestions_for(sketch.value());
  const auto* fixed_suggestion = find_action(
      duplicate_fixed_suggestions, SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  REQUIRE(fixed_suggestion != nullptr);
  auto fixed_transaction = transaction_executor.apply(sketch.value(),
                                                      SketchRepairCommand(*fixed_suggestion));
  REQUIRE(fixed_transaction);
  REQUIRE(fixed_transaction.value().undoable());
  auto pushed_fixed = stack.push(fixed_transaction.value());
  REQUIRE(pushed_fixed);
  CHECK(pushed_fixed.value().pushed());
  CHECK(pushed_fixed.value().remaining_stack_size() == 1U);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.a")) != nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) == nullptr);

  auto duplicate_dimension_suggestions = suggestions_for(sketch.value());
  const auto* dimension_suggestion = find_action(
      duplicate_dimension_suggestions, SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  REQUIRE(dimension_suggestion != nullptr);
  auto dimension_transaction = transaction_executor.apply(
      sketch.value(), SketchRepairCommand(*dimension_suggestion));
  REQUIRE(dimension_transaction);
  REQUIRE(dimension_transaction.value().undoable());
  auto pushed_dimension = stack.push(dimension_transaction.value());
  REQUIRE(pushed_dimension);
  CHECK(pushed_dimension.value().pushed());
  CHECK(pushed_dimension.value().remaining_stack_size() == 2U);
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.a")) != nullptr);
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.b")) == nullptr);

  auto undo_dimension = stack.undo_latest(sketch.value());
  REQUIRE(undo_dimension);
  CHECK(undo_dimension.value().undone());
  CHECK(undo_dimension.value().remaining_stack_size() == 1U);
  REQUIRE(undo_dimension.value().restored_dimension_ids().size() == 1U);
  CHECK(undo_dimension.value().restored_dimension_ids().front() == SketchDimensionId("dim.b"));
  CHECK(sketch.value().find_driving_dimension(SketchDimensionId("dim.b")) != nullptr);
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) == nullptr);

  auto undo_fixed = stack.undo_latest(sketch.value());
  REQUIRE(undo_fixed);
  CHECK(undo_fixed.value().undone());
  CHECK(undo_fixed.value().remaining_stack_size() == 0U);
  REQUIRE(undo_fixed.value().restored_constraint_ids().size() == 1U);
  CHECK(undo_fixed.value().restored_constraint_ids().front() == SketchConstraintId("constraint.b"));
  CHECK(sketch.value().find_geometric_constraint(SketchConstraintId("constraint.b")) != nullptr);
  CHECK(stack.empty());
}

TEST_CASE("Sketch repair undo stack reports empty undo without mutation",
          "[core][sketch-repair-undo-stack]") {
  auto sketch = Sketch::create(SketchId("sketch.undo_stack.empty"), "Empty",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  SketchRepairUndoStack stack;
  auto result = stack.undo_latest(sketch.value());
  REQUIRE(result);
  CHECK(result.value().status() == SketchRepairUndoStackStatus::Empty);
  CHECK(result.value().remaining_stack_size() == 0U);
}
