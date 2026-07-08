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

namespace blcad::geometry {
namespace {

constexpr const char* kCircularCutId = "geometry.circular_cut";

// Ueberstand der Schneidgeometrie ueber die Zielgrenzen hinaus. Er stellt einen
// sauberen `through_all`-Cut sicher, auch bei Rundungsfehlern an den Deckflaechen.
constexpr double kThroughAllMargin = 1.0;
constexpr double kAxisTolerance = 1.0e-9;

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kCircularCutId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }

  return message;
}

struct CutAxisPlacement {
  gp_Ax2 axis;
  double height = 0.0;
};

[[nodiscard]] Result<CutAxisPlacement> make_cut_axis(Point3 center, Vector3 direction,
                                                     double x_min, double y_min, double z_min,
                                                     double x_max, double y_max, double z_max) {
  const double abs_x = std::abs(direction.x);
  const double abs_y = std::abs(direction.y);
  const double abs_z = std::abs(direction.z);

  if (abs_x > 1.0 - kAxisTolerance && abs_y < kAxisTolerance && abs_z < kAxisTolerance) {
    const double sign = direction.x >= 0.0 ? 1.0 : -1.0;
    const double base_x = sign > 0.0 ? x_min - kThroughAllMargin : x_max + kThroughAllMargin;
    return Result<CutAxisPlacement>::success(CutAxisPlacement{
        gp_Ax2(gp_Pnt(base_x, center.y, center.z), gp_Dir(sign, 0.0, 0.0)),
        (x_max - x_min) + 2.0 * kThroughAllMargin});
  }

  if (abs_y > 1.0 - kAxisTolerance && abs_x < kAxisTolerance && abs_z < kAxisTolerance) {
    const double sign = direction.y >= 0.0 ? 1.0 : -1.0;
    const double base_y = sign > 0.0 ? y_min - kThroughAllMargin : y_max + kThroughAllMargin;
    return Result<CutAxisPlacement>::success(CutAxisPlacement{
        gp_Ax2(gp_Pnt(center.x, base_y, center.z), gp_Dir(0.0, sign, 0.0)),
        (y_max - y_min) + 2.0 * kThroughAllMargin});
  }

  if (abs_z > 1.0 - kAxisTolerance && abs_x < kAxisTolerance && abs_y < kAxisTolerance) {
    const double sign = direction.z >= 0.0 ? 1.0 : -1.0;
    const double base_z = sign > 0.0 ? z_min - kThroughAllMargin : z_max + kThroughAllMargin;
    return Result<CutAxisPlacement>::success(CutAxisPlacement{
        gp_Ax2(gp_Pnt(center.x, center.y, base_z), gp_Dir(0.0, 0.0, sign)),
        (z_max - z_min) + 2.0 * kThroughAllMargin});
  }

  return Result<CutAxisPlacement>::failure(
      make_geometry_error("circular cut axis must be one of the principal axes"));
}

} // namespace

Result<GeometryShape> CircularCutAdapter::cut_circular_hole(const GeometryShape& target,
                                                            const Quantity& diameter,
                                                            Point2 center) const {
  return cut_circular_hole_along_axis(target, diameter, Point3{center.x, center.y, 0.0},
                                      Vector3{0.0, 0.0, 1.0});
}

Result<GeometryShape> CircularCutAdapter::cut_circular_hole_along_axis(
    const GeometryShape& target, const Quantity& diameter, Point3 center,
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

    const auto cut_axis = make_cut_axis(center, axis_direction, x_min, y_min, z_min, x_max, y_max,
                                        z_max);
    if (cut_axis.has_error()) {
      return Result<GeometryShape>::failure(cut_axis.error());
    }

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
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(make_geometry_error("cut produced an empty shape"));
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
