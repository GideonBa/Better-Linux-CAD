#include "blcad/core/part_pattern_feature.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <unordered_set>
#include <utility>

namespace blcad {
namespace {

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
                             FeatureBodyResultContext body_result_context) {
  auto valid = validate_common(id, name, sources, direction, count_parameter);
  if (valid.has_error())
    return Result<LinearPatternFeature>::failure(valid.error());
  if (extent_parameter.empty())
    return Result<LinearPatternFeature>::failure(
        Error::validation(id.value(), "linear pattern extent parameter must not be empty"));
  return Result<LinearPatternFeature>::success(
      LinearPatternFeature(std::move(id), std::move(name), std::move(sources), std::move(direction),
                           std::move(count_parameter), extent_mode, std::move(extent_parameter),
                           direction_sign, std::move(body_result_context)));
}

LinearPatternFeature::LinearPatternFeature(FeatureId id, std::string name,
                                           std::vector<PatternSourceReference> sources,
                                           AxisReference direction, ParameterId count_parameter,
                                           LinearPatternExtentMode extent_mode,
                                           ParameterId extent_parameter,
                                           PatternDirectionSign direction_sign,
                                           FeatureBodyResultContext body_result_context)
    : id_(std::move(id)), name_(std::move(name)), sources_(std::move(sources)),
      direction_(std::move(direction)), count_parameter_(std::move(count_parameter)),
      extent_mode_(extent_mode), extent_parameter_(std::move(extent_parameter)),
      direction_sign_(direction_sign), body_result_context_(std::move(body_result_context)) {}

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

Result<CircularPatternFeature>
CircularPatternFeature::create(FeatureId id, std::string name,
                               std::vector<PatternSourceReference> sources, AxisReference axis,
                               ParameterId count_parameter, double total_angle_deg,
                               bool equal_spacing, FeatureBodyResultContext body_result_context) {
  auto valid = validate_common(id, name, sources, axis, count_parameter);
  if (valid.has_error())
    return Result<CircularPatternFeature>::failure(valid.error());
  if (!std::isfinite(total_angle_deg) || total_angle_deg <= 0.0 || total_angle_deg > 360.0)
    return Result<CircularPatternFeature>::failure(Error::validation(
        id.value(), "circular pattern total angle must be finite and in (0, 360] degrees"));
  if (!equal_spacing)
    return Result<CircularPatternFeature>::failure(
        Error::validation(id.value(), "circular pattern requires equal spacing in the first MVP"));
  return Result<CircularPatternFeature>::success(CircularPatternFeature(
      std::move(id), std::move(name), std::move(sources), std::move(axis),
      std::move(count_parameter), total_angle_deg, equal_spacing, std::move(body_result_context)));
}

CircularPatternFeature::CircularPatternFeature(FeatureId id, std::string name,
                                               std::vector<PatternSourceReference> sources,
                                               AxisReference axis, ParameterId count_parameter,
                                               double total_angle_deg, bool equal_spacing,
                                               FeatureBodyResultContext body_result_context)
    : id_(std::move(id)), name_(std::move(name)), sources_(std::move(sources)),
      axis_(std::move(axis)), count_parameter_(std::move(count_parameter)),
      total_angle_deg_(total_angle_deg), equal_spacing_(equal_spacing),
      body_result_context_(std::move(body_result_context)) {}

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

} // namespace blcad
