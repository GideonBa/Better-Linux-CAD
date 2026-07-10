#include "blcad/core/sketch_repair_commands.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] SketchRepairCommandResult skipped(std::string message) {
  return SketchRepairCommandResult(SketchRepairCommandStatus::SkippedUnsupported,
                                   std::move(message), {}, {});
}

[[nodiscard]] std::string endpoint_key(const SketchEntityId& id, SketchReferenceTargetKind kind) {
  const char* suffix = kind == SketchReferenceTargetKind::LineSegmentStart ? ":start" : ":end";
  return id.value() + suffix;
}

[[nodiscard]] std::string target_key(const SketchReferenceTarget& target) {
  return std::string(to_string(target.kind())) + ":" + target.entity().value();
}

[[nodiscard]] std::string dimension_target_key(const SketchDrivingDimension& dimension) {
  std::string first = target_key(dimension.first_target());
  std::string second = target_key(dimension.second_target());
  if (second < first) std::swap(first, second);
  return first + "|" + second;
}

[[nodiscard]] bool is_fixed_endpoint_constraint(const SketchGeometricConstraint& constraint) noexcept {
  return constraint.kind() == SketchGeometricConstraintKind::Fixed &&
         (constraint.first_target().kind() == SketchReferenceTargetKind::LineSegmentStart ||
          constraint.first_target().kind() == SketchReferenceTargetKind::LineSegmentEnd);
}

[[nodiscard]] std::string fixed_endpoint_target(const SketchGeometricConstraint& constraint) {
  return endpoint_key(constraint.first_target().entity(), constraint.first_target().kind());
}

[[nodiscard]] std::string deterministic_fixed_id_for(std::string target) {
  for (char& character : target) {
    if (character == ':' || character == '|' || character == '/') character = '.';
  }
  return "repair.fixed." + target;
}

[[nodiscard]] Result<SketchReferenceTarget> endpoint_target_from_string(const std::string& target) {
  const auto separator = target.rfind(':');
  if (separator == std::string::npos || separator == 0U || separator + 1U >= target.size()) {
    return Result<SketchReferenceTarget>::failure(
        validation_error(target, "repair command endpoint target must have form <line-id>:start or <line-id>:end"));
  }

  const SketchEntityId entity(target.substr(0, separator));
  const std::string suffix = target.substr(separator + 1U);
  if (suffix == "start") return SketchReferenceTarget::create_line_segment_start(entity);
  if (suffix == "end") return SketchReferenceTarget::create_line_segment_end(entity);

  return Result<SketchReferenceTarget>::failure(
      validation_error(target, "repair command endpoint target must end with start or end"));
}

[[nodiscard]] std::vector<SketchConstraintId> duplicate_fixed_constraint_ids_to_remove(
    const Sketch& sketch, const std::string& target) {
  std::vector<SketchConstraintId> ids;
  for (const auto& constraint : sketch.geometric_constraints()) {
    if (is_fixed_endpoint_constraint(constraint) && fixed_endpoint_target(constraint) == target) {
      ids.push_back(constraint.id());
    }
  }
  std::sort(ids.begin(), ids.end(), [](const SketchConstraintId& left, const SketchConstraintId& right) {
    return left.value() < right.value();
  });
  if (ids.size() <= 1U) return {};
  return std::vector<SketchConstraintId>(ids.begin() + 1, ids.end());
}

[[nodiscard]] std::vector<SketchDimensionId> duplicate_dimension_ids_to_remove(
    const Sketch& sketch, const std::string& target) {
  std::vector<SketchDimensionId> ids;
  for (const auto& dimension : sketch.driving_dimensions()) {
    if (dimension_target_key(dimension) == target) ids.push_back(dimension.id());
  }
  std::sort(ids.begin(), ids.end(), [](const SketchDimensionId& left, const SketchDimensionId& right) {
    return left.value() < right.value();
  });
  if (ids.size() <= 1U) return {};
  return std::vector<SketchDimensionId>(ids.begin() + 1, ids.end());
}

[[nodiscard]] Result<SketchRepairCommandResult> apply_add_fixed_endpoint(Sketch& sketch,
                                                                         const std::string& target) {
  auto endpoint = endpoint_target_from_string(target);
  if (endpoint.has_error()) return Result<SketchRepairCommandResult>::failure(endpoint.error());

  if (sketch.find_line_segment(endpoint.value().entity()) == nullptr) {
    return Result<SketchRepairCommandResult>::failure(
        validation_error(target, "repair command endpoint line must exist in sketch"));
  }

  const SketchConstraintId id(deterministic_fixed_id_for(target));
  if (sketch.find_geometric_constraint(id) != nullptr || sketch.find_constraint(id) != nullptr) {
    return Result<SketchRepairCommandResult>::success(
        skipped("deterministic fixed endpoint constraint already exists"));
  }

  auto constraint = SketchGeometricConstraint::create_fixed(id, endpoint.value());
  if (constraint.has_error()) return Result<SketchRepairCommandResult>::failure(constraint.error());
  auto added = sketch.add_constraint(constraint.value());
  if (added.has_error()) return Result<SketchRepairCommandResult>::failure(added.error());

  return Result<SketchRepairCommandResult>::success(SketchRepairCommandResult(
      SketchRepairCommandStatus::Applied, "added deterministic fixed endpoint constraint", {id}, {}));
}

[[nodiscard]] Result<SketchRepairCommandResult> apply_remove_duplicate_fixed(Sketch& sketch,
                                                                             const std::string& target) {
  const auto to_remove = duplicate_fixed_constraint_ids_to_remove(sketch, target);
  if (to_remove.empty()) {
    return Result<SketchRepairCommandResult>::success(
        skipped("no duplicate fixed endpoint constraints remain for this target"));
  }

  std::vector<SketchConstraintId> removed;
  for (const auto& id : to_remove) {
    auto result = sketch.remove_geometric_constraint(id);
    if (result.has_error()) return Result<SketchRepairCommandResult>::failure(result.error());
    removed.push_back(id);
  }

  return Result<SketchRepairCommandResult>::success(SketchRepairCommandResult(
      SketchRepairCommandStatus::Applied, "removed duplicate fixed endpoint constraints",
      std::move(removed), {}));
}

[[nodiscard]] Result<SketchRepairCommandResult> apply_remove_duplicate_dimension(Sketch& sketch,
                                                                                 const std::string& target) {
  const auto to_remove = duplicate_dimension_ids_to_remove(sketch, target);
  if (to_remove.empty()) {
    return Result<SketchRepairCommandResult>::success(
        skipped("no duplicate driving dimensions remain for this target"));
  }

  std::vector<SketchDimensionId> removed;
  for (const auto& id : to_remove) {
    auto result = sketch.remove_driving_dimension(id);
    if (result.has_error()) return Result<SketchRepairCommandResult>::failure(result.error());
    removed.push_back(id);
  }

  return Result<SketchRepairCommandResult>::success(SketchRepairCommandResult(
      SketchRepairCommandStatus::Applied, "removed duplicate driving dimensions", {},
      std::move(removed)));
}

} // namespace

std::string_view to_string(SketchRepairCommandStatus status) noexcept {
  switch (status) {
  case SketchRepairCommandStatus::Applied: return "applied";
  case SketchRepairCommandStatus::SkippedUnsupported: return "skipped_unsupported";
  }
  return "skipped_unsupported";
}

SketchRepairCommand::SketchRepairCommand(SketchRepairSuggestion suggestion)
    : suggestion_(std::move(suggestion)) {}

const SketchRepairSuggestion& SketchRepairCommand::suggestion() const noexcept { return suggestion_; }
SketchRepairSuggestionAction SketchRepairCommand::action() const noexcept { return suggestion_.action(); }
const std::string& SketchRepairCommand::target() const noexcept { return suggestion_.target(); }

SketchRepairCommandResult::SketchRepairCommandResult(
    SketchRepairCommandStatus status, std::string message,
    std::vector<SketchConstraintId> changed_constraint_ids,
    std::vector<SketchDimensionId> changed_dimension_ids)
    : status_(status), message_(std::move(message)),
      changed_constraint_ids_(std::move(changed_constraint_ids)),
      changed_dimension_ids_(std::move(changed_dimension_ids)) {}

SketchRepairCommandStatus SketchRepairCommandResult::status() const noexcept { return status_; }
const std::string& SketchRepairCommandResult::message() const noexcept { return message_; }
const std::vector<SketchConstraintId>& SketchRepairCommandResult::changed_constraint_ids() const noexcept {
  return changed_constraint_ids_;
}
const std::vector<SketchDimensionId>& SketchRepairCommandResult::changed_dimension_ids() const noexcept {
  return changed_dimension_ids_;
}
bool SketchRepairCommandResult::applied() const noexcept {
  return status_ == SketchRepairCommandStatus::Applied;
}

Result<SketchRepairCommandResult> SketchRepairCommandExecutor::apply(
    Sketch& sketch, const SketchRepairCommand& command) const {
  switch (command.action()) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return apply_add_fixed_endpoint(sketch, command.target());
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return apply_remove_duplicate_fixed(sketch, command.target());
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return apply_remove_duplicate_dimension(sketch, command.target());
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return Result<SketchRepairCommandResult>::success(
        skipped("repair suggestion is not part of the safe explicit command subset"));
  }
  return Result<SketchRepairCommandResult>::success(skipped("unsupported repair command"));
}

} // namespace blcad
