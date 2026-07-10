#include "blcad/geometry/circular_cut_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
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

constexpr const char* kCircularCutId = "geometry.circular_cut";
constexpr double kThroughAllMargin = 1.0;
constexpr double kAxisTolerance = 1.0e-9;

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kCircularCutId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0')
    return "OCCT operation failed";
  return message;
}

struct CutAxisPlacement {
  gp_Ax2 axis;
  double height = 0.0;
};

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
        make_geometry_error("circular cut axis must not be zero length"));
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

[[nodiscard]] Result<CutAxisPlacement> make_cut_axis(Point3 center, Vector3 direction, double x_min,
                                                     double y_min, double z_min, double x_max,
                                                     double y_max, double z_max) {
  auto axis = normalize_axis(direction);
  if (axis.has_error())
    return Result<CutAxisPlacement>::failure(axis.error());

  double min_projection = 0.0;
  double max_projection = 0.0;
  bool first = true;
  for (const Point3 corner : bounding_box_corners(x_min, y_min, z_min, x_max, y_max, z_max)) {
    const double projection = dot(corner, axis.value());
    if (first) {
      min_projection = projection;
      max_projection = projection;
      first = false;
    } else {
      min_projection = std::min(min_projection, projection);
      max_projection = std::max(max_projection, projection);
    }
  }

  const double center_projection = dot(center, axis.value());
  const double base_projection = min_projection - kThroughAllMargin;
  const double delta = base_projection - center_projection;
  const Point3 base{center.x + axis.value().x * delta, center.y + axis.value().y * delta,
                    center.z + axis.value().z * delta};

  return Result<CutAxisPlacement>::success(
      CutAxisPlacement{gp_Ax2(gp_Pnt(base.x, base.y, base.z),
                              gp_Dir(axis.value().x, axis.value().y, axis.value().z)),
                       (max_projection - min_projection) + 2.0 * kThroughAllMargin});
}

} // namespace

Result<GeometryShape> CircularCutAdapter::cut_circular_hole(const GeometryShape& target,
                                                            const Quantity& diameter,
                                                            Point2 center) const {
  return cut_circular_hole_along_axis(target, diameter, Point3{center.x, center.y, 0.0},
                                      Vector3{0.0, 0.0, 1.0});
}

Result<GeometryShape>
CircularCutAdapter::cut_circular_hole_along_axis(const GeometryShape& target,
                                                 const Quantity& diameter, Point3 center,
                                                 Vector3 axis_direction) const {
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        make_geometry_error("circular cut requires a non-empty target shape"));
  }

  const double diameter_mm = diameter.millimeters();
  if (diameter_mm <= 0.0) {
    return Result<GeometryShape>::failure(
        make_geometry_error("circular cut diameter must be greater than zero"));
  }

  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;

    Bnd_Box bounds;
    BRepBndLib::Add(target_shape, bounds);
    if (bounds.IsVoid()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("target shape has no bounding volume"));
    }

    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);

    const auto cut_axis =
        make_cut_axis(center, axis_direction, x_min, y_min, z_min, x_max, y_max, z_max);
    if (cut_axis.has_error())
      return Result<GeometryShape>::failure(cut_axis.error());

    const double radius = diameter_mm / 2.0;
    BRepPrimAPI_MakeCylinder cylinder_builder(cut_axis.value().axis, radius,
                                              cut_axis.value().height);
    cylinder_builder.Build();
    if (!cylinder_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build cut cylinder"));
    }

    BRepAlgoAPI_Cut cut_builder(target_shape, cylinder_builder.Shape());
    cut_builder.Build();
    if (!cut_builder.IsDone() || cut_builder.HasErrors()) {
      return Result<GeometryShape>::failure(make_geometry_error("boolean cut failed"));
    }

    TopoDS_Shape shape = cut_builder.Shape();
    if (shape.IsNull())
      return Result<GeometryShape>::failure(make_geometry_error("cut produced an empty shape"));

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
