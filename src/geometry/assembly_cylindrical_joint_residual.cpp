#include "assembly_cylindrical_joint_residual.hpp"

#include <cmath>
#include <numbers>

namespace blcad::geometry::detail {
namespace {
Vector3 difference(const Point3& a, const Point3& b) noexcept {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vector3 difference(const Vector3& a, const Vector3& b) noexcept {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
double dot(const Vector3& a, const Vector3& b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vector3 cross(const Vector3& a, const Vector3& b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vector3 scaled(const Vector3& value, double factor) noexcept {
  return {value.x * factor, value.y * factor, value.z * factor};
}
} // namespace

CylindricalJointResidualDescriptor build_cylindrical_joint_residual(
    const AssemblyFrameTargetDescriptor& frame_a, const AssemblyFrameTargetDescriptor& frame_b,
    double requested_translation_mm, double requested_rotation_deg) noexcept {
  const Vector3 delta = difference(frame_b.origin, frame_a.origin);
  const double translation = dot(delta, frame_a.z_axis);
  const double reference_cosine = dot(frame_a.x_axis, frame_b.x_axis);
  const double reference_sine = dot(frame_a.z_axis, cross(frame_a.x_axis, frame_b.x_axis));
  const double target = requested_rotation_deg * (std::numbers::pi_v<double> / 180.0);
  const double target_cosine = std::cos(target);
  const double target_sine = std::sin(target);
  return {difference(frame_a.z_axis, frame_b.z_axis),
          difference(delta, scaled(frame_a.z_axis, translation)),
          translation - requested_translation_mm,
          reference_sine * target_cosine - reference_cosine * target_sine,
          reference_cosine * target_cosine + reference_sine * target_sine - 1.0};
}

} // namespace blcad::geometry::detail
