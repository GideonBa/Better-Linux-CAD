#include "blcad/geometry/reference_generated_profile_resolver.hpp"

#include "blcad/geometry/reference_driven_sketch_helper.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
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

[[nodiscard]] double cross(Vector2 left, Vector2 right) noexcept {
  return left.x * right.y - left.y * right.x;
}

[[nodiscard]] bool same_point(Point2 left, Point2 right) noexcept {
  return std::abs(left.x - right.x) <= k_tolerance && std::abs(left.y - right.y) <= k_tolerance;
}

[[nodiscard]] bool same_direction(Vector2 left, Vector2 right) noexcept {
  return std::abs(cross(normalize(left), normalize(right))) <= k_tolerance;
}

[[nodiscard]] const ReferenceGeneratedLine*
find_reference_generated_line(const std::vector<ReferenceGeneratedLine>& lines,
                              SketchEntityId id) noexcept {
  for (const auto& line : lines) {
    if (line.id() == id) {
      return &line;
    }
  }
  return nullptr;
}

} // namespace

Result<LineSegment>
ReferenceGeneratedProfileResolver::resolve_line(const PartDocument& document, const Sketch& sketch,
                                                const ReferenceGeneratedLine& line) const {
  const SketchConstraint* start_constraint = sketch.find_constraint(line.start_constraint());
  if (start_constraint == nullptr) {
    return Result<LineSegment>::failure(validation_error(
        line.id().value(), "reference-generated line start constraint must exist in sketch"));
  }
  const SketchConstraint* end_constraint = sketch.find_constraint(line.end_constraint());
  if (end_constraint == nullptr) {
    return Result<LineSegment>::failure(validation_error(
        line.id().value(), "reference-generated line end constraint must exist in sketch"));
  }

  ReferenceDrivenSketchHelper helper;
  auto resolved = helper.create_profile_helper_line_from_projected_point_constraints(
      document, sketch, line.id(), *start_constraint, *end_constraint);
  if (resolved.has_error()) {
    return Result<LineSegment>::failure(resolved.error());
  }

  if (line.direction_constraint().has_value()) {
    const SketchConstraint* direction_constraint =
        sketch.find_constraint(line.direction_constraint().value());
    if (direction_constraint == nullptr) {
      return Result<LineSegment>::failure(validation_error(
          line.id().value(), "reference-generated line direction constraint must exist in sketch"));
    }

    auto projected_direction =
        helper.resolve_projected_line_constraint(document, sketch, *direction_constraint);
    if (projected_direction.has_error()) {
      return Result<LineSegment>::failure(projected_direction.error());
    }

    const Vector2 line_direction = vector_between(resolved.value().start(), resolved.value().end());
    if (!same_direction(line_direction, projected_direction.value().direction)) {
      return Result<LineSegment>::failure(validation_error(
          line.id().value(),
          "reference-generated line direction must match projected-line constraint"));
    }

    if (direction_constraint->kind() == SketchConstraintKind::CollinearWithProjectedLine) {
      const Vector2 anchor_to_line =
          vector_between(projected_direction.value().point, resolved.value().start());
      if (std::abs(cross(anchor_to_line, projected_direction.value().direction)) > k_tolerance) {
        return Result<LineSegment>::failure(validation_error(
            line.id().value(), "reference-generated line must be collinear with projected line"));
      }
    }
  }

  return resolved;
}

Result<std::vector<Point2>> ReferenceGeneratedProfileResolver::resolve_closed_profile_vertices(
    const PartDocument& document, const Sketch& sketch, const ClosedProfile& profile,
    const std::vector<ReferenceGeneratedLine>& reference_generated_lines) const {
  std::vector<LineSegment> resolved_lines;
  resolved_lines.reserve(profile.line_segments().size());

  for (const auto& id : profile.line_segments()) {
    if (const ReferenceGeneratedLine* generated_line =
            find_reference_generated_line(reference_generated_lines, id)) {
      auto resolved = resolve_line(document, sketch, *generated_line);
      if (resolved.has_error()) {
        return Result<std::vector<Point2>>::failure(resolved.error());
      }
      resolved_lines.push_back(resolved.value());
      continue;
    }

    const LineSegment* explicit_line = sketch.find_line_segment(id);
    if (explicit_line == nullptr) {
      return Result<std::vector<Point2>>::failure(validation_error(
          profile.id().value(),
          "closed profile line id must resolve to an explicit or reference-generated line"));
    }
    resolved_lines.push_back(*explicit_line);
  }

  for (std::size_t index = 0; index < resolved_lines.size(); ++index) {
    const LineSegment& current = resolved_lines[index];
    const LineSegment& next = resolved_lines[(index + 1U) % resolved_lines.size()];
    if (!same_point(current.end(), next.start())) {
      return Result<std::vector<Point2>>::failure(validation_error(
          profile.id().value(),
          "reference-generated closed profile lines must be ordered and connected"));
    }
  }

  std::vector<Point2> vertices;
  vertices.reserve(resolved_lines.size());
  for (const LineSegment& line : resolved_lines) {
    vertices.push_back(line.start());
  }
  return Result<std::vector<Point2>>::success(std::move(vertices));
}

} // namespace blcad::geometry
