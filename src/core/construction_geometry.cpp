#include "blcad/core/construction_geometry.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] double dot(Vector3 left, Vector3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(dot(vector, vector));
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept {
  return Vector3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                 left.x * right.y - left.y * right.x};
}

[[nodiscard]] bool is_unit(Vector3 vector) noexcept {
  return std::abs(length(vector) - 1.0) <= k_tolerance;
}

[[nodiscard]] bool is_zero(Vector3 vector) noexcept {
  return length(vector) <= k_tolerance;
}

[[nodiscard]] bool is_orthogonal(Vector3 left, Vector3 right) noexcept {
  return std::abs(dot(left, right)) <= k_tolerance;
}

[[nodiscard]] bool same_direction(Vector3 left, Vector3 right) noexcept {
  const Vector3 cross_product = cross(left, right);
  return length(cross_product) <= k_tolerance && dot(left, right) > 0.0;
}

[[nodiscard]] Result<std::size_t> validate_parameter_dependencies(
    const std::string& object_id, const std::vector<ParameterId>& parameter_dependencies) {
  for (const auto& parameter_id : parameter_dependencies) {
    if (parameter_id.empty()) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "construction geometry parameter dependencies must not be empty"));
    }
  }

  for (std::size_t i = 0; i < parameter_dependencies.size(); ++i) {
    for (std::size_t j = i + 1U; j < parameter_dependencies.size(); ++j) {
      if (parameter_dependencies[i] == parameter_dependencies[j]) {
        return Result<std::size_t>::failure(Error::validation(
            object_id, "construction geometry parameter dependencies must be unique"));
      }
    }
  }

  return Result<std::size_t>::success(parameter_dependencies.size());
}

} // namespace

Result<ConstructionPoint> ConstructionPoint::create_explicit(
    ConstructionPointId id, std::string name, Point3 position,
    std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_point") : id.value();

  if (id.empty()) {
    return Result<ConstructionPoint>::failure(
        Error::validation(object_id, "construction point id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPoint>::failure(
        Error::validation(object_id, "construction point name must not be empty"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionPoint>::failure(dependencies.error());
  }

  return Result<ConstructionPoint>::success(ConstructionPoint(
      std::move(id), std::move(name), position, std::move(parameter_dependencies)));
}

const ConstructionPointId& ConstructionPoint::id() const noexcept {
  return id_;
}

const std::string& ConstructionPoint::name() const noexcept {
  return name_;
}

Point3 ConstructionPoint::position() const noexcept {
  return position_;
}

const std::vector<ParameterId>& ConstructionPoint::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

ConstructionPoint::ConstructionPoint(ConstructionPointId id, std::string name, Point3 position,
                                     std::vector<ParameterId> parameter_dependencies)
    : id_(std::move(id)), name_(std::move(name)), position_(position),
      parameter_dependencies_(std::move(parameter_dependencies)) {}

Result<ConstructionLine> ConstructionLine::create_explicit(
    ConstructionLineId id, std::string name, Point3 point, Vector3 direction,
    std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_line") : id.value();

  if (id.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line name must not be empty"));
  }

  if (is_zero(direction)) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line direction must not be zero length"));
  }

  if (!is_unit(direction)) {
    return Result<ConstructionLine>::failure(
        Error::validation(object_id, "construction line direction must be unit length"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionLine>::failure(dependencies.error());
  }

  return Result<ConstructionLine>::success(ConstructionLine(
      std::move(id), std::move(name), point, direction, std::move(parameter_dependencies)));
}

const ConstructionLineId& ConstructionLine::id() const noexcept {
  return id_;
}

const std::string& ConstructionLine::name() const noexcept {
  return name_;
}

Point3 ConstructionLine::point() const noexcept {
  return point_;
}

Vector3 ConstructionLine::direction() const noexcept {
  return direction_;
}

const std::vector<ParameterId>& ConstructionLine::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

ConstructionLine::ConstructionLine(ConstructionLineId id, std::string name, Point3 point,
                                   Vector3 direction,
                                   std::vector<ParameterId> parameter_dependencies)
    : id_(std::move(id)), name_(std::move(name)), point_(point), direction_(direction),
      parameter_dependencies_(std::move(parameter_dependencies)) {}

Result<ConstructionPlane> ConstructionPlane::create_explicit(
    ConstructionPlaneId id, std::string name, Point3 origin, Vector3 x_axis, Vector3 y_axis,
    Vector3 normal, std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("construction_plane") : id.value();

  if (id.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane name must not be empty"));
  }

  if (is_zero(x_axis) || is_zero(y_axis) || is_zero(normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must not be zero length"));
  }

  if (!is_unit(x_axis) || !is_unit(y_axis) || !is_unit(normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must be unit length"));
  }

  if (!is_orthogonal(x_axis, y_axis) || !is_orthogonal(x_axis, normal) ||
      !is_orthogonal(y_axis, normal)) {
    return Result<ConstructionPlane>::failure(
        Error::validation(object_id, "construction plane axes and normal must be orthogonal"));
  }

  if (!same_direction(cross(x_axis, y_axis), normal)) {
    return Result<ConstructionPlane>::failure(Error::validation(
        object_id, "construction plane normal must match x_axis cross y_axis"));
  }

  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error()) {
    return Result<ConstructionPlane>::failure(dependencies.error());
  }

  return Result<ConstructionPlane>::success(ConstructionPlane(
      std::move(id), std::move(name), origin, x_axis, y_axis, normal,
      std::move(parameter_dependencies)));
}

const ConstructionPlaneId& ConstructionPlane::id() const noexcept {
  return id_;
}

const std::string& ConstructionPlane::name() const noexcept {
  return name_;
}

Point3 ConstructionPlane::origin() const noexcept {
  return origin_;
}

Vector3 ConstructionPlane::x_axis() const noexcept {
  return x_axis_;
}

Vector3 ConstructionPlane::y_axis() const noexcept {
  return y_axis_;
}

Vector3 ConstructionPlane::normal() const noexcept {
  return normal_;
}

DatumPlaneId ConstructionPlane::workplane_id() const {
  return DatumPlaneId(id_.value());
}

const std::vector<ParameterId>& ConstructionPlane::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

ConstructionPlane::ConstructionPlane(ConstructionPlaneId id, std::string name, Point3 origin,
                                     Vector3 x_axis, Vector3 y_axis, Vector3 normal,
                                     std::vector<ParameterId> parameter_dependencies)
    : id_(std::move(id)), name_(std::move(name)), origin_(origin), x_axis_(x_axis),
      y_axis_(y_axis), normal_(normal), parameter_dependencies_(std::move(parameter_dependencies)) {}

} // namespace blcad
