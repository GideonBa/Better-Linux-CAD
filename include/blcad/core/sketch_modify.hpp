#pragma once

#include "blcad/core/sketch.hpp"

#include <algorithm>
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
// candidate Sketch that copies all unaffected intent verbatim. References that
// the rewrite would invalidate (profiles, embedded constraints/dimensions,
// tangent continuity) are explicitly rejected before any candidate is built, so
// the source Sketch is never partially mutated. In-place edits that preserve an
// entity id and its endpoint roles keep every referencing intent so usable
// constraints survive.

enum class SketchModifyEndSelector { Start, End };

struct SketchModifyResult {
  Sketch sketch;
  // Source entity ids whose identity/endpoint semantics changed (removed or
  // split). References to these are invalid; in-place edits are not listed.
  std::vector<std::string> invalidated_ids;
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

[[nodiscard]] inline const SketchEntityId& sketch_modify_curve_id(
    const SketchModifyCurve& curve) noexcept {
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
  if (value < 0.0) value += two_pi;
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
  if (std::abs(sweep) <= kSketchModifyIdentityTolerance) return std::nullopt;
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

struct CurveSampler {
  enum class Kind { Line, Arc } kind{Kind::Line};
  Point2 line_start;
  Point2 line_end;
  ArcGeometry arc;
};

[[nodiscard]] inline std::optional<CurveSampler> line_sampler(const LineSegment& line) noexcept {
  return CurveSampler{CurveSampler::Kind::Line, line.start(), line.end(), {}};
}

[[nodiscard]] inline std::optional<CurveSampler> arc_sampler(const ArcSegment& arc) noexcept {
  const auto geometry = arc_geometry(arc.start(), arc.mid(), arc.end());
  if (!geometry) return std::nullopt;
  return CurveSampler{CurveSampler::Kind::Arc, {}, {}, *geometry};
}

[[nodiscard]] inline Point2 sampler_point(const CurveSampler& sampler, double t) noexcept {
  if (sampler.kind == CurveSampler::Kind::Line)
    return {sampler.line_start.x + (sampler.line_end.x - sampler.line_start.x) * t,
            sampler.line_start.y + (sampler.line_end.y - sampler.line_start.y) * t};
  return arc_point(sampler.arc, sampler.arc.start_angle + sampler.arc.sweep * t);
}

// Parameter of the closest point on the (unbounded) supporting line/circle.
[[nodiscard]] inline std::optional<double> sampler_project(const CurveSampler& sampler,
                                                           Point2 point) noexcept {
  if (sampler.kind == CurveSampler::Kind::Line) {
    const Point2 direction{sampler.line_end.x - sampler.line_start.x,
                           sampler.line_end.y - sampler.line_start.y};
    const double length_sq = direction.x * direction.x + direction.y * direction.y;
    if (length_sq <= kSketchModifyIdentityTolerance) return std::nullopt;
    return ((point.x - sampler.line_start.x) * direction.x +
            (point.y - sampler.line_start.y) * direction.y) /
           length_sq;
  }
  const double angle = std::atan2(point.y - sampler.arc.center.y, point.x - sampler.arc.center.x);
  return arc_param(sampler.arc, angle);
}

// Intersection points of two supporting curves restricted to both actual spans.
[[nodiscard]] inline std::vector<Point2> curve_intersections(const CurveSampler& first,
                                                             const CurveSampler& second) {
  std::vector<Point2> points;
  const auto on_span = [](const CurveSampler& sampler, Point2 candidate) {
    const auto t = sampler_project(sampler, candidate);
    return t.has_value() && *t >= -kSketchModifyParamTolerance &&
           *t <= 1.0 + kSketchModifyParamTolerance;
  };
  const auto add = [&](Point2 candidate) {
    if (!sketch_modify_finite(candidate)) return;
    if (!on_span(first, candidate) || !on_span(second, candidate)) return;
    for (const auto existing : points)
      if (sketch_modify_distance(existing, candidate) <= kSketchModifyPointTolerance) return;
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
    if (a <= kSketchModifyIdentityTolerance) return;
    const Point2 f{line.line_start.x - arc.center.x, line.line_start.y - arc.center.y};
    const double b = 2.0 * (f.x * d.x + f.y * d.y);
    const double c = f.x * f.x + f.y * f.y - arc.radius * arc.radius;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < -kSketchModifyIdentityTolerance) return;
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
  if (const auto* line = sketch.find_line_segment(id)) return line_sampler(*line);
  if (const auto* arc = sketch.find_arc_segment(id)) return arc_sampler(*arc);
  return std::nullopt;
}

// Rebuilds a candidate Sketch replacing `originals` with `replacements`, copying
// every other entity and all intent. Callers pre-validate reference safety, so
// this only performs the mechanical copy.
[[nodiscard]] inline Result<Sketch> rebuild_candidate(
    const Sketch& source, const std::vector<std::string>& originals,
    const std::vector<SketchModifyCurve>& replacements) {
  std::vector<std::string> removed(originals.begin(), originals.end());
  const auto is_removed = [&removed](const SketchEntityId& id) {
    return std::find(removed.begin(), removed.end(), id.value()) != removed.end();
  };

  auto candidate = Sketch::create(source.id(), source.name(), source.workplane());
  if (candidate.has_error()) return candidate;
  const auto add_curve = [&candidate](const SketchModifyCurve& curve) -> Result<std::size_t> {
    return std::visit([&candidate](const auto& value) { return candidate.value().add_entity(value); },
                      curve);
  };

  for (const auto& line : source.line_segments())
    if (!is_removed(line.id())) {
      auto added = candidate.value().add_entity(line);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
  for (const auto& arc : source.arc_segments())
    if (!is_removed(arc.id())) {
      auto added = candidate.value().add_entity(arc);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
  for (const auto& spline : source.spline_segments())
    if (!is_removed(spline.id())) {
      auto added = candidate.value().add_entity(spline);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
  for (const auto& replacement : replacements) {
    auto added = add_curve(replacement);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& point : source.projected_points()) {
    auto added = candidate.value().add_reference(point);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& line : source.projected_lines()) {
    auto added = candidate.value().add_reference(line);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& line : source.reference_generated_lines()) {
    auto added = candidate.value().add_reference(line);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint : source.constraints()) {
    auto added = candidate.value().add_constraint(constraint);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& constraint : source.geometric_constraints()) {
    auto added = candidate.value().add_constraint(constraint);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& dimension : source.driving_dimensions()) {
    auto added = candidate.value().add_dimension(dimension);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& operation : source.trim_extend_operations()) {
    auto added = candidate.value().add_trim_extend_operation(operation);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& continuity : source.tangent_continuities()) {
    auto added = candidate.value().add_tangent_continuity(continuity);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.rectangle_profiles()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.circle_profiles()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.closed_profiles()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.arc_closed_profiles()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.composite_closed_profiles()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  for (const auto& profile : source.circular_hole_patterns()) {
    auto added = candidate.value().add_profile(profile);
    if (added.has_error()) return Result<Sketch>::failure(added.error());
  }
  return candidate;
}

// True when `id` participates in any ordered profile contour.
[[nodiscard]] inline bool entity_in_profile(const Sketch& sketch, const SketchEntityId& id) {
  const auto in = [&id](const std::vector<SketchEntityId>& contour) {
    return std::find(contour.begin(), contour.end(), id) != contour.end();
  };
  for (const auto& profile : sketch.closed_profiles())
    if (in(profile.line_segments())) return true;
  for (const auto& profile : sketch.arc_closed_profiles())
    if (in(profile.curve_segments())) return true;
  for (const auto& profile : sketch.composite_closed_profiles()) {
    if (in(profile.outer_contour())) return true;
    for (const auto& contour : profile.inner_contours())
      if (in(contour)) return true;
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

} // namespace detail

class SketchModifyService {
public:
  // Removes the picked portion of a line/arc bounded by its nearest
  // intersections with other line/arc entities. Shortening keeps the id; a
  // removed middle splits into two; no bounding intersection removes it.
  [[nodiscard]] static Result<SketchModifyResult> trim(const Sketch& sketch, SketchEntityId target,
                                                       Point2 pick) {
    using namespace detail;
    if (!sketch_modify_finite(pick))
      return fail(target.value(), "trim pick coordinates must be finite");
    auto sampler = curve_sampler_for(sketch, target);
    if (!sampler)
      return fail(target.value(), "trim target must be an existing line or arc entity");
    if (entity_in_profile(sketch, target))
      return fail(target.value(),
                  "trimming an entity used by a profile requires an explicit profile rewrite");

    std::vector<double> cuts = intersection_params(sketch, target, *sampler);
    const auto pick_param = sampler_project(*sampler, pick);
    if (!pick_param || *pick_param < -kSketchModifyParamTolerance ||
        *pick_param > 1.0 + kSketchModifyParamTolerance)
      return fail(target.value(), "trim pick must lie on the target curve");
    const double tp = std::clamp(*pick_param, 0.0, 1.0);

    double lower = 0.0;
    double upper = 1.0;
    bool has_lower = false;
    bool has_upper = false;
    for (const double cut : cuts) {
      if (cut <= tp + kSketchModifyParamTolerance && cut >= lower) {
        lower = cut;
        has_lower = cut > kSketchModifyParamTolerance;
      }
    }
    for (auto iterator = cuts.rbegin(); iterator != cuts.rend(); ++iterator) {
      const double cut = *iterator;
      if (cut >= tp - kSketchModifyParamTolerance && cut <= upper) {
        upper = cut;
        has_upper = cut < 1.0 - kSketchModifyParamTolerance;
      }
    }

    if (!has_lower && !has_upper)
      return build(sketch, {target.value()}, {}, {target.value()});
    if (has_lower && has_upper) {
      auto first = subcurve(sketch, target, 0.0, lower, target);
      if (first.has_error()) return Result<SketchModifyResult>::failure(first.error());
      auto second = subcurve(sketch, target, upper, 1.0, split_id(sketch, target));
      if (second.has_error()) return Result<SketchModifyResult>::failure(second.error());
      return build(sketch, {target.value()}, {std::move(first.value()), std::move(second.value())},
                   {target.value()});
    }
    const double keep_lo = has_upper ? upper : 0.0;
    const double keep_hi = has_upper ? 1.0 : lower;
    auto kept = subcurve(sketch, target, keep_lo, keep_hi, target);
    if (kept.has_error()) return Result<SketchModifyResult>::failure(kept.error());
    return build(sketch, {target.value()}, {std::move(kept.value())}, {});
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
      return fail(target.value(), "extend target must be an existing line or arc entity");
    if (entity_in_profile(sketch, target))
      return fail(target.value(),
                  "extending an entity used by a profile requires an explicit profile rewrite");
    const Point2 start = sampler_point(*sampler, 0.0);
    const Point2 end = sampler_point(*sampler, 1.0);
    const bool extend_end =
        sketch_modify_distance(pick, end) <= sketch_modify_distance(pick, start);

    std::optional<double> best;
    for (const auto& other : other_samplers(sketch, target)) {
      for (const auto candidate : curve_intersections_unbounded(*sampler, other.sampler,
                                                                other.id, sketch)) {
        const auto raw = support_param(*sampler, candidate);
        if (!raw) continue;
        const double t = *raw;
        if (extend_end && t > 1.0 + kSketchModifyParamTolerance) {
          if (!best || t < *best) best = t;
        } else if (!extend_end && t < -kSketchModifyParamTolerance) {
          if (!best || t > *best) best = t;
        }
      }
    }
    if (!best)
      return fail(target.value(), "extend found no reachable intersection beyond the endpoint");
    const Point2 moved = support_point(*sampler, *best);
    auto rebuilt = replace_endpoint(sketch, target, extend_end, moved);
    if (rebuilt.has_error()) return Result<SketchModifyResult>::failure(rebuilt.error());
    return build(sketch, {target.value()}, {std::move(rebuilt.value())}, {});
  }

  // Splits a line/arc/spline at the projected pick into two entities. The first
  // keeps the id; the second gets a fresh id. References to the id are invalid.
  [[nodiscard]] static Result<SketchModifyResult> split(const Sketch& sketch, SketchEntityId target,
                                                        Point2 point) {
    using namespace detail;
    if (!sketch_modify_finite(point))
      return fail(target.value(), "split point coordinates must be finite");
    if (entity_in_profile(sketch, target))
      return fail(target.value(),
                  "splitting an entity used by a profile requires an explicit profile rewrite");
    if (const std::string reference = invalidated_reference(sketch, target); !reference.empty())
      return fail(target.value(), "split would invalidate " + reference);

    if (const auto* spline = sketch.find_spline_segment(target)) {
      const auto t = spline_param(*spline, point);
      if (!t) return fail(target.value(), "split point must lie on the spline");
      auto halves = split_spline(*spline, *t, split_id(sketch, target));
      if (halves.has_error()) return Result<SketchModifyResult>::failure(halves.error());
      return build(sketch, {target.value()}, std::move(halves.value()), {target.value()});
    }
    auto sampler = curve_sampler_for(sketch, target);
    if (!sampler)
      return fail(target.value(), "split target must be an existing line, arc, or spline entity");
    const auto t = sampler_project(*sampler, point);
    if (!t || *t <= kSketchModifyParamTolerance || *t >= 1.0 - kSketchModifyParamTolerance)
      return fail(target.value(), "split point must lie strictly inside the target curve");
    auto first = subcurve(sketch, target, 0.0, *t, target);
    if (first.has_error()) return Result<SketchModifyResult>::failure(first.error());
    auto second = subcurve(sketch, target, *t, 1.0, split_id(sketch, target));
    if (second.has_error()) return Result<SketchModifyResult>::failure(second.error());
    return build(sketch, {target.value()}, {std::move(first.value()), std::move(second.value())},
                 {target.value()});
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

  [[nodiscard]] static Result<SketchModifyResult> fail(std::string object_id,
                                                       std::string message) {
    return Result<SketchModifyResult>::failure(
        Error::validation(std::move(object_id), std::move(message)));
  }

  [[nodiscard]] static Result<SketchModifyResult> build(
      const Sketch& sketch, std::vector<std::string> originals,
      std::vector<detail::SketchModifyCurve> replacements,
      std::vector<std::string> invalidated) {
    for (const auto& id : invalidated)
      if (const std::string reference = detail::invalidated_reference(sketch, SketchEntityId(id));
          !reference.empty())
        return fail(id, "modification would invalidate " + reference);
    auto candidate = detail::rebuild_candidate(sketch, originals, replacements);
    if (candidate.has_error()) return Result<SketchModifyResult>::failure(candidate.error());
    return Result<SketchModifyResult>::success(
        {std::move(candidate.value()), std::move(invalidated)});
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

  // All intersection parameters of the target curve with other line/arc curves.
  [[nodiscard]] static std::vector<double> intersection_params(const Sketch& sketch,
                                                               const SketchEntityId& target,
                                                               const detail::CurveSampler& sampler) {
    std::vector<double> params;
    for (const auto& other : other_samplers(sketch, target))
      for (const auto point : detail::curve_intersections(sampler, other.sampler)) {
        const auto t = detail::sampler_project(sampler, point);
        if (t && *t > detail::kSketchModifyParamTolerance &&
            *t < 1.0 - detail::kSketchModifyParamTolerance)
          params.push_back(*t);
      }
    std::sort(params.begin(), params.end());
    params.erase(std::unique(params.begin(), params.end(),
                             [](double a, double b) {
                               return std::abs(a - b) <= detail::kSketchModifyParamTolerance;
                             }),
                 params.end());
    return params;
  }

  struct NamedSampler {
    SketchEntityId id;
    detail::CurveSampler sampler;
  };

  [[nodiscard]] static std::vector<NamedSampler> other_samplers(const Sketch& sketch,
                                                                const SketchEntityId& target) {
    std::vector<NamedSampler> result;
    for (const auto& line : sketch.line_segments())
      if (line.id() != target)
        if (auto sampler = detail::line_sampler(line)) result.push_back({line.id(), *sampler});
    for (const auto& arc : sketch.arc_segments())
      if (arc.id() != target)
        if (auto sampler = detail::arc_sampler(arc)) result.push_back({arc.id(), *sampler});
    return result;
  }

  // Parameter on the target support (unbounded) with no span restriction.
  [[nodiscard]] static std::optional<double> support_param(const detail::CurveSampler& sampler,
                                                           Point2 point) {
    if (sampler.kind == detail::CurveSampler::Kind::Line)
      return detail::sampler_project(sampler, point);
    const double angle =
        std::atan2(point.y - sampler.arc.center.y, point.x - sampler.arc.center.x);
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
  [[nodiscard]] static std::vector<Point2> curve_intersections_unbounded(
      const detail::CurveSampler& target, const detail::CurveSampler& other,
      const SketchEntityId&, const Sketch&) {
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
                                        sampler_point(*sampler, to));
      if (result.has_error()) return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    if (const auto* arc = sketch.find_arc_segment(id)) {
      auto sampler = arc_sampler(*arc);
      if (!sampler) return Result<SketchModifyCurve>::failure(Error::validation(
          id.value(), "arc entity has degenerate geometry"));
      auto result = ArcSegment::create_three_point(new_id, sampler_point(*sampler, from),
                                                   sampler_point(*sampler, (from + to) * 0.5),
                                                   sampler_point(*sampler, to));
      if (result.has_error()) return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    return Result<SketchModifyCurve>::failure(
        Error::validation(id.value(), "subcurve target must be a line or arc"));
  }

  [[nodiscard]] static Result<detail::SketchModifyCurve> replace_endpoint(
      const Sketch& sketch, const SketchEntityId& id, bool replace_end, Point2 moved) {
    using namespace detail;
    if (const auto* line = sketch.find_line_segment(id)) {
      auto result = LineSegment::create(id, replace_end ? line->start() : moved,
                                        replace_end ? moved : line->end());
      if (result.has_error()) return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    if (const auto* arc = sketch.find_arc_segment(id)) {
      auto sampler = arc_sampler(*arc);
      if (!sampler) return Result<SketchModifyCurve>::failure(Error::validation(
          id.value(), "arc entity has degenerate geometry"));
      const auto moved_param = support_param(*sampler, moved);
      const double target = moved_param.value_or(replace_end ? 1.0 : 0.0);
      const double lo = replace_end ? 0.0 : target;
      const double hi = replace_end ? target : 1.0;
      auto result = ArcSegment::create_three_point(id, support_point(*sampler, lo),
                                                   support_point(*sampler, (lo + hi) * 0.5),
                                                   support_point(*sampler, hi));
      if (result.has_error()) return Result<SketchModifyCurve>::failure(result.error());
      return Result<SketchModifyCurve>::success(std::move(result.value()));
    }
    return Result<SketchModifyCurve>::failure(
        Error::validation(id.value(), "extend target must be a line or arc"));
  }

  [[nodiscard]] static std::optional<double> spline_param(const SplineSegment& spline,
                                                          Point2 point) {
    double best_t = 0.0;
    double best_distance = std::numeric_limits<double>::infinity();
    constexpr std::size_t samples = 256U;
    for (std::size_t index = 0U; index <= samples; ++index) {
      const double t = static_cast<double>(index) / static_cast<double>(samples);
      const double distance = detail::sketch_modify_distance(evaluate_spline(spline, t), point);
      if (distance < best_distance) {
        best_distance = distance;
        best_t = t;
      }
    }
    if (best_t <= detail::kSketchModifyParamTolerance ||
        best_t >= 1.0 - detail::kSketchModifyParamTolerance)
      return std::nullopt;
    return best_t;
  }

  [[nodiscard]] static Point2 evaluate_spline(const SplineSegment& spline, double t) noexcept {
    const double u = 1.0 - t;
    const double b0 = u * u * u;
    const double b1 = 3.0 * u * u * t;
    const double b2 = 3.0 * u * t * t;
    const double b3 = t * t * t;
    return {b0 * spline.start().x + b1 * spline.control1().x + b2 * spline.control2().x +
                b3 * spline.end().x,
            b0 * spline.start().y + b1 * spline.control1().y + b2 * spline.control2().y +
                b3 * spline.end().y};
  }

  [[nodiscard]] static Result<std::vector<detail::SketchModifyCurve>> split_spline(
      const SplineSegment& spline, double t, SketchEntityId second_id) {
    const auto lerp = [](Point2 a, Point2 b, double factor) {
      return Point2{a.x + (b.x - a.x) * factor, a.y + (b.y - a.y) * factor};
    };
    const Point2 p0 = spline.start();
    const Point2 p1 = spline.control1();
    const Point2 p2 = spline.control2();
    const Point2 p3 = spline.end();
    const Point2 a = lerp(p0, p1, t);
    const Point2 b = lerp(p1, p2, t);
    const Point2 c = lerp(p2, p3, t);
    const Point2 d = lerp(a, b, t);
    const Point2 e = lerp(b, c, t);
    const Point2 f = lerp(d, e, t);
    auto first = SplineSegment::create_cubic_bezier(spline.id(), p0, a, d, f);
    if (first.has_error())
      return Result<std::vector<detail::SketchModifyCurve>>::failure(first.error());
    auto second = SplineSegment::create_cubic_bezier(std::move(second_id), f, e, c, p3);
    if (second.has_error())
      return Result<std::vector<detail::SketchModifyCurve>>::failure(second.error());
    return Result<std::vector<detail::SketchModifyCurve>>::success(
        {std::move(first.value()), std::move(second.value())});
  }

  [[nodiscard]] static Result<SketchModifyResult> corner(const Sketch& sketch,
                                                         const SketchEntityId& first_id,
                                                         const SketchEntityId& second_id,
                                                         double size, CornerKind kind,
                                                         bool keep_trim) {
    using namespace detail;
    if (first_id == second_id)
      return fail(first_id.value(), "fillet/chamfer requires two distinct line entities");
    if (!std::isfinite(size) || size <= kSketchModifyIdentityTolerance)
      return fail(first_id.value(), "fillet radius / chamfer distance must be positive and finite");
    const auto* first = sketch.find_line_segment(first_id);
    const auto* second = sketch.find_line_segment(second_id);
    if (first == nullptr || second == nullptr)
      return fail(first_id.value(), "Block 116 fillet/chamfer requires two line entities");
    if (entity_in_profile(sketch, first_id) || entity_in_profile(sketch, second_id))
      return fail(first_id.value(),
                  "fillet/chamfer on a profile edge requires an explicit profile rewrite");

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
      if (length <= kSketchModifyIdentityTolerance) return std::nullopt;
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
      if (created.has_error()) return Result<SketchModifyResult>::failure(created.error());
      arc = std::move(created.value());
    } else {
      auto created = LineSegment::create(corner_id(sketch, "line.chamfer."), first_point,
                                         second_point);
      if (created.has_error()) return Result<SketchModifyResult>::failure(created.error());
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
    if (arc) replacements.emplace_back(std::move(*arc));
    if (chamfer_line) replacements.emplace_back(std::move(*chamfer_line));
    return build(sketch, std::move(originals), std::move(replacements), {});
  }

  [[nodiscard]] static Result<detail::SketchModifyCurve> trim_line_to(const LineSegment& line,
                                                                      Point2 corner,
                                                                      Point2 tangent) {
    using namespace detail;
    const bool start_is_corner =
        sketch_modify_distance(line.start(), corner) <= sketch_modify_distance(line.end(), corner);
    auto result = LineSegment::create(line.id(), start_is_corner ? tangent : line.start(),
                                      start_is_corner ? line.end() : tangent);
    if (result.has_error()) return Result<SketchModifyCurve>::failure(result.error());
    return Result<SketchModifyCurve>::success(std::move(result.value()));
  }
};

} // namespace blcad
