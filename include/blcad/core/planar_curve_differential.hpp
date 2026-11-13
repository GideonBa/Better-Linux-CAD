#pragma once

#include "blcad/core/rational_bezier.hpp"
#include "blcad/core/result.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace blcad {

// At a regular endpoint the unit tangent points into the entity. Curvature is
// dT/ds and is unchanged by reversal or a regular change of parameter speed.
struct PlanarEndpointDifferential {
  Point2 inward_tangent;
  Point2 curvature_vector;
};

class PlanarCurveDifferential {
public:
  [[nodiscard]] static Result<PlanarEndpointDifferential>
  line(Point2 endpoint, Point2 other, double length_scale, const std::string& object_id) {
    const Point2 direction = subtract(other, endpoint);
    const double speed = magnitude(direction);
    if (!regular(direction, speed, length_scale))
      return failure(object_id, "Smooth requires a finite nonzero line endpoint speed");
    return Result<PlanarEndpointDifferential>::success({divide(direction, speed), {0, 0}});
  }

  [[nodiscard]] static Result<PlanarEndpointDifferential> spline(const RationalBezier& curve,
                                                                 bool at_end, double length_scale,
                                                                 const std::string& object_id) {
    if (!curve.valid())
      return failure(object_id, "Smooth requires finite positive rational spline weights");
    // Remove absolute translation and common homogeneous weight scale before
    // differentiation; neither changes the geometric endpoint differential.
    RationalBezier local = curve;
    const auto origin = curve.poles[at_end ? 3U : 0U];
    const double weight_scale = *std::max_element(curve.weights.begin(), curve.weights.end());
    for (std::size_t i = 0; i < local.poles.size(); ++i) {
      local.poles[i] = subtract(local.poles[i], origin);
      local.weights[i] /= weight_scale;
    }
    const double parameter = at_end ? 1.0 : 0.0;
    const Point2 first = local.derivative(parameter);
    const Point2 second = local.second_derivative(parameter);
    const double speed = magnitude(first);
    if (!regular(first, speed, length_scale) || !finite(second))
      return failure(object_id, "Smooth requires a finite nonzero spline endpoint speed");
    const Point2 tangent = divide(first, speed);
    const double curvature = (cross(tangent, second) / speed) / speed;
    const Point2 vector{-tangent.y * curvature, tangent.x * curvature};
    if (!finite(vector))
      return failure(object_id, "Smooth endpoint curvature exceeds numeric range");
    return Result<PlanarEndpointDifferential>::success(
        {{at_end ? -tangent.x : tangent.x, at_end ? -tangent.y : tangent.y}, vector});
  }

  [[nodiscard]] static Result<PlanarEndpointDifferential> arc(const std::array<Point2, 3>& points,
                                                              bool at_end, double length_scale,
                                                              const std::string& object_id) {
    if (!std::isfinite(length_scale) || length_scale <= 0 || !finite(points[0]) ||
        !finite(points[1]) || !finite(points[2]))
      return failure(object_id, "Smooth requires finite arc coordinates and scale");
    // Translate before squaring: a remote origin must not change the arc's
    // curvature through cancellation of absolute-coordinate squared norms.
    const Point2 u = divide(subtract(points[1], points[0]), length_scale);
    const Point2 v = divide(subtract(points[2], points[0]), length_scale);
    const double u_length = magnitude(u), v_length = magnitude(v);
    const double determinant = cross(u, v);
    if (!std::isfinite(determinant) || u_length <= 1e-12 || v_length <= 1e-12 ||
        std::abs(determinant) <= 1e-12 * u_length * v_length)
      return failure(object_id, "Smooth requires a regular noncollinear three-point arc");
    const double u_squared = u.x * u.x + u.y * u.y;
    const double v_squared = v.x * v.x + v.y * v.y;
    const Point2 center{(v.y * u_squared - u.y * v_squared) / (2 * determinant),
                        (u.x * v_squared - v.x * u_squared) / (2 * determinant)};
    const Point2 endpoint = at_end ? v : Point2{};
    const Point2 radial = subtract(endpoint, center);
    const double radius = magnitude(radial);
    if (!std::isfinite(radius) || radius <= 1e-12)
      return failure(object_id, "Smooth requires a finite positive arc radius");
    const Point2 radial_unit = divide(radial, radius);
    const double direction = (determinant > 0 ? 1.0 : -1.0) * (at_end ? -1.0 : 1.0);
    const double inverse_radius = (1.0 / radius) / length_scale;
    const Point2 curvature{-radial_unit.x * inverse_radius, -radial_unit.y * inverse_radius};
    if (!finite(curvature))
      return failure(object_id, "Smooth arc curvature exceeds numeric range");
    return Result<PlanarEndpointDifferential>::success(
        {{-radial_unit.y * direction, radial_unit.x * direction}, curvature});
  }

private:
  [[nodiscard]] static Point2 subtract(Point2 a, Point2 b) {
    return {a.x - b.x, a.y - b.y};
  }
  [[nodiscard]] static Point2 divide(Point2 a, double divisor) {
    return {a.x / divisor, a.y / divisor};
  }
  [[nodiscard]] static double magnitude(Point2 a) {
    return std::hypot(a.x, a.y);
  }
  [[nodiscard]] static double cross(Point2 a, Point2 b) {
    return a.x * b.y - a.y * b.x;
  }
  [[nodiscard]] static bool finite(Point2 a) {
    return std::isfinite(a.x) && std::isfinite(a.y);
  }
  [[nodiscard]] static bool regular(Point2 derivative, double speed, double length_scale) {
    return finite(derivative) && std::isfinite(speed) && std::isfinite(length_scale) &&
           length_scale > 0 && speed > 1e-12 * length_scale;
  }
  [[nodiscard]] static Result<PlanarEndpointDifferential> failure(const std::string& object_id,
                                                                  std::string message) {
    return Result<PlanarEndpointDifferential>::failure(
        Error::validation(object_id, std::move(message)));
  }
};

} // namespace blcad
