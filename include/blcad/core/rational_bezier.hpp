#pragma once

#include "blcad/core/spatial.hpp"

#include <array>
#include <cmath>

namespace blcad {

// Homogeneous cubic Bezier algebra shared by persistence consumers, the solver,
// editing and rendering. Unit weights reproduce the ordinary polynomial cubic.
struct RationalBezier {
  std::array<Point2, 4> poles;
  std::array<double, 4> weights{1, 1, 1, 1};

  struct Homogeneous {
    double x, y, w;
  };
  using Controls = std::array<Homogeneous, 4>;

  [[nodiscard]] Controls homogeneous() const noexcept {
    Controls result{};
    for (std::size_t i = 0; i < 4; ++i)
      result[i] = {poles[i].x * weights[i], poles[i].y * weights[i], weights[i]};
    return result;
  }
  [[nodiscard]] static Homogeneous mix(Homogeneous a, Homogeneous b, double t) noexcept {
    return {(1 - t) * a.x + t * b.x, (1 - t) * a.y + t * b.y, (1 - t) * a.w + t * b.w};
  }
  [[nodiscard]] static Homogeneous evaluate(Controls controls, int degree, double t) noexcept {
    for (int level = degree; level > 0; --level)
      for (int i = 0; i < level; ++i)
        controls[i] = mix(controls[i], controls[i + 1], t);
    return controls[0];
  }
  [[nodiscard]] static Controls differentiated(Controls controls, int degree) noexcept {
    for (int i = 0; i < degree; ++i)
      controls[i] = {degree * (controls[i + 1].x - controls[i].x),
                     degree * (controls[i + 1].y - controls[i].y),
                     degree * (controls[i + 1].w - controls[i].w)};
    return controls;
  }
  [[nodiscard]] Point2 point(double t) const noexcept {
    const auto h = evaluate(homogeneous(), 3, t);
    return {h.x / h.w, h.y / h.w};
  }
  [[nodiscard]] Point2 derivative(double t) const noexcept {
    const auto controls = homogeneous();
    const auto h = evaluate(controls, 3, t);
    const auto d = evaluate(differentiated(controls, 3), 2, t);
    return {(d.x - h.x * d.w / h.w) / h.w, (d.y - h.y * d.w / h.w) / h.w};
  }
  [[nodiscard]] Point2 second_derivative(double t) const noexcept {
    const auto controls = homogeneous();
    const auto h = evaluate(controls, 3, t);
    const auto first = differentiated(controls, 3);
    const auto d = evaluate(first, 2, t);
    const auto dd = evaluate(differentiated(first, 2), 1, t);
    const auto p = point(t);
    const auto v = derivative(t);
    return {(dd.x - p.x * dd.w - 2 * v.x * d.w) / h.w, (dd.y - p.y * dd.w - 2 * v.y * d.w) / h.w};
  }
  // Cubic blossom evaluated at a,a,a / a,a,b / a,b,b / b,b,b.
  // Exact for subdivision, reversal and polynomial continuation alike.
  [[nodiscard]] RationalBezier interval(double a, double b) const noexcept {
    RationalBezier result{};
    for (int i = 0; i < 4; ++i) {
      auto controls = homogeneous();
      for (int level = 0; level < 3; ++level) {
        const double t = level < 3 - i ? a : b;
        for (int j = 0; j < 3 - level; ++j)
          controls[j] = mix(controls[j], controls[j + 1], t);
      }
      result.weights[i] = controls[0].w;
      result.poles[i] = {controls[0].x / controls[0].w, controls[0].y / controls[0].w};
    }
    return result;
  }
  [[nodiscard]] bool valid() const noexcept {
    for (std::size_t i = 0; i < 4; ++i)
      if (!std::isfinite(weights[i]) || weights[i] <= 0 || !std::isfinite(poles[i].x) ||
          !std::isfinite(poles[i].y))
        return false;
    return true;
  }
};

} // namespace blcad
