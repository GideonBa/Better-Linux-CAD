#include "blcad/core/datum_plane.hpp"

#include <utility>

namespace blcad {

Result<DatumPlane> DatumPlane::xy(DatumPlaneId id, std::string name) {
  const auto object_id = id.empty() ? std::string("datum_plane") : id.value();

  if (id.empty()) {
    return Result<DatumPlane>::failure(
        Error::validation(object_id, "datum plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<DatumPlane>::failure(
        Error::validation(object_id, "datum plane name must not be empty"));
  }

  return Result<DatumPlane>::success(DatumPlane(std::move(id), std::move(name),
                                                Point3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                                                Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}));
}

const DatumPlaneId& DatumPlane::id() const noexcept {
  return id_;
}

const std::string& DatumPlane::name() const noexcept {
  return name_;
}

Point3 DatumPlane::origin() const noexcept {
  return origin_;
}

Vector3 DatumPlane::x_axis() const noexcept {
  return x_axis_;
}

Vector3 DatumPlane::y_axis() const noexcept {
  return y_axis_;
}

Vector3 DatumPlane::normal() const noexcept {
  return normal_;
}

DatumPlane::DatumPlane(DatumPlaneId id, std::string name, Point3 origin, Vector3 x_axis,
                       Vector3 y_axis, Vector3 normal)
    : id_(std::move(id)), name_(std::move(name)), origin_(origin), x_axis_(x_axis), y_axis_(y_axis),
      normal_(normal) {}

} // namespace blcad
