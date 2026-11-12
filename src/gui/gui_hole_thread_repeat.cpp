#include "blcad/gui/gui_hole_thread_repeat.hpp"

#include <algorithm>
#include <map>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("gui.hole_thread.repeat", std::move(message));
}
struct Inputs {
  PartDocument& part;
  const std::vector<ParameterId>& owned;
  std::string previous, next;
  std::map<std::string, ParameterId> copies;
  std::optional<Error> failure;
  ParameterId parameter(const ParameterId& id) {
    if (const auto found = copies.find(id.value()); found != copies.end())
      return found->second;
    const auto* source = part.find_parameter(id);
    if (!source) {
      failure = error("A repeated dimension is unavailable.");
      return id;
    }
    if (std::find(owned.begin(), owned.end(), id) == owned.end() || source->is_expression())
      return id;
    const auto original = *source;
    const auto suffix = !previous.empty() && id.value().starts_with(previous + ".")
                            ? id.value().substr(previous.size())
                            : ".input." + std::to_string(copies.size());
    ParameterId fresh(next + suffix);
    std::size_t identity = copies.size();
    while (part.find_parameter(fresh))
      fresh = ParameterId(next + ".input." + std::to_string(identity++));
    std::size_t index = 1;
    std::string name;
    do {
      name = original.name() + "_copy_" + std::to_string(index++);
    } while (part.find_parameter(std::string_view(name)));
    auto copy = original.type() == ParameterType::Angle
                    ? Parameter::create_angle(fresh, name, original.value(), original.scope())
                : original.type() == ParameterType::Count
                    ? Parameter::create_count(fresh, name, original.value(), original.scope())
                    : Parameter::create_length(fresh, name, original.value(), original.scope());
    if (!copy)
      failure = copy.error();
    else if (auto added = part.add_parameter(std::move(copy.value())); !added)
      failure = added.error();
    copies.emplace(id.value(), fresh);
    return fresh;
  }
  HoleDimension dimension(const HoleDimension& value) {
    if (const auto* id = std::get_if<ParameterId>(&value))
      return parameter(*id);
    return value;
  }
  Result<ThreadDefinition> thread(const ThreadDefinition& value) {
    auto profile = value.profile();
    if (auto* custom = std::get_if<ThreadParameterProfile>(&profile)) {
      custom->major_diameter = parameter(custom->major_diameter);
      custom->minor_diameter = parameter(custom->minor_diameter);
      custom->pitch = parameter(custom->pitch);
      custom->included_angle = parameter(custom->included_angle);
      if (custom->taper_angle)
        custom->taper_angle = parameter(*custom->taper_angle);
    }
    auto length = parameter(value.length());
    auto start = value.start();
    if (start)
      start = parameter(*start);
    if (failure)
      return Result<ThreadDefinition>::failure(*failure);
    return ThreadDefinition::create(std::move(profile), std::move(length), std::move(start),
                                    value.handedness(), value.representation());
  }
};
} // namespace

Result<PartDocument>
GuiHoleThreadRepeatCommands::create(PartDocument part, FeatureId source, FeatureId next,
                                    std::string name, std::string old_namespace,
                                    std::string next_namespace,
                                    const std::vector<ParameterId>& owned_parameters) {
  if (next_namespace.empty() || next == source || next_namespace == old_namespace)
    return Result<PartDocument>::failure(error("A repeated feature requires fresh identities."));
  Inputs inputs{part, owned_parameters, std::move(old_namespace), std::move(next_namespace), {},
                {}};
  if (const auto* found = part.find_hole_feature(source)) {
    const auto original = *found;
    auto diameter = inputs.dimension(original.diameter());
    auto extent = original.extent();
    if (auto* blind = std::get_if<HoleBlindExtent>(&extent))
      blind->depth = inputs.parameter(blind->depth);
    if (auto* symmetric = std::get_if<HoleSymmetricExtent>(&extent))
      symmetric->total_depth = inputs.parameter(symmetric->total_depth);
    auto tip = original.tip();
    if (auto* cone = std::get_if<HoleConicalTip>(&tip))
      cone->included_angle = inputs.parameter(cone->included_angle);
    auto manufacturing = original.manufacturing();
    std::visit(
        [&](auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleSpotface>) {
            value.diameter = inputs.dimension(value.diameter);
            value.depth = inputs.dimension(value.depth);
          } else if constexpr (std::is_same_v<T, HoleCountersink> ||
                               std::is_same_v<T, HoleCenter>) {
            value.diameter = inputs.dimension(value.diameter);
            value.included_angle = inputs.dimension(value.included_angle);
          } else if constexpr (std::is_same_v<T, HoleFitted>) {
            value.minimum_diameter = inputs.dimension(value.minimum_diameter);
            value.maximum_diameter = inputs.dimension(value.maximum_diameter);
          } else if constexpr (std::is_same_v<T, HoleStepped>) {
            for (std::size_t i = 0; i < value.steps.size(); ++i) {
              auto& step = value.steps[i];
              step.id = inputs.next + ".step." + std::to_string(i);
              step.diameter = inputs.dimension(step.diameter);
              step.depth = inputs.dimension(step.depth);
            }
          }
        },
        manufacturing);
    auto placements = original.placements();
    for (std::size_t i = 0; i < placements.size(); ++i)
      placements[i].id = HolePlacementId(inputs.next + ".position." + std::to_string(i));
    auto thread = original.thread();
    if (thread) {
      auto repeated = inputs.thread(*thread);
      if (!repeated)
        return Result<PartDocument>::failure(repeated.error());
      thread = std::move(repeated.value());
    }
    if (inputs.failure)
      return Result<PartDocument>::failure(*inputs.failure);
    auto repeated = HoleFeature::create(
        std::move(next), std::move(name), original.kind(), original.target_body(),
        original.support(), std::move(placements), std::move(diameter), std::move(extent),
        original.direction(), std::move(tip), std::move(manufacturing), std::move(thread));
    if (!repeated)
      return Result<PartDocument>::failure(repeated.error());
    auto added = part.add_hole_feature(std::move(repeated.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
  } else if (const auto* found = part.find_thread_feature(source)) {
    const auto original = *found;
    auto thread = inputs.thread(original.thread());
    if (!thread)
      return Result<PartDocument>::failure(thread.error());
    auto repeated =
        ThreadFeature::create(std::move(next), std::move(name), original.target_body(),
                              original.support(), std::move(thread.value()), original.direction());
    if (!repeated)
      return Result<PartDocument>::failure(repeated.error());
    auto added = part.add_thread_feature(std::move(repeated.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
  } else
    return Result<PartDocument>::failure(error("Choose a current Hole or Thread to repeat."));
  return Result<PartDocument>::success(std::move(part));
}
} // namespace blcad::gui
