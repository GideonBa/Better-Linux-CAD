#include "blcad/geometry/sweep_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

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

[[nodiscard]] Error sweep_error(const FeatureId& id, std::string message) {
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

[[nodiscard]] Vector3 rotated(Vector3 value, Vector3 axis, double angle) {
  axis = normalized(axis);
  const double c = std::cos(angle), s = std::sin(angle);
  const Vector3 crossed = cross(axis, value);
  const double axial = dot(axis, value) * (1.0 - c);
  return {value.x * c + crossed.x * s + axis.x * axial,
          value.y * c + crossed.y * s + axis.y * axial,
          value.z * c + crossed.z * s + axis.z * axial};
}

[[nodiscard]] std::vector<SweepPathSegment>
make_twist_guide(const std::vector<SweepPathSegment>& path, double twist_angle_deg) {
  std::vector<Point3> points;
  points.reserve(path.size() + 1U);
  points.push_back(path.front().start);
  for (const auto& segment : path)
    points.push_back(segment.end);
  std::vector<double> distances(points.size(), 0.0);
  for (std::size_t index = 1U; index < points.size(); ++index)
    distances[index] = distances[index - 1U] + length(between(points[index - 1U], points[index]));
  const double total = distances.back();
  Vector3 tangent = normalized(between(points[0], points[1]));
  const Vector3 helper =
      std::abs(tangent.z) < 0.9 ? Vector3{0.0, 0.0, 1.0} : Vector3{0.0, 1.0, 0.0};
  Vector3 transported = normalized(cross(tangent, helper));
  std::vector<Point3> guide_points;
  guide_points.reserve(points.size());
  for (std::size_t index = 0U; index < points.size(); ++index) {
    if (index + 1U < points.size())
      tangent = normalized(between(points[index], points[index + 1U]));
    const Vector3 projected{transported.x - tangent.x * dot(transported, tangent),
                            transported.y - tangent.y * dot(transported, tangent),
                            transported.z - tangent.z * dot(transported, tangent)};
    if (length(projected) > kTolerance)
      transported = normalized(projected);
    const double fraction = total <= kTolerance ? 0.0 : distances[index] / total;
    const Vector3 offset = rotated(transported, tangent, twist_angle_deg * kPi / 180.0 * fraction);
    guide_points.push_back(
        {points[index].x + offset.x, points[index].y + offset.y, points[index].z + offset.z});
  }
  std::vector<SweepPathSegment> guide;
  guide.reserve(path.size());
  for (std::size_t index = 1U; index < guide_points.size(); ++index)
    guide.push_back(
        {SweepPathSegmentKind::Line, guide_points[index - 1U], {}, guide_points[index]});
  return guide;
}

[[nodiscard]] bool same_point(Point3 left, Point3 right) {
  return length(between(left, right)) <= kTolerance;
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT sweep operation failed" : message;
}

[[nodiscard]] Result<TopoDS_Edge> path_edge(const FeatureId& id, const SweepPathSegment& segment,
                                            std::string_view role) {
  if (same_point(segment.start, segment.end))
    return Result<TopoDS_Edge>::failure(
        sweep_error(id, std::string(role) + " segment endpoints must be distinct"));
  if (segment.kind == SweepPathSegmentKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(
          sweep_error(id, std::string("could not build ") + std::string(role) + " line"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  if (same_point(segment.start, segment.mid) || same_point(segment.mid, segment.end))
    return Result<TopoDS_Edge>::failure(
        sweep_error(id, std::string(role) + " arc points must be distinct"));
  GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
  if (!arc.IsDone())
    return Result<TopoDS_Edge>::failure(
        sweep_error(id, std::string("could not build ") + std::string(role) + " arc"));
  BRepBuilderAPI_MakeEdge edge(arc.Value());
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(
        sweep_error(id, std::string("could not build ") + std::string(role) + " arc edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

[[nodiscard]] Result<TopoDS_Edge> profile_edge(const FeatureId& id,
                                               const ClosedProfileCurveSegment& segment) {
  if (segment.kind == ClosedProfileCurveKind::Line)
    return path_edge(id, {SweepPathSegmentKind::Line, segment.start, {}, segment.end}, "profile");
  if (segment.kind == ClosedProfileCurveKind::CircularArc)
    return path_edge(id,
                     {SweepPathSegmentKind::CircularArc, segment.start, segment.mid, segment.end},
                     "profile");
  if (same_point(segment.start, segment.end))
    return Result<TopoDS_Edge>::failure(
        sweep_error(id, "profile spline endpoints must be distinct"));
  TColgp_Array1OfPnt poles(1, 4);
  poles.SetValue(1, point(segment.start));
  poles.SetValue(2, point(segment.mid));
  poles.SetValue(3, point(segment.control2));
  poles.SetValue(4, point(segment.end));
  Handle(Geom_BezierCurve) bezier = new Geom_BezierCurve(poles);
  BRepBuilderAPI_MakeEdge edge(bezier);
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(sweep_error(id, "could not build profile spline"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

template <typename Segment, typename EdgeFactory>
[[nodiscard]] Result<TopoDS_Wire>
make_wire(const FeatureId& id, const std::vector<Segment>& segments, std::string_view role,
          bool closed, EdgeFactory&& edge_factory) {
  if (segments.empty())
    return Result<TopoDS_Wire>::failure(
        sweep_error(id, std::string(role) + " requires at least one segment"));
  for (std::size_t index = 1U; index < segments.size(); ++index)
    if (!same_point(segments[index - 1U].end, segments[index].start))
      return Result<TopoDS_Wire>::failure(
          sweep_error(id, std::string(role) + " segments must be connected in order"));
  if (closed && !same_point(segments.back().end, segments.front().start))
    return Result<TopoDS_Wire>::failure(sweep_error(id, std::string(role) + " must be closed"));
  if (!closed && segments.size() > 1U && same_point(segments.back().end, segments.front().start))
    return Result<TopoDS_Wire>::failure(sweep_error(id, std::string(role) + " must remain open"));

  std::vector<TopoDS_Edge> edges;
  edges.reserve(segments.size());
  for (const auto& segment : segments) {
    auto edge = edge_factory(segment);
    if (edge.has_error())
      return Result<TopoDS_Wire>::failure(edge.error());
    edges.push_back(edge.value());
  }
  for (std::size_t first = 0U; first < edges.size(); ++first)
    for (std::size_t second = first + 2U; second < edges.size(); ++second) {
      if (closed && first == 0U && second + 1U == edges.size())
        continue;
      BRepExtrema_DistShapeShape distance(edges[first], edges[second]);
      distance.Perform();
      if (distance.IsDone() && distance.Value() <= kTolerance)
        return Result<TopoDS_Wire>::failure(sweep_error(
            id, std::string(role) + " must not self-intersect or repeat branch junctions"));
    }
  BRepBuilderAPI_MakeWire wire;
  for (const auto& edge : edges)
    wire.Add(edge);
  if (!wire.IsDone())
    return Result<TopoDS_Wire>::failure(
        sweep_error(id, std::string("could not build ") + std::string(role) + " wire"));
  return Result<TopoDS_Wire>::success(wire.Wire());
}

[[nodiscard]] Result<TopoDS_Shape>
execute_sweep(const FeatureId& id, const TopoDS_Wire& profile, const TopoDS_Wire& spine,
              const SweepPathSegment& first_path, PathOrientationRule orientation_rule,
              std::optional<Vector3> fixed_up_vector, const std::optional<TopoDS_Wire>& guide,
              bool solid) {
  try {
    const Vector3 tangent = between(first_path.start, first_path.end);
    if (length(tangent) <= kTolerance)
      return Result<TopoDS_Shape>::failure(sweep_error(id, "sweep path has no start tangent"));
    BRepOffsetAPI_MakePipeShell builder(spine);
    if (guide.has_value()) {
      builder.SetMode(*guide, Standard_True, BRepFill_NoContact);
    } else if (orientation_rule == PathOrientationRule::ProfileNormal)
      builder.SetMode(Standard_True);
    else if (orientation_rule == PathOrientationRule::MinimumTwist)
      builder.SetMode(Standard_False);
    else {
      if (!fixed_up_vector.has_value() || length(*fixed_up_vector) <= kTolerance)
        return Result<TopoDS_Shape>::failure(
            sweep_error(id, "fixed-up sweep orientation requires a non-zero vector"));
      if (length(cross(tangent, *fixed_up_vector)) <= kTolerance)
        return Result<TopoDS_Shape>::failure(
            sweep_error(id, "fixed-up vector must not be parallel to the path start tangent"));
      builder.SetMode(gp_Dir(fixed_up_vector->x, fixed_up_vector->y, fixed_up_vector->z));
    }
    builder.Add(profile, Standard_False, Standard_False);
    if (!builder.IsReady())
      return Result<TopoDS_Shape>::failure(sweep_error(id, "sweep profile is not ready"));
    builder.Build();
    if (!builder.IsDone())
      return Result<TopoDS_Shape>::failure(sweep_error(id, "OCCT sweep did not complete"));
    if (solid && !builder.MakeSolid())
      return Result<TopoDS_Shape>::failure(
          sweep_error(id, "closed sweep profile did not produce a solid"));
    TopoDS_Shape result = builder.Shape();
    if (result.IsNull() || !BRepCheck_Analyzer(result, true).IsValid())
      return Result<TopoDS_Shape>::failure(
          sweep_error(id, "sweep result is invalid or self-intersecting"));
    if (solid) {
      if (!TopExp_Explorer(result, TopAbs_SOLID).More())
        return Result<TopoDS_Shape>::failure(sweep_error(id, "sweep produced no solid result"));
      GProp_GProps properties;
      BRepGProp::VolumeProperties(result, properties);
      if (!std::isfinite(properties.Mass()) || properties.Mass() <= kTolerance)
        return Result<TopoDS_Shape>::failure(
            sweep_error(id, "sweep result has no positive volume"));
    } else if (!TopExp_Explorer(result, TopAbs_FACE).More() ||
               TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return Result<TopoDS_Shape>::failure(
          sweep_error(id, "surface sweep must produce faces without a solid"));
    }
    return Result<TopoDS_Shape>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<TopoDS_Shape>::failure(sweep_error(id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<TopoDS_Shape>::failure(sweep_error(id, exception.what()));
  } catch (...) {
    return Result<TopoDS_Shape>::failure(sweep_error(id, "unknown sweep geometry error"));
  }
}

} // namespace

Result<GeometryShape> SweepAdapter::sweep_closed_profile(
    FeatureId feature_id, const std::vector<ClosedProfileCurveSegment>& profile,
    const std::vector<SweepPathSegment>& path, PathOrientationRule orientation_rule,
    std::optional<Vector3> fixed_up_vector, double twist_angle_deg,
    const std::vector<SweepPathSegment>* guide) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("sweep", "feature id must not be empty"));
  auto profile_wire = make_wire(
      feature_id, profile, "sweep profile", true,
      [&](const ClosedProfileCurveSegment& segment) { return profile_edge(feature_id, segment); });
  if (profile_wire.has_error())
    return Result<GeometryShape>::failure(profile_wire.error());
  auto path_wire =
      make_wire(feature_id, path, "sweep path", false, [&](const SweepPathSegment& segment) {
        return path_edge(feature_id, segment, "path");
      });
  if (path_wire.has_error())
    return Result<GeometryShape>::failure(path_wire.error());
  if (guide != nullptr && std::abs(twist_angle_deg) > kTolerance)
    return Result<GeometryShape>::failure(
        sweep_error(feature_id, "combined guide and twist are not supported"));
  std::vector<SweepPathSegment> generated;
  if (guide == nullptr && std::abs(twist_angle_deg) > kTolerance) {
    generated = make_twist_guide(path, twist_angle_deg);
    guide = &generated;
  }
  std::optional<TopoDS_Wire> guide_wire;
  if (guide != nullptr) {
    auto built =
        make_wire(feature_id, *guide, "sweep guide", false, [&](const SweepPathSegment& segment) {
          return path_edge(feature_id, segment, "guide");
        });
    if (built.has_error())
      return Result<GeometryShape>::failure(built.error());
    guide_wire = built.value();
  }
  auto shape = execute_sweep(feature_id, profile_wire.value(), path_wire.value(), path.front(),
                             orientation_rule, fixed_up_vector, guide_wire, true);
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

Result<GeometryShape>
SweepAdapter::sweep_open_profile(FeatureId feature_id, const std::vector<SweepPathSegment>& profile,
                                 const std::vector<SweepPathSegment>& path,
                                 PathOrientationRule orientation_rule,
                                 std::optional<Vector3> fixed_up_vector, double twist_angle_deg,
                                 const std::vector<SweepPathSegment>* guide) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("sweep", "feature id must not be empty"));
  auto profile_wire = make_wire(
      feature_id, profile, "surface profile", false,
      [&](const SweepPathSegment& segment) { return path_edge(feature_id, segment, "profile"); });
  if (profile_wire.has_error())
    return Result<GeometryShape>::failure(profile_wire.error());
  auto path_wire =
      make_wire(feature_id, path, "sweep path", false, [&](const SweepPathSegment& segment) {
        return path_edge(feature_id, segment, "path");
      });
  if (path_wire.has_error())
    return Result<GeometryShape>::failure(path_wire.error());
  if (guide != nullptr && std::abs(twist_angle_deg) > kTolerance)
    return Result<GeometryShape>::failure(
        sweep_error(feature_id, "combined guide and twist are not supported"));
  std::vector<SweepPathSegment> generated;
  if (guide == nullptr && std::abs(twist_angle_deg) > kTolerance) {
    generated = make_twist_guide(path, twist_angle_deg);
    guide = &generated;
  }
  std::optional<TopoDS_Wire> guide_wire;
  if (guide != nullptr) {
    auto built =
        make_wire(feature_id, *guide, "sweep guide", false, [&](const SweepPathSegment& segment) {
          return path_edge(feature_id, segment, "guide");
        });
    if (built.has_error())
      return Result<GeometryShape>::failure(built.error());
    guide_wire = built.value();
  }
  auto shape = execute_sweep(feature_id, profile_wire.value(), path_wire.value(), path.front(),
                             orientation_rule, fixed_up_vector, guide_wire, false);
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

} // namespace blcad::geometry
