#pragma once

#include "blcad/gui/gui_types.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

using GuiCommandEnablement = std::function<bool(const GuiCommandContext&)>;

struct GuiCommandDefinition {
  std::string id;
  std::string label;
  GuiCommandEnablement enabled_when;
};

class GuiCommandRegistry {
public:
  [[nodiscard]] bool register_command(GuiCommandDefinition command);
  [[nodiscard]] const GuiCommandDefinition* find(std::string_view id) const noexcept;
  [[nodiscard]] bool is_enabled(std::string_view id,
                                const GuiCommandContext& context) const noexcept;
  [[nodiscard]] const std::vector<GuiCommandDefinition>& commands() const noexcept;

private:
  std::vector<GuiCommandDefinition> commands_;
};

} // namespace blcad::gui
