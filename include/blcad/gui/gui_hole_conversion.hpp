#pragma once
#include "blcad/core/part_document.hpp"

namespace blcad::gui {
class GuiHoleConversionCommands {
public:
  // Explicit conversion only. Keep the source Sketch; its circle center/origin
  // becomes an independent placement while diameter and support stay bound.
  [[nodiscard]] static Result<PartDocument> candidate(const PartDocument&, FeatureId);
};
} // namespace blcad::gui
