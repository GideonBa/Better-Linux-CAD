#include "blcad/gui/gui_selection_model.hpp"

#include <algorithm>
#include <utility>

namespace blcad::gui {

bool GuiSelectionModel::add(GuiSelection selection) {
  if (selection.semantic_id.empty() || contains(selection.kind, selection.semantic_id)) {
    return false;
  }
  entries_.push_back(std::move(selection));
  return true;
}

bool GuiSelectionModel::remove(GuiSelectionKind kind, std::string_view semantic_id) {
  const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const GuiSelection& entry) {
    return entry.kind == kind && entry.semantic_id == semantic_id;
  });
  if (found == entries_.end()) {
    return false;
  }
  entries_.erase(found);
  return true;
}

void GuiSelectionModel::clear() noexcept {
  entries_.clear();
}

const std::vector<GuiSelection>& GuiSelectionModel::entries() const noexcept {
  return entries_;
}

std::size_t GuiSelectionModel::size() const noexcept {
  return entries_.size();
}

bool GuiSelectionModel::empty() const noexcept {
  return entries_.empty();
}

std::uint32_t GuiSelectionModel::kind_mask() const noexcept {
  std::uint32_t mask = 0;
  for (const auto& entry : entries_) {
    mask |= selection_kind_bit(entry.kind);
  }
  return mask;
}

bool GuiSelectionModel::contains(GuiSelectionKind kind,
                                 std::string_view semantic_id) const noexcept {
  return std::any_of(entries_.begin(), entries_.end(), [&](const GuiSelection& entry) {
    return entry.kind == kind && entry.semantic_id == semantic_id;
  });
}

} // namespace blcad::gui
