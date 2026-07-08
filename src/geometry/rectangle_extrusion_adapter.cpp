#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kRectangleExtrusionId = "geometry.rectangle_extrusion";

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kRectangleExtrusionId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }

  return message;
}

} // namespace

GeometryShape::GeometryShape() = default;
GeometryShape::~GeometryShape() = default;

GeometryShape::GeometryShape(const GeometryShape& other) = default;
GeometryShape& GeometryShape::operator=(const GeometryShape& other) = default;
GeometryShape::GeometryShape(GeometryShape&& other) noexcept = default;
GeometryShape& GeometryShape::operator=(GeometryShape&& other) noexcept = default;

bool GeometryShape::empty() const noexcept {
  return impl_ == nullptr || impl_->shape.IsNull();
}

GeometryShape::GeometryShape(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

Result<GeometryShape>
RectangleExtrusionAdapter::make_extruded_rectangle(const Quantity& width, const Quantity& height,
                                                   const Quantity& thickness) const {
  try {
    const double half_width = width.millimeters() / 2.0;
    const double half_height = height.millimeters() / 2.0;

    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(-half_width, -half_height, 0.0));
    polygon.Add(gp_Pnt(half_width, -half_height, 0.0));
    polygon.Add(gp_Pnt(half_width, half_height, 0.0));
    polygon.Add(gp_Pnt(-half_width, half_height, 0.0));
    polygon.Close();

    if (!polygon.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build rectangle wire"));
    }

    BRepBuilderAPI_MakeFace face_builder(polygon.Wire());
    if (!face_builder.IsDone()) {
      return Result<GeometryShape>::failure(make_geometry_error("could not build rectangle face"));
    }

    BRepPrimAPI_MakePrism prism_builder(face_builder.Face(),
                                        gp_Vec(0.0, 0.0, thickness.millimeters()));
    prism_builder.Build();

    if (!prism_builder.IsDone()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("could not extrude rectangle face"));
    }

    TopoDS_Shape shape = prism_builder.Shape();
    if (shape.IsNull()) {
      return Result<GeometryShape>::failure(
          make_geometry_error("extrusion produced an empty shape"));
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

ShapeSummary RectangleExtrusionAdapter::summarize(const GeometryShape& shape) const {
  ShapeSummary summary;

  if (shape.empty()) {
    return summary;
  }

  summary.is_null = shape.impl_->shape.IsNull();

  for (TopExp_Explorer explorer(shape.impl_->shape, TopAbs_SOLID); explorer.More();
       explorer.Next()) {
    ++summary.solid_count;
  }

  if (!summary.is_null) {
    GProp_GProps volume_properties;
    BRepGProp::VolumeProperties(shape.impl_->shape, volume_properties);
    summary.volume_mm3 = volume_properties.Mass();
  }

  return summary;
}

} // namespace blcad::geometry
