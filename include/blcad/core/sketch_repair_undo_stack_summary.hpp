#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch_repair_undo_stack.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace blcad {

class SketchRepairUndoStackSummaryEntry {
public:
  SketchRepairUndoStackSummaryEntry(std::size_t index, bool latest,
                                    SketchRepairTransactionStatus transaction_status,
                                    SketchRepairSuggestionAction action, std::string target,
                                    bool undoable,
                                    std::vector<SketchConstraintId> added_constraint_ids,
                                    std::vector<SketchConstraintId> removed_constraint_ids,
                                    std::vector<SketchDimensionId> removed_dimension_ids);

  [[nodiscard]] std::size_t index() const noexcept;
  [[nodiscard]] bool latest() const noexcept;
  [[nodiscard]] SketchRepairTransactionStatus transaction_status() const noexcept;
  [[nodiscard]] SketchRepairSuggestionAction action() const noexcept;
  [[nodiscard]] const std::string& target() const noexcept;
  [[nodiscard]] bool undoable() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& added_constraint_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& removed_constraint_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchDimensionId>& removed_dimension_ids() const noexcept;

private:
  std::size_t index_;
  bool latest_;
  SketchRepairTransactionStatus transaction_status_;
  SketchRepairSuggestionAction action_;
  std::string target_;
  bool undoable_;
  std::vector<SketchConstraintId> added_constraint_ids_;
  std::vector<SketchConstraintId> removed_constraint_ids_;
  std::vector<SketchDimensionId> removed_dimension_ids_;
};

class SketchRepairUndoStackSummary {
public:
  explicit SketchRepairUndoStackSummary(std::vector<SketchRepairUndoStackSummaryEntry> entries);

  [[nodiscard]] const std::vector<SketchRepairUndoStackSummaryEntry>& entries() const noexcept;
  [[nodiscard]] std::size_t stack_size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const SketchRepairUndoStackSummaryEntry* latest() const noexcept;

private:
  std::vector<SketchRepairUndoStackSummaryEntry> entries_;
};

class SketchRepairUndoStackSummarizer {
public:
  [[nodiscard]] SketchRepairUndoStackSummary summarize(const SketchRepairUndoStack& stack) const;
};

[[nodiscard]] Result<std::string>
serialize_sketch_repair_undo_stack_summary_to_json(const SketchRepairUndoStackSummary& summary);

} // namespace blcad
