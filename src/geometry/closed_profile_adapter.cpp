#include "blcad/geometry/closed_profile_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kClosedProfileId = "geometry.closed_profile";
constexpr double kThroughAllMargin = 1.0;
constexpr double kAxisTolerance = 1.0e-9;

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kClosedProfileId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0')
    return "OCCT operation failed";
  return message;
}

[[nodiscard]] gp_Pnt to_gp_point(Point3 point) noexcept {
  return gp_Pnt(point.x, point.y, point.z);
}

[[nodiscard]] double dot(Point3 point, Vector3 axis) noexcept {
  return point.x * axis.x + point.y * axis.y + point.z * axis.z;
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

[[nodiscard]] Result<Vector3> normalize_axis(Vector3 direction) {
  const double axis_length = length(direction);
  if (axis_length <= kAxisTolerance) {
    return Result<Vector3>::failure(
        make_geometry_error("closed profile through-all axis must not be zero length"));
  }
  return Result<Vector3>::success(
      Vector3{direction.x / axis_length, direction.y / axis_length, direction.z / axis_length});
}

[[nodiscard]] std::vector<Point3> bounding_box_corners(double x_min, double y_min, double z_min,
                                                       double x_max, double y_max, double z_max) {
  return {Point3{x_min, y_min, z_min}, Point3{x_min, y_min, z_max}, Point3{x_min, y_max, z_min},
          Point3{x_min, y_max, z_max}, Point3{x_max, y_min, z_min}, Point3{x_max, y_min, z_max},
          Point3{x_max, y_max, z_min}, Point3{x_max, y_max, z_max}};
}

struct ThroughAllPlacement {
  Vector3 axis;
  double length = 0.0;
  double start_projection = 0.0;
};

[[nodiscard]] ThroughAllPlacement make_through_all_placement(Vector3 axis, double x_min,
                                                             double y_min, double z_min,
                                                             double x_max, double y_max,
                                                             double z_max) {
  double min_projection = 0.0;
  double max_projection = 0.0;
  bool first = true;
  for (const Point3 corner : bounding_box_corners(x_min, y_min, z_min, x_max, y_max, z_max)) {
    const double projection = dot(corner, axis);
    if (first) {
      min_projection = projection;
      max_projection = projection;
      first = false;
    } else {
      min_projection = std::min(min_projection, projection);
      max_projection = std::max(max_projection, projection);
    }
  }
  return ThroughAllPlacement{axis, (max_projection - min_projection) + 2.0 * kThroughAllMargin,
                             min_projection - kThroughAllMargin};
}

[[nodiscard]] Point3 translate_along_axis(Point3 point, Vector3 axis, double delta) noexcept {
  return Point3{point.x + axis.x * delta, point.y + axis.y * delta, point.z + axis.z * delta};
}

[[nodiscard]] Result<TopoDS_Wire> make_profile_wire(const std::vector<Point3>& vertices,
                                                    std::string_view contour_kind);

[[nodiscard]] std::vector<Point3> translated_vertices(const std::vector<Point3>& vertices,
                                                      Vector3 axis, double distance) {
  std::vector<Point3> result;
  result.reserve(vertices.size());
  for (const Point3 vertex : vertices)
    result.push_back(translate_along_axis(vertex, axis, distance));
  return result;
}

[[nodiscard]] Result<TopoDS_Shape> make_tapered_shape(const std::vector<Point3>& base_vertices,
                                                      Vector3 axis, double depth,
                                                      double taper_angle_deg) {
  const Point3 origin = base_vertices.front();
  Vector3 u;
  bool found_basis = false;
  for (std::size_t index = 1U; index < base_vertices.size(); ++index) {
    const Vector3 edge{base_vertices[index].x - origin.x, base_vertices[index].y - origin.y,
                       base_vertices[index].z - origin.z};
    const double edge_length = length(edge);
    if (edge_length > kAxisTolerance) {
      u = {edge.x / edge_length, edge.y / edge_length, edge.z / edge_length};
      found_basis = true;
      break;
    }
  }
  if (!found_basis)
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("tapered profile requires a non-degenerate radial extent"));
  const Vector3 v{axis.y * u.z - axis.z * u.y, axis.z * u.x - axis.x * u.z,
                  axis.x * u.y - axis.y * u.x};
  std::vector<Point2> polygon;
  polygon.reserve(base_vertices.size());
  for (const Point3 vertex : base_vertices) {
    const Vector3 relative{vertex.x - origin.x, vertex.y - origin.y, vertex.z - origin.z};
    polygon.push_back({relative.x * u.x + relative.y * u.y + relative.z * u.z,
                       relative.x * v.x + relative.y * v.y + relative.z * v.z});
  }
  double signed_area = 0.0;
  for (std::size_t index = 0U; index < polygon.size(); ++index) {
    const Point2 current = polygon[index];
    const Point2 next = polygon[(index + 1U) % polygon.size()];
    signed_area += current.x * next.y - next.x * current.y;
  }
  if (std::abs(signed_area) <= kAxisTolerance)
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("tapered profile requires a nonzero planar area"));
  constexpr double kPi = 3.14159265358979323846;
  const double offset = std::tan(taper_angle_deg * kPi / 180.0) * depth;
  if (!std::isfinite(offset))
    return Result<TopoDS_Shape>::failure(make_geometry_error("taper offset must be finite"));

  const double orientation = signed_area > 0.0 ? 1.0 : -1.0;
  std::vector<Point2> offset_polygon;
  offset_polygon.reserve(polygon.size());
  for (std::size_t index = 0U; index < polygon.size(); ++index) {
    const Point2 previous = polygon[(index + polygon.size() - 1U) % polygon.size()];
    const Point2 current = polygon[index];
    const Point2 next = polygon[(index + 1U) % polygon.size()];
    const Point2 previous_direction{current.x - previous.x, current.y - previous.y};
    const Point2 current_direction{next.x - current.x, next.y - current.y};
    const double previous_length = std::sqrt(previous_direction.x * previous_direction.x +
                                             previous_direction.y * previous_direction.y);
    const double current_length = std::sqrt(current_direction.x * current_direction.x +
                                            current_direction.y * current_direction.y);
    if (previous_length <= kAxisTolerance || current_length <= kAxisTolerance)
      return Result<TopoDS_Shape>::failure(
          make_geometry_error("tapered profile contains a degenerate edge"));
    const Point2 previous_normal{orientation * previous_direction.y / previous_length,
                                 -orientation * previous_direction.x / previous_length};
    const Point2 current_normal{orientation * current_direction.y / current_length,
                                -orientation * current_direction.x / current_length};
    const Point2 first{previous.x + previous_normal.x * offset,
                       previous.y + previous_normal.y * offset};
    const Point2 second{current.x + current_normal.x * offset,
                        current.y + current_normal.y * offset};
    const double denominator =
        previous_direction.x * current_direction.y - previous_direction.y * current_direction.x;
    if (std::abs(denominator) <= kAxisTolerance)
      return Result<TopoDS_Shape>::failure(
          make_geometry_error("tapered profile adjacent edges must not be collinear"));
    const double t =
        ((second.x - first.x) * current_direction.y - (second.y - first.y) * current_direction.x) /
        denominator;
    offset_polygon.push_back(
        {first.x + previous_direction.x * t, first.y + previous_direction.y * t});
  }

  std::vector<Point3> top_vertices;
  top_vertices.reserve(offset_polygon.size());
  for (const Point2 vertex : offset_polygon) {
    top_vertices.push_back(Point3{origin.x + u.x * vertex.x + v.x * vertex.y + axis.x * depth,
                                  origin.y + u.y * vertex.x + v.y * vertex.y + axis.y * depth,
                                  origin.z + u.z * vertex.x + v.z * vertex.y + axis.z * depth});
  }
  auto base_wire = make_profile_wire(base_vertices, "base tapered");
  auto top_wire = make_profile_wire(top_vertices, "end tapered");
  if (base_wire.has_error())
    return Result<TopoDS_Shape>::failure(base_wire.error());
  if (top_wire.has_error())
    return Result<TopoDS_Shape>::failure(top_wire.error());
  BRepOffsetAPI_ThruSections loft(true, false);
  loft.AddWire(base_wire.value());
  loft.AddWire(top_wire.value());
  loft.Build();
  if (!loft.IsDone() || loft.Shape().IsNull())
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("could not build tapered profile extrusion"));
  return Result<TopoDS_Shape>::success(loft.Shape());
}

[[nodiscard]] Result<TopoDS_Wire> make_profile_wire(const std::vector<Point3>& vertices,
                                                    std::string_view contour_kind) {
  if (vertices.size() < 3U) {
    return Result<TopoDS_Wire>::failure(make_geometry_error(
        std::string(contour_kind) + " closed profile requires at least three vertices"));
  }

  BRepBuilderAPI_MakePolygon polygon;
  for (const Point3 vertex : vertices)
    polygon.Add(to_gp_point(vertex));
  polygon.Close();

  if (!polygon.IsDone()) {
    return Result<TopoDS_Wire>::failure(make_geometry_error(
        std::string("could not build ") + std::string(contour_kind) + " closed profile wire"));
  }

  return Result<TopoDS_Wire>::success(polygon.Wire());
}

[[nodiscard]] Result<TopoDS_Shape> make_profile_face(const std::vector<Point3>& vertices) {
  auto outer_wire = make_profile_wire(vertices, "outer");
  if (outer_wire.has_error())
    return Result<TopoDS_Shape>::failure(outer_wire.error());

  BRepBuilderAPI_MakeFace face_builder(outer_wire.value());
  if (!face_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("could not build closed profile face"));
  }

  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<TopoDS_Shape>
make_profile_face_with_holes(const std::vector<Point3>& outer_vertices,
                             const std::vector<std::vector<Point3>>& inner_vertices) {
  if (inner_vertices.empty())
    return make_profile_face(outer_vertices);

  auto outer_wire = make_profile_wire(outer_vertices, "outer");
  if (outer_wire.has_error())
    return Result<TopoDS_Shape>::failure(outer_wire.error());

  BRepBuilderAPI_MakeFace face_builder(outer_wire.value());
  for (const auto& inner : inner_vertices) {
    auto inner_wire = make_profile_wire(inner, "inner");
    if (inner_wire.has_error())
      return Result<TopoDS_Shape>::failure(inner_wire.error());
    const TopoDS_Wire reversed_inner_wire = TopoDS::Wire(inner_wire.value().Reversed());
    face_builder.Add(reversed_inner_wire);
  }

  if (!face_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("could not build closed profile face with holes"));
  }

  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<std::vector<Point3>>
move_profile_to_through_all_start(const std::vector<Point3>& vertices,
                                  const ThroughAllPlacement& placement) {
  if (vertices.empty()) {
    return Result<std::vector<Point3>>::failure(
        make_geometry_error("closed profile requires vertices"));
  }
  const double profile_projection = dot(vertices.front(), placement.axis);
  const double delta = placement.start_projection - profile_projection;
  std::vector<Point3> moved;
  moved.reserve(vertices.size());
  for (Point3 vertex : vertices)
    moved.push_back(translate_along_axis(vertex, placement.axis, delta));
  return Result<std::vector<Point3>>::success(std::move(moved));
}

[[nodiscard]] Result<std::vector<std::vector<Point3>>>
move_inner_profiles_to_through_all_start(const std::vector<std::vector<Point3>>& contours,
                                         const ThroughAllPlacement& placement) {
  std::vector<std::vector<Point3>> moved;
  moved.reserve(contours.size());
  for (const auto& contour : contours) {
    auto moved_contour = move_profile_to_through_all_start(contour, placement);
    if (moved_contour.has_error()) {
      return Result<std::vector<std::vector<Point3>>>::failure(moved_contour.error());
    }
    moved.push_back(std::move(moved_contour.value()));
  }
  return Result<std::vector<std::vector<Point3>>>::success(std::move(moved));
}

[[nodiscard]] Result<TopoDS_Shape> make_prism_shape_from_face(TopoDS_Shape face, Vector3 direction,
                                                              const Quantity& depth) {
  BRepPrimAPI_MakePrism prism_builder(face, gp_Vec(direction.x * depth.millimeters(),
                                                   direction.y * depth.millimeters(),
                                                   direction.z * depth.millimeters()));
  prism_builder.Build();

  if (!prism_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(make_geometry_error("could not extrude closed profile"));
  }

  TopoDS_Shape shape = prism_builder.Shape();
  if (shape.IsNull()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("closed profile extrusion produced an empty shape"));
  }

  return Result<TopoDS_Shape>::success(std::move(shape));
}

[[nodiscard]] Result<ThroughAllPlacement>
make_placement_from_target_shape(const TopoDS_Shape& target_shape, Vector3 axis_direction) {
  auto axis = normalize_axis(axis_direction);
  if (axis.has_error())
    return Result<ThroughAllPlacement>::failure(axis.error());

  Bnd_Box bounds;
  BRepBndLib::Add(target_shape, bounds);
  if (bounds.IsVoid()) {
    return Result<ThroughAllPlacement>::failure(
        make_geometry_error("target shape has no bounding volume"));
  }

  double x_min = 0.0;
  double y_min = 0.0;
  double z_min = 0.0;
  double x_max = 0.0;
  double y_max = 0.0;
  double z_max = 0.0;
  bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
  return Result<ThroughAllPlacement>::success(
      make_through_all_placement(axis.value(), x_min, y_min, z_min, x_max, y_max, z_max));
}

} // namespace

Result<GeometryShape>
ClosedProfileAdapter::make_extruded_profile(const std::vector<Point3>& vertices, Vector3 direction,
                                            const Quantity& depth) const {
  if (depth.millimeters() <= 0.0) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile extrusion depth must be greater than zero"));
  }

  try {
    auto face = make_profile_face(vertices);
    if (face.has_error())
      return Result<GeometryShape>::failure(face.error());
    auto shape = make_prism_shape_from_face(face.value(), direction, depth);
    if (shape.has_error())
      return Result<GeometryShape>::failure(shape.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error"));
  }
}

Result<GeometryShape> ClosedProfileAdapter::make_extruded_profile_span(
    const std::vector<Point3>& vertices, Vector3 direction, double start_distance_mm,
    double end_distance_mm, std::optional<double> taper_angle_deg) const {
  if (vertices.size() < 3U)
    return Result<GeometryShape>::failure(
        make_geometry_error("finite profile extrusion requires at least three vertices"));
  auto axis = normalize_axis(direction);
  if (axis.has_error())
    return Result<GeometryShape>::failure(axis.error());
  const double depth = end_distance_mm - start_distance_mm;
  if (!std::isfinite(start_distance_mm) || !std::isfinite(end_distance_mm) ||
      depth <= kAxisTolerance)
    return Result<GeometryShape>::failure(
        make_geometry_error("finite profile extrusion requires an increasing nonzero span"));
  try {
    const auto base = translated_vertices(vertices, axis.value(), start_distance_mm);
    Result<TopoDS_Shape> shape = Result<TopoDS_Shape>::failure(
        make_geometry_error("could not build finite profile extrusion"));
    if (taper_angle_deg.has_value() && std::abs(*taper_angle_deg) > kAxisTolerance) {
      shape = make_tapered_shape(base, axis.value(), depth, *taper_angle_deg);
    } else {
      auto face = make_profile_face(base);
      if (face.has_error())
        return Result<GeometryShape>::failure(face.error());
      auto quantity = Quantity::length_mm(depth, "geometry.extrude.span");
      if (quantity.has_error())
        return Result<GeometryShape>::failure(quantity.error());
      shape = make_prism_shape_from_face(face.value(), axis.value(), quantity.value());
    }
    if (shape.has_error())
      return Result<GeometryShape>::failure(shape.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error"));
  }
}

Result<AxisProjectionSpan> ClosedProfileAdapter::projection_span(const GeometryShape& target,
                                                                 Point3 origin,
                                                                 Vector3 axis_direction) const {
  if (target.empty())
    return Result<AxisProjectionSpan>::failure(
        make_geometry_error("projection span requires a non-empty target shape"));
  auto axis = normalize_axis(axis_direction);
  if (axis.has_error())
    return Result<AxisProjectionSpan>::failure(axis.error());
  try {
    Bnd_Box bounds;
    BRepBndLib::Add(target.impl_->shape, bounds);
    if (bounds.IsVoid())
      return Result<AxisProjectionSpan>::failure(
          make_geometry_error("target shape has no bounding volume"));
    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
    AxisProjectionSpan span;
    bool first = true;
    for (const Point3 corner : bounding_box_corners(x_min, y_min, z_min, x_max, y_max, z_max)) {
      const double projection = (corner.x - origin.x) * axis.value().x +
                                (corner.y - origin.y) * axis.value().y +
                                (corner.z - origin.z) * axis.value().z;
      if (first) {
        span.minimum = projection;
        span.maximum = projection;
        first = false;
      } else {
        span.minimum = std::min(span.minimum, projection);
        span.maximum = std::max(span.maximum, projection);
      }
    }
    return Result<AxisProjectionSpan>::success(span);
  } catch (const Standard_Failure& failure) {
    return Result<AxisProjectionSpan>::failure(
        make_geometry_error(standard_failure_message(failure)));
  }
}

Result<double> ClosedProfileAdapter::first_intersection_distance(const GeometryShape& target,
                                                                 Point3 origin,
                                                                 Vector3 axis_direction) const {
  if (target.empty())
    return Result<double>::failure(
        make_geometry_error("to-next intersection requires a non-empty target shape"));
  auto axis = normalize_axis(axis_direction);
  if (axis.has_error())
    return Result<double>::failure(axis.error());
  try {
    IntCurvesFace_ShapeIntersector intersector;
    intersector.Load(target.impl_->shape, kAxisTolerance);
    intersector.Perform(
        gp_Lin(to_gp_point(origin), gp_Dir(axis.value().x, axis.value().y, axis.value().z)),
        kAxisTolerance, 1.0e100);
    double nearest = 0.0;
    for (int index = 1; index <= intersector.NbPnt(); ++index) {
      const double distance = intersector.WParameter(index);
      if (distance > kAxisTolerance && (nearest == 0.0 || distance < nearest))
        nearest = distance;
    }
    if (nearest == 0.0)
      return Result<double>::failure(
          make_geometry_error("to-next target has no forward face intersection"));
    return Result<double>::success(nearest);
  } catch (const Standard_Failure& failure) {
    return Result<double>::failure(make_geometry_error(standard_failure_message(failure)));
  }
}

Result<GeometryShape> ClosedProfileAdapter::make_extruded_profile_with_holes(
    const std::vector<Point3>& outer_vertices,
    const std::vector<std::vector<Point3>>& inner_vertices, Vector3 direction,
    const Quantity& depth) const {
  if (depth.millimeters() <= 0.0) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile extrusion depth must be greater than zero"));
  }
  if (inner_vertices.empty()) {
    return Result<GeometryShape>::failure(make_geometry_error(
        "composite closed profile extrusion requires at least one inner contour"));
  }

  try {
    auto face = make_profile_face_with_holes(outer_vertices, inner_vertices);
    if (face.has_error())
      return Result<GeometryShape>::failure(face.error());
    auto shape = make_prism_shape_from_face(face.value(), direction, depth);
    if (shape.has_error())
      return Result<GeometryShape>::failure(shape.error());
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error"));
  }
}

Result<GeometryShape>
ClosedProfileAdapter::cut_profile_through_all(const GeometryShape& target,
                                              const std::vector<Point3>& vertices,
                                              Vector3 axis_direction) const {
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile cut requires a non-empty target shape"));
  }

  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    auto placement = make_placement_from_target_shape(target_shape, axis_direction);
    if (placement.has_error())
      return Result<GeometryShape>::failure(placement.error());

    const auto moved_vertices = move_profile_to_through_all_start(vertices, placement.value());
    if (moved_vertices.has_error())
      return Result<GeometryShape>::failure(moved_vertices.error());

    auto face = make_profile_face(moved_vertices.value());
    if (face.has_error())
      return Result<GeometryShape>::failure(face.error());

    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(placement.value().axis.x * placement.value().length,
                             placement.value().axis.y * placement.value().length,
                             placement.value().axis.z * placement.value().length));
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("could not build closed profile cut prism"));
    }

    BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("closed profile boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("closed profile cut produced an empty shape"));
    }

    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error"));
  }
}

Result<GeometryShape> ClosedProfileAdapter::cut_profile_with_holes_through_all(
    const GeometryShape& target, const std::vector<Point3>& outer_vertices,
    const std::vector<std::vector<Point3>>& inner_vertices, Vector3 axis_direction) const {
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile cut requires a non-empty target shape"));
  }
  if (inner_vertices.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("composite closed profile cut requires at least one inner contour"));
  }

  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    auto placement = make_placement_from_target_shape(target_shape, axis_direction);
    if (placement.has_error())
      return Result<GeometryShape>::failure(placement.error());

    auto moved_outer = move_profile_to_through_all_start(outer_vertices, placement.value());
    if (moved_outer.has_error())
      return Result<GeometryShape>::failure(moved_outer.error());
    auto moved_inner = move_inner_profiles_to_through_all_start(inner_vertices, placement.value());
    if (moved_inner.has_error())
      return Result<GeometryShape>::failure(moved_inner.error());

    auto face = make_profile_face_with_holes(moved_outer.value(), moved_inner.value());
    if (face.has_error())
      return Result<GeometryShape>::failure(face.error());

    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(placement.value().axis.x * placement.value().length,
                             placement.value().axis.y * placement.value().length,
                             placement.value().axis.z * placement.value().length));
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("could not build closed profile cut prism"));
    }

    BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("closed profile boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("closed profile cut produced an empty shape"));
    }

    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape))));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(make_geometry_error("unknown geometry error"));
  }
}

} // namespace blcad::geometry
