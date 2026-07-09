#include "blcad/geometry/sketch_reference_projector.hpp"

#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Vector3 vector_between(Point3 from, Point3 to) noexcept {
  return Vector3{to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] Point3 translated(Point3 point, Vector3 direction) noexcept {
  return Point3{point.x + direction.x, point.y + direction.y, point.z + direction.z};
}

[[nodiscard]] double dot(Vector3 left, Vector3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] double length(Vector2 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

[[nodiscard]] Vector2 normalize(Vector2 vector) noexcept {
  const double vector_length = length(vector);
  return Vector2{vector.x / vector_length, vector.y / vector_length};
}

[[nodiscard]] double signed_distance_to_workplane(const ResolvedWorkplane& workplane,
                                                  Point3 point) noexcept {
  return dot(vector_between(workplane.origin, point), workplane.normal);
}

[[nodiscard]] Result<Point2> project_point_to_workplane(const ResolvedWorkplane& workplane,
                                                        Point3 point,
                                                        const std::string& object_id) {
  const Vector3 relative = vector_between(workplane.origin, point);
  const double distance = dot(relative, workplane.normal);
  if (std::abs(distance) > k_tolerance) {
    return Result<Point2>::failure(validation_error(
        object_id, "projected sketch reference must lie on the sketch workplane"));
  }

  return Result<Point2>::success(Point2{dot(relative, workplane.x_axis), dot(relative, workplane.y_axis)});
}

[[nodiscard]] Result<ResolvedSketchLineReference>
make_projected_line(SketchEntityId id, const ResolvedWorkplane& workplane, Point3 point,
                    Vector3 direction) {
  const std::string object_id = id.value();
  if (std::abs(signed_distance_to_workplane(workplane, point)) > k_tolerance ||
      std::abs(dot(direction, workplane.normal)) > k_tolerance) {
    return Result<ResolvedSketchLineReference>::failure(validation_error(
        object_id, "projected sketch line must lie on the sketch workplane"));
  }

  auto local_point = project_point_to_workplane(workplane, point, object_id);
  if (local_point.has_error()) {
    return Result<ResolvedSketchLineReference>::failure(local_point.error());
  }

  const Point3 second_point = translated(point, direction);
  auto local_second_point = project_point_to_workplane(workplane, second_point, object_id);
  if (local_second_point.has_error()) {
    return Result<ResolvedSketchLineReference>::failure(local_second_point.error());
  }

  Vector2 local_direction{local_second_point.value().x - local_point.value().x,
                          local_second_point.value().y - local_point.value().y};
  if (length(local_direction) <= k_tolerance) {
    return Result<ResolvedSketchLineReference>::failure(
        validation_error(object_id, "projected sketch line direction must not collapse"));
  }

  return Result<ResolvedSketchLineReference>::success(
      ResolvedSketchLineReference{std::move(id), local_point.value(), normalize(local_direction)});
}

} // namespace

Result<ResolvedSketchPointReference>
SketchReferenceProjector::resolve_point(const PartDocument& document, const Sketch& sketch,
                                        const ProjectedSketchPoint& reference) const {
  WorkplaneResolver workplane_resolver;
  auto workplane = workplane_resolver.resolve_for_sketch(document, sketch);
  if (workplane.has_error()) {
    return Result<ResolvedSketchPointReference>::failure(workplane.error());
  }

  Point3 source_point{};
  if (reference.source() == ProjectedSketchPointSource::ConstructionPoint) {
    ConstructionPointResolver resolver;
    auto resolved = resolver.resolve(document, reference.construction_point());
    if (resolved.has_error()) {
      return Result<ResolvedSketchPointReference>::failure(resolved.error());
    }
    source_point = resolved.value().position;
  } else {
    SemanticReferenceEvaluator evaluator;
    auto resolved = evaluator.resolve_vertex(document, reference.semantic_vertex().value());
    if (resolved.has_error()) {
      return Result<ResolvedSketchPointReference>::failure(resolved.error());
    }
    source_point = resolved.value().position;
  }

  auto local_point = project_point_to_workplane(workplane.value(), source_point, reference.id().value());
  if (local_point.has_error()) {
    return Result<ResolvedSketchPointReference>::failure(local_point.error());
  }

  return Result<ResolvedSketchPointReference>::success(
      ResolvedSketchPointReference{reference.id(), local_point.value()});
}

Result<ResolvedSketchLineReference>
SketchReferenceProjector::resolve_line(const PartDocument& document, const Sketch& sketch,
                                       const ProjectedSketchLine& reference) const {
  WorkplaneResolver workplane_resolver;
  auto workplane = workplane_resolver.resolve_for_sketch(document, sketch);
  if (workplane.has_error()) {
    return Result<ResolvedSketchLineReference>::failure(workplane.error());
  }

  if (reference.source() == ProjectedSketchLineSource::ConstructionLine) {
    ConstructionLineResolver resolver;
    auto resolved = resolver.resolve(document, reference.construction_line());
    if (resolved.has_error()) {
      return Result<ResolvedSketchLineReference>::failure(resolved.error());
    }
    return make_projected_line(reference.id(), workplane.value(), resolved.value().point,
                               resolved.value().direction);
  }

  SemanticReferenceEvaluator evaluator;
  auto resolved = evaluator.resolve_edge(document, reference.semantic_edge().value());
  if (resolved.has_error()) {
    return Result<ResolvedSketchLineReference>::failure(resolved.error());
  }

  const Vector3 direction = vector_between(resolved.value().start, resolved.value().end);
  return make_projected_line(reference.id(), workplane.value(), resolved.value().start, direction);
}

} // namespace blcad::geometry
