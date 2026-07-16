#pragma once

#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

enum class SketchSplineRepresentation { ControlPoints, FitPoints };

[[nodiscard]] constexpr std::string_view
to_string(SketchSplineRepresentation representation) noexcept {
  switch (representation) {
  case SketchSplineRepresentation::ControlPoints: return "control_points";
  case SketchSplineRepresentation::FitPoints: return "fit_points";
  }
  return "control_points";
}

enum class SketchSplineHandleKind { Endpoint, ControlPoint, FitPoint, Tangent };

[[nodiscard]] constexpr std::string_view to_string(SketchSplineHandleKind kind) noexcept {
  switch (kind) {
  case SketchSplineHandleKind::Endpoint: return "endpoint";
  case SketchSplineHandleKind::ControlPoint: return "control_point";
  case SketchSplineHandleKind::FitPoint: return "fit_point";
  case SketchSplineHandleKind::Tangent: return "tangent";
  }
  return "control_point";
}

struct SketchSplineHandle {
  std::string id;
  SketchSplineHandleKind kind{SketchSplineHandleKind::ControlPoint};
  std::size_t segment_index{0U};
  std::size_t point_index{0U};
  Point2 position;

  friend bool operator==(const SketchSplineHandle&, const SketchSplineHandle&) = default;
};

struct SketchSplineContinuityHandle {
  std::string id;
  std::size_t first_segment{0U};
  std::size_t second_segment{0U};
  Point2 junction;
  Point2 incoming_tangent;
  Point2 outgoing_tangent;
  bool position_continuous{false};
  bool tangent_continuous{false};
};

struct SketchSplineEditPreview {
  SketchSplineRepresentation representation{SketchSplineRepresentation::ControlPoints};
  std::vector<SplineSegment> segments;
  std::vector<SketchSplineHandle> handles;
  std::vector<SketchSplineContinuityHandle> continuity_handles;
  std::vector<std::vector<Point2>> control_polygons;
};

// Block-113 headless spline-authoring authority. The persistent representation
// remains the existing cubic SplineSegment chain. Fit points, handle selection,
// and control polygons are authoring state and are deterministically expanded
// before one complete Sketch replacement is committed.
class SketchSplineEditModel {
public:
  [[nodiscard]] static Result<SketchSplineEditModel>
  from_segments(const Sketch& sketch, std::vector<SketchEntityId> ordered_entities) {
    const std::string object_id = sketch.id().empty() ? "sketch_spline_edit" : sketch.id().value();
    if (ordered_entities.empty())
      return Result<SketchSplineEditModel>::failure(
          Error::validation(object_id, "spline edit requires at least one spline entity"));

    std::set<std::string> unique;
    std::vector<SplineSegment> segments;
    segments.reserve(ordered_entities.size());
    for (const auto& id : ordered_entities) {
      if (id.empty() || !unique.insert(id.value()).second)
        return Result<SketchSplineEditModel>::failure(
            Error::validation(object_id, "spline edit entity ids must be non-empty and unique"));
      const auto* segment = sketch.find_spline_segment(id);
      if (segment == nullptr)
        return Result<SketchSplineEditModel>::failure(
            Error::validation(id.value(), "spline edit entity must exist in the Sketch"));
      segments.push_back(*segment);
    }
    for (std::size_t index = 1U; index < segments.size(); ++index)
      if (!same_point(segments[index - 1U].end(), segments[index].start()))
        return Result<SketchSplineEditModel>::failure(Error::validation(
            ordered_entities[index].value(), "ordered spline edit entities must form a connected chain"));

    return Result<SketchSplineEditModel>::success(SketchSplineEditModel(
        sketch.id(), std::move(ordered_entities), std::move(segments)));
  }

  [[nodiscard]] static Result<SketchSplineEditModel>
  from_fit_points(SketchId sketch, SketchEntityId base_entity, std::vector<Point2> fit_points) {
    const std::string object_id = base_entity.empty() ? "sketch_spline_edit" : base_entity.value();
    if (sketch.empty() || base_entity.empty())
      return Result<SketchSplineEditModel>::failure(
          Error::validation(object_id, "fit spline requires Sketch and base entity ids"));
    if (fit_points.size() < 2U)
      return Result<SketchSplineEditModel>::failure(
          Error::validation(object_id, "fit spline requires at least two fit points"));
    for (const auto point : fit_points)
      if (!finite(point))
        return Result<SketchSplineEditModel>::failure(
            Error::validation(object_id, "fit spline points must be finite"));
    for (std::size_t index = 1U; index < fit_points.size(); ++index)
      if (same_point(fit_points[index - 1U], fit_points[index]))
        return Result<SketchSplineEditModel>::failure(
            Error::validation(object_id, "consecutive fit spline points must be distinct"));

    SketchSplineEditModel model(sketch, {base_entity}, {});
    model.representation_ = SketchSplineRepresentation::FitPoints;
    model.fit_points_ = std::move(fit_points);
    auto rebuilt = model.rebuild_from_fit_points();
    if (rebuilt.has_error())
      return Result<SketchSplineEditModel>::failure(rebuilt.error());
    return Result<SketchSplineEditModel>::success(std::move(model));
  }

  [[nodiscard]] const SketchId& sketch() const noexcept { return sketch_; }
  [[nodiscard]] const std::vector<SketchEntityId>& source_entities() const noexcept {
    return source_entities_;
  }
  [[nodiscard]] SketchSplineRepresentation representation() const noexcept {
    return representation_;
  }
  [[nodiscard]] const std::vector<SplineSegment>& segments() const noexcept { return segments_; }
  [[nodiscard]] const std::vector<Point2>& fit_points() const noexcept { return fit_points_; }

  [[nodiscard]] Result<std::size_t> convert_to_fit_points() {
    if (representation_ == SketchSplineRepresentation::FitPoints)
      return Result<std::size_t>::success(fit_points_.size());
    fit_points_.clear();
    fit_points_.reserve(segments_.size() * 2U + 1U);
    fit_points_.push_back(segments_.front().start());
    for (const auto& segment : segments_) {
      fit_points_.push_back(evaluate(segment, 0.5));
      fit_points_.push_back(segment.end());
    }
    representation_ = SketchSplineRepresentation::FitPoints;
    return rebuild_from_fit_points();
  }

  [[nodiscard]] Result<std::size_t> convert_to_control_points() {
    if (representation_ == SketchSplineRepresentation::ControlPoints)
      return Result<std::size_t>::success(segments_.size());
    auto rebuilt = rebuild_from_fit_points();
    if (rebuilt.has_error()) return rebuilt;
    representation_ = SketchSplineRepresentation::ControlPoints;
    return Result<std::size_t>::success(segments_.size());
  }

  [[nodiscard]] Result<std::size_t> move_control_point(std::size_t segment_index,
                                                       std::size_t point_index,
                                                       Point2 position) {
    if (representation_ != SketchSplineRepresentation::ControlPoints)
      return Result<std::size_t>::failure(Error::validation(
          sketch_.value(), "control-point editing requires control-point representation"));
    if (segment_index >= segments_.size() || point_index > 3U || !finite(position))
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "invalid spline control-point edit target"));

    auto points = control_points(segments_[segment_index]);
    points[point_index] = position;
    auto replacement = SplineSegment::create_cubic_bezier(
        segments_[segment_index].id(), points[0], points[1], points[2], points[3]);
    if (replacement.has_error()) return Result<std::size_t>::failure(replacement.error());
    segments_[segment_index] = std::move(replacement.value());

    // Preserve explicit chain connectivity when an endpoint is moved.
    if (point_index == 0U && segment_index > 0U) {
      auto previous = control_points(segments_[segment_index - 1U]);
      previous[3] = position;
      auto updated = SplineSegment::create_cubic_bezier(
          segments_[segment_index - 1U].id(), previous[0], previous[1], previous[2], previous[3]);
      if (updated.has_error()) return Result<std::size_t>::failure(updated.error());
      segments_[segment_index - 1U] = std::move(updated.value());
    }
    if (point_index == 3U && segment_index + 1U < segments_.size()) {
      auto next = control_points(segments_[segment_index + 1U]);
      next[0] = position;
      auto updated = SplineSegment::create_cubic_bezier(
          segments_[segment_index + 1U].id(), next[0], next[1], next[2], next[3]);
      if (updated.has_error()) return Result<std::size_t>::failure(updated.error());
      segments_[segment_index + 1U] = std::move(updated.value());
    }
    return Result<std::size_t>::success(segment_index);
  }

  [[nodiscard]] Result<std::size_t> move_fit_point(std::size_t index, Point2 position) {
    if (representation_ != SketchSplineRepresentation::FitPoints)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit-point editing requires fit-point representation"));
    if (index >= fit_points_.size() || !finite(position))
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "invalid spline fit-point edit target"));
    if ((index > 0U && same_point(position, fit_points_[index - 1U])) ||
        (index + 1U < fit_points_.size() && same_point(position, fit_points_[index + 1U])))
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "consecutive fit spline points must be distinct"));
    fit_points_[index] = position;
    return rebuild_from_fit_points();
  }

  [[nodiscard]] Result<std::size_t> insert_fit_point(std::size_t index, Point2 position) {
    if (representation_ != SketchSplineRepresentation::FitPoints)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit-point insertion requires fit-point representation"));
    if (index > fit_points_.size() || !finite(position))
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "invalid fit-point insertion target"));
    if ((index > 0U && same_point(position, fit_points_[index - 1U])) ||
        (index < fit_points_.size() && same_point(position, fit_points_[index])))
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "inserted fit point must be distinct from its neighbours"));
    fit_points_.insert(fit_points_.begin() + static_cast<std::ptrdiff_t>(index), position);
    auto rebuilt = rebuild_from_fit_points();
    if (rebuilt.has_error()) return rebuilt;
    return Result<std::size_t>::success(index);
  }

  [[nodiscard]] Result<std::size_t> remove_fit_point(std::size_t index) {
    if (representation_ != SketchSplineRepresentation::FitPoints)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit-point removal requires fit-point representation"));
    if (index >= fit_points_.size())
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit-point removal target is out of range"));
    if (fit_points_.size() <= 2U)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit spline must retain at least two points"));
    fit_points_.erase(fit_points_.begin() + static_cast<std::ptrdiff_t>(index));
    auto rebuilt = rebuild_from_fit_points();
    if (rebuilt.has_error()) return rebuilt;
    return Result<std::size_t>::success(index);
  }

  [[nodiscard]] Result<std::size_t> align_tangent(std::size_t junction_index) {
    if (junction_index + 1U >= segments_.size())
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "spline tangent junction is out of range"));
    auto first = control_points(segments_[junction_index]);
    auto second = control_points(segments_[junction_index + 1U]);
    const Point2 junction = first[3];
    const double incoming_length = distance(first[2], junction);
    const double outgoing_length = distance(junction, second[1]);
    Point2 direction{junction.x - first[2].x, junction.y - first[2].y};
    double magnitude = std::hypot(direction.x, direction.y);
    if (magnitude <= kTolerance) {
      direction = {second[1].x - junction.x, second[1].y - junction.y};
      magnitude = std::hypot(direction.x, direction.y);
    }
    if (magnitude <= kTolerance)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "spline tangent handles must not both collapse"));
    direction = {direction.x / magnitude, direction.y / magnitude};
    first[2] = {junction.x - direction.x * incoming_length,
                junction.y - direction.y * incoming_length};
    second[0] = junction;
    second[1] = {junction.x + direction.x * outgoing_length,
                 junction.y + direction.y * outgoing_length};
    auto first_segment = SplineSegment::create_cubic_bezier(
        segments_[junction_index].id(), first[0], first[1], first[2], first[3]);
    auto second_segment = SplineSegment::create_cubic_bezier(
        segments_[junction_index + 1U].id(), second[0], second[1], second[2], second[3]);
    if (first_segment.has_error()) return Result<std::size_t>::failure(first_segment.error());
    if (second_segment.has_error()) return Result<std::size_t>::failure(second_segment.error());
    segments_[junction_index] = std::move(first_segment.value());
    segments_[junction_index + 1U] = std::move(second_segment.value());
    return Result<std::size_t>::success(junction_index);
  }

  [[nodiscard]] SketchSplineEditPreview preview() const {
    SketchSplineEditPreview result;
    result.representation = representation_;
    result.segments = segments_;
    result.control_polygons.reserve(segments_.size());
    for (std::size_t segment_index = 0U; segment_index < segments_.size(); ++segment_index) {
      const auto points = control_points(segments_[segment_index]);
      result.control_polygons.push_back({points[0], points[1], points[2], points[3]});
      if (representation_ == SketchSplineRepresentation::ControlPoints) {
        for (std::size_t point_index = 0U; point_index < points.size(); ++point_index)
          result.handles.push_back({"spline/" + segments_[segment_index].id().value() + "/control/" +
                                        std::to_string(point_index),
                                    point_index == 0U || point_index == 3U
                                        ? SketchSplineHandleKind::Endpoint
                                        : SketchSplineHandleKind::ControlPoint,
                                    segment_index, point_index, points[point_index]});
      }
    }
    if (representation_ == SketchSplineRepresentation::FitPoints)
      for (std::size_t index = 0U; index < fit_points_.size(); ++index)
        result.handles.push_back({"spline/fit/" + std::to_string(index),
                                  SketchSplineHandleKind::FitPoint, 0U, index, fit_points_[index]});
    result.continuity_handles = continuity_handles();
    return result;
  }

  [[nodiscard]] Result<Sketch> build_sketch(const Sketch& source,
                                            bool persist_internal_tangency = true) const {
    if (source.id() != sketch_)
      return Result<Sketch>::failure(
          Error::validation(sketch_.value(), "spline edit source belongs to another Sketch"));

    std::set<std::string> selected;
    for (const auto& id : source_entities_) selected.insert(id.value());
    std::set<std::string> generated;
    for (const auto& segment : segments_) generated.insert(segment.id().value());

    const auto removed = [&](const SketchEntityId& id) {
      return selected.contains(id.value()) && !generated.contains(id.value());
    };
    for (const auto& profile : source.arc_closed_profiles())
      for (const auto& id : profile.curve_segments())
        if (removed(id))
          return Result<Sketch>::failure(Error::dependency(
              id.value(), "spline insertion/removal would invalidate an arc closed profile"));
    for (const auto& profile : source.composite_closed_profiles()) {
      for (const auto& id : profile.outer_contour())
        if (removed(id))
          return Result<Sketch>::failure(Error::dependency(
              id.value(), "spline insertion/removal would invalidate a composite profile"));
      for (const auto& contour : profile.inner_contours())
        for (const auto& id : contour)
          if (removed(id))
            return Result<Sketch>::failure(Error::dependency(
                id.value(), "spline insertion/removal would invalidate a composite profile"));
    }

    auto candidate = Sketch::create(source.id(), source.name(), source.workplane());
    if (candidate.has_error()) return candidate;
    for (const auto& line : source.line_segments()) {
      auto added = candidate.value().add_entity(line);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& arc : source.arc_segments()) {
      auto added = candidate.value().add_entity(arc);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& spline : source.spline_segments())
      if (!selected.contains(spline.id().value())) {
        auto added = candidate.value().add_entity(spline);
        if (added.has_error()) return Result<Sketch>::failure(added.error());
      }
    for (const auto& spline : segments_) {
      auto added = candidate.value().add_entity(spline);
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
      const bool first_selected = selected.contains(continuity.first_entity().value());
      const bool second_selected = selected.contains(continuity.second_entity().value());
      if (first_selected && second_selected) continue;
      if (removed(continuity.first_entity()) || removed(continuity.second_entity()))
        return Result<Sketch>::failure(Error::dependency(
            continuity.id().value(), "spline insertion/removal would invalidate tangent continuity"));
      auto added = candidate.value().add_tangent_continuity(continuity);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    if (persist_internal_tangency)
      for (std::size_t index = 1U; index < segments_.size(); ++index) {
        auto continuity = SketchTangentContinuity::create_tangent(
            SketchConstraintId("constraint.spline.edit.tangent." + std::to_string(index)),
            segments_[index - 1U].id(), segments_[index].id());
        if (continuity.has_error()) return Result<Sketch>::failure(continuity.error());
        auto added = candidate.value().add_tangent_continuity(std::move(continuity.value()));
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

private:
  static constexpr double kTolerance = 1.0e-9;

  SketchSplineEditModel(SketchId sketch, std::vector<SketchEntityId> source_entities,
                        std::vector<SplineSegment> segments)
      : sketch_(std::move(sketch)), source_entities_(std::move(source_entities)),
        segments_(std::move(segments)) {}

  [[nodiscard]] static bool finite(Point2 point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
  }
  [[nodiscard]] static double distance(Point2 first, Point2 second) noexcept {
    return std::hypot(second.x - first.x, second.y - first.y);
  }
  [[nodiscard]] static bool same_point(Point2 first, Point2 second) noexcept {
    return distance(first, second) <= kTolerance;
  }
  [[nodiscard]] static Point2 evaluate(const SplineSegment& segment, double t) noexcept {
    const double u = 1.0 - t;
    const double b0 = u * u * u;
    const double b1 = 3.0 * u * u * t;
    const double b2 = 3.0 * u * t * t;
    const double b3 = t * t * t;
    return {b0 * segment.start().x + b1 * segment.control1().x +
                b2 * segment.control2().x + b3 * segment.end().x,
            b0 * segment.start().y + b1 * segment.control1().y +
                b2 * segment.control2().y + b3 * segment.end().y};
  }
  [[nodiscard]] static std::vector<Point2> control_points(const SplineSegment& segment) {
    return {segment.start(), segment.control1(), segment.control2(), segment.end()};
  }

  [[nodiscard]] SketchEntityId generated_id(std::size_t index) const {
    if (index < source_entities_.size()) return source_entities_[index];
    const std::string base = source_entities_.empty() ? "spline.edit" : source_entities_.front().value();
    return SketchEntityId(base + ".fit." + std::to_string(index + 1U));
  }

  [[nodiscard]] Result<std::size_t> rebuild_from_fit_points() {
    if (fit_points_.size() < 2U)
      return Result<std::size_t>::failure(
          Error::validation(sketch_.value(), "fit spline requires at least two points"));
    std::vector<SplineSegment> rebuilt;
    rebuilt.reserve(fit_points_.size() - 1U);
    for (std::size_t index = 0U; index + 1U < fit_points_.size(); ++index) {
      const Point2 p0 = index == 0U ? fit_points_[index] : fit_points_[index - 1U];
      const Point2 p1 = fit_points_[index];
      const Point2 p2 = fit_points_[index + 1U];
      const Point2 p3 = index + 2U < fit_points_.size() ? fit_points_[index + 2U] : p2;
      const Point2 control1{p1.x + (p2.x - p0.x) / 6.0,
                            p1.y + (p2.y - p0.y) / 6.0};
      const Point2 control2{p2.x - (p3.x - p1.x) / 6.0,
                            p2.y - (p3.y - p1.y) / 6.0};
      auto segment = SplineSegment::create_cubic_bezier(generated_id(index), p1, control1,
                                                        control2, p2);
      if (segment.has_error()) return Result<std::size_t>::failure(segment.error());
      rebuilt.push_back(std::move(segment.value()));
    }
    segments_ = std::move(rebuilt);
    return Result<std::size_t>::success(segments_.size());
  }

  [[nodiscard]] std::vector<SketchSplineContinuityHandle> continuity_handles() const {
    std::vector<SketchSplineContinuityHandle> handles;
    for (std::size_t index = 1U; index < segments_.size(); ++index) {
      const auto& first = segments_[index - 1U];
      const auto& second = segments_[index];
      const Point2 incoming{first.end().x - first.control2().x,
                            first.end().y - first.control2().y};
      const Point2 outgoing{second.control1().x - second.start().x,
                            second.control1().y - second.start().y};
      const double cross = incoming.x * outgoing.y - incoming.y * outgoing.x;
      const double scale = std::hypot(incoming.x, incoming.y) * std::hypot(outgoing.x, outgoing.y);
      handles.push_back({"spline/continuity/" + std::to_string(index), index - 1U, index,
                         first.end(), incoming, outgoing, same_point(first.end(), second.start()),
                         scale > kTolerance && std::abs(cross) <= kTolerance * scale});
    }
    return handles;
  }

  SketchId sketch_;
  std::vector<SketchEntityId> source_entities_;
  SketchSplineRepresentation representation_{SketchSplineRepresentation::ControlPoints};
  std::vector<SplineSegment> segments_;
  std::vector<Point2> fit_points_;
};

} // namespace blcad
