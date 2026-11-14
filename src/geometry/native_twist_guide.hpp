#pragma once
#include "blcad/geometry/sweep_adapter.hpp"

namespace blcad::geometry::detail {
// Tolerance-checked derived normal-plane guide. The modeled spine remains the original native
// curve.
[[nodiscard]] Result<std::vector<SweepPathSegment>>
native_twist_guide(const FeatureId&, const std::vector<DerivedCurveRef>& bounded_spans,
                   PathOrientationRule, std::optional<Vector3> up, double twist_degrees,
                   double modeling_tolerance_mm);
} // namespace blcad::geometry::detail
