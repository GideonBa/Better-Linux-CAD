#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch_repair_command_labels.hpp"
#include "blcad/core/sketch_repair_presentation_metadata.hpp"
#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad {

class SketchRepairPresentationSnapshotEntry {
public:
  SketchRepairPresentationSnapshotEntry(std::size_t index,
                                        bool latest,
                                        SketchRepairTransactionStatus transaction_status,
                                        SketchRepairSuggestionAction action,
                                        std::string target,
                                        bool undoable,
                                        std::string title,
                                        std::string description,
                                        std::string label_id,
                                        SketchRepairDisplayCategory category,
                                        SketchRepairDisplayPriority priority,
                                        SketchRepairAffectedCounts affected_counts,
                                        std::string affected_summary);

  [[nodiscard]] std::size_t index() const noexcept;
  [[nodiscard]] bool latest() const noexcept;
  [[nodiscard]] SketchRepairTransactionStatus transaction_status() const noexcept;
  [[nodiscard]] SketchRepairSuggestionAction action() const noexcept;
  [[nodiscard]] const std::string& target() const noexcept;
  [[nodiscard]] bool undoable() const noexcept;
  [[nodiscard]] const std::string& title() const noexcept;
  [[nodiscard]] const std::string& description() const noexcept;
  [[nodiscard]] const std::string& label_id() const noexcept;
  [[nodiscard]] SketchRepairDisplayCategory category() const noexcept;
  [[nodiscard]] SketchRepairDisplayPriority priority() const noexcept;
  [[nodiscard]] const SketchRepairAffectedCounts& affected_counts() const noexcept;
  [[nodiscard]] const std::string& affected_summary() const noexcept;

private:
  std::size_t index_;
  bool latest_;
  SketchRepairTransactionStatus transaction_status_;
  SketchRepairSuggestionAction action_;
  std::string target_;
  bool undoable_;
  std::string title_;
  std::string description_;
  std::string label_id_;
  SketchRepairDisplayCategory category_;
  SketchRepairDisplayPriority priority_;
  SketchRepairAffectedCounts affected_counts_;
  std::string affected_summary_;
};

class SketchRepairPresentationSnapshot {
public:
  explicit SketchRepairPresentationSnapshot(std::vector<SketchRepairPresentationSnapshotEntry> entries);

  [[nodiscard]] const std::vector<SketchRepairPresentationSnapshotEntry>& entries() const noexcept;
  [[nodiscard]] std::size_t entry_count() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const SketchRepairPresentationSnapshotEntry* latest() const noexcept;

private:
  std::vector<SketchRepairPresentationSnapshotEntry> entries_;
};

class SketchRepairPresentationSnapshotBuilder {
public:
  [[nodiscard]] SketchRepairPresentationSnapshot build(
      const SketchRepairUndoStackSummary& summary) const;
};

[[nodiscard]] Result<std::string> serialize_sketch_repair_presentation_snapshot_to_json(
    const SketchRepairPresentationSnapshot& snapshot);

} // namespace blcad
