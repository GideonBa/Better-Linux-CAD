#pragma once

#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::test {

struct IntegratedSolidVolume {
  double volume_mm3;
  double relative_error;
};

// Test-only adaptive integration; GUI tests keep OCCT headers out of their target.
[[nodiscard]] IntegratedSolidVolume integrated_solid_volume(const geometry::GeometryShape& shape);

enum class SolidPointLocation { Inside, Outside, Boundary, Unknown };
[[nodiscard]] SolidPointLocation classify_solid_point(const geometry::GeometryShape& shape,
                                                      Point3 point, double tolerance_mm = 1e-7);

// Distance to actual trimmed faces, excluding the interior of a Solid.
[[nodiscard]] double distance_to_faces(const geometry::GeometryShape&, Point3);

} // namespace blcad::test
