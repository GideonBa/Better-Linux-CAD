#pragma once

#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <array>

namespace blcad::geometry {

struct GeometryAffineTransform {
  std::array<double, 16> matrix{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};

  [[nodiscard]] static GeometryAffineTransform identity() noexcept;
  [[nodiscard]] static GeometryAffineTransform translation(Vector3 offset) noexcept;
  [[nodiscard]] static GeometryAffineTransform rotation(Point3 origin, Vector3 axis,
                                                        double angle_deg) noexcept;
  [[nodiscard]] static GeometryAffineTransform uniform_scale(Point3 center, double factor) noexcept;
  [[nodiscard]] Point3 transform_point(Point3 point) const noexcept;
  [[nodiscard]] Vector3 transform_vector(Vector3 vector) const noexcept;
  [[nodiscard]] GeometryAffineTransform
  followed_by(const GeometryAffineTransform& operation) const noexcept;
};

struct GeometryShapeBounds {
  Point3 minimum;
  Point3 maximum;
};

class BodyTransformAdapter {
public:
  [[nodiscard]] Result<GeometryShape> translate(const GeometryShape& shape, Vector3 offset) const;
  [[nodiscard]] Result<GeometryShape> rotate(const GeometryShape& shape, Point3 origin,
                                             Vector3 axis, double angle_deg) const;
  [[nodiscard]] Result<GeometryShape> uniform_scale(const GeometryShape& shape, Point3 center,
                                                    double factor) const;
  [[nodiscard]] Result<GeometryShapeBounds> bounds(const GeometryShape& shape) const;
};

} // namespace blcad::geometry
