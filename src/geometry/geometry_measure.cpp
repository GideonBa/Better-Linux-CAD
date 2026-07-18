#include "blcad/geometry/geometry_measure.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace blcad::geometry {
namespace {

constexpr double epsilon = 1.0e-12;

Vector3 subtract(Point3 a, Point3 b) noexcept {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Point3 add(Point3 p, Vector3 v) noexcept {
  return {p.x + v.x, p.y + v.y, p.z + v.z};
}
Vector3 scale(Vector3 v, double factor) noexcept {
  return {v.x * factor, v.y * factor, v.z * factor};
}
double dot(Vector3 a, Vector3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vector3 cross(Vector3 a, Vector3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double magnitude(Vector3 v) noexcept {
  return std::sqrt(dot(v, v));
}
Vector3 normalized(Vector3 v) noexcept {
  const double length = magnitude(v);
  return length > epsilon ? scale(v, 1.0 / length) : Vector3{};
}
bool finite(Point3 p) noexcept {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool finite(Vector3 v) noexcept {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Error measure_error(std::string message) {
  return Error::validation("geometry.measure", std::move(message));
}

Result<GeometryDistanceMeasurement> point_point(Point3 a, Point3 b) {
  if (!finite(a) || !finite(b))
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("measure points must be finite"));
  return Result<GeometryDistanceMeasurement>::success({magnitude(subtract(b, a)), a, b});
}

Result<GeometryDistanceMeasurement>
point_line(Point3 point, const AssemblyLineTargetDescriptor& line, bool point_first) {
  const Vector3 direction = normalized(line.direction);
  if (!finite(point) || !finite(line.origin) || !finite(line.direction) ||
      magnitude(direction) <= epsilon)
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("point/edge distance requires a finite point and line"));
  const Point3 witness =
      add(line.origin, scale(direction, dot(subtract(point, line.origin), direction)));
  const double distance = magnitude(subtract(point, witness));
  return point_first ? Result<GeometryDistanceMeasurement>::success({distance, point, witness})
                     : Result<GeometryDistanceMeasurement>::success({distance, witness, point});
}

Result<GeometryDistanceMeasurement>
point_plane(Point3 point, const AssemblyPlanarTargetDescriptor& plane, bool point_first) {
  const Vector3 normal = normalized(plane.normal);
  if (!finite(point) || !finite(plane.origin) || !finite(plane.normal) ||
      magnitude(normal) <= epsilon)
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("point/face distance requires a finite point and plane"));
  const double signed_distance = dot(subtract(point, plane.origin), normal);
  const Point3 witness = add(point, scale(normal, -signed_distance));
  return point_first ? Result<GeometryDistanceMeasurement>::success(
                           {std::abs(signed_distance), point, witness})
                     : Result<GeometryDistanceMeasurement>::success(
                           {std::abs(signed_distance), witness, point});
}

Result<GeometryDistanceMeasurement> line_line(const AssemblyLineTargetDescriptor& a,
                                              const AssemblyLineTargetDescriptor& b) {
  const Vector3 u = normalized(a.direction);
  const Vector3 v = normalized(b.direction);
  if (!finite(a.origin) || !finite(b.origin) || magnitude(u) <= epsilon || magnitude(v) <= epsilon)
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("edge/edge distance requires two finite lines"));
  const Vector3 w = subtract(a.origin, b.origin);
  const double uv = dot(u, v);
  const double denominator = 1.0 - uv * uv;
  double ta = 0.0;
  double tb = 0.0;
  if (std::abs(denominator) > epsilon) {
    ta = (uv * dot(v, w) - dot(u, w)) / denominator;
    tb = (dot(v, w) - uv * dot(u, w)) / denominator;
  } else {
    tb = dot(v, w);
  }
  const Point3 witness_a = add(a.origin, scale(u, ta));
  const Point3 witness_b = add(b.origin, scale(v, tb));
  return point_point(witness_a, witness_b);
}

Result<GeometryDistanceMeasurement> line_plane(const AssemblyLineTargetDescriptor& line,
                                               const AssemblyPlanarTargetDescriptor& plane,
                                               bool line_first) {
  const Vector3 direction = normalized(line.direction);
  const Vector3 normal = normalized(plane.normal);
  if (!finite(line.origin) || !finite(line.direction) || !finite(plane.origin) ||
      !finite(plane.normal) || magnitude(direction) <= epsilon || magnitude(normal) <= epsilon)
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("edge/face distance requires a finite line and plane"));
  const double denominator = dot(direction, normal);
  if (std::abs(denominator) > epsilon) {
    const Point3 intersection =
        add(line.origin,
            scale(direction, dot(subtract(plane.origin, line.origin), normal) / denominator));
    return Result<GeometryDistanceMeasurement>::success({0.0, intersection, intersection});
  }
  auto result = point_plane(line.origin, plane, true);
  if (result.has_error() || line_first)
    return result;
  return Result<GeometryDistanceMeasurement>::success(
      {result.value().distance_mm, result.value().witness_b, result.value().witness_a});
}

Result<GeometryDistanceMeasurement> plane_plane(const AssemblyPlanarTargetDescriptor& a,
                                                const AssemblyPlanarTargetDescriptor& b) {
  const Vector3 na = normalized(a.normal);
  const Vector3 nb = normalized(b.normal);
  if (!finite(a.origin) || !finite(b.origin) || !finite(a.normal) || !finite(b.normal) ||
      magnitude(na) <= epsilon || magnitude(nb) <= epsilon)
    return Result<GeometryDistanceMeasurement>::failure(
        measure_error("face/face distance requires two finite planes"));
  if (magnitude(cross(na, nb)) > epsilon)
    return Result<GeometryDistanceMeasurement>::success({0.0, a.origin, a.origin});
  return point_plane(a.origin, b, true);
}

Result<Vector3> direction(const GeometryMeasureTarget& target) {
  Vector3 value;
  if (const auto* line = std::get_if<AssemblyLineTargetDescriptor>(&target))
    value = line->direction;
  else if (const auto* plane = std::get_if<AssemblyPlanarTargetDescriptor>(&target))
    value = plane->normal;
  else
    return Result<Vector3>::failure(
        measure_error("angle requires linear-edge or planar-face targets"));
  if (!finite(value) || magnitude(value) <= epsilon)
    return Result<Vector3>::failure(measure_error("angle target direction must be finite"));
  return Result<Vector3>::success(normalized(value));
}

const Point3* point(const GeometryMeasureTarget& target) noexcept {
  if (const auto* value = std::get_if<Point3>(&target))
    return value;
  if (const auto* value = std::get_if<AssemblyPointTargetDescriptor>(&target))
    return &value->point;
  return nullptr;
}

} // namespace

Result<GeometryDistanceMeasurement>
GeometryMeasureQuery::distance(const GeometryMeasureTarget& a,
                               const GeometryMeasureTarget& b) const {
  if (const auto* point_a = point(a)) {
    if (const auto* point_b = point(b))
      return point_point(*point_a, *point_b);
    if (const auto* line_b = std::get_if<AssemblyLineTargetDescriptor>(&b))
      return point_line(*point_a, *line_b, true);
    if (const auto* plane_b = std::get_if<AssemblyPlanarTargetDescriptor>(&b))
      return point_plane(*point_a, *plane_b, true);
  }
  if (const auto* line_a = std::get_if<AssemblyLineTargetDescriptor>(&a)) {
    if (const auto* point_b = point(b))
      return point_line(*point_b, *line_a, false);
    if (const auto* line_b = std::get_if<AssemblyLineTargetDescriptor>(&b))
      return line_line(*line_a, *line_b);
    if (const auto* plane_b = std::get_if<AssemblyPlanarTargetDescriptor>(&b))
      return line_plane(*line_a, *plane_b, true);
  }
  if (const auto* plane_a = std::get_if<AssemblyPlanarTargetDescriptor>(&a)) {
    if (const auto* point_b = point(b))
      return point_plane(*point_b, *plane_a, false);
    if (const auto* plane_b = std::get_if<AssemblyPlanarTargetDescriptor>(&b))
      return plane_plane(*plane_a, *plane_b);
    if (const auto* line_b = std::get_if<AssemblyLineTargetDescriptor>(&b))
      return line_plane(*line_b, *plane_a, false);
  }
  return Result<GeometryDistanceMeasurement>::failure(
      measure_error("unsupported point/edge/face distance target combination"));
}

Result<GeometryAngleMeasurement> GeometryMeasureQuery::angle(const GeometryMeasureTarget& a,
                                                             const GeometryMeasureTarget& b) const {
  auto first = direction(a);
  auto second = direction(b);
  if (first.has_error())
    return Result<GeometryAngleMeasurement>::failure(first.error());
  if (second.has_error())
    return Result<GeometryAngleMeasurement>::failure(second.error());
  if (magnitude(first.value()) <= epsilon || magnitude(second.value()) <= epsilon)
    return Result<GeometryAngleMeasurement>::failure(
        measure_error("angle target direction must be non-degenerate"));
  const double cosine = std::clamp(std::abs(dot(first.value(), second.value())), 0.0, 1.0);
  const bool first_plane = std::holds_alternative<AssemblyPlanarTargetDescriptor>(a);
  const bool second_plane = std::holds_alternative<AssemblyPlanarTargetDescriptor>(b);
  const double radians = first_plane != second_plane ? std::asin(cosine) : std::acos(cosine);
  return Result<GeometryAngleMeasurement>::success({radians * 180.0 / std::numbers::pi});
}

Result<GeometryRadialMeasurement>
GeometryMeasureQuery::radius_diameter(const GeometryMeasureTarget& target) const {
  double radius = 0.0;
  if (const auto* circle = std::get_if<AssemblyCircularEdgeTargetDescriptor>(&target)) {
    if (!finite(circle->center) || !finite(circle->normal) || magnitude(circle->normal) <= epsilon)
      return Result<GeometryRadialMeasurement>::failure(
          measure_error("circular edge target must be finite and non-degenerate"));
    radius = circle->radius_mm;
  } else if (const auto* cylinder =
                 std::get_if<AssemblyCylindricalSurfaceTargetDescriptor>(&target)) {
    if (!finite(cylinder->axis_origin) || !finite(cylinder->axis_direction) ||
        magnitude(cylinder->axis_direction) <= epsilon)
      return Result<GeometryRadialMeasurement>::failure(
          measure_error("cylindrical face target must be finite and non-degenerate"));
    radius = cylinder->radius_mm;
  } else
    return Result<GeometryRadialMeasurement>::failure(
        measure_error("radius/diameter requires a circular edge or cylindrical face"));
  if (!std::isfinite(radius) || radius <= 0.0)
    return Result<GeometryRadialMeasurement>::failure(
        measure_error("radius must be finite and positive"));
  return Result<GeometryRadialMeasurement>::success({radius, radius * 2.0});
}

} // namespace blcad::geometry
