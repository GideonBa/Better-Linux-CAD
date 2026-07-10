#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include "blcad/core/sketch_repair_command_labels.hpp"
#include "blcad/core/sketch_repair_presentation_metadata.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace blcad {
namespace {

using json = nlohmann::json;

[[nodiscard]] std::vector<SketchConstraintId> added_constraint_ids_for(
    const SketchRepairTransaction& transaction) {
  std::vector<SketchConstraintId> ids;
  ids.reserve(transaction.added_geometric_constraints().size());
  for (const auto& constraint : transaction.added_geometric_constraints()) ids.push_back(constraint.id());
  return ids;
}

[[nodiscard]] std::vector<SketchConstraintId> removed_constraint_ids_for(
    const SketchRepairTransaction& transaction) {
  std::vector<SketchConstraintId> ids;
  ids.reserve(transaction.removed_geometric_constraints().size());
  for (const auto& constraint : transaction.removed_geometric_constraints()) ids.push_back(constraint.id());
  return ids;
}

[[nodiscard]] std::vector<SketchDimensionId> removed_dimension_ids_for(
    const SketchRepairTransaction& transaction) {
  std::vector<SketchDimensionId> ids;
  ids.reserve(transaction.removed_driving_dimensions().size());
  for (const auto& dimension : transaction.removed_driving_dimensions()) ids.push_back(dimension.id());
  return ids;
}

[[nodiscard]] json constraint_ids_to_json(const std::vector<SketchConstraintId>& ids) {
  json values = json::array();
  for (const auto& id : ids) values.push_back(id.value());
  return values;
}

[[nodiscard]] json dimension_ids_to_json(const std::vector<SketchDimensionId>& ids) {
  json values = json::array();
  for (const auto& id : ids) values.push_back(id.value());
  return values;
}

[[nodiscard]] json affected_counts_to_json(const SketchRepairAffectedCounts& counts) {
  return json{{"added_constraints", counts.added_constraints()},
              {"removed_constraints", counts.removed_constraints()},
              {"removed_dimensions", counts.removed_dimensions()},
              {"total", counts.total()}};
}

} // namespace

SketchRepairUndoStackSummaryEntry::SketchRepairUndoStackSummaryEntry(
    std::size_t index, bool latest, SketchRepairTransactionStatus transaction_status,
    SketchRepairSuggestionAction action, std::string target, bool undoable,
    std::vector<SketchConstraintId> added_constraint_ids,
    std::vector<SketchConstraintId> removed_constraint_ids,
    std::vector<SketchDimensionId> removed_dimension_ids)
    : index_(index), latest_(latest), transaction_status_(transaction_status), action_(action),
      target_(std::move(target)), undoable_(undoable),
      added_constraint_ids_(std::move(added_constraint_ids)),
      removed_constraint_ids_(std::move(removed_constraint_ids)),
      removed_dimension_ids_(std::move(removed_dimension_ids)) {}

std::size_t SketchRepairUndoStackSummaryEntry::index() const noexcept { return index_; }
bool SketchRepairUndoStackSummaryEntry::latest() const noexcept { return latest_; }
SketchRepairTransactionStatus SketchRepairUndoStackSummaryEntry::transaction_status() const noexcept {
  return transaction_status_;
}
SketchRepairSuggestionAction SketchRepairUndoStackSummaryEntry::action() const noexcept { return action_; }
const std::string& SketchRepairUndoStackSummaryEntry::target() const noexcept { return target_; }
bool SketchRepairUndoStackSummaryEntry::undoable() const noexcept { return undoable_; }
const std::vector<SketchConstraintId>&
SketchRepairUndoStackSummaryEntry::added_constraint_ids() const noexcept {
  return added_constraint_ids_;
}
const std::vector<SketchConstraintId>&
SketchRepairUndoStackSummaryEntry::removed_constraint_ids() const noexcept {
  return removed_constraint_ids_;
}
const std::vector<SketchDimensionId>&
SketchRepairUndoStackSummaryEntry::removed_dimension_ids() const noexcept {
  return removed_dimension_ids_;
}

SketchRepairUndoStackSummary::SketchRepairUndoStackSummary(
    std::vector<SketchRepairUndoStackSummaryEntry> entries)
    : entries_(std::move(entries)) {}

const std::vector<SketchRepairUndoStackSummaryEntry>&
SketchRepairUndoStackSummary::entries() const noexcept {
  return entries_;
}
std::size_t SketchRepairUndoStackSummary::stack_size() const noexcept { return entries_.size(); }
bool SketchRepairUndoStackSummary::empty() const noexcept { return entries_.empty(); }
const SketchRepairUndoStackSummaryEntry* SketchRepairUndoStackSummary::latest() const noexcept {
  if (entries_.empty()) return nullptr;
  return &entries_.back();
}

SketchRepairUndoStackSummary SketchRepairUndoStackSummarizer::summarize(
    const SketchRepairUndoStack& stack) const {
  std::vector<SketchRepairUndoStackSummaryEntry> entries;
  entries.reserve(stack.transactions().size());
  for (std::size_t index = 0; index < stack.transactions().size(); ++index) {
    const auto& transaction = stack.transactions()[index];
    entries.emplace_back(index, index + 1U == stack.transactions().size(), transaction.status(),
                         transaction.command().action(), transaction.command().target(),
                         transaction.undoable(), added_constraint_ids_for(transaction),
                         removed_constraint_ids_for(transaction), removed_dimension_ids_for(transaction));
  }
  return SketchRepairUndoStackSummary(std::move(entries));
}

Result<std::string> serialize_sketch_repair_undo_stack_summary_to_json(
    const SketchRepairUndoStackSummary& summary) {
  const SketchRepairCommandLabeler labeler;
  const SketchRepairPresentationMetadataProvider metadata_provider;
  json entries = json::array();
  for (const auto& entry : summary.entries()) {
    const auto label = labeler.label_for(entry);
    const auto metadata = metadata_provider.metadata_for(entry);
    entries.push_back(json{{"index", entry.index()},
                           {"latest", entry.latest()},
                           {"transaction_status", std::string(to_string(entry.transaction_status()))},
                           {"action", std::string(to_string(entry.action()))},
                           {"target", entry.target()},
                           {"undoable", entry.undoable()},
                           {"title", label.title()},
                           {"description", label.description()},
                           {"label_id", metadata.label_id()},
                           {"display_category", std::string(to_string(metadata.category()))},
                           {"display_priority", std::string(to_string(metadata.priority()))},
                           {"affected_counts", affected_counts_to_json(metadata.affected_counts())},
                           {"affected_summary", metadata.affected_summary()},
                           {"added_constraint_ids", constraint_ids_to_json(entry.added_constraint_ids())},
                           {"removed_constraint_ids", constraint_ids_to_json(entry.removed_constraint_ids())},
                           {"removed_dimension_ids", dimension_ids_to_json(entry.removed_dimension_ids())}});
  }

  json root{{"schema", "blcad.sketch_repair_undo_stack_summary.debug"},
            {"version", 1},
            {"stack_size", summary.stack_size()},
            {"empty", summary.empty()},
            {"entries", std::move(entries)}};
  return Result<std::string>::success(root.dump(2));
}

} // namespace blcad
