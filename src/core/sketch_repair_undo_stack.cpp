#include "blcad/core/sketch_repair_undo_stack.hpp"

#include <string>
#include <utility>

namespace blcad {

std::string_view to_string(SketchRepairUndoStackStatus status) noexcept {
  switch (status) {
  case SketchRepairUndoStackStatus::Pushed:
    return "pushed";
  case SketchRepairUndoStackStatus::RejectedNotUndoable:
    return "rejected_not_undoable";
  case SketchRepairUndoStackStatus::Undone:
    return "undone";
  case SketchRepairUndoStackStatus::Empty:
    return "empty";
  }
  return "empty";
}

SketchRepairUndoStackResult::SketchRepairUndoStackResult(
    SketchRepairUndoStackStatus status, SketchRepairTransactionStatus transaction_status,
    std::string message, std::size_t remaining_stack_size,
    std::vector<SketchConstraintId> restored_constraint_ids,
    std::vector<SketchDimensionId> restored_dimension_ids,
    std::vector<SketchConstraintId> removed_constraint_ids)
    : status_(status), transaction_status_(transaction_status), message_(std::move(message)),
      remaining_stack_size_(remaining_stack_size),
      restored_constraint_ids_(std::move(restored_constraint_ids)),
      restored_dimension_ids_(std::move(restored_dimension_ids)),
      removed_constraint_ids_(std::move(removed_constraint_ids)) {}

SketchRepairUndoStackStatus SketchRepairUndoStackResult::status() const noexcept {
  return status_;
}
SketchRepairTransactionStatus SketchRepairUndoStackResult::transaction_status() const noexcept {
  return transaction_status_;
}
const std::string& SketchRepairUndoStackResult::message() const noexcept {
  return message_;
}
std::size_t SketchRepairUndoStackResult::remaining_stack_size() const noexcept {
  return remaining_stack_size_;
}
const std::vector<SketchConstraintId>&
SketchRepairUndoStackResult::restored_constraint_ids() const noexcept {
  return restored_constraint_ids_;
}
const std::vector<SketchDimensionId>&
SketchRepairUndoStackResult::restored_dimension_ids() const noexcept {
  return restored_dimension_ids_;
}
const std::vector<SketchConstraintId>&
SketchRepairUndoStackResult::removed_constraint_ids() const noexcept {
  return removed_constraint_ids_;
}
bool SketchRepairUndoStackResult::pushed() const noexcept {
  return status_ == SketchRepairUndoStackStatus::Pushed;
}
bool SketchRepairUndoStackResult::undone() const noexcept {
  return status_ == SketchRepairUndoStackStatus::Undone;
}

bool SketchRepairUndoStack::empty() const noexcept {
  return transactions_.empty();
}
std::size_t SketchRepairUndoStack::size() const noexcept {
  return transactions_.size();
}
const std::vector<SketchRepairTransaction>& SketchRepairUndoStack::transactions() const noexcept {
  return transactions_;
}

Result<SketchRepairUndoStackResult>
SketchRepairUndoStack::push(SketchRepairTransaction transaction) {
  if (!transaction.undoable()) {
    return Result<SketchRepairUndoStackResult>::success(SketchRepairUndoStackResult(
        SketchRepairUndoStackStatus::RejectedNotUndoable, transaction.status(),
        "repair transaction is not undoable", transactions_.size(), {}, {}, {}));
  }

  const SketchRepairTransactionStatus transaction_status = transaction.status();
  transactions_.push_back(std::move(transaction));
  return Result<SketchRepairUndoStackResult>::success(
      SketchRepairUndoStackResult(SketchRepairUndoStackStatus::Pushed, transaction_status,
                                  "repair transaction pushed", transactions_.size(), {}, {}, {}));
}

Result<SketchRepairUndoStackResult> SketchRepairUndoStack::undo_latest(Sketch& sketch) {
  if (transactions_.empty()) {
    return Result<SketchRepairUndoStackResult>::success(SketchRepairUndoStackResult(
        SketchRepairUndoStackStatus::Empty, SketchRepairTransactionStatus::SkippedUnsupported,
        "repair undo stack is empty", 0U, {}, {}, {}));
  }

  const SketchRepairTransactionExecutor executor;
  auto undo = executor.undo(sketch, transactions_.back());
  if (undo.has_error())
    return Result<SketchRepairUndoStackResult>::failure(undo.error());

  if (undo.value().undone()) {
    transactions_.pop_back();
    return Result<SketchRepairUndoStackResult>::success(SketchRepairUndoStackResult(
        SketchRepairUndoStackStatus::Undone, undo.value().status(), undo.value().message(),
        transactions_.size(), undo.value().restored_constraint_ids(),
        undo.value().restored_dimension_ids(), undo.value().removed_constraint_ids()));
  }

  return Result<SketchRepairUndoStackResult>::success(SketchRepairUndoStackResult(
      SketchRepairUndoStackStatus::RejectedNotUndoable, undo.value().status(),
      undo.value().message(), transactions_.size(), undo.value().restored_constraint_ids(),
      undo.value().restored_dimension_ids(), undo.value().removed_constraint_ids()));
}

void SketchRepairUndoStack::clear() noexcept {
  transactions_.clear();
}

} // namespace blcad
