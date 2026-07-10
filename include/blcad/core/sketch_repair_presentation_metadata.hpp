#pragma once

#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/core/sketch_repair_undo_stack_summary.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace blcad {

enum class SketchRepairDisplayCategory {
  SafeApply,
  RequiresUserChoice,
  RequiresParameterValue,
  UndoEntry
};
[[nodiscard]] std::string_view to_string(SketchRepairDisplayCategory category) noexcept;

enum class SketchRepairDisplayPriority { Low, Normal, High };
[[nodiscard]] std::string_view to_string(SketchRepairDisplayPriority priority) noexcept;

class SketchRepairAffectedCounts {
public:
  SketchRepairAffectedCounts(std::size_t added_constraints,
                             std::size_t removed_constraints,
                             std::size_t removed_dimensions);

  [[nodiscard]] std::size_t added_constraints() const noexcept;
  [[nodiscard]] std::size_t removed_constraints() const noexcept;
  [[nodiscard]] std::size_t removed_dimensions() const noexcept;
  [[nodiscard]] std::size_t total() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

private:
  std::size_t added_constraints_;
  std::size_t removed_constraints_;
  std::size_t removed_dimensions_;
};

class SketchRepairPresentationMetadata {
public:
  SketchRepairPresentationMetadata(std::string label_id,
                                   SketchRepairDisplayCategory category,
                                   SketchRepairDisplayPriority priority,
                                   SketchRepairAffectedCounts affected_counts,
                                   std::string affected_summary);

  [[nodiscard]] const std::string& label_id() const noexcept;
  [[nodiscard]] SketchRepairDisplayCategory category() const noexcept;
  [[nodiscard]] SketchRepairDisplayPriority priority() const noexcept;
  [[nodiscard]] const SketchRepairAffectedCounts& affected_counts() const noexcept;
  [[nodiscard]] const std::string& affected_summary() const noexcept;

private:
  std::string label_id_;
  SketchRepairDisplayCategory category_;
  SketchRepairDisplayPriority priority_;
  SketchRepairAffectedCounts affected_counts_;
  std::string affected_summary_;
};

class SketchRepairPresentationMetadataProvider {
public:
  [[nodiscard]] SketchRepairPresentationMetadata metadata_for(
      SketchRepairSuggestionAction action) const;
  [[nodiscard]] SketchRepairPresentationMetadata metadata_for(
      const SketchRepairUndoStackSummaryEntry& entry) const;
};

} // namespace blcad
