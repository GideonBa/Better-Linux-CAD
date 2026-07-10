#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch_diagnostics.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class SketchRepairSuggestionAction {
  AddFixedEndpointConstraint,
  RemoveConflictingOrientationConstraint,
  RemoveDuplicateFixedEndpointConstraint,
  RemoveDuplicateDrivingDimension,
  AddDrivingDimension
};

[[nodiscard]] std::string_view to_string(SketchRepairSuggestionAction action) noexcept;

class SketchRepairSuggestion {
public:
  SketchRepairSuggestion(SketchRepairSuggestionAction action, std::string target,
                         SketchDiagnosticKind originating_diagnostic_kind,
                         std::string originating_diagnostic_target, std::string message);

  [[nodiscard]] SketchRepairSuggestionAction action() const noexcept;
  [[nodiscard]] const std::string& target() const noexcept;
  [[nodiscard]] SketchDiagnosticKind originating_diagnostic_kind() const noexcept;
  [[nodiscard]] const std::string& originating_diagnostic_target() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;

private:
  SketchRepairSuggestionAction action_;
  std::string target_;
  SketchDiagnosticKind originating_diagnostic_kind_;
  std::string originating_diagnostic_target_;
  std::string message_;
};

class SketchRepairSuggestionReport {
public:
  explicit SketchRepairSuggestionReport(SketchId sketch_id);

  [[nodiscard]] const SketchId& sketch_id() const noexcept;
  [[nodiscard]] const std::vector<SketchRepairSuggestion>& suggestions() const noexcept;
  [[nodiscard]] std::size_t suggestion_count() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  void add(SketchRepairSuggestion suggestion);

private:
  SketchId sketch_id_;
  std::vector<SketchRepairSuggestion> suggestions_;
};

class SketchRepairSuggester {
public:
  [[nodiscard]] SketchRepairSuggestionReport
  suggest(const SketchDiagnosticReport& diagnostics) const;
};

[[nodiscard]] Result<std::string>
serialize_sketch_repair_suggestion_report_to_json(const SketchRepairSuggestionReport& report);

} // namespace blcad
