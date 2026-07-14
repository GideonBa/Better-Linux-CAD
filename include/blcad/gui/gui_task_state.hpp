#pragma once

#include "blcad/gui/gui_types.hpp"

#include <string>
#include <string_view>

namespace blcad::gui {

class GuiTaskState {
public:
  [[nodiscard]] bool begin(std::string command_id);
  [[nodiscard]] bool begin_parameter_editing() noexcept;
  [[nodiscard]] bool show_preview() noexcept;
  [[nodiscard]] bool return_to_editing() noexcept;
  [[nodiscard]] bool apply() noexcept;
  [[nodiscard]] bool cancel() noexcept;

  [[nodiscard]] GuiTaskStage stage() const noexcept;
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] std::string_view command_id() const noexcept;

private:
  void reset() noexcept;

  GuiTaskStage stage_{GuiTaskStage::Inactive};
  std::string command_id_;
};

} // namespace blcad::gui
