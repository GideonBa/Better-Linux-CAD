#include "blcad/core/sketch_repair_transactions.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] const SketchGeometricConstraint*
find_constraint_in(const std::vector<SketchGeometricConstraint>& constraints,
                   const SketchConstraintId& id) noexcept {
  const auto found = std::find_if(constraints.begin(), constraints.end(),
                                  [&id](const auto& constraint) { return constraint.id() == id; });
  return found == constraints.end() ? nullptr : &*found;
}

[[nodiscard]] const SketchDrivingDimension*
find_dimension_in(const std::vector<SketchDrivingDimension>& dimensions,
                  const SketchDimensionId& id) noexcept {
  const auto found = std::find_if(dimensions.begin(), dimensions.end(),
                                  [&id](const auto& dimension) { return dimension.id() == id; });
  return found == dimensions.end() ? nullptr : &*found;
}

[[nodiscard]] std::vector<SketchGeometricConstraint>
capture_removed_constraints(const std::vector<SketchGeometricConstraint>& before,
                            const SketchRepairCommandResult& result) {
  std::vector<SketchGeometricConstraint> removed;
  for (const auto& id : result.changed_constraint_ids()) {
    if (const auto* constraint = find_constraint_in(before, id); constraint != nullptr) {
      removed.push_back(*constraint);
    }
  }
  return removed;
}

[[nodiscard]] std::vector<SketchGeometricConstraint>
capture_added_constraints(const Sketch& sketch,
                          const std::vector<SketchGeometricConstraint>& before,
                          const SketchRepairCommandResult& result) {
  std::vector<SketchGeometricConstraint> added;
  for (const auto& id : result.changed_constraint_ids()) {
    if (find_constraint_in(before, id) != nullptr)
      continue;
    if (const auto* constraint = sketch.find_geometric_constraint(id); constraint != nullptr) {
      added.push_back(*constraint);
    }
  }
  return added;
}

[[nodiscard]] std::vector<SketchDrivingDimension>
capture_removed_dimensions(const std::vector<SketchDrivingDimension>& before,
                           const SketchRepairCommandResult& result) {
  std::vector<SketchDrivingDimension> removed;
  for (const auto& id : result.changed_dimension_ids()) {
    if (const auto* dimension = find_dimension_in(before, id); dimension != nullptr) {
      removed.push_back(*dimension);
    }
  }
  return removed;
}

} // namespace

std::string_view to_string(SketchRepairTransactionStatus status) noexcept {
  switch (status) {
  case SketchRepairTransactionStatus::Applied:
    return "applied";
  case SketchRepairTransactionStatus::SkippedUnsupported:
    return "skipped_unsupported";
  case SketchRepairTransactionStatus::Undone:
    return "undone";
  }
  return "skipped_unsupported";
}

SketchRepairTransaction::SketchRepairTransaction(
    SketchRepairTransactionStatus status, SketchRepairCommand command,
    SketchRepairCommandResult command_result,
    std::vector<SketchGeometricConstraint> added_geometric_constraints,
    std::vector<SketchGeometricConstraint> removed_geometric_constraints,
    std::vector<SketchDrivingDimension> removed_driving_dimensions)
    : status_(status), command_(std::move(command)), command_result_(std::move(command_result)),
      added_geometric_constraints_(std::move(added_geometric_constraints)),
      removed_geometric_constraints_(std::move(removed_geometric_constraints)),
      removed_driving_dimensions_(std::move(removed_driving_dimensions)) {}

SketchRepairTransactionStatus SketchRepairTransaction::status() const noexcept {
  return status_;
}
const SketchRepairCommand& SketchRepairTransaction::command() const noexcept {
  return command_;
}
const SketchRepairCommandResult& SketchRepairTransaction::command_result() const noexcept {
  return command_result_;
}
const std::vector<SketchGeometricConstraint>&
SketchRepairTransaction::added_geometric_constraints() const noexcept {
  return added_geometric_constraints_;
}
const std::vector<SketchGeometricConstraint>&
SketchRepairTransaction::removed_geometric_constraints() const noexcept {
  return removed_geometric_constraints_;
}
const std::vector<SketchDrivingDimension>&
SketchRepairTransaction::removed_driving_dimensions() const noexcept {
  return removed_driving_dimensions_;
}
bool SketchRepairTransaction::applied() const noexcept {
  return status_ == SketchRepairTransactionStatus::Applied;
}
bool SketchRepairTransaction::undoable() const noexcept {
  return applied() &&
         (!added_geometric_constraints_.empty() || !removed_geometric_constraints_.empty() ||
          !removed_driving_dimensions_.empty());
}

SketchRepairTransactionUndoResult::SketchRepairTransactionUndoResult(
    SketchRepairTransactionStatus status, std::string message,
    std::vector<SketchConstraintId> restored_constraint_ids,
    std::vector<SketchDimensionId> restored_dimension_ids,
    std::vector<SketchConstraintId> removed_constraint_ids)
    : status_(status), message_(std::move(message)),
      restored_constraint_ids_(std::move(restored_constraint_ids)),
      restored_dimension_ids_(std::move(restored_dimension_ids)),
      removed_constraint_ids_(std::move(removed_constraint_ids)) {}

SketchRepairTransactionStatus SketchRepairTransactionUndoResult::status() const noexcept {
  return status_;
}
const std::string& SketchRepairTransactionUndoResult::message() const noexcept {
  return message_;
}
const std::vector<SketchConstraintId>&
SketchRepairTransactionUndoResult::restored_constraint_ids() const noexcept {
  return restored_constraint_ids_;
}
const std::vector<SketchDimensionId>&
SketchRepairTransactionUndoResult::restored_dimension_ids() const noexcept {
  return restored_dimension_ids_;
}
const std::vector<SketchConstraintId>&
SketchRepairTransactionUndoResult::removed_constraint_ids() const noexcept {
  return removed_constraint_ids_;
}
bool SketchRepairTransactionUndoResult::undone() const noexcept {
  return status_ == SketchRepairTransactionStatus::Undone;
}

Result<SketchRepairTransaction>
SketchRepairTransactionExecutor::apply(Sketch& sketch, const SketchRepairCommand& command) const {
  const auto constraints_before = sketch.geometric_constraints();
  const auto dimensions_before = sketch.driving_dimensions();

  const SketchRepairCommandExecutor command_executor;
  auto result = command_executor.apply(sketch, command);
  if (result.has_error())
    return Result<SketchRepairTransaction>::failure(result.error());

  if (!result.value().applied()) {
    return Result<SketchRepairTransaction>::success(SketchRepairTransaction(
        SketchRepairTransactionStatus::SkippedUnsupported, command, result.value(), {}, {}, {}));
  }

  return Result<SketchRepairTransaction>::success(
      SketchRepairTransaction(SketchRepairTransactionStatus::Applied, command, result.value(),
                              capture_added_constraints(sketch, constraints_before, result.value()),
                              capture_removed_constraints(constraints_before, result.value()),
                              capture_removed_dimensions(dimensions_before, result.value())));
}

Result<SketchRepairTransactionUndoResult>
SketchRepairTransactionExecutor::undo(Sketch& sketch,
                                      const SketchRepairTransaction& transaction) const {
  if (!transaction.undoable()) {
    return Result<SketchRepairTransactionUndoResult>::success(
        SketchRepairTransactionUndoResult(SketchRepairTransactionStatus::SkippedUnsupported,
                                          "repair transaction is not undoable", {}, {}, {}));
  }

  std::vector<SketchConstraintId> removed_added_constraints;
  for (const auto& constraint : transaction.added_geometric_constraints()) {
    auto removed = sketch.remove_geometric_constraint(constraint.id());
    if (removed.has_error())
      return Result<SketchRepairTransactionUndoResult>::failure(removed.error());
    removed_added_constraints.push_back(constraint.id());
  }

  std::vector<SketchConstraintId> restored_constraints;
  for (const auto& constraint : transaction.removed_geometric_constraints()) {
    if (sketch.find_geometric_constraint(constraint.id()) != nullptr) {
      return Result<SketchRepairTransactionUndoResult>::failure(
          validation_error(constraint.id().value(),
                           "cannot undo repair transaction because constraint id already exists"));
    }
    auto restored = sketch.add_constraint(constraint);
    if (restored.has_error())
      return Result<SketchRepairTransactionUndoResult>::failure(restored.error());
    restored_constraints.push_back(constraint.id());
  }

  std::vector<SketchDimensionId> restored_dimensions;
  for (const auto& dimension : transaction.removed_driving_dimensions()) {
    if (sketch.find_driving_dimension(dimension.id()) != nullptr) {
      return Result<SketchRepairTransactionUndoResult>::failure(
          validation_error(dimension.id().value(),
                           "cannot undo repair transaction because dimension id already exists"));
    }
    auto restored = sketch.add_dimension(dimension);
    if (restored.has_error())
      return Result<SketchRepairTransactionUndoResult>::failure(restored.error());
    restored_dimensions.push_back(dimension.id());
  }

  return Result<SketchRepairTransactionUndoResult>::success(SketchRepairTransactionUndoResult(
      SketchRepairTransactionStatus::Undone, "repair transaction undone",
      std::move(restored_constraints), std::move(restored_dimensions),
      std::move(removed_added_constraints)));
}

} // namespace blcad
