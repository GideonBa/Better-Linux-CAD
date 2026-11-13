#pragma once
#include "blcad/core/result.hpp"
#include "blcad/geometry/derived_curve.hpp"
#include <Geom_Curve.hxx>
#include <TopoDS_Edge.hxx>
#include <utility>

namespace blcad::geometry {
class DerivedCurve {
public:
  DerivedCurve(Handle(Geom_Curve) value, bool closed)
      : curve(std::move(value)), closure_proven(closed) {}
  Handle(Geom_Curve) curve;
  bool closure_proven{};
};
[[nodiscard]] Result<TopoDS_Edge> derived_curve_edge(const DerivedCurveRef& source,
                                                     const std::string& object_id);
[[nodiscard]] Result<std::pair<Point3, Point3>> derived_curve_bounds(const DerivedCurveRef& source,
                                                                     const std::string& object_id);
[[nodiscard]] Result<double> derived_curve_length(const DerivedCurveRef& source,
                                                  const std::string& object_id);
[[nodiscard]] Result<Point3> derived_curve_point_at_length_fraction(const DerivedCurveRef& source,
                                                                    double fraction,
                                                                    const std::string& object_id);
[[nodiscard]] Result<Vector3> derived_curve_start_tangent(const DerivedCurveRef& source,
                                                          const std::string& object_id);

} // namespace blcad::geometry
