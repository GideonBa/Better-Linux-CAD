#include "blcad/core/sweep_feature.hpp"

#include <cmath>
#include <utility>

namespace blcad {

std::string_view to_string(SweepFeatureKind kind) noexcept {
  switch (kind) {
  case SweepFeatureKind::Sweep:
    return "sweep";
  case SweepFeatureKind::SweepCut:
    return "sweep_cut";
  case SweepFeatureKind::SweepSurface:
    return "sweep_surface";
  }
  return "sweep";
}

std::string_view to_string(SweepProfileKind kind) noexcept {
  return kind == SweepProfileKind::ClosedRegion ? "closed_region" : "open_path";
}

SweepProfileReference::SweepProfileReference(
    SweepProfileKind kind, std::variant<ProfileRegionReference, PathCurveId> source)
    : kind_(kind), source_(std::move(source)) {}

Result<SweepProfileReference>
SweepProfileReference::create_closed_region(ProfileRegionReference profile) {
  if (profile.role() != PartFeatureInputRole::SweepProfile)
    return Result<SweepProfileReference>::failure(Error::validation(
        profile.profile().value(), "closed sweep profile must use sweep_profile role"));
  return Result<SweepProfileReference>::success(
      SweepProfileReference(SweepProfileKind::ClosedRegion, std::move(profile)));
}

Result<SweepProfileReference> SweepProfileReference::create_open_path(PathCurveId path_curve) {
  if (path_curve.empty())
    return Result<SweepProfileReference>::failure(
        Error::validation("sweep_profile", "open sweep profile path id must not be empty"));
  return Result<SweepProfileReference>::success(
      SweepProfileReference(SweepProfileKind::OpenPath, std::move(path_curve)));
}

SweepProfileKind SweepProfileReference::kind() const noexcept {
  return kind_;
}
const std::variant<ProfileRegionReference, PathCurveId>&
SweepProfileReference::source() const noexcept {
  return source_;
}
std::string SweepProfileReference::source_node_id() const {
  return kind_ == SweepProfileKind::ClosedRegion
             ? std::get<ProfileRegionReference>(source_).source_node_id()
             : std::get<PathCurveId>(source_).value();
}

Result<SweepFeature>
SweepFeature::create_sweep(FeatureId id, std::string name, SweepProfileReference profile,
                           PathCurveId path, FeatureBodyResultContext body_result_context,
                           std::optional<PathOrientationRule> orientation_override,
                           std::optional<Vector3> fixed_up_vector_override,
                           std::optional<ParameterId> twist_parameter) {
  return create(std::move(id), std::move(name), SweepFeatureKind::Sweep, std::move(profile),
                std::move(path), std::move(body_result_context), orientation_override,
                fixed_up_vector_override, std::move(twist_parameter));
}

Result<SweepFeature>
SweepFeature::create_sweep_cut(FeatureId id, std::string name, SweepProfileReference profile,
                               PathCurveId path, FeatureBodyResultContext body_result_context,
                               std::optional<PathOrientationRule> orientation_override,
                               std::optional<Vector3> fixed_up_vector_override,
                               std::optional<ParameterId> twist_parameter) {
  return create(std::move(id), std::move(name), SweepFeatureKind::SweepCut, std::move(profile),
                std::move(path), std::move(body_result_context), orientation_override,
                fixed_up_vector_override, std::move(twist_parameter));
}

Result<SweepFeature>
SweepFeature::create_sweep_surface(FeatureId id, std::string name, SweepProfileReference profile,
                                   PathCurveId path, FeatureBodyResultContext body_result_context,
                                   std::optional<PathOrientationRule> orientation_override,
                                   std::optional<Vector3> fixed_up_vector_override,
                                   std::optional<ParameterId> twist_parameter) {
  return create(std::move(id), std::move(name), SweepFeatureKind::SweepSurface, std::move(profile),
                std::move(path), std::move(body_result_context), orientation_override,
                fixed_up_vector_override, std::move(twist_parameter));
}

Result<SweepFeature> SweepFeature::create(FeatureId id, std::string name, SweepFeatureKind kind,
                                          SweepProfileReference profile, PathCurveId path,
                                          FeatureBodyResultContext body_result_context,
                                          std::optional<PathOrientationRule> orientation_override,
                                          std::optional<Vector3> fixed_up_vector_override,
                                          std::optional<ParameterId> twist_parameter) {
  const std::string object_id = id.empty() ? "sweep_feature" : id.value();
  if (id.empty() || name.empty() || path.empty())
    return Result<SweepFeature>::failure(
        Error::validation(object_id, "sweep feature requires non-empty id, name, and path id"));
  const bool surface = kind == SweepFeatureKind::SweepSurface;
  if (surface != (profile.kind() == SweepProfileKind::OpenPath))
    return Result<SweepFeature>::failure(
        Error::validation(object_id, surface ? "surface sweep requires an open path profile"
                                             : "solid sweep requires a closed profile region"));
  if (kind == SweepFeatureKind::SweepCut &&
      body_result_context.operation_mode() != FeatureBodyOperationMode::Cut)
    return Result<SweepFeature>::failure(
        Error::validation(object_id, "sweep-cut requires cut body operation mode"));
  if (kind != SweepFeatureKind::SweepCut &&
      body_result_context.operation_mode() == FeatureBodyOperationMode::Cut)
    return Result<SweepFeature>::failure(
        Error::validation(object_id, "non-cut sweep cannot use cut body operation mode"));
  if (surface && body_result_context.operation_mode() != FeatureBodyOperationMode::NewBody)
    return Result<SweepFeature>::failure(
        Error::validation(object_id, "surface sweep requires new-body operation mode"));
  const bool fixed_override = orientation_override == std::optional<PathOrientationRule>(
                                                          PathOrientationRule::FixedUpVector);
  if (fixed_override != fixed_up_vector_override.has_value())
    return Result<SweepFeature>::failure(Error::validation(
        object_id, "fixed-up sweep override requires exactly one fixed up vector"));
  if (fixed_up_vector_override.has_value()) {
    const auto value = *fixed_up_vector_override;
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
        std::hypot(std::hypot(value.x, value.y), value.z) <= 1.0e-12)
      return Result<SweepFeature>::failure(
          Error::validation(object_id, "sweep fixed up vector must be finite and non-zero"));
  }
  return Result<SweepFeature>::success(
      SweepFeature(std::move(id), std::move(name), kind, std::move(profile), std::move(path),
                   std::move(body_result_context), orientation_override, fixed_up_vector_override,
                   std::move(twist_parameter)));
}

SweepFeature::SweepFeature(FeatureId id, std::string name, SweepFeatureKind kind,
                           SweepProfileReference profile, PathCurveId path,
                           FeatureBodyResultContext body_result_context,
                           std::optional<PathOrientationRule> orientation_override,
                           std::optional<Vector3> fixed_up_vector_override,
                           std::optional<ParameterId> twist_parameter)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), profile_(std::move(profile)),
      path_(std::move(path)), body_result_context_(std::move(body_result_context)),
      orientation_override_(orientation_override),
      fixed_up_vector_override_(fixed_up_vector_override),
      twist_parameter_(std::move(twist_parameter)) {}

const FeatureId& SweepFeature::id() const noexcept {
  return id_;
}
const std::string& SweepFeature::name() const noexcept {
  return name_;
}
SweepFeatureKind SweepFeature::kind() const noexcept {
  return kind_;
}
const SweepProfileReference& SweepFeature::profile() const noexcept {
  return profile_;
}
const PathCurveId& SweepFeature::path() const noexcept {
  return path_;
}
const std::optional<PathOrientationRule>& SweepFeature::orientation_override() const noexcept {
  return orientation_override_;
}
const std::optional<Vector3>& SweepFeature::fixed_up_vector_override() const noexcept {
  return fixed_up_vector_override_;
}
const std::optional<ParameterId>& SweepFeature::twist_parameter() const noexcept {
  return twist_parameter_;
}
const FeatureBodyResultContext& SweepFeature::body_result_context() const noexcept {
  return body_result_context_;
}

} // namespace blcad
