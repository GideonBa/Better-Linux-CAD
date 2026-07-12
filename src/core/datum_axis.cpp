#include "blcad/core/datum_axis.hpp"

#include "blcad/core/error.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

namespace blcad {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

[[nodiscard]] bool is_unit(Vector3 vector) noexcept {
  return std::abs(length(vector) - 1.0) <= k_tolerance;
}

[[nodiscard]] bool is_zero(Vector3 vector) noexcept {
  return length(vector) <= k_tolerance;
}

[[nodiscard]] bool is_finite(Point3 point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

[[nodiscard]] bool is_finite(Vector3 vector) noexcept {
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] Result<std::size_t>
validate_parameter_dependencies(const std::string& object_id,
                                const std::vector<ParameterId>& parameter_dependencies) {
  for (const auto& parameter_id : parameter_dependencies) {
    if (parameter_id.empty()) {
      return Result<std::size_t>::failure(
          Error::validation(object_id, "datum axis parameter dependencies must not be empty"));
    }
  }

  for (std::size_t i = 0; i < parameter_dependencies.size(); ++i) {
    for (std::size_t j = i + 1U; j < parameter_dependencies.size(); ++j) {
      if (parameter_dependencies[i] == parameter_dependencies[j]) {
        return Result<std::size_t>::failure(
            Error::validation(object_id, "datum axis parameter dependencies must be unique"));
      }
    }
  }
  return Result<std::size_t>::success(parameter_dependencies.size());
}

} // namespace

std::string_view to_string(DatumAxisKind kind) noexcept {
  switch (kind) {
  case DatumAxisKind::Explicit:
    return "explicit";
  case DatumAxisKind::FromConstructionLine:
    return "from_construction_line";
  }
  return "explicit";
}

Result<DatumAxis> DatumAxis::create_explicit(DatumAxisId id, std::string name, Point3 origin,
                                             Vector3 direction,
                                             std::vector<ParameterId> parameter_dependencies) {
  const auto object_id = id.empty() ? std::string("datum_axis") : id.value();
  if (id.empty())
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis id must not be empty"));
  if (name.empty())
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis name must not be empty"));
  if (!is_finite(origin) || !is_finite(direction))
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis origin and direction must be finite"));
  if (is_zero(direction))
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis direction must not be zero length"));
  if (!is_unit(direction))
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis direction must be unit length"));
  const auto dependencies = validate_parameter_dependencies(object_id, parameter_dependencies);
  if (dependencies.has_error())
    return Result<DatumAxis>::failure(dependencies.error());
  return Result<DatumAxis>::success(
      DatumAxis(std::move(id), std::move(name), DatumAxisKind::Explicit, origin, direction,
                std::move(parameter_dependencies), ConstructionLineId()));
}

Result<DatumAxis>
DatumAxis::create_from_construction_line(DatumAxisId id, std::string name,
                                         ConstructionLineId source_construction_line) {
  const auto object_id = id.empty() ? std::string("datum_axis") : id.value();
  if (id.empty())
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis id must not be empty"));
  if (name.empty())
    return Result<DatumAxis>::failure(
        Error::validation(object_id, "datum axis name must not be empty"));
  if (source_construction_line.empty())
    return Result<DatumAxis>::failure(Error::validation(
        object_id, "construction-line-derived datum axis source line id must not be empty"));
  return Result<DatumAxis>::success(DatumAxis(std::move(id), std::move(name),
                                              DatumAxisKind::FromConstructionLine, Point3{},
                                              Vector3{}, {}, std::move(source_construction_line)));
}

DatumAxis::DatumAxis(DatumAxisId id, std::string name, DatumAxisKind kind, Point3 origin,
                     Vector3 direction, std::vector<ParameterId> parameter_dependencies,
                     ConstructionLineId source_construction_line)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), origin_(origin),
      direction_(direction), parameter_dependencies_(std::move(parameter_dependencies)),
      source_construction_line_(std::move(source_construction_line)) {}

const DatumAxisId& DatumAxis::id() const noexcept {
  return id_;
}

const std::string& DatumAxis::name() const noexcept {
  return name_;
}

DatumAxisKind DatumAxis::kind() const noexcept {
  return kind_;
}

Point3 DatumAxis::origin() const noexcept {
  return origin_;
}

Vector3 DatumAxis::direction() const noexcept {
  return direction_;
}

const std::vector<ParameterId>& DatumAxis::parameter_dependencies() const noexcept {
  return parameter_dependencies_;
}

const ConstructionLineId& DatumAxis::source_construction_line() const noexcept {
  return source_construction_line_;
}

} // namespace blcad
