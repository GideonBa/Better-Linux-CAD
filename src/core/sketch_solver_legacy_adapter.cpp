#include "blcad/core/sketch_constraint_solver.hpp"

#include "blcad/core/part_document.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::string topology_entity_id(const SketchEntityId& id) {
  return "entity/" + id.value();
}

[[nodiscard]] Result<SketchSolverTarget>
solver_target(const SketchTopology& topology, const SketchReferenceTarget& target,
              std::string_view object_id) {
  const std::string entity_id = topology_entity_id(target.entity());
  const auto* entity = topology.find_entity(entity_id);
  if (entity == nullptr)
    return Result<SketchSolverTarget>::failure(validation_error(
        std::string(object_id), "persisted Sketch target does not exist in migrated topology"));

  switch (target.kind()) {
  case SketchReferenceTargetKind::LineSegment:
  case SketchReferenceTargetKind::ProjectedPoint:
  case SketchReferenceTargetKind::ProjectedLine:
    return SketchSolverTarget::entity(entity_id);
  case SketchReferenceTargetKind::LineSegmentStart:
    if (entity->points().empty())
      return Result<SketchSolverTarget>::failure(validation_error(
          std::string(object_id), "persisted Sketch start target has no topology point"));
    return SketchSolverTarget::point(entity->points().front());
  case SketchReferenceTargetKind::LineSegmentEnd:
    if (entity->points().size() < 2U)
      return Result<SketchSolverTarget>::failure(validation_error(
          std::string(object_id), "persisted Sketch end target has no topology point"));
    return SketchSolverTarget::point(entity->points().back());
  }
  return Result<SketchSolverTarget>::failure(
      validation_error(std::string(object_id), "unsupported persisted Sketch target kind"));
}

[[nodiscard]] Result<std::vector<SketchSolverTarget>>
solver_targets(const SketchTopology& topology, const SketchReferenceTarget& first,
               const SketchReferenceTarget& second, std::string_view object_id) {
  auto first_target = solver_target(topology, first, object_id);
  if (first_target.has_error())
    return Result<std::vector<SketchSolverTarget>>::failure(first_target.error());
  auto second_target = solver_target(topology, second, object_id);
  if (second_target.has_error())
    return Result<std::vector<SketchSolverTarget>>::failure(second_target.error());
  return Result<std::vector<SketchSolverTarget>>::success(
      {std::move(first_target.value()), std::move(second_target.value())});
}

[[nodiscard]] Result<double> dimension_value(const PartDocument& document,
                                             const SketchDrivingDimension& dimension) {
  const Parameter* parameter = document.find_parameter(dimension.parameter());
  if (parameter == nullptr)
    return Result<double>::failure(validation_error(
        dimension.id().value(), "Sketch driving dimension parameter does not exist"));
  if (parameter->type() != ParameterType::Length)
    return Result<double>::failure(validation_error(
        dimension.id().value(), "Sketch driving distance dimension requires a length parameter"));
  return Result<double>::success(parameter->value().millimeters());
}

[[nodiscard]] Result<double>
signed_axis_dimension_value(const SketchTopology& topology,
                            const std::vector<SketchSolverTarget>& targets, double magnitude,
                            bool horizontal, std::string_view object_id) {
  if (targets.size() != 2U || targets[0].kind() != SketchSolverTargetKind::Point ||
      targets[1].kind() != SketchSolverTargetKind::Point)
    return Result<double>::failure(validation_error(
        std::string(object_id), "axis distance dimension requires two topology point targets"));
  const auto* first = topology.find_point(SketchPointId(targets[0].id()));
  const auto* second = topology.find_point(SketchPointId(targets[1].id()));
  if (first == nullptr || second == nullptr)
    return Result<double>::failure(validation_error(
        std::string(object_id), "axis distance dimension references an unknown topology point"));
  const double delta = horizontal ? second->position().x - first->position().x
                                  : second->position().y - first->position().y;
  return Result<double>::success(delta < 0.0 ? -magnitude : magnitude);
}

} // namespace

Result<SketchConstraintSystem>
SketchConstraintSystemBuilder::from_legacy(const SketchTopology& topology, const Sketch& sketch,
                                           const PartDocument& document) {
  if (topology.sketch() != sketch.id())
    return Result<SketchConstraintSystem>::failure(validation_error(
        sketch.id().value(), "migrated topology and persisted Sketch must reference the same Sketch"));
  if (document.find_sketch(sketch.id()) == nullptr)
    return Result<SketchConstraintSystem>::failure(validation_error(
        sketch.id().value(), "persisted Sketch must belong to the supplied PartDocument"));

  std::vector<SketchSolverConstraint> constraints;

  for (const auto& constraint : sketch.constraints()) {
    auto targets = solver_targets(topology, constraint.constrained_target(),
                                  constraint.reference_target(), constraint.id().value());
    if (targets.has_error())
      return Result<SketchConstraintSystem>::failure(targets.error());
    SketchSolverConstraintKind kind = SketchSolverConstraintKind::Coincident;
    switch (constraint.kind()) {
    case SketchConstraintKind::CoincidentToProjectedPoint:
      kind = SketchSolverConstraintKind::Coincident;
      break;
    case SketchConstraintKind::ParallelToProjectedLine:
      kind = SketchSolverConstraintKind::Parallel;
      break;
    case SketchConstraintKind::CollinearWithProjectedLine:
      kind = SketchSolverConstraintKind::Collinear;
      break;
    }
    auto solver_constraint = SketchSolverConstraint::create(
        "constraint/" + constraint.id().value(), kind, std::move(targets.value()));
    if (solver_constraint.has_error())
      return Result<SketchConstraintSystem>::failure(solver_constraint.error());
    constraints.push_back(std::move(solver_constraint.value()));
  }

  for (const auto& constraint : sketch.geometric_constraints()) {
    std::vector<SketchSolverTarget> targets;
    auto first = solver_target(topology, constraint.first_target(), constraint.id().value());
    if (first.has_error()) return Result<SketchConstraintSystem>::failure(first.error());
    targets.push_back(std::move(first.value()));
    if (constraint.second_target()) {
      auto second = solver_target(topology, *constraint.second_target(), constraint.id().value());
      if (second.has_error()) return Result<SketchConstraintSystem>::failure(second.error());
      targets.push_back(std::move(second.value()));
    }

    SketchSolverConstraintKind kind = SketchSolverConstraintKind::Fixed;
    switch (constraint.kind()) {
    case SketchGeometricConstraintKind::Fixed: kind = SketchSolverConstraintKind::Fixed; break;
    case SketchGeometricConstraintKind::Horizontal:
      kind = SketchSolverConstraintKind::Horizontal;
      break;
    case SketchGeometricConstraintKind::Vertical:
      kind = SketchSolverConstraintKind::Vertical;
      break;
    case SketchGeometricConstraintKind::Parallel:
      kind = SketchSolverConstraintKind::Parallel;
      break;
    case SketchGeometricConstraintKind::Perpendicular:
      kind = SketchSolverConstraintKind::Perpendicular;
      break;
    case SketchGeometricConstraintKind::EqualLength:
      kind = SketchSolverConstraintKind::Equal;
      break;
    }
    auto solver_constraint = SketchSolverConstraint::create(
        "geometric-constraint/" + constraint.id().value(), kind, std::move(targets));
    if (solver_constraint.has_error())
      return Result<SketchConstraintSystem>::failure(solver_constraint.error());
    constraints.push_back(std::move(solver_constraint.value()));
  }

  for (const auto& continuity : sketch.tangent_continuities()) {
    auto first = SketchSolverTarget::entity(topology_entity_id(continuity.first_entity()));
    auto second = SketchSolverTarget::entity(topology_entity_id(continuity.second_entity()));
    if (first.has_error()) return Result<SketchConstraintSystem>::failure(first.error());
    if (second.has_error()) return Result<SketchConstraintSystem>::failure(second.error());
    auto solver_constraint = SketchSolverConstraint::create(
        "tangent/" + continuity.id().value(), SketchSolverConstraintKind::Tangent,
        {std::move(first.value()), std::move(second.value())});
    if (solver_constraint.has_error())
      return Result<SketchConstraintSystem>::failure(solver_constraint.error());
    constraints.push_back(std::move(solver_constraint.value()));
  }

  for (const auto& dimension : sketch.driving_dimensions()) {
    auto targets = solver_targets(topology, dimension.first_target(), dimension.second_target(),
                                  dimension.id().value());
    if (targets.has_error())
      return Result<SketchConstraintSystem>::failure(targets.error());
    auto magnitude = dimension_value(document, dimension);
    if (magnitude.has_error())
      return Result<SketchConstraintSystem>::failure(magnitude.error());

    SketchSolverConstraintKind kind = SketchSolverConstraintKind::AlignedDistance;
    double value = magnitude.value();
    switch (dimension.kind()) {
    case SketchDrivingDimensionKind::HorizontalDistance: {
      kind = SketchSolverConstraintKind::HorizontalDistance;
      auto signed_value = signed_axis_dimension_value(topology, targets.value(), value, true,
                                                     dimension.id().value());
      if (signed_value.has_error())
        return Result<SketchConstraintSystem>::failure(signed_value.error());
      value = signed_value.value();
      break;
    }
    case SketchDrivingDimensionKind::VerticalDistance: {
      kind = SketchSolverConstraintKind::VerticalDistance;
      auto signed_value = signed_axis_dimension_value(topology, targets.value(), value, false,
                                                     dimension.id().value());
      if (signed_value.has_error())
        return Result<SketchConstraintSystem>::failure(signed_value.error());
      value = signed_value.value();
      break;
    }
    case SketchDrivingDimensionKind::AlignedDistance:
    case SketchDrivingDimensionKind::PointToPointDistance:
      kind = SketchSolverConstraintKind::AlignedDistance;
      break;
    }

    auto solver_constraint = SketchSolverConstraint::create(
        "dimension/" + dimension.id().value(), kind, std::move(targets.value()), value);
    if (solver_constraint.has_error())
      return Result<SketchConstraintSystem>::failure(solver_constraint.error());
    constraints.push_back(std::move(solver_constraint.value()));
  }

  return SketchConstraintSystem::create(sketch.id(), std::move(constraints));
}

} // namespace blcad
