#include "blcad/gui/gui_document_session.hpp"

namespace blcad::gui {
namespace {
Error error() {
  return Error::validation("gui.assembly.representation",
                           "Apply or cancel the active task and return to the root Assembly before "
                           "switching representations");
}
} // namespace
Result<std::size_t> GuiDocumentSession::activate_assembly_position(AssemblyPositionId id) {
  if (!project() || project_part_context_active() || task_.active() || part_workflow_base_ ||
      history_prefix_count_ || reference_repair_mode_)
    return Result<std::size_t>::failure(error());
  if (id == active_position_ && derived_results_fresh())
    return Result<std::size_t>::success(0);
  auto candidate = *this;
  if (id != master_position_id() && !project()->find_assembly_position(id))
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "Named position does not exist"));
  candidate.active_position_ = std::move(id);
  auto checked = candidate.rebuild_model_state_view(active_model_state_);
  if (!checked)
    return checked;
  *this = std::move(candidate);
  return checked;
}
Result<std::size_t> GuiDocumentSession::activate_assembly_design_view(AssemblyDesignViewId id) {
  if (!project() || project_part_context_active() || task_.active() || part_workflow_base_ ||
      history_prefix_count_ || reference_repair_mode_)
    return Result<std::size_t>::failure(error());
  if (id == active_design_view_ && derived_results_fresh())
    return Result<std::size_t>::success(0);
  auto candidate = *this;
  if (id != default_design_view_id() && !project()->find_assembly_design_view(id))
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "Named design view does not exist"));
  candidate.active_design_view_ = std::move(id);
  auto checked = candidate.rebuild_model_state_view(active_model_state_);
  if (!checked)
    return checked;
  *this = std::move(candidate);
  return checked;
}
Result<std::size_t>
GuiDocumentSession::commit_assembly_representations(std::vector<AssemblyPosition> positions,
                                                    std::vector<AssemblyDesignView> views,
                                                    std::string label) {
  if (!project() || project_part_context_active() || part_workflow_base_ || reference_repair_mode_)
    return Result<std::size_t>::failure(error());
  auto candidate = *this;
  const auto state = active_model_state_;
  const auto position = active_position_;
  const auto view = active_design_view_;
  candidate.active_model_state_ = primary_model_state_id();
  candidate.active_position_ = master_position_id();
  candidate.active_design_view_ = default_design_view_id();
  candidate.effective_project_.reset();
  auto changed = candidate.commit_project_transaction(std::move(label), [&](Project& root) {
    auto a = root.replace_assembly_positions(positions);
    if (!a)
      return a;
    auto b = root.replace_assembly_design_views(views);
    if (!b)
      return b;
    return Result<std::size_t>::success(a.value() + b.value());
  });
  if (!changed)
    return changed;
  candidate.active_position_ =
      candidate.project()->find_assembly_position(position) ? position : master_position_id();
  candidate.active_design_view_ =
      candidate.project()->find_assembly_design_view(view) ? view : default_design_view_id();
  auto restored = candidate.rebuild_model_state_view(state);
  if (!restored)
    return restored;
  *this = std::move(candidate);
  return changed;
}
} // namespace blcad::gui
