#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_transactions.hpp"
#include "blcad/core/sketch_repair_undo_stack.hpp"
#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
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

TEST_CASE("Sketch repair undo stack summary reports an empty stack",
          "[core][sketch-repair-undo-stack-summary]") {
  const SketchRepairUndoStack stack;
  const SketchRepairUndoStackSummarizer summarizer;

  const auto summary = summarizer.summarize(stack);
  CHECK(summary.empty());
  CHECK(summary.stack_size() == 0U);
  CHECK(summary.latest() == nullptr);

  const auto json = serialize_sketch_repair_undo_stack_summary_to_json(summary);
  REQUIRE(json);
  CHECK(json.value().find("blcad.sketch_repair_undo_stack_summary.debug") != std::string::npos);
  CHECK(json.value().find("\"stack_size\": 0") != std::string::npos);
}

TEST_CASE("Sketch repair undo stack summary exposes transactions in stack order",
          "[core][sketch-repair-undo-stack-summary]") {
  auto sketch =
      Sketch::create(SketchId("sketch.summary.lifo"), "Summary", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});
  add_duplicate_fixed_constraints(sketch.value());
  add_duplicate_dimensions(sketch.value());

  const SketchRepairTransactionExecutor transaction_executor;
  SketchRepairUndoStack stack;

  auto fixed_suggestions = suggestions_for(sketch.value());
  const auto* fixed_suggestion = find_action(
      fixed_suggestions, SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  REQUIRE(fixed_suggestion != nullptr);
  auto fixed_transaction =
      transaction_executor.apply(sketch.value(), SketchRepairCommand(*fixed_suggestion));
  REQUIRE(fixed_transaction);
  REQUIRE(stack.push(fixed_transaction.value()));

  auto dimension_suggestions = suggestions_for(sketch.value());
  const auto* dimension_suggestion = find_action(
      dimension_suggestions, SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  REQUIRE(dimension_suggestion != nullptr);
  auto dimension_transaction =
      transaction_executor.apply(sketch.value(), SketchRepairCommand(*dimension_suggestion));
  REQUIRE(dimension_transaction);
  REQUIRE(stack.push(dimension_transaction.value()));

  const SketchRepairUndoStackSummarizer summarizer;
  const auto summary = summarizer.summarize(stack);
  REQUIRE(summary.stack_size() == 2U);
  REQUIRE(summary.latest() != nullptr);
  CHECK(summary.latest()->action() ==
        SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);

  const auto& first = summary.entries().front();
  CHECK(first.index() == 0U);
  CHECK_FALSE(first.latest());
  CHECK(first.action() == SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  REQUIRE(first.removed_constraint_ids().size() == 1U);
  CHECK(first.removed_constraint_ids().front() == SketchConstraintId("constraint.b"));

  const auto& second = summary.entries().back();
  CHECK(second.index() == 1U);
  CHECK(second.latest());
  CHECK(second.action() == SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  REQUIRE(second.removed_dimension_ids().size() == 1U);
  CHECK(second.removed_dimension_ids().front() == SketchDimensionId("dim.b"));

  const auto json = serialize_sketch_repair_undo_stack_summary_to_json(summary);
  REQUIRE(json);
  CHECK(json.value().find("remove_duplicate_fixed_endpoint_constraint") != std::string::npos);
  CHECK(json.value().find("remove_duplicate_driving_dimension") != std::string::npos);
  CHECK(json.value().find("\"latest\": true") != std::string::npos);
}
