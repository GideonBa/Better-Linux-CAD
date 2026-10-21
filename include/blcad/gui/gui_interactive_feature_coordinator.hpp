#pragma once

#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_interactive_path_sweep_loft.hpp"
#include "blcad/gui/gui_interactive_pattern_body.hpp"
#include "blcad/gui/gui_interactive_surface.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include <tuple>
#include <utility>
#include <vector>

namespace blcad::gui {

// Block 124..128 coordinator that connects the interactive Extrude, Revolve,
// Fillet/Chamfer, Shell/Draft, Pattern/Mirror, Body Boolean/Transform,
// Path/Sweep, Loft, and Surface controllers to the Block-123 candidate-only
// manipulator layer. It is deliberately Qt-free: the shell owns one instance and
// routes the manipulator overlay's candidate callback into on_candidate(...).
// No document authority lives here; every mutation still flows through a
// controller's one Apply transaction. At most one controller is active.
class GuiInteractiveFeatureCoordinator {
public:
  GuiInteractiveFeatureCoordinator(GuiDocumentSession* session,
                                   GuiViewportManipulatorLayer* layer) noexcept
      : session_(session), layer_(layer) {}

  [[nodiscard]] GuiInteractiveExtrudeController& extrude() noexcept {
    return extrude_;
  }
  [[nodiscard]] const GuiInteractiveExtrudeController& extrude() const noexcept {
    return extrude_;
  }
  [[nodiscard]] GuiInteractiveRevolveController& revolve() noexcept {
    return revolve_;
  }
  [[nodiscard]] const GuiInteractiveRevolveController& revolve() const noexcept {
    return revolve_;
  }
  [[nodiscard]] GuiInteractiveFinishingController& finishing() noexcept {
    return finishing_;
  }
  [[nodiscard]] const GuiInteractiveFinishingController& finishing() const noexcept {
    return finishing_;
  }
  [[nodiscard]] const GuiInteractiveShellDraftController& shell_draft() const noexcept {
    return shell_draft_;
  }
  [[nodiscard]] GuiInteractiveShellDraftController& shell_draft() noexcept {
    return shell_draft_;
  }
  [[nodiscard]] GuiInteractivePatternMirrorController& pattern_mirror() noexcept {
    return pattern_mirror_;
  }
  [[nodiscard]] const GuiInteractivePatternMirrorController& pattern_mirror() const noexcept {
    return pattern_mirror_;
  }
  [[nodiscard]] GuiInteractiveBodyOperationController& body_operation() noexcept {
    return body_operation_;
  }
  [[nodiscard]] const GuiInteractiveBodyOperationController& body_operation() const noexcept {
    return body_operation_;
  }
  [[nodiscard]] GuiInteractivePathSweepController& path_sweep() noexcept {
    return path_sweep_;
  }
  [[nodiscard]] GuiInteractiveLoftController& loft() noexcept {
    return loft_;
  }
  [[nodiscard]] GuiInteractiveSurfaceController& surface() noexcept {
    return surface_;
  }

  [[nodiscard]] bool active() const noexcept {
    return std::apply([](const auto&... controller) { return (controller.active() || ...); },
                      controllers());
  }

  // Re-publish the active controller's handles to the shared manipulator layer.
  // A caller that begins a finishing/shell-draft command through the exposed
  // controller accessors calls this to seed the handles.
  void refresh_handles() {
    publish_handles();
  }

  [[nodiscard]] Result<std::size_t> begin_extrude(SketchId sketch, ProfileId profile,
                                                  FeatureId feature, std::string name,
                                                  ParameterId distance_parameter,
                                                  BodyId result_body,
                                                  GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return no_session<std::size_t>();
    auto begun =
        extrude_.begin(*session_, std::move(sketch), std::move(profile), std::move(feature),
                       std::move(name), std::move(distance_parameter), std::move(result_body));
    if (begun.has_error())
      return begun;
    extrude_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  [[nodiscard]] Result<std::size_t>
  begin_extrude_with_new_inputs(SketchId sketch, ProfileId profile, FeatureId feature,
                                std::string name, Parameter distance_parameter,
                                Body result_body,
                                GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return no_session<std::size_t>();
    auto begun = extrude_.begin_with_new_inputs(
        *session_, std::move(sketch), std::move(profile), std::move(feature),
        std::move(name), std::move(distance_parameter), std::move(result_body));
    if (begun.has_error())
      return begun;
    extrude_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  [[nodiscard]] Result<std::size_t> begin_revolve(SketchId sketch, ProfileId profile,
                                                  FeatureId feature, std::string name,
                                                  AxisReference axis, BodyId result_body,
                                                  GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return no_session<std::size_t>();
    auto begun =
        revolve_.begin(*session_, std::move(sketch), std::move(profile), std::move(feature),
                       std::move(name), std::move(axis), std::move(result_body));
    if (begun.has_error())
      return begun;
    revolve_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  [[nodiscard]] Result<std::size_t>
  begin_revolve_with_new_body(SketchId sketch, ProfileId profile, FeatureId feature,
                              std::string name, AxisReference axis, Body result_body,
                              GuiFeatureManipulatorFrame frame = {}) {
    cancel();
    if (session_ == nullptr)
      return no_session<std::size_t>();
    auto begun = revolve_.begin_with_new_body(
        *session_, std::move(sketch), std::move(profile), std::move(feature),
        std::move(name), std::move(axis), std::move(result_body));
    if (begun.has_error())
      return begun;
    revolve_.set_manipulator_frame(frame);
    publish_handles();
    return begun;
  }

  // Fold a manipulator candidate into the active controller. Handles are only
  // re-seeded once the drag has ended (empty active handle) so the layer is
  // never reset mid-drag.
  bool on_candidate(const GuiViewportManipulatorCandidate& candidate) {
    bool changed = false;
    visit_active([&candidate, &changed](auto& controller) {
      changed = controller.apply_manipulator(candidate);
    });
    if (changed && layer_ != nullptr && layer_->active_handle_id().empty())
      publish_handles();
    return changed;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview() const {
    if (session_ == nullptr)
      return no_session<GuiPartFeaturePreview>();
    Result<GuiPartFeaturePreview> previewed = nothing_active<GuiPartFeaturePreview>();
    visit_active(
        [this, &previewed](const auto& controller) { previewed = controller.preview(*session_); });
    return previewed;
  }

  [[nodiscard]] Result<std::size_t> apply() {
    if (session_ == nullptr)
      return no_session<std::size_t>();
    Result<std::size_t> applied = nothing_active<std::size_t>();
    visit_active([this, &applied](auto& controller) { applied = controller.apply(*session_); });
    if (!applied.has_error())
      clear_handles();
    return applied;
  }

  void cancel() noexcept {
    std::apply([](auto&... controller) { (controller.cancel(), ...); }, controllers());
    clear_handles();
  }

private:
  // Every controller exposes the same active/handles/apply_manipulator/preview/
  // apply/cancel surface, so each operation walks this one ordered list instead
  // of repeating a nine-way chain. The order is the Block 124..128 command order
  // and decides which controller wins if two were ever active at once.
  using ControllerRefs =
      std::tuple<GuiInteractiveExtrudeController&, GuiInteractiveRevolveController&,
                 GuiInteractiveFinishingController&, GuiInteractiveShellDraftController&,
                 GuiInteractivePatternMirrorController&, GuiInteractiveBodyOperationController&,
                 GuiInteractivePathSweepController&, GuiInteractiveLoftController&,
                 GuiInteractiveSurfaceController&>;
  using ConstControllerRefs = std::tuple<
      const GuiInteractiveExtrudeController&, const GuiInteractiveRevolveController&,
      const GuiInteractiveFinishingController&, const GuiInteractiveShellDraftController&,
      const GuiInteractivePatternMirrorController&, const GuiInteractiveBodyOperationController&,
      const GuiInteractivePathSweepController&, const GuiInteractiveLoftController&,
      const GuiInteractiveSurfaceController&>;

  [[nodiscard]] ControllerRefs controllers() noexcept {
    return std::tie(extrude_, revolve_, finishing_, shell_draft_, pattern_mirror_, body_operation_,
                    path_sweep_, loft_, surface_);
  }
  [[nodiscard]] ConstControllerRefs controllers() const noexcept {
    return std::tie(extrude_, revolve_, finishing_, shell_draft_, pattern_mirror_, body_operation_,
                    path_sweep_, loft_, surface_);
  }

  // Run `visitor` on the first active controller, if any.
  template <typename Visitor> void visit_active(Visitor visitor) {
    std::apply(
        [&visitor](auto&... controller) {
          (void)(... || (controller.active() ? (visitor(controller), true) : false));
        },
        controllers());
  }
  template <typename Visitor> void visit_active(Visitor visitor) const {
    std::apply(
        [&visitor](const auto&... controller) {
          (void)(... || (controller.active() ? (visitor(controller), true) : false));
        },
        controllers());
  }

  template <typename T> [[nodiscard]] static Result<T> no_session() {
    return Result<T>::failure(Error::validation("gui.interactive_features", "no active session"));
  }
  template <typename T> [[nodiscard]] static Result<T> nothing_active() {
    return Result<T>::failure(
        Error::validation("gui.interactive_features", "no interactive feature is active"));
  }

  void publish_handles() {
    if (layer_ == nullptr)
      return;
    std::vector<GuiViewportManipulatorHandle> handles;
    visit_active([&handles](const auto& controller) { handles = controller.handles(); });
    (void)layer_->set_handles(std::move(handles));
  }

  void clear_handles() noexcept {
    if (layer_ != nullptr)
      layer_->clear_handles();
  }

  GuiDocumentSession* session_{nullptr};
  GuiViewportManipulatorLayer* layer_{nullptr};
  GuiInteractiveExtrudeController extrude_;
  GuiInteractiveRevolveController revolve_;
  GuiInteractiveFinishingController finishing_;
  GuiInteractiveShellDraftController shell_draft_;
  GuiInteractivePatternMirrorController pattern_mirror_;
  GuiInteractiveBodyOperationController body_operation_;
  GuiInteractivePathSweepController path_sweep_;
  GuiInteractiveLoftController loft_;
  GuiInteractiveSurfaceController surface_;
};

} // namespace blcad::gui
