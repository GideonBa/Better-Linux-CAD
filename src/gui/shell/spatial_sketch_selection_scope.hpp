#pragma once
#include "blcad/gui/occt_viewport.hpp"
namespace blcad::gui {
// Own only the task's disposable input presentations. Document selection is routed separately.
class SpatialSketchSelectionScope {
public:
  explicit SpatialSketchSelectionScope(OcctViewport& viewport) : viewport_(viewport) {}
  bool active() const noexcept {
    return active_;
  }
  bool restoring() const noexcept {
    return restoring_;
  }
  Result<std::size_t> show(std::vector<geometry::ViewportSceneItem> items) {
    if (!active_) {
      mask_ = viewport_.selection_filter_mask();
      scope_ = viewport_.selection_semantic_scope();
      selected_ = pending_ ? pending_ : viewport_.selected_semantic();
      active_ = true;
    }
    std::vector<std::string> ids;
    for (const auto& item : items)
      ids.push_back(item.semantic_id);
    restoring_ = true;
    auto shown = viewport_.set_preview_selection_targets(std::move(items));
    restoring_ = false;
    if (!shown) {
      finish();
      return shown;
    }
    viewport_.set_selection_filter_mask(0xFFFFFFFFU);
    viewport_.set_selection_semantic_scope(std::move(ids));
    return shown;
  }
  void finish() {
    if (!active_ && !pending_)
      return;
    restoring_ = true;
    if (active_) {
      viewport_.clear_preview_selection_targets();
      viewport_.set_selection_filter_mask(mask_);
      viewport_.set_selection_semantic_scope(scope_);
      active_ = false;
      pending_ = selected_;
    }
    if (pending_) {
      if (viewport_.select_semantic(pending_->semantic_id))
        pending_.reset();
    } else
      viewport_.clear_selection();
    restoring_ = false;
  }

private:
  OcctViewport& viewport_;
  bool active_{false}, restoring_{false};
  std::uint32_t mask_{};
  std::optional<std::vector<std::string>> scope_;
  std::optional<GuiSelection> selected_, pending_;
};
} // namespace blcad::gui
