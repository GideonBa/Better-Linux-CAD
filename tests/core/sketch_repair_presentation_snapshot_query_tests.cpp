#include "blcad/core/sketch_repair_presentation_snapshot_query.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

SketchRepairPresentationSnapshotEntry make_entry(
    std::size_t index,
    bool latest,
    SketchRepairSuggestionAction action,
    bool undoable,
    std::string target,
    std::string title,
    std::string label_id,
    SketchRepairDisplayCategory category,
    SketchRepairDisplayPriority priority,
    SketchRepairAffectedCounts counts,
    std::string affected_summary) {
  return SketchRepairPresentationSnapshotEntry(
      index, latest, SketchRepairTransactionStatus::Applied, action, std::move(target), undoable,
      std::move(title), "description", std::move(label_id), category, priority, counts,
      std::move(affected_summary));
}

SketchRepairPresentationSnapshot sample_snapshot() {
  std::vector<SketchRepairPresentationSnapshotEntry> entries;
  entries.push_back(make_entry(0U, false,
                               SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint,
                               true, "line.a:start", "Undo remove duplicate fixed endpoint constraints",
                               "undo.remove_duplicate_fixed_endpoint_constraint",
                               SketchRepairDisplayCategory::UndoEntry,
                               SketchRepairDisplayPriority::Normal,
                               SketchRepairAffectedCounts(0U, 1U, 0U), "1 constraint removed"));
  entries.push_back(make_entry(1U, false,
                               SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint,
                               false, "line.locked", "Resolve horizontal/vertical conflict",
                               "repair.remove_conflicting_orientation_constraint",
                               SketchRepairDisplayCategory::RequiresUserChoice,
                               SketchRepairDisplayPriority::High,
                               SketchRepairAffectedCounts(0U, 0U, 0U),
                               "No affected sketch records"));
  entries.push_back(make_entry(2U, true,
                               SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension,
                               true, "line.a:start|line.a:end",
                               "Undo remove duplicate driving dimension",
                               "undo.remove_duplicate_driving_dimension",
                               SketchRepairDisplayCategory::UndoEntry,
                               SketchRepairDisplayPriority::Normal,
                               SketchRepairAffectedCounts(0U, 0U, 1U), "1 dimension removed"));
  return SketchRepairPresentationSnapshot(std::move(entries));
}

} // namespace

TEST_CASE("Sketch repair presentation snapshot query returns empty snapshots",
          "[core][sketch-repair-presentation-snapshot-query]") {
  const SketchRepairPresentationSnapshot snapshot({});
  const SketchRepairPresentationSnapshotQueryEngine engine;

  const auto result = engine.execute(snapshot, SketchRepairPresentationSnapshotQuery::all());

  CHECK(result.snapshot().empty());
  CHECK(result.snapshot().entry_count() == 0U);
  CHECK(result.counts().total_count() == 0U);

  const auto json = serialize_sketch_repair_presentation_snapshot_query_result_to_json(result);
  REQUIRE(json);
  CHECK(json.value().find("blcad.sketch_repair_presentation_snapshot_query.debug") !=
        std::string::npos);
  CHECK(json.value().find("\"entry_count\": 0") != std::string::npos);
}

TEST_CASE("Sketch repair presentation snapshot query filters by category and preserves order",
          "[core][sketch-repair-presentation-snapshot-query]") {
  const auto snapshot = sample_snapshot();
  const SketchRepairPresentationSnapshotQueryEngine engine;

  const auto result = engine.execute(
      snapshot, SketchRepairPresentationSnapshotQuery::with_category(
                    SketchRepairDisplayCategory::UndoEntry));

  REQUIRE(result.snapshot().entry_count() == 2U);
  CHECK(result.snapshot().entries()[0].index() == 0U);
  CHECK(result.snapshot().entries()[1].index() == 2U);
  CHECK(result.counts().category_count(SketchRepairDisplayCategory::UndoEntry) == 2U);
  CHECK(result.counts().priority_count(SketchRepairDisplayPriority::Normal) == 2U);
  CHECK(result.counts().total_count() == 2U);
}

TEST_CASE("Sketch repair presentation snapshot query filters by priority",
          "[core][sketch-repair-presentation-snapshot-query]") {
  const auto snapshot = sample_snapshot();
  const SketchRepairPresentationSnapshotQueryEngine engine;

  const auto result = engine.execute(
      snapshot, SketchRepairPresentationSnapshotQuery::with_priority(
                    SketchRepairDisplayPriority::High));

  REQUIRE(result.snapshot().entry_count() == 1U);
  CHECK(result.snapshot().entries().front().index() == 1U);
  CHECK(result.snapshot().entries().front().priority() == SketchRepairDisplayPriority::High);
  CHECK(result.counts().priority_count(SketchRepairDisplayPriority::High) == 1U);
}

TEST_CASE("Sketch repair presentation snapshot query supports latest-only and undoable-only",
          "[core][sketch-repair-presentation-snapshot-query]") {
  const auto snapshot = sample_snapshot();
  const SketchRepairPresentationSnapshotQueryEngine engine;

  SketchRepairPresentationSnapshotQuery latest_query;
  latest_query.set_latest_only(true);
  const auto latest_result = engine.execute(snapshot, latest_query);
  REQUIRE(latest_result.snapshot().entry_count() == 1U);
  CHECK(latest_result.snapshot().entries().front().index() == 2U);
  CHECK(latest_result.snapshot().entries().front().latest());

  SketchRepairPresentationSnapshotQuery undoable_query;
  undoable_query.set_undoable_only(true);
  const auto undoable_result = engine.execute(snapshot, undoable_query);
  REQUIRE(undoable_result.snapshot().entry_count() == 2U);
  CHECK(undoable_result.snapshot().entries()[0].index() == 0U);
  CHECK(undoable_result.snapshot().entries()[1].index() == 2U);
  CHECK(undoable_result.counts().category_count(SketchRepairDisplayCategory::UndoEntry) == 2U);
}

TEST_CASE("Sketch repair presentation snapshot query supports combined filters and JSON metadata",
          "[core][sketch-repair-presentation-snapshot-query]") {
  const auto snapshot = sample_snapshot();
  const SketchRepairPresentationSnapshotQueryEngine engine;

  SketchRepairPresentationSnapshotQuery query;
  query.set_category_filter(SketchRepairDisplayCategory::UndoEntry);
  query.set_priority_filter(SketchRepairDisplayPriority::Normal);
  query.set_latest_only(true);
  query.set_undoable_only(true);

  const auto result = engine.execute(snapshot, query);
  REQUIRE(result.snapshot().entry_count() == 1U);
  CHECK(result.snapshot().entries().front().index() == 2U);
  CHECK(result.snapshot().entries().front().label_id() ==
        "undo.remove_duplicate_driving_dimension");
  CHECK(result.counts().category_count(SketchRepairDisplayCategory::UndoEntry) == 1U);
  CHECK(result.counts().priority_count(SketchRepairDisplayPriority::Normal) == 1U);

  const auto json = serialize_sketch_repair_presentation_snapshot_query_result_to_json(result);
  REQUIRE(json);
  CHECK(json.value().find("\"category\": \"undo_entry\"") != std::string::npos);
  CHECK(json.value().find("\"priority\": \"normal\"") != std::string::npos);
  CHECK(json.value().find("\"latest_only\": true") != std::string::npos);
  CHECK(json.value().find("\"undoable_only\": true") != std::string::npos);
  CHECK(json.value().find("\"label_id\": \"undo.remove_duplicate_driving_dimension\"") !=
        std::string::npos);
  CHECK(json.value().find("\"undo_entry\": 1") != std::string::npos);
}
