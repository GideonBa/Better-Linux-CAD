#include "blcad/geometry/sketch_region_finder.hpp"

#include "blcad/geometry/reference_generated_profile_resolver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

struct LocalLine {
  SketchEntityId id;
  Point2 start;
  Point2 end;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
}

[[nodiscard]] double orientation(Point2 a, Point2 b, Point2 c) noexcept {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] bool on_segment(Point2 a, Point2 b, Point2 p) noexcept {
  return std::min(a.x, b.x) - k_tolerance <= p.x && p.x <= std::max(a.x, b.x) + k_tolerance &&
         std::min(a.y, b.y) - k_tolerance <= p.y && p.y <= std::max(a.y, b.y) + k_tolerance &&
         std::abs(orientation(a, b, p)) <= k_tolerance;
}

[[nodiscard]] bool segments_intersect(Point2 a1, Point2 a2, Point2 b1, Point2 b2) noexcept {
  const double o1 = orientation(a1, a2, b1);
  const double o2 = orientation(a1, a2, b2);
  const double o3 = orientation(b1, b2, a1);
  const double o4 = orientation(b1, b2, a2);

  if (((o1 > k_tolerance && o2 < -k_tolerance) ||
       (o1 < -k_tolerance && o2 > k_tolerance)) &&
      ((o3 > k_tolerance && o4 < -k_tolerance) ||
       (o3 < -k_tolerance && o4 > k_tolerance))) {
    return true;
  }

  return (std::abs(o1) <= k_tolerance && on_segment(a1, a2, b1)) ||
         (std::abs(o2) <= k_tolerance && on_segment(a1, a2, b2)) ||
         (std::abs(o3) <= k_tolerance && on_segment(b1, b2, a1)) ||
         (std::abs(o4) <= k_tolerance && on_segment(b1, b2, a2));
}

[[nodiscard]] bool are_adjacent_segments(std::size_t first, std::size_t second,
                                         std::size_t count) noexcept {
  return first + 1U == second || second + 1U == first ||
         (first == 0U && second + 1U == count) || (second == 0U && first + 1U == count);
}

[[nodiscard]] Vector2 vector_between(Point2 from, Point2 to) noexcept {
  return Vector2{to.x - from.x, to.y - from.y};
}

[[nodiscard]] double length(Vector2 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

[[nodiscard]] Vector2 normalize(Vector2 vector) noexcept {
  const double vector_length = length(vector);
  return Vector2{vector.x / vector_length, vector.y / vector_length};
}

[[nodiscard]] const SketchDrivingDimension* find_dimension_for_segment(
    const Sketch& sketch, const SketchEntityId& line_id) noexcept {
  for (const auto& dimension : sketch.driving_dimensions()) {
    if (dimension.first_target().entity() == line_id && dimension.second_target().entity() == line_id &&
        dimension.first_target().kind() == SketchReferenceTargetKind::LineSegmentStart &&
        dimension.second_target().kind() == SketchReferenceTargetKind::LineSegmentEnd) {
      return &dimension;
    }
  }
  return nullptr;
}

[[nodiscard]] Result<LocalLine> resolve_dimension_line(const PartDocument& document,
                                                       const Sketch& sketch,
                                                       const LineSegment& line) {
  const SketchDrivingDimension* dimension = find_dimension_for_segment(sketch, line.id());
  if (dimension == nullptr) {
    return Result<LocalLine>::success(LocalLine{line.id(), line.start(), line.end()});
  }

  const Parameter* parameter = document.find_parameter(dimension->parameter());
  if (parameter == nullptr) {
    return Result<LocalLine>::failure(validation_error(
        dimension->id().value(), "driving dimension parameter must exist in part document"));
  }

  const double value = parameter->value().millimeters();
  const Vector2 original_direction = vector_between(line.start(), line.end());
  if (length(original_direction) <= k_tolerance) {
    return Result<LocalLine>::failure(validation_error(
        line.id().value(), "dimension-driven region line direction must not be degenerate"));
  }

  Point2 end = line.end();
  switch (dimension->kind()) {
  case SketchDrivingDimensionKind::HorizontalDistance: {
    const double sign = original_direction.x < 0.0 ? -1.0 : 1.0;
    end = Point2{line.start().x + sign * value, line.start().y};
    break;
  }
  case SketchDrivingDimensionKind::VerticalDistance: {
    const double sign = original_direction.y < 0.0 ? -1.0 : 1.0;
    end = Point2{line.start().x, line.start().y + sign * value};
    break;
  }
  case SketchDrivingDimensionKind::AlignedDistance:
  case SketchDrivingDimensionKind::PointToPointDistance: {
    const Vector2 direction = normalize(original_direction);
    end = Point2{line.start().x + direction.x * value, line.start().y + direction.y * value};
    break;
  }
  }

  return Result<LocalLine>::success(LocalLine{line.id(), line.start(), end});
}

[[nodiscard]] Result<std::vector<LocalLine>> collect_local_lines(const PartDocument& document,
                                                                 const Sketch& sketch) {
  std::vector<LocalLine> lines;
  lines.reserve(sketch.line_segments().size() + sketch.reference_generated_lines().size());

  for (const auto& line : sketch.line_segments()) {
    auto resolved = resolve_dimension_line(document, sketch, line);
    if (resolved.has_error()) return Result<std::vector<LocalLine>>::failure(resolved.error());
    lines.push_back(resolved.value());
  }

  ReferenceGeneratedProfileResolver reference_resolver;
  for (const auto& line : sketch.reference_generated_lines()) {
    auto resolved = reference_resolver.resolve_line(document, sketch, line);
    if (resolved.has_error()) return Result<std::vector<LocalLine>>::failure(resolved.error());
    lines.push_back(LocalLine{resolved.value().id(), resolved.value().start(), resolved.value().end()});
  }

  return Result<std::vector<LocalLine>>::success(std::move(lines));
}

[[nodiscard]] std::size_t point_index(std::vector<Point2>& points, Point2 point) {
  for (std::size_t index = 0; index < points.size(); ++index) {
    if (same_point(points[index], point)) return index;
  }
  points.push_back(point);
  return points.size() - 1U;
}

[[nodiscard]] bool same_undirected_segment(const LocalLine& left, const LocalLine& right) noexcept {
  return (same_point(left.start, right.start) && same_point(left.end, right.end)) ||
         (same_point(left.start, right.end) && same_point(left.end, right.start));
}

[[nodiscard]] Result<std::vector<LocalLine>> order_single_loop(std::vector<LocalLine> lines,
                                                               const std::string& object_id) {
  if (lines.size() < 3U) {
    return Result<std::vector<LocalLine>>::failure(
        validation_error(object_id, "automatic region detection requires at least three lines"));
  }

  for (std::size_t i = 0; i < lines.size(); ++i) {
    for (std::size_t j = i + 1U; j < lines.size(); ++j) {
      if (lines[i].id == lines[j].id || same_undirected_segment(lines[i], lines[j])) {
        return Result<std::vector<LocalLine>>::failure(
            validation_error(object_id, "automatic region detection rejects duplicate edges"));
      }
    }
  }

  std::vector<Point2> points;
  std::vector<std::vector<std::size_t>> incident;
  for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
    const std::size_t start = point_index(points, lines[line_index].start);
    const std::size_t end = point_index(points, lines[line_index].end);
    if (start == end) {
      return Result<std::vector<LocalLine>>::failure(
          validation_error(object_id, "automatic region detection rejects degenerate edges"));
    }
    if (incident.size() < points.size()) incident.resize(points.size());
    incident[start].push_back(line_index);
    incident[end].push_back(line_index);
  }

  if (points.size() != lines.size()) {
    return Result<std::vector<LocalLine>>::failure(validation_error(
        object_id, "automatic region detection supports one simple single-contour loop only"));
  }

  for (const auto& edges : incident) {
    if (edges.size() != 2U) {
      return Result<std::vector<LocalLine>>::failure(validation_error(
          object_id, "automatic region detection rejects gaps and branched regions"));
    }
  }

  std::vector<bool> used(lines.size(), false);
  std::vector<LocalLine> ordered;
  ordered.reserve(lines.size());

  LocalLine current = lines.front();
  ordered.push_back(current);
  used[0] = true;
  Point2 loop_start = current.start;
  Point2 cursor = current.end;

  for (std::size_t step = 1U; step < lines.size(); ++step) {
    std::vector<std::size_t> candidates;
    for (std::size_t index = 0; index < lines.size(); ++index) {
      if (used[index]) continue;
      if (same_point(lines[index].start, cursor) || same_point(lines[index].end, cursor)) {
        candidates.push_back(index);
      }
    }

    if (candidates.empty()) {
      return Result<std::vector<LocalLine>>::failure(
          validation_error(object_id, "automatic region detection found a gap in the loop"));
    }
    if (candidates.size() > 1U) {
      return Result<std::vector<LocalLine>>::failure(
          validation_error(object_id, "automatic region detection found an ambiguous branch"));
    }

    LocalLine next = lines[candidates.front()];
    if (same_point(next.end, cursor)) {
      std::swap(next.start, next.end);
    }
    ordered.push_back(next);
    used[candidates.front()] = true;
    cursor = next.end;
  }

  if (!same_point(cursor, loop_start)) {
    return Result<std::vector<LocalLine>>::failure(
        validation_error(object_id, "automatic region detection loop does not close"));
  }

  for (std::size_t i = 0; i < ordered.size(); ++i) {
    for (std::size_t j = i + 1U; j < ordered.size(); ++j) {
      if (are_adjacent_segments(i, j, ordered.size())) continue;
      if (segments_intersect(ordered[i].start, ordered[i].end, ordered[j].start, ordered[j].end)) {
        return Result<std::vector<LocalLine>>::failure(
            validation_error(object_id, "automatic region detection rejects self-intersecting regions"));
      }
    }
  }

  return Result<std::vector<LocalLine>>::success(std::move(ordered));
}

[[nodiscard]] ProfileId generated_region_id(const Sketch& sketch) {
  return ProfileId("generated.region." + sketch.id().value() + ".0");
}

} // namespace

Result<GeneratedClosedProfileCandidate> SketchRegionFinder::find_single_region(
    const PartDocument& document, const Sketch& sketch) const {
  auto lines = collect_local_lines(document, sketch);
  if (lines.has_error()) return Result<GeneratedClosedProfileCandidate>::failure(lines.error());

  auto ordered = order_single_loop(std::move(lines.value()), sketch.id().value());
  if (ordered.has_error()) return Result<GeneratedClosedProfileCandidate>::failure(ordered.error());

  GeneratedClosedProfileCandidate candidate;
  candidate.id = generated_region_id(sketch);
  candidate.line_segments.reserve(ordered.value().size());
  candidate.vertices.reserve(ordered.value().size());
  for (const auto& line : ordered.value()) {
    candidate.line_segments.push_back(line.id);
    candidate.vertices.push_back(line.start);
  }

  return Result<GeneratedClosedProfileCandidate>::success(std::move(candidate));
}

Result<ClosedProfile> SketchRegionFinder::make_closed_profile(
    const GeneratedClosedProfileCandidate& candidate) const {
  return ClosedProfile::create(candidate.id, candidate.line_segments);
}

} // namespace blcad::geometry
