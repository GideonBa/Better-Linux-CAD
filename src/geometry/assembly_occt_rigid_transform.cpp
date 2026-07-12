#include "assembly_occt_rigid_transform.hpp"

#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <numbers>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
  return degrees * (std::numbers::pi_v<double> / 180.0);
}

} // namespace

gp_Trsf to_occt_rigid_transform(const RigidTransform& transform) {
  const gp_Ax1 x_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
  const gp_Ax1 y_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
  const gp_Ax1 z_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));

  gp_Trsf rotation_x;
  rotation_x.SetRotation(x_axis, degrees_to_radians(transform.rotation_deg.x));
  gp_Trsf rotation_y;
  rotation_y.SetRotation(y_axis, degrees_to_radians(transform.rotation_deg.y));
  gp_Trsf rotation_z;
  rotation_z.SetRotation(z_axis, degrees_to_radians(transform.rotation_deg.z));
  gp_Trsf translation;
  translation.SetTranslation(
      gp_Vec(transform.translation_mm.x, transform.translation_mm.y, transform.translation_mm.z));

  // gp_Trsf multiplication applies the right-hand factor first.
  return translation * rotation_z * rotation_y * rotation_x;
}

TopLoc_Location to_occt_location(const RigidTransform& transform) {
  return TopLoc_Location(to_occt_rigid_transform(transform));
}

} // namespace blcad::geometry::detail
