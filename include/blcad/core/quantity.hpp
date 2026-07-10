#pragma once

#include "blcad/core/result.hpp"

#include <cstddef>
#include <string_view>

namespace blcad {

enum class QuantityKind {
  LengthMm,
  Count,
};

class Quantity {
public:
  [[nodiscard]] static Result<Quantity> length_mm(double value_mm,
                                                  std::string_view object_id = "quantity");
  // A dimensionless positive integer amount, for example a hole count.
  [[nodiscard]] static Result<Quantity> count(double value,
                                              std::string_view object_id = "quantity");

  [[nodiscard]] QuantityKind kind() const noexcept;
  [[nodiscard]] double millimeters() const noexcept;
  [[nodiscard]] std::size_t count_value() const noexcept;
  [[nodiscard]] std::string_view unit() const noexcept;
  [[nodiscard]] bool is_positive_length() const noexcept;
  [[nodiscard]] bool is_valid_count() const noexcept;

private:
  Quantity(double value, QuantityKind kind);

  double value_mm_;
  QuantityKind kind_;
};

} // namespace blcad
