#include "blcad/core/quantity.hpp"

#include <cmath>
#include <string>

namespace blcad {
namespace {

constexpr double k_count_integer_tolerance = 1.0e-9;

} // namespace

Result<Quantity> Quantity::length_mm(double value_mm, std::string_view object_id) {
  const std::string id(object_id);

  if (!std::isfinite(value_mm)) {
    return Result<Quantity>::failure(Error::validation(id, "length must be finite"));
  }

  if (value_mm <= 0.0) {
    return Result<Quantity>::failure(Error::validation(id, "length must be greater than 0 mm"));
  }

  return Result<Quantity>::success(Quantity(value_mm, QuantityKind::LengthMm));
}

Result<Quantity> Quantity::count(double value, std::string_view object_id) {
  const std::string id(object_id);

  if (!std::isfinite(value)) {
    return Result<Quantity>::failure(Error::validation(id, "count must be finite"));
  }

  if (value < 1.0) {
    return Result<Quantity>::failure(Error::validation(id, "count must be at least 1"));
  }

  if (std::abs(value - std::round(value)) > k_count_integer_tolerance) {
    return Result<Quantity>::failure(Error::validation(id, "count must be a whole number"));
  }

  return Result<Quantity>::success(Quantity(std::round(value), QuantityKind::Count));
}

Result<Quantity> Quantity::angle_deg(double value_deg, std::string_view object_id) {
  const std::string id(object_id);

  if (!std::isfinite(value_deg)) {
    return Result<Quantity>::failure(Error::validation(id, "angle must be finite"));
  }

  return Result<Quantity>::success(Quantity(value_deg, QuantityKind::AngleDeg));
}

QuantityKind Quantity::kind() const noexcept {
  return kind_;
}

double Quantity::millimeters() const noexcept {
  return value_mm_;
}

std::size_t Quantity::count_value() const noexcept {
  return static_cast<std::size_t>(value_mm_);
}

double Quantity::degrees() const noexcept {
  return value_mm_;
}

std::string_view Quantity::unit() const noexcept {
  if (kind_ == QuantityKind::Count) {
    return "1";
  }
  if (kind_ == QuantityKind::AngleDeg) {
    return "deg";
  }
  return "mm";
}

bool Quantity::is_positive_length() const noexcept {
  return kind_ == QuantityKind::LengthMm && value_mm_ > 0.0;
}

bool Quantity::is_valid_count() const noexcept {
  return kind_ == QuantityKind::Count && value_mm_ >= 1.0;
}

bool Quantity::is_valid_angle() const noexcept {
  return kind_ == QuantityKind::AngleDeg;
}

Quantity::Quantity(double value, QuantityKind kind) : value_mm_(value), kind_(kind) {}

} // namespace blcad
