#pragma once

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

namespace blcad::test {

struct IntegratedSolidVolume {
  double volume_mm3;
  double relative_error;
};

// Test-only adaptive integration; GUI tests keep OCCT headers out of their target.
[[nodiscard]] IntegratedSolidVolume integrated_solid_volume(const geometry::GeometryShape& shape);

} // namespace blcad::test
