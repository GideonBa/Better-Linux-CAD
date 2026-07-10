#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_presentation_snapshot.hpp"
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

TEST_CASE("Sketch repair presentation snapshot reports an empty stack",
          "[core][sketch-repair-presentation-snapshot]") {
  const SketchRepairUndoStack stack;
  const SketchRepairUndoStackSummarizer summarizer;
  const SketchRepairPresentationSnapshotBuilder builder;

  const auto summary = summarizer.summarize(stack);
  const auto snapshot = builder.build(summary);

  CHECK(snapshot.empty());
  CHECK(snapshot.entry_count() == 0U);
  CHECK(snapshot.latest() == nullptr);

  const auto json = serialize_sketch_repair_presentation_snapshot_to_json(snapshot);
  REQUIRE(json);
  CHECK(json.value().find("blcad.sketch_repair_presentation_snapshot.debug") != std::string::npos);
  CHECK(json.value().find("\"entry_count\": 0") != std::string::npos);
}

TEST_CASE("Sketch repair presentation snapshot combines summary labels and metadata",
          "[core][sketch-repair-presentation-snapshot]") {
  auto sketch = Sketch::create(SketchId("sketch.presentation.snapshot"), "Snapshot",
                               DatumPlaneId("datum.xy"));
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
  auto fixed_transaction = transaction_executor.apply(sketch.value(), SketchRepairCommand(*fixed_suggestion));
  REQUIRE(fixed_transaction);
  REQUIRE(fixed_transaction.value().undoable());
  REQUIRE(stack.push(fixed_transaction.value()));

  auto dimension_suggestions = suggestions_for(sketch.value());
  const auto* dimension_suggestion = find_action(
      dimension_suggestions, SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  REQUIRE(dimension_suggestion != nullptr);
  auto dimension_transaction = transaction_executor.apply(sketch.value(),
                                                          SketchRepairCommand(*dimension_suggestion));
  REQUIRE(dimension_transaction);
  REQUIRE(dimension_transaction.value().undoable());
  REQUIRE(stack.push(dimension_transaction.value()));

  const SketchRepairUndoStackSummarizer summarizer;
  const SketchRepairPresentationSnapshotBuilder builder;
  const auto snapshot = builder.build(summarizer.summarize(stack));

  REQUIRE(snapshot.entry_count() == 2U);
  REQUIRE(snapshot.latest() != nullptr);
  CHECK(snapshot.latest()->latest());
  CHECK(snapshot.latest()->action() == SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  CHECK(snapshot.latest()->title() == "Undo remove duplicate driving dimension");
  CHECK(snapshot.latest()->label_id() == "undo.remove_duplicate_driving_dimension");
  CHECK(snapshot.latest()->category() == SketchRepairDisplayCategory::UndoEntry);
  CHECK(snapshot.latest()->priority() == SketchRepairDisplayPriority::Normal);
  CHECK(snapshot.latest()->affected_counts().removed_dimensions() == 1U);
  CHECK(snapshot.latest()->affected_summary() == "1 dimension removed");

  const auto& first = snapshot.entries().front();
  CHECK(first.index() == 0U);
  CHECK_FALSE(first.latest());
  CHECK(first.action() == SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint);
  CHECK(first.label_id() == "undo.remove_duplicate_fixed_endpoint_constraint");
  CHECK(first.affected_counts().removed_constraints() == 1U);
  CHECK(first.affected_summary() == "1 constraint removed");

  const auto json = serialize_sketch_repair_presentation_snapshot_to_json(snapshot);
  REQUIRE(json);
  CHECK(json.value().find("blcad.sketch_repair_presentation_snapshot.debug") != std::string::npos);
  CHECK(json.value().find("\"title\": \"Undo remove duplicate driving dimension\"") !=
        std::string::npos);
  CHECK(json.value().find("\"label_id\": \"undo.remove_duplicate_driving_dimension\"") !=
        std::string::npos);
  CHECK(json.value().find("\"affected_summary\": \"1 dimension removed\"") !=
        std::string::npos);
}
