#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/path_curve.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace blcad {

enum class SweepFeatureKind { Sweep, SweepCut, SweepSurface };
enum class SweepProfileKind { ClosedRegion, OpenPath };

[[nodiscard]] std::string_view to_string(SweepFeatureKind kind) noexcept;
[[nodiscard]] std::string_view to_string(SweepProfileKind kind) noexcept;

class SweepProfileReference {
public:
  [[nodiscard]] static Result<SweepProfileReference>
  create_closed_region(ProfileRegionReference profile);
  [[nodiscard]] static Result<SweepProfileReference> create_open_path(PathCurveId path_curve);

  [[nodiscard]] SweepProfileKind kind() const noexcept;
  [[nodiscard]] const std::variant<ProfileRegionReference, PathCurveId>& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  SweepProfileReference(SweepProfileKind kind,
                        std::variant<ProfileRegionReference, PathCurveId> source);
  SweepProfileKind kind_;
  std::variant<ProfileRegionReference, PathCurveId> source_;
};

class SweepFeature {
public:
  [[nodiscard]] static Result<SweepFeature>
  create_sweep(FeatureId id, std::string name, SweepProfileReference profile, PathCurveId path,
               FeatureBodyResultContext body_result_context,
               std::optional<PathOrientationRule> orientation_override = std::nullopt,
               std::optional<Vector3> fixed_up_vector_override = std::nullopt,
               std::optional<ParameterId> twist_parameter = std::nullopt);
  [[nodiscard]] static Result<SweepFeature>
  create_sweep_cut(FeatureId id, std::string name, SweepProfileReference profile, PathCurveId path,
                   FeatureBodyResultContext body_result_context,
                   std::optional<PathOrientationRule> orientation_override = std::nullopt,
                   std::optional<Vector3> fixed_up_vector_override = std::nullopt,
                   std::optional<ParameterId> twist_parameter = std::nullopt);
  [[nodiscard]] static Result<SweepFeature>
  create_sweep_surface(FeatureId id, std::string name, SweepProfileReference profile,
                       PathCurveId path, FeatureBodyResultContext body_result_context,
                       std::optional<PathOrientationRule> orientation_override = std::nullopt,
                       std::optional<Vector3> fixed_up_vector_override = std::nullopt,
                       std::optional<ParameterId> twist_parameter = std::nullopt);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] SweepFeatureKind kind() const noexcept;
  [[nodiscard]] const SweepProfileReference& profile() const noexcept;
  [[nodiscard]] const PathCurveId& path() const noexcept;
  [[nodiscard]] const std::optional<PathOrientationRule>& orientation_override() const noexcept;
  [[nodiscard]] const std::optional<Vector3>& fixed_up_vector_override() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& twist_parameter() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  [[nodiscard]] static Result<SweepFeature>
  create(FeatureId id, std::string name, SweepFeatureKind kind, SweepProfileReference profile,
         PathCurveId path, FeatureBodyResultContext body_result_context,
         std::optional<PathOrientationRule> orientation_override,
         std::optional<Vector3> fixed_up_vector_override,
         std::optional<ParameterId> twist_parameter);
  SweepFeature(FeatureId id, std::string name, SweepFeatureKind kind, SweepProfileReference profile,
               PathCurveId path, FeatureBodyResultContext body_result_context,
               std::optional<PathOrientationRule> orientation_override,
               std::optional<Vector3> fixed_up_vector_override,
               std::optional<ParameterId> twist_parameter);

  FeatureId id_;
  std::string name_;
  SweepFeatureKind kind_;
  SweepProfileReference profile_;
  PathCurveId path_;
  FeatureBodyResultContext body_result_context_;
  std::optional<PathOrientationRule> orientation_override_;
  std::optional<Vector3> fixed_up_vector_override_;
  std::optional<ParameterId> twist_parameter_;
};

} // namespace blcad
