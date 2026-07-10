#include "blcad/core/sketch_repair_command_labels.hpp"

#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] SketchRepairCommandLabel label_for_action(SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return SketchRepairCommandLabel(
        "Add fixed endpoint constraint",
        "Add a deterministic fixed constraint to the currently unconstrained sketch endpoint.");
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return SketchRepairCommandLabel(
        "Resolve horizontal/vertical conflict",
        "Choose which conflicting orientation constraint should be removed before solving.");
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return SketchRepairCommandLabel(
        "Remove duplicate fixed endpoint constraints",
        "Remove duplicate fixed endpoint constraints while keeping the deterministic first record.");
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return SketchRepairCommandLabel(
        "Remove duplicate driving dimension",
        "Remove duplicate driving dimensions that target the same sketch endpoint pair.");
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return SketchRepairCommandLabel(
        "Add driving dimension",
        "Add a driving dimension once parameter creation and a user-supplied value are available.");
  }
  return SketchRepairCommandLabel("Repair sketch", "Apply a supported sketch repair action.");
}

[[nodiscard]] std::string undo_title_for(SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "Undo add fixed endpoint constraint";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "Undo resolve horizontal/vertical conflict";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "Undo remove duplicate fixed endpoint constraints";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "Undo remove duplicate driving dimension";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "Undo add driving dimension";
  }
  return "Undo sketch repair";
}

[[nodiscard]] std::string undo_description_for(const SketchRepairUndoStackSummaryEntry& entry) {
  switch (entry.action()) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "Remove the fixed endpoint constraint that the repair command added for target " +
           entry.target() + ".";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "Restore the orientation-conflict repair state for target " + entry.target() + ".";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "Restore the duplicate fixed endpoint constraints that were removed for target " +
           entry.target() + ".";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "Restore the duplicate driving dimensions that were removed for target " + entry.target() +
           ".";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "Remove the driving dimension added for target " + entry.target() + ".";
  }
  return "Undo the sketch repair transaction for target " + entry.target() + ".";
}

} // namespace

SketchRepairCommandLabel::SketchRepairCommandLabel(std::string title, std::string description)
    : title_(std::move(title)), description_(std::move(description)) {}

const std::string& SketchRepairCommandLabel::title() const noexcept { return title_; }
const std::string& SketchRepairCommandLabel::description() const noexcept { return description_; }

SketchRepairCommandLabel SketchRepairCommandLabeler::label_for(
    SketchRepairSuggestionAction action) const {
  return label_for_action(action);
}

SketchRepairCommandLabel SketchRepairCommandLabeler::label_for(
    const SketchRepairUndoStackSummaryEntry& entry) const {
  return SketchRepairCommandLabel(undo_title_for(entry.action()), undo_description_for(entry));
}

} // namespace blcad
