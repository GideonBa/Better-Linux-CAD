#include "blcad/gui/gui_thread_recipe.hpp"

#include <algorithm>
#include <cmath>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("gui.thread.recipe", std::move(message));
}
struct OwnedInputs {
  PartDocument& part;
  std::string prefix;
  std::optional<Error> failure;
  ParameterId add(const char* role, double value, bool angle = false) {
    const ParameterId parameter_id(prefix + "." + role);
    if (prefix.empty()) {
      failure = error("Owned Thread inputs require a fresh draft namespace.");
      return parameter_id;
    }
    std::size_t index = 1;
    std::string name;
    do {
      name = "thread_" + std::to_string(index++) + "_" + role;
    } while (part.find_parameter(std::string_view(name)));
    auto quantity = angle ? Quantity::angle_deg(value) : Quantity::length_mm(value);
    if (!quantity) {
      failure = quantity.error();
      return parameter_id;
    }
    auto parameter = angle ? Parameter::create_angle(parameter_id, name, quantity.value())
                           : Parameter::create_length(parameter_id, name, quantity.value());
    if (!parameter) {
      failure = parameter.error();
      return parameter_id;
    }
    auto inserted = part.add_parameter(std::move(parameter.value()));
    if (!inserted)
      failure = inserted.error();
    return parameter_id;
  }
};
Result<ThreadDefinition> definition(const PartDocument& part, const FeatureId& id) {
  if (const auto* thread = part.find_thread_feature(id))
    return Result<ThreadDefinition>::success(thread->thread());
  if (const auto* hole = part.find_hole_feature(id); hole && hole->thread())
    return Result<ThreadDefinition>::success(*hole->thread());
  return Result<ThreadDefinition>::failure(error("Choose a current internal or external Thread."));
}
Result<PartDocument> replace(PartDocument part, const FeatureId& id, ThreadDefinition definition,
                             const GuiThreadParameterValues& values) {
  const auto apply_values = [&](const auto& record) -> Result<std::size_t> {
    const auto used = record.parameter_dependencies();
    for (const auto& [key, scalar] : values) {
      const ParameterId parameter_id(key);
      if (std::find(used.begin(), used.end(), parameter_id) == used.end())
        continue;
      const auto* parameter = part.find_parameter(parameter_id);
      if (!parameter)
        return Result<std::size_t>::failure(error("Thread dimension is unavailable."));
      auto quantity = parameter->type() == ParameterType::Angle   ? Quantity::angle_deg(scalar)
                      : parameter->type() == ParameterType::Count ? Quantity::count(scalar)
                                                                  : Quantity::length_mm(scalar);
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(parameter_id, quantity.value());
      if (!changed)
        return Result<std::size_t>::failure(changed.error());
    }
    return Result<std::size_t>::success(values.size());
  };
  Result<std::size_t> updated =
      Result<std::size_t>::failure(error("Thread feature is unavailable."));
  if (const auto* thread = part.find_thread_feature(id)) {
    auto next = ThreadFeature::create(id, thread->name(), thread->target_body(), thread->support(),
                                      std::move(definition), thread->direction());
    if (!next)
      return Result<PartDocument>::failure(next.error());
    auto applied = apply_values(next.value());
    if (!applied)
      return Result<PartDocument>::failure(applied.error());
    updated = part.update_thread_feature(std::move(next.value()));
  } else if (const auto* hole = part.find_hole_feature(id)) {
    auto next =
        HoleFeature::create(id, hole->name(), hole->kind(), hole->target_body(), hole->support(),
                            hole->placements(), hole->diameter(), hole->extent(), hole->direction(),
                            hole->tip(), hole->manufacturing(), std::move(definition));
    if (!next)
      return Result<PartDocument>::failure(next.error());
    auto applied = apply_values(next.value());
    if (!applied)
      return Result<PartDocument>::failure(applied.error());
    updated = part.update_hole_feature(std::move(next.value()));
  }
  if (!updated)
    return Result<PartDocument>::failure(updated.error());
  return Result<PartDocument>::success(std::move(part));
}
} // namespace

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
  OwnedInputs inputs{part, std::move(prefix), {}};
  ThreadParameterProfile profile{"Custom",
                                 "Custom external thread",
                                 "Authored",
                                 inputs.add("major", 2 * radius),
                                 inputs.add("minor", 2 * (radius - depth)),
                                 inputs.add("pitch", 1.5 * depth),
                                 inputs.add("angle", 60, true),
                                 {}};
  if (std::abs(taper) > 1e-9)
    profile.taper_angle = inputs.add("taper", taper, true);
  const auto length = inputs.add("length", std::min(4., span));
  if (inputs.failure)
    return Result<PartDocument>::failure(*inputs.failure);
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

Result<PartDocument> GuiThreadRecipeCommands::profile(PartDocument part, FeatureId id,
                                                      ThreadProfile profile,
                                                      const GuiThreadParameterValues& values) {
  auto original = definition(part, id);
  if (!original)
    return Result<PartDocument>::failure(original.error());
  auto next = ThreadDefinition::create(std::move(profile), original.value().length(),
                                       original.value().start(), original.value().handedness(),
                                       original.value().representation());
  if (!next)
    return Result<PartDocument>::failure(next.error());
  return replace(std::move(part), id, std::move(next.value()), values);
}

Result<PartDocument>
GuiThreadRecipeCommands::custom_profile(PartDocument part, FeatureId id, std::string prefix,
                                        const GuiThreadParameterValues& values) {
  auto original = definition(part, id);
  if (!original)
    return Result<PartDocument>::failure(original.error());
  auto resolved = resolve_thread_definition(part, original.value());
  if (!resolved)
    return Result<PartDocument>::failure(resolved.error());
  const auto& value = resolved.value();
  OwnedInputs inputs{part, std::move(prefix), {}};
  ThreadParameterProfile custom{value.system,
                                value.designation,
                                value.tolerance,
                                inputs.add("major", value.major_diameter_mm),
                                inputs.add("minor", value.minor_diameter_mm),
                                inputs.add("pitch", value.pitch_mm),
                                inputs.add("angle", value.included_angle_deg, true),
                                {}};
  if (std::visit(
          [](const auto& profile) {
            if constexpr (std::is_same_v<std::decay_t<decltype(profile)>, ThreadStandardProfile>)
              return profile.columns.taper_angle.has_value();
            else
              return profile.taper_angle.has_value();
          },
          original.value().profile()))
    custom.taper_angle = inputs.add("taper", value.taper_angle_deg, true);
  if (inputs.failure)
    return Result<PartDocument>::failure(*inputs.failure);
  return profile(std::move(part), id, std::move(custom), values);
}

Result<PartDocument> GuiThreadRecipeCommands::start_offset(PartDocument part, FeatureId id,
                                                           std::string prefix, bool enabled,
                                                           const GuiThreadParameterValues& values) {
  auto original = definition(part, id);
  if (!original)
    return Result<PartDocument>::failure(original.error());
  OwnedInputs inputs{part, std::move(prefix), {}};
  auto start = original.value().start();
  if (!enabled)
    start.reset();
  else if (!start)
    start = inputs.add("start", 1);
  if (inputs.failure)
    return Result<PartDocument>::failure(*inputs.failure);
  auto next =
      ThreadDefinition::create(original.value().profile(), original.value().length(), start,
                               original.value().handedness(), original.value().representation());
  if (!next)
    return Result<PartDocument>::failure(next.error());
  return replace(std::move(part), id, std::move(next.value()), values);
}

Result<PartDocument> GuiThreadRecipeCommands::taper(PartDocument part, FeatureId id,
                                                    std::string prefix, bool enabled,
                                                    const GuiThreadParameterValues& values) {
  auto original = definition(part, id);
  if (!original)
    return Result<PartDocument>::failure(original.error());
  const auto* current = std::get_if<ThreadParameterProfile>(&original.value().profile());
  if (!current)
    return Result<PartDocument>::failure(
        error("Choose Custom or map a typed taper column in the pinned profile."));
  auto custom = *current;
  OwnedInputs inputs{part, std::move(prefix), {}};
  if (!enabled)
    custom.taper_angle.reset();
  else if (!custom.taper_angle)
    custom.taper_angle = inputs.add("taper", 1, true);
  if (inputs.failure)
    return Result<PartDocument>::failure(*inputs.failure);
  return profile(std::move(part), id, std::move(custom), values);
}

} // namespace blcad::gui
