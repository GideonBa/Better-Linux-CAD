#pragma once

#include "blcad/core/part_document.hpp"
#include <map>

namespace blcad::gui {
using GuiThreadParameterValues = std::map<std::string, double>;

// Disposable commands over canonical Thread intent. Geometric support dimensions
// are derived by Geometry; the complete candidate execution remains authoritative.
class GuiThreadRecipeCommands {
public:
  [[nodiscard]] static Result<PartDocument>
  create_external(PartDocument part, FeatureId id, std::string name, BodyId target,
                  FaceReference support, std::string draft_namespace, double radius_mm,
                  double axial_length_mm, double taper_angle_deg,
                  ThreadDirection direction = ThreadDirection::AlongSupportAxis);
  [[nodiscard]] static Result<PartDocument> profile(PartDocument part, FeatureId feature,
                                                    ThreadProfile profile,
                                                    const GuiThreadParameterValues& values = {});
  [[nodiscard]] static Result<PartDocument>
  custom_profile(PartDocument part, FeatureId feature, std::string draft_namespace,
                 const GuiThreadParameterValues& values = {});
  [[nodiscard]] static Result<PartDocument>
  start_offset(PartDocument part, FeatureId feature, std::string draft_namespace, bool enabled,
               const GuiThreadParameterValues& values = {});
  [[nodiscard]] static Result<PartDocument> taper(PartDocument part, FeatureId feature,
                                                  std::string draft_namespace, bool enabled,
                                                  const GuiThreadParameterValues& values = {});
};

} // namespace blcad::gui
