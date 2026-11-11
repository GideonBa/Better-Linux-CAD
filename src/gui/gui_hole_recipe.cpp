#include "blcad/gui/gui_hole_recipe.hpp"

#include <QUuid>
#include <algorithm>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("gui.hole.recipe", std::move(message));
}
struct Dimensions {
  PartDocument& part;
  std::string prefix;
  std::optional<Error> failure;
  std::vector<ParameterId> used;
  ParameterId add(std::string role, double value, bool angle = false, std::string name_role = {}) {
    const ParameterId id(prefix + "." + role);
    if (const auto* existing = part.find_parameter(id)) {
      if (existing->type() != (angle ? ParameterType::Angle : ParameterType::Length))
        failure = error("Pending dimension has an incompatible type.");
      else if (std::find(used.begin(), used.end(), id) == used.end()) {
        auto quantity = angle ? Quantity::angle_deg(value) : Quantity::length_mm(value);
        if (!quantity)
          failure = quantity.error();
        else {
          auto changed = part.set_parameter_value(id, quantity.value());
          if (!changed)
            failure = changed.error();
        }
      }
      return id;
    }
    if (!name_role.empty())
      role = std::move(name_role);
    std::replace(role.begin(), role.end(), '.', '_');
    std::size_t index = 1;
    std::string name;
    do {
      name = "hole_" + std::to_string(index++) + "_" + role;
    } while (part.find_parameter(std::string_view(name)));
    auto q = angle ? Quantity::angle_deg(value) : Quantity::length_mm(value);
    if (!q) {
      failure = q.error();
      return id;
    }
    auto parameter = angle ? Parameter::create_angle(id, name, q.value())
                           : Parameter::create_length(id, name, q.value());
    if (!parameter) {
      failure = parameter.error();
      return id;
    }
    auto added = part.add_parameter(std::move(parameter.value()));
    if (!added)
      failure = added.error();
    return id;
  }
};
struct Draft {
  HoleFeature original;
  HoleDiameter diameter;
  HoleExtent extent;
  HoleTip tip;
  HoleManufacturing manufacturing;
  std::optional<ThreadDefinition> thread;
  explicit Draft(const HoleFeature& h)
      : original(h), diameter(h.diameter()), extent(h.extent()), tip(h.tip()),
        manufacturing(h.manufacturing()), thread(h.thread()) {}
  HoleDimension* dimension(GuiHoleDimensionSlot slot) {
    HoleDimension* result = nullptr;
    using R = GuiHoleDimensionRole;
    if (slot.role == R::Bore) {
      result = &diameter;
    } else
      std::visit(
          [&](auto& manufacturing) {
            using T = std::decay_t<decltype(manufacturing)>;
            if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleSpotface>) {
              if (slot.role == R::SeatDiameter) {
                result = &manufacturing.diameter;
              }
              if (slot.role == R::SeatDepth) {
                result = &manufacturing.depth;
              }
            } else if constexpr (std::is_same_v<T, HoleCountersink> ||
                                 std::is_same_v<T, HoleCenter>) {
              if (slot.role == R::SeatDiameter) {
                result = &manufacturing.diameter;
              }
              if (slot.role == R::SeatAngle) {
                result = &manufacturing.included_angle;
              }
            } else if constexpr (std::is_same_v<T, HoleFitted>) {
              if (slot.role == R::Minimum) {
                result = &manufacturing.minimum_diameter;
              }
              if (slot.role == R::Maximum) {
                result = &manufacturing.maximum_diameter;
              }
            } else if constexpr (std::is_same_v<T, HoleStepped>) {
              if (slot.step < manufacturing.steps.size()) {
                if (slot.role == R::StepDiameter) {
                  result = &manufacturing.steps[slot.step].diameter;
                }
                if (slot.role == R::StepDepth) {
                  result = &manufacturing.steps[slot.step].depth;
                }
              }
            }
          },
          manufacturing);
    return result;
  }
  Result<PartDocument> finish(PartDocument part, const std::optional<Error>& failure = {}) {
    if (failure)
      return Result<PartDocument>::failure(*failure);
    auto replacement = HoleFeature::create(
        original.id(), original.name(), original.kind(), original.target_body(), original.support(),
        original.placements(), std::move(diameter), std::move(extent), original.direction(),
        std::move(tip), std::move(manufacturing), std::move(thread));
    if (!replacement)
      return Result<PartDocument>::failure(replacement.error());
    auto updated = part.update_hole_feature(std::move(replacement.value()));
    if (!updated)
      return Result<PartDocument>::failure(updated.error());
    return Result<PartDocument>::success(std::move(part));
  }
};
Result<PartDocument> missing() {
  return Result<PartDocument>::failure(error("Choose a current Hole definition."));
}
} // namespace

Result<PartDocument> GuiHoleRecipeCommands::recipe(PartDocument part, FeatureId id,
                                                   std::string prefix, GuiHoleRecipe recipe,
                                                   std::optional<ProfileRegionReference> slot) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  const auto size = resolve_hole_diameter(part, draft.diameter);
  if (!size)
    return Result<PartDocument>::failure(size.error());
  const double bore = size.value().diameter.millimeters();
  Dimensions dims{part, std::move(prefix), {}, draft.original.parameter_dependencies()};
  switch (recipe) {
  case GuiHoleRecipe::Simple:
    draft.manufacturing = std::monostate{};
    break;
  case GuiHoleRecipe::Counterbore:
    draft.manufacturing =
        HoleCounterbore{dims.add("seat.diameter", bore + 4), dims.add("seat.depth", 2)};
    break;
  case GuiHoleRecipe::Countersink:
    draft.manufacturing =
        HoleCountersink{dims.add("seat.diameter", bore + 4), dims.add("seat.angle", 90, true)};
    break;
  case GuiHoleRecipe::Spotface:
    draft.manufacturing =
        HoleSpotface{dims.add("seat.diameter", bore + 4), dims.add("seat.depth", 1)};
    break;
  case GuiHoleRecipe::Stepped:
    draft.manufacturing =
        HoleStepped{{{"step." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
                      dims.add("step.outer.diameter", bore + 6), dims.add("step.outer.depth", 1)},
                     {"step." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
                      dims.add("step.inner.diameter", bore + 4), dims.add("step.inner.depth", 2)}}};
    break;
  case GuiHoleRecipe::Fitted:
  case GuiHoleRecipe::Reamed:
    draft.manufacturing = HoleFitted{
        "Custom fit", dims.add("fit.minimum", bore), dims.add("fit.maximum", bore + .1),
        recipe == GuiHoleRecipe::Fitted ? HoleFitProcess::Fitted : HoleFitProcess::Reamed};
    break;
  case GuiHoleRecipe::Center:
    draft.extent = HoleBlindExtent{dims.add("depth", 5)};
    draft.tip = HoleConicalTip{dims.add("tip.angle", 90, true)};
    draft.manufacturing =
        HoleCenter{dims.add("seat.diameter", bore + 4), dims.add("seat.angle", 90, true)};
    break;
  case GuiHoleRecipe::Slot:
    if (!slot)
      return Result<PartDocument>::failure(error("Select a closed slot profile."));
    draft.manufacturing = HoleSlot{*slot};
    break;
  default:
    return Result<PartDocument>::failure(error("Unknown Hole recipe."));
  }
  return draft.finish(std::move(part), dims.failure);
}

Result<PartDocument> GuiHoleRecipeCommands::extent(PartDocument part, FeatureId id,
                                                   std::string prefix, GuiHoleExtentChoice extent,
                                                   std::optional<FaceReference> face,
                                                   std::optional<BodyId> body) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  Dimensions dims{part, std::move(prefix), {}, draft.original.parameter_dependencies()};
  switch (extent) {
  case GuiHoleExtentChoice::Blind:
    draft.extent = HoleBlindExtent{dims.add("depth", 5)};
    break;
  case GuiHoleExtentChoice::ThroughAll:
    draft.extent = HoleThroughAllExtent{};
    break;
  case GuiHoleExtentChoice::ToNext:
    draft.extent = HoleToNextExtent{};
    break;
  case GuiHoleExtentChoice::ToFace:
    if (!face)
      return Result<PartDocument>::failure(error("Choose a limit face."));
    draft.extent = HoleToFaceExtent{*face};
    break;
  case GuiHoleExtentChoice::UpToBody:
    if (!body)
      return Result<PartDocument>::failure(error("Choose a limit Body."));
    draft.extent = HoleUpToBodyExtent{*body};
    break;
  case GuiHoleExtentChoice::Symmetric:
    draft.extent = HoleSymmetricExtent{dims.add("total.depth", 8)};
    break;
  default:
    return Result<PartDocument>::failure(error("Unknown Hole extent."));
  }
  return draft.finish(std::move(part), dims.failure);
}

Result<PartDocument> GuiHoleRecipeCommands::tip(PartDocument part, FeatureId id, std::string prefix,
                                                bool conical) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  Dimensions dims{part, std::move(prefix), {}, draft.original.parameter_dependencies()};
  draft.tip =
      conical ? HoleTip{HoleConicalTip{dims.add("tip.angle", 118, true)}} : HoleTip{HoleFlatTip{}};
  return draft.finish(std::move(part), dims.failure);
}

Result<PartDocument> GuiHoleRecipeCommands::internal_thread(PartDocument part, FeatureId id,
                                                            std::string prefix, bool enabled) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  Dimensions dims{part, std::move(prefix), {}, draft.original.parameter_dependencies()};
  if (!enabled)
    draft.thread.reset();
  else {
    const auto size = resolve_hole_diameter(part, draft.diameter);
    if (!size)
      return Result<PartDocument>::failure(size.error());
    const double minor = size.value().diameter.millimeters();
    auto definition =
        ThreadDefinition::create(ThreadParameterProfile{"Custom",
                                                        "Custom internal thread",
                                                        "Authored",
                                                        dims.add("thread.major", minor + 2),
                                                        dims.add("thread.minor", minor),
                                                        dims.add("thread.pitch", 1),
                                                        dims.add("thread.angle", 60, true),
                                                        {}},
                                 dims.add("thread.length", 4));
    if (!definition)
      return Result<PartDocument>::failure(definition.error());
    draft.thread = std::move(definition.value());
  }
  return draft.finish(std::move(part), dims.failure);
}

Result<PartDocument> GuiHoleRecipeCommands::dimension(PartDocument part, FeatureId id,
                                                      GuiHoleDimensionSlot slot,
                                                      HoleDimension source) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  auto* selected = draft.dimension(slot);
  if (!selected)
    return Result<PartDocument>::failure(
        error("Dimension does not belong to the selected recipe."));
  *selected = std::move(source);
  return draft.finish(std::move(part));
}
Result<HoleDimension> GuiHoleRecipeCommands::dimension_source(const HoleFeature& hole,
                                                              GuiHoleDimensionSlot slot) {
  Draft draft(hole);
  if (const auto* value = draft.dimension(slot))
    return Result<HoleDimension>::success(*value);
  return Result<HoleDimension>::failure(error("Dimension does not belong to the selected recipe."));
}
Result<PartDocument> GuiHoleRecipeCommands::custom_dimension(PartDocument part, FeatureId id,
                                                             std::string prefix,
                                                             GuiHoleDimensionSlot slot) {
  const auto* h = part.find_hole_feature(id);
  if (!h)
    return missing();
  Draft draft(*h);
  auto* selected = draft.dimension(slot);
  if (!selected)
    return Result<PartDocument>::failure(error("Choose a current recipe dimension."));
  const bool angle = slot.role == GuiHoleDimensionRole::SeatAngle;
  auto resolved = resolve_hole_dimension(part, *selected, angle);
  if (!resolved)
    return Result<PartDocument>::failure(resolved.error());
  Dimensions dims{part, std::move(prefix), {}, draft.original.parameter_dependencies()};
  const char* names[] = {"diameter",    "seat.diameter", "seat.depth",    "seat.angle",
                         "fit.minimum", "fit.maximum",   "step.diameter", "step.depth"};
  std::string name = "custom." + std::string(names[int(slot.role)]);
  if (slot.role == GuiHoleDimensionRole::StepDiameter ||
      slot.role == GuiHoleDimensionRole::StepDepth)
    name += "." + std::to_string(slot.step + 1);
  *selected = dims.add(
      "custom." + std::to_string(int(slot.role)) + "." + std::to_string(slot.step) + "." +
          QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
      angle ? resolved.value().diameter.degrees() : resolved.value().diameter.millimeters(), angle,
      name);
  return draft.finish(std::move(part), dims.failure);
}
} // namespace blcad::gui
