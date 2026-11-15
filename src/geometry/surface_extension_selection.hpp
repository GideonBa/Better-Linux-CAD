#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
namespace blcad::geometry::detail {
struct AnalyticSurfaceBoundary {
  TopoDS_Face face;
  TopoDS_Edge edge;
  double u0, u1, v0, v1;
  int side;
};
[[nodiscard]] Result<AnalyticSurfaceBoundary>
resolve_extension_boundary(FeatureId, const TopoDS_Shape& target, const TopoDS_Shape& selection,
                           double tolerance);
} // namespace blcad::geometry::detail
