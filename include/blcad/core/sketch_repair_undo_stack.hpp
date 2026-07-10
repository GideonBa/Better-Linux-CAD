#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_repair_transactions.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class SketchRepairUndoStackStatus { Pushed, RejectedNotUndoable, Undone, Empty };
[[nodiscard]] std::string_view to_string(SketchRepairUndoStackStatus status) noexcept;

class SketchRepairUndoStackResult {
public:
  SketchRepairUndoStackResult(SketchRepairUndoStackStatus status,
                              SketchRepairTransactionStatus transaction_status,
                              std::string message, std::size_t remaining_stack_size,
                              std::vector<SketchConstraintId> restored_constraint_ids,
                              std::vector<SketchDimensionId> restored_dimension_ids,
                              std::vector<SketchConstraintId> removed_constraint_ids);

  [[nodiscard]] SketchRepairUndoStackStatus status() const noexcept;
  [[nodiscard]] SketchRepairTransactionStatus transaction_status() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;
  [[nodiscard]] std::size_t remaining_stack_size() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& restored_constraint_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchDimensionId>& restored_dimension_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& removed_constraint_ids() const noexcept;
  [[nodiscard]] bool pushed() const noexcept;
  [[nodiscard]] bool undone() const noexcept;

private:
  SketchRepairUndoStackStatus status_;
  SketchRepairTransactionStatus transaction_status_;
  std::string message_;
  std::size_t remaining_stack_size_;
  std::vector<SketchConstraintId> restored_constraint_ids_;
  std::vector<SketchDimensionId> restored_dimension_ids_;
  std::vector<SketchConstraintId> removed_constraint_ids_;
};

class SketchRepairUndoStack {
public:
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const std::vector<SketchRepairTransaction>& transactions() const noexcept;

  [[nodiscard]] Result<SketchRepairUndoStackResult> push(SketchRepairTransaction transaction);
  [[nodiscard]] Result<SketchRepairUndoStackResult> undo_latest(Sketch& sketch);
  void clear() noexcept;

private:
  std::vector<SketchRepairTransaction> transactions_;
};

} // namespace blcad
