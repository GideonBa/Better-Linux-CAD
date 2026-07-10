#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_presentation_metadata.hpp"
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

} // namespace

TEST_CASE("Sketch repair presentation metadata classifies repair actions",
          "[core][sketch-repair-presentation-metadata]") {
  const SketchRepairPresentationMetadataProvider metadata_provider;

  const auto safe =
      metadata_provider.metadata_for(SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension);
  CHECK(safe.label_id() == "repair.remove_duplicate_driving_dimension");
  CHECK(safe.category() == SketchRepairDisplayCategory::SafeApply);
  CHECK(safe.priority() == SketchRepairDisplayPriority::Normal);
  CHECK(safe.affected_counts().empty());
  CHECK(safe.affected_summary() == "No affected sketch records");

  const auto choice = metadata_provider.metadata_for(
      SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint);
  CHECK(choice.label_id() == "repair.remove_conflicting_orientation_constraint");
  CHECK(choice.category() == SketchRepairDisplayCategory::RequiresUserChoice);
  CHECK(choice.priority() == SketchRepairDisplayPriority::High);

  const auto parameter =
      metadata_provider.metadata_for(SketchRepairSuggestionAction::AddDrivingDimension);
  CHECK(parameter.label_id() == "repair.add_driving_dimension");
  CHECK(parameter.category() == SketchRepairDisplayCategory::RequiresParameterValue);
  CHECK(parameter.priority() == SketchRepairDisplayPriority::Low);
}

TEST_CASE("Sketch repair presentation metadata summarizes undo stack affected records",
          "[core][sketch-repair-presentation-metadata]") {
  auto sketch = Sketch::create(SketchId("sketch.presentation.metadata"), "Presentation",
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

  const SketchRepairTransactionExecutor transaction_executor;
  auto transaction = transaction_executor.apply(sketch.value(), SketchRepairCommand(*suggestion));
  REQUIRE(transaction);
  REQUIRE(transaction.value().undoable());

  SketchRepairUndoStack stack;
  REQUIRE(stack.push(transaction.value()));

  const SketchRepairUndoStackSummarizer summarizer;
  const auto summary = summarizer.summarize(stack);
  REQUIRE(summary.latest() != nullptr);

  const SketchRepairPresentationMetadataProvider metadata_provider;
  const auto metadata = metadata_provider.metadata_for(*summary.latest());
  CHECK(metadata.label_id() == "undo.remove_duplicate_driving_dimension");
  CHECK(metadata.category() == SketchRepairDisplayCategory::UndoEntry);
  CHECK(metadata.priority() == SketchRepairDisplayPriority::Normal);
  CHECK(metadata.affected_counts().added_constraints() == 0U);
  CHECK(metadata.affected_counts().removed_constraints() == 0U);
  CHECK(metadata.affected_counts().removed_dimensions() == 1U);
  CHECK(metadata.affected_counts().total() == 1U);
  CHECK(metadata.affected_summary() == "1 dimension removed");

  const auto json = serialize_sketch_repair_undo_stack_summary_to_json(summary);
  REQUIRE(json);
  CHECK(json.value().find("\"label_id\": \"undo.remove_duplicate_driving_dimension\"") !=
        std::string::npos);
  CHECK(json.value().find("\"display_category\": \"undo_entry\"") != std::string::npos);
  CHECK(json.value().find("\"display_priority\": \"normal\"") != std::string::npos);
  CHECK(json.value().find("\"affected_summary\": \"1 dimension removed\"") != std::string::npos);
}
