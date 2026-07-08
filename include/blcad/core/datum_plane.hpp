#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <string>

namespace blcad {

class DatumPlane {
public:
  [[nodiscard]] static Result<DatumPlane> xy(DatumPlaneId id = DatumPlaneId("datum.xy"),
                                             std::string name = "XY");

  [[nodiscard]] const DatumPlaneId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] Point3 origin() const noexcept;
  [[nodiscard]] Vector3 x_axis() const noexcept;
  [[nodiscard]] Vector3 y_axis() const noexcept;
  [[nodiscard]] Vector3 normal() const noexcept;

private:
  DatumPlane(DatumPlaneId id, std::string name, Point3 origin, Vector3 x_axis, Vector3 y_axis,
             Vector3 normal);

  DatumPlaneId id_;
  std::string name_;
  Point3 origin_;
  Vector3 x_axis_;
  Vector3 y_axis_;
  Vector3 normal_;
};

} // namespace blcad
