#include "blcad/core/quantity.hpp"

#include <cmath>
#include <string>

namespace blcad {

Result<Quantity> Quantity::length_mm(double value_mm, std::string_view object_id) {
  const std::string id(object_id);

  if (!std::isfinite(value_mm)) {
    return Result<Quantity>::failure(Error::validation(id, "length must be finite"));
  }

  if (value_mm <= 0.0) {
    return Result<Quantity>::failure(Error::validation(id, "length must be greater than 0 mm"));
  }

  return Result<Quantity>::success(Quantity(value_mm));
}

double Quantity::millimeters() const noexcept {
  return value_mm_;
}

std::string_view Quantity::unit() const noexcept {
  return "mm";
}

bool Quantity::is_positive_length() const noexcept {
  return value_mm_ > 0.0;
}

Quantity::Quantity(double value_mm) : value_mm_(value_mm) {}

} // namespace blcad
