#pragma once

#include "blcad/core/result.hpp"

#include <string_view>

namespace blcad {

class Quantity {
public:
  [[nodiscard]] static Result<Quantity> length_mm(double value_mm,
                                                  std::string_view object_id = "quantity");

  [[nodiscard]] double millimeters() const noexcept;
  [[nodiscard]] std::string_view unit() const noexcept;
  [[nodiscard]] bool is_positive_length() const noexcept;

private:
  explicit Quantity(double value_mm);

  double value_mm_;
};

} // namespace blcad
