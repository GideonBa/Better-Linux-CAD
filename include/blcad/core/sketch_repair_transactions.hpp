#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_repair_commands.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class SketchRepairTransactionStatus { Applied, SkippedUnsupported, Undone };
[[nodiscard]] std::string_view to_string(SketchRepairTransactionStatus status) noexcept;

class SketchRepairTransaction {
public:
  SketchRepairTransaction(SketchRepairTransactionStatus status, SketchRepairCommand command,
                          SketchRepairCommandResult command_result,
                          std::vector<SketchGeometricConstraint> added_geometric_constraints,
                          std::vector<SketchGeometricConstraint> removed_geometric_constraints,
                          std::vector<SketchDrivingDimension> removed_driving_dimensions);

  [[nodiscard]] SketchRepairTransactionStatus status() const noexcept;
  [[nodiscard]] const SketchRepairCommand& command() const noexcept;
  [[nodiscard]] const SketchRepairCommandResult& command_result() const noexcept;
  [[nodiscard]] const std::vector<SketchGeometricConstraint>&
  added_geometric_constraints() const noexcept;
  [[nodiscard]] const std::vector<SketchGeometricConstraint>&
  removed_geometric_constraints() const noexcept;
  [[nodiscard]] const std::vector<SketchDrivingDimension>&
  removed_driving_dimensions() const noexcept;
  [[nodiscard]] bool applied() const noexcept;
  [[nodiscard]] bool undoable() const noexcept;

private:
  SketchRepairTransactionStatus status_;
  SketchRepairCommand command_;
  SketchRepairCommandResult command_result_;
  std::vector<SketchGeometricConstraint> added_geometric_constraints_;
  std::vector<SketchGeometricConstraint> removed_geometric_constraints_;
  std::vector<SketchDrivingDimension> removed_driving_dimensions_;
};

class SketchRepairTransactionUndoResult {
public:
  SketchRepairTransactionUndoResult(SketchRepairTransactionStatus status, std::string message,
                                    std::vector<SketchConstraintId> restored_constraint_ids,
                                    std::vector<SketchDimensionId> restored_dimension_ids,
                                    std::vector<SketchConstraintId> removed_constraint_ids);

  [[nodiscard]] SketchRepairTransactionStatus status() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& restored_constraint_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchDimensionId>& restored_dimension_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& removed_constraint_ids() const noexcept;
  [[nodiscard]] bool undone() const noexcept;

private:
  SketchRepairTransactionStatus status_;
  std::string message_;
  std::vector<SketchConstraintId> restored_constraint_ids_;
  std::vector<SketchDimensionId> restored_dimension_ids_;
  std::vector<SketchConstraintId> removed_constraint_ids_;
};

class SketchRepairTransactionExecutor {
public:
  [[nodiscard]] Result<SketchRepairTransaction> apply(Sketch& sketch,
                                                      const SketchRepairCommand& command) const;
  [[nodiscard]] Result<SketchRepairTransactionUndoResult>
  undo(Sketch& sketch, const SketchRepairTransaction& transaction) const;
};

} // namespace blcad
