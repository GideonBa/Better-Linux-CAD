#pragma once

#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_interactive_pattern_body.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include <utility>
#include <vector>

namespace blcad::gui {

// Block 124/125/126 coordinator that connects the interactive Extrude, Revolve,
// Fillet/Chamfer, Shell/Draft, Pattern/Mirror, and Body Boolean/Transform
// controllers to the Block-123 candidate-only manipulator layer. It is
// deliberately Qt-free: MainWindow owns one instance and routes the manipulator
// shell's candidate callback into on_candidate(...). No document authority lives
// here; every mutation still flows through a controller's one Apply transaction.
// At most one controller is active.
class GuiInteractiveFeatureCoordinator {
public:
  GuiInteractiveFeatureCoordinator(GuiDocumentSession* session,
                                   GuiViewportManipulatorLayer* layer) noexcept
      : session_(session), layer_(layer) {}

  [[nodiscard]] GuiInteractiveExtrudeController& extrude() noexcept { return extrude_; }
  [[nodiscard]] GuiInteractiveRevolveController& revolve() noexcept { return revolve_; }
  [[nodiscard]] GuiInteractiveFinishingController& finishing() noexcept { return finishing_; }
  [[nodiscard]] GuiInteractiveShellDraftController& shell_draft() noexcept { return shell_draft_; }
  [[nodiscard]] GuiInteractivePatternMirrorController& pattern_mirror() noexcept {
    return pattern_mirror_;
  }
  [[nodiscard]] GuiInteractiveBodyOperationController& body_operation() noexcept {
    return body_operation_;
  }
  [[nodiscard]] bool active() const noexcept {
    return extrude_.active() || revolve_.active() || finishing_.active() || shell_draft_.active() ||
           pattern_mirror_.active() || body_operation_.active();
  }

  // Re-publish the active controller's handles to the shared manipulator layer.
  // A caller that begins a finishing/shell-draft command through the exposed
  // controller accessors calls this to seed the handles.
  void refresh_handles() { publish_handles(); }

  [[nodiscard]] Result<std::size_t>
  begin_extrude(SketchId sketch, ProfileId profile, FeatureId feature, std::string name,
                ParameterId distance_parameter, BodyId result_body,
                GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_features", "no active session"));
    auto begun = extrude_.begin(*session_, std::move(sketch), std::move(profile),
                                std::move(feature), std::move(name),
                                std::move(distance_parameter), std::move(result_body));
    if (begun.has_error()) return begun;
    extrude_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  [[nodiscard]] Result<std::size_t>
  begin_revolve(SketchId sketch, ProfileId profile, FeatureId feature, std::string name,
                AxisReference axis, BodyId result_body, GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_features", "no active session"));
    auto begun = revolve_.begin(*session_, std::move(sketch), std::move(profile),
                                std::move(feature), std::move(name), std::move(axis),
                                std::move(result_body));
    if (begun.has_error()) return begun;
    revolve_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  // Fold a manipulator candidate into the active controller. Handles are only
  // re-seeded once the drag has ended (empty active handle) so the layer is
  // never reset mid-drag.
  void on_candidate(const GuiViewportManipulatorCandidate& candidate) {
    bool changed = false;
    if (extrude_.active()) changed = extrude_.apply_manipulator(candidate);
    else if (revolve_.active()) changed = revolve_.apply_manipulator(candidate);
    else if (finishing_.active()) changed = finishing_.apply_manipulator(candidate);
    else if (shell_draft_.active()) changed = shell_draft_.apply_manipulator(candidate);
    else if (pattern_mirror_.active()) changed = pattern_mirror_.apply_manipulator(candidate);
    else if (body_operation_.active()) changed = body_operation_.apply_manipulator(candidate);
    if (changed && layer_ != nullptr && layer_->active_handle_id().empty()) publish_handles();
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview() const {
    if (session_ == nullptr)
      return Result<GuiPartFeaturePreview>::failure(
          Error::validation("gui.interactive_features", "no active session"));
    if (extrude_.active()) return extrude_.preview(*session_);
    if (revolve_.active()) return revolve_.preview(*session_);
    if (finishing_.active()) return finishing_.preview(*session_);
    if (shell_draft_.active()) return shell_draft_.preview(*session_);
    if (pattern_mirror_.active()) return pattern_mirror_.preview(*session_);
    if (body_operation_.active()) return body_operation_.preview(*session_);
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_features", "no interactive feature is active"));
  }

  [[nodiscard]] Result<std::size_t> apply() {
    if (session_ == nullptr)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_features", "no active session"));
    Result<std::size_t> applied = Result<std::size_t>::failure(
        Error::validation("gui.interactive_features", "no interactive feature is active"));
    if (extrude_.active()) applied = extrude_.apply(*session_);
    else if (revolve_.active()) applied = revolve_.apply(*session_);
    else if (finishing_.active()) applied = finishing_.apply(*session_);
    else if (shell_draft_.active()) applied = shell_draft_.apply(*session_);
    else if (pattern_mirror_.active()) applied = pattern_mirror_.apply(*session_);
    else if (body_operation_.active()) applied = body_operation_.apply(*session_);
    if (!applied.has_error()) clear_handles();
    return applied;
  }

  void cancel() noexcept {
    extrude_.cancel();
    revolve_.cancel();
    finishing_.cancel();
    shell_draft_.cancel();
    pattern_mirror_.cancel();
    body_operation_.cancel();
    clear_handles();
  }

private:
  void publish_handles() {
    if (layer_ == nullptr) return;
    std::vector<GuiViewportManipulatorHandle> handles;
    if (extrude_.active()) {
      if (auto handle = extrude_.extent_handle()) handles.push_back(std::move(*handle));
      if (auto handle = extrude_.taper_handle()) handles.push_back(std::move(*handle));
      if (auto handle = extrude_.thin_handle()) handles.push_back(std::move(*handle));
    } else if (revolve_.active()) {
      if (auto handle = revolve_.angle_handle()) handles.push_back(std::move(*handle));
    } else if (finishing_.active()) {
      handles = finishing_.handles();
    } else if (shell_draft_.active()) {
      handles = shell_draft_.handles();
    } else if (pattern_mirror_.active()) {
      handles = pattern_mirror_.handles();
    } else if (body_operation_.active()) {
      handles = body_operation_.handles();
    }
    (void)layer_->set_handles(std::move(handles));
  }

  void clear_handles() noexcept {
    if (layer_ != nullptr) layer_->clear_handles();
  }

  GuiDocumentSession* session_{nullptr};
  GuiViewportManipulatorLayer* layer_{nullptr};
  GuiInteractiveExtrudeController extrude_;
  GuiInteractiveRevolveController revolve_;
  GuiInteractiveFinishingController finishing_;
  GuiInteractiveShellDraftController shell_draft_;
  GuiInteractivePatternMirrorController pattern_mirror_;
  GuiInteractiveBodyOperationController body_operation_;
};

} // namespace blcad::gui
