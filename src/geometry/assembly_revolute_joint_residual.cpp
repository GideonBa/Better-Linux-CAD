#include "assembly_revolute_joint_residual.hpp"

#include <cmath>
#include <numbers>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] Vector3 difference(const Vector3& target, const Vector3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y,
                 lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
  return degrees * (std::numbers::pi_v<double> / 180.0);
}

} // namespace

RevoluteJointResidualDescriptor build_revolute_joint_residual(
    const AssemblySpaceAxisDescriptor& axis_a, const AssemblySpacePlanarDescriptor& seat_a,
    const AssemblySpaceAxisDescriptor& axis_b, const AssemblySpacePlanarDescriptor& seat_b,
    double requested_coordinate_deg) noexcept {
  const Vector3 axis_origin_delta = difference(axis_b.origin, axis_a.origin);
  const Vector3 seat_origin_delta = difference(seat_b.origin, seat_a.origin);

  const double reference_cosine = dot(seat_a.x_axis, seat_b.x_axis);
  const double reference_sine = dot(axis_a.direction, cross(seat_a.x_axis, seat_b.x_axis));
  const double target_rad = degrees_to_radians(requested_coordinate_deg);
  const double target_cosine = std::cos(target_rad);
  const double target_sine = std::sin(target_rad);

  return RevoluteJointResidualDescriptor{
      difference(axis_a.direction, axis_b.direction),
      cross(axis_origin_delta, axis_a.direction),
      dot(seat_origin_delta, seat_a.normal),
      reference_sine * target_cosine - reference_cosine * target_sine,
      reference_cosine * target_cosine + reference_sine * target_sine - 1.0};
}

} // namespace blcad::geometry::detail
