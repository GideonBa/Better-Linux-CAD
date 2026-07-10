#include "blcad/core/sketch_repair_presentation_snapshot.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace blcad {
namespace {

using json = nlohmann::json;

[[nodiscard]] json affected_counts_to_json(const SketchRepairAffectedCounts& counts) {
  return json{{"added_constraints", counts.added_constraints()},
              {"removed_constraints", counts.removed_constraints()},
              {"removed_dimensions", counts.removed_dimensions()},
              {"total", counts.total()}};
}

} // namespace

SketchRepairPresentationSnapshotEntry::SketchRepairPresentationSnapshotEntry(
    std::size_t index, bool latest, SketchRepairTransactionStatus transaction_status,
    SketchRepairSuggestionAction action, std::string target, bool undoable, std::string title,
    std::string description, std::string label_id, SketchRepairDisplayCategory category,
    SketchRepairDisplayPriority priority, SketchRepairAffectedCounts affected_counts,
    std::string affected_summary)
    : index_(index), latest_(latest), transaction_status_(transaction_status), action_(action),
      target_(std::move(target)), undoable_(undoable), title_(std::move(title)),
      description_(std::move(description)), label_id_(std::move(label_id)), category_(category),
      priority_(priority), affected_counts_(affected_counts),
      affected_summary_(std::move(affected_summary)) {}

std::size_t SketchRepairPresentationSnapshotEntry::index() const noexcept {
  return index_;
}
bool SketchRepairPresentationSnapshotEntry::latest() const noexcept {
  return latest_;
}
SketchRepairTransactionStatus
SketchRepairPresentationSnapshotEntry::transaction_status() const noexcept {
  return transaction_status_;
}
SketchRepairSuggestionAction SketchRepairPresentationSnapshotEntry::action() const noexcept {
  return action_;
}
const std::string& SketchRepairPresentationSnapshotEntry::target() const noexcept {
  return target_;
}
bool SketchRepairPresentationSnapshotEntry::undoable() const noexcept {
  return undoable_;
}
const std::string& SketchRepairPresentationSnapshotEntry::title() const noexcept {
  return title_;
}
const std::string& SketchRepairPresentationSnapshotEntry::description() const noexcept {
  return description_;
}
const std::string& SketchRepairPresentationSnapshotEntry::label_id() const noexcept {
  return label_id_;
}
SketchRepairDisplayCategory SketchRepairPresentationSnapshotEntry::category() const noexcept {
  return category_;
}
SketchRepairDisplayPriority SketchRepairPresentationSnapshotEntry::priority() const noexcept {
  return priority_;
}
const SketchRepairAffectedCounts&
SketchRepairPresentationSnapshotEntry::affected_counts() const noexcept {
  return affected_counts_;
}
const std::string& SketchRepairPresentationSnapshotEntry::affected_summary() const noexcept {
  return affected_summary_;
}

SketchRepairPresentationSnapshot::SketchRepairPresentationSnapshot(
    std::vector<SketchRepairPresentationSnapshotEntry> entries)
    : entries_(std::move(entries)) {}

const std::vector<SketchRepairPresentationSnapshotEntry>&
SketchRepairPresentationSnapshot::entries() const noexcept {
  return entries_;
}
std::size_t SketchRepairPresentationSnapshot::entry_count() const noexcept {
  return entries_.size();
}
bool SketchRepairPresentationSnapshot::empty() const noexcept {
  return entries_.empty();
}
const SketchRepairPresentationSnapshotEntry*
SketchRepairPresentationSnapshot::latest() const noexcept {
  if (entries_.empty())
    return nullptr;
  return &entries_.back();
}

SketchRepairPresentationSnapshot
SketchRepairPresentationSnapshotBuilder::build(const SketchRepairUndoStackSummary& summary) const {
  const SketchRepairCommandLabeler labeler;
  const SketchRepairPresentationMetadataProvider metadata_provider;
  std::vector<SketchRepairPresentationSnapshotEntry> entries;
  entries.reserve(summary.entries().size());
  for (const auto& entry : summary.entries()) {
    const auto label = labeler.label_for(entry);
    const auto metadata = metadata_provider.metadata_for(entry);
    entries.emplace_back(entry.index(), entry.latest(), entry.transaction_status(), entry.action(),
                         entry.target(), entry.undoable(), label.title(), label.description(),
                         metadata.label_id(), metadata.category(), metadata.priority(),
                         metadata.affected_counts(), metadata.affected_summary());
  }
  return SketchRepairPresentationSnapshot(std::move(entries));
}

Result<std::string> serialize_sketch_repair_presentation_snapshot_to_json(
    const SketchRepairPresentationSnapshot& snapshot) {
  json entries = json::array();
  for (const auto& entry : snapshot.entries()) {
    entries.push_back(
        json{{"index", entry.index()},
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
             {"affected_summary", entry.affected_summary()}});
  }

  json root{{"schema", "blcad.sketch_repair_presentation_snapshot.debug"},
            {"version", 1},
            {"entry_count", snapshot.entry_count()},
            {"empty", snapshot.empty()},
            {"entries", std::move(entries)}};
  return Result<std::string>::success(root.dump(2));
}

} // namespace blcad
