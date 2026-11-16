#pragma once
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include <cstddef>
#include <vector>
namespace blcad {
class PartDocument;
}
namespace blcad::test::curve_surface_milestone {
// Native trimmed-face and differential witnesses, independent of feature feedback.
// This is bounded boundary evidence, not a claim of global longitudinal G2.
struct PhysicalReport {
  std::size_t solids = 0, stations = 0;
  double volume_mm3 = 0, boundary_error_mm = 0, normal_error_rad = 0, curvature_error_per_mm = 0,
         blend_error_mm = 0;
  bool valid = true, material = true;
};
PhysicalReport physical(const geometry::GeometryShape&, bool pattern = true, bool blend = true,
                        double x_shift = 0);
// Integrate only the isolated duct terminal region to avoid subtracting large Loft masses.
double rim_removed_volume(const geometry::GeometryShape&, const geometry::GeometryShape&);
PhysicalReport reimported(const std::vector<geometry::GeometryShape>&);
PartDocument connected_fixture();
void edit_connected_section_and_guide(PartDocument&, double shift, bool repair_anchor = true);
double section_error(const geometry::GeometryShape&, double middle_shift = 0);
} // namespace blcad::test::curve_surface_milestone
