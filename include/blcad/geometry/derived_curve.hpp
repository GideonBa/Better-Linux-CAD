#pragma once
#include "blcad/core/spatial.hpp"
#include <memory>

namespace blcad::geometry {
// Geometry-owned immutable analytic/spline product, never persistent model intent.
class DerivedCurve;
using DerivedCurveRef = std::shared_ptr<const DerivedCurve>;
[[nodiscard]] DerivedCurveRef reversed_derived_curve(const DerivedCurveRef& source);
[[nodiscard]] DerivedCurveRef transformed_derived_curve(const DerivedCurveRef& source,
                                                        Point3 origin, Vector3 x, Vector3 y,
                                                        Vector3 z);
[[nodiscard]] DerivedCurveRef translated_derived_curve(const DerivedCurveRef& source,
                                                       Vector3 delta);
} // namespace blcad::geometry
