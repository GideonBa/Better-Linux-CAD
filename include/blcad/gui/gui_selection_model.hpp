#pragma once

#include "blcad/gui/gui_types.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace blcad::gui {

class GuiSelectionModel {
public:
  [[nodiscard]] bool add(GuiSelection selection);
  [[nodiscard]] bool remove(GuiSelectionKind kind, std::string_view semantic_id);
  void clear() noexcept;
  void set_filter_mask(std::uint32_t mask) noexcept;

  [[nodiscard]] const std::vector<GuiSelection>& entries() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::uint32_t kind_mask() const noexcept;
  [[nodiscard]] std::uint32_t filter_mask() const noexcept;
  [[nodiscard]] bool allows(GuiSelectionKind kind) const noexcept;
  [[nodiscard]] bool contains(GuiSelectionKind kind, std::string_view semantic_id) const noexcept;

private:
  std::vector<GuiSelection> entries_;
  std::uint32_t filter_mask_{0xFFFFFFFFU};
};

} // namespace blcad::gui
