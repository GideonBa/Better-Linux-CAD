#include "blcad/gui/gui_command_registry.hpp"

#include <algorithm>
#include <utility>

namespace blcad::gui {

bool GuiCommandRegistry::register_command(GuiCommandDefinition command) {
  if (command.id.empty() || command.label.empty() || !command.enabled_when || find(command.id)) {
    return false;
  }
  commands_.push_back(std::move(command));
  return true;
}

const GuiCommandDefinition* GuiCommandRegistry::find(std::string_view id) const noexcept {
  const auto found = std::find_if(commands_.begin(), commands_.end(),
                                  [&](const auto& command) { return command.id == id; });
  return found == commands_.end() ? nullptr : &*found;
}

bool GuiCommandRegistry::is_enabled(std::string_view id,
                                    const GuiCommandContext& context) const noexcept {
  const auto* command = find(id);
  return command != nullptr && command->enabled_when(context);
}

const std::vector<GuiCommandDefinition>& GuiCommandRegistry::commands() const noexcept {
  return commands_;
}

} // namespace blcad::gui
