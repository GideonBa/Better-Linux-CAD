#include "blcad/core/sketch_repair_suggestions.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace blcad {
namespace {

using json = nlohmann::json;

[[nodiscard]] std::string repair_message_for(const SketchConstraintDiagnostic& diagnostic,
                                             SketchRepairSuggestionAction action) {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "add an explicit fixed endpoint constraint for this currently unconstrained endpoint";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "remove either the horizontal or vertical constraint on this line before solving";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "remove duplicate fixed endpoint constraints and keep one deterministic fixed endpoint constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "remove duplicate driving dimensions targeting the same endpoint pair";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "add at least one driving dimension to make this profile sketch parametrically controlled";
  }
  return diagnostic.message();
}

[[nodiscard]] bool suggestion_for_diagnostic(const SketchConstraintDiagnostic& diagnostic,
                                             SketchRepairSuggestionAction& action) noexcept {
  switch (diagnostic.kind()) {
  case SketchDiagnosticKind::UnconstrainedLineEndpoint:
    action = SketchRepairSuggestionAction::AddFixedEndpointConstraint;
    return true;
  case SketchDiagnosticKind::ProfileWithoutDrivingDimensions:
    action = SketchRepairSuggestionAction::AddDrivingDimension;
    return true;
  case SketchDiagnosticKind::ConflictingHorizontalVerticalConstraints:
    action = SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint;
    return true;
  case SketchDiagnosticKind::DuplicateFixedEndpoint:
    action = SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint;
    return true;
  case SketchDiagnosticKind::DuplicateDrivingDimensionTarget:
    action = SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension;
    return true;
  case SketchDiagnosticKind::FreeSplineControlPoint:
  case SketchDiagnosticKind::RedundantOrientationConstraint:
    return false;
  }
  return false;
}

} // namespace

std::string_view to_string(SketchRepairSuggestionAction action) noexcept {
  switch (action) {
  case SketchRepairSuggestionAction::AddFixedEndpointConstraint:
    return "add_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint:
    return "remove_conflicting_orientation_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint:
    return "remove_duplicate_fixed_endpoint_constraint";
  case SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension:
    return "remove_duplicate_driving_dimension";
  case SketchRepairSuggestionAction::AddDrivingDimension:
    return "add_driving_dimension";
  }
  return "add_fixed_endpoint_constraint";
}

SketchRepairSuggestion::SketchRepairSuggestion(SketchRepairSuggestionAction action,
                                               std::string target,
                                               SketchDiagnosticKind originating_diagnostic_kind,
                                               std::string originating_diagnostic_target,
                                               std::string message)
    : action_(action), target_(std::move(target)),
      originating_diagnostic_kind_(originating_diagnostic_kind),
      originating_diagnostic_target_(std::move(originating_diagnostic_target)),
      message_(std::move(message)) {}

SketchRepairSuggestionAction SketchRepairSuggestion::action() const noexcept { return action_; }
const std::string& SketchRepairSuggestion::target() const noexcept { return target_; }
SketchDiagnosticKind SketchRepairSuggestion::originating_diagnostic_kind() const noexcept {
  return originating_diagnostic_kind_;
}
const std::string& SketchRepairSuggestion::originating_diagnostic_target() const noexcept {
  return originating_diagnostic_target_;
}
const std::string& SketchRepairSuggestion::message() const noexcept { return message_; }

SketchRepairSuggestionReport::SketchRepairSuggestionReport(SketchId sketch_id)
    : sketch_id_(std::move(sketch_id)) {}

const SketchId& SketchRepairSuggestionReport::sketch_id() const noexcept { return sketch_id_; }
const std::vector<SketchRepairSuggestion>& SketchRepairSuggestionReport::suggestions() const noexcept {
  return suggestions_;
}
std::size_t SketchRepairSuggestionReport::suggestion_count() const noexcept {
  return suggestions_.size();
}
bool SketchRepairSuggestionReport::empty() const noexcept { return suggestions_.empty(); }
void SketchRepairSuggestionReport::add(SketchRepairSuggestion suggestion) {
  suggestions_.push_back(std::move(suggestion));
}

SketchRepairSuggestionReport SketchRepairSuggester::suggest(
    const SketchDiagnosticReport& diagnostics) const {
  SketchRepairSuggestionReport report(diagnostics.sketch_id());
  for (const auto& diagnostic : diagnostics.diagnostics()) {
    SketchRepairSuggestionAction action{};
    if (!suggestion_for_diagnostic(diagnostic, action)) continue;
    report.add(SketchRepairSuggestion(action, diagnostic.target(), diagnostic.kind(), diagnostic.target(),
                                      repair_message_for(diagnostic, action)));
  }
  return report;
}

Result<std::string> serialize_sketch_repair_suggestion_report_to_json(
    const SketchRepairSuggestionReport& report) {
  json suggestions = json::array();
  for (const auto& suggestion : report.suggestions()) {
    suggestions.push_back(json{{"action", std::string(to_string(suggestion.action()))},
                               {"target", suggestion.target()},
                               {"originating_diagnostic_kind",
                                std::string(to_string(suggestion.originating_diagnostic_kind()))},
                               {"originating_diagnostic_target",
                                suggestion.originating_diagnostic_target()},
                               {"message", suggestion.message()},
                               {"mutates_model", false}});
  }

  json root{{"schema", "blcad.sketch_repair_suggestions.debug"},
            {"version", 1},
            {"sketch", report.sketch_id().value()},
            {"suggestion_count", report.suggestion_count()},
            {"suggestions", std::move(suggestions)}};
  return Result<std::string>::success(root.dump(2));
}

} // namespace blcad
