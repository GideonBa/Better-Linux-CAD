#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class SketchRepairCommandStatus { Applied, SkippedUnsupported };
[[nodiscard]] std::string_view to_string(SketchRepairCommandStatus status) noexcept;

class SketchRepairCommand {
public:
  explicit SketchRepairCommand(SketchRepairSuggestion suggestion);

  [[nodiscard]] const SketchRepairSuggestion& suggestion() const noexcept;
  [[nodiscard]] SketchRepairSuggestionAction action() const noexcept;
  [[nodiscard]] const std::string& target() const noexcept;

private:
  SketchRepairSuggestion suggestion_;
};

class SketchRepairCommandResult {
public:
  SketchRepairCommandResult(SketchRepairCommandStatus status, std::string message,
                            std::vector<SketchConstraintId> changed_constraint_ids,
                            std::vector<SketchDimensionId> changed_dimension_ids);

  [[nodiscard]] SketchRepairCommandStatus status() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraintId>& changed_constraint_ids() const noexcept;
  [[nodiscard]] const std::vector<SketchDimensionId>& changed_dimension_ids() const noexcept;
  [[nodiscard]] bool applied() const noexcept;

private:
  SketchRepairCommandStatus status_;
  std::string message_;
  std::vector<SketchConstraintId> changed_constraint_ids_;
  std::vector<SketchDimensionId> changed_dimension_ids_;
};

class SketchRepairCommandExecutor {
public:
  [[nodiscard]] Result<SketchRepairCommandResult> apply(Sketch& sketch,
                                                        const SketchRepairCommand& command) const;
};

} // namespace blcad
