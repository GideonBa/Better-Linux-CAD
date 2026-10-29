#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_contour.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/dimension_driven_profile_resolver.hpp"
#include "blcad/geometry/reference_generated_profile_resolver.hpp"
#include "blcad/geometry/sketch_finish.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace blcad::geometry {

enum class ContainedProfileHoleKind { Polygon, Circle, Curves };

// A directly nested profile that bounds a hole in the material region of a
// selected outer profile. Polygon vertices stay in sketch-local coordinates;
// circles retain their analytic definition so geometry creation does not
// approximate them with line segments.
struct ContainedProfileHole {
  ProfileId profile;
  ContainedProfileHoleKind kind = ContainedProfileHoleKind::Polygon;
  std::vector<Point2> vertices;
  Point2 center;
  double radius_mm = 0.0;
  // Exact sketch-local curves; vertices are used only for containment/picking.
  std::vector<ClosedProfileCurveSegment> curves{};
};

// Compatibility view used by callers that only support analytic circles.
struct ContainedCircleHole {
  ProfileId profile;
  Point2 center;
  double radius_mm = 0.0;
};

namespace profile_region_classifier_detail {

inline constexpr double kTolerance = 1.0e-9;

[[nodiscard]] inline double squared_distance(Point2 left, Point2 right) noexcept {
  const double dx = left.x - right.x;
  const double dy = left.y - right.y;
  return dx * dx + dy * dy;
}

[[nodiscard]] inline double squared_distance_to_segment(Point2 point, Point2 start,
                                                        Point2 end) noexcept {
  const double dx = end.x - start.x;
  const double dy = end.y - start.y;
  const double length_squared = dx * dx + dy * dy;
  if (length_squared <= kTolerance * kTolerance)
    return squared_distance(point, start);
  const double projection =
      std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared, 0.0, 1.0);
  return squared_distance(point, Point2{start.x + projection * dx, start.y + projection * dy});
}

[[nodiscard]] inline double cross(Point2 a, Point2 b, Point2 c) noexcept {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] inline bool point_on_segment(Point2 point, Point2 start, Point2 end) noexcept {
  if (std::abs(cross(start, end, point)) > kTolerance)
    return false;
  return point.x >= std::min(start.x, end.x) - kTolerance &&
         point.x <= std::max(start.x, end.x) + kTolerance &&
         point.y >= std::min(start.y, end.y) - kTolerance &&
         point.y <= std::max(start.y, end.y) + kTolerance;
}

[[nodiscard]] inline int orientation(Point2 a, Point2 b, Point2 c) noexcept {
  const double value = cross(a, b, c);
  if (value > kTolerance)
    return 1;
  if (value < -kTolerance)
    return -1;
  return 0;
}

[[nodiscard]] inline bool segments_intersect_or_touch(Point2 a, Point2 b, Point2 c,
                                                      Point2 d) noexcept {
  const int abc = orientation(a, b, c);
  const int abd = orientation(a, b, d);
  const int cda = orientation(c, d, a);
  const int cdb = orientation(c, d, b);
  if (abc * abd < 0 && cda * cdb < 0)
    return true;
  return (abc == 0 && point_on_segment(c, a, b)) || (abd == 0 && point_on_segment(d, a, b)) ||
         (cda == 0 && point_on_segment(a, c, d)) || (cdb == 0 && point_on_segment(b, c, d));
}

[[nodiscard]] inline bool
point_strictly_inside_polygon(Point2 point, const std::vector<Point2>& polygon) noexcept {
  if (polygon.size() < 3U)
    return false;
  bool inside = false;
  for (std::size_t current = 0U, previous = polygon.size() - 1U; current < polygon.size();
       previous = current++) {
    const Point2 a = polygon[previous];
    const Point2 b = polygon[current];
    if (point_on_segment(point, a, b))
      return false;
    const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                         (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x);
    if (crosses)
      inside = !inside;
  }
  return inside;
}

[[nodiscard]] inline bool polygon_is_simple(const std::vector<Point2>& polygon) noexcept {
  if (polygon.size() < 3U)
    return false;
  for (std::size_t index = 0U; index < polygon.size(); ++index) {
    if (squared_distance(polygon[index], polygon[(index + 1U) % polygon.size()]) <=
        kTolerance * kTolerance)
      return false;
  }
  for (std::size_t left = 0U; left < polygon.size(); ++left) {
    const std::size_t left_next = (left + 1U) % polygon.size();
    for (std::size_t right = left + 1U; right < polygon.size(); ++right) {
      const std::size_t right_next = (right + 1U) % polygon.size();
      if (left == right || left_next == right || right_next == left)
        continue;
      if (segments_intersect_or_touch(polygon[left], polygon[left_next], polygon[right],
                                      polygon[right_next]))
        return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool
polygon_boundaries_intersect_or_touch(const std::vector<Point2>& left,
                                      const std::vector<Point2>& right) noexcept {
  for (std::size_t first = 0U; first < left.size(); ++first) {
    for (std::size_t second = 0U; second < right.size(); ++second) {
      if (segments_intersect_or_touch(left[first], left[(first + 1U) % left.size()], right[second],
                                      right[(second + 1U) % right.size()]))
        return true;
    }
  }
  return false;
}

[[nodiscard]] inline bool
polygon_strictly_inside_polygon(const std::vector<Point2>& inner,
                                const std::vector<Point2>& outer) noexcept {
  if (inner.size() < 3U || outer.size() < 3U || polygon_boundaries_intersect_or_touch(inner, outer))
    return false;
  return std::all_of(inner.begin(), inner.end(),
                     [&](Point2 point) { return point_strictly_inside_polygon(point, outer); });
}

[[nodiscard]] inline bool
circle_strictly_inside_polygon(Point2 center, double radius,
                               const std::vector<Point2>& polygon) noexcept {
  if (!point_strictly_inside_polygon(center, polygon))
    return false;
  const double required_distance = radius + kTolerance;
  const double required_squared = required_distance * required_distance;
  for (std::size_t index = 0U; index < polygon.size(); ++index) {
    if (squared_distance_to_segment(center, polygon[index],
                                    polygon[(index + 1U) % polygon.size()]) <= required_squared)
      return false;
  }
  return true;
}

[[nodiscard]] inline bool polygon_strictly_inside_circle(const std::vector<Point2>& polygon,
                                                         Point2 center, double radius) noexcept {
  const double allowed_radius = radius - kTolerance;
  if (allowed_radius <= 0.0)
    return false;
  const double allowed_squared = allowed_radius * allowed_radius;
  return std::all_of(polygon.begin(), polygon.end(), [&](Point2 point) {
    return squared_distance(point, center) < allowed_squared;
  });
}

[[nodiscard]] inline bool segment_intersects_circle_boundary(Point2 start, Point2 end,
                                                             Point2 center,
                                                             double radius) noexcept {
  const double radius_squared = radius * radius;
  const double start_delta = squared_distance(start, center) - radius_squared;
  const double end_delta = squared_distance(end, center) - radius_squared;
  const double distance_tolerance = kTolerance * std::max(1.0, radius_squared);
  if (std::abs(start_delta) <= distance_tolerance || std::abs(end_delta) <= distance_tolerance)
    return true;
  if ((start_delta < 0.0) != (end_delta < 0.0))
    return true;
  if (start_delta < 0.0 && end_delta < 0.0)
    return false;
  const double nearest = squared_distance_to_segment(center, start, end);
  return nearest <= (radius + kTolerance) * (radius + kTolerance);
}

[[nodiscard]] inline bool
polygon_circle_boundaries_intersect_or_touch(const std::vector<Point2>& polygon, Point2 center,
                                             double radius) noexcept {
  for (std::size_t index = 0U; index < polygon.size(); ++index) {
    if (segment_intersects_circle_boundary(polygon[index], polygon[(index + 1U) % polygon.size()],
                                           center, radius))
      return true;
  }
  return false;
}

[[nodiscard]] inline bool circle_boundaries_intersect_or_touch(Point2 first_center,
                                                               double first_radius,
                                                               Point2 second_center,
                                                               double second_radius) noexcept {
  const double distance = std::sqrt(squared_distance(first_center, second_center));
  return distance <= first_radius + second_radius + kTolerance &&
         distance + kTolerance >= std::abs(first_radius - second_radius);
}

[[nodiscard]] inline bool circle_strictly_inside_circle(Point2 inner_center, double inner_radius,
                                                        Point2 outer_center,
                                                        double outer_radius) noexcept {
  return std::sqrt(squared_distance(inner_center, outer_center)) + inner_radius + kTolerance <
         outer_radius;
}

[[nodiscard]] inline Result<std::vector<Point2>>
resolve_closed_profile_vertices(const PartDocument& document, const Sketch& sketch,
                                const ClosedProfile& profile) {
  for (const SketchEntityId& entity : profile.line_segments()) {
    if (sketch.find_reference_generated_line(entity) != nullptr) {
      return ReferenceGeneratedProfileResolver{}.resolve_closed_profile_vertices(
          document, sketch, profile, sketch.reference_generated_lines());
    }
  }
  if (!sketch.driving_dimensions().empty())
    return DimensionDrivenProfileResolver{}.resolve_closed_profile_vertices(document, sketch,
                                                                            profile);
  return sketch.closed_profile_vertices(profile);
}

[[nodiscard]] inline bool
boundaries_intersect_or_touch(const ContainedProfileHole& left,
                              const ContainedProfileHole& right) noexcept {
  if (left.kind != ContainedProfileHoleKind::Circle &&
      right.kind != ContainedProfileHoleKind::Circle)
    return polygon_boundaries_intersect_or_touch(left.vertices, right.vertices);
  if (left.kind != ContainedProfileHoleKind::Circle)
    return polygon_circle_boundaries_intersect_or_touch(left.vertices, right.center,
                                                        right.radius_mm);
  if (right.kind != ContainedProfileHoleKind::Circle)
    return polygon_circle_boundaries_intersect_or_touch(right.vertices, left.center,
                                                        left.radius_mm);
  return circle_boundaries_intersect_or_touch(left.center, left.radius_mm, right.center,
                                              right.radius_mm);
}

[[nodiscard]] inline bool strictly_contains(const ContainedProfileHole& outer,
                                            const ContainedProfileHole& inner) noexcept {
  if (outer.kind != ContainedProfileHoleKind::Circle &&
      inner.kind != ContainedProfileHoleKind::Circle)
    return polygon_strictly_inside_polygon(inner.vertices, outer.vertices);
  if (outer.kind != ContainedProfileHoleKind::Circle)
    return circle_strictly_inside_polygon(inner.center, inner.radius_mm, outer.vertices);
  if (inner.kind != ContainedProfileHoleKind::Circle)
    return polygon_strictly_inside_circle(inner.vertices, outer.center, outer.radius_mm);
  return circle_strictly_inside_circle(inner.center, inner.radius_mm, outer.center,
                                       outer.radius_mm);
}

} // namespace profile_region_classifier_detail

// Resolve one exact curved contour without converting its persistent curves to
// line segments. Sampling remains a conservative containment/picking product.
[[nodiscard]] inline Result<ContainedProfileHole>
resolve_curved_profile_hole(const PartDocument& document, const Sketch& sketch, const ProfileId& id,
                            const std::vector<SketchEntityId>& ids) {
  auto subset = Sketch::create(sketch.id(), sketch.name(), sketch.workplane());
  if (!subset)
    return Result<ContainedProfileHole>::failure(subset.error());
  for (const auto& entity : ids) {
    auto added = [&] {
      if (const auto* line = sketch.find_line_segment(entity))
        return subset.value().add_entity(*line);
      if (const auto* arc = sketch.find_arc_segment(entity))
        return subset.value().add_entity(*arc);
      if (const auto* spline = sketch.find_spline_segment(entity))
        return subset.value().add_entity(*spline);
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "profile curve must exist"));
    }();
    if (!added)
      return Result<ContainedProfileHole>::failure(added.error());
  }
  auto region = SketchRegionFinder{}.find_single_curve_region(document, subset.value());
  if (!region)
    return Result<ContainedProfileHole>::failure(region.error());
  auto traversal = resolve_sketch_contour(sketch, region.value().source_entities());
  if (!traversal)
    return Result<ContainedProfileHole>::failure(traversal.error());
  ContainedProfileHole result{
      id, ContainedProfileHoleKind::Curves, region.value().vertices, {}, 0.0, {}};
  const auto point = [](Point2 p) { return Point3{p.x, p.y, 0}; };
  for (const auto& entry : traversal.value()) {
    ClosedProfileCurveSegment curve;
    if (const auto* line = sketch.find_line_segment(entry.id))
      curve = {ClosedProfileCurveKind::Line, point(line->start()), {}, point(line->end())};
    else if (const auto* arc = sketch.find_arc_segment(entry.id))
      curve = {ClosedProfileCurveKind::CircularArc, point(arc->start()), point(arc->mid()),
               point(arc->end())};
    else {
      const auto& spline = *sketch.find_spline_segment(entry.id);
      curve = {ClosedProfileCurveKind::CubicBezierSpline,
               point(spline.start()),
               point(spline.control1()),
               point(spline.end()),
               point(spline.control2()),
               spline.weights()};
    }
    if (entry.reversed) {
      std::swap(curve.start, curve.end);
      if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
        std::swap(curve.mid, curve.control2);
        std::reverse(curve.weights.begin(), curve.weights.end());
      }
    }
    result.curves.push_back(curve);
  }
  return Result<ContainedProfileHole>::success(std::move(result));
}

// Classifies material-region holes formed by all supported simple closed
// profiles. Only immediate children of `outer_profile` are returned: a loop
// nested inside another child is an island/deeper region, not a second hole of
// the outer loop. Touching or crossing candidates are intentionally excluded.
[[nodiscard]] inline Result<std::vector<ContainedProfileHole>>
classify_contained_profile_holes(const PartDocument& document, const Sketch& sketch,
                                 const ContainedProfileHole& outer) {
  using namespace profile_region_classifier_detail;
  const auto& outer_profile = outer.profile;
  if (outer.kind == ContainedProfileHoleKind::Circle &&
      (!std::isfinite(outer.radius_mm) || outer.radius_mm <= kTolerance ||
       !std::isfinite(outer.center.x) || !std::isfinite(outer.center.y)))
    return Result<std::vector<ContainedProfileHole>>::failure(Error::validation(
        outer_profile.value(), "outer circle must have a finite positive radius"));
  if (outer.kind != ContainedProfileHoleKind::Circle && !polygon_is_simple(outer.vertices)) {
    return Result<std::vector<ContainedProfileHole>>::failure(
        Error::validation(outer_profile.value(), "outer profile must be a simple polygon"));
  }

  std::vector<ContainedProfileHole> candidates;
  candidates.reserve(sketch.rectangle_profiles().size() + sketch.closed_profiles().size() +
                     sketch.circle_profiles().size());

  for (const RectangleProfile& rectangle : sketch.rectangle_profiles()) {
    if (rectangle.id() == outer_profile || sketch.profile_has_construction(rectangle.id()))
      continue;
    const Parameter* width = document.find_parameter(rectangle.width_parameter());
    const Parameter* height = document.find_parameter(rectangle.height_parameter());
    if (width == nullptr || height == nullptr || width->type() != ParameterType::Length ||
        height->type() != ParameterType::Length)
      continue;
    const double width_mm = width->value().millimeters();
    const double height_mm = height->value().millimeters();
    if (!std::isfinite(width_mm) || !std::isfinite(height_mm) || width_mm <= kTolerance ||
        height_mm <= kTolerance)
      continue;
    const Point2 center = rectangle.center();
    const double half_width = width_mm / 2.0;
    const double half_height = height_mm / 2.0;
    std::vector<Point2> vertices{{center.x - half_width, center.y - half_height},
                                 {center.x + half_width, center.y - half_height},
                                 {center.x + half_width, center.y + half_height},
                                 {center.x - half_width, center.y + half_height}};
    if (strictly_contains(outer,
                          {rectangle.id(), ContainedProfileHoleKind::Polygon, vertices, {}, 0.0}))
      candidates.push_back(
          {rectangle.id(), ContainedProfileHoleKind::Polygon, std::move(vertices), {}, 0.0});
  }

  for (const ClosedProfile& closed : sketch.closed_profiles()) {
    if (closed.id() == outer_profile || sketch.profile_has_construction(closed.id()))
      continue;
    auto vertices = resolve_closed_profile_vertices(document, sketch, closed);
    if (vertices.has_error() || !polygon_is_simple(vertices.value()) ||
        !strictly_contains(
            outer, {closed.id(), ContainedProfileHoleKind::Polygon, vertices.value(), {}, 0.0}))
      continue;
    candidates.push_back(
        {closed.id(), ContainedProfileHoleKind::Polygon, std::move(vertices.value()), {}, 0.0});
  }

  for (const CircleProfile& circle : sketch.circle_profiles()) {
    if (circle.id() == outer_profile || sketch.profile_has_construction(circle.id()))
      continue;
    const Parameter* diameter = document.find_parameter(circle.diameter_parameter());
    if (diameter == nullptr || diameter->type() != ParameterType::Length)
      continue;
    const double radius = diameter->value().millimeters() / 2.0;
    if (!std::isfinite(radius) || radius <= kTolerance ||
        !strictly_contains(
            outer, {circle.id(), ContainedProfileHoleKind::Circle, {}, circle.center(), radius}))
      continue;
    candidates.push_back(
        {circle.id(), ContainedProfileHoleKind::Circle, {}, circle.center(), radius});
  }

  const auto append_curved = [&](const ProfileId& id, const std::vector<SketchEntityId>& ids) {
    if (id == outer_profile || sketch.profile_has_construction(id))
      return;
    auto hole = resolve_curved_profile_hole(document, sketch, id, ids);
    if (hole && strictly_contains(outer, hole.value()))
      candidates.push_back(std::move(hole.value()));
  };
  for (const auto& profile : sketch.arc_closed_profiles())
    append_curved(profile.id(), profile.curve_segments());
  // A selection-first feature can materialize only its outer region; enclosed
  // raw spline/arc loops still bound holes and must agree with the viewport.
  auto raw = SketchFinishService::analyze_unmaterialized(document, sketch);
  if (raw)
    for (const auto& region : raw.value().regions) {
      if (region.id == outer_profile)
        continue;
      if (!region.curve_segments.empty())
        append_curved(region.id, region.curve_segments);
      else if (strictly_contains(
                   outer, {region.id, ContainedProfileHoleKind::Polygon, region.vertices, {}, 0.0}))
        candidates.push_back(
            {region.id, ContainedProfileHoleKind::Polygon, region.vertices, {}, 0.0, {}});
    }

  std::vector<bool> valid(candidates.size(), true);
  for (std::size_t left = 0U; left < candidates.size(); ++left) {
    for (std::size_t right = left + 1U; right < candidates.size(); ++right) {
      if (!boundaries_intersect_or_touch(candidates[left], candidates[right]))
        continue;
      valid[left] = false;
      valid[right] = false;
    }
  }

  std::vector<bool> immediate(candidates.size(), false);
  for (std::size_t candidate = 0U; candidate < candidates.size(); ++candidate) {
    if (!valid[candidate])
      continue;
    bool has_intermediate_parent = false;
    for (std::size_t possible_parent = 0U; possible_parent < candidates.size(); ++possible_parent) {
      if (candidate == possible_parent || !valid[possible_parent])
        continue;
      if (strictly_contains(candidates[possible_parent], candidates[candidate])) {
        has_intermediate_parent = true;
        break;
      }
    }
    if (!has_intermediate_parent)
      immediate[candidate] = true;
  }

  std::vector<ContainedProfileHole> result;
  result.reserve(candidates.size());
  for (std::size_t candidate = 0U; candidate < candidates.size(); ++candidate)
    if (immediate[candidate])
      result.push_back(std::move(candidates[candidate]));
  return Result<std::vector<ContainedProfileHole>>::success(std::move(result));
}

[[nodiscard]] inline Result<std::vector<ContainedProfileHole>>
classify_contained_profile_holes(const PartDocument& document, const Sketch& sketch,
                                 const ProfileId& outer_profile,
                                 const std::vector<Point2>& outer_vertices) {
  return classify_contained_profile_holes(
      document, sketch,
      {outer_profile, ContainedProfileHoleKind::Polygon, outer_vertices, {}, 0.0});
}

[[nodiscard]] inline Result<std::vector<ContainedCircleHole>>
classify_contained_circle_holes(const PartDocument& document, const Sketch& sketch,
                                const ProfileId& outer_profile,
                                const std::vector<Point2>& outer_vertices) {
  auto classified =
      classify_contained_profile_holes(document, sketch, outer_profile, outer_vertices);
  if (classified.has_error())
    return Result<std::vector<ContainedCircleHole>>::failure(classified.error());
  std::vector<ContainedCircleHole> result;
  for (const ContainedProfileHole& hole : classified.value()) {
    if (hole.kind == ContainedProfileHoleKind::Circle)
      result.push_back({hole.profile, hole.center, hole.radius_mm});
  }
  return Result<std::vector<ContainedCircleHole>>::success(std::move(result));
}

} // namespace blcad::geometry
