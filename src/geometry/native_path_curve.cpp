#include "native_path_curve.hpp"
#include "derived_curve_internal.hpp"
#include <Adaptor3d_CurveOnSurface.hxx>
#include <Approx_Curve3d.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2d_Line.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <cmath>
#include <numbers>

namespace blcad::geometry::detail {
namespace {
gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
Result<DerivedCurveRef> checked_curve(Handle(Geom_Curve) curve, const std::string& id) {
  auto product = std::make_shared<const DerivedCurve>(curve, curve->IsClosed());
  auto edge = derived_curve_edge(product, id);
  if (!edge)
    return Result<DerivedCurveRef>::failure(edge.error());
  auto tangent = derived_curve_start_tangent(product, id);
  if (!tangent)
    return Result<DerivedCurveRef>::failure(tangent.error());
  auto extent = derived_curve_length(product, id);
  if (!extent)
    return Result<DerivedCurveRef>::failure(extent.error());
  if (extent.value() <= 1e-8)
    return Result<DerivedCurveRef>::failure(Error::geometry(id, "Native path curve is degenerate"));
  return Result<DerivedCurveRef>::success(std::move(product));
}
Result<DerivedCurveRef> failed(const std::string& id, const Standard_Failure& failure) {
  return Result<DerivedCurveRef>::failure(
      Error::geometry(id, failure.GetMessageString() ? failure.GetMessageString()
                                                     : "Native curve construction failed"));
}
} // namespace
Result<DerivedCurveRef> native_spatial_spline(const std::vector<Point3>& points,
                                              const SketchSpline3D& spline) {
  try {
    TColgp_Array1OfPnt poles(1, static_cast<int>(points.size()));
    for (int i = 1; i <= poles.Length(); ++i)
      poles.SetValue(i, point(points[i - 1]));
    Handle(Geom_BSplineCurve) curve;
    const auto degree = static_cast<int>(spline.degree());
    if (spline.representation() == SketchSpline3DRepresentation::ControlPoints) {
      const int count = poles.Length() - degree + 1;
      TColStd_Array1OfReal knots(1, count);
      TColStd_Array1OfInteger multiplicities(1, count);
      for (int i = 1; i <= count; ++i) {
        knots.SetValue(i, i - 1);
        multiplicities.SetValue(i, i == 1 || i == count ? degree + 1 : 1);
      }
      curve = new Geom_BSplineCurve(poles, knots, multiplicities, degree, false);
    } else {
      const auto continuity =
          spline.continuity() == SketchSpline3DContinuity::Curvature ? GeomAbs_C2
          : spline.continuity() == SketchSpline3DContinuity::Tangent ? GeomAbs_C1
                                                                     : GeomAbs_C0;
      // Preserve the existing Sketch3D fit definition, independently of path tolerance.
      GeomAPI_PointsToBSpline builder(poles, degree, degree, continuity, 1e-7);
      if (!builder.IsDone())
        return Result<DerivedCurveRef>::failure(
            Error::geometry(spline.id().value(), "Could not build fit-point 3D spline"));
      curve = builder.Curve();
    }
    return Result<DerivedCurveRef>::success(
        std::make_shared<const DerivedCurve>(curve, curve->IsClosed()));
  } catch (const Standard_Failure& failure) {
    return failed(spline.id().value(), failure);
  }
}
Result<DerivedCurveRef> native_rational_bezier(const std::vector<Point3>& points,
                                               const std::array<double, 4>& weights,
                                               const std::string& id) {
  if (points.size() != 4)
    return Result<DerivedCurveRef>::failure(
        Error::geometry(id, "Rational cubic requires four controls"));
  try {
    TColgp_Array1OfPnt poles(1, 4);
    TColStd_Array1OfReal native_weights(1, 4);
    for (int i = 1; i <= 4; ++i) {
      poles.SetValue(i, point(points[i - 1]));
      native_weights.SetValue(i, weights[i - 1]);
    }
    return checked_curve(new Geom_BezierCurve(poles, native_weights), id);
  } catch (const Standard_Failure& failure) {
    return failed(id, failure);
  }
}
Result<DerivedCurveRef> native_path_helix(Point3 origin, Vector3 axis, Vector3 radial,
                                          double radius, double pitch, std::size_t turns,
                                          bool right, double tolerance, const std::string& id) {
  if (turns == 0 || turns > 128)
    return Result<DerivedCurveRef>::failure(
        Error::geometry(id, "Curve-preserving Helix supports 1 to 128 turns"));
  try {
    const double q = pitch / (2 * std::numbers::pi), norm = std::hypot(1., q);
    const double end = norm * 2 * std::numbers::pi * turns;
    Handle(Geom2d_Line) line =
        new Geom2d_Line(gp_Pnt2d(0, 0), gp_Dir2d((right ? 1. : -1.) / norm, q / norm));
    Handle(Geom_CylindricalSurface) surface = new Geom_CylindricalSurface(
        gp_Ax3(point(origin), gp_Dir(axis.x, axis.y, axis.z), gp_Dir(radial.x, radial.y, radial.z)),
        radius);
    Handle(Adaptor3d_Curve) analytic = new Adaptor3d_CurveOnSurface(
        new Geom2dAdaptor_Curve(line, 0, end), new GeomAdaptor_Surface(surface));
    Approx_Curve3d approximation(analytic, tolerance, GeomAbs_C2, 4096, 14);
    if (!approximation.IsDone() || !std::isfinite(approximation.MaxError()) ||
        approximation.MaxError() > tolerance || approximation.Curve().IsNull())
      return Result<DerivedCurveRef>::failure(Error::geometry(
          id, "Helix approximation did not meet modeling tolerance within its budget"));
    return checked_curve(approximation.Curve(), id);
  } catch (const Standard_Failure& failure) {
    return failed(id, failure);
  }
}
} // namespace blcad::geometry::detail
