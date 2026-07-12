#include "blcad/geometry/assembly_hierarchy_revolute_joint_equation_builder.hpp"

#include "assembly_revolute_joint_residual.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<AssemblyHierarchyConstraintTarget> make_query_target(
    const AssemblyHierarchyConstraintEndpoint& endpoint) {
  return AssemblyHierarchyConstraintTarget::create(
      endpoint.occurrence_path(), endpoint.component_instance(), endpoint.semantic_reference());
}

} // namespace

Result<AssemblyHierarchyRevoluteJointEquationDescriptor>
AssemblyHierarchyRevoluteJointEquationBuilder::build(
    const Project& project, const AssemblyHierarchyJoint& joint,
    const Quantity& requested_coordinate) const {
  if (joint.state() != AssemblyJointState::Active) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "hierarchy Revolute equation construction requires an active joint"));
  }
  if (joint.type() != AssemblyJointType::Revolute) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "hierarchy Revolute equation construction requires a Revolute joint"));
  }
  if (requested_coordinate.kind() != QuantityKind::AngleDeg) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "hierarchy Revolute drive coordinate must use degrees"));
  }

  auto query_target_a = make_query_target(joint.target_a());
  if (query_target_a.has_error()) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(
        query_target_a.error());
  }
  auto query_target_b = make_query_target(joint.target_b());
  if (query_target_b.has_error()) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(
        query_target_b.error());
  }

  const AssemblyHierarchyConstraintTargetResolver resolver;
  auto target_a = resolver.resolve_insert(project, query_target_a.value());
  if (target_a.has_error()) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(target_a.error());
  }
  auto target_b = resolver.resolve_insert(project, query_target_b.value());
  if (target_b.has_error()) {
    return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::failure(target_b.error());
  }

  return Result<AssemblyHierarchyRevoluteJointEquationDescriptor>::success(
      AssemblyHierarchyRevoluteJointEquationDescriptor{
          joint.id(), target_a.value(), target_b.value(), requested_coordinate.degrees(),
          detail::build_revolute_joint_residual(
              target_a.value().axis, target_a.value().seating_plane, target_b.value().axis,
              target_b.value().seating_plane, requested_coordinate.degrees())});
}

} // namespace blcad::geometry
