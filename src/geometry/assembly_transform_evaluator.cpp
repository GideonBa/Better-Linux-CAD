#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <cmath>
#include <numbers>

namespace blcad::geometry {
namespace {

[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
  return degrees * (std::numbers::pi_v<double> / 180.0);
}

[[nodiscard]] Vector3 rotate_x(const Vector3& vector, double angle_rad) noexcept {
  const double cosine = std::cos(angle_rad);
  const double sine = std::sin(angle_rad);
  return Vector3{vector.x, cosine * vector.y - sine * vector.z,
                 sine * vector.y + cosine * vector.z};
}

[[nodiscard]] Vector3 rotate_y(const Vector3& vector, double angle_rad) noexcept {
  const double cosine = std::cos(angle_rad);
  const double sine = std::sin(angle_rad);
  return Vector3{cosine * vector.x + sine * vector.z, vector.y,
                 -sine * vector.x + cosine * vector.z};
}

[[nodiscard]] Vector3 rotate_z(const Vector3& vector, double angle_rad) noexcept {
  const double cosine = std::cos(angle_rad);
  const double sine = std::sin(angle_rad);
  return Vector3{cosine * vector.x - sine * vector.y,
                 sine * vector.x + cosine * vector.y, vector.z};
}

[[nodiscard]] Vector3 rotate_xyz(const Vector3& vector,
                                 const Vector3& rotation_deg) noexcept {
  Vector3 rotated = rotate_x(vector, degrees_to_radians(rotation_deg.x));
  rotated = rotate_y(rotated, degrees_to_radians(rotation_deg.y));
  return rotate_z(rotated, degrees_to_radians(rotation_deg.z));
}

} // namespace

Point3 AssemblyTransformEvaluator::evaluate_point(const RigidTransform& transform,
                                                  const Point3& component_local_point) const
    noexcept {
  const Vector3 rotated = rotate_xyz(
      Vector3{component_local_point.x, component_local_point.y, component_local_point.z},
      transform.rotation_deg);
  return Point3{rotated.x + transform.translation_mm.x,
                rotated.y + transform.translation_mm.y,
                rotated.z + transform.translation_mm.z};
}

Vector3 AssemblyTransformEvaluator::evaluate_vector(
    const RigidTransform& transform, const Vector3& component_local_vector) const noexcept {
  return rotate_xyz(component_local_vector, transform.rotation_deg);
}

AssemblySpacePlanarDescriptor AssemblyTransformEvaluator::evaluate_plane(
    const RigidTransform& transform,
    const ComponentLocalPlanarDescriptor& component_local_plane) const noexcept {
  return AssemblySpacePlanarDescriptor{
      evaluate_point(transform, component_local_plane.origin),
      evaluate_vector(transform, component_local_plane.x_axis),
      evaluate_vector(transform, component_local_plane.y_axis),
      evaluate_vector(transform, component_local_plane.normal)};
}

} // namespace blcad::geometry
