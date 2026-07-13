#include "blcad/core/revolve_feature.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <utility>

namespace blcad {

std::string_view to_string(RevolveFeatureKind kind) noexcept {
  return kind == RevolveFeatureKind::Revolve ? "revolve" : "revolve_cut";
}

std::string_view to_string(RevolveExtentMode mode) noexcept {
  switch (mode) {
  case RevolveExtentMode::Full:
    return "full";
  case RevolveExtentMode::Angle:
    return "angle";
  case RevolveExtentMode::Symmetric:
    return "symmetric";
  }
  return "full";
}

std::string_view to_string(RevolveSide side) noexcept {
  return side == RevolveSide::Positive ? "positive" : "negative";
}

RevolveAngleExtent::RevolveAngleExtent(RevolveExtentMode mode,
                                       std::optional<double> angle_deg,
                                       std::optional<RevolveSide> side)
    : mode_(mode), angle_deg_(angle_deg), side_(side) {}

RevolveAngleExtent RevolveAngleExtent::full() {
  return RevolveAngleExtent(RevolveExtentMode::Full, std::nullopt, std::nullopt);
}

Result<RevolveAngleExtent> RevolveAngleExtent::angle(double angle_deg, RevolveSide side) {
  if (!std::isfinite(angle_deg) || angle_deg <= 0.0 || angle_deg >= 360.0)
    return Result<RevolveAngleExtent>::failure(Error::validation(
        "revolve_extent", "partial revolve angle must be finite and strictly between 0 and 360 degrees"));
  return Result<RevolveAngleExtent>::success(
      RevolveAngleExtent(RevolveExtentMode::Angle, angle_deg, side));
}

Result<RevolveAngleExtent> RevolveAngleExtent::symmetric(double total_angle_deg) {
  if (!std::isfinite(total_angle_deg) || total_angle_deg <= 0.0 || total_angle_deg >= 360.0)
    return Result<RevolveAngleExtent>::failure(Error::validation(
        "revolve_extent", "symmetric revolve angle must be finite and strictly between 0 and 360 degrees"));
  return Result<RevolveAngleExtent>::success(
      RevolveAngleExtent(RevolveExtentMode::Symmetric, total_angle_deg, std::nullopt));
}

RevolveExtentMode RevolveAngleExtent::mode() const noexcept { return mode_; }
const std::optional<double>& RevolveAngleExtent::angle_deg() const noexcept { return angle_deg_; }
const std::optional<RevolveSide>& RevolveAngleExtent::side() const noexcept { return side_; }

Result<RevolveFeature>
RevolveFeature::create_revolve(FeatureId id, std::string name,
                               ProfileRegionReference profile, AxisReference axis,
                               RevolveAngleExtent extent,
                               FeatureBodyResultContext body_result_context) {
  return create(std::move(id), std::move(name), RevolveFeatureKind::Revolve,
                std::move(profile), std::move(axis), std::move(extent),
                std::move(body_result_context));
}

Result<RevolveFeature>
RevolveFeature::create_revolve_cut(FeatureId id, std::string name,
                                   ProfileRegionReference profile, AxisReference axis,
                                   RevolveAngleExtent extent,
                                   FeatureBodyResultContext body_result_context) {
  return create(std::move(id), std::move(name), RevolveFeatureKind::RevolveCut,
                std::move(profile), std::move(axis), std::move(extent),
                std::move(body_result_context));
}

Result<RevolveFeature>
RevolveFeature::create(FeatureId id, std::string name, RevolveFeatureKind kind,
                       ProfileRegionReference profile, AxisReference axis,
                       RevolveAngleExtent extent,
                       FeatureBodyResultContext body_result_context) {
  const std::string object_id = id.empty() ? "revolve_feature" : id.value();
  if (id.empty())
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve feature id must not be empty"));
  if (name.empty())
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve feature name must not be empty"));
  if (profile.role() != PartFeatureInputRole::RevolveProfile)
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve profile must use revolve_profile role"));
  if (axis.role() != PartFeatureInputRole::RevolveAxis ||
      axis.expected_capability() != PartFeatureInputCapability::Axis)
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve axis must use revolve_axis role and Axis capability"));
  if (kind == RevolveFeatureKind::RevolveCut &&
      body_result_context.operation_mode() != FeatureBodyOperationMode::Cut)
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve-cut feature requires cut body operation mode"));
  if (kind == RevolveFeatureKind::Revolve &&
      body_result_context.operation_mode() == FeatureBodyOperationMode::Cut)
    return Result<RevolveFeature>::failure(
        Error::validation(object_id, "revolve feature cannot use cut body operation mode"));
  return Result<RevolveFeature>::success(
      RevolveFeature(std::move(id), std::move(name), kind, std::move(profile),
                     std::move(axis), std::move(extent), std::move(body_result_context)));
}

RevolveFeature::RevolveFeature(FeatureId id, std::string name, RevolveFeatureKind kind,
                               ProfileRegionReference profile, AxisReference axis,
                               RevolveAngleExtent extent,
                               FeatureBodyResultContext body_result_context)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), profile_(std::move(profile)),
      axis_(std::move(axis)), extent_(std::move(extent)),
      body_result_context_(std::move(body_result_context)) {}

const FeatureId& RevolveFeature::id() const noexcept { return id_; }
const std::string& RevolveFeature::name() const noexcept { return name_; }
RevolveFeatureKind RevolveFeature::kind() const noexcept { return kind_; }
const ProfileRegionReference& RevolveFeature::profile() const noexcept { return profile_; }
const AxisReference& RevolveFeature::axis() const noexcept { return axis_; }
const RevolveAngleExtent& RevolveFeature::extent() const noexcept { return extent_; }
const FeatureBodyResultContext& RevolveFeature::body_result_context() const noexcept {
  return body_result_context_;
}

} // namespace blcad
