#include "blcad/gui/gui_task_state.hpp"

#include <utility>

namespace blcad::gui {

bool GuiTaskState::begin(std::string command_id) {
  if (active() || command_id.empty()) {
    return false;
  }
  command_id_ = std::move(command_id);
  stage_ = GuiTaskStage::CollectingSelection;
  return true;
}

bool GuiTaskState::begin_parameter_editing() noexcept {
  if (stage_ != GuiTaskStage::CollectingSelection) {
    return false;
  }
  stage_ = GuiTaskStage::EditingParameters;
  return true;
}

bool GuiTaskState::show_preview() noexcept {
  if (stage_ != GuiTaskStage::EditingParameters) {
    return false;
  }
  stage_ = GuiTaskStage::Preview;
  return true;
}

bool GuiTaskState::return_to_editing() noexcept {
  if (stage_ != GuiTaskStage::Preview) {
    return false;
  }
  stage_ = GuiTaskStage::EditingParameters;
  return true;
}

bool GuiTaskState::apply() noexcept {
  if (stage_ != GuiTaskStage::Preview) {
    return false;
  }
  reset();
  return true;
}

bool GuiTaskState::cancel() noexcept {
  if (!active()) {
    return false;
  }
  reset();
  return true;
}

GuiTaskStage GuiTaskState::stage() const noexcept {
  return stage_;
}

bool GuiTaskState::active() const noexcept {
  return stage_ != GuiTaskStage::Inactive;
}

std::string_view GuiTaskState::command_id() const noexcept {
  return command_id_;
}

void GuiTaskState::reset() noexcept {
  stage_ = GuiTaskStage::Inactive;
  command_id_.clear();
}

} // namespace blcad::gui
