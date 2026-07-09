#include "blcad/geometry/closed_profile_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <utility>

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
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }

  return message;
}

[[nodiscard]] gp_Pnt to_gp_point(Point3 point) noexcept {
  return gp_Pnt(point.x, point.y, point.z);
}

[[nodiscard]] Result<TopoDS_Shape> make_profile_face(const std::vector<Point3>& vertices) {
  if (vertices.size() < 3U) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("closed profile requires at least three vertices"));
  }

  BRepBuilderAPI_MakePolygon polygon;
  for (const Point3 vertex : vertices) {
    polygon.Add(to_gp_point(vertex));
  }
  polygon.Close();

  if (!polygon.IsDone()) {
    return Result<TopoDS_Shape>::failure(make_geometry_error("could not build closed profile wire"));
  }

  BRepBuilderAPI_MakeFace face_builder(polygon.Wire());
  if (!face_builder.IsDone()) {
    return Result<TopoDS_Shape>::failure(make_geometry_error("could not build closed profile face"));
  }

  return Result<TopoDS_Shape>::success(face_builder.Face());
}

[[nodiscard]] Result<Vector3> normalize_principal_axis(Vector3 direction) {
  const double abs_x = std::abs(direction.x);
  const double abs_y = std::abs(direction.y);
  const double abs_z = std::abs(direction.z);

  if (abs_x > 1.0 - kAxisTolerance && abs_y < kAxisTolerance && abs_z < kAxisTolerance) {
    return Result<Vector3>::success(Vector3{direction.x >= 0.0 ? 1.0 : -1.0, 0.0, 0.0});
  }

  if (abs_y > 1.0 - kAxisTolerance && abs_x < kAxisTolerance && abs_z < kAxisTolerance) {
    return Result<Vector3>::success(Vector3{0.0, direction.y >= 0.0 ? 1.0 : -1.0, 0.0});
  }

  if (abs_z > 1.0 - kAxisTolerance && abs_x < kAxisTolerance && abs_y < kAxisTolerance) {
    return Result<Vector3>::success(Vector3{0.0, 0.0, direction.z >= 0.0 ? 1.0 : -1.0});
  }

  return Result<Vector3>::failure(
      make_geometry_error("closed profile through-all axis must be one of the principal axes"));
}

[[nodiscard]] double coordinate_along_axis(Point3 point, Vector3 axis) noexcept {
  if (std::abs(axis.x) > 1.0 - kAxisTolerance) {
    return point.x;
  }
  if (std::abs(axis.y) > 1.0 - kAxisTolerance) {
    return point.y;
  }
  return point.z;
}

[[nodiscard]] double axis_component(Vector3 axis) noexcept {
  if (std::abs(axis.x) > 1.0 - kAxisTolerance) {
    return axis.x;
  }
  if (std::abs(axis.y) > 1.0 - kAxisTolerance) {
    return axis.y;
  }
  return axis.z;
}

[[nodiscard]] Point3 translate_along_axis(Point3 point, Vector3 axis, double delta) noexcept {
  return Point3{point.x + axis.x * delta, point.y + axis.y * delta,
                point.z + axis.z * delta};
}

[[nodiscard]] Result<std::vector<Point3>> move_profile_to_through_all_start(
    const std::vector<Point3>& vertices, Vector3 axis, double min_x, double min_y, double min_z,
    double max_x, double max_y, double max_z) {
  if (vertices.empty()) {
    return Result<std::vector<Point3>>::failure(
        make_geometry_error("closed profile requires vertices"));
  }

  const double profile_coordinate = coordinate_along_axis(vertices.front(), axis);
  double start_coordinate = 0.0;

  if (std::abs(axis.x) > 1.0 - kAxisTolerance) {
    start_coordinate = axis.x > 0.0 ? min_x - kThroughAllMargin : max_x + kThroughAllMargin;
  } else if (std::abs(axis.y) > 1.0 - kAxisTolerance) {
    start_coordinate = axis.y > 0.0 ? min_y - kThroughAllMargin : max_y + kThroughAllMargin;
  } else {
    start_coordinate = axis.z > 0.0 ? min_z - kThroughAllMargin : max_z + kThroughAllMargin;
  }

  const double delta = (start_coordinate - profile_coordinate) / axis_component(axis);

  std::vector<Point3> moved;
  moved.reserve(vertices.size());
  for (Point3 vertex : vertices) {
    moved.push_back(translate_along_axis(vertex, axis, delta));
  }

  return Result<std::vector<Point3>>::success(std::move(moved));
}

[[nodiscard]] double through_all_length(Vector3 axis, double min_x, double min_y, double min_z,
                                        double max_x, double max_y, double max_z) noexcept {
  if (std::abs(axis.x) > 1.0 - kAxisTolerance) {
    return (max_x - min_x) + 2.0 * kThroughAllMargin;
  }
  if (std::abs(axis.y) > 1.0 - kAxisTolerance) {
    return (max_y - min_y) + 2.0 * kThroughAllMargin;
  }
  return (max_z - min_z) + 2.0 * kThroughAllMargin;
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
    if (face.has_error()) {
      return Result<GeometryShape>::failure(face.error());
    }

    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(direction.x * depth.millimeters(), direction.y * depth.millimeters(),
                             direction.z * depth.millimeters()));
    prism_builder.Build();

    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not extrude closed profile"));
    }

    TopoDS_Shape shape = prism_builder.Shape();
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("closed profile extrusion produced an empty shape"));
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

Result<GeometryShape> ClosedProfileAdapter::cut_profile_through_all(
    const GeometryShape& target, const std::vector<Point3>& vertices, Vector3 axis_direction) const {
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("closed profile cut requires a non-empty target shape"));
  }

  try {
    const auto axis = normalize_principal_axis(axis_direction);
    if (axis.has_error()) {
      return Result<GeometryShape>::failure(axis.error());
    }

    const TopoDS_Shape& target_shape = target.impl_->shape;

    Bnd_Box bounds;
    BRepBndLib::Add(target_shape, bounds);
    if (bounds.IsVoid()) {
      return Result<GeometryShape>::failure(make_geometry_error("target shape has no bounding volume"));
    }

    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);

    const auto moved_vertices = move_profile_to_through_all_start(
        vertices, axis.value(), x_min, y_min, z_min, x_max, y_max, z_max);
    if (moved_vertices.has_error()) {
      return Result<GeometryShape>::failure(moved_vertices.error());
    }

    auto face = make_profile_face(moved_vertices.value());
    if (face.has_error()) {
      return Result<GeometryShape>::failure(face.error());
    }

    const double length = through_all_length(axis.value(), x_min, y_min, z_min, x_max, y_max, z_max);
    BRepPrimAPI_MakePrism prism_builder(
        face.value(), gp_Vec(axis.value().x * length, axis.value().y * length,
                             axis.value().z * length));
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build closed profile cut prism"));
    }

    BRepAlgoAPI_Cut cut_builder(target_shape, prism_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(make_geometry_error("closed profile boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(make_geometry_error("closed profile cut produced an empty shape"));
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
