#pragma once
#include <cstddef>
namespace blcad::geometry {
// Bounded native sampling/refinement witnesses; never a continuum error bound.
struct LoftVerificationReport {
  double curve_error_mm = 0, position_error_mm = 0, normal_error_rad = 0,
         curvature_error_per_mm = 0;
  double linear_tolerance_mm = 0, angular_tolerance_rad = 0, curvature_tolerance_per_mm = 0;
  std::size_t g1_joins = 0, g2_joins = 0, stations = 0;
  bool before_body_operation = true;
};
} // namespace blcad::geometry
