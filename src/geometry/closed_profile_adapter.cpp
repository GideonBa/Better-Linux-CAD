#include "blcad/geometry/closed_profile_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
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
  if (message == nullptr || *message == '\0') return "OCCT operation failed";
  return message;
}

[[nodiscard]] gp_Pnt to_gp_point(Point3 point) noexcept { return gp_Pnt(point.x, point.y, point.z); }

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
  return Result<Vector3>::success(Vector3{direction.x / axis_length, direction.y / axis_length,
                                          direction.z / axis_length});
}

[[nodiscard]] std::vector<Point3> bounding_box_corners(double x_min, double y_min, double z_min,
                                                       double x_max, double y_max, double z_max) {
  return {Point3{x_min, y_min, z_min}, Point3{x_min, y_min, z_max},
          Point3{x_min, y_max, z_min}, Point3{x_min, y_max, z_max},
          Point3{x_max, y_min, z_min}, Point3{x_max, y_min, z_max},
          Point3{x_max, y_max, z_min}, Point3{x_max, y_max, z_max}};
}

struct ThroughAllPlacement {
  Vector3 axis;
  double length = 0.0;
  double start_projection = 0.0;
};

[[nodiscard]] ThroughAllPlacement make_through_all_placement(Vector3 axis, double x_min, double y_min,
                                                             double z_min, double x_max, double y_max,
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
                                                     std::string_view contour_kind) {
  if (vertices.size() < 3U) {
    return Result<TopoDS_Wire>::failure(make_geometry_error(
        std::string(contour_kind) + " closed profile requires at least three vertices"));
  }

  BRepBuilderAPI_MakePolygon polygon;
  for (const Point3 vertex : vertices) polygon.Add(to_gp_point(vertex));
  polygon.Close();

  if (!polygon.IsDone()) {
    return Result<TopoDS_Wire>::failure(make_geometry_error(
        std::string("could not build ") + std::string(contour_kind) + " closed profile wire"));
  }

  return Result<TopoDS_Wire>::success(polygon.Wire());
}

[[nodiscard]] Result<TopoDS_Shape> make_profile_face(const std::vector<Point3>& vertices) {
  auto outer_wire = make_profile_wire(vertices, "outer");
  if (outer_wire.has_error()) return Result<TopoDS_Shape>::failure(outer_wire.error());

  BRepBuilderAPI_MakeFace face_builder(outer_wire.value());
  if (!face_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(make_geometry_error("could not build closed profile face"));
  }

  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<TopoDS_Shape> make_profile_face_with_holes(
    const std::vector<Point3>& outer_vertices,
    const std::vector<std::vector<Point3>>& inner_vertices) {
  if (inner_vertices.empty()) return make_profile_face(outer_vertices);

  auto outer_wire = make_profile_wire(outer_vertices, "outer");
  if (outer_wire.has_error()) return Result<TopoDS_Shape>::failure(outer_wire.error());

  BRepBuilderAPI_MakeFace face_builder(outer_wire.value());
  for (const auto& inner : inner_vertices) {
    auto inner_wire = make_profile_wire(inner, "inner");
    if (inner_wire.has_error()) return Result<TopoDS_Shape>::failure(inner_wire.error());
    const TopoDS_Wire reversed_inner_wire = TopoDS::Wire(inner_wire.value().Reversed());
    face_builder.Add(reversed_inner_wire);
  }

  if (!face_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("could not build closed profile face with holes"));
  }

  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<std::vector<Point3>> move_profile_to_through_all_start(
    const std::vector<Point3>& vertices, const ThroughAllPlacement& placement) {
  if (vertices.empty()) {
    return Result<std::vector<Point3>>::failure(make_geometry_error("closed profile requires vertices"));
  }
  const double profile_projection = dot(vertices.front(), placement.axis);
  const double delta = placement.start_projection - profile_projection;
  std::vector<Point3> moved;
  moved.reserve(vertices.size());
  for (Point3 vertex : vertices) moved.push_back(translate_along_axis(vertex, placement.axis, delta));
  return Result<std::vector<Point3>>::success(std::move(moved));
}

[[nodiscard]] Result<std::vector<std::vector<Point3>>> move_inner_profiles_to_through_all_start(
    const std::vector<std::vector<Point3>>& contours, const ThroughAllPlacement& placement) {
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

[[nodiscard]] Result<TopoDS_Shape> make_prism_shape_from_face(TopoDS_Shape face,
                                                              Vector3 direction,
                                                              const Quantity& depth) {
  BRepPrimAPI_MakePrism prism_builder(
      face, gp_Vec(direction.x * depth.millimeters(), direction.y * depth.millimeters(),
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

[[nodiscard]] Result<ThroughAllPlacement> make_placement_from_target(const GeometryShape& target,
                                                                     Vector3 axis_direction) {
  auto axis = normalize_axis(axis_direction);
  if (axis.has_error()) return Result<ThroughAllPlacement>::failure(axis.error());

  const TopoDS_Shape& target_shape = target.impl_->shape;
  Bnd_Box bounds;
  BRepBndLib::Add(target_shape, bounds);
  if (bounds.IsVoid()) {
    return Result<ThroughAllPlacement>::failure(make_geometry_error("target shape has no bounding volume"));
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

Result<GeometryShape> ClosedProfileAdapter::make_extruded_profile(
    const std::vector<Point3>& vertices, Vector3 direction, const Quantity& depth) const {
  if (depth.millimeters() <= 0.0) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile extrusion depth must be greater than zero"));
  }

  try {
    auto face = make_profile_face(vertices);
    if (face.has_error()) return Result<GeometryShape>::failure(face.error());
    auto shape = make_prism_shape_from_face(face.value(), direction, depth);
    if (shape.has_error()) return Result<GeometryShape>::failure(shape.error());
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

Result<GeometryShape> ClosedProfileAdapter::make_extruded_profile_with_holes(
    const std::vector<Point3>& outer_vertices,
    const std::vector<std::vector<Point3>>& inner_vertices,
    Vector3 direction, const Quantity& depth) const {
  if (depth.millimeters() <= 0.0) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile extrusion depth must be greater than zero"));
  }
  if (inner_vertices.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("composite closed profile extrusion requires at least one inner contour"));
  }

  try {
    auto face = make_profile_face_with_holes(outer_vertices, inner_vertices);
    if (face.has_error()) return Result<GeometryShape>::failure(face.error());
    auto shape = make_prism_shape_from_face(face.value(), direction, depth);
    if (shape.has_error()) return Result<GeometryShape>::failure(shape.error());
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

Result<GeometryShape> ClosedProfileAdapter::cut_profile_through_all(
    const GeometryShape& target, const std::vector<Point3>& vertices, Vector3 axis_direction) const {
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile cut requires a non-empty target shape"));
  }

  try {
    auto placement = make_placement_from_target(target, axis_direction);
    if (placement.has_error()) return Result<GeometryShape>::failure(placement.error());

    const auto moved_vertices = move_profile_to_through_all_start(vertices, placement.value());
    if (moved_vertices.has_error()) return Result<GeometryShape>::failure(moved_vertices.error());

    auto face = make_profile_face(moved_vertices.value());
    if (face.has_error()) return Result<GeometryShape>::failure(face.error());

    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(placement.value().axis.x * placement.value().length,
                             placement.value().axis.y * placement.value().length,
                             placement.value().axis.z * placement.value().length));
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build closed profile cut prism"));
    }

    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(make_geometry_error("closed profile boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull()) return Result<GeometryShape>::failure(make_geometry_error("closed profile cut produced an empty shape"));

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
    auto placement = make_placement_from_target(target, axis_direction);
    if (placement.has_error()) return Result<GeometryShape>::failure(placement.error());

    auto moved_outer = move_profile_to_through_all_start(outer_vertices, placement.value());
    if (moved_outer.has_error()) return Result<GeometryShape>::failure(moved_outer.error());
    auto moved_inner = move_inner_profiles_to_through_all_start(inner_vertices, placement.value());
    if (moved_inner.has_error()) return Result<GeometryShape>::failure(moved_inner.error());

    auto face = make_profile_face_with_holes(moved_outer.value(), moved_inner.value());
    if (face.has_error()) return Result<GeometryShape>::failure(face.error());

    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(placement.value().axis.x * placement.value().length,
                             placement.value().axis.y * placement.value().length,
                             placement.value().axis.z * placement.value().length));
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build closed profile cut prism"));
    }

    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(make_geometry_error("closed profile boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull()) return Result<GeometryShape>::failure(make_geometry_error("closed profile cut produced an empty shape"));

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
