#pragma once
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include <vector>
namespace blcad::geometry {
struct SurfaceFreeEdge {
  GeometryShape shape;
  Point3 midpoint;
  double length_mm = 0;
  std::size_t source = 0;
};
struct SurfaceGap {
  std::size_t first_edge = 0;
  std::size_t second_edge = 0;
  double minimum_mm = 0;
  double maximum_mm = 0;
  bool within_tolerance = false;
};
struct SurfaceBoundaryReport {
  std::vector<SurfaceFreeEdge> free_edges;
  std::vector<SurfaceGap> gaps;
  std::size_t non_manifold_edges = 0;
  double tolerance_mm = 0;
};
// Derived native geometry only. Edge positions in this report are transient overlay keys.
class SurfaceBoundaryQuery {
public:
  // Each input is one transient native edge (or a one-edge wire).
  [[nodiscard]] static Result<SurfaceGap> measure_gap(const GeometryShape&, const GeometryShape&,
                                                      double tolerance_mm);
  [[nodiscard]] static Result<SurfaceBoundaryReport>
  inspect(const std::vector<GeometryShape>&, double tolerance_mm, double search_radius_mm);
};
} // namespace blcad::geometry
