#include "blcad/gui/gui_thread_recipe.hpp"

#include <algorithm>
#include <cmath>

namespace blcad::gui {

Result<PartDocument> GuiThreadRecipeCommands::create_external(PartDocument part, FeatureId id,
                                                              std::string name, BodyId target,
                                                              FaceReference support,
                                                              std::string prefix, double radius,
                                                              double span, double taper) {
  const auto fail = [](std::string message) {
    return Result<PartDocument>::failure(
        Error::validation("gui.thread.recipe", std::move(message)));
  };
  if (prefix.empty() || !std::isfinite(radius) || !std::isfinite(span) || !std::isfinite(taper) ||
      radius <= 0 || span <= 0)
    return fail("Choose a finite exterior Thread support.");
  // Separate ideal-V flanks also in modeled mode: pitch > 2*depth*tan(60deg/2).
  // Dimensions remain editable authored inputs; labels do not claim conformity.
  const double depth = std::min({1., radius / 5, span / 8});
  std::optional<Error> failure;
  const auto add = [&](const char* role, double value, bool angle = false) -> ParameterId {
    const ParameterId parameter_id(prefix + "." + role);
    std::size_t index = 1;
    std::string parameter_name;
    do {
      parameter_name = "thread_" + std::to_string(index++) + "_" + role;
    } while (part.find_parameter(std::string_view(parameter_name)));
    auto quantity = angle ? Quantity::angle_deg(value) : Quantity::length_mm(value);
    if (!quantity) {
      failure = quantity.error();
      return parameter_id;
    }
    auto parameter = angle
                         ? Parameter::create_angle(parameter_id, parameter_name, quantity.value())
                         : Parameter::create_length(parameter_id, parameter_name, quantity.value());
    if (!parameter) {
      failure = parameter.error();
      return parameter_id;
    }
    auto inserted = part.add_parameter(std::move(parameter.value()));
    if (!inserted)
      failure = inserted.error();
    return parameter_id;
  };
  ThreadParameterProfile profile{"Custom",
                                 "Custom external thread",
                                 "Authored",
                                 add("major", 2 * radius),
                                 add("minor", 2 * (radius - depth)),
                                 add("pitch", 1.5 * depth),
                                 add("angle", 60, true),
                                 {}};
  if (std::abs(taper) > 1e-9)
    profile.taper_angle = add("taper", taper, true);
  const auto length = add("length", std::min(4., span));
  if (failure)
    return Result<PartDocument>::failure(*failure);
  auto thread = ThreadDefinition::create(std::move(profile), length);
  if (!thread)
    return Result<PartDocument>::failure(thread.error());
  auto feature = ThreadFeature::create(std::move(id), std::move(name), std::move(target),
                                       std::move(support), std::move(thread.value()));
  if (!feature)
    return Result<PartDocument>::failure(feature.error());
  auto added = part.add_thread_feature(std::move(feature.value()));
  if (!added)
    return Result<PartDocument>::failure(added.error());
  return Result<PartDocument>::success(std::move(part));
}

} // namespace blcad::gui
