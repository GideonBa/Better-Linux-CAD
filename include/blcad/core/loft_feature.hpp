#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/path_curve.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

enum class LoftFeatureKind { Loft, LoftCut, LoftSurface };
enum class LoftSectionKind { ClosedRegion, OpenPath };
enum class LoftContinuity { C0, G1, G2 };

[[nodiscard]] std::string_view to_string(LoftFeatureKind kind) noexcept;
[[nodiscard]] std::string_view to_string(LoftSectionKind kind) noexcept;
[[nodiscard]] std::string_view to_string(LoftContinuity continuity) noexcept;

class ProfileSectionReference {
public:
  [[nodiscard]] static Result<ProfileSectionReference>
  create_closed_region(ProfileRegionReference profile,
                       std::optional<SketchEntityId> alignment_reference = std::nullopt,
                       bool flip_normal = false,
                       std::optional<ParameterId> rotation_offset = std::nullopt);
  [[nodiscard]] static Result<ProfileSectionReference>
  create_open_path(PathCurveId path_curve, bool flip_normal = false,
                   std::optional<ParameterId> rotation_offset = std::nullopt);

  [[nodiscard]] LoftSectionKind kind() const noexcept;
  [[nodiscard]] const std::variant<ProfileRegionReference, PathCurveId>& source() const noexcept;
  [[nodiscard]] const std::optional<SketchEntityId>& alignment_reference() const noexcept;
  [[nodiscard]] bool flip_normal() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& rotation_offset() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  ProfileSectionReference(LoftSectionKind kind,
                          std::variant<ProfileRegionReference, PathCurveId> source,
                          std::optional<SketchEntityId> alignment_reference, bool flip_normal,
                          std::optional<ParameterId> rotation_offset);

  LoftSectionKind kind_;
  std::variant<ProfileRegionReference, PathCurveId> source_;
  std::optional<SketchEntityId> alignment_reference_;
  bool flip_normal_ = false;
  std::optional<ParameterId> rotation_offset_;
};

class LoftFeature {
public:
  [[nodiscard]] static Result<LoftFeature>
  create_loft(FeatureId id, std::string name, std::vector<ProfileSectionReference> sections,
              FeatureBodyResultContext body_result_context,
              std::optional<PathCurveId> path_curve = std::nullopt,
              std::vector<PathCurveId> guide_curves = {},
              LoftContinuity continuity = LoftContinuity::C0);
  [[nodiscard]] static Result<LoftFeature>
  create_loft_cut(FeatureId id, std::string name, std::vector<ProfileSectionReference> sections,
                  FeatureBodyResultContext body_result_context,
                  std::optional<PathCurveId> path_curve = std::nullopt,
                  std::vector<PathCurveId> guide_curves = {},
                  LoftContinuity continuity = LoftContinuity::C0);
  [[nodiscard]] static Result<LoftFeature>
  create_loft_surface(FeatureId id, std::string name, std::vector<ProfileSectionReference> sections,
                      FeatureBodyResultContext body_result_context,
                      std::optional<PathCurveId> path_curve = std::nullopt,
                      std::vector<PathCurveId> guide_curves = {},
                      LoftContinuity continuity = LoftContinuity::C0);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] LoftFeatureKind kind() const noexcept;
  [[nodiscard]] const std::vector<ProfileSectionReference>& sections() const noexcept;
  [[nodiscard]] const std::optional<PathCurveId>& path_curve() const noexcept;
  [[nodiscard]] const std::vector<PathCurveId>& guide_curves() const noexcept;
  [[nodiscard]] LoftContinuity continuity() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  [[nodiscard]] static Result<LoftFeature>
  create(FeatureId id, std::string name, LoftFeatureKind kind,
         std::vector<ProfileSectionReference> sections,
         FeatureBodyResultContext body_result_context, std::optional<PathCurveId> path_curve,
         std::vector<PathCurveId> guide_curves, LoftContinuity continuity);
  LoftFeature(FeatureId id, std::string name, LoftFeatureKind kind,
              std::vector<ProfileSectionReference> sections,
              FeatureBodyResultContext body_result_context, std::optional<PathCurveId> path_curve,
              std::vector<PathCurveId> guide_curves, LoftContinuity continuity);

  FeatureId id_;
  std::string name_;
  LoftFeatureKind kind_;
  std::vector<ProfileSectionReference> sections_;
  FeatureBodyResultContext body_result_context_;
  std::optional<PathCurveId> path_curve_;
  std::vector<PathCurveId> guide_curves_;
  LoftContinuity continuity_ = LoftContinuity::C0;
};

} // namespace blcad
