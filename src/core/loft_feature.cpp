#include "blcad/core/loft_feature.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace blcad {

std::string_view to_string(LoftFeatureKind kind) noexcept {
  switch (kind) {
  case LoftFeatureKind::Loft:
    return "loft";
  case LoftFeatureKind::LoftCut:
    return "loft_cut";
  case LoftFeatureKind::LoftSurface:
    return "loft_surface";
  }
  return "loft";
}

std::string_view to_string(LoftSectionKind kind) noexcept {
  return kind == LoftSectionKind::ClosedRegion ? "closed_region" : "open_path";
}

std::string_view to_string(LoftContinuity continuity) noexcept {
  switch (continuity) {
  case LoftContinuity::C0:
    return "c0";
  case LoftContinuity::G1:
    return "g1";
  case LoftContinuity::G2:
    return "g2";
  }
  return "c0";
}

Result<ProfileSectionReference> ProfileSectionReference::create_closed_region(
    ProfileRegionReference profile, std::optional<SketchEntityId> alignment_reference,
    bool flip_normal, std::optional<ParameterId> rotation_offset) {
  if ((alignment_reference.has_value() && alignment_reference->empty()) ||
      (rotation_offset.has_value() && rotation_offset->empty()))
    return Result<ProfileSectionReference>::failure(Error::validation(
        profile.source_node_id(), "loft section alignment and rotation ids must not be empty"));
  return Result<ProfileSectionReference>::success(ProfileSectionReference(
      LoftSectionKind::ClosedRegion, std::move(profile), std::move(alignment_reference),
      flip_normal, std::move(rotation_offset)));
}

Result<ProfileSectionReference>
ProfileSectionReference::create_open_path(PathCurveId path_curve, bool flip_normal,
                                          std::optional<ParameterId> rotation_offset) {
  if (path_curve.empty() || (rotation_offset.has_value() && rotation_offset->empty()))
    return Result<ProfileSectionReference>::failure(
        Error::validation(path_curve.empty() ? "loft_section" : path_curve.value(),
                          "open loft section requires non-empty path and rotation ids"));
  return Result<ProfileSectionReference>::success(
      ProfileSectionReference(LoftSectionKind::OpenPath, std::move(path_curve), std::nullopt,
                              flip_normal, std::move(rotation_offset)));
}

ProfileSectionReference::ProfileSectionReference(
    LoftSectionKind kind, std::variant<ProfileRegionReference, PathCurveId> source,
    std::optional<SketchEntityId> alignment_reference, bool flip_normal,
    std::optional<ParameterId> rotation_offset)
    : kind_(kind), source_(std::move(source)), alignment_reference_(std::move(alignment_reference)),
      flip_normal_(flip_normal), rotation_offset_(std::move(rotation_offset)) {}

LoftSectionKind ProfileSectionReference::kind() const noexcept {
  return kind_;
}
const std::variant<ProfileRegionReference, PathCurveId>&
ProfileSectionReference::source() const noexcept {
  return source_;
}
const std::optional<SketchEntityId>& ProfileSectionReference::alignment_reference() const noexcept {
  return alignment_reference_;
}
bool ProfileSectionReference::flip_normal() const noexcept {
  return flip_normal_;
}
const std::optional<ParameterId>& ProfileSectionReference::rotation_offset() const noexcept {
  return rotation_offset_;
}
std::string ProfileSectionReference::source_node_id() const {
  return kind_ == LoftSectionKind::ClosedRegion
             ? std::get<ProfileRegionReference>(source_).source_node_id()
             : std::get<PathCurveId>(source_).value();
}

std::string ProfileSectionReference::identity() const {
  const auto frame = [](const std::string& value) {
    return std::to_string(value.size()) + ":" + value + "/";
  };
  if (kind_ == LoftSectionKind::OpenPath)
    return "path/" + frame(std::get<PathCurveId>(source_).value());
  const auto& profile = std::get<ProfileRegionReference>(source_);
  return "profile/" + frame(profile.sketch().value()) + frame(profile.profile().value());
}

Result<LoftFeature>
LoftFeature::with_constraints(std::optional<LoftConstraintIntent> constraints) const {
  const auto fail = [&](std::string message) {
    return Result<LoftFeature>::failure(Error::validation(id_.value(), std::move(message)));
  };
  if (constraints) {
    const auto& intent = *constraints;
    if (intent.linear_tolerance.empty() || !std::isfinite(intent.angular_tolerance_rad) ||
        intent.angular_tolerance_rad <= 0.0 || intent.angular_tolerance_rad > 0.1 ||
        !std::isfinite(intent.curvature_tolerance_per_mm) ||
        intent.curvature_tolerance_per_mm <= 0.0 || intent.curvature_tolerance_per_mm > 1.0)
      return fail(
          "Loft verification requires a Length tolerance and bounded angular/curvature tolerances");
    if (!intent.start && !intent.end && intent.guides.empty())
      return fail("Loft constraints require authored support or guide bindings");
    for (const bool first : {true, false}) {
      const auto& condition = first ? intent.start : intent.end;
      if (!condition)
        continue;
      if (condition->section_identity != (first ? sections_.front() : sections_.back()).identity())
        return fail("Loft boundary support must bind the corresponding terminal section identity");
      if (condition->support.role() != PartFeatureInputRole::LoftBoundarySupport ||
          (condition->continuity != LoftBoundaryContinuity::G1 &&
           condition->continuity != LoftBoundaryContinuity::G2) ||
          !std::isfinite(condition->tangent_scale) || condition->tangent_scale <= 0.0 ||
          condition->tangent_scale > 1e6)
        return fail(
            "Loft boundary support requires its Face role, G1/G2 and a positive tangent scale");
      const bool imported =
          condition->support.source_kind() == PartFeatureInputSourceKind::ImportedTopology;
      if (imported != condition->imported_source_body.has_value() ||
          (condition->imported_source_body && condition->imported_source_body->empty()))
        return fail("Imported Loft support requires exactly one explicit base source Body");
    }
    if (intent.guides.size() != guide_curves_.size())
      return fail("Constrained Loft requires one ordered binding for every authored guide");
    for (std::size_t g = 0; g < intent.guides.size(); ++g) {
      for (std::size_t other = 0; other < g; ++other)
        for (std::size_t i = 0; i < sections_.size() && i < intent.guides[g].anchors.size() &&
                                i < intent.guides[other].anchors.size();
             ++i) {
          const auto& a = intent.guides[g].anchors[i];
          const auto& b = intent.guides[other].anchors[i];
          const bool closed =
              sections_[i].kind() == LoftSectionKind::ClosedRegion && !a.boundary_entity;
          const auto fraction = [closed](double f) { return closed && f == 1.0 ? 0.0 : f; };
          if (a.boundary_entity == b.boundary_entity &&
              fraction(a.boundary_fraction) == fraction(b.boundary_fraction))
            return fail("Loft guides must use distinct authored boundary anchors at every section");
        }
      const auto& binding = intent.guides[g];
      if (binding.guide != guide_curves_[g] || binding.anchors.size() != sections_.size())
        return fail("Loft guide binding must retain guide order and every logical section");
      double previous = -1.0;
      for (std::size_t i = 0; i < binding.anchors.size(); ++i) {
        const auto& anchor = binding.anchors[i];
        if (anchor.section_identity != sections_[i].identity() ||
            (anchor.boundary_entity && (anchor.boundary_entity->empty() ||
                                        sections_[i].kind() != LoftSectionKind::ClosedRegion)) ||
            !std::isfinite(anchor.boundary_fraction) || anchor.boundary_fraction < 0.0 ||
            anchor.boundary_fraction > 1.0 || !std::isfinite(anchor.guide_fraction) ||
            anchor.guide_fraction < 0.0 || anchor.guide_fraction > 1.0)
          return fail(
              "Loft guide anchor requires a logical section and normalized true-length fractions");
        const double station =
            binding.reversed ? 1.0 - anchor.guide_fraction : anchor.guide_fraction;
        if (station <= previous)
          return fail("Loft guide stations must increase strictly in the authored direction");
        previous = station;
      }
    }
  }
  auto copy = *this;
  copy.constraints_ = std::move(constraints);
  return Result<LoftFeature>::success(std::move(copy));
}

const std::optional<LoftConstraintIntent>& LoftFeature::constraints() const noexcept {
  return constraints_;
}

Result<LoftFeature> LoftFeature::create_loft(FeatureId id, std::string name,
                                             std::vector<ProfileSectionReference> sections,
                                             FeatureBodyResultContext body_result_context,
                                             std::optional<PathCurveId> path_curve,
                                             std::vector<PathCurveId> guide_curves,
                                             LoftContinuity continuity) {
  return create(std::move(id), std::move(name), LoftFeatureKind::Loft, std::move(sections),
                std::move(body_result_context), std::move(path_curve), std::move(guide_curves),
                continuity);
}

Result<LoftFeature> LoftFeature::create_loft_cut(FeatureId id, std::string name,
                                                 std::vector<ProfileSectionReference> sections,
                                                 FeatureBodyResultContext body_result_context,
                                                 std::optional<PathCurveId> path_curve,
                                                 std::vector<PathCurveId> guide_curves,
                                                 LoftContinuity continuity) {
  return create(std::move(id), std::move(name), LoftFeatureKind::LoftCut, std::move(sections),
                std::move(body_result_context), std::move(path_curve), std::move(guide_curves),
                continuity);
}

Result<LoftFeature> LoftFeature::create_loft_surface(FeatureId id, std::string name,
                                                     std::vector<ProfileSectionReference> sections,
                                                     FeatureBodyResultContext body_result_context,
                                                     std::optional<PathCurveId> path_curve,
                                                     std::vector<PathCurveId> guide_curves,
                                                     LoftContinuity continuity) {
  return create(std::move(id), std::move(name), LoftFeatureKind::LoftSurface, std::move(sections),
                std::move(body_result_context), std::move(path_curve), std::move(guide_curves),
                continuity);
}

Result<LoftFeature> LoftFeature::create(FeatureId id, std::string name, LoftFeatureKind kind,
                                        std::vector<ProfileSectionReference> sections,
                                        FeatureBodyResultContext body_result_context,
                                        std::optional<PathCurveId> path_curve,
                                        std::vector<PathCurveId> guide_curves,
                                        LoftContinuity continuity) {
  const std::string object_id = id.empty() ? "loft_feature" : id.value();
  if (id.empty() || name.empty())
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft feature requires non-empty id and name"));
  if (continuity != LoftContinuity::C0 && continuity != LoftContinuity::G1 &&
      continuity != LoftContinuity::G2)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "Unknown longitudinal Loft continuity"));
  if (sections.size() < 2U)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft feature requires at least two ordered sections"));
  const bool surface = kind == LoftFeatureKind::LoftSurface;
  const LoftSectionKind section_kind = sections.front().kind();
  if (std::any_of(sections.begin(), sections.end(),
                  [section_kind](const auto& section) { return section.kind() != section_kind; }))
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft sections must use one homogeneous profile kind"));
  if (!surface && section_kind != LoftSectionKind::ClosedRegion)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "solid loft requires closed profile sections"));
  std::unordered_set<std::string> section_sources;
  for (const auto& section : sections)
    if (!section_sources.insert(section.identity()).second)
      return Result<LoftFeature>::failure(
          Error::validation(object_id, "loft sections must have distinct ordered sources"));
  if (kind == LoftFeatureKind::LoftCut &&
      body_result_context.operation_mode() != FeatureBodyOperationMode::Cut)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft-cut requires cut body operation mode"));
  if (kind != LoftFeatureKind::LoftCut &&
      body_result_context.operation_mode() == FeatureBodyOperationMode::Cut)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "non-cut loft cannot use cut body operation mode"));
  if (surface && body_result_context.operation_mode() != FeatureBodyOperationMode::NewBody)
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "surface loft requires new-body operation mode"));
  if (path_curve.has_value() && path_curve->empty())
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft path curve id must not be empty"));
  if (path_curve.has_value() &&
      std::any_of(sections.begin(), sections.end(), [&path_curve](const auto& section) {
        return section.kind() == LoftSectionKind::OpenPath &&
               std::get<PathCurveId>(section.source()) == *path_curve;
      }))
    return Result<LoftFeature>::failure(
        Error::validation(object_id, "loft path must differ from open section paths"));
  std::unordered_set<std::string> guides;
  for (const auto& guide : guide_curves) {
    if (guide.empty() || !guides.insert(guide.value()).second ||
        (path_curve.has_value() && guide == *path_curve) ||
        std::any_of(sections.begin(), sections.end(), [&guide](const auto& section) {
          return section.kind() == LoftSectionKind::OpenPath &&
                 std::get<PathCurveId>(section.source()) == guide;
        }))
      return Result<LoftFeature>::failure(Error::validation(
          object_id,
          "loft guides must be non-empty, unique, and distinct from path and section sources"));
  }
  return Result<LoftFeature>::success(LoftFeature(
      std::move(id), std::move(name), kind, std::move(sections), std::move(body_result_context),
      std::move(path_curve), std::move(guide_curves), continuity));
}

LoftFeature::LoftFeature(FeatureId id, std::string name, LoftFeatureKind kind,
                         std::vector<ProfileSectionReference> sections,
                         FeatureBodyResultContext body_result_context,
                         std::optional<PathCurveId> path_curve,
                         std::vector<PathCurveId> guide_curves, LoftContinuity continuity)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), sections_(std::move(sections)),
      body_result_context_(std::move(body_result_context)), path_curve_(std::move(path_curve)),
      guide_curves_(std::move(guide_curves)), continuity_(continuity) {}

const FeatureId& LoftFeature::id() const noexcept {
  return id_;
}
const std::string& LoftFeature::name() const noexcept {
  return name_;
}
LoftFeatureKind LoftFeature::kind() const noexcept {
  return kind_;
}
const std::vector<ProfileSectionReference>& LoftFeature::sections() const noexcept {
  return sections_;
}
const std::optional<PathCurveId>& LoftFeature::path_curve() const noexcept {
  return path_curve_;
}
const std::vector<PathCurveId>& LoftFeature::guide_curves() const noexcept {
  return guide_curves_;
}
LoftContinuity LoftFeature::continuity() const noexcept {
  return continuity_;
}
const FeatureBodyResultContext& LoftFeature::body_result_context() const noexcept {
  return body_result_context_;
}

} // namespace blcad
