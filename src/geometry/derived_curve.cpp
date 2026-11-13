#include "blcad/geometry/derived_curve.hpp"
#include "derived_curve_internal.hpp"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BndLib_Add3dCurve.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Standard_Failure.hxx>
#include <cmath>
#include <gp_Trsf.hxx>

namespace blcad::geometry {
DerivedCurveRef reversed_derived_curve(const DerivedCurveRef& source) {
  if (!source)
    return {};
  return std::make_shared<const DerivedCurve>(source->curve->Reversed(), source->closure_proven);
}
DerivedCurveRef transformed_derived_curve(const DerivedCurveRef& source, Point3 origin, Vector3 x,
                                          Vector3 y, Vector3 z) {
  if (!source)
    return {};
  gp_Trsf transform;
  transform.SetValues(x.x, y.x, z.x, origin.x, x.y, y.y, z.y, origin.y, x.z, y.z, z.z, origin.z);
  return std::make_shared<const DerivedCurve>(
      Handle(Geom_Curve)::DownCast(source->curve->Transformed(transform)), source->closure_proven);
}
DerivedCurveRef translated_derived_curve(const DerivedCurveRef& source, Vector3 delta) {
  return transformed_derived_curve(source, {delta.x, delta.y, delta.z}, {1, 0, 0}, {0, 1, 0},
                                   {0, 0, 1});
}
Result<TopoDS_Edge> derived_curve_edge(const DerivedCurveRef& source,
                                       const std::string& object_id) {
  const auto fail = [&](std::string message) {
    return Result<TopoDS_Edge>::failure(Error::geometry(object_id, std::move(message)));
  };
  try {
    if (!source || source->curve.IsNull() || !std::isfinite(source->curve->FirstParameter()) ||
        !std::isfinite(source->curve->LastParameter()) ||
        source->curve->FirstParameter() >= source->curve->LastParameter())
      return fail("Derived equation curve requires a finite resolved domain");
    if (source->curve->IsClosed() != source->closure_proven)
      return fail("Equation curve closure differs from its proven modeling intent");
    BRepBuilderAPI_MakeEdge edge(source->curve);
    if (!edge.IsDone())
      return fail("Could not construct the derived equation edge");
    return Result<TopoDS_Edge>::success(edge.Edge());
  } catch (const Standard_Failure& failure) {
    return fail(failure.GetMessageString() ? failure.GetMessageString()
                                           : "OCCT equation edge failed");
  }
}

Result<std::pair<Point3, Point3>> derived_curve_bounds(const DerivedCurveRef& source,
                                                       const std::string& id) {
  if (!source || source->curve.IsNull())
    return Result<std::pair<Point3, Point3>>::failure(Error::geometry(id, "Missing derived curve"));
  try {
    Bnd_Box box;
    GeomAdaptor_Curve curve(source->curve);
    // Analytic bounds or the exact spline control hull conservatively contain
    // the complete modeled curve, unlike a display/pick polygon.
    BndLib_Add3dCurve::Add(curve, 0, box);
    box.SetGap(0);
    double a, b, c, d, e, f;
    box.Get(a, b, c, d, e, f);
    return Result<std::pair<Point3, Point3>>::success({{a, b, c}, {d, e, f}});
  } catch (const Standard_Failure&) {
    return Result<std::pair<Point3, Point3>>::failure(
        Error::geometry(id, "Derived curve bounds could not be resolved"));
  }
}
Result<double> derived_curve_length(const DerivedCurveRef& source, const std::string& id) {
  if (!source || source->curve.IsNull())
    return Result<double>::failure(Error::geometry(id, "Missing derived curve"));
  try {
    GeomAdaptor_Curve curve(source->curve);
    const double value =
        GCPnts_AbscissaPoint::Length(curve, curve.FirstParameter(), curve.LastParameter(), 1e-8);
    if (!std::isfinite(value) || value <= 0)
      return Result<double>::failure(Error::geometry(id, "Derived curve length is invalid"));
    return Result<double>::success(value);
  } catch (const Standard_Failure&) {
    return Result<double>::failure(
        Error::geometry(id, "Derived curve length could not be resolved"));
  }
}
Result<Point3> derived_curve_point_at_length_fraction(const DerivedCurveRef& source,
                                                      double fraction, const std::string& id) {
  auto extent = derived_curve_length(source, id);
  if (!extent)
    return Result<Point3>::failure(extent.error());
  if (!std::isfinite(fraction) || fraction < 0 || fraction > 1)
    return Result<Point3>::failure(
        Error::geometry(id, "Curve length fraction must be between zero and one"));
  try {
    GeomAdaptor_Curve curve(source->curve);
    double parameter = fraction == 0 ? curve.FirstParameter() : curve.LastParameter();
    if (fraction > 0 && fraction < 1) {
      GCPnts_AbscissaPoint at(curve, fraction * extent.value(), curve.FirstParameter());
      if (!at.IsDone())
        return Result<Point3>::failure(
            Error::geometry(id, "Curve length station did not converge"));
      parameter = at.Parameter();
    }
    const auto p = source->curve->Value(parameter);
    return Result<Point3>::success({p.X(), p.Y(), p.Z()});
  } catch (const Standard_Failure&) {
    return Result<Point3>::failure(
        Error::geometry(id, "Curve length station could not be resolved"));
  }
}
Result<Vector3> derived_curve_start_tangent(const DerivedCurveRef& source, const std::string& id) {
  if (!source || source->curve.IsNull())
    return Result<Vector3>::failure(Error::geometry(id, "Missing derived curve"));
  try {
    gp_Pnt p;
    gp_Vec tangent;
    source->curve->D1(source->curve->FirstParameter(), p, tangent);
    const double value = std::hypot(tangent.X(), tangent.Y(), tangent.Z());
    if (!std::isfinite(value) || value == 0)
      return Result<Vector3>::failure(
          Error::geometry(id, "Derived curve start tangent is singular"));
    return Result<Vector3>::success(
        {tangent.X() / value, tangent.Y() / value, tangent.Z() / value});
  } catch (const Standard_Failure&) {
    return Result<Vector3>::failure(
        Error::geometry(id, "Derived curve tangent could not be resolved"));
  }
}
} // namespace blcad::geometry
