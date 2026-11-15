#pragma once
#include "blcad/core/loft_feature.hpp"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <cstddef>
namespace blcad::geometry::detail {
struct SurfaceJoinReport {
  double position_error_mm = 0;
  double normal_error_rad = 0;
  double curvature_error_per_mm = 0;
  std::size_t stations = 0;
};
// Transient numerical evidence on actual located trimmed faces. No topology
// identity is authored. Callers validate native topology before accepting a candidate.
// The report is a bounded sampling/refinement witness, not a global continuum error bound.
[[nodiscard]] Result<SurfaceJoinReport>
verify_surface_join(FeatureId id, const TopoDS_Edge& boundary, const TopoDS_Face& candidate,
                    const TopoDS_Face& support, LoftBoundaryContinuity continuity,
                    double position_tolerance_mm, double normal_tolerance_rad,
                    double curvature_tolerance_per_mm, bool reverse_support = false);
} // namespace blcad::geometry::detail
