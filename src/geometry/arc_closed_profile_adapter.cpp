#include "blcad/geometry/closed_profile_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kArcProfileId = "geometry.arc_closed_profile";
constexpr double kThroughAllMargin = 1.0;
constexpr double kAxisTolerance = 1.0e-9;

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kArcProfileId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') return "OCCT operation failed";
  return message;
}

[[nodiscard]] gp_Pnt to_gp_point(Point3 point) noexcept { return gp_Pnt(point.x, point.y, point.z); }
[[nodiscard]] bool same_point(Point3 left, Point3 right) noexcept { return std::abs(left.x - right.x) <= kAxisTolerance && std::abs(left.y - right.y) <= kAxisTolerance && std::abs(left.z - right.z) <= kAxisTolerance; }
[[nodiscard]] double dot(Point3 point, Vector3 axis) noexcept { return point.x * axis.x + point.y * axis.y + point.z * axis.z; }
[[nodiscard]] double length(Vector3 vector) noexcept { return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z); }
[[nodiscard]] Point3 translate_along_axis(Point3 point, Vector3 axis, double delta) noexcept { return Point3{point.x + axis.x * delta, point.y + axis.y * delta, point.z + axis.z * delta}; }

[[nodiscard]] Result<Vector3> normalize_axis(Vector3 direction) {
  const double axis_length = length(direction);
  if (axis_length <= kAxisTolerance) return Result<Vector3>::failure(make_geometry_error("arc closed profile through-all axis must not be zero length"));
  return Result<Vector3>::success(Vector3{direction.x / axis_length, direction.y / axis_length, direction.z / axis_length});
}

[[nodiscard]] std::vector<Point3> bounding_box_corners(double x_min, double y_min, double z_min, double x_max, double y_max, double z_max) {
  return {Point3{x_min, y_min, z_min}, Point3{x_min, y_min, z_max}, Point3{x_min, y_max, z_min}, Point3{x_min, y_max, z_max}, Point3{x_max, y_min, z_min}, Point3{x_max, y_min, z_max}, Point3{x_max, y_max, z_min}, Point3{x_max, y_max, z_max}};
}

struct ThroughAllPlacement { Vector3 axis; double length = 0.0; double start_projection = 0.0; };
[[nodiscard]] ThroughAllPlacement make_through_all_placement(Vector3 axis, double x_min, double y_min, double z_min, double x_max, double y_max, double z_max) {
  double min_projection = 0.0; double max_projection = 0.0; bool first = true;
  for (const Point3 corner : bounding_box_corners(x_min, y_min, z_min, x_max, y_max, z_max)) { const double projection = dot(corner, axis); if (first) { min_projection = projection; max_projection = projection; first = false; } else { min_projection = std::min(min_projection, projection); max_projection = std::max(max_projection, projection); } }
  return ThroughAllPlacement{axis, (max_projection - min_projection) + 2.0 * kThroughAllMargin, min_projection - kThroughAllMargin};
}

[[nodiscard]] ClosedProfileCurveSegment translate_curve_along_axis(ClosedProfileCurveSegment curve, Vector3 axis, double delta) noexcept { curve.start = translate_along_axis(curve.start, axis, delta); curve.mid = translate_along_axis(curve.mid, axis, delta); curve.end = translate_along_axis(curve.end, axis, delta); curve.control2 = translate_along_axis(curve.control2, axis, delta); return curve; }

[[nodiscard]] Result<TopoDS_Edge> make_curve_edge(const ClosedProfileCurveSegment& curve) {
  if (curve.kind == ClosedProfileCurveKind::Line) {
    if (same_point(curve.start, curve.end)) return Result<TopoDS_Edge>::failure(make_geometry_error("line curve edge must not be degenerate"));
    BRepBuilderAPI_MakeEdge edge_builder(to_gp_point(curve.start), to_gp_point(curve.end));
    if (!edge_builder.IsDone()) return Result<TopoDS_Edge>::failure(make_geometry_error("could not build line curve edge"));
    return Result<TopoDS_Edge>::success(edge_builder.Edge());
  }

  if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
    if (same_point(curve.start, curve.end)) return Result<TopoDS_Edge>::failure(make_geometry_error("spline curve edge endpoints must be distinct"));
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, to_gp_point(curve.start));
    poles.SetValue(2, to_gp_point(curve.mid));
    poles.SetValue(3, to_gp_point(curve.control2));
    poles.SetValue(4, to_gp_point(curve.end));
    Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
    BRepBuilderAPI_MakeEdge edge_builder(bezier);
    if (!edge_builder.IsDone()) return Result<TopoDS_Edge>::failure(make_geometry_error("could not build spline curve edge"));
    return Result<TopoDS_Edge>::success(edge_builder.Edge());
  }

  if (same_point(curve.start, curve.mid) || same_point(curve.start, curve.end) || same_point(curve.mid, curve.end)) return Result<TopoDS_Edge>::failure(make_geometry_error("arc curve edge points must be distinct"));
  GC_MakeArcOfCircle arc_builder(to_gp_point(curve.start), to_gp_point(curve.mid), to_gp_point(curve.end));
  if (!arc_builder.IsDone()) return Result<TopoDS_Edge>::failure(make_geometry_error("could not build circular arc curve"));
  BRepBuilderAPI_MakeEdge edge_builder(arc_builder.Value());
  if (!edge_builder.IsDone()) return Result<TopoDS_Edge>::failure(make_geometry_error("could not build circular arc edge"));
  return Result<TopoDS_Edge>::success(edge_builder.Edge());
}

[[nodiscard]] Result<TopoDS_Wire> make_curve_wire(const std::vector<ClosedProfileCurveSegment>& curves) {
  if (curves.size() < 3U) return Result<TopoDS_Wire>::failure(make_geometry_error("arc closed profile requires at least three curve segments"));
  BRepBuilderAPI_MakeWire wire_builder;
  for (const auto& curve : curves) { auto edge = make_curve_edge(curve); if (edge.has_error()) return Result<TopoDS_Wire>::failure(edge.error()); wire_builder.Add(edge.value()); if (!wire_builder.IsDone()) return Result<TopoDS_Wire>::failure(make_geometry_error("could not add curve edge to wire")); }
  return Result<TopoDS_Wire>::success(wire_builder.Wire());
}

[[nodiscard]] Result<TopoDS_Shape> make_curve_face(const std::vector<ClosedProfileCurveSegment>& curves) {
  auto wire = make_curve_wire(curves);
  if (wire.has_error()) return Result<TopoDS_Shape>::failure(wire.error());
  BRepBuilderAPI_MakeFace face_builder(wire.value());
  if (!face_builder.IsDone()) return Result<TopoDS_Shape>::failure(make_geometry_error("could not build arc closed profile face"));
  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<std::vector<ClosedProfileCurveSegment>> move_curves_to_through_all_start(const std::vector<ClosedProfileCurveSegment>& curves, const ThroughAllPlacement& placement) {
  if (curves.empty()) return Result<std::vector<ClosedProfileCurveSegment>>::failure(make_geometry_error("arc closed profile requires curves"));
  const double profile_projection = dot(curves.front().start, placement.axis);
  const double delta = placement.start_projection - profile_projection;
  std::vector<ClosedProfileCurveSegment> moved; moved.reserve(curves.size());
  for (const auto& curve : curves) moved.push_back(translate_curve_along_axis(curve, placement.axis, delta));
  return Result<std::vector<ClosedProfileCurveSegment>>::success(std::move(moved));
}

[[nodiscard]] Result<TopoDS_Shape> make_prism_shape_from_face(TopoDS_Shape face, Vector3 direction, const Quantity& depth) {
  BRepPrimAPI_MakePrism prism_builder(face, gp_Vec(direction.x * depth.millimeters(), direction.y * depth.millimeters(), direction.z * depth.millimeters()));
  prism_builder.Build();
  if (!prism_builder.IsDone()) return Result<TopoDS_Shape>::failure(make_geometry_error("could not extrude arc closed profile"));
  TopoDS_Shape shape = prism_builder.Shape();
  if (shape.IsNull()) return Result<TopoDS_Shape>::failure(make_geometry_error("arc closed profile extrusion produced an empty shape"));
  return Result<TopoDS_Shape>::success(std::move(shape));
}

[[nodiscard]] Result<ThroughAllPlacement> make_placement_from_target(const GeometryShape& target, Vector3 axis_direction) {
  auto axis = normalize_axis(axis_direction); if (axis.has_error()) return Result<ThroughAllPlacement>::failure(axis.error());
  const TopoDS_Shape& target_shape = target.impl_->shape; Bnd_Box bounds; BRepBndLib::Add(target_shape, bounds); if (bounds.IsVoid()) return Result<ThroughAllPlacement>::failure(make_geometry_error("target shape has no bounding volume"));
  double x_min = 0.0, y_min = 0.0, z_min = 0.0, x_max = 0.0, y_max = 0.0, z_max = 0.0; bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
  return Result<ThroughAllPlacement>::success(make_through_all_placement(axis.value(), x_min, y_min, z_min, x_max, y_max, z_max));
}

} // namespace

Result<GeometryShape> ClosedProfileAdapter::make_extruded_profile_from_curves(const std::vector<ClosedProfileCurveSegment>& curves, Vector3 direction, const Quantity& depth) const {
  if (depth.millimeters() <= 0.0) return Result<GeometryShape>::failure(make_geometry_error("arc closed profile extrusion depth must be greater than zero"));
  try { auto face = make_curve_face(curves); if (face.has_error()) return Result<GeometryShape>::failure(face.error()); auto shape = make_prism_shape_from_face(face.value(), direction, depth); if (shape.has_error()) return Result<GeometryShape>::failure(shape.error()); return Result<GeometryShape>::success(GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value())))); } catch (const Standard_Failure& failure) { return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure))); } catch (const std::exception& exception) { return Result<GeometryShape>::failure(make_geometry_error(exception.what())); } catch (...) { return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error")); }
}

Result<GeometryShape> ClosedProfileAdapter::cut_profile_curves_through_all(const GeometryShape& target, const std::vector<ClosedProfileCurveSegment>& curves, Vector3 axis_direction) const {
  if (target.empty()) return Result<GeometryShape>::failure(make_geometry_error("arc closed profile cut requires a non-empty target shape"));
  try { auto placement = make_placement_from_target(target, axis_direction); if (placement.has_error()) return Result<GeometryShape>::failure(placement.error()); auto moved_curves = move_curves_to_through_all_start(curves, placement.value()); if (moved_curves.has_error()) return Result<GeometryShape>::failure(moved_curves.error()); auto face = make_curve_face(moved_curves.value()); if (face.has_error()) return Result<GeometryShape>::failure(face.error()); BRepPrimAPI_MakePrism prism_builder(face.value(), gp_Vec(placement.value().axis.x * placement.value().length, placement.value().axis.y * placement.value().length, placement.value().axis.z * placement.value().length)); prism_builder.Build(); if (!prism_builder.IsDone()) return Result<GeometryShape>::failure(make_geometry_error("could not build arc profile cut prism")); const TopoDS_Shape& target_shape = target.impl_->shape; BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape()); cut_builder.Build(); if (!cut_builder.IsDone() || cut_builder.HasErrors()) return Result<GeometryShape>::failure(make_geometry_error("arc profile boolean cut failed")); TopoDS_Shape shape = cut_builder.Shape(); if (shape.IsNull()) return Result<GeometryShape>::failure(make_geometry_error("arc profile cut produced an empty shape")); return Result<GeometryShape>::success(GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape)))); } catch (const Standard_Failure& failure) { return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure))); } catch (const std::exception& exception) { return Result<GeometryShape>::failure(make_geometry_error(exception.what())); } catch (...) { return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error")); }
}

} // namespace blcad::geometry
