#pragma once
#include "blcad/geometry/equation_curve_geometry.hpp"
#include <vector>

namespace blcad::geometry {
// Disposable selection/display chords. They never replace the modeled curve.
[[nodiscard]] Result<std::vector<Point3>>
sample_equation_curve_for_presentation(const EquationCurveGeometry& source,
                                       double deflection_mm = .01,
                                       std::size_t maximum_points = 32769);
} // namespace blcad::geometry
