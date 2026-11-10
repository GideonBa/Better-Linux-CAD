#pragma once

#include "blcad/core/result.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepLib.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Ax3.hxx>

#include <cmath>
#include <numbers>
#include <string>

namespace blcad::geometry::detail {
// Exact surface pcurve with a tolerance-controlled 3D approximation. Fractional
// turns are retained. Callers own workload/capability limits before allocation;
// Block 166 may reuse this without depending on the general Sweep executor.
inline Result<TopoDS_Edge> cylindrical_helix(const std::string& id, const gp_Ax3& frame,
                                             double radius, double pitch, double first_z,
                                             double last_z, bool right) {
  const double q = pitch / (2 * std::numbers::pi), norm = std::hypot(1., q);
  const double end = (last_z - first_z) * norm / q;
  if (!(radius > 0 && pitch > 0 && last_z > first_z) || !std::isfinite(end))
    return Result<TopoDS_Edge>::failure(Error::geometry(id, "Invalid helical curve interval"));
  Handle(Geom2d_Line) curve =
      new Geom2d_Line(gp_Pnt2d(0, first_z), gp_Dir2d((right ? 1. : -1.) / norm, q / norm));
  Handle(Geom_CylindricalSurface) surface = new Geom_CylindricalSurface(frame, radius);
  BRepBuilderAPI_MakeEdge builder(curve, surface, 0, end);
  if (!builder.IsDone())
    return Result<TopoDS_Edge>::failure(Error::geometry(id, "Cannot build helical pcurve"));
  auto edge = builder.Edge();
  if (!BRepLib::BuildCurves3d(edge, 1e-7))
    return Result<TopoDS_Edge>::failure(Error::geometry(id, "Cannot approximate helical curve"));
  return Result<TopoDS_Edge>::success(std::move(edge));
}
} // namespace blcad::geometry::detail
