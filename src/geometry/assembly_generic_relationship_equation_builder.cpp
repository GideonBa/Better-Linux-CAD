#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include <cmath>
#include <numbers>
#include <string>
#include <type_traits>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool supported_relationship(AssemblyConstraintType type) noexcept {
  return type == AssemblyConstraintType::Coincident || type == AssemblyConstraintType::Parallel ||
         type == AssemblyConstraintType::Perpendicular || type == AssemblyConstraintType::Angle;
}

[[nodiscard]] Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return Vector3{target.x - source.x, target.y - source.y, target.z - source.z};
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] Point3 evaluate_point_to_root(const AssemblyResolvedGeometricTarget& target,
                                            const Point3& point) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return point;
  }
  const AssemblyHierarchyTransformEvaluator evaluator;
  return evaluator.evaluate_point(target.transforms_inner_to_outer, point);
}

[[nodiscard]] Vector3 evaluate_vector_to_root(const AssemblyResolvedGeometricTarget& target,
                                              const Vector3& vector) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return vector;
  }
  const AssemblyHierarchyTransformEvaluator evaluator;
  return evaluator.evaluate_vector(target.transforms_inner_to_outer, vector);
}

[[nodiscard]] Result<Point3> selected_point(const AssemblyResolvedGeometricTarget& target,
                                            AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Point) {
    return Result<Point3>::failure(
        validation_error("geometry.assembly_generic_relationship",
                         "generic Coincident point projection requires Point compatibility"));
  }
  auto point = project_point(target);
  if (point.has_error()) {
    return Result<Point3>::failure(point.error());
  }
  return Result<Point3>::success(evaluate_point_to_root(target, point.value().point));
}

[[nodiscard]] Result<AssemblyLineTargetDescriptor>
selected_line(const AssemblyResolvedGeometricTarget& target,
              AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Line) {
    return Result<AssemblyLineTargetDescriptor>::failure(
        validation_error("geometry.assembly_generic_relationship",
                         "generic Coincident line projection requires Line compatibility"));
  }
  auto line = project_line(target);
  if (line.has_error()) {
    return Result<AssemblyLineTargetDescriptor>::failure(line.error());
  }
  return Result<AssemblyLineTargetDescriptor>::success(
      AssemblyLineTargetDescriptor{evaluate_point_to_root(target, line.value().origin),
                                   evaluate_vector_to_root(target, line.value().direction)});
}

[[nodiscard]] Result<AssemblyPlanarTargetDescriptor>
selected_plane(const AssemblyResolvedGeometricTarget& target,
               AssemblyGeometricTargetCapability capability) {
  if (capability != AssemblyGeometricTargetCapability::Plane) {
    return Result<AssemblyPlanarTargetDescriptor>::failure(
        validation_error("geometry.assembly_generic_relationship",
                         "generic Coincident plane projection requires Plane compatibility"));
  }
  auto plane = project_plane(target);
  if (plane.has_error()) {
    return Result<AssemblyPlanarTargetDescriptor>::failure(plane.error());
  }
  return Result<AssemblyPlanarTargetDescriptor>::success(
      AssemblyPlanarTargetDescriptor{evaluate_point_to_root(target, plane.value().origin),
                                     evaluate_vector_to_root(target, plane.value().x_axis),
                                     evaluate_vector_to_root(target, plane.value().y_axis),
                                     evaluate_vector_to_root(target, plane.value().normal)});
}

[[nodiscard]] Result<Vector3> selected_direction(const AssemblyResolvedGeometricTarget& target,
                                                 AssemblyGeometricTargetCapability capability) {
  switch (capability) {
  case AssemblyGeometricTargetCapability::Line: {
    auto line = project_line(target);
    if (line.has_error()) {
      return Result<Vector3>::failure(line.error());
    }
    return Result<Vector3>::success(evaluate_vector_to_root(target, line.value().direction));
  }
  case AssemblyGeometricTargetCapability::Axis: {
    auto axis = project_axis(target);
    if (axis.has_error()) {
      return Result<Vector3>::failure(axis.error());
    }
    return Result<Vector3>::success(evaluate_vector_to_root(target, axis.value().direction));
  }
  case AssemblyGeometricTargetCapability::Plane: {
    auto plane = project_plane(target);
    if (plane.has_error()) {
      return Result<Vector3>::failure(plane.error());
    }
    return Result<Vector3>::success(evaluate_vector_to_root(target, plane.value().normal));
  }
  case AssemblyGeometricTargetCapability::Point:
  case AssemblyGeometricTargetCapability::Circle:
  case AssemblyGeometricTargetCapability::Cylinder:
  case AssemblyGeometricTargetCapability::Frame:
    break;
  }
  return Result<Vector3>::failure(validation_error(
      "geometry.assembly_generic_relationship",
      "generic directional relationship requires Line, Axis, or Plane compatibility"));
}

} // namespace

Result<AssemblyGenericRelationshipEquationDescriptor>
AssemblyGenericRelationshipEquationBuilder::build(const Project& project,
                                                  const AssemblyConstraint& constraint) const {
  if (constraint.state() != AssemblyConstraintState::Active) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "generic relationship equation construction requires an active relationship"));
  }
  if (!supported_relationship(constraint.type())) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        constraint.id().value(),
        "generic relationship equation construction requires Coincident, Parallel, "
        "Perpendicular, or Angle intent"));
  }

  const AssemblyConstraintTargetResolver resolver;
  auto target_a = resolver.resolve_geometric(project, constraint.target_a());
  if (target_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolver.resolve_geometric(project, constraint.target_b());
  if (target_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(target_b.error());
  }
  return build(constraint.id(), constraint.type(), target_a.value(), target_b.value(),
               constraint.angle());
}

Result<AssemblyGenericRelationshipEquationDescriptor>
AssemblyGenericRelationshipEquationBuilder::build(AssemblyConstraintId relationship,
                                                  AssemblyConstraintType type,
                                                  const AssemblyResolvedGeometricTarget& target_a,
                                                  const AssemblyResolvedGeometricTarget& target_b,
                                                  std::optional<Quantity> angle) const {
  if (!supported_relationship(type)) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
        relationship.value(),
        "generic relationship equation construction requires Coincident, Parallel, "
        "Perpendicular, or Angle intent"));
  }

  auto valid_a = validate_resolved_geometric_target(target_a);
  if (valid_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(valid_a.error());
  }
  auto valid_b = validate_resolved_geometric_target(target_b);
  if (valid_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(valid_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(type, target_a, target_b);
  if (compatibility.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(compatibility.error());
  }

  const auto capability_a = compatibility.value().target_a_capability;
  const auto capability_b = compatibility.value().target_b_capability;
  const AssemblyGenericRelationshipTargetDescriptor equation_target_a{target_a, capability_a};
  const AssemblyGenericRelationshipTargetDescriptor equation_target_b{target_b, capability_b};

  if (type == AssemblyConstraintType::Coincident) {
    if (capability_a == AssemblyGeometricTargetCapability::Point &&
        capability_b == AssemblyGeometricTargetCapability::Point) {
      auto point_a = selected_point(target_a, capability_a);
      auto point_b = selected_point(target_b, capability_b);
      if (point_a.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_a.error());
      }
      if (point_b.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_b.error());
      }
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{CoincidentPointPointResidualDescriptor{
              difference(point_b.value(), point_a.value())}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }

    const bool point_line = capability_a == AssemblyGeometricTargetCapability::Point &&
                            capability_b == AssemblyGeometricTargetCapability::Line;
    const bool line_point = capability_a == AssemblyGeometricTargetCapability::Line &&
                            capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_line || line_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_line ? AssemblyGenericRelationshipTargetRole::TargetA
                     : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_line ? selected_point(target_a, capability_a)
                              : selected_point(target_b, capability_b);
      auto line = point_line ? selected_line(target_b, capability_b)
                             : selected_line(target_a, capability_a);
      if (point.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      }
      if (line.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(line.error());
      }
      const Vector3 point_from_line = difference(point.value(), line.value().origin);
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{CoincidentPointLineResidualDescriptor{
              point_role, cross(point_from_line, line.value().direction)}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }

    const bool point_plane = capability_a == AssemblyGeometricTargetCapability::Point &&
                             capability_b == AssemblyGeometricTargetCapability::Plane;
    const bool plane_point = capability_a == AssemblyGeometricTargetCapability::Plane &&
                             capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_plane || plane_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_plane ? AssemblyGenericRelationshipTargetRole::TargetA
                      : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_plane ? selected_point(target_a, capability_a)
                               : selected_point(target_b, capability_b);
      auto plane = point_plane ? selected_plane(target_b, capability_b)
                               : selected_plane(target_a, capability_a);
      if (point.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      }
      if (plane.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(plane.error());
      }
      const double signed_distance =
          dot(difference(point.value(), plane.value().origin), plane.value().normal);
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{
              CoincidentPointPlaneResidualDescriptor{point_role, signed_distance}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }
  }

  auto direction_a = selected_direction(target_a, capability_a);
  if (direction_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_a.error());
  }
  auto direction_b = selected_direction(target_b, capability_b);
  if (direction_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_b.error());
  }

  if (type == AssemblyConstraintType::Parallel) {
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{
            ParallelDirectionResidualDescriptor{cross(direction_a.value(), direction_b.value())}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }
  if (type == AssemblyConstraintType::Perpendicular) {
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{PerpendicularDirectionResidualDescriptor{
            dot(direction_a.value(), direction_b.value())}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }
  if (type == AssemblyConstraintType::Angle) {
    if (!angle.has_value() || angle->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
          relationship.value(),
          "directional Angle equation construction requires an angle value in degrees"));
    }
    const double target_angle_deg = angle->degrees();
    const double target_cosine = std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
    const double direction_dot = dot(direction_a.value(), direction_b.value());
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{DirectionalAngleResidualDescriptor{
            target_angle_deg, direction_dot, direction_dot - target_cosine}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }

  return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
      relationship.value(), "generic relationship compatibility selected an unsupported pair"));
}

} // namespace blcad::geometry
