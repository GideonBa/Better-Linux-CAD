#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/sketch_repair_presentation_snapshot.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace blcad {

class SketchRepairPresentationSnapshotQuery {
public:
  SketchRepairPresentationSnapshotQuery();

  [[nodiscard]] static SketchRepairPresentationSnapshotQuery all();
  [[nodiscard]] static SketchRepairPresentationSnapshotQuery
  with_category(SketchRepairDisplayCategory category);
  [[nodiscard]] static SketchRepairPresentationSnapshotQuery
  with_priority(SketchRepairDisplayPriority priority);

  void set_category_filter(SketchRepairDisplayCategory category);
  void clear_category_filter() noexcept;
  void set_priority_filter(SketchRepairDisplayPriority priority);
  void clear_priority_filter() noexcept;
  void set_latest_only(bool latest_only) noexcept;
  void set_undoable_only(bool undoable_only) noexcept;

  [[nodiscard]] const std::optional<SketchRepairDisplayCategory>& category_filter() const noexcept;
  [[nodiscard]] const std::optional<SketchRepairDisplayPriority>& priority_filter() const noexcept;
  [[nodiscard]] bool latest_only() const noexcept;
  [[nodiscard]] bool undoable_only() const noexcept;
  [[nodiscard]] bool matches(const SketchRepairPresentationSnapshotEntry& entry) const noexcept;

private:
  std::optional<SketchRepairDisplayCategory> category_filter_;
  std::optional<SketchRepairDisplayPriority> priority_filter_;
  bool latest_only_;
  bool undoable_only_;
};

class SketchRepairPresentationSnapshotQueryCounts {
public:
  SketchRepairPresentationSnapshotQueryCounts(std::size_t safe_apply_count,
                                              std::size_t requires_user_choice_count,
                                              std::size_t requires_parameter_value_count,
                                              std::size_t undo_entry_count,
                                              std::size_t low_priority_count,
                                              std::size_t normal_priority_count,
                                              std::size_t high_priority_count);

  [[nodiscard]] std::size_t category_count(SketchRepairDisplayCategory category) const noexcept;
  [[nodiscard]] std::size_t priority_count(SketchRepairDisplayPriority priority) const noexcept;
  [[nodiscard]] std::size_t total_count() const noexcept;

private:
  std::size_t safe_apply_count_;
  std::size_t requires_user_choice_count_;
  std::size_t requires_parameter_value_count_;
  std::size_t undo_entry_count_;
  std::size_t low_priority_count_;
  std::size_t normal_priority_count_;
  std::size_t high_priority_count_;
};

class SketchRepairPresentationSnapshotQueryResult {
public:
  SketchRepairPresentationSnapshotQueryResult(SketchRepairPresentationSnapshotQuery query,
                                              SketchRepairPresentationSnapshot snapshot,
                                              SketchRepairPresentationSnapshotQueryCounts counts);

  [[nodiscard]] const SketchRepairPresentationSnapshotQuery& query() const noexcept;
  [[nodiscard]] const SketchRepairPresentationSnapshot& snapshot() const noexcept;
  [[nodiscard]] const SketchRepairPresentationSnapshotQueryCounts& counts() const noexcept;

private:
  SketchRepairPresentationSnapshotQuery query_;
  SketchRepairPresentationSnapshot snapshot_;
  SketchRepairPresentationSnapshotQueryCounts counts_;
};

class SketchRepairPresentationSnapshotQueryEngine {
public:
  [[nodiscard]] SketchRepairPresentationSnapshotQueryResult
  execute(const SketchRepairPresentationSnapshot& snapshot,
          const SketchRepairPresentationSnapshotQuery& query) const;
};

[[nodiscard]] Result<std::string>
serialize_sketch_repair_presentation_snapshot_query_result_to_json(
    const SketchRepairPresentationSnapshotQueryResult& result);

} // namespace blcad
