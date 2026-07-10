#include "blcad/core/sketch_repair_presentation_metadata.hpp"

#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] std::string label_id_for_action(SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "repair.add_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "repair.remove_conflicting_orientation_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "repair.remove_duplicate_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "repair.remove_duplicate_driving_dimension";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "repair.add_driving_dimension";
  }
  return "repair.unknown";
}

[[nodiscard]] std::string undo_label_id_for_action(SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "undo.add_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "undo.remove_conflicting_orientation_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "undo.remove_duplicate_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "undo.remove_duplicate_driving_dimension";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "undo.add_driving_dimension";
  }
  return "undo.unknown";
}

[[nodiscard]] SketchRepairDisplayCategory category_for_action(
    SketchRepairSuggestionAction action) noexcept {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return SketchRepairDisplayCategory::SafeApply;
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return SketchRepairDisplayCategory::RequiresUserChoice;
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return SketchRepairDisplayCategory::RequiresParameterValue;
  }
  return SketchRepairDisplayCategory::RequiresUserChoice;
}

[[nodiscard]] SketchRepairDisplayPriority priority_for_action(
    SketchRepairSuggestionAction action) noexcept {
  switch (action) {
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return SketchRepairDisplayPriority::High;
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return SketchRepairDisplayPriority::Normal;
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return SketchRepairDisplayPriority::Low;
  }
  return SketchRepairDisplayPriority::Normal;
}

[[nodiscard]] std::string pluralize(std::size_t count, const char* singular,
                                    const char* plural) {
  return std::to_string(count) + " " + (count == 1U ? singular : plural);
}

[[nodiscard]] std::string affected_summary_for(const SketchRepairAffectedCounts& counts) {
  if (counts.empty()) return "No affected sketch records";

  std::string summary;
  if (counts.added_constraints() > 0U) {
    summary += pluralize(counts.added_constraints(), "constraint added", "constraints added");
  }
  if (counts.removed_constraints() > 0U) {
    if (!summary.empty()) summary += ", ";
    summary += pluralize(counts.removed_constraints(), "constraint removed", "constraints removed");
  }
  if (counts.removed_dimensions() > 0U) {
    if (!summary.empty()) summary += ", ";
    summary += pluralize(counts.removed_dimensions(), "dimension removed", "dimensions removed");
  }
  return summary;
}

} // namespace

std::string_view to_string(SketchRepairDisplayCategory category) noexcept {
  switch (category) {
  case SketchRepairDisplayCategory::SafeApply: return "safe_apply";
  case SketchRepairDisplayCategory::RequiresUserChoice: return "requires_user_choice";
  case SketchRepairDisplayCategory::RequiresParameterValue: return "requires_parameter_value";
  case SketchRepairDisplayCategory::UndoEntry: return "undo_entry";
  }
  return "requires_user_choice";
}

std::string_view to_string(SketchRepairDisplayPriority priority) noexcept {
  switch (priority) {
  case SketchRepairDisplayPriority::Low: return "low";
  case SketchRepairDisplayPriority::Normal: return "normal";
  case SketchRepairDisplayPriority::High: return "high";
  }
  return "normal";
}

SketchRepairAffectedCounts::SketchRepairAffectedCounts(std::size_t added_constraints,
                                                       std::size_t removed_constraints,
                                                       std::size_t removed_dimensions)
    : added_constraints_(added_constraints), removed_constraints_(removed_constraints),
      removed_dimensions_(removed_dimensions) {}

std::size_t SketchRepairAffectedCounts::added_constraints() const noexcept {
  return added_constraints_;
}
std::size_t SketchRepairAffectedCounts::removed_constraints() const noexcept {
  return removed_constraints_;
}
std::size_t SketchRepairAffectedCounts::removed_dimensions() const noexcept {
  return removed_dimensions_;
}
std::size_t SketchRepairAffectedCounts::total() const noexcept {
  return added_constraints_ + removed_constraints_ + removed_dimensions_;
}
bool SketchRepairAffectedCounts::empty() const noexcept { return total() == 0U; }

SketchRepairPresentationMetadata::SketchRepairPresentationMetadata(
    std::string label_id, SketchRepairDisplayCategory category,
    SketchRepairDisplayPriority priority, SketchRepairAffectedCounts affected_counts,
    std::string affected_summary)
    : label_id_(std::move(label_id)), category_(category), priority_(priority),
      affected_counts_(affected_counts), affected_summary_(std::move(affected_summary)) {}

const std::string& SketchRepairPresentationMetadata::label_id() const noexcept { return label_id_; }
SketchRepairDisplayCategory SketchRepairPresentationMetadata::category() const noexcept {
  return category_;
}
SketchRepairDisplayPriority SketchRepairPresentationMetadata::priority() const noexcept {
  return priority_;
}
const SketchRepairAffectedCounts& SketchRepairPresentationMetadata::affected_counts() const noexcept {
  return affected_counts_;
}
const std::string& SketchRepairPresentationMetadata::affected_summary() const noexcept {
  return affected_summary_;
}

SketchRepairPresentationMetadata SketchRepairPresentationMetadataProvider::metadata_for(
    SketchRepairSuggestionAction action) const {
  const SketchRepairAffectedCounts counts(0U, 0U, 0U);
  return SketchRepairPresentationMetadata(label_id_for_action(action), category_for_action(action),
                                          priority_for_action(action), counts,
                                          affected_summary_for(counts));
}

SketchRepairPresentationMetadata SketchRepairPresentationMetadataProvider::metadata_for(
    const SketchRepairUndoStackSummaryEntry& entry) const {
  const SketchRepairAffectedCounts counts(entry.added_constraint_ids().size(),
                                          entry.removed_constraint_ids().size(),
                                          entry.removed_dimension_ids().size());
  return SketchRepairPresentationMetadata(undo_label_id_for_action(entry.action()),
                                          SketchRepairDisplayCategory::UndoEntry,
                                          SketchRepairDisplayPriority::Normal, counts,
                                          affected_summary_for(counts));
}

} // namespace blcad
