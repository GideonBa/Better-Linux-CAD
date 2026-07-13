#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class RevolveFeatureKind { Revolve, RevolveCut };
enum class RevolveExtentMode { Full, Angle, Symmetric };
enum class RevolveSide { Positive, Negative };

[[nodiscard]] std::string_view to_string(RevolveFeatureKind kind) noexcept;
[[nodiscard]] std::string_view to_string(RevolveExtentMode mode) noexcept;
[[nodiscard]] std::string_view to_string(RevolveSide side) noexcept;

// Full is the canonical 360-degree intent. Angle stores an authored magnitude
// in (0, 360) without principal-angle normalization and an explicit side.
// Symmetric stores the total included angle in (0, 360), centered on the
// profile plane. Geometry-level self-intersection is rejected by Block 62.
class RevolveAngleExtent {
public:
  [[nodiscard]] static RevolveAngleExtent full();
  [[nodiscard]] static Result<RevolveAngleExtent>
  angle(double angle_deg, RevolveSide side = RevolveSide::Positive);
  [[nodiscard]] static Result<RevolveAngleExtent> symmetric(double total_angle_deg);

  [[nodiscard]] RevolveExtentMode mode() const noexcept;
  [[nodiscard]] const std::optional<double>& angle_deg() const noexcept;
  [[nodiscard]] const std::optional<RevolveSide>& side() const noexcept;

private:
  RevolveAngleExtent(RevolveExtentMode mode, std::optional<double> angle_deg,
                     std::optional<RevolveSide> side);

  RevolveExtentMode mode_;
  std::optional<double> angle_deg_;
  std::optional<RevolveSide> side_;
};

class RevolveFeature {
public:
  [[nodiscard]] static Result<RevolveFeature>
  create_revolve(FeatureId id, std::string name, ProfileRegionReference profile,
                 AxisReference axis, RevolveAngleExtent extent,
                 FeatureBodyResultContext body_result_context);
  [[nodiscard]] static Result<RevolveFeature>
  create_revolve_cut(FeatureId id, std::string name, ProfileRegionReference profile,
                     AxisReference axis, RevolveAngleExtent extent,
                     FeatureBodyResultContext body_result_context);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] RevolveFeatureKind kind() const noexcept;
  [[nodiscard]] const ProfileRegionReference& profile() const noexcept;
  [[nodiscard]] const AxisReference& axis() const noexcept;
  [[nodiscard]] const RevolveAngleExtent& extent() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  [[nodiscard]] static Result<RevolveFeature>
  create(FeatureId id, std::string name, RevolveFeatureKind kind,
         ProfileRegionReference profile, AxisReference axis, RevolveAngleExtent extent,
         FeatureBodyResultContext body_result_context);

  RevolveFeature(FeatureId id, std::string name, RevolveFeatureKind kind,
                 ProfileRegionReference profile, AxisReference axis,
                 RevolveAngleExtent extent, FeatureBodyResultContext body_result_context);

  FeatureId id_;
  std::string name_;
  RevolveFeatureKind kind_;
  ProfileRegionReference profile_;
  AxisReference axis_;
  RevolveAngleExtent extent_;
  FeatureBodyResultContext body_result_context_;
};

} // namespace blcad
