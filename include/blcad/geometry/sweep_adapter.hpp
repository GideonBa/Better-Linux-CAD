#pragma once

#include "blcad/core/path_curve.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <optional>
#include <vector>

namespace blcad::geometry {

enum class SweepPathSegmentKind {
  Line,
  CircularArc,
  DerivedNative,
  DerivedEquation = DerivedNative
};

struct SweepPathSegment {
  SweepPathSegmentKind kind = SweepPathSegmentKind::Line;
  Point3 start;
  Point3 mid;
  Point3 end;
  DerivedCurveRef derived_curve{};
  // Zero retains historical execution, including its twist interpretation.
  double modeling_tolerance_mm = 0;
};

class SweepAdapter {
public:
  [[nodiscard]] Result<double>
  measure_path_length(PathCurveId path, const std::vector<SweepPathSegment>& segments) const;
  [[nodiscard]] Result<GeometryShape>
  path_shape(PathCurveId path, const std::vector<SweepPathSegment>& segments, bool closed) const;
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
