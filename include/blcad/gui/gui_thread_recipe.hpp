#pragma once

#include "blcad/core/part_document.hpp"

namespace blcad::gui {

// Disposable commands over canonical Thread intent. Geometric support dimensions
// are derived by Geometry; the complete candidate execution remains authoritative.
class GuiThreadRecipeCommands {
public:
  [[nodiscard]] static Result<PartDocument>
  create_external(PartDocument part, FeatureId id, std::string name, BodyId target,
                  FaceReference support, std::string draft_namespace, double radius_mm,
                  double axial_length_mm, double taper_angle_deg);
};

} // namespace blcad::gui
