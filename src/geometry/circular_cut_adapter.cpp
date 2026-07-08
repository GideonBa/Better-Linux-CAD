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

} // namespace

Result<GeometryShape> CircularCutAdapter::cut_circular_hole(const GeometryShape& target,
                                                            const Quantity& diameter,
                                                            Point2 center) const {
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

    const double base_z = z_min - kThroughAllMargin;
    const double height = (z_max - z_min) + 2.0 * kThroughAllMargin;
    const double radius = diameter_mm / 2.0;

    const gp_Ax2 axis(gp_Pnt(center.x, center.y, base_z), gp_Dir(0.0, 0.0, 1.0));

    BRepPrimAPI_MakeCylinder cylinder_builder(axis, radius, height);
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
