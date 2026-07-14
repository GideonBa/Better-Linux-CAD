#include "blcad/geometry/revolve_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double kTolerance = 1.0e-8;
constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error revolve_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] gp_Pnt point(Point3 value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] Vector3 between(Point3 from, Point3 to) {
  return {to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] double dot(Vector3 left, Vector3 right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) {
  return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

[[nodiscard]] double length(Vector3 value) {
  return std::sqrt(dot(value, value));
}

[[nodiscard]] Vector3 normalized(Vector3 value) {
  const double magnitude = length(value);
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] bool same_point(Point3 left, Point3 right) {
  return length(between(left, right)) <= kTolerance;
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT revolve operation failed" : message;
}

[[nodiscard]] Result<TopoDS_Wire>
polygon_wire(const FeatureId& id, const std::vector<Point3>& vertices, std::string_view role) {
  if (vertices.size() < 3U)
    return Result<TopoDS_Wire>::failure(
        revolve_error(id, std::string(role) + " profile requires at least three vertices"));
  BRepBuilderAPI_MakePolygon builder;
  for (Point3 vertex : vertices)
    builder.Add(point(vertex));
  builder.Close();
  if (!builder.IsDone())
    return Result<TopoDS_Wire>::failure(
        revolve_error(id, std::string("could not build ") + std::string(role) + " profile wire"));
  return Result<TopoDS_Wire>::success(builder.Wire());
}

[[nodiscard]] Result<TopoDS_Face> polygon_face(const FeatureId& id,
                                               const std::vector<Point3>& outer,
                                               const std::vector<std::vector<Point3>>& inner) {
  auto outer_wire = polygon_wire(id, outer, "outer");
  if (outer_wire.has_error())
    return Result<TopoDS_Face>::failure(outer_wire.error());
  BRepBuilderAPI_MakeFace builder(outer_wire.value());
  for (const auto& contour : inner) {
    auto wire = polygon_wire(id, contour, "inner");
    if (wire.has_error())
      return Result<TopoDS_Face>::failure(wire.error());
    builder.Add(TopoDS::Wire(wire.value().Reversed()));
  }
  if (!builder.IsDone())
    return Result<TopoDS_Face>::failure(revolve_error(id, "could not build revolve profile face"));
  return Result<TopoDS_Face>::success(builder.Face());
}

[[nodiscard]] Result<TopoDS_Edge> curve_edge(const FeatureId& id,
                                             const ClosedProfileCurveSegment& curve) {
  if (curve.kind == ClosedProfileCurveKind::Line) {
    if (same_point(curve.start, curve.end))
      return Result<TopoDS_Edge>::failure(revolve_error(id, "revolve line must not be degenerate"));
    BRepBuilderAPI_MakeEdge builder(point(curve.start), point(curve.end));
    if (!builder.IsDone())
      return Result<TopoDS_Edge>::failure(revolve_error(id, "could not build revolve line"));
    return Result<TopoDS_Edge>::success(builder.Edge());
  }
  if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
    if (same_point(curve.start, curve.end))
      return Result<TopoDS_Edge>::failure(
          revolve_error(id, "revolve spline endpoints must be distinct"));
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, point(curve.start));
    poles.SetValue(2, point(curve.mid));
    poles.SetValue(3, point(curve.control2));
    poles.SetValue(4, point(curve.end));
    Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
    BRepBuilderAPI_MakeEdge builder(bezier);
    if (!builder.IsDone())
      return Result<TopoDS_Edge>::failure(revolve_error(id, "could not build revolve spline"));
    return Result<TopoDS_Edge>::success(builder.Edge());
  }
  if (same_point(curve.start, curve.mid) || same_point(curve.mid, curve.end) ||
      same_point(curve.start, curve.end))
    return Result<TopoDS_Edge>::failure(revolve_error(id, "revolve arc points must be distinct"));
  GC_MakeArcOfCircle arc(point(curve.start), point(curve.mid), point(curve.end));
  if (!arc.IsDone())
    return Result<TopoDS_Edge>::failure(revolve_error(id, "could not build revolve arc"));
  BRepBuilderAPI_MakeEdge builder(arc.Value());
  if (!builder.IsDone())
    return Result<TopoDS_Edge>::failure(revolve_error(id, "could not build revolve arc edge"));
  return Result<TopoDS_Edge>::success(builder.Edge());
}

[[nodiscard]] Result<TopoDS_Face> curve_face(const FeatureId& id,
                                             const std::vector<ClosedProfileCurveSegment>& curves) {
  if (curves.size() < 2U)
    return Result<TopoDS_Face>::failure(
        revolve_error(id, "revolve curve profile requires at least two segments"));
  BRepBuilderAPI_MakeWire wire;
  for (const auto& curve : curves) {
    auto edge = curve_edge(id, curve);
    if (edge.has_error())
      return Result<TopoDS_Face>::failure(edge.error());
    wire.Add(edge.value());
  }
  if (!wire.IsDone())
    return Result<TopoDS_Face>::failure(revolve_error(id, "could not build revolve curve wire"));
  BRepBuilderAPI_MakeFace face(wire.Wire());
  if (!face.IsDone())
    return Result<TopoDS_Face>::failure(revolve_error(id, "could not build revolve curve face"));
  return Result<TopoDS_Face>::success(face.Face());
}

[[nodiscard]] Result<std::size_t> validate_axis_and_profile(const FeatureId& id,
                                                            const std::vector<Point3>& samples,
                                                            Point3 axis_origin,
                                                            Vector3 axis_direction) {
  if (samples.size() < 3U || length(axis_direction) <= kTolerance)
    return Result<std::size_t>::failure(
        revolve_error(id, "revolve requires a non-degenerate profile and axis"));
  const Vector3 axis = normalized(axis_direction);
  Vector3 normal;
  bool found_plane = false;
  for (std::size_t first = 1U; first < samples.size() && !found_plane; ++first) {
    for (std::size_t second = first + 1U; second < samples.size(); ++second) {
      normal = cross(between(samples[0], samples[first]), between(samples[0], samples[second]));
      if (length(normal) > kTolerance) {
        normal = normalized(normal);
        found_plane = true;
        break;
      }
    }
  }
  if (!found_plane)
    return Result<std::size_t>::failure(revolve_error(id, "revolve profile has zero planar area"));
  if (std::abs(dot(normal, axis)) > kTolerance)
    return Result<std::size_t>::failure(
        revolve_error(id, "revolve axis must lie in the profile plane"));
  if (std::abs(dot(between(samples[0], axis_origin), normal)) > kTolerance)
    return Result<std::size_t>::failure(
        revolve_error(id, "revolve axis origin must lie in the profile plane"));
  for (Point3 sample : samples)
    if (std::abs(dot(between(samples[0], sample), normal)) > kTolerance)
      return Result<std::size_t>::failure(revolve_error(id, "revolve profile must be planar"));

  const Vector3 radial = normalized(cross(normal, axis));
  double minimum = dot(between(axis_origin, samples.front()), radial);
  double maximum = minimum;
  for (Point3 sample : samples) {
    const double distance = dot(between(axis_origin, sample), radial);
    minimum = std::min(minimum, distance);
    maximum = std::max(maximum, distance);
  }
  if (minimum < -kTolerance && maximum > kTolerance)
    return Result<std::size_t>::failure(
        revolve_error(id, "revolve profile crosses its axis and would self-intersect"));
  if (std::max(std::abs(minimum), std::abs(maximum)) <= kTolerance)
    return Result<std::size_t>::failure(revolve_error(id, "revolve profile lies on its axis"));
  return Result<std::size_t>::success(samples.size());
}

[[nodiscard]] Result<TopoDS_Shape> revolve_face(const FeatureId& id, TopoDS_Face face,
                                                Point3 axis_origin, Vector3 axis_direction,
                                                double start_angle_deg, double sweep_angle_deg) {
  if (!std::isfinite(start_angle_deg) || !std::isfinite(sweep_angle_deg) ||
      std::abs(sweep_angle_deg) <= kTolerance || std::abs(sweep_angle_deg) > 360.0 + kTolerance)
    return Result<TopoDS_Shape>::failure(revolve_error(id, "revolve sweep angle is invalid"));
  const Vector3 axis = normalized(axis_direction);
  const gp_Ax1 rotation_axis(point(axis_origin), gp_Dir(axis.x, axis.y, axis.z));
  TopoDS_Shape start_face = face;
  if (std::abs(start_angle_deg) > kTolerance) {
    gp_Trsf transform;
    transform.SetRotation(rotation_axis, start_angle_deg * kPi / 180.0);
    BRepBuilderAPI_Transform rotated(face, transform, true);
    if (!rotated.IsDone())
      return Result<TopoDS_Shape>::failure(
          revolve_error(id, "could not place symmetric revolve start profile"));
    start_face = rotated.Shape();
  }
  BRepPrimAPI_MakeRevol builder(start_face, rotation_axis, sweep_angle_deg * kPi / 180.0, true);
  builder.Build();
  if (!builder.IsDone())
    return Result<TopoDS_Shape>::failure(revolve_error(id, "OCCT revolve did not complete"));
  TopoDS_Shape result = builder.Shape();
  if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More())
    return Result<TopoDS_Shape>::failure(revolve_error(id, "revolve produced no solid result"));
  if (!BRepCheck_Analyzer(result, true).IsValid())
    return Result<TopoDS_Shape>::failure(
        revolve_error(id, "revolve result is invalid or self-intersecting"));
  GProp_GProps properties;
  BRepGProp::VolumeProperties(result, properties);
  if (!std::isfinite(properties.Mass()) || properties.Mass() <= kTolerance)
    return Result<TopoDS_Shape>::failure(
        revolve_error(id, "revolve result has no positive volume"));
  return Result<TopoDS_Shape>::success(std::move(result));
}

template <typename FaceFactory>
[[nodiscard]] Result<TopoDS_Shape>
execute_revolve(const FeatureId& id, const std::vector<Point3>& samples, Point3 axis_origin,
                Vector3 axis_direction, double start_angle_deg, double sweep_angle_deg,
                FaceFactory&& face_factory) {
  if (id.empty())
    return Result<TopoDS_Shape>::failure(
        Error::validation("revolve", "feature id must not be empty"));
  auto valid = validate_axis_and_profile(id, samples, axis_origin, axis_direction);
  if (valid.has_error())
    return Result<TopoDS_Shape>::failure(valid.error());
  try {
    auto face = face_factory();
    if (face.has_error())
      return Result<TopoDS_Shape>::failure(face.error());
    return revolve_face(id, face.value(), axis_origin, axis_direction, start_angle_deg,
                        sweep_angle_deg);
  } catch (const Standard_Failure& failure) {
    return Result<TopoDS_Shape>::failure(revolve_error(id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<TopoDS_Shape>::failure(revolve_error(id, exception.what()));
  } catch (...) {
    return Result<TopoDS_Shape>::failure(revolve_error(id, "unknown revolve geometry error"));
  }
}

} // namespace

Result<GeometryShape> RevolveAdapter::revolve_profile(FeatureId feature_id,
                                                      const std::vector<Point3>& vertices,
                                                      Point3 axis_origin, Vector3 axis_direction,
                                                      double start_angle_deg,
                                                      double sweep_angle_deg) const {
  auto shape =
      execute_revolve(feature_id, vertices, axis_origin, axis_direction, start_angle_deg,
                      sweep_angle_deg, [&] { return polygon_face(feature_id, vertices, {}); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

Result<GeometryShape> RevolveAdapter::revolve_profile_with_holes(
    FeatureId feature_id, const std::vector<Point3>& outer_vertices,
    const std::vector<std::vector<Point3>>& inner_vertices, Point3 axis_origin,
    Vector3 axis_direction, double start_angle_deg, double sweep_angle_deg) const {
  std::vector<Point3> samples = outer_vertices;
  for (const auto& contour : inner_vertices)
    samples.insert(samples.end(), contour.begin(), contour.end());
  auto shape = execute_revolve(
      feature_id, samples, axis_origin, axis_direction, start_angle_deg, sweep_angle_deg,
      [&] { return polygon_face(feature_id, outer_vertices, inner_vertices); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

Result<GeometryShape> RevolveAdapter::revolve_profile_curves(
    FeatureId feature_id, const std::vector<ClosedProfileCurveSegment>& curves, Point3 axis_origin,
    Vector3 axis_direction, double start_angle_deg, double sweep_angle_deg) const {
  std::vector<Point3> samples;
  samples.reserve(curves.size() * 4U);
  for (const auto& curve : curves) {
    samples.push_back(curve.start);
    samples.push_back(curve.mid);
    samples.push_back(curve.end);
    if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline)
      samples.push_back(curve.control2);
  }
  auto shape = execute_revolve(feature_id, samples, axis_origin, axis_direction, start_angle_deg,
                               sweep_angle_deg, [&] { return curve_face(feature_id, curves); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

} // namespace blcad::geometry
