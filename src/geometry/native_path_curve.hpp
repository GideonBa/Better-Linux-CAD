#pragma once
#include "blcad/core/sketch_3d.hpp"
#include "blcad/geometry/derived_curve.hpp"
#include <array>
#include <vector>

namespace blcad::geometry::detail {
// The same native spline definition is used by Sketch3D and opt-in paths.
[[nodiscard]] Result<DerivedCurveRef> native_spatial_spline(const std::vector<Point3>&,
                                                            const SketchSpline3D&);
[[nodiscard]] Result<DerivedCurveRef> native_rational_bezier(const std::vector<Point3>&,
                                                             const std::array<double, 4>&,
                                                             const std::string& id);
[[nodiscard]] Result<DerivedCurveRef> native_path_helix(Point3 origin, Vector3 axis, Vector3 radial,
                                                        double radius, double pitch,
                                                        std::size_t turns, bool right,
                                                        double tolerance_mm, const std::string& id);
} // namespace blcad::geometry::detail
