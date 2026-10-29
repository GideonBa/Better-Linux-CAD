#pragma once

#include "blcad/core/spatial.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace blcad::geometry {

enum class SketchDimensionPresentation {
  Horizontal,
  Vertical,
  Aligned,
  Radius,
  Diameter,
  Angle,
  ArcLength
};

struct SketchDimensionLine {
  Point2 start;
  Point2 end;
  friend bool operator==(const SketchDimensionLine&, const SketchDimensionLine&) = default;
};
struct SketchDimensionArc {
  Point2 center;
  double radius{};
  double start_angle{};
  double sweep_angle{};
  friend bool operator==(const SketchDimensionArc&, const SketchDimensionArc&) = default;
};
struct SketchDimensionTerminator {
  Point2 tip;
  Point2 direction;
  friend bool operator==(const SketchDimensionTerminator&,
                         const SketchDimensionTerminator&) = default;
};

// Immutable measured geometry, separate from where the user places its label.
// Everything is in Sketch-local coordinates; no camera, Qt or persistence state.
struct SketchDimensionGeometry {
  SketchDimensionPresentation presentation{SketchDimensionPresentation::Aligned};
  std::vector<Point2> measured_references;
  std::vector<SketchDimensionLine> measured_lines;
  Point2 center;
  double radius{};
  double start_angle{};
  double sweep_angle{};
  double default_offset{1};
  bool bounded_arc{false};
  bool angular_vertex{true};
  friend bool operator==(const SketchDimensionGeometry&, const SketchDimensionGeometry&) = default;
};

struct SketchDimensionLayout {
  SketchDimensionGeometry geometry;
  Point2 label_anchor;
  std::vector<SketchDimensionLine> dimension_lines;
  std::vector<SketchDimensionLine> extension_lines;
  std::vector<SketchDimensionLine> label_leaders;
  std::vector<SketchDimensionArc> dimension_arcs;
  std::vector<SketchDimensionTerminator> terminators;
  std::vector<Point2> origin_marks;
  friend bool operator==(const SketchDimensionLayout&, const SketchDimensionLayout&) = default;
};

namespace dimension_layout_detail {
inline Point2 add(Point2 a, Point2 b) {
  return {a.x + b.x, a.y + b.y};
}
inline Point2 subtract(Point2 a, Point2 b) {
  return {a.x - b.x, a.y - b.y};
}
inline Point2 scale(Point2 a, double s) {
  return {a.x * s, a.y * s};
}
inline double dot(Point2 a, Point2 b) {
  return a.x * b.x + a.y * b.y;
}
inline double magnitude(Point2 a) {
  return std::hypot(a.x, a.y);
}
inline Point2 unit(Point2 a) {
  const auto n = magnitude(a);
  return n > 1e-12 ? scale(a, 1 / n) : Point2{1, 0};
}
inline Point2 polar(double angle) {
  return {std::cos(angle), std::sin(angle)};
}
inline double turn(double angle) {
  const auto t = 2 * std::numbers::pi;
  double a = std::fmod(angle, t);
  return a < 0 ? a + t : a;
}
inline Point2 closest_on_segment(Point2 p, Point2 a, Point2 b) {
  const auto d = subtract(b, a);
  const double n = dot(d, d);
  return n > 1e-20 ? add(a, scale(d, std::clamp(dot(subtract(p, a), d) / n, 0.0, 1.0))) : a;
}
inline double arc_angle(const SketchDimensionGeometry& g, Point2 label) {
  const auto v = subtract(label, g.center);
  const auto angle =
      magnitude(v) > 1e-12 ? std::atan2(v.y, v.x) : g.start_angle + g.sweep_angle / 2;
  if (!g.bounded_arc)
    return angle;
  const auto travelled =
      g.sweep_angle >= 0 ? turn(angle - g.start_angle) : turn(g.start_angle - angle);
  if (travelled <= std::abs(g.sweep_angle) + 1e-10)
    return angle;
  const auto end = g.start_angle + g.sweep_angle;
  const auto separation = [](double a, double b) {
    return std::abs(std::remainder(a - b, 2 * std::numbers::pi));
  };
  return separation(angle, g.start_angle) < separation(angle, end) ? g.start_angle : end;
}
inline void append_line(std::vector<SketchDimensionLine>& lines, Point2 a, Point2 b) {
  if (magnitude(subtract(a, b)) > 1e-10)
    lines.push_back({a, b});
}
} // namespace dimension_layout_detail

[[nodiscard]] inline Point2 default_dimension_label_anchor(const SketchDimensionGeometry& g) {
  using namespace dimension_layout_detail;
  const auto offset = g.default_offset;
  if ((g.presentation == SketchDimensionPresentation::Radius ||
       g.presentation == SketchDimensionPresentation::Diameter) &&
      !g.bounded_arc)
    return add(g.center, {offset * .55, offset * .55});
  if (g.presentation == SketchDimensionPresentation::Radius ||
      g.presentation == SketchDimensionPresentation::Diameter ||
      g.presentation == SketchDimensionPresentation::ArcLength) {
    const auto angle = g.bounded_arc ? g.start_angle + g.sweep_angle / 2 : std::numbers::pi / 4;
    return add(g.center, scale(polar(angle), g.radius + offset));
  }
  if (g.presentation == SketchDimensionPresentation::Angle) {
    if (g.angular_vertex)
      return add(g.center, scale(polar(g.start_angle + g.sweep_angle / 2), g.radius));
    return add(g.center, {offset, offset});
  }
  if (g.measured_references.size() < 2)
    return add(g.center, {offset, offset});
  const auto a = g.measured_references.front(), b = g.measured_references.back();
  const auto mid = scale(add(a, b), .5);
  if (g.presentation == SketchDimensionPresentation::Horizontal)
    return {mid.x, std::max(a.y, b.y) + offset};
  if (g.presentation == SketchDimensionPresentation::Vertical)
    return {std::max(a.x, b.x) + offset, mid.y};
  const auto along = unit(subtract(b, a));
  Point2 normal{-along.y, along.x};
  if (normal.y < 0 || (std::abs(normal.y) < 1e-12 && normal.x < 0))
    normal = scale(normal, -1);
  return add(mid, scale(normal, offset));
}

// Reflow from measured references on every placement, including transient label
// drags. Witness gaps, overruns and arrow sizes are applied later in logical DIPs.
[[nodiscard]] inline SketchDimensionLayout place_dimension_layout(const SketchDimensionGeometry& g,
                                                                  Point2 label) {
  using namespace dimension_layout_detail;
  SketchDimensionLayout result;
  result.geometry = g;
  result.label_anchor = label;
  const auto radial = g.presentation == SketchDimensionPresentation::Radius ||
                      g.presentation == SketchDimensionPresentation::Diameter;
  if (radial) {
    const auto direction = polar(arc_angle(g, label));
    const auto tip = add(g.center, scale(direction, g.radius));
    const auto a = g.presentation == SketchDimensionPresentation::Radius
                       ? g.center
                       : add(g.center, scale(direction, -g.radius));
    append_line(result.dimension_lines, a, tip);
    if (g.presentation == SketchDimensionPresentation::Diameter)
      result.terminators.push_back({a, scale(direction, -1)});
    else
      result.origin_marks.push_back(g.center);
    result.terminators.push_back({tip, direction});
    if (magnitude(subtract(closest_on_segment(label, a, tip), label)) > 1e-10)
      append_line(result.label_leaders, tip, label);
    return result;
  }
  if (g.presentation == SketchDimensionPresentation::Angle ||
      g.presentation == SketchDimensionPresentation::ArcLength) {
    if (!g.angular_vertex) {
      // Parallel offset lines have no finite angular vertex. Identify both real
      // entities with leaders rather than inventing an intersection/angle arc.
      for (const auto& line : g.measured_lines)
        append_line(result.label_leaders, scale(add(line.start, line.end), .5), label);
      return result;
    }
    double radius = magnitude(subtract(label, g.center));
    if (radius < 1e-10)
      radius = std::max(g.radius, g.default_offset);
    result.dimension_arcs.push_back({g.center, radius, g.start_angle, g.sweep_angle});
    const double sign = g.sweep_angle >= 0 ? 1 : -1;
    for (std::size_t i = 0; i < 2; ++i) {
      const double angle = g.start_angle + (i ? g.sweep_angle : 0);
      const auto point = add(g.center, scale(polar(angle), radius));
      Point2 reference;
      if (g.presentation == SketchDimensionPresentation::ArcLength)
        reference = add(g.center, scale(polar(angle), g.radius));
      else if (g.measured_lines.size() == 2)
        reference = closest_on_segment(point, g.measured_lines[i].start, g.measured_lines[i].end);
      else
        reference = g.center;
      append_line(result.extension_lines, reference, point);
      result.terminators.push_back(
          {point, scale(Point2{-std::sin(angle), std::cos(angle)}, i ? sign : -sign)});
    }
    auto bounded = g;
    bounded.bounded_arc = true;
    const auto closest = add(g.center, scale(polar(arc_angle(bounded, label)), radius));
    if (magnitude(subtract(closest, label)) > 1e-10)
      append_line(result.label_leaders, closest, label);
    return result;
  }
  if (g.measured_references.size() < 2)
    return result;
  const auto a = g.measured_references.front(), b = g.measured_references.back();
  Point2 first, second;
  if (g.presentation == SketchDimensionPresentation::Horizontal) {
    first = {a.x, label.y};
    second = {b.x, label.y};
  } else if (g.presentation == SketchDimensionPresentation::Vertical) {
    first = {label.x, a.y};
    second = {label.x, b.y};
  } else {
    const auto along = unit(subtract(b, a));
    const Point2 normal{-along.y, along.x};
    const auto offset = scale(normal, dot(subtract(label, a), normal));
    first = add(a, offset);
    second = add(b, offset);
  }
  append_line(result.dimension_lines, first, second);
  append_line(result.extension_lines, a, first);
  append_line(result.extension_lines, b, second);
  const auto direction = unit(subtract(second, first));
  result.terminators = {{first, scale(direction, -1)}, {second, direction}};
  const auto closest = closest_on_segment(label, first, second);
  if (magnitude(subtract(closest, label)) > 1e-10)
    append_line(result.label_leaders, closest, label);
  return result;
}
} // namespace blcad::geometry
