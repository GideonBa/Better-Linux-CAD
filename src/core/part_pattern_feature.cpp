#include "blcad/core/part_pattern_feature.hpp"

#include "blcad/core/error.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace blcad {
namespace {

Result<std::size_t> validate_placements(const FeatureId& id,
                                        std::optional<PatternPlacementOptions>& options,
                                        bool linear) {
  if (!options)
    return Result<std::size_t>::success(0);
  const auto fail = [&] {
    return Result<std::size_t>::failure(Error::validation(
        id.value(), "Pattern placement options require unique non-seed indices below 1024 and a "
                    "typed second linear direction"));
  };
  if (options->skipped.size() > pattern_placement_limit)
    return fail();
  std::sort(options->skipped.begin(), options->skipped.end());
  if (std::adjacent_find(options->skipped.begin(), options->skipped.end()) !=
      options->skipped.end())
    return fail();
  for (const auto index : options->skipped)
    if (index == PatternOccurrenceIndex{} || index.first >= pattern_placement_limit ||
        index.second >= pattern_placement_limit ||
        ((!linear || !options->second_direction) && index.second != 0))
      return fail();
  if (const auto& second = options->second_direction) {
    if (!linear || second->count.empty() || second->extent.empty() ||
        second->direction.role() != PartFeatureInputRole::PatternAxis ||
        second->direction.expected_capability() != PartFeatureInputCapability::Axis ||
        (second->extent_mode != LinearPatternExtentMode::Spacing &&
         second->extent_mode != LinearPatternExtentMode::TotalExtent) ||
        (second->sign != PatternDirectionSign::Positive &&
         second->sign != PatternDirectionSign::Negative))
      return fail();
  }
  return Result<std::size_t>::success(options->skipped.size());
}

Result<std::size_t> resolved_count(const PartDocument& part, ParameterId id, bool extended,
                                   const FeatureId& feature) {
  const auto* p = part.find_parameter(id);
  if (!p || p->type() != ParameterType::Count || p->value().count_value() < (extended ? 1U : 2U) ||
      p->value().count_value() > pattern_placement_limit)
    return Result<std::size_t>::failure(
        Error::validation(feature.value(), extended ? "Pattern count must be in [1,1024]"
                                                    : "Legacy Pattern count must be in [2,1024]"));
  return Result<std::size_t>::success(p->value().count_value());
}
Result<double> resolved_spacing(const PartDocument& part, ParameterId id,
                                LinearPatternExtentMode mode, std::size_t count,
                                PatternDirectionSign sign, const FeatureId& feature) {
  const auto* p = part.find_parameter(id);
  if (!p || p->type() != ParameterType::Length || !p->value().is_positive_length())
    return Result<double>::failure(
        Error::validation(feature.value(), "Pattern extent must be a positive Length parameter"));
  double step = mode == LinearPatternExtentMode::TotalExtent
                    ? (count == 1 ? 0 : p->value().millimeters() / (count - 1))
                    : p->value().millimeters();
  if (!std::isfinite(step * (count - 1)))
    return Result<double>::failure(
        Error::validation(feature.value(), "Pattern extent overflows the placement range"));
  return Result<double>::success(sign == PatternDirectionSign::Negative ? -step : step);
}

bool skipped(const std::optional<PatternPlacementOptions>& options, PatternOccurrenceIndex index) {
  return options && std::binary_search(options->skipped.begin(), options->skipped.end(), index);
}

[[nodiscard]] Result<std::size_t>
validate_common(const FeatureId& id, const std::string& name,
                const std::vector<PatternSourceReference>& sources,
                const AxisReference& direction_or_axis, const ParameterId& count_parameter) {
  const std::string object_id = id.empty() ? "part_pattern" : id.value();
  if (id.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "pattern feature id must not be empty"));
  if (name.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "pattern feature name must not be empty"));
  if (sources.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "pattern requires at least one source"));
  std::unordered_set<std::string> unique_sources;
  for (const auto& source : sources)
    if (!unique_sources.insert(source.source_node_id()).second)
      return Result<std::size_t>::failure(
          Error::validation(object_id, "pattern sources must be unique"));
  if (direction_or_axis.role() != PartFeatureInputRole::PatternAxis ||
      direction_or_axis.expected_capability() != PartFeatureInputCapability::Axis)
    return Result<std::size_t>::failure(Error::validation(
        object_id, "pattern direction/axis must use pattern_axis role and Axis capability"));
  if (count_parameter.empty())
    return Result<std::size_t>::failure(
        Error::validation(object_id, "pattern count parameter must not be empty"));
  return Result<std::size_t>::success(sources.size());
}

} // namespace

std::string_view to_string(PatternSourceKind kind) noexcept {
  return kind == PatternSourceKind::Feature ? "feature" : "body";
}

std::string_view to_string(LinearPatternExtentMode mode) noexcept {
  return mode == LinearPatternExtentMode::Spacing ? "spacing" : "total_extent";
}

std::string_view to_string(PatternDirectionSign sign) noexcept {
  return sign == PatternDirectionSign::Positive ? "positive" : "negative";
}

Result<PatternSourceReference> PatternSourceReference::feature(FeatureId feature) {
  if (feature.empty())
    return Result<PatternSourceReference>::failure(
        Error::validation("pattern_source", "pattern source feature id must not be empty"));
  return Result<PatternSourceReference>::success(PatternSourceReference(std::move(feature)));
}

Result<PatternSourceReference> PatternSourceReference::body(BodyId body) {
  if (body.empty())
    return Result<PatternSourceReference>::failure(
        Error::validation("pattern_source", "pattern source body id must not be empty"));
  return Result<PatternSourceReference>::success(PatternSourceReference(std::move(body)));
}

PatternSourceReference::PatternSourceReference(PatternSource source) : source_(std::move(source)) {}

PatternSourceKind PatternSourceReference::kind() const noexcept {
  return std::holds_alternative<FeatureId>(source_) ? PatternSourceKind::Feature
                                                    : PatternSourceKind::Body;
}

const PatternSource& PatternSourceReference::source() const noexcept {
  return source_;
}

std::string PatternSourceReference::source_node_id() const {
  if (const auto* feature = std::get_if<FeatureId>(&source_))
    return feature->value();
  return "body:" + std::get<BodyId>(source_).value();
}

Result<LinearPatternFeature>
LinearPatternFeature::create(FeatureId id, std::string name,
                             std::vector<PatternSourceReference> sources, AxisReference direction,
                             ParameterId count_parameter, LinearPatternExtentMode extent_mode,
                             ParameterId extent_parameter, PatternDirectionSign direction_sign,
                             FeatureBodyResultContext body_result_context,
                             std::optional<PatternPlacementOptions> placements) {
  auto valid = validate_common(id, name, sources, direction, count_parameter);
  if (valid.has_error())
    return Result<LinearPatternFeature>::failure(valid.error());
  if (extent_parameter.empty())
    return Result<LinearPatternFeature>::failure(
        Error::validation(id.value(), "linear pattern extent parameter must not be empty"));
  if ((extent_mode != LinearPatternExtentMode::Spacing &&
       extent_mode != LinearPatternExtentMode::TotalExtent) ||
      (direction_sign != PatternDirectionSign::Positive &&
       direction_sign != PatternDirectionSign::Negative))
    return Result<LinearPatternFeature>::failure(
        Error::validation(id.value(), "linear pattern requires known extent and direction modes"));
  auto placement_valid = validate_placements(id, placements, true);
  if (!placement_valid)
    return Result<LinearPatternFeature>::failure(placement_valid.error());
  return Result<LinearPatternFeature>::success(
      LinearPatternFeature(std::move(id), std::move(name), std::move(sources), std::move(direction),
                           std::move(count_parameter), extent_mode, std::move(extent_parameter),
                           direction_sign, std::move(body_result_context), std::move(placements)));
}

LinearPatternFeature::LinearPatternFeature(
    FeatureId id, std::string name, std::vector<PatternSourceReference> sources,
    AxisReference direction, ParameterId count_parameter, LinearPatternExtentMode extent_mode,
    ParameterId extent_parameter, PatternDirectionSign direction_sign,
    FeatureBodyResultContext body_result_context, std::optional<PatternPlacementOptions> placements)
    : id_(std::move(id)), name_(std::move(name)), sources_(std::move(sources)),
      direction_(std::move(direction)), count_parameter_(std::move(count_parameter)),
      extent_mode_(extent_mode), extent_parameter_(std::move(extent_parameter)),
      direction_sign_(direction_sign), body_result_context_(std::move(body_result_context)),
      placements_(std::move(placements)) {}

const FeatureId& LinearPatternFeature::id() const noexcept {
  return id_;
}
const std::string& LinearPatternFeature::name() const noexcept {
  return name_;
}
const std::vector<PatternSourceReference>& LinearPatternFeature::sources() const noexcept {
  return sources_;
}
const AxisReference& LinearPatternFeature::direction() const noexcept {
  return direction_;
}
const ParameterId& LinearPatternFeature::count_parameter() const noexcept {
  return count_parameter_;
}
LinearPatternExtentMode LinearPatternFeature::extent_mode() const noexcept {
  return extent_mode_;
}
const ParameterId& LinearPatternFeature::extent_parameter() const noexcept {
  return extent_parameter_;
}
PatternDirectionSign LinearPatternFeature::direction_sign() const noexcept {
  return direction_sign_;
}
const FeatureBodyResultContext& LinearPatternFeature::body_result_context() const noexcept {
  return body_result_context_;
}
const std::optional<PatternPlacementOptions>& LinearPatternFeature::placements() const noexcept {
  return placements_;
}

Result<CircularPatternFeature>
CircularPatternFeature::create(FeatureId id, std::string name,
                               std::vector<PatternSourceReference> sources, AxisReference axis,
                               ParameterId count_parameter, double total_angle_deg,
                               bool equal_spacing, FeatureBodyResultContext body_result_context,
                               std::optional<PatternPlacementOptions> placements) {
  auto valid = validate_common(id, name, sources, axis, count_parameter);
  if (valid.has_error())
    return Result<CircularPatternFeature>::failure(valid.error());
  if (!std::isfinite(total_angle_deg) || total_angle_deg <= 0.0 || total_angle_deg > 360.0)
    return Result<CircularPatternFeature>::failure(Error::validation(
        id.value(), "circular pattern total angle must be finite and in (0, 360] degrees"));
  if (!equal_spacing)
    return Result<CircularPatternFeature>::failure(
        Error::validation(id.value(), "circular pattern requires equal spacing in the first MVP"));
  auto placement_valid = validate_placements(id, placements, false);
  if (!placement_valid)
    return Result<CircularPatternFeature>::failure(placement_valid.error());
  return Result<CircularPatternFeature>::success(
      CircularPatternFeature(std::move(id), std::move(name), std::move(sources), std::move(axis),
                             std::move(count_parameter), total_angle_deg, equal_spacing,
                             std::move(body_result_context), std::move(placements)));
}

CircularPatternFeature::CircularPatternFeature(FeatureId id, std::string name,
                                               std::vector<PatternSourceReference> sources,
                                               AxisReference axis, ParameterId count_parameter,
                                               double total_angle_deg, bool equal_spacing,
                                               FeatureBodyResultContext body_result_context,
                                               std::optional<PatternPlacementOptions> placements)
    : id_(std::move(id)), name_(std::move(name)), sources_(std::move(sources)),
      axis_(std::move(axis)), count_parameter_(std::move(count_parameter)),
      total_angle_deg_(total_angle_deg), equal_spacing_(equal_spacing),
      body_result_context_(std::move(body_result_context)), placements_(std::move(placements)) {}

const FeatureId& CircularPatternFeature::id() const noexcept {
  return id_;
}
const std::string& CircularPatternFeature::name() const noexcept {
  return name_;
}
const std::vector<PatternSourceReference>& CircularPatternFeature::sources() const noexcept {
  return sources_;
}
const AxisReference& CircularPatternFeature::axis() const noexcept {
  return axis_;
}
const ParameterId& CircularPatternFeature::count_parameter() const noexcept {
  return count_parameter_;
}
double CircularPatternFeature::total_angle_deg() const noexcept {
  return total_angle_deg_;
}
bool CircularPatternFeature::equal_spacing() const noexcept {
  return equal_spacing_;
}
const FeatureBodyResultContext& CircularPatternFeature::body_result_context() const noexcept {
  return body_result_context_;
}
const std::optional<PatternPlacementOptions>& CircularPatternFeature::placements() const noexcept {
  return placements_;
}

std::string pattern_occurrence_key(PatternOccurrenceIndex index) {
  return std::to_string(index.first) + "/" + std::to_string(index.second);
}

std::string pattern_source_occurrence_key(const PatternSourceReference& source,
                                          PatternOccurrenceIndex index, bool extended) {
  const auto id = source.source_node_id();
  const auto kind = std::string(to_string(source.kind()));
  if (!extended)
    return kind + "/" + id + "/" + std::to_string(index.first);
  return "occurrence/" + kind + "/" + std::to_string(id.size()) + ":" + id + "/" +
         pattern_occurrence_key(index);
}

Result<std::vector<PatternPlacement>> evaluate_pattern_placements(const PartDocument& part,
                                                                  const LinearPatternFeature& f) {
  const auto fail = [](const Error& error) {
    return Result<std::vector<PatternPlacement>>::failure(error);
  };
  auto first = resolved_count(part, f.count_parameter(), f.placements().has_value(), f.id());
  if (!first)
    return fail(first.error());
  auto step = resolved_spacing(part, f.extent_parameter(), f.extent_mode(), first.value(),
                               f.direction_sign(), f.id());
  if (!step)
    return fail(step.error());
  std::size_t second = 1;
  double second_step = 0;
  if (f.placements() && f.placements()->second_direction) {
    const auto& direction = *f.placements()->second_direction;
    auto count = resolved_count(part, direction.count, true, f.id());
    if (!count)
      return fail(count.error());
    auto extent = resolved_spacing(part, direction.extent, direction.extent_mode, count.value(),
                                   direction.sign, f.id());
    if (!extent)
      return fail(extent.error());
    second = count.value();
    second_step = extent.value();
  }
  if (first.value() > pattern_placement_limit / second ||
      first.value() * second > pattern_placement_limit / f.sources().size())
    return fail(
        Error::validation(f.id().value(), "Pattern exceeds the 1024 source-occurrence limit"));
  std::vector<PatternPlacement> result;
  result.reserve(first.value() * second);
  for (std::size_t i = 0; i < first.value(); ++i)
    for (std::size_t j = 0; j < second; ++j)
      result.push_back(
          {{i, j}, i * step.value(), j * second_step, 0, skipped(f.placements(), {i, j})});
  return Result<std::vector<PatternPlacement>>::success(std::move(result));
}

Result<std::vector<PatternPlacement>> evaluate_pattern_placements(const PartDocument& part,
                                                                  const CircularPatternFeature& f) {
  auto count = resolved_count(part, f.count_parameter(), f.placements().has_value(), f.id());
  if (!count)
    return Result<std::vector<PatternPlacement>>::failure(count.error());
  if (count.value() > pattern_placement_limit / f.sources().size())
    return Result<std::vector<PatternPlacement>>::failure(
        Error::validation(f.id().value(), "Pattern exceeds the 1024 source-occurrence limit"));
  const double step =
      count.value() == 1
          ? 0
          : f.total_angle_deg() / (f.total_angle_deg() == 360 ? count.value() : count.value() - 1);
  std::vector<PatternPlacement> result;
  result.reserve(count.value());
  for (std::size_t i = 0; i < count.value(); ++i)
    result.push_back({{i, 0}, 0, 0, i * step, skipped(f.placements(), {i, 0})});
  return Result<std::vector<PatternPlacement>>::success(std::move(result));
}

} // namespace blcad
