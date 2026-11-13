#pragma once

#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/derived_curve.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace blcad::geometry {

enum class ClosedProfileCurveKind {
  Line,
  CircularArc,
  CubicBezierSpline,
  DerivedEquation,
};

struct ClosedProfileCurveSegment {
  ClosedProfileCurveKind kind;
  Point3 start;
  Point3 mid;
  Point3 end;
  Point3 control2{};
  std::array<double, 4> weights{1, 1, 1, 1};
  DerivedCurveRef derived_curve{};
};

struct AxisProjectionSpan {
  double minimum = 0.0;
  double maximum = 0.0;
};

class ClosedProfileAdapter {
public:
  [[nodiscard]] Result<std::size_t> validate_planar_tool_footprint(const GeometryShape& support,
                                                                   const GeometryShape& tool,
                                                                   Point3 origin,
                                                                   Vector3 normal) const;
  [[nodiscard]] Result<GeometryShape> make_extruded_profile(const std::vector<Point3>& vertices,
                                                            Vector3 direction,
                                                            const Quantity& depth) const;

  [[nodiscard]] Result<GeometryShape>
  make_extruded_profile_span(const std::vector<Point3>& vertices, Vector3 direction,
                             double start_distance_mm, double end_distance_mm,
                             std::optional<double> taper_angle_deg = std::nullopt) const;

  [[nodiscard]] Result<AxisProjectionSpan>
  projection_span(const GeometryShape& target, Point3 origin, Vector3 axis_direction) const;

  [[nodiscard]] Result<double> first_intersection_distance(const GeometryShape& target,
                                                           Point3 origin,
                                                           Vector3 axis_direction) const;

  // Ordered material intervals on the complete axis. Tangencies and boundary-only
  // contacts are excluded using actual solid classification between intersections.
  [[nodiscard]] Result<std::vector<AxisProjectionSpan>>
  axis_material_spans(const GeometryShape& target, Point3 origin, Vector3 axis_direction) const;

  // Select the first separated material region of a clipped cylindrical tool.
  // A connected region spanning a later axis interval is ambiguous and rejected.
  [[nodiscard]] Result<GeometryShape>
  first_forward_material_component(const GeometryShape& clipped_tool, Point3 origin,
                                   Vector3 axis_direction) const;

  // Require the complete circular footprint on one actual trimmed planar face.
  // Geometry is transient evidence; no traversal index becomes authored identity.
  [[nodiscard]] Result<std::size_t> validate_planar_circle_footprint(const GeometryShape& support,
                                                                     Point3 center, Vector3 normal,
                                                                     double radius_mm) const;
  // Clip to the side containing kept_point and require the complete (possibly
  // elliptical) plane section inside the actual finite trimmed face.
  [[nodiscard]] Result<GeometryShape> terminate_at_planar_face(const GeometryShape& tool,
                                                               const GeometryShape& face,
                                                               Point3 kept_point) const;

  [[nodiscard]] Result<GeometryShape>
  make_extruded_profile_with_holes(const std::vector<Point3>& outer_vertices,
                                   const std::vector<std::vector<Point3>>& inner_vertices,
                                   Vector3 direction, const Quantity& depth) const;

  [[nodiscard]] Result<GeometryShape>
  make_extruded_profile_from_curves(const std::vector<ClosedProfileCurveSegment>& curves,
                                    Vector3 direction, const Quantity& depth) const;

  [[nodiscard]] Result<GeometryShape> cut_profile_through_all(const GeometryShape& target,
                                                              const std::vector<Point3>& vertices,
                                                              Vector3 axis_direction) const;

  [[nodiscard]] Result<GeometryShape> cut_profile_with_holes_through_all(
      const GeometryShape& target, const std::vector<Point3>& outer_vertices,
      const std::vector<std::vector<Point3>>& inner_vertices, Vector3 axis_direction) const;

  [[nodiscard]] Result<GeometryShape>
  cut_profile_curves_through_all(const GeometryShape& target,
                                 const std::vector<ClosedProfileCurveSegment>& curves,
                                 Vector3 axis_direction) const;
};

} // namespace blcad::geometry
