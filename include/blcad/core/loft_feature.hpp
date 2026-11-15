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

// Boundary continuity is independent of longitudinal LoftContinuity.
enum class LoftBoundaryContinuity { G1, G2 };

struct LoftBoundaryCondition {
  std::string section_identity;
  FaceReference support;
  std::optional<BodyId> imported_source_body;
  LoftBoundaryContinuity continuity = LoftBoundaryContinuity::G1;
  bool reverse_normal = false;
  double tangent_scale = 1.0;
};

struct LoftGuideAnchor {
  std::string section_identity;
  std::optional<SketchEntityId> boundary_entity;
  double boundary_fraction = 0.0;
  double guide_fraction = 0.0;
};

struct LoftGuideBinding {
  PathCurveId guide;
  bool reversed = false;
  std::vector<LoftGuideAnchor> anchors;
};

struct LoftPatchBoundaryCondition {
  LoftBoundaryCondition condition;
  PathCurveId from_guide;
  PathCurveId to_guide;
};

struct LoftConstraintIntent {
  ParameterId linear_tolerance;
  double angular_tolerance_rad = 1e-5;
  double curvature_tolerance_per_mm = 1e-5;
  std::optional<LoftBoundaryCondition> start;
  std::optional<LoftBoundaryCondition> end;
  std::vector<LoftGuideBinding> guides;
  std::vector<LoftPatchBoundaryCondition> patch_boundaries{};
  [[nodiscard]] std::vector<const LoftBoundaryCondition*> boundary_conditions() const {
    std::vector<const LoftBoundaryCondition*> result;
    if (start)
      result.push_back(&*start);
    if (end)
      result.push_back(&*end);
    for (const auto& patch : patch_boundaries)
      result.push_back(&patch.condition);
    return result;
  }
};

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
  [[nodiscard]] std::string identity() const;

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

  [[nodiscard]] Result<LoftFeature>
  with_constraints(std::optional<LoftConstraintIntent> constraints) const;
  [[nodiscard]] const std::optional<LoftConstraintIntent>& constraints() const noexcept;

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
  std::optional<LoftConstraintIntent> constraints_;
};

} // namespace blcad
