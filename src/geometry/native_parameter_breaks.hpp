#pragma once
#include <cmath>
#include <stdexcept>
#include <vector>
namespace blcad::geometry::detail {
// Native parameter values only. Repeat periodic knots through the actual trim,
// including a trim whose seam lies outside the basis curve/surface's first period.
inline void append_native_break(std::vector<double>& values, double knot, double first, double last,
                                bool periodic, double period) {
  if (!periodic) {
    if (knot > first && knot < last)
      values.push_back(knot);
    return;
  }
  if (!std::isfinite(period) || period <= 0)
    throw std::runtime_error("Invalid native knot period");
  const double begin = std::ceil((first - knot) / period), end = std::floor((last - knot) / period);
  if (!std::isfinite(begin) || !std::isfinite(end) || std::abs(begin) > 9e15 ||
      std::abs(end) > 9e15 || end - begin > 256)
    throw std::runtime_error("Periodic native trim exceeds the verification budget");
  for (auto shift = static_cast<long long>(begin); shift <= static_cast<long long>(end); ++shift) {
    const double value = knot + double(shift) * period;
    if (value > first && value < last)
      values.push_back(value);
  }
}
} // namespace blcad::geometry::detail
