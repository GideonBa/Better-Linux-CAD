#pragma once
#include "blcad/core/path_curve.hpp"
#include "blcad/geometry/sweep_adapter.hpp"
#include <TopoDS_Wire.hxx>
namespace blcad::geometry::detail {
// Prove the supported circular normal-bundle capability before asking OCCT for a closed pipe.
struct CircularSweepWires {
  TopoDS_Wire profile;
  TopoDS_Wire spine;
};
[[nodiscard]] Result<CircularSweepWires> validate_circular_closed_sweep(
    const FeatureId&, const TopoDS_Wire& profile, const TopoDS_Wire& spine, PathOrientationRule,
    std::optional<Vector3>, double twist_degrees, bool user_guide, double modeling_tolerance);
[[nodiscard]] Result<GeometryShape>
sweep_circular_closed_twist(const FeatureId&, const std::vector<ClosedProfileCurveSegment>&,
                            const TopoDS_Wire&, PathOrientationRule, std::optional<Vector3>,
                            double twist_degrees, double modeling_tolerance);
} // namespace blcad::geometry::detail
