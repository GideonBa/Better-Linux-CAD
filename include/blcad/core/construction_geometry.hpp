#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <string>
#include <vector>

namespace blcad {

class ConstructionPoint {
public:
  [[nodiscard]] static Result<ConstructionPoint>
  create_explicit(ConstructionPointId id, std::string name, Point3 position,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] const ConstructionPointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] Point3 position() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;

private:
  ConstructionPoint(ConstructionPointId id, std::string name, Point3 position,
                    std::vector<ParameterId> parameter_dependencies);

  ConstructionPointId id_;
  std::string name_;
  Point3 position_;
  std::vector<ParameterId> parameter_dependencies_;
};

class ConstructionLine {
public:
  [[nodiscard]] static Result<ConstructionLine>
  create_explicit(ConstructionLineId id, std::string name, Point3 point, Vector3 direction,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] const ConstructionLineId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] Point3 point() const noexcept;
  [[nodiscard]] Vector3 direction() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;

private:
  ConstructionLine(ConstructionLineId id, std::string name, Point3 point, Vector3 direction,
                   std::vector<ParameterId> parameter_dependencies);

  ConstructionLineId id_;
  std::string name_;
  Point3 point_;
  Vector3 direction_;
  std::vector<ParameterId> parameter_dependencies_;
};

class ConstructionPlane {
public:
  [[nodiscard]] static Result<ConstructionPlane>
  create_explicit(ConstructionPlaneId id, std::string name, Point3 origin, Vector3 x_axis,
                  Vector3 y_axis, Vector3 normal,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] const ConstructionPlaneId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] Point3 origin() const noexcept;
  [[nodiscard]] Vector3 x_axis() const noexcept;
  [[nodiscard]] Vector3 y_axis() const noexcept;
  [[nodiscard]] Vector3 normal() const noexcept;
  [[nodiscard]] DatumPlaneId workplane_id() const;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;

private:
  ConstructionPlane(ConstructionPlaneId id, std::string name, Point3 origin, Vector3 x_axis,
                    Vector3 y_axis, Vector3 normal,
                    std::vector<ParameterId> parameter_dependencies);

  ConstructionPlaneId id_;
  std::string name_;
  Point3 origin_;
  Vector3 x_axis_;
  Vector3 y_axis_;
  Vector3 normal_;
  std::vector<ParameterId> parameter_dependencies_;
};

} // namespace blcad
