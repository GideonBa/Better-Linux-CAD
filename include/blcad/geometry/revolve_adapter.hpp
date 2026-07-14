#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <vector>

namespace blcad::geometry {

class RevolveAdapter {
public:
  [[nodiscard]] Result<GeometryShape>
  revolve_profile(FeatureId feature_id, const std::vector<Point3>& vertices, Point3 axis_origin,
                  Vector3 axis_direction, double start_angle_deg, double sweep_angle_deg) const;

  [[nodiscard]] Result<GeometryShape>
  revolve_profile_with_holes(FeatureId feature_id, const std::vector<Point3>& outer_vertices,
                             const std::vector<std::vector<Point3>>& inner_vertices,
                             Point3 axis_origin, Vector3 axis_direction, double start_angle_deg,
                             double sweep_angle_deg) const;

  [[nodiscard]] Result<GeometryShape>
  revolve_profile_curves(FeatureId feature_id, const std::vector<ClosedProfileCurveSegment>& curves,
                         Point3 axis_origin, Vector3 axis_direction, double start_angle_deg,
                         double sweep_angle_deg) const;
};

} // namespace blcad::geometry
