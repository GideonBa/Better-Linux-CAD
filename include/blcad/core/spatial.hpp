#pragma once

#include <cmath>

namespace blcad {

struct Point2 {
  double x = 0.0;
  double y = 0.0;

  friend bool operator==(const Point2&, const Point2&) = default;
};

struct Vector2 {
  double x = 0.0;
  double y = 0.0;

  friend bool operator==(const Vector2&, const Vector2&) = default;
};

struct Point3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  friend bool operator==(const Point3&, const Point3&) = default;
};

struct Vector3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  friend bool operator==(const Vector3&, const Vector3&) = default;
};

// Shared Euclidean helpers. These are pure arithmetic over the POD types above
// and carry no model authority, so every layer may use them. A caller that needs
// a tolerance policy for degenerate input keeps that policy local: `normalized`
// has the precondition `length(value) > 0` and does not invent one.

[[nodiscard]] constexpr double dot(Vector2 left, Vector2 right) noexcept {
  return left.x * right.x + left.y * right.y;
}

[[nodiscard]] inline double length(Vector2 value) noexcept {
  return std::sqrt(dot(value, value));
}

[[nodiscard]] constexpr double dot(Vector3 left, Vector3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr Vector3 cross(Vector3 left, Vector3 right) noexcept {
  return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                 left.x * right.y - left.y * right.x};
}

[[nodiscard]] constexpr Vector3 displacement(Point3 from, Point3 to) noexcept {
  return {to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] inline double length(Vector3 value) noexcept {
  return std::sqrt(dot(value, value));
}

[[nodiscard]] inline Vector3 normalized(Vector3 value) noexcept {
  const double magnitude = length(value);
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

// Signed position of a point along an axis direction. Distinct from `dot`: the
// first argument is a position, not a direction, so the result is a coordinate
// along `axis` rather than a projection between two vectors.
[[nodiscard]] constexpr double project_onto_axis(Point3 point, Vector3 axis) noexcept {
  return point.x * axis.x + point.y * axis.y + point.z * axis.z;
}

[[nodiscard]] inline double distance(Point3 left, Point3 right) noexcept {
  return std::hypot(std::hypot(left.x - right.x, left.y - right.y), left.z - right.z);
}

} // namespace blcad
