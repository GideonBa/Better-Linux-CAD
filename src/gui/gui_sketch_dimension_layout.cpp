#include "blcad/gui/gui_sketch_dimension_layout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace blcad::gui {
namespace {
GuiSketchScreenPoint add(GuiSketchScreenPoint a, GuiSketchScreenPoint b) {
  return {a.x + b.x, a.y + b.y};
}
GuiSketchScreenPoint sub(GuiSketchScreenPoint a, GuiSketchScreenPoint b) {
  return {a.x - b.x, a.y - b.y};
}
GuiSketchScreenPoint mul(GuiSketchScreenPoint a, double s) {
  return {a.x * s, a.y * s};
}
double dot(GuiSketchScreenPoint a, GuiSketchScreenPoint b) {
  return a.x * b.x + a.y * b.y;
}
double norm(GuiSketchScreenPoint a) {
  return std::hypot(a.x, a.y);
}
GuiSketchScreenPoint unit(GuiSketchScreenPoint a) {
  const auto n = norm(a);
  return n > 1e-12 ? mul(a, 1 / n) : GuiSketchScreenPoint{1, 0};
}
double closest_parameter(GuiSketchScreenPoint p, GuiSketchScreenPoint a, GuiSketchScreenPoint b) {
  const auto d = sub(b, a);
  const auto n = dot(d, d);
  return n > 1e-20 ? std::clamp(dot(sub(p, a), d) / n, 0.0, 1.0) : 0;
}
double cross(GuiSketchScreenPoint a, GuiSketchScreenPoint b, GuiSketchScreenPoint p) {
  return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}
bool triangle_contains(const std::array<GuiSketchScreenPoint, 3>& t, GuiSketchScreenPoint p) {
  const double a = cross(t[0], t[1], p), b = cross(t[1], t[2], p), c = cross(t[2], t[0], p);
  return !((a < 0 || b < 0 || c < 0) && (a > 0 || b > 0 || c > 0));
}
} // namespace

Result<GuiDimensionScreenLayout>
resolve_dimension_screen_layout(const GuiSketchAnnotationPrimitive& annotation,
                                const GuiSketchPlaneMapping& mapping,
                                GuiDimensionLayoutMetrics metrics) {
  if (!std::isfinite(metrics.arrow_length_dip) || metrics.arrow_length_dip <= 0 ||
      !std::isfinite(metrics.witness_gap_dip) || metrics.witness_gap_dip < 0 ||
      !std::isfinite(metrics.witness_overrun_dip) || metrics.witness_overrun_dip < 0 ||
      !std::isfinite(metrics.arc_error_dip) || metrics.arc_error_dip <= 0 ||
      !std::isfinite(metrics.text_gap_dip) || metrics.text_gap_dip < 0)
    return Result<GuiDimensionScreenLayout>::failure(Error::validation(
        annotation.semantic_id, "Dimension screen metrics must be finite and positive"));
  GuiDimensionScreenLayout result;
  auto label = mapping.plane_to_screen(annotation.point);
  if (!label)
    return Result<GuiDimensionScreenLayout>::failure(label.error());
  result.label_anchor = label.value();
  if (annotation.label_bounds_dip) {
    const auto& r = *annotation.label_bounds_dip;
    result.text_bounds =
        GuiSketchScreenRect{{label.value().x + r.left(), label.value().y + r.top()},
                            {label.value().x + r.right(), label.value().y + r.bottom()}};
  }
  std::optional<Error> error;
  const auto project = [&](Point2 p) {
    auto value = mapping.plane_to_screen(p);
    if (!value) {
      error = value.error();
      return GuiSketchScreenPoint{};
    }
    return value.value();
  };
  const auto unproject = [&](GuiSketchScreenPoint p) {
    auto value = mapping.screen_to_plane(p);
    if (!value) {
      error = value.error();
      return Point2{};
    }
    return value.value();
  };
  const auto segment = [&](GuiDimensionStrokeRole role, Point2 a, Point2 b) {
    result.strokes.push_back({role, {project(a), project(b)}, {a, b}});
  };
  if (!annotation.dimension_layout) {
    // Compatibility for callers constructing old annotations directly. New
    // dimensions always provide explicit family geometry.
    if (annotation.leader.size() >= 2) {
      segment(GuiDimensionStrokeRole::Dimension, annotation.leader.front(),
              annotation.leader.back());
      const auto a = annotation.leader.front(), b = annotation.leader.back();
      segment(GuiDimensionStrokeRole::Leader, {(a.x + b.x) / 2, (a.y + b.y) / 2}, annotation.point);
    }
    if (error)
      return Result<GuiDimensionScreenLayout>::failure(*error);
    return Result<GuiDimensionScreenLayout>::success(std::move(result));
  }
  const auto layout =
      geometry::place_dimension_layout(annotation.dimension_layout->geometry, annotation.point);
  for (const auto& p : layout.geometry.measured_references)
    result.measured_references.push_back(project(p));
  for (const auto& line : layout.dimension_lines)
    segment(GuiDimensionStrokeRole::Dimension, line.start, line.end);
  for (const auto& line : layout.label_leaders)
    segment(GuiDimensionStrokeRole::Leader, line.start, line.end);
  for (const auto& line : layout.extension_lines) {
    const auto a = project(line.start), b = project(line.end);
    const auto d = sub(b, a);
    const double n = norm(d);
    if (n < 1e-8)
      continue;
    const auto u = unit(d);
    const auto start = add(a, mul(u, std::min(metrics.witness_gap_dip, n * .25)));
    const auto end = add(b, mul(u, metrics.witness_overrun_dip));
    result.strokes.push_back(
        {GuiDimensionStrokeRole::Extension, {start, end}, {unproject(start), unproject(end)}});
  }
  for (const auto& arc : layout.dimension_arcs) {
    const auto at = [&](double angle) {
      return Point2{arc.center.x + arc.radius * std::cos(angle),
                    arc.center.y + arc.radius * std::sin(angle)};
    };
    GuiDimensionScreenStroke stroke{GuiDimensionStrokeRole::Dimension, {}, {}};
    const auto first = at(arc.start_angle);
    const auto last = at(arc.start_angle + arc.sweep_angle);
    stroke.plane_points.push_back(first);
    stroke.points.push_back(project(first));
    std::function<void(double, double, Point2, Point2, GuiSketchScreenPoint, GuiSketchScreenPoint,
                       int)>
        subdivide;
    subdivide = [&](double a, double b, Point2 pa, Point2 pb, GuiSketchScreenPoint sa,
                    GuiSketchScreenPoint sb, int depth) {
      const double angle = (a + b) / 2;
      const auto pm = at(angle);
      const auto sm = project(pm);
      const auto nearest = add(sa, mul(sub(sb, sa), closest_parameter(sm, sa, sb)));
      if (norm(sub(sm, nearest)) > metrics.arc_error_dip && depth < 14) {
        subdivide(a, angle, pa, pm, sa, sm, depth + 1);
        subdivide(angle, b, pm, pb, sm, sb, depth + 1);
      } else {
        stroke.plane_points.push_back(pb);
        stroke.points.push_back(sb);
      }
    };
    subdivide(arc.start_angle, arc.start_angle + arc.sweep_angle, first, last,
              stroke.points.front(), project(last), 0);
    result.strokes.push_back(std::move(stroke));
  }
  for (const auto& origin : layout.origin_marks) {
    const auto p = project(origin);
    for (const auto d : {GuiSketchScreenPoint{2.5, 0}, GuiSketchScreenPoint{0, 2.5}}) {
      const auto a = sub(p, d), b = add(p, d);
      result.strokes.push_back(
          {GuiDimensionStrokeRole::Origin, {a, b}, {unproject(a), unproject(b)}});
    }
  }
  // Keep the value rectangle clear of its measure line in logical DIPs.
  // The persisted plane anchor places the dimension; text is view-sized.
  GuiSketchScreenPoint normal{0, -1};
  if (!layout.dimension_arcs.empty()) {
    const auto center = project(layout.dimension_arcs.front().center);
    normal = unit(sub(label.value(), center));
  } else {
    const auto& guides =
        layout.label_leaders.empty() ? layout.dimension_lines : layout.label_leaders;
    if (!guides.empty()) {
      const auto along = unit(sub(project(guides.back().end), project(guides.back().start)));
      normal = {-along.y, along.x};
      if (normal.y > 0 || (std::abs(normal.y) < 1e-10 && normal.x > 0))
        normal = mul(normal, -1);
    }
  }
  const double half_width =
      annotation.label_bounds_dip
          ? (annotation.label_bounds_dip->right() - annotation.label_bounds_dip->left()) / 2
          : 0;
  const double half_height =
      annotation.label_bounds_dip
          ? (annotation.label_bounds_dip->bottom() - annotation.label_bounds_dip->top()) / 2
          : 8;
  const double clearance =
      std::abs(normal.x) * half_width + std::abs(normal.y) * half_height + metrics.text_gap_dip;
  result.label_anchor = add(label.value(), mul(normal, clearance));
  if (annotation.label_bounds_dip) {
    const auto& r = *annotation.label_bounds_dip;
    result.text_bounds = GuiSketchScreenRect{
        {result.label_anchor.x + r.left(), result.label_anchor.y + r.top()},
        {result.label_anchor.x + r.right(), result.label_anchor.y + r.bottom()}};
  }
  double length = 0;
  for (const auto& stroke : result.strokes)
    if (stroke.role == GuiDimensionStrokeRole::Dimension)
      for (std::size_t i = 1; i < stroke.points.size(); ++i)
        length += norm(sub(stroke.points[i], stroke.points[i - 1]));
  const bool outside = layout.terminators.size() == 2 && length < 2 * metrics.arrow_length_dip + 6;
  for (const auto& end : layout.terminators) {
    const auto tip = project(end.tip);
    const auto forward = project({end.tip.x + end.direction.x, end.tip.y + end.direction.y});
    auto direction = unit(sub(forward, tip));
    if (outside)
      direction = mul(direction, -1);
    const auto base = sub(tip, mul(direction, metrics.arrow_length_dip));
    const GuiSketchScreenPoint normal{-direction.y, direction.x};
    result.arrows.push_back({{tip, add(base, mul(normal, metrics.arrow_length_dip * .42)),
                              sub(base, mul(normal, metrics.arrow_length_dip * .42))},
                             end.tip});
    if (outside) {
      const auto tail = sub(tip, mul(direction, metrics.arrow_length_dip + 2));
      result.strokes.push_back(
          {GuiDimensionStrokeRole::Dimension, {tip, tail}, {end.tip, unproject(tail)}});
    }
  }
  if (error)
    return Result<GuiDimensionScreenLayout>::failure(*error);
  return Result<GuiDimensionScreenLayout>::success(std::move(result));
}

std::optional<GuiDimensionLayoutHit>
hit_dimension_screen_layout(const GuiDimensionScreenLayout& layout, GuiSketchScreenPoint point,
                            double tolerance_dip) {
  std::optional<GuiDimensionLayoutHit> result;
  const auto record = [&](Point2 plane, double distance) {
    if (distance <= tolerance_dip && (!result || distance < result->distance_dip))
      result = GuiDimensionLayoutHit{plane, distance};
  };
  for (const auto& stroke : layout.strokes) {
    for (std::size_t i = 1; i < stroke.points.size(); ++i) {
      const auto a = stroke.points[i - 1], b = stroke.points[i];
      const double t = closest_parameter(point, a, b);
      const auto p = add(a, mul(sub(b, a), t));
      const auto pa = stroke.plane_points[i - 1], pb = stroke.plane_points[i];
      record({pa.x + (pb.x - pa.x) * t, pa.y + (pb.y - pa.y) * t}, norm(sub(point, p)));
    }
  }
  for (const auto& arrow : layout.arrows) {
    if (triangle_contains(arrow.polygon, point)) {
      record(arrow.plane_tip, 0);
      continue;
    }
    for (std::size_t i = 0; i < 3; ++i) {
      const auto a = arrow.polygon[i], b = arrow.polygon[(i + 1) % 3];
      record(arrow.plane_tip,
             norm(sub(point, add(a, mul(sub(b, a), closest_parameter(point, a, b))))));
    }
  }
  return result;
}
} // namespace blcad::gui
