#include "blcad/core/sketch_repair_presentation_snapshot_query.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

using json = nlohmann::json;

[[nodiscard]] json affected_counts_to_json(const SketchRepairAffectedCounts& counts) {
  return json{{"added_constraints", counts.added_constraints()},
              {"removed_constraints", counts.removed_constraints()},
              {"removed_dimensions", counts.removed_dimensions()},
              {"total", counts.total()}};
}

[[nodiscard]] json entry_to_json(const SketchRepairPresentationSnapshotEntry& entry) {
  return json{{"index", entry.index()},
              {"latest", entry.latest()},
              {"transaction_status", std::string(to_string(entry.transaction_status()))},
              {"action", std::string(to_string(entry.action()))},
              {"target", entry.target()},
              {"undoable", entry.undoable()},
              {"title", entry.title()},
              {"description", entry.description()},
              {"label_id", entry.label_id()},
              {"display_category", std::string(to_string(entry.category()))},
              {"display_priority", std::string(to_string(entry.priority()))},
              {"affected_counts", affected_counts_to_json(entry.affected_counts())},
              {"affected_summary", entry.affected_summary()}};
}

[[nodiscard]] json
optional_category_to_json(const std::optional<SketchRepairDisplayCategory>& category) {
  if (!category.has_value())
    return nullptr;
  return std::string(to_string(category.value()));
}

[[nodiscard]] json
optional_priority_to_json(const std::optional<SketchRepairDisplayPriority>& priority) {
  if (!priority.has_value())
    return nullptr;
  return std::string(to_string(priority.value()));
}

[[nodiscard]] SketchRepairPresentationSnapshotQueryCounts
compute_counts(const SketchRepairPresentationSnapshot& snapshot) {
  std::size_t safe_apply_count = 0U;
  std::size_t requires_user_choice_count = 0U;
  std::size_t requires_parameter_value_count = 0U;
  std::size_t undo_entry_count = 0U;
  std::size_t low_priority_count = 0U;
  std::size_t normal_priority_count = 0U;
  std::size_t high_priority_count = 0U;

  for (const auto& entry : snapshot.entries()) {
    switch (entry.category()) {
    case SketchRepairDisplayCategory::SafeApply:
      ++safe_apply_count;
      break;
    case SketchRepairDisplayCategory::RequiresUserChoice:
      ++requires_user_choice_count;
      break;
    case SketchRepairDisplayCategory::RequiresParameterValue:
      ++requires_parameter_value_count;
      break;
    case SketchRepairDisplayCategory::UndoEntry:
      ++undo_entry_count;
      break;
    }

    switch (entry.priority()) {
    case SketchRepairDisplayPriority::Low:
      ++low_priority_count;
      break;
    case SketchRepairDisplayPriority::Normal:
      ++normal_priority_count;
      break;
    case SketchRepairDisplayPriority::High:
      ++high_priority_count;
      break;
    }
  }

  return SketchRepairPresentationSnapshotQueryCounts(
      safe_apply_count, requires_user_choice_count, requires_parameter_value_count,
      undo_entry_count, low_priority_count, normal_priority_count, high_priority_count);
}

[[nodiscard]] json
category_counts_to_json(const SketchRepairPresentationSnapshotQueryCounts& counts) {
  return json{{"safe_apply", counts.category_count(SketchRepairDisplayCategory::SafeApply)},
              {"requires_user_choice",
               counts.category_count(SketchRepairDisplayCategory::RequiresUserChoice)},
              {"requires_parameter_value",
               counts.category_count(SketchRepairDisplayCategory::RequiresParameterValue)},
              {"undo_entry", counts.category_count(SketchRepairDisplayCategory::UndoEntry)}};
}

[[nodiscard]] json
priority_counts_to_json(const SketchRepairPresentationSnapshotQueryCounts& counts) {
  return json{{"low", counts.priority_count(SketchRepairDisplayPriority::Low)},
              {"normal", counts.priority_count(SketchRepairDisplayPriority::Normal)},
              {"high", counts.priority_count(SketchRepairDisplayPriority::High)}};
}

[[nodiscard]] json query_to_json(const SketchRepairPresentationSnapshotQuery& query) {
  return json{{"category", optional_category_to_json(query.category_filter())},
              {"priority", optional_priority_to_json(query.priority_filter())},
              {"latest_only", query.latest_only()},
              {"undoable_only", query.undoable_only()}};
}

} // namespace

SketchRepairPresentationSnapshotQuery::SketchRepairPresentationSnapshotQuery()
    : latest_only_(false), undoable_only_(false) {}

SketchRepairPresentationSnapshotQuery SketchRepairPresentationSnapshotQuery::all() {
  return SketchRepairPresentationSnapshotQuery();
}

SketchRepairPresentationSnapshotQuery
SketchRepairPresentationSnapshotQuery::with_category(SketchRepairDisplayCategory category) {
  SketchRepairPresentationSnapshotQuery query;
  query.set_category_filter(category);
  return query;
}

SketchRepairPresentationSnapshotQuery
SketchRepairPresentationSnapshotQuery::with_priority(SketchRepairDisplayPriority priority) {
  SketchRepairPresentationSnapshotQuery query;
  query.set_priority_filter(priority);
  return query;
}

void SketchRepairPresentationSnapshotQuery::set_category_filter(
    SketchRepairDisplayCategory category) {
  category_filter_ = category;
}

void SketchRepairPresentationSnapshotQuery::clear_category_filter() noexcept {
  category_filter_.reset();
}

void SketchRepairPresentationSnapshotQuery::set_priority_filter(
    SketchRepairDisplayPriority priority) {
  priority_filter_ = priority;
}

void SketchRepairPresentationSnapshotQuery::clear_priority_filter() noexcept {
  priority_filter_.reset();
}

void SketchRepairPresentationSnapshotQuery::set_latest_only(bool latest_only) noexcept {
  latest_only_ = latest_only;
}

void SketchRepairPresentationSnapshotQuery::set_undoable_only(bool undoable_only) noexcept {
  undoable_only_ = undoable_only;
}

const std::optional<SketchRepairDisplayCategory>&
SketchRepairPresentationSnapshotQuery::category_filter() const noexcept {
  return category_filter_;
}

const std::optional<SketchRepairDisplayPriority>&
SketchRepairPresentationSnapshotQuery::priority_filter() const noexcept {
  return priority_filter_;
}

bool SketchRepairPresentationSnapshotQuery::latest_only() const noexcept {
  return latest_only_;
}

bool SketchRepairPresentationSnapshotQuery::undoable_only() const noexcept {
  return undoable_only_;
}

bool SketchRepairPresentationSnapshotQuery::matches(
    const SketchRepairPresentationSnapshotEntry& entry) const noexcept {
  if (category_filter_.has_value() && entry.category() != category_filter_.value())
    return false;
  if (priority_filter_.has_value() && entry.priority() != priority_filter_.value())
    return false;
  if (latest_only_ && !entry.latest())
    return false;
  if (undoable_only_ && !entry.undoable())
    return false;
  return true;
}

SketchRepairPresentationSnapshotQueryCounts::SketchRepairPresentationSnapshotQueryCounts(
    std::size_t safe_apply_count, std::size_t requires_user_choice_count,
    std::size_t requires_parameter_value_count, std::size_t undo_entry_count,
    std::size_t low_priority_count, std::size_t normal_priority_count,
    std::size_t high_priority_count)
    : safe_apply_count_(safe_apply_count), requires_user_choice_count_(requires_user_choice_count),
      requires_parameter_value_count_(requires_parameter_value_count),
      undo_entry_count_(undo_entry_count), low_priority_count_(low_priority_count),
      normal_priority_count_(normal_priority_count), high_priority_count_(high_priority_count) {}

std::size_t SketchRepairPresentationSnapshotQueryCounts::category_count(
    SketchRepairDisplayCategory category) const noexcept {
  switch (category) {
  case SketchRepairDisplayCategory::SafeApply:
    return safe_apply_count_;
  case SketchRepairDisplayCategory::RequiresUserChoice:
    return requires_user_choice_count_;
  case SketchRepairDisplayCategory::RequiresParameterValue:
    return requires_parameter_value_count_;
  case SketchRepairDisplayCategory::UndoEntry:
    return undo_entry_count_;
  }
  return 0U;
}

std::size_t SketchRepairPresentationSnapshotQueryCounts::priority_count(
    SketchRepairDisplayPriority priority) const noexcept {
  switch (priority) {
  case SketchRepairDisplayPriority::Low:
    return low_priority_count_;
  case SketchRepairDisplayPriority::Normal:
    return normal_priority_count_;
  case SketchRepairDisplayPriority::High:
    return high_priority_count_;
  }
  return 0U;
}

std::size_t SketchRepairPresentationSnapshotQueryCounts::total_count() const noexcept {
  return safe_apply_count_ + requires_user_choice_count_ + requires_parameter_value_count_ +
         undo_entry_count_;
}

SketchRepairPresentationSnapshotQueryResult::SketchRepairPresentationSnapshotQueryResult(
    SketchRepairPresentationSnapshotQuery query, SketchRepairPresentationSnapshot snapshot,
    SketchRepairPresentationSnapshotQueryCounts counts)
    : query_(std::move(query)), snapshot_(std::move(snapshot)), counts_(counts) {}

const SketchRepairPresentationSnapshotQuery&
SketchRepairPresentationSnapshotQueryResult::query() const noexcept {
  return query_;
}

const SketchRepairPresentationSnapshot&
SketchRepairPresentationSnapshotQueryResult::snapshot() const noexcept {
  return snapshot_;
}

const SketchRepairPresentationSnapshotQueryCounts&
SketchRepairPresentationSnapshotQueryResult::counts() const noexcept {
  return counts_;
}

SketchRepairPresentationSnapshotQueryResult SketchRepairPresentationSnapshotQueryEngine::execute(
    const SketchRepairPresentationSnapshot& snapshot,
    const SketchRepairPresentationSnapshotQuery& query) const {
  std::vector<SketchRepairPresentationSnapshotEntry> entries;
  entries.reserve(snapshot.entries().size());
  for (const auto& entry : snapshot.entries()) {
    if (query.matches(entry))
      entries.push_back(entry);
  }

  SketchRepairPresentationSnapshot filtered_snapshot(std::move(entries));
  const auto counts = compute_counts(filtered_snapshot);
  return SketchRepairPresentationSnapshotQueryResult(query, std::move(filtered_snapshot), counts);
}

Result<std::string> serialize_sketch_repair_presentation_snapshot_query_result_to_json(
    const SketchRepairPresentationSnapshotQueryResult& result) {
  json entries = json::array();
  for (const auto& entry : result.snapshot().entries())
    entries.push_back(entry_to_json(entry));

  json root{{"schema", "blcad.sketch_repair_presentation_snapshot_query.debug"},
            {"version", 1},
            {"query", query_to_json(result.query())},
            {"entry_count", result.snapshot().entry_count()},
            {"empty", result.snapshot().empty()},
            {"category_counts", category_counts_to_json(result.counts())},
            {"priority_counts", priority_counts_to_json(result.counts())},
            {"entries", std::move(entries)}};
  return Result<std::string>::success(root.dump(2));
}

} // namespace blcad
