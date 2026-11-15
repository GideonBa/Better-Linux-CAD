#pragma once
#include "surface_join_verification.hpp"
#include <optional>
#include <vector>
namespace blcad::geometry::detail {
struct LoftPatchBoundary {
  TopoDS_Edge curve;
  std::optional<TopoDS_Face> support;
  LoftBoundaryContinuity continuity = LoftBoundaryContinuity::G1;
  bool reverse_normal = false;
};
struct VerifiedLoftPatch {
  TopoDS_Face face;
  std::vector<SurfaceJoinReport> joins;
  double curve_error_mm = 0;
};
[[nodiscard]] Result<VerifiedLoftPatch>
build_constrained_loft_patch(FeatureId id, const std::vector<LoftPatchBoundary>& boundaries,
                             const std::vector<TopoDS_Edge>& internal_curves,
                             double linear_tolerance_mm, double angular_tolerance_rad,
                             double curvature_tolerance_per_mm);
} // namespace blcad::geometry::detail
