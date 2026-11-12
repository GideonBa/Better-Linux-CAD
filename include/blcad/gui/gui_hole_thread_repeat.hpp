#pragma once

#include "blcad/core/part_document.hpp"

namespace blcad::gui {
// Repeat the current creation's typed intent. Only dimensions owned by that
// disposable draft are copied; explicit shared/formula/standard bindings remain.
class GuiHoleThreadRepeatCommands {
public:
  [[nodiscard]] static Result<PartDocument>
  create(PartDocument part, FeatureId source, FeatureId next, std::string name,
         std::string old_namespace, std::string next_namespace,
         const std::vector<ParameterId>& owned_parameters);
};
} // namespace blcad::gui
