#pragma once

#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"

#include <algorithm>
#include <vector>

namespace blcad::gui {

inline void filter_presentation_visibility(const GuiDocumentSession& session,
                                           std::vector<geometry::ViewportSceneItem>& items) {
  std::erase_if(items, [&](const auto& item) {
    if (!session.presentation_visible(item.semantic_id))
      return true;
    const std::string owner_body_id =
        !item.owner_body_id.empty()
            ? item.owner_body_id
            : (item.semantic_id.starts_with("body/") ? item.semantic_id.substr(5U) : std::string{});
    if (!owner_body_id.empty() && !session.presentation_visible("body/" + owner_body_id))
      return true;
    if (!owner_body_id.empty() && session.part_shape_cache()) {
      const auto* body = session.part_shape_cache()->find_body_result(BodyId(owner_body_id));
      return body && !session.presentation_visible("feature/" + body->source_feature_id.value());
    }
    return false;
  });
}

inline void filter_presentation_visibility(const GuiDocumentSession& session,
                                           GuiSketchInteractionScene& scene) {
  const auto hidden = [&](const auto& item) {
    return !session.presentation_visible(item.semantic_id);
  };
  const auto removed_curves = std::erase_if(scene.curves, hidden);
  std::erase_if(scene.points, hidden);
  std::erase_if(scene.annotations, hidden);
  // Derived intersection snaps must not retain hidden curve intersections.
  // Authored endpoint, nearest and axis snaps remain available independently.
  if (removed_curves != 0U)
    scene.intersections.clear();
}

} // namespace blcad::gui
