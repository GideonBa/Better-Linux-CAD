#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_command_labels.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_transactions.hpp"
#include "blcad/core/sketch_repair_undo_stack.hpp"
#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

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

TEST_CASE("Sketch repair command labeler returns deterministic labels for all actions",
          "[core][sketch-repair-labels]") {
  const SketchRepairCommandLabeler labeler;
  const std::array actions{SketchRepairSuggestionAction::AddFixedEndpointConstraint,
                           SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint,
                           SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint,
                           SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension,
                           SketchRepairSuggestionAction::AddDrivingDimension};

  for (const auto action : actions) {
    const auto label = labeler.label_for(action);
    CHECK_FALSE(label.title().empty());
    CHECK_FALSE(label.description().empty());
  }

  CHECK(labeler.label_for(SketchRepairSuggestionAction::AddFixedEndpointConstraint).title() ==
        "Add fixed endpoint constraint");
  CHECK(labeler.label_for(SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint)
            .title() == "Resolve horizontal/vertical conflict");
  CHECK(labeler.label_for(SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint)
            .title() == "Remove duplicate fixed endpoint constraints");
  CHECK(labeler.label_for(SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension).title() ==
        "Remove duplicate driving dimension");
  CHECK(labeler.label_for(SketchRepairSuggestionAction::AddDrivingDimension).title() ==
        "Add driving dimension");
}

TEST_CASE("Sketch repair undo stack summary labels are deterministic",
          "[core][sketch-repair-labels]") {
  auto sketch =
      Sketch::create(SketchId("sketch.labels.summary"), "Labels", DatumPlaneId("datum.xy"));
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

  const SketchRepairCommandLabeler labeler;
  const auto label = labeler.label_for(*summary.latest());
  CHECK(label.title() == "Undo remove duplicate driving dimension");
  CHECK_FALSE(summary.latest()->target().empty());
  CHECK(label.description().find(summary.latest()->target()) != std::string::npos);

  const auto json = serialize_sketch_repair_undo_stack_summary_to_json(summary);
  REQUIRE(json);
  CHECK(json.value().find("\"title\": \"Undo remove duplicate driving dimension\"") !=
        std::string::npos);
  CHECK(json.value().find("\"description\":") != std::string::npos);
}
