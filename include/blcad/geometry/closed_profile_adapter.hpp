#pragma once

#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <string_view>
#include <vector>

namespace blcad::geometry {

class ClosedProfileAdapter {
public:
  [[nodiscard]] Result<GeometryShape> make_extruded_profile(
      const std::vector<Point3>& vertices, Vector3 direction, const Quantity& depth) const;

  [[nodiscard]] Result<GeometryShape> make_extruded_profile_with_holes(
      const std::vector<Point3>& outer_vertices,
      const std::vector<std::vector<Point3>>& inner_vertices,
      Vector3 direction, const Quantity& depth) const;

  [[nodiscard]] Result<GeometryShape> cut_profile_through_all(const GeometryShape& target,
                                                              const std::vector<Point3>& vertices,
                                                              Vector3 axis_direction) const;

  [[nodiscard]] Result<GeometryShape> cut_profile_with_holes_through_all(
      const GeometryShape& target, const std::vector<Point3>& outer_vertices,
      const std::vector<std::vector<Point3>>& inner_vertices, Vector3 axis_direction) const;
};

} // namespace blcad::geometry
