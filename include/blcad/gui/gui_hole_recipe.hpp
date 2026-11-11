#pragma once

#include "blcad/core/part_document.hpp"

namespace blcad::gui {

enum class GuiHoleRecipe {
  Simple,
  Counterbore,
  Countersink,
  Spotface,
  Stepped,
  Fitted,
  Reamed,
  Center,
  Slot
};
enum class GuiHoleExtentChoice { Blind, ThroughAll, ToNext, ToFace, UpToBody, Symmetric };
enum class GuiHoleDimensionRole {
  Bore,
  SeatDiameter,
  SeatDepth,
  SeatAngle,
  Minimum,
  Maximum,
  StepDiameter,
  StepDepth
};
struct GuiHoleDimensionSlot {
  GuiHoleDimensionRole role{GuiHoleDimensionRole::Bore};
  std::size_t step{};
};

// Disposable authoring commands over the existing Core model. The caller owns
// a fresh draft namespace; role dimensions are reused only within that draft.
// No command mutates its source or publishes a document transaction.
class GuiHoleRecipeCommands {
public:
  [[nodiscard]] static Result<PartDocument> recipe(PartDocument part, FeatureId hole,
                                                   std::string draft_namespace,
                                                   GuiHoleRecipe recipe,
                                                   std::optional<ProfileRegionReference> slot = {});
  [[nodiscard]] static Result<PartDocument>
  extent(PartDocument part, FeatureId hole, std::string draft_namespace, GuiHoleExtentChoice extent,
         std::optional<FaceReference> face = {}, std::optional<BodyId> body = {});
  [[nodiscard]] static Result<PartDocument> tip(PartDocument part, FeatureId hole,
                                                std::string draft_namespace, bool conical);
  [[nodiscard]] static Result<PartDocument>
  internal_thread(PartDocument part, FeatureId hole, std::string draft_namespace, bool enabled);
  [[nodiscard]] static Result<PartDocument>
  dimension(PartDocument part, FeatureId hole, GuiHoleDimensionSlot slot, HoleDimension source);
  [[nodiscard]] static Result<HoleDimension> dimension_source(const HoleFeature& hole,
                                                              GuiHoleDimensionSlot slot);
  [[nodiscard]] static Result<PartDocument> custom_dimension(PartDocument part, FeatureId hole,
                                                             std::string draft_namespace,
                                                             GuiHoleDimensionSlot slot);
};

} // namespace blcad::gui
