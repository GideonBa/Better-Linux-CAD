#include "blcad/geometry/reference_driven_sketch_helper.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<const ProjectedSketchPoint*> find_projected_point_reference(
    const Sketch& sketch, const SketchReferenceTarget& target, const std::string& object_id) {
  if (target.kind() != SketchReferenceTargetKind::ProjectedPoint) {
    return Result<const ProjectedSketchPoint*>::failure(
        validation_error(object_id, "constraint reference target must be a projected point"));
  }

  const ProjectedSketchPoint* point = sketch.find_projected_point(target.entity());
  if (point == nullptr) {
    return Result<const ProjectedSketchPoint*>::failure(
        validation_error(object_id, "projected point reference target must exist in sketch"));
  }

  return Result<const ProjectedSketchPoint*>::success(point);
}

[[nodiscard]] Result<const ProjectedSketchLine*> find_projected_line_reference(
    const Sketch& sketch, const SketchReferenceTarget& target, const std::string& object_id) {
  if (target.kind() != SketchReferenceTargetKind::ProjectedLine) {
    return Result<const ProjectedSketchLine*>::failure(
        validation_error(object_id, "constraint reference target must be a projected line"));
  }

  const ProjectedSketchLine* line = sketch.find_projected_line(target.entity());
  if (line == nullptr) {
    return Result<const ProjectedSketchLine*>::failure(
        validation_error(object_id, "projected line reference target must exist in sketch"));
  }

  return Result<const ProjectedSketchLine*>::success(line);
}

[[nodiscard]] bool is_line_endpoint_target(SketchReferenceTargetKind kind) noexcept {
  return kind == SketchReferenceTargetKind::LineSegmentStart ||
         kind == SketchReferenceTargetKind::LineSegmentEnd;
}

} // namespace

Result<ResolvedCoincidentProjectedPointConstraint>
ReferenceDrivenSketchHelper::resolve_coincident_point_constraint(
    const PartDocument& document, const Sketch& sketch, const SketchConstraint& constraint) const {
  const std::string object_id = constraint.id().value();
  if (constraint.kind() != SketchConstraintKind::CoincidentToProjectedPoint) {
    return Result<ResolvedCoincidentProjectedPointConstraint>::failure(validation_error(
        object_id, "expected coincident-to-projected-point sketch constraint"));
  }

  if (!is_line_endpoint_target(constraint.constrained_target().kind())) {
    return Result<ResolvedCoincidentProjectedPointConstraint>::failure(validation_error(
        object_id, "coincident projected-point constraint must constrain a line endpoint"));
  }

  auto projected_point = find_projected_point_reference(sketch, constraint.reference_target(), object_id);
  if (projected_point.has_error()) {
    return Result<ResolvedCoincidentProjectedPointConstraint>::failure(projected_point.error());
  }

  SketchReferenceProjector projector;
  auto resolved = projector.resolve_point(document, sketch, *projected_point.value());
  if (resolved.has_error()) {
    return Result<ResolvedCoincidentProjectedPointConstraint>::failure(resolved.error());
  }

  return Result<ResolvedCoincidentProjectedPointConstraint>::success(
      ResolvedCoincidentProjectedPointConstraint{constraint.id(),
                                                constraint.constrained_target().entity(),
                                                resolved.value().position});
}

Result<ResolvedProjectedLineConstraint>
ReferenceDrivenSketchHelper::resolve_projected_line_constraint(
    const PartDocument& document, const Sketch& sketch, const SketchConstraint& constraint) const {
  const std::string object_id = constraint.id().value();
  if (constraint.kind() != SketchConstraintKind::ParallelToProjectedLine &&
      constraint.kind() != SketchConstraintKind::CollinearWithProjectedLine) {
    return Result<ResolvedProjectedLineConstraint>::failure(
        validation_error(object_id, "expected projected-line sketch constraint"));
  }

  if (constraint.constrained_target().kind() != SketchReferenceTargetKind::LineSegment) {
    return Result<ResolvedProjectedLineConstraint>::failure(validation_error(
        object_id, "projected-line constraint must constrain a line segment"));
  }

  auto projected_line = find_projected_line_reference(sketch, constraint.reference_target(), object_id);
  if (projected_line.has_error()) {
    return Result<ResolvedProjectedLineConstraint>::failure(projected_line.error());
  }

  SketchReferenceProjector projector;
  auto resolved = projector.resolve_line(document, sketch, *projected_line.value());
  if (resolved.has_error()) {
    return Result<ResolvedProjectedLineConstraint>::failure(resolved.error());
  }

  return Result<ResolvedProjectedLineConstraint>::success(ResolvedProjectedLineConstraint{
      constraint.id(), constraint.constrained_target().entity(), resolved.value().point,
      resolved.value().direction});
}

Result<LineSegment>
ReferenceDrivenSketchHelper::create_profile_helper_line_from_projected_point_constraints(
    const PartDocument& document, const Sketch& sketch, SketchEntityId line_id,
    const SketchConstraint& start_constraint, const SketchConstraint& end_constraint) const {
  const std::string object_id = line_id.empty() ? std::string("reference_profile_helper") : line_id.value();
  if (line_id.empty()) {
    return Result<LineSegment>::failure(
        validation_error(object_id, "profile helper line id must not be empty"));
  }

  if (start_constraint.constrained_target().entity() != line_id ||
      start_constraint.constrained_target().kind() != SketchReferenceTargetKind::LineSegmentStart) {
    return Result<LineSegment>::failure(validation_error(
        object_id, "profile helper start constraint must constrain the helper line start"));
  }

  if (end_constraint.constrained_target().entity() != line_id ||
      end_constraint.constrained_target().kind() != SketchReferenceTargetKind::LineSegmentEnd) {
    return Result<LineSegment>::failure(validation_error(
        object_id, "profile helper end constraint must constrain the helper line end"));
  }

  auto start = resolve_coincident_point_constraint(document, sketch, start_constraint);
  if (start.has_error()) {
    return Result<LineSegment>::failure(start.error());
  }

  auto end = resolve_coincident_point_constraint(document, sketch, end_constraint);
  if (end.has_error()) {
    return Result<LineSegment>::failure(end.error());
  }

  return LineSegment::create(std::move(line_id), start.value().point, end.value().point);
}

} // namespace blcad::geometry
