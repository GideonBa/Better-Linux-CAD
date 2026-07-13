#include "blcad/geometry/body_transform_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <memory>
#include <string>

namespace blcad::geometry {
namespace {

constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error transform_error(std::string message) {
  return Error::geometry("body_transform", std::move(message));
}

[[nodiscard]] TopoDS_Shape apply_occt(const TopoDS_Shape& shape, const gp_Trsf& transform) {
  BRepBuilderAPI_Transform operation(shape, transform, true);
  return operation.IsDone() ? operation.Shape() : TopoDS_Shape{};
}

} // namespace

GeometryAffineTransform GeometryAffineTransform::identity() noexcept {
  return {};
}

GeometryAffineTransform GeometryAffineTransform::translation(Vector3 offset) noexcept {
  GeometryAffineTransform result;
  result.matrix[3] = offset.x;
  result.matrix[7] = offset.y;
  result.matrix[11] = offset.z;
  return result;
}

GeometryAffineTransform GeometryAffineTransform::rotation(Point3 origin, Vector3 axis,
                                                          double angle_deg) noexcept {
  const double length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
  const double x = axis.x / length;
  const double y = axis.y / length;
  const double z = axis.z / length;
  const double angle = angle_deg * kPi / 180.0;
  const double c = std::cos(angle);
  const double s = std::sin(angle);
  const double t = 1.0 - c;
  GeometryAffineTransform result;
  result.matrix = {t * x * x + c,
                   t * x * y - s * z,
                   t * x * z + s * y,
                   0.0,
                   t * x * y + s * z,
                   t * y * y + c,
                   t * y * z - s * x,
                   0.0,
                   t * x * z - s * y,
                   t * y * z + s * x,
                   t * z * z + c,
                   0.0,
                   0.0,
                   0.0,
                   0.0,
                   1.0};
  const Point3 rotated_origin = result.transform_point(origin);
  result.matrix[3] = origin.x - rotated_origin.x;
  result.matrix[7] = origin.y - rotated_origin.y;
  result.matrix[11] = origin.z - rotated_origin.z;
  return result;
}

GeometryAffineTransform GeometryAffineTransform::uniform_scale(Point3 center,
                                                               double factor) noexcept {
  GeometryAffineTransform result;
  result.matrix = {
      factor, 0.0, 0.0,    center.x * (1.0 - factor), 0.0, factor, 0.0, center.y * (1.0 - factor),
      0.0,    0.0, factor, center.z * (1.0 - factor), 0.0, 0.0,    0.0, 1.0};
  return result;
}

Point3 GeometryAffineTransform::transform_point(Point3 point) const noexcept {
  return {matrix[0] * point.x + matrix[1] * point.y + matrix[2] * point.z + matrix[3],
          matrix[4] * point.x + matrix[5] * point.y + matrix[6] * point.z + matrix[7],
          matrix[8] * point.x + matrix[9] * point.y + matrix[10] * point.z + matrix[11]};
}

Vector3 GeometryAffineTransform::transform_vector(Vector3 vector) const noexcept {
  return {matrix[0] * vector.x + matrix[1] * vector.y + matrix[2] * vector.z,
          matrix[4] * vector.x + matrix[5] * vector.y + matrix[6] * vector.z,
          matrix[8] * vector.x + matrix[9] * vector.y + matrix[10] * vector.z};
}

GeometryAffineTransform
GeometryAffineTransform::followed_by(const GeometryAffineTransform& operation) const noexcept {
  GeometryAffineTransform result;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      result.matrix[row * 4 + column] = 0.0;
      for (int inner = 0; inner < 4; ++inner)
        result.matrix[row * 4 + column] +=
            operation.matrix[row * 4 + inner] * matrix[inner * 4 + column];
    }
  }
  return result;
}

Result<GeometryShape> BodyTransformAdapter::translate(const GeometryShape& shape,
                                                      Vector3 offset) const {
  if (shape.empty())
    return Result<GeometryShape>::failure(transform_error("input shape must not be empty"));
  gp_Trsf transform;
  transform.SetTranslation(gp_Vec(offset.x, offset.y, offset.z));
  TopoDS_Shape result = apply_occt(shape.impl_->shape, transform);
  if (result.IsNull())
    return Result<GeometryShape>::failure(
        transform_error("OCCT transform did not produce a shape"));
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(result)));
}

Result<GeometryShape> BodyTransformAdapter::rotate(const GeometryShape& shape, Point3 origin,
                                                   Vector3 axis, double angle_deg) const {
  if (shape.empty())
    return Result<GeometryShape>::failure(transform_error("input shape must not be empty"));
  const double length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
  if (length <= 1.0e-12)
    return Result<GeometryShape>::failure(transform_error("rotation axis must not be zero length"));
  gp_Trsf transform;
  transform.SetRotation(
      gp_Ax1(gp_Pnt(origin.x, origin.y, origin.z), gp_Dir(axis.x, axis.y, axis.z)),
      angle_deg * kPi / 180.0);
  TopoDS_Shape result = apply_occt(shape.impl_->shape, transform);
  if (result.IsNull())
    return Result<GeometryShape>::failure(
        transform_error("OCCT transform did not produce a shape"));
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(result)));
}

Result<GeometryShape> BodyTransformAdapter::uniform_scale(const GeometryShape& shape, Point3 center,
                                                          double factor) const {
  if (shape.empty())
    return Result<GeometryShape>::failure(transform_error("input shape must not be empty"));
  if (!(factor > 0.0) || !std::isfinite(factor))
    return Result<GeometryShape>::failure(transform_error("uniform scale factor must be positive"));
  gp_Trsf transform;
  transform.SetScale(gp_Pnt(center.x, center.y, center.z), factor);
  TopoDS_Shape result = apply_occt(shape.impl_->shape, transform);
  if (result.IsNull())
    return Result<GeometryShape>::failure(
        transform_error("OCCT transform did not produce a shape"));
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(result)));
}

Result<GeometryShapeBounds> BodyTransformAdapter::bounds(const GeometryShape& shape) const {
  if (shape.empty())
    return Result<GeometryShapeBounds>::failure(transform_error("input shape must not be empty"));
  Bnd_Box box;
  BRepBndLib::Add(shape.impl_->shape, box);
  if (box.IsVoid())
    return Result<GeometryShapeBounds>::failure(transform_error("shape bounds are void"));
  Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
  box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  return Result<GeometryShapeBounds>::success(
      GeometryShapeBounds{{xmin, ymin, zmin}, {xmax, ymax, zmax}});
}

} // namespace blcad::geometry
