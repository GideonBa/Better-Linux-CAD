#include "blcad/geometry/surface_trim_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include "surface_extension_selection.hpp"
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <cmath>
namespace blcad::geometry {
Result<SurfaceExtensionFrame>
SurfaceTrimAdapter::extension_frame(FeatureId id, const GeometryShape& target,
                                    const std::vector<SweepPathSegment>& boundary,
                                    double tolerance) const {
  const auto fail = [&](std::string message) {
    return Result<SurfaceExtensionFrame>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (target.empty() || boundary.empty() || !std::isfinite(tolerance) || tolerance < 1e-9 ||
      tolerance > 1)
    return fail("Extension frame requires finite analytic geometry and physical tolerance");
  try {
    const bool closed = blcad::distance(boundary.front().start, boundary.back().end) <= tolerance;
    const auto shape =
        SweepAdapter{}.path_shape(PathCurveId("surface.extension.frame"), boundary, closed);
    if (!shape)
      return Result<SurfaceExtensionFrame>::failure(shape.error());
    const auto query = detail::resolve_extension_boundary(id, target.impl_->shape,
                                                          shape.value().impl_->shape, tolerance);
    if (!query)
      return Result<SurfaceExtensionFrame>::failure(query.error());
    const auto& data = query.value();
    const auto surface = BRep_Tool::Surface(data.face);
    const double u = data.side == 0 ? data.u0 : data.side == 1 ? data.u1 : (data.u0 + data.u1) / 2;
    const double v = data.side == 2 ? data.v0 : data.side == 3 ? data.v1 : (data.v0 + data.v1) / 2;
    gp_Pnt point;
    gp_Vec du, dv;
    surface->D1(u, v, point, du, dv);
    auto outward = data.side < 2 ? du : dv;
    if (outward.SquareMagnitude() < 1e-24)
      return fail("Extension frame has a singular tangent");
    outward.Normalize();
    if (data.side == 0 || data.side == 2)
      outward.Reverse();
    return Result<SurfaceExtensionFrame>::success(
        {{point.X(), point.Y(), point.Z()}, {outward.X(), outward.Y(), outward.Z()}});
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString());
  }
}
} // namespace blcad::geometry
