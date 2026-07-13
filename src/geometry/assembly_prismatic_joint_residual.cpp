#include "assembly_prismatic_joint_residual.hpp"

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

PrismaticJointResidualDescriptor
build_prismatic_joint_residual(const AssemblyFrameTargetDescriptor& frame_a,
                               const AssemblyFrameTargetDescriptor& frame_b,
                               double requested_translation_mm) noexcept {
  const Vector3 delta = difference(frame_b.origin, frame_a.origin);
  const double axial_translation = dot(delta, frame_a.z_axis);
  return {difference(frame_a.z_axis, frame_b.z_axis),
          difference(delta, scaled(frame_a.z_axis, axial_translation)),
          dot(frame_a.z_axis, cross(frame_a.x_axis, frame_b.x_axis)),
          dot(frame_a.x_axis, frame_b.x_axis) - 1.0, axial_translation - requested_translation_mm};
}

} // namespace blcad::geometry::detail
