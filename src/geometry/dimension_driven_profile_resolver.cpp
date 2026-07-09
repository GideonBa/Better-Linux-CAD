#include "blcad/geometry/dimension_driven_profile_resolver.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
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

[[nodiscard]] Result<Point2> resolve_dimension_end_point(const PartDocument& document,
                                                         const LineSegment& original_line,
                                                         Point2 start,
                                                         const SketchDrivingDimension& dimension) {
  const Parameter* parameter = document.find_parameter(dimension.parameter());
  if (parameter == nullptr) {
    return Result<Point2>::failure(validation_error(
        dimension.id().value(), "driving dimension parameter must exist in part document"));
  }

  const double value = parameter->value().millimeters();
  const Vector2 original_direction = vector_between(original_line.start(), original_line.end());
  if (length(original_direction) <= k_tolerance) {
    return Result<Point2>::failure(validation_error(
        original_line.id().value(), "dimension-driven line direction must not be degenerate"));
  }

  switch (dimension.kind()) {
  case SketchDrivingDimensionKind::HorizontalDistance: {
    const double sign = original_direction.x < 0.0 ? -1.0 : 1.0;
    return Result<Point2>::success(Point2{start.x + sign * value, start.y});
  }
  case SketchDrivingDimensionKind::VerticalDistance: {
    const double sign = original_direction.y < 0.0 ? -1.0 : 1.0;
    return Result<Point2>::success(Point2{start.x, start.y + sign * value});
  }
  case SketchDrivingDimensionKind::AlignedDistance:
  case SketchDrivingDimensionKind::PointToPointDistance: {
    const Vector2 direction = normalize(original_direction);
    return Result<Point2>::success(Point2{start.x + direction.x * value, start.y + direction.y * value});
  }
  }

  return Result<Point2>::failure(validation_error(dimension.id().value(),
                                                  "unsupported driving dimension kind"));
}

} // namespace

Result<std::vector<Point2>> DimensionDrivenProfileResolver::resolve_closed_profile_vertices(
    const PartDocument& document, const Sketch& sketch, const ClosedProfile& profile) const {
  if (profile.line_segments().empty()) {
    return Result<std::vector<Point2>>::failure(validation_error(profile.id().value(),
                                                                 "closed profile must contain lines"));
  }

  bool has_dimension = false;
  for (const auto& line_id : profile.line_segments()) {
    if (find_dimension_for_segment(sketch, line_id) != nullptr) {
      has_dimension = true;
      break;
    }
  }

  if (!has_dimension) {
    return sketch.closed_profile_vertices(profile);
  }

  std::vector<Point2> vertices;
  vertices.reserve(profile.line_segments().size());

  const LineSegment* first_line = sketch.find_line_segment(profile.line_segments().front());
  if (first_line == nullptr) {
    return Result<std::vector<Point2>>::failure(validation_error(
        profile.id().value(), "dimension-driven profile requires explicit line segment ids"));
  }
  vertices.push_back(first_line->start());

  for (const auto& line_id : profile.line_segments()) {
    const LineSegment* line = sketch.find_line_segment(line_id);
    if (line == nullptr) {
      return Result<std::vector<Point2>>::failure(validation_error(
          profile.id().value(), "dimension-driven profile requires explicit line segment ids"));
    }

    const Point2 start = vertices.back();
    Point2 end = line->end();
    if (const SketchDrivingDimension* dimension = find_dimension_for_segment(sketch, line_id)) {
      auto resolved_end = resolve_dimension_end_point(document, *line, start, *dimension);
      if (resolved_end.has_error()) {
        return Result<std::vector<Point2>>::failure(resolved_end.error());
      }
      end = resolved_end.value();
    }

    if (line_id == profile.line_segments().back()) {
      if (!same_point(end, vertices.front())) {
        return Result<std::vector<Point2>>::failure(validation_error(
            profile.id().value(), "dimension-driven closed profile must resolve back to its first vertex"));
      }
    } else {
      vertices.push_back(end);
    }
  }

  return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad::geometry
