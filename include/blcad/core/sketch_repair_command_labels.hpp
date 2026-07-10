#pragma once

#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include <string>

namespace blcad {

class SketchRepairCommandLabel {
public:
  SketchRepairCommandLabel(std::string title, std::string description);

  [[nodiscard]] const std::string& title() const noexcept;
  [[nodiscard]] const std::string& description() const noexcept;

private:
  std::string title_;
  std::string description_;
};

class SketchRepairCommandLabeler {
public:
  [[nodiscard]] SketchRepairCommandLabel label_for(SketchRepairSuggestionAction action) const;
  [[nodiscard]] SketchRepairCommandLabel
  label_for(const SketchRepairUndoStackSummaryEntry& entry) const;
};

} // namespace blcad
