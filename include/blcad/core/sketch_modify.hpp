#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {

// Block 116 headless Sketch modification authority: trim, extend, split,
// two-line fillet, and two-line chamfer. Every operation rewrites a disposable
// candidate Sketch that copies all unaffected intent verbatim. Ordered profile
// containers whose member curve changes are invalidated while their remaining
// curves survive; other references that a rewrite would invalidate (embedded
// constraints/dimensions and tangent continuity) are rejected before a
// candidate is built, so the source Sketch is never partially mutated. In-place
// edits that preserve an entity id and its endpoint roles keep every referencing
// intent so usable constraints survive.

enum class SketchModifyEndSelector { Start, End };

// A persistent attachment created by trim.  The result endpoint is always a
// point on the rewritten curve.  The cutter is addressed by its topology
// entity id ("entity/..." or "profile/..."); when the cut landed on an
// explicit curve endpoint, cutter_endpoint requests a true point-to-point
// Coincident constraint instead of PointOnObject.
struct SketchModifyTrimAttachment {
  SketchEntityId result_entity;
  SketchModifyEndSelector result_endpoint{SketchModifyEndSelector::Start};
  std::string cutter_entity;
  std::optional<SketchModifyEndSelector> cutter_endpoint;

  friend bool operator==(const SketchModifyTrimAttachment&,
                         const SketchModifyTrimAttachment&) = default;
};

// Point-scoped intent attached to this endpoint in the source topology is no
// longer valid after an in-place trim moves that endpoint.  Entity-scoped
// intent remains valid because the curve identity survives.
struct SketchModifyInvalidatedEndpoint {
  SketchEntityId entity;
  SketchModifyEndSelector endpoint{SketchModifyEndSelector::Start};

  friend bool operator==(const SketchModifyInvalidatedEndpoint&,
                         const SketchModifyInvalidatedEndpoint&) = default;
};

struct SketchModifyResult {
  Sketch sketch;
  // Source entity ids whose identity/endpoint semantics changed (removed or
  // split). References to these are invalid; in-place edits are not listed.
  std::vector<std::string> invalidated_ids;
  // Materialized profile containers that were intentionally removed because a
  // member curve changed. Their remaining member curves stay in the Sketch and
  // may form new regions when profiles are materialized again.
  std::vector<std::string> invalidated_profile_ids;
  // New trim boundaries which the GUI authoring layer turns into automatic
  // Coincident / PointOnObject intents in the same transaction as the rewrite.
  std::vector<SketchModifyTrimAttachment> attachments;
  // Source endpoint roles moved by an in-place edit and whose point-scoped
  // constraint/dimension intent must be pruned before new attachments are
  // authored.
  std::vector<SketchModifyInvalidatedEndpoint> invalidated_endpoints;
};

namespace detail {

using SketchModifyCurve = std::variant<LineSegment, ArcSegment, SplineSegment>;

constexpr double kSketchModifyIdentityTolerance = 1.0e-9;
constexpr double kSketchModifyPointTolerance = 1.0e-7;
constexpr double kSketchModifyParamTolerance = 1.0e-6;
constexpr double kSketchModifyPi = 3.141592653589793238462643383279502884;

[[nodiscard]] inline bool sketch_modify_finite(Point2 point) noexcept {
  return std::isfinite(point.x) && std::isfinite(point.y);
}

[[nodiscard]] inline double sketch_modify_distance(Point2 first, Point2 second) noexcept {
  return std::hypot(second.x - first.x, second.y - first.y);
}

[[nodiscard]] inline const SketchEntityId&
sketch_modify_curve_id(const SketchModifyCurve& curve) noexcept {
  return std::visit([](const auto& value) -> const SketchEntityId& { return value.id(); }, curve);
}

struct ArcGeometry {
  Point2 center;
  double radius{0.0};
  double start_angle{0.0};
  double sweep{0.0};
};

[[nodiscard]] inline double sketch_modify_wrap_two_pi(double angle) noexcept {
  const double two_pi = 2.0 * kSketchModifyPi;
  double value = std::fmod(angle, two_pi);
  if (value < 0.0)
    value += two_pi;
  return value;
}

[[nodiscard]] inline std::optional<ArcGeometry> arc_geometry(Point2 start, Point2 mid,
                                                             Point2 end) noexcept {
  const double determinant =
      2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) + end.x * (start.y - mid.y));
  if (!std::isfinite(determinant) || std::abs(determinant) <= kSketchModifyIdentityTolerance)
    return std::nullopt;
  const double start_norm = start.x * start.x + start.y * start.y;
  const double mid_norm = mid.x * mid.x + mid.y * mid.y;
  const double end_norm = end.x * end.x + end.y * end.y;
  const Point2 center{
      (start_norm * (mid.y - end.y) + mid_norm * (end.y - start.y) + end_norm * (start.y - mid.y)) /
          determinant,
      (start_norm * (end.x - mid.x) + mid_norm * (start.x - end.x) + end_norm * (mid.x - start.x)) /
          determinant};
  const double radius = sketch_modify_distance(center, start);
  if (!sketch_modify_finite(center) || !std::isfinite(radius) ||
      radius <= kSketchModifyIdentityTolerance)
    return std::nullopt;
  const double start_angle = std::atan2(start.y - center.y, start.x - center.x);
  const double mid_angle = std::atan2(mid.y - center.y, mid.x - center.x);
  const double end_angle = std::atan2(end.y - center.y, end.x - center.x);
  const double ccw_sweep = sketch_modify_wrap_two_pi(end_angle - start_angle);
  const double ccw_to_mid = sketch_modify_wrap_two_pi(mid_angle - start_angle);
  const bool counter_clockwise = ccw_to_mid <= ccw_sweep + kSketchModifyIdentityTolerance;
  const double sweep =
      counter_clockwise ? ccw_sweep : -sketch_modify_wrap_two_pi(start_angle - end_angle);
  if (std::abs(sweep) <= kSketchModifyIdentityTolerance)
    return std::nullopt;
  return ArcGeometry{center, radius, start_angle, sweep};
}

[[nodiscard]] inline Point2 arc_point(const ArcGeometry& arc, double angle) noexcept {
  return {arc.center.x + arc.radius * std::cos(angle), arc.center.y + arc.radius * std::sin(angle)};
}

// Fractional position of `angle` along the arc sweep, or nullopt when it is not
// inside the swept range.
[[nodiscard]] inline std::optional<double> arc_param(const ArcGeometry& arc,
                                                     double angle) noexcept {
  double delta = angle - arc.start_angle;
  if (arc.sweep > 0.0) {
    delta = sketch_modify_wrap_two_pi(delta);
    const double t = delta / arc.sweep;
    if (t >= -kSketchModifyParamTolerance && t <= 1.0 + kSketchModifyParamTolerance)
      return std::clamp(t, 0.0, 1.0);
    return std::nullopt;
  }
  delta = sketch_modify_wrap_two_pi(-delta);
  const double t = delta / (-arc.sweep);
  if (t >= -kSketchModifyParamTolerance && t <= 1.0 + kSketchModifyParamTolerance)
    return std::clamp(t, 0.0, 1.0);
  return std::nullopt;
}

struct CubicBezierGeometry {
  Point2 start;
  Point2 control1;
  Point2 control2;
  Point2 end;
  std::array<double, 4> weights{1, 1, 1, 1};
  [[nodiscard]] RationalBezier curve() const noexcept {
    return {{start, control1, control2, end}, weights};
  }
};

struct CurveSampler {
  enum class Kind { Line, Arc, Spline } kind{Kind::Line};
  Point2 line_start;
  Point2 line_end;
  ArcGeometry arc;
  CubicBezierGeometry spline;
};

[[nodiscard]] inline std::optional<CurveSampler> line_sampler(const LineSegment& line) noexcept {
  return CurveSampler{CurveSampler::Kind::Line, line.start(), line.end(), {}, {}};
}

[[nodiscard]] inline std::optional<CurveSampler> arc_sampler(const ArcSegment& arc) noexcept {
  const auto geometry = arc_geometry(arc.start(), arc.mid(), arc.end());
  if (!geometry)
    return std::nullopt;
  return CurveSampler{CurveSampler::Kind::Arc, {}, {}, *geometry, {}};
}

[[nodiscard]] inline std::optional<CurveSampler>
spline_sampler(const SplineSegment& spline) noexcept {
  return CurveSampler{CurveSampler::Kind::Spline,
                      {},
                      {},
                      {},
                      {spline.start(), spline.control1(), spline.control2(), spline.end(), spline.weights()}};
}

[[nodiscard]] inline Point2 bezier_point(const CubicBezierGeometry& spline, double t) noexcept {
  return spline.curve().point(t);
}
[[nodiscard]] inline Point2 bezier_derivative(const CubicBezierGeometry& spline, double t) noexcept {
  return spline.curve().derivative(t);
}
[[nodiscard]] inline Point2 bezier_second_derivative(const CubicBezierGeometry& spline, double t) noexcept {
  return spline.curve().second_derivative(t);
}

[[nodiscard]] inline Point2 sampler_point(const CurveSampler& sampler, double t) noexcept {
  if (sampler.kind == CurveSampler::Kind::Line)
    return {sampler.line_start.x + (sampler.line_end.x - sampler.line_start.x) * t,
            sampler.line_start.y + (sampler.line_end.y - sampler.line_start.y) * t};
  if (sampler.kind == CurveSampler::Kind::Arc)
    return arc_point(sampler.arc, sampler.arc.start_angle + sampler.arc.sweep * t);
  return bezier_point(sampler.spline, t);
}

[[nodiscard]] inline Point2 sampler_derivative(const CurveSampler& sampler, double t) noexcept {
  if (sampler.kind == CurveSampler::Kind::Line)
    return {sampler.line_end.x - sampler.line_start.x, sampler.line_end.y - sampler.line_start.y};
  if (sampler.kind == CurveSampler::Kind::Arc) {
    const double angle = sampler.arc.start_angle + sampler.arc.sweep * t;
    return {-sampler.arc.radius * sampler.arc.sweep * std::sin(angle),
            sampler.arc.radius * sampler.arc.sweep * std::cos(angle)};
  }
  return bezier_derivative(sampler.spline, t);
}

// Parameter of the closest point on the (unbounded) supporting line/circle.
[[nodiscard]] inline std::optional<double> sampler_project(const CurveSampler& sampler,
                                                           Point2 point) noexcept {
  if (sampler.kind == CurveSampler::Kind::Line) {
    const Point2 direction{sampler.line_end.x - sampler.line_start.x,
                           sampler.line_end.y - sampler.line_start.y};
    const double length_sq = direction.x * direction.x + direction.y * direction.y;
    if (length_sq <= kSketchModifyIdentityTolerance)
      return std::nullopt;
    return ((point.x - sampler.line_start.x) * direction.x +
            (point.y - sampler.line_start.y) * direction.y) /
           length_sq;
  }
  if (sampler.kind == CurveSampler::Kind::Arc) {
    const double angle = std::atan2(point.y - sampler.arc.center.y, point.x - sampler.arc.center.x);
    return arc_param(sampler.arc, angle);
  }

  // Seed the closest-point solve densely enough to distinguish separate lobes,
  // then refine the squared-distance stationary point with Newton iterations.
  double best_t = 0.0;
  double best_distance_sq = std::numeric_limits<double>::infinity();
  constexpr std::size_t samples = 256U;
  for (std::size_t index = 0U; index <= samples; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(samples);
    const Point2 candidate = bezier_point(sampler.spline, t);
    const double dx = candidate.x - point.x;
    const double dy = candidate.y - point.y;
    const double distance_sq = dx * dx + dy * dy;
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_t = t;
    }
  }
  for (std::size_t iteration = 0U; iteration < 16U; ++iteration) {
    const Point2 candidate = bezier_point(sampler.spline, best_t);
    const Point2 first = bezier_derivative(sampler.spline, best_t);
    const Point2 second = bezier_second_derivative(sampler.spline, best_t);
    const Point2 delta{candidate.x - point.x, candidate.y - point.y};
    const double numerator = delta.x * first.x + delta.y * first.y;
    const double denominator =
        first.x * first.x + first.y * first.y + delta.x * second.x + delta.y * second.y;
    if (!std::isfinite(denominator) || std::abs(denominator) <= kSketchModifyIdentityTolerance)
      break;
    const double next = std::clamp(best_t - numerator / denominator, 0.0, 1.0);
    if (std::abs(next - best_t) <= 1.0e-12) {
      best_t = next;
      break;
    }
    best_t = next;
  }
  return best_t;
}

struct ParameterizedPoint {
  double parameter{0.0};
  Point2 point;
};

[[nodiscard]] inline double point_to_line_distance(Point2 point, Point2 start,
                                                   Point2 end) noexcept {
  const Point2 direction{end.x - start.x, end.y - start.y};
  const double length = std::hypot(direction.x, direction.y);
  if (length <= kSketchModifyIdentityTolerance)
    return sketch_modify_distance(point, start);
  return std::abs(direction.x * (start.y - point.y) - (start.x - point.x) * direction.y) / length;
}

inline void flatten_bezier(const CubicBezierGeometry& spline, double from, double to,
                           std::size_t depth, std::vector<ParameterizedPoint>& result) {
  constexpr std::size_t minimum_depth = 7U;
  constexpr std::size_t maximum_depth = 16U;
  constexpr double flatness_tolerance = 1.0e-5;
  const Point2 start = bezier_point(spline, from);
  const Point2 end = bezier_point(spline, to);
  const double one_third = from + (to - from) / 3.0;
  const double two_thirds = from + 2.0 * (to - from) / 3.0;
  const double flatness =
      std::max(point_to_line_distance(bezier_point(spline, one_third), start, end),
               point_to_line_distance(bezier_point(spline, two_thirds), start, end));
  if (depth >= maximum_depth || (depth >= minimum_depth && flatness <= flatness_tolerance)) {
    result.push_back({to, end});
    return;
  }
  const double middle = (from + to) * 0.5;
  flatten_bezier(spline, from, middle, depth + 1U, result);
  flatten_bezier(spline, middle, to, depth + 1U, result);
}

[[nodiscard]] inline std::vector<ParameterizedPoint>
parameterized_polyline(const CurveSampler& sampler) {
  std::vector<ParameterizedPoint> result;
  result.push_back({0.0, sampler_point(sampler, 0.0)});
  if (sampler.kind == CurveSampler::Kind::Line) {
    result.push_back({1.0, sampler_point(sampler, 1.0)});
    return result;
  }
  if (sampler.kind == CurveSampler::Kind::Spline) {
    flatten_bezier(sampler.spline, 0.0, 1.0, 0U, result);
    return result;
  }
  const std::size_t segments = std::clamp<std::size_t>(
      static_cast<std::size_t>(std::ceil(std::abs(sampler.arc.sweep) / (kSketchModifyPi / 180.0))),
      8U, 720U);
  result.reserve(segments + 1U);
  for (std::size_t index = 1U; index <= segments; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(segments);
    result.push_back({t, sampler_point(sampler, t)});
  }
  return result;
}

[[nodiscard]] inline bool refine_intersection(const CurveSampler& first, const CurveSampler& second,
                                              double& first_t, double& second_t) noexcept {
  for (std::size_t iteration = 0U; iteration < 20U; ++iteration) {
    const Point2 first_point = sampler_point(first, first_t);
    const Point2 second_point = sampler_point(second, second_t);
    const Point2 residual{first_point.x - second_point.x, first_point.y - second_point.y};
    if (std::hypot(residual.x, residual.y) <= kSketchModifyPointTolerance)
      return true;
    const Point2 first_derivative = sampler_derivative(first, first_t);
    const Point2 second_derivative = sampler_derivative(second, second_t);
    const double determinant =
        first_derivative.y * second_derivative.x - first_derivative.x * second_derivative.y;
    if (std::abs(determinant) <= kSketchModifyIdentityTolerance)
      break;
    const double first_step =
        (residual.x * second_derivative.y - residual.y * second_derivative.x) / determinant;
    const double second_step =
        (first_derivative.y * residual.x - first_derivative.x * residual.y) / determinant;
    first_t = std::clamp(first_t + first_step, 0.0, 1.0);
    second_t = std::clamp(second_t + second_step, 0.0, 1.0);
  }
  return sketch_modify_distance(sampler_point(first, first_t), sampler_point(second, second_t)) <=
         1.0e-5;
}

[[nodiscard]] inline std::vector<Point2> sampled_curve_intersections(const CurveSampler& first,
                                                                     const CurveSampler& second) {
  const auto first_points = parameterized_polyline(first);
  const auto second_points = parameterized_polyline(second);
  std::vector<Point2> result;
  for (std::size_t first_index = 1U; first_index < first_points.size(); ++first_index) {
    const Point2 p = first_points[first_index - 1U].point;
    const Point2 r{first_points[first_index].point.x - p.x,
                   first_points[first_index].point.y - p.y};
    for (std::size_t second_index = 1U; second_index < second_points.size(); ++second_index) {
      const Point2 q = second_points[second_index - 1U].point;
      const Point2 s{second_points[second_index].point.x - q.x,
                     second_points[second_index].point.y - q.y};
      const double denominator = r.x * s.y - r.y * s.x;
      if (std::abs(denominator) <= kSketchModifyIdentityTolerance)
        continue;
      const double first_fraction = ((q.x - p.x) * s.y - (q.y - p.y) * s.x) / denominator;
      const double second_fraction = ((q.x - p.x) * r.y - (q.y - p.y) * r.x) / denominator;
      if (first_fraction < -kSketchModifyParamTolerance ||
          first_fraction > 1.0 + kSketchModifyParamTolerance ||
          second_fraction < -kSketchModifyParamTolerance ||
          second_fraction > 1.0 + kSketchModifyParamTolerance)
        continue;
      double first_t =
          first_points[first_index - 1U].parameter +
          (first_points[first_index].parameter - first_points[first_index - 1U].parameter) *
              first_fraction;
      double second_t =
          second_points[second_index - 1U].parameter +
          (second_points[second_index].parameter - second_points[second_index - 1U].parameter) *
              second_fraction;
      if (!refine_intersection(first, second, first_t, second_t))
        continue;
      const Point2 first_point = sampler_point(first, first_t);
      const Point2 second_point = sampler_point(second, second_t);
      const Point2 point{(first_point.x + second_point.x) * 0.5,
                         (first_point.y + second_point.y) * 0.5};
      if (std::none_of(result.begin(), result.end(), [&](Point2 existing) {
            return sketch_modify_distance(existing, point) <= 1.0e-5;
          }))
        result.push_back(point);
    }
  }
  return result;
}

// Intersection points of two supporting curves restricted to both actual spans.
[[nodiscard]] inline std::vector<Point2> curve_intersections(const CurveSampler& first,
                                                             const CurveSampler& second) {
  if (first.kind == CurveSampler::Kind::Spline || second.kind == CurveSampler::Kind::Spline)
    return sampled_curve_intersections(first, second);
  std::vector<Point2> points;
  const auto on_span = [](const CurveSampler& sampler, Point2 candidate) {
    const auto t = sampler_project(sampler, candidate);
    return t.has_value() && *t >= -kSketchModifyParamTolerance &&
           *t <= 1.0 + kSketchModifyParamTolerance;
  };
  const auto add = [&](Point2 candidate) {
    if (!sketch_modify_finite(candidate))
      return;
    if (!on_span(first, candidate) || !on_span(second, candidate))
      return;
    for (const auto existing : points)
      if (sketch_modify_distance(existing, candidate) <= kSketchModifyPointTolerance)
        return;
    points.push_back(candidate);
  };

  if (first.kind == CurveSampler::Kind::Line && second.kind == CurveSampler::Kind::Line) {
    const Point2 p = first.line_start;
    const Point2 r{first.line_end.x - p.x, first.line_end.y - p.y};
    const Point2 q = second.line_start;
    const Point2 s{second.line_end.x - q.x, second.line_end.y - q.y};
    const double denom = r.x * s.y - r.y * s.x;
    if (std::abs(denom) > kSketchModifyIdentityTolerance) {
      const double t = ((q.x - p.x) * s.y - (q.y - p.y) * s.x) / denom;
      add({p.x + r.x * t, p.y + r.y * t});
    }
    return points;
  }

  const auto line_circle = [&](const CurveSampler& line, const ArcGeometry& arc) {
    const Point2 d{line.line_end.x - line.line_start.x, line.line_end.y - line.line_start.y};
    const double a = d.x * d.x + d.y * d.y;
    if (a <= kSketchModifyIdentityTolerance)
      return;
    const Point2 f{line.line_start.x - arc.center.x, line.line_start.y - arc.center.y};
    const double b = 2.0 * (f.x * d.x + f.y * d.y);
    const double c = f.x * f.x + f.y * f.y - arc.radius * arc.radius;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < -kSketchModifyIdentityTolerance)
      return;
    const double root = std::sqrt(std::max(0.0, discriminant));
    for (const double t : {(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)})
      add({line.line_start.x + d.x * t, line.line_start.y + d.y * t});
  };

  if (first.kind == CurveSampler::Kind::Line && second.kind == CurveSampler::Kind::Arc) {
    line_circle(first, second.arc);
    return points;
  }
  if (first.kind == CurveSampler::Kind::Arc && second.kind == CurveSampler::Kind::Line) {
    line_circle(second, first.arc);
    return points;
  }

  const ArcGeometry& a = first.arc;
  const ArcGeometry& b = second.arc;
  const double center_distance = sketch_modify_distance(a.center, b.center);
  if (center_distance <= kSketchModifyIdentityTolerance ||
      center_distance > a.radius + b.radius + kSketchModifyPointTolerance ||
      center_distance < std::abs(a.radius - b.radius) - kSketchModifyPointTolerance)
    return points;
  const double along =
      (center_distance * center_distance + a.radius * a.radius - b.radius * b.radius) /
      (2.0 * center_distance);
  const double height_sq = a.radius * a.radius - along * along;
  const double height = std::sqrt(std::max(0.0, height_sq));
  const Point2 axis{(b.center.x - a.center.x) / center_distance,
                    (b.center.y - a.center.y) / center_distance};
  const Point2 base{a.center.x + axis.x * along, a.center.y + axis.y * along};
  add({base.x - axis.y * height, base.y + axis.x * height});
  add({base.x + axis.y * height, base.y - axis.x * height});
  return points;
}

[[nodiscard]] inline std::optional<CurveSampler> curve_sampler_for(const Sketch& sketch,
                                                                   const SketchEntityId& id) {
  if (const auto* line = sketch.find_line_segment(id))
    return line_sampler(*line);
  if (const auto* arc = sketch.find_arc_segment(id))
    return arc_sampler(*arc);
  if (const auto* spline = sketch.find_spline_segment(id))
    return spline_sampler(*spline);
  return std::nullopt;
}

[[nodiscard]] inline bool
contour_contains_any(const std::vector<SketchEntityId>& contour,
                     const std::vector<std::string>& entity_ids) noexcept {
  return std::any_of(contour.begin(), contour.end(), [&](const SketchEntityId& id) {
    return std::find(entity_ids.begin(), entity_ids.end(), id.value()) != entity_ids.end();
  });
}

[[nodiscard]] inline std::vector<std::string>
affected_profile_ids(const Sketch& sketch, const std::vector<std::string>& entity_ids) {
  std::vector<std::string> result;
  for (const auto& profile : sketch.closed_profiles())
    if (contour_contains_any(profile.line_segments(), entity_ids))
      result.push_back(profile.id().value());
  for (const auto& profile : sketch.arc_closed_profiles())
    if (contour_contains_any(profile.curve_segments(), entity_ids))
      result.push_back(profile.id().value());
  for (const auto& profile : sketch.composite_closed_profiles()) {
    bool affected = contour_contains_any(profile.outer_contour(), entity_ids);
    for (const auto& contour : profile.inner_contours())
      affected = affected || contour_contains_any(contour, entity_ids);
    if (affected)
      result.push_back(profile.id().value());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

// Rebuilds a candidate Sketch replacing `originals` with `replacements`, copying
// every other entity and all intent. Ordered profile containers that reference a
// rewritten entity are deliberately omitted: their member curves remain, while
// the now-stale region can be materialized again from the resulting topology.
[[nodiscard]] inline Result<Sketch>
rebuild_candidate(const Sketch& source, const std::vector<std::string>& originals,
                  const std::vector<SketchModifyCurve>& replacements,
                  const std::vector<std::string>& explicitly_removed_profiles = {}) {
  std::vector<std::string> removed(originals.begin(), originals.end());
  const auto is_removed = [&removed](const SketchEntityId& id) {
    return std::find(removed.begin(), removed.end(), id.value()) != removed.end();
  };
  const auto profile_is_removed = [&](const ProfileId& id) {
    return std::find(explicitly_removed_profiles.begin(), explicitly_removed_profiles.end(),
                     id.value()) != explicitly_removed_profiles.end();
  };

  auto candidate = Sketch::create(source.id(), source.name(), source.workplane());
  if (candidate.has_error())
    return candidate;
  const auto add_curve = [&candidate](const SketchModifyCurve& curve) -> Result<std::size_t> {
    return std::visit(
        [&candidate](const auto& value) { return candidate.value().add_entity(value); }, curve);
  };

  for (const auto& line : source.line_segments())
    if (!is_removed(line.id())) {
      auto added = candidate.value().add_entity(line);
      if (added.has_error())
        return Result<Sketch>::failure(added.error());
    }
  for (const auto& arc : source.arc_segments())
    if (!is_removed(arc.id())) {
      auto added = candidate.value().add_entity(arc);
      if (added.has_error())
        return Result<Sketch>::failure(added.error());
    }
  for (const auto& spline : source.spline_segments())
    if (!is_removed(spline.id())) {
      auto added = candidate.value().add_entity(spline);
      if (added.has_error())
        return Result<Sketch>::failure(added.error());
    }
  for (const auto& replacement : replacements) {
    auto added = add_curve(replacement);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& point : source.points()) {
    auto added = candidate.value().add_entity(point);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& point : source.projected_points()) {
    auto added = candidate.value().add_reference(point);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& line : source.projected_lines()) {
    auto added = candidate.value().add_reference(line);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& line : source.reference_generated_lines()) {
    auto added = candidate.value().add_reference(line);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint : source.constraints()) {
    auto added = candidate.value().add_constraint(constraint);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint : source.geometric_constraints()) {
    auto added = candidate.value().add_constraint(constraint);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& dimension : source.driving_dimensions()) {
    auto added = candidate.value().add_dimension(dimension);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& operation : source.trim_extend_operations()) {
    auto added = candidate.value().add_trim_extend_operation(operation);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& continuity : source.tangent_continuities()) {
    auto added = candidate.value().add_tangent_continuity(continuity);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.rectangle_profiles()) {
    if (profile_is_removed(profile.id()))
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.circle_profiles()) {
    if (profile_is_removed(profile.id()))
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.closed_profiles()) {
    if (profile_is_removed(profile.id()) || contour_contains_any(profile.line_segments(), removed))
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.arc_closed_profiles()) {
    if (profile_is_removed(profile.id()) || contour_contains_any(profile.curve_segments(), removed))
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.composite_closed_profiles()) {
    bool affected =
        profile_is_removed(profile.id()) || contour_contains_any(profile.outer_contour(), removed);
    for (const auto& contour : profile.inner_contours())
      affected = affected || contour_contains_any(contour, removed);
    if (affected)
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.circular_hole_patterns()) {
    if (profile_is_removed(profile.id()))
      continue;
    auto added = candidate.value().add_profile(profile);
    if (added.has_error())
      return Result<Sketch>::failure(added.error());
  }
  return candidate;
}

// True when `id` participates in any ordered profile contour.
[[nodiscard]] inline bool entity_in_profile(const Sketch& sketch, const SketchEntityId& id) {
  const auto in = [&id](const std::vector<SketchEntityId>& contour) {
    return std::find(contour.begin(), contour.end(), id) != contour.end();
  };
  for (const auto& profile : sketch.closed_profiles())
    if (in(profile.line_segments()))
      return true;
  for (const auto& profile : sketch.arc_closed_profiles())
    if (in(profile.curve_segments()))
      return true;
  for (const auto& profile : sketch.composite_closed_profiles()) {
    if (in(profile.outer_contour()))
      return true;
    for (const auto& contour : profile.inner_contours())
      if (in(contour))
        return true;
  }
  return false;
}

// Names the first intent that references `id` through a way the caller marks as
// invalidated (removed/split source). Returns an empty string when none does.
[[nodiscard]] inline std::string invalidated_reference(const Sketch& sketch,
                                                       const SketchEntityId& id) {
  const auto references = [&id](const SketchReferenceTarget& target) {
    return target.entity() == id;
  };
  for (const auto& constraint : sketch.constraints())
    if (references(constraint.constrained_target()) || references(constraint.reference_target()))
      return "constraint " + constraint.id().value();
  for (const auto& constraint : sketch.geometric_constraints())
    if (references(constraint.first_target()) ||
        (constraint.second_target() && references(*constraint.second_target())))
      return "constraint " + constraint.id().value();
  for (const auto& dimension : sketch.driving_dimensions())
    if (references(dimension.first_target()) || references(dimension.second_target()))
      return "dimension " + dimension.id().value();
  for (const auto& continuity : sketch.tangent_continuities())
    if (continuity.first_entity() == id || continuity.second_entity() == id)
      return "tangent continuity " + continuity.id().value();
  return {};
}

[[nodiscard]] inline Result<CurveSampler> circle_sampler(const PartDocument& document,
                                                         const CircleProfile& profile) {
  const Parameter* diameter = document.find_parameter(profile.diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length)
    return Result<CurveSampler>::failure(Error::validation(
        profile.id().value(), "circle trim requires an existing Length diameter parameter"));
  const double radius = diameter->value().millimeters() * 0.5;
  if (!std::isfinite(radius) || radius <= kSketchModifyIdentityTolerance)
    return Result<CurveSampler>::failure(
        Error::validation(profile.id().value(), "circle trim requires a positive diameter"));
  return Result<CurveSampler>::success(
      CurveSampler{CurveSampler::Kind::Arc,
                   {},
                   {},
                   ArcGeometry{profile.center(), radius, 0.0, 2.0 * kSketchModifyPi},
                   {}});
}

} // namespace detail

class SketchModifyService {
public:
  // Removes the picked portion of a line/arc/spline bounded by its nearest
  // intersections with other explicit curves. Shortening keeps the id; a
  // removed middle splits into two; no bounding intersection removes it.
  // Materialized profile containers owning the changed curve are omitted from
  // the result, while all their remaining constituent curves are preserved.
  [[nodiscard]] static Result<SketchModifyResult> trim(const Sketch& sketch, SketchEntityId target,
                                                       Point2 pick) {
    return trim_entity(sketch, std::move(target), pick, {});
  }

  // Document-aware entity trim additionally treats every parametric
  // CircleProfile in the Sketch as an analytic cutting curve.
  [[nodiscard]] static Result<SketchModifyResult>
  trim(const PartDocument& document, const Sketch& sketch, SketchEntityId target, Point2 pick) {
    auto circles = circle_samplers(document, sketch, std::nullopt);
    if (circles.has_error())
      return Result<SketchModifyResult>::failure(circles.error());
    return trim_entity(sketch, std::move(target), pick, std::move(circles.value()));
  }

  // Trims a persistent full circle. With two or more distinct intersections,
  // the selected circular portion is removed and the complementary span is
  // materialized as one exact ArcSegment. With fewer than two intersections the
  // whole circle is removed, matching trim's open-curve no-boundary behavior.
  // In either case the CircleProfile container is invalidated.
  [[nodiscard]] static Result<SketchModifyResult>
  trim(const PartDocument& document, const Sketch& sketch, ProfileId target, Point2 pick) {
    return trim_circle_profile(document, sketch, std::move(target), pick);
  }

  // Extends the endpoint nearest the pick along the entity's support to its
  // nearest intersection beyond that endpoint. Keeps the id in place.
  [[nodiscard]] static Result<SketchModifyResult> extend(const Sketch& sketch,
                                                         SketchEntityId target, Point2 pick) {
    using namespace detail;
    if (!sketch_modify_finite(pick))
      return fail(target.value(), "extend pick coordinates must be finite");
    auto sampler = curve_sampler_for(sketch, target);
    if (!sampler)
      return fail(target.value(), "extend target must be an existing line, arc or spline entity");
    const Point2 start = sampler_point(*sampler, 0.0);
    const Point2 end = sampler_point(*sampler, 1.0);
    const bool extend_end =
        sketch_modify_distance(pick, end) <= sketch_modify_distance(pick, start);

    if (sampler->kind == CurveSampler::Kind::Spline) {
      // Continue the same cubic polynomial, preserving its original interval.
      // Bound the search explicitly instead of replacing the curve by a line.
      std::optional<double> closest;
      for (double reach = 1.0; reach <= 64.0 && !closest; reach *= 2.0) {
        const double from = extend_end ? 1.0 : -reach;
        const double to = extend_end ? 1.0 + reach : 0.0;
        const auto continued = sampler->spline.curve().interval(from, to);
        if (!continued.valid()) break; // rational continuation cannot cross a pole
        const CurveSampler continuation{CurveSampler::Kind::Spline, {}, {}, {},
            {continued.poles[0], continued.poles[1], continued.poles[2], continued.poles[3],
             continued.weights}};
        for (const auto& other : other_samplers(sketch, target)) {
          for (const auto point : curve_intersections(continuation, other.sampler)) {
            auto local = sampler_project(continuation, point);
            if (!local) continue;
            const double t = from + (to - from) * *local;
            if ((extend_end && t <= 1.0 + kSketchModifyParamTolerance) ||
                (!extend_end && t >= -kSketchModifyParamTolerance)) continue;
            if (!closest || (extend_end ? t < *closest : t > *closest)) closest = t;
          }
        }
      }
      if (!closest) return fail(target.value(), "spline continuation found no reachable intersection");
      auto extended = subcurve(sketch, target, extend_end ? 0.0 : *closest,
                               extend_end ? *closest : 1.0, target);
      if (!extended) return Result<SketchModifyResult>::failure(extended.error());
      return build(sketch, {target.value()}, {std::move(extended.value())}, {}, {}, {},
                   {{target, extend_end ? SketchModifyEndSelector::End : SketchModifyEndSelector::Start}});
    }
    std::optional<double> best;
    for (const auto& other : other_samplers(sketch, target)) {
      for (const auto candidate :
           curve_intersections_unbounded(*sampler, other.sampler, other.id, sketch)) {
        const auto raw = support_param(*sampler, candidate);
        if (!raw)
          continue;
        const double t = *raw;
        if (extend_end && t > 1.0 + kSketchModifyParamTolerance) {
          if (!best || t < *best)
            best = t;
        } else if (!extend_end && t < -kSketchModifyParamTolerance) {
          if (!best || t > *best)
            best = t;
        }
      }
    }
    if (!best)
      return fail(target.value(), "extend found no reachable intersection beyond the endpoint");
    const Point2 moved = support_point(*sampler, *best);
    auto rebuilt = replace_endpoint(sketch, target, extend_end, moved);
    if (rebuilt.has_error())
      return Result<SketchModifyResult>::failure(rebuilt.error());
    return build(sketch, {target.value()}, {std::move(rebuilt.value())}, {});
  }

  // Splits a line/arc/spline at the projected pick into two entities. The first
  // keeps the id; the second gets a fresh id. References to the id are invalid.
  [[nodiscard]] static Result<SketchModifyResult> split(const Sketch& sketch, SketchEntityId target,
                                                        Point2 point) {
    using namespace detail;
    if (!sketch_modify_finite(point))
      return fail(target.value(), "split point coordinates must be finite");
    if (const std::string reference = invalidated_reference(sketch, target); !reference.empty())
      return fail(target.value(), "split would invalidate " + reference);

    if (const auto* spline = sketch.find_spline_segment(target)) {
      const auto t = spline_param(*spline, point);
      if (!t)
        return fail(target.value(), "split point must lie on the spline");
      auto halves = split_spline(*spline, *t, split_id(sketch, target));
      if (halves.has_error())
        return Result<SketchModifyResult>::failure(halves.error());
      auto result = build(sketch, {target.value()}, std::move(halves.value()), {target.value()});
      return restore_edited_profiles(sketch, std::move(result), target, split_id(sketch, target));
    }
    auto sampler = curve_sampler_for(sketch, target);
    if (!sampler)
      return fail(target.value(), "split target must be an existing line, arc, or spline entity");
    const auto t = sampler_project(*sampler, point);
    if (!t || *t <= kSketchModifyParamTolerance || *t >= 1.0 - kSketchModifyParamTolerance)
      return fail(target.value(), "split point must lie strictly inside the target curve");
    auto first = subcurve(sketch, target, 0.0, *t, target);
    if (first.has_error())
      return Result<SketchModifyResult>::failure(first.error());
    auto second = subcurve(sketch, target, *t, 1.0, split_id(sketch, target));
    if (second.has_error())
      return Result<SketchModifyResult>::failure(second.error());
    auto result = build(sketch, {target.value()},
                        {std::move(first.value()), std::move(second.value())}, {target.value()});
    return restore_edited_profiles(sketch, std::move(result), target, split_id(sketch, target));
  }

  // Rounds the corner between two lines with a tangent arc of `radius`. When
  // `keep_trim` is true both lines are shortened to the tangent points.
  [[nodiscard]] static Result<SketchModifyResult> fillet(const Sketch& sketch, SketchEntityId first,
                                                         SketchEntityId second, double radius,
                                                         bool keep_trim = true) {
    return corner(sketch, first, second, radius, CornerKind::Fillet, keep_trim);
  }

  // Cuts the corner between two lines with a chamfer line at `distance` from the
  // corner along each line. When `keep_trim` is true both lines are shortened.
  [[nodiscard]] static Result<SketchModifyResult> chamfer(const Sketch& sketch,
                                                          SketchEntityId first,
                                                          SketchEntityId second, double distance,
                                                          bool keep_trim = true) {
    return corner(sketch, first, second, distance, CornerKind::Chamfer, keep_trim);
  }

private:
  enum class CornerKind { Fillet, Chamfer };

  struct NamedSampler {
    SketchEntityId id;
    detail::CurveSampler sampler;
  };

  struct TrimCutterSampler {
    std::string topology_entity;
    detail::CurveSampler sampler;
    bool has_endpoints{false};
  };

  struct TrimCut {
    double target_parameter{0.0};
    std::string cutter_entity;
    std::optional<SketchModifyEndSelector> cutter_endpoint;
  };

  [[nodiscard]] static Result<SketchModifyResult> fail(std::string object_id, std::string message) {
    return Result<SketchModifyResult>::failure(
        Error::validation(std::move(object_id), std::move(message)));
  }

  [[nodiscard]] static Result<std::vector<TrimCutterSampler>>
  circle_samplers(const PartDocument& document, const Sketch& sketch,
                  std::optional<ProfileId> excluded) {
    std::vector<TrimCutterSampler> result;
    result.reserve(sketch.circle_profiles().size());
    for (const auto& circle : sketch.circle_profiles()) {
      if (excluded && circle.id() == *excluded)
        continue;
      auto sampler = detail::circle_sampler(document, circle);
      if (sampler.has_error())
        return Result<std::vector<TrimCutterSampler>>::failure(sampler.error());
      result.push_back(
          {"profile/" + circle.id().value(), std::move(sampler.value()), false});
    }
    return Result<std::vector<TrimCutterSampler>>::success(std::move(result));
  }

  [[nodiscard]] static Result<SketchModifyResult>
  trim_entity(const Sketch& sketch, SketchEntityId target, Point2 pick,
              std::vector<TrimCutterSampler> additional_cutters) {
    using namespace detail;
    if (!sketch_modify_finite(pick))
      return fail(target.value(), "trim pick coordinates must be finite");
    auto sampler = curve_sampler_for(sketch, target);
    if (!sampler)
      return fail(target.value(), "trim target must be an existing line, arc, or spline entity");

    std::vector<TrimCut> cuts = intersection_cuts(sketch, target, *sampler, additional_cutters);
    const auto pick_param = sampler_project(*sampler, pick);
    if (!pick_param || *pick_param < -kSketchModifyParamTolerance ||
        *pick_param > 1.0 + kSketchModifyParamTolerance)
      return fail(target.value(), "trim pick must lie on the target curve");
    const double tp = std::clamp(*pick_param, 0.0, 1.0);

    const TrimCut* lower_cut = nullptr;
    const TrimCut* upper_cut = nullptr;
    double lower = 0.0;
    double upper = 1.0;
    for (const TrimCut& cut : cuts) {
      if (cut.target_parameter <= tp + kSketchModifyParamTolerance &&
          cut.target_parameter >= lower) {
        lower = cut.target_parameter;
        lower_cut = cut.target_parameter > kSketchModifyParamTolerance ? &cut : nullptr;
      }
    }
    for (auto iterator = cuts.rbegin(); iterator != cuts.rend(); ++iterator) {
      const TrimCut& cut = *iterator;
      if (cut.target_parameter >= tp - kSketchModifyParamTolerance &&
          cut.target_parameter <= upper) {
        upper = cut.target_parameter;
        upper_cut = cut.target_parameter < 1.0 - kSketchModifyParamTolerance ? &cut : nullptr;
      }
    }

    if (lower_cut == nullptr && upper_cut == nullptr)
      return build(sketch, {target.value()}, {}, {target.value()});
    if (lower_cut != nullptr && upper_cut != nullptr) {
      auto first = subcurve(sketch, target, 0.0, lower, target);
      if (first.has_error())
        return Result<SketchModifyResult>::failure(first.error());
      const SketchEntityId second_id = split_id(sketch, target);
      auto second = subcurve(sketch, target, upper, 1.0, second_id);
      if (second.has_error())
        return Result<SketchModifyResult>::failure(second.error());
      std::vector<SketchModifyTrimAttachment> attachments{
          {target, SketchModifyEndSelector::End, lower_cut->cutter_entity,
           lower_cut->cutter_endpoint},
          {second_id, SketchModifyEndSelector::Start, upper_cut->cutter_entity,
           upper_cut->cutter_endpoint}};
      return build(sketch, {target.value()}, {std::move(first.value()), std::move(second.value())},
                   {target.value()}, {}, std::move(attachments));
    }
    const bool moved_start = upper_cut != nullptr;
    const TrimCut& boundary = moved_start ? *upper_cut : *lower_cut;
    const double keep_lo = moved_start ? upper : 0.0;
    const double keep_hi = moved_start ? 1.0 : lower;
    auto kept = subcurve(sketch, target, keep_lo, keep_hi, target);
    if (kept.has_error())
      return Result<SketchModifyResult>::failure(kept.error());
    const SketchModifyEndSelector endpoint =
        moved_start ? SketchModifyEndSelector::Start : SketchModifyEndSelector::End;
    return build(sketch, {target.value()}, {std::move(kept.value())}, {}, {},
                 {{target, endpoint, boundary.cutter_entity, boundary.cutter_endpoint}},
                 {{target, endpoint}});
  }

  [[nodiscard]] static SketchEntityId circle_trim_id(const Sketch& sketch,
                                                     const ProfileId& profile) {
    const std::string prefix = "arc.trim." + profile.value();
    for (std::size_t index = 0U;; ++index) {
      const std::string suffix = index == 0U ? std::string{} : "." + std::to_string(index);
      SketchEntityId candidate(prefix + suffix);
      if (sketch.find_line_segment(candidate) == nullptr &&
          sketch.find_arc_segment(candidate) == nullptr &&
          sketch.find_spline_segment(candidate) == nullptr)
        return candidate;
    }
  }

  [[nodiscard]] static Result<SketchModifyResult> trim_circle_profile(const PartDocument& document,
                                                                      const Sketch& sketch,
                                                                      ProfileId target,
                                                                      Point2 pick) {
    using namespace detail;
    if (!sketch_modify_finite(pick))
      return fail(target.value(), "trim pick coordinates must be finite");
    const CircleProfile* circle = sketch.find_circle_profile(target);
    if (circle == nullptr)
      return fail(target.value(), "profile trim target must be an existing circle profile");
    auto target_sampler = circle_sampler(document, *circle);
    if (target_sampler.has_error())
      return Result<SketchModifyResult>::failure(target_sampler.error());

    const double picked_radius = sketch_modify_distance(circle->center(), pick);
    // GUI circle hits are projected onto a 72-segment presentation polyline.
    // Its maximum radial sagitta is just under 0.1% of the radius, so accept a
    // small representation allowance while still rejecting center/far picks.
    const double radius_tolerance =
        kSketchModifyPointTolerance + 2.0e-3 * target_sampler.value().arc.radius;
    if (std::abs(picked_radius - target_sampler.value().arc.radius) > radius_tolerance)
      return fail(target.value(), "trim pick must lie on the target circle");
    const auto picked = sampler_project(target_sampler.value(), pick);
    if (!picked)
      return fail(target.value(), "trim pick must lie on the target circle");

    std::vector<TrimCut> cuts;
    const auto append_cuts = [&](const TrimCutterSampler& cutter) {
      for (const Point2 point : curve_intersections(target_sampler.value(), cutter.sampler)) {
        const auto parameter = sampler_project(target_sampler.value(), point);
        if (!parameter)
          continue;
        double normalized =
            sketch_modify_wrap_two_pi(*parameter * 2.0 * kSketchModifyPi) /
            (2.0 * kSketchModifyPi);
        if (normalized >= 1.0 - kSketchModifyParamTolerance)
          normalized = 0.0;
        std::optional<SketchModifyEndSelector> cutter_endpoint;
        if (cutter.has_endpoints) {
          const auto cutter_parameter = sampler_project(cutter.sampler, point);
          if (cutter_parameter && std::abs(*cutter_parameter) <= kSketchModifyParamTolerance)
            cutter_endpoint = SketchModifyEndSelector::Start;
          else if (cutter_parameter &&
                   std::abs(*cutter_parameter - 1.0) <= kSketchModifyParamTolerance)
            cutter_endpoint = SketchModifyEndSelector::End;
        }
        cuts.push_back({normalized, cutter.topology_entity, std::move(cutter_endpoint)});
      }
    };
    for (const auto& other : other_samplers(sketch, SketchEntityId("__circle_profile_target__")))
      append_cuts({"entity/" + other.id.value(), other.sampler, true});
    auto other_circles = circle_samplers(document, sketch, target);
    if (other_circles.has_error())
      return Result<SketchModifyResult>::failure(other_circles.error());
    for (const auto& other : other_circles.value())
      append_cuts(other);
    std::sort(cuts.begin(), cuts.end(), [](const TrimCut& first, const TrimCut& second) {
      if (first.target_parameter != second.target_parameter)
        return first.target_parameter < second.target_parameter;
      return prefer_trim_cut(first, second);
    });
    std::vector<TrimCut> unique_cuts;
    unique_cuts.reserve(cuts.size());
    for (TrimCut& cut : cuts) {
      if (unique_cuts.empty() ||
          std::abs(unique_cuts.back().target_parameter - cut.target_parameter) >
              kSketchModifyParamTolerance) {
        unique_cuts.push_back(std::move(cut));
      } else if (prefer_trim_cut(cut, unique_cuts.back())) {
        unique_cuts.back() = std::move(cut);
      }
    }
    cuts = std::move(unique_cuts);
    if (cuts.size() < 2U)
      return build(sketch, {}, {}, {}, {target.value()});

    double pick_parameter = *picked;
    if (pick_parameter >= 1.0 - kSketchModifyParamTolerance)
      pick_parameter = 0.0;

    auto upper = std::upper_bound(
        cuts.begin(), cuts.end(), pick_parameter + kSketchModifyParamTolerance,
        [](double parameter, const TrimCut& cut) { return parameter < cut.target_parameter; });
    const TrimCut& upper_cut = upper == cuts.end() ? cuts.front() : *upper;
    const TrimCut& lower_cut =
        upper == cuts.begin() || upper == cuts.end() ? cuts.back() : *(upper - 1);
    const double upper_parameter =
        upper == cuts.end() ? upper_cut.target_parameter + 1.0 : upper_cut.target_parameter;
    const double lower_parameter = lower_cut.target_parameter;
    double removed_span = upper_parameter - lower_parameter;
    if (removed_span <= 0.0)
      removed_span += 1.0;
    const double kept_span = 1.0 - removed_span;
    if (kept_span <= kSketchModifyParamTolerance)
      return fail(target.value(), "circle trim would leave a degenerate arc");
    const double kept_start = std::fmod(upper_parameter, 1.0);
    const double kept_mid = std::fmod(kept_start + kept_span * 0.5, 1.0);
    const double kept_end = std::fmod(kept_start + kept_span, 1.0);
    const SketchEntityId result_id = circle_trim_id(sketch, target);
    auto arc = ArcSegment::create_three_point(result_id,
                                              sampler_point(target_sampler.value(), kept_start),
                                              sampler_point(target_sampler.value(), kept_mid),
                                              sampler_point(target_sampler.value(), kept_end));
    if (arc.has_error())
      return Result<SketchModifyResult>::failure(arc.error());
    return build(sketch, {},
                 {SketchModifyCurve(arc.value().with_construction(circle->construction()))}, {},
                 {target.value()},
                 {{result_id, SketchModifyEndSelector::Start, upper_cut.cutter_entity,
                   upper_cut.cutter_endpoint},
                  {result_id, SketchModifyEndSelector::End, lower_cut.cutter_entity,
                   lower_cut.cutter_endpoint}});
  }

  [[nodiscard]] static Result<SketchModifyResult>
  build(const Sketch& sketch, std::vector<std::string> originals,
        std::vector<detail::SketchModifyCurve> replacements, std::vector<std::string> invalidated,
        std::vector<std::string> explicitly_invalidated_profiles = {},
        std::vector<SketchModifyTrimAttachment> attachments = {},
        std::vector<SketchModifyInvalidatedEndpoint> invalidated_endpoints = {}) {
    for (const auto& id : invalidated)
      if (const std::string reference = detail::invalidated_reference(sketch, SketchEntityId(id));
          !reference.empty())
        return fail(id, "modification would invalidate " + reference);
    std::vector<std::string> invalidated_profiles = detail::affected_profile_ids(sketch, originals);
    invalidated_profiles.insert(invalidated_profiles.end(), explicitly_invalidated_profiles.begin(),
                                explicitly_invalidated_profiles.end());
    std::sort(invalidated_profiles.begin(), invalidated_profiles.end());
    invalidated_profiles.erase(
        std::unique(invalidated_profiles.begin(), invalidated_profiles.end()),
        invalidated_profiles.end());
    auto candidate =
        detail::rebuild_candidate(sketch, originals, replacements, invalidated_profiles);
    if (candidate.has_error())
      return Result<SketchModifyResult>::failure(candidate.error());
    return Result<SketchModifyResult>::success({std::move(candidate.value()),
                                                std::move(invalidated),
                                                std::move(invalidated_profiles),
                                                std::move(attachments),
                                                std::move(invalidated_endpoints)});
  }

  // Split inserts a second curve after its source. Corner edits splice a
  // connector between adjacent members and promote polygon profiles to curves.
  // Profile ids stay stable so downstream feature bindings remain meaningful.
  [[nodiscard]] static Result<SketchModifyResult>
  restore_edited_profiles(const Sketch& source, Result<SketchModifyResult> result,
                          const SketchEntityId& first, const SketchEntityId& inserted,
                          std::optional<SketchEntityId> second = std::nullopt) {
    if (!result) return result;
    auto& edited = result.value();
    const auto rewrite = [&](const std::vector<SketchEntityId>& contour)
        -> Result<std::vector<SketchEntityId>> {
      const bool has_first = std::find(contour.begin(), contour.end(), first) != contour.end();
      const bool has_second = second && std::find(contour.begin(), contour.end(), *second) != contour.end();
      if (!has_first && !has_second)
        return Result<std::vector<SketchEntityId>>::success(contour);
      if (second && (!has_first || !has_second))
        return Result<std::vector<SketchEntityId>>::failure(Error::validation(
            first.value(), "corner targets must be adjacent members of the same profile contour"));
      std::vector<SketchEntityId> changed;
      bool spliced = false;
      for (std::size_t i = 0; i < contour.size(); ++i) {
        changed.push_back(contour[i]);
        const auto& next = contour[(i + 1U) % contour.size()];
        const bool insert = second
            ? ((contour[i] == first && next == *second) ||
               (contour[i] == *second && next == first))
            : contour[i] == first;
        if (!insert) continue;
        auto previous = detail::curve_sampler_for(edited.sketch, contour[i]);
        auto connector = detail::curve_sampler_for(edited.sketch, inserted);
        if (!previous || !connector)
          return Result<std::vector<SketchEntityId>>::failure(
              Error::validation(inserted.value(), "profile rewrite lost a source curve"));
        const Point2 join = detail::sampler_point(*previous, 1.0);
        if (detail::sketch_modify_distance(join, detail::sampler_point(*connector, 0.0)) >
            detail::kSketchModifyPointTolerance) {
          if (detail::sketch_modify_distance(join, detail::sampler_point(*connector, 1.0)) >
              detail::kSketchModifyPointTolerance)
            return Result<std::vector<SketchEntityId>>::failure(
                Error::validation(inserted.value(), "profile connector does not meet its predecessor"));
          auto reversed = subcurve(edited.sketch, inserted, 1.0, 0.0, inserted);
          if (!reversed) return Result<std::vector<SketchEntityId>>::failure(reversed.error());
          auto replacement = detail::rebuild_candidate(edited.sketch, {inserted.value()}, {reversed.value()});
          if (!replacement) return Result<std::vector<SketchEntityId>>::failure(replacement.error());
          edited.sketch = std::move(replacement.value());
        }
        changed.push_back(inserted);
        spliced = true;
      }
      if (!spliced)
        return Result<std::vector<SketchEntityId>>::failure(
            Error::validation(first.value(), "corner targets are not adjacent in the profile"));
      return Result<std::vector<SketchEntityId>>::success(std::move(changed));
    };
    const auto affected = [&](const ProfileId& id) {
      return std::find(edited.invalidated_profile_ids.begin(), edited.invalidated_profile_ids.end(),
                       id.value()) != edited.invalidated_profile_ids.end();
    };
    const auto restored = [&](const ProfileId& id) {
      std::erase(edited.invalidated_profile_ids, id.value());
    };
    for (const auto& profile : source.closed_profiles()) {
      if (!affected(profile.id())) continue;
      auto contour = rewrite(profile.line_segments());
      if (!contour) return Result<SketchModifyResult>::failure(contour.error());
      const bool curved = std::any_of(contour.value().begin(), contour.value().end(),
          [&](const auto& id) { return edited.sketch.find_line_segment(id) == nullptr; });
      Result<std::size_t> added = [&]() {
        if (curved) {
          auto replacement = ArcClosedProfile::create(profile.id(), contour.value());
          if (!replacement) return Result<std::size_t>::failure(replacement.error());
          return edited.sketch.add_profile(replacement.value());
        }
        auto replacement = ClosedProfile::create(profile.id(), contour.value());
        if (!replacement) return Result<std::size_t>::failure(replacement.error());
        return edited.sketch.add_profile(replacement.value());
      }();
      if (!added) return Result<SketchModifyResult>::failure(added.error());
      restored(profile.id());
    }
    for (const auto& profile : source.arc_closed_profiles()) {
      if (!affected(profile.id())) continue;
      auto contour = rewrite(profile.curve_segments());
      if (!contour) return Result<SketchModifyResult>::failure(contour.error());
      auto replacement = ArcClosedProfile::create(profile.id(), contour.value());
      if (!replacement) return Result<SketchModifyResult>::failure(replacement.error());
      auto added = edited.sketch.add_profile(replacement.value());
      if (!added) return Result<SketchModifyResult>::failure(added.error());
      restored(profile.id());
    }
    for (const auto& profile : source.composite_closed_profiles()) {
      if (!affected(profile.id())) continue;
      auto outer = rewrite(profile.outer_contour());
      if (!outer) return Result<SketchModifyResult>::failure(outer.error());
      std::vector<std::vector<SketchEntityId>> inner;
      for (const auto& original : profile.inner_contours()) {
        auto contour = rewrite(original);
        if (!contour) return Result<SketchModifyResult>::failure(contour.error());
        inner.push_back(std::move(contour.value()));
      }
      auto replacement = CompositeClosedProfile::create(profile.id(), std::move(outer.value()),
                                                         std::move(inner));
      if (!replacement) return Result<SketchModifyResult>::failure(replacement.error());
      auto added = edited.sketch.add_profile(replacement.value());
      if (!added) return Result<SketchModifyResult>::failure(added.error());
      restored(profile.id());
    }
    return result;
  }

  [[nodiscard]] static SketchEntityId split_id(const Sketch& sketch, const SketchEntityId& base) {
    for (std::size_t index = 1U;; ++index) {
      SketchEntityId candidate(base.value() + ".split." + std::to_string(index));
      if (sketch.find_line_segment(candidate) == nullptr &&
          sketch.find_arc_segment(candidate) == nullptr &&
          sketch.find_spline_segment(candidate) == nullptr)
        return candidate;
    }
  }

  [[nodiscard]] static SketchEntityId corner_id(const Sketch& sketch, std::string_view prefix) {
    for (std::size_t index = 1U;; ++index) {
      SketchEntityId candidate(std::string(prefix) + std::to_string(index));
      if (sketch.find_line_segment(candidate) == nullptr &&
          sketch.find_arc_segment(candidate) == nullptr &&
          sketch.find_spline_segment(candidate) == nullptr)
        return candidate;
    }
  }

  [[nodiscard]] static bool prefer_trim_cut(const TrimCut& candidate,
                                            const TrimCut& current) noexcept {
    if (candidate.cutter_endpoint.has_value() != current.cutter_endpoint.has_value())
      return candidate.cutter_endpoint.has_value();
    if (candidate.cutter_entity != current.cutter_entity)
      return candidate.cutter_entity < current.cutter_entity;
    return static_cast<int>(candidate.cutter_endpoint.value_or(SketchModifyEndSelector::End)) <
           static_cast<int>(current.cutter_endpoint.value_or(SketchModifyEndSelector::End));
  }

  // All intersections of the target curve with other explicit curves and any
  // document-resolved analytic cutters.  Preserve the chosen cutter identity
  // so the GUI can author the matching persistent relation after the rewrite.
  [[nodiscard]] static std::vector<TrimCut>
  intersection_cuts(const Sketch& sketch, const SketchEntityId& target,
                    const detail::CurveSampler& sampler,
                    const std::vector<TrimCutterSampler>& additional_cutters = {}) {
    std::vector<TrimCut> cuts;
    const auto append = [&](const TrimCutterSampler& cutter) {
      for (const Point2 point : detail::curve_intersections(sampler, cutter.sampler)) {
        const auto target_parameter = detail::sampler_project(sampler, point);
        if (!target_parameter ||
            *target_parameter <= detail::kSketchModifyParamTolerance ||
            *target_parameter >= 1.0 - detail::kSketchModifyParamTolerance)
          continue;
        std::optional<SketchModifyEndSelector> cutter_endpoint;
        if (cutter.has_endpoints) {
          const auto cutter_parameter = detail::sampler_project(cutter.sampler, point);
          if (cutter_parameter &&
              std::abs(*cutter_parameter) <= detail::kSketchModifyParamTolerance)
            cutter_endpoint = SketchModifyEndSelector::Start;
          else if (cutter_parameter &&
                   std::abs(*cutter_parameter - 1.0) <= detail::kSketchModifyParamTolerance)
            cutter_endpoint = SketchModifyEndSelector::End;
        }
        cuts.push_back(
            {*target_parameter, cutter.topology_entity, std::move(cutter_endpoint)});
      }
    };
    for (const NamedSampler& other : other_samplers(sketch, target))
      append({"entity/" + other.id.value(), other.sampler, true});
    for (const TrimCutterSampler& other : additional_cutters)
      append(other);

    std::sort(cuts.begin(), cuts.end(), [](const TrimCut& first, const TrimCut& second) {
      if (first.target_parameter != second.target_parameter)
        return first.target_parameter < second.target_parameter;
      return prefer_trim_cut(first, second);
    });
    std::vector<TrimCut> unique;
    unique.reserve(cuts.size());
    for (TrimCut& cut : cuts) {
      if (unique.empty() ||
          std::abs(unique.back().target_parameter - cut.target_parameter) >
              detail::kSketchModifyParamTolerance) {
        unique.push_back(std::move(cut));
      } else if (prefer_trim_cut(cut, unique.back())) {
        unique.back() = std::move(cut);
      }
    }
    return unique;
  }

  [[nodiscard]] static std::vector<NamedSampler> other_samplers(const Sketch& sketch,
                                                                const SketchEntityId& target) {
    std::vector<NamedSampler> result;
    for (const auto& line : sketch.line_segments())
      if (line.id() != target)
        if (auto sampler = detail::line_sampler(line))
          result.push_back({line.id(), *sampler});
    for (const auto& arc : sketch.arc_segments())
      if (arc.id() != target)
        if (auto sampler = detail::arc_sampler(arc))
          result.push_back({arc.id(), *sampler});
    for (const auto& spline : sketch.spline_segments())
      if (spline.id() != target)
        if (auto sampler = detail::spline_sampler(spline))
          result.push_back({spline.id(), *sampler});
    return result;
  }

  // Parameter on the target support (unbounded) with no span restriction.
  [[nodiscard]] static std::optional<double> support_param(const detail::CurveSampler& sampler,
                                                           Point2 point) {
    if (sampler.kind == detail::CurveSampler::Kind::Line)
      return detail::sampler_project(sampler, point);
    const double angle = std::atan2(point.y - sampler.arc.center.y, point.x - sampler.arc.center.x);
    double delta = angle - sampler.arc.start_angle;
    if (sampler.arc.sweep > 0.0)
      return detail::sketch_modify_wrap_two_pi(delta) / sampler.arc.sweep;
    return detail::sketch_modify_wrap_two_pi(-delta) / (-sampler.arc.sweep);
  }

  [[nodiscard]] static Point2 support_point(const detail::CurveSampler& sampler, double t) {
    if (sampler.kind == detail::CurveSampler::Kind::Line)
      return detail::sampler_point(sampler, t);
    return detail::arc_point(sampler.arc, sampler.arc.start_angle + sampler.arc.sweep * t);
  }

  // Intersection points of the target support with another actual curve, used
  // by extend so the target is treated as unbounded.
  [[nodiscard]] static std::vector<Point2>
  curve_intersections_unbounded(const detail::CurveSampler& target,
                                const detail::CurveSampler& other, const SketchEntityId&,
                                const Sketch&) {
    detail::CurveSampler unbounded = target;
    if (target.kind == detail::CurveSampler::Kind::Line) {
      const Point2 s = target.line_start;
      const Point2 e = target.line_end;
      const Point2 direction{e.x - s.x, e.y - s.y};
      unbounded.line_start = {s.x - direction.x * 1000.0, s.y - direction.y * 1000.0};
      unbounded.line_end = {e.x + direction.x * 1000.0, e.y + direction.y * 1000.0};
    } else {
      unbounded.arc.start_angle = 0.0;
      unbounded.arc.sweep = 2.0 * detail::kSketchModifyPi;
    }
    return detail::curve_intersections(unbounded, other);
  }

  [[nodiscard]] static Result<detail::SketchModifyCurve> subcurve(const Sketch& sketch,
                                                                  const SketchEntityId& id,
                                                                  double from, double to,
                                                                  SketchEntityId new_id) {
    using namespace detail;
    if (const auto* line = sketch.find_line_segment(id)) {
      auto sampler = line_sampler(*line);
      auto result = LineSegment::create(new_id, sampler_point(*sampler, from),
                                        sampler_point(*sampler, to), line->role());
      if (result.has_error())
        return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    if (const auto* arc = sketch.find_arc_segment(id)) {
      auto sampler = arc_sampler(*arc);
      if (!sampler)
        return Result<SketchModifyCurve>::failure(
            Error::validation(id.value(), "arc entity has degenerate geometry"));
      auto result = ArcSegment::create_three_point(new_id, sampler_point(*sampler, from),
                                                   sampler_point(*sampler, (from + to) * 0.5),
                                                   sampler_point(*sampler, to));
      if (result.has_error())
        return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(
          result.value().with_construction(arc->construction()));
    }
    if (const auto* spline = sketch.find_spline_segment(id)) {
      const auto curve = spline->curve().interval(from, to);
      auto result = SplineSegment::create_cubic_bezier(std::move(new_id), curve.poles[0],
          curve.poles[1], curve.poles[2], curve.poles[3], curve.weights);
      if (!result) return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(
          result.value().with_construction(spline->construction()));
    }
    return Result<SketchModifyCurve>::failure(
        Error::validation(id.value(), "subcurve target must be a line, arc, or spline"));
  }

  [[nodiscard]] static Result<detail::SketchModifyCurve>
  replace_endpoint(const Sketch& sketch, const SketchEntityId& id, bool replace_end, Point2 moved) {
    using namespace detail;
    if (const auto* line = sketch.find_line_segment(id)) {
      auto result = LineSegment::create(id, replace_end ? line->start() : moved,
                                        replace_end ? moved : line->end(), line->role());
      if (result.has_error())
        return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    if (const auto* arc = sketch.find_arc_segment(id)) {
      auto sampler = arc_sampler(*arc);
      if (!sampler)
        return Result<SketchModifyCurve>::failure(
            Error::validation(id.value(), "arc entity has degenerate geometry"));
      const auto moved_param = support_param(*sampler, moved);
      const double target = moved_param.value_or(replace_end ? 1.0 : 0.0);
      const double lo = replace_end ? 0.0 : target;
      const double hi = replace_end ? target : 1.0;
      auto result = ArcSegment::create_three_point(id, support_point(*sampler, lo),
                                                   support_point(*sampler, (lo + hi) * 0.5),
                                                   support_point(*sampler, hi));
      if (result.has_error())
        return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(
          result.value().with_construction(arc->construction()));
    }
    return Result<SketchModifyCurve>::failure(
        Error::validation(id.value(), "extend target must be a line or arc"));
  }

  [[nodiscard]] static std::optional<double> spline_param(const SplineSegment& spline,
                                                          Point2 point) {
    const auto sampler = detail::spline_sampler(spline);
    const auto t = detail::sampler_project(*sampler, point);
    if (!t || *t <= detail::kSketchModifyParamTolerance ||
        *t >= 1.0 - detail::kSketchModifyParamTolerance) return std::nullopt;
    return t;
  }

  [[nodiscard]] static Result<std::vector<detail::SketchModifyCurve>>
  split_spline(const SplineSegment& spline, double t, SketchEntityId second_id) {
    const auto a = spline.curve().interval(0, t);
    const auto b = spline.curve().interval(t, 1);
    auto first = SplineSegment::create_cubic_bezier(spline.id(), a.poles[0], a.poles[1],
                                                   a.poles[2], a.poles[3], a.weights);
    if (!first) return Result<std::vector<detail::SketchModifyCurve>>::failure(first.error());
    auto second = SplineSegment::create_cubic_bezier(std::move(second_id), b.poles[0], b.poles[1],
                                                    b.poles[2], b.poles[3], b.weights);
    if (!second) return Result<std::vector<detail::SketchModifyCurve>>::failure(second.error());
    return Result<std::vector<detail::SketchModifyCurve>>::success(
        {first.value().with_construction(spline.construction()),
         second.value().with_construction(spline.construction())});
  }

  [[nodiscard]] static double curve_length(const detail::CurveSampler& curve, double from,
                                           double to) {
    const auto speed = [&](double t) {
      const auto derivative = detail::sampler_derivative(curve, t);
      return std::hypot(derivative.x, derivative.y);
    };
    if (from > to) std::swap(from, to);
    const auto integrate = [&](auto&& self, double a, double b, double fa, double fm,
                               double fb, double whole, double tolerance, int depth) -> double {
      const double m = (a + b) * 0.5;
      const double fl = speed((a + m) * 0.5);
      const double fr = speed((m + b) * 0.5);
      const double left = (m - a) * (fa + 4 * fl + fm) / 6;
      const double right = (b - m) * (fm + 4 * fr + fb) / 6;
      if (depth == 0 || std::abs(left + right - whole) <= 15 * tolerance)
        return left + right + (left + right - whole) / 15;
      return self(self, a, m, fa, fl, fm, left, tolerance * 0.5, depth - 1) +
             self(self, m, b, fm, fr, fb, right, tolerance * 0.5, depth - 1);
    };
    const double fa = speed(from), fm = speed((from + to) * 0.5), fb = speed(to);
    const double whole = (to - from) * (fa + 4 * fm + fb) / 6;
    return integrate(integrate, from, to, fa, fm, fb, whole,
                      1.0e-10 * std::max(1.0, whole), 16);
  }

  [[nodiscard]] static Result<SketchModifyResult>
  curved_corner(const Sketch& sketch, SketchEntityId first_id, SketchEntityId second_id,
                double size, CornerKind kind, bool keep_trim) {
    using namespace detail;
    const auto first = curve_sampler_for(sketch, first_id);
    const auto second = curve_sampler_for(sketch, second_id);
    if (!first || !second)
      return fail(first_id.value(), "corner targets must be explicit line, arc or spline entities");
    std::optional<std::pair<double, double>> ends;
    for (double a : {0.0, 1.0})
      for (double b : {0.0, 1.0})
        if (sketch_modify_distance(sampler_point(*first, a), sampler_point(*second, b)) <=
            kSketchModifyPointTolerance) {
          if (ends) return fail(first_id.value(), "corner has more than one shared endpoint");
          ends = {a, b};
        }
    if (!ends) return fail(first_id.value(), "curved corner targets must share one endpoint");
    double first_t = 0, second_t = 0;
    Point2 center{};
    if (kind == CornerKind::Chamfer) {
      const auto at_distance = [&](const CurveSampler& curve, double end) -> std::optional<double> {
        if (size >= curve_length(curve, 0, 1) - kSketchModifyPointTolerance) return std::nullopt;
        double lo = 0, hi = 1;
        for (int iteration = 0; iteration < 60; ++iteration) {
          const double fraction = (lo + hi) * 0.5;
          const double t = end == 0 ? fraction : 1 - fraction;
          if (curve_length(curve, end, t) < size) lo = fraction;
          else hi = fraction;
        }
        const double fraction = (lo + hi) * 0.5;
        return end == 0 ? fraction : 1 - fraction;
      };
      const auto a = at_distance(*first, ends->first), b = at_distance(*second, ends->second);
      if (!a || !b) return fail(first_id.value(), "chamfer setback would consume a source curve");
      first_t = *a; second_t = *b;
    } else {
      const auto offset = [size](const CurveSampler& curve, double t, double side)
          -> std::optional<Point2> {
        const auto point = sampler_point(curve, t);
        const auto tangent = sampler_derivative(curve, t);
        const double length = std::hypot(tangent.x, tangent.y);
        if (length <= kSketchModifyIdentityTolerance) return std::nullopt;
        return Point2{point.x - side * size * tangent.y / length,
                      point.y + side * size * tangent.x / length};
      };
      double best = std::numeric_limits<double>::infinity();
      for (double side_a : {-1.0, 1.0}) for (double side_b : {-1.0, 1.0})
        for (double seed_a : {0.01, 0.08, 0.25, 0.6})
          for (double seed_b : {0.01, 0.08, 0.25, 0.6}) {
            double a = ends->first == 0 ? seed_a : 1 - seed_a;
            double b = ends->second == 0 ? seed_b : 1 - seed_b;
            for (int iteration = 0; iteration < 60; ++iteration) {
              const auto ca = offset(*first, a, side_a), cb = offset(*second, b, side_b);
              if (!ca || !cb) break;
              const Point2 residual{ca->x - cb->x, ca->y - cb->y};
              const double error = std::hypot(residual.x, residual.y);
              if (error <= 1.0e-9 * std::max(1.0, size)) {
                const double score = curve_length(*first, ends->first, a) +
                                     curve_length(*second, ends->second, b);
                if (score < best && std::abs(a - ends->first) > 1.0e-6 &&
                    std::abs(b - ends->second) > 1.0e-6 &&
                    sketch_modify_distance(sampler_point(*first, a), sampler_point(*second, b)) >
                        kSketchModifyPointTolerance) {
                  best = score; first_t = a; second_t = b; center = *ca;
                }
                break;
              }
              constexpr double h = 1.0e-6;
              const auto ap = offset(*first, a + h, side_a), am = offset(*first, a - h, side_a);
              const auto bp = offset(*second, b + h, side_b), bm = offset(*second, b - h, side_b);
              if (!ap || !am || !bp || !bm) break;
              const Point2 da{(ap->x - am->x) / (2*h), (ap->y - am->y) / (2*h)};
              const Point2 db{-(bp->x - bm->x) / (2*h), -(bp->y - bm->y) / (2*h)};
              const double determinant = da.x * db.y - da.y * db.x;
              if (std::abs(determinant) <= 1.0e-12) break;
              const double step_a = (residual.x * db.y - residual.y * db.x) / determinant;
              const double step_b = (da.x * residual.y - da.y * residual.x) / determinant;
              bool improved = false;
              for (double scale = 1; scale >= 1.0/128; scale *= 0.5) {
                const double next_a = std::clamp(a - scale * step_a, 1.0e-7, 1 - 1.0e-7);
                const double next_b = std::clamp(b - scale * step_b, 1.0e-7, 1 - 1.0e-7);
                const auto next_ca = offset(*first, next_a, side_a);
                const auto next_cb = offset(*second, next_b, side_b);
                if (next_ca && next_cb && sketch_modify_distance(*next_ca, *next_cb) < error) {
                  a = next_a; b = next_b; improved = true; break;
                }
              }
              if (!improved) break;
            }
          }
      if (!std::isfinite(best))
        return fail(first_id.value(), "no nondegenerate tangent fillet fits the connected curves");
    }
    const Point2 a = sampler_point(*first, first_t), b = sampler_point(*second, second_t);
    const SketchEntityId connector = corner_id(sketch,
        kind == CornerKind::Fillet ? "arc.fillet." : "line.chamfer.");
    std::vector<SketchModifyCurve> replacements;
    if (keep_trim) {
      auto kept_a = subcurve(sketch, first_id, ends->first == 0 ? first_t : 0,
                            ends->first == 0 ? 1 : first_t, first_id);
      auto kept_b = subcurve(sketch, second_id, ends->second == 0 ? second_t : 0,
                            ends->second == 0 ? 1 : second_t, second_id);
      if (!kept_a) return Result<SketchModifyResult>::failure(kept_a.error());
      if (!kept_b) return Result<SketchModifyResult>::failure(kept_b.error());
      replacements.push_back(kept_a.value()); replacements.push_back(kept_b.value());
    }
    if (kind == CornerKind::Fillet) {
      const Point2 direction{(a.x + b.x) * 0.5 - center.x, (a.y + b.y) * 0.5 - center.y};
      const double length = std::hypot(direction.x, direction.y);
      if (length <= kSketchModifyIdentityTolerance)
        return fail(connector.value(), "fillet would have an ambiguous semicircular sweep");
      const Point2 mid{center.x + size * direction.x / length,
                       center.y + size * direction.y / length};
      auto arc = ArcSegment::create_three_point(connector, a, mid, b);
      if (!arc) return Result<SketchModifyResult>::failure(arc.error());
      replacements.push_back(arc.value().with_construction(
          sketch.curve_is_construction(first_id) && sketch.curve_is_construction(second_id)));
    } else {
      auto line = LineSegment::create(connector, a, b,
                                      sketch.curve_is_construction(first_id) &&
                                              sketch.curve_is_construction(second_id)
                                          ? SketchLineRole::Construction
                                          : SketchLineRole::Geometry);
      if (!line) return Result<SketchModifyResult>::failure(line.error());
      replacements.push_back(line.value());
    }
    std::vector<SketchModifyInvalidatedEndpoint> moved;
    if (keep_trim) moved = {{first_id, ends->first == 0 ? SketchModifyEndSelector::Start : SketchModifyEndSelector::End},
                           {second_id, ends->second == 0 ? SketchModifyEndSelector::Start : SketchModifyEndSelector::End}};
    auto result = build(sketch, keep_trim ? std::vector<std::string>{first_id.value(), second_id.value()}
                                         : std::vector<std::string>{},
                        std::move(replacements), {}, {}, {}, std::move(moved));
    if (!keep_trim) return result;
    return restore_edited_profiles(sketch, std::move(result), first_id, connector, second_id);
  }

  [[nodiscard]] static Result<SketchModifyResult>
  corner(const Sketch& sketch, const SketchEntityId& first_id, const SketchEntityId& second_id,
         double size, CornerKind kind, bool keep_trim) {
    using namespace detail;
    if (first_id == second_id)
      return fail(first_id.value(), "fillet/chamfer requires two distinct line entities");
    if (!std::isfinite(size) || size <= kSketchModifyIdentityTolerance)
      return fail(first_id.value(), "fillet radius / chamfer distance must be positive and finite");
    const auto* first = sketch.find_line_segment(first_id);
    const auto* second = sketch.find_line_segment(second_id);
    if (first == nullptr || second == nullptr)
      return curved_corner(sketch, first_id, second_id, size, kind, keep_trim);

    const Point2 p = first->start();
    const Point2 r{first->end().x - p.x, first->end().y - p.y};
    const Point2 q = second->start();
    const Point2 s{second->end().x - q.x, second->end().y - q.y};
    const double denom = r.x * s.y - r.y * s.x;
    if (std::abs(denom) <= kSketchModifyIdentityTolerance)
      return fail(first_id.value(), "fillet/chamfer lines must not be parallel");
    const double tt = ((q.x - p.x) * s.y - (q.y - p.y) * s.x) / denom;
    const Point2 corner{p.x + r.x * tt, p.y + r.y * tt};

    // Direction along each line pointing away from the corner, toward the
    // endpoint that is kept.
    const auto away = [&corner](const LineSegment& line) -> std::optional<Point2> {
      const bool start_is_corner = sketch_modify_distance(line.start(), corner) <=
                                   sketch_modify_distance(line.end(), corner);
      const Point2 far = start_is_corner ? line.end() : line.start();
      const double length = sketch_modify_distance(corner, far);
      if (length <= kSketchModifyIdentityTolerance)
        return std::nullopt;
      return Point2{(far.x - corner.x) / length, (far.y - corner.y) / length};
    };
    const auto first_dir = away(*first);
    const auto second_dir = away(*second);
    if (!first_dir || !second_dir)
      return fail(first_id.value(), "fillet/chamfer lines are degenerate at the corner");

    double first_offset = size;
    double second_offset = size;
    std::optional<ArcSegment> arc;
    std::optional<LineSegment> chamfer_line;
    if (kind == CornerKind::Fillet) {
      const double cos_full = first_dir->x * second_dir->x + first_dir->y * second_dir->y;
      const double half_angle = std::acos(std::clamp(cos_full, -1.0, 1.0)) * 0.5;
      const double tangent = std::tan(half_angle);
      if (tangent <= kSketchModifyIdentityTolerance)
        return fail(first_id.value(), "fillet lines are collinear");
      const double setback = size / tangent;
      first_offset = setback;
      second_offset = setback;
    }
    const double first_available = std::max(sketch_modify_distance(corner, first->start()),
                                            sketch_modify_distance(corner, first->end()));
    const double second_available = std::max(sketch_modify_distance(corner, second->start()),
                                             sketch_modify_distance(corner, second->end()));
    if (keep_trim && (first_offset >= first_available - kSketchModifyPointTolerance ||
                      second_offset >= second_available - kSketchModifyPointTolerance))
      return fail(first_id.value(), "corner size would consume or reverse a source curve");
    const Point2 first_point{corner.x + first_dir->x * first_offset,
                             corner.y + first_dir->y * first_offset};
    const Point2 second_point{corner.x + second_dir->x * second_offset,
                              corner.y + second_dir->y * second_offset};
    if (kind == CornerKind::Fillet) {
      const double setback = first_offset;
      const double bisector_x = first_dir->x + second_dir->x;
      const double bisector_y = first_dir->y + second_dir->y;
      const double bisector_length = std::hypot(bisector_x, bisector_y);
      if (bisector_length <= kSketchModifyIdentityTolerance)
        return fail(first_id.value(), "fillet lines are collinear");
      const double center_distance = std::hypot(setback, size);
      const Point2 center{corner.x + bisector_x / bisector_length * center_distance,
                          corner.y + bisector_y / bisector_length * center_distance};
      const Point2 mid_direction{(first_point.x + second_point.x) * 0.5 - center.x,
                                 (first_point.y + second_point.y) * 0.5 - center.y};
      const double mid_length = std::hypot(mid_direction.x, mid_direction.y);
      if (mid_length <= kSketchModifyIdentityTolerance)
        return fail(first_id.value(), "fillet arc is degenerate");
      const Point2 mid{center.x + mid_direction.x / mid_length * size,
                       center.y + mid_direction.y / mid_length * size};
      auto created = ArcSegment::create_three_point(corner_id(sketch, "arc.fillet."), first_point,
                                                    mid, second_point);
      if (created.has_error())
        return Result<SketchModifyResult>::failure(created.error());
      arc = created.value().with_construction(first->construction() && second->construction());
    } else {
      auto created = LineSegment::create(
          corner_id(sketch, "line.chamfer."), first_point, second_point,
          first->construction() && second->construction() ? SketchLineRole::Construction
                                                          : SketchLineRole::Geometry);
      if (created.has_error())
        return Result<SketchModifyResult>::failure(created.error());
      chamfer_line = std::move(created.value());
    }

    std::vector<std::string> originals;
    std::vector<SketchModifyCurve> replacements;
    if (keep_trim) {
      originals = {first_id.value(), second_id.value()};
      auto trimmed_first = trim_line_to(*first, corner, first_point);
      auto trimmed_second = trim_line_to(*second, corner, second_point);
      if (trimmed_first.has_error())
        return Result<SketchModifyResult>::failure(trimmed_first.error());
      if (trimmed_second.has_error())
        return Result<SketchModifyResult>::failure(trimmed_second.error());
      replacements.push_back(std::move(trimmed_first.value()));
      replacements.push_back(std::move(trimmed_second.value()));
    }
    const SketchEntityId connector = arc ? arc->id() : chamfer_line->id();
    if (arc)
      replacements.emplace_back(std::move(*arc));
    if (chamfer_line)
      replacements.emplace_back(std::move(*chamfer_line));
    std::vector<SketchModifyInvalidatedEndpoint> moved;
    if (keep_trim) {
      const auto endpoint = [&corner](const LineSegment& line) {
        return sketch_modify_distance(line.start(), corner) <= sketch_modify_distance(line.end(), corner)
            ? SketchModifyEndSelector::Start : SketchModifyEndSelector::End;
      };
      moved = {{first_id, endpoint(*first)}, {second_id, endpoint(*second)}};
    }
    auto result = build(sketch, std::move(originals), std::move(replacements), {}, {}, {}, std::move(moved));
    if (!keep_trim) return result;
    return restore_edited_profiles(sketch, std::move(result), first_id, connector, second_id);
  }

  [[nodiscard]] static Result<detail::SketchModifyCurve>
  trim_line_to(const LineSegment& line, Point2 corner, Point2 tangent) {
    using namespace detail;
    const bool start_is_corner =
        sketch_modify_distance(line.start(), corner) <= sketch_modify_distance(line.end(), corner);
    auto result = LineSegment::create(line.id(), start_is_corner ? tangent : line.start(),
                                      start_is_corner ? line.end() : tangent, line.role());
    if (result.has_error())
      return Result<SketchModifyCurve>::failure(result.error());
    return Result<SketchModifyCurve>::success(std::move(result.value()));
  }
};

} // namespace blcad
