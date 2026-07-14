#pragma once

#include "blcad/core/path_curve.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <optional>
#include <vector>

namespace blcad::geometry {

enum class SweepPathSegmentKind { Line, CircularArc };

struct SweepPathSegment {
  SweepPathSegmentKind kind = SweepPathSegmentKind::Line;
  Point3 start;
  Point3 mid;
  Point3 end;
};

class SweepAdapter {
public:
  [[nodiscard]] Result<GeometryShape> sweep_closed_profile(
      FeatureId feature_id, const std::vector<ClosedProfileCurveSegment>& profile,
      const std::vector<SweepPathSegment>& path, PathOrientationRule orientation_rule,
      std::optional<Vector3> fixed_up_vector = std::nullopt, double twist_angle_deg = 0.0,
      const std::vector<SweepPathSegment>* guide = nullptr) const;

  [[nodiscard]] Result<GeometryShape> sweep_open_profile(
      FeatureId feature_id, const std::vector<SweepPathSegment>& profile,
      const std::vector<SweepPathSegment>& path, PathOrientationRule orientation_rule,
      std::optional<Vector3> fixed_up_vector = std::nullopt, double twist_angle_deg = 0.0,
      const std::vector<SweepPathSegment>* guide = nullptr) const;
};

} // namespace blcad::geometry
