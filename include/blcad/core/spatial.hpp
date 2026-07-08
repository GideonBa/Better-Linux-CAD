#pragma once

namespace blcad {

struct Point2 {
  double x = 0.0;
  double y = 0.0;

  friend bool operator==(const Point2&, const Point2&) = default;
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

} // namespace blcad
