#include "assembly_cross_hierarchy_motion_numeric_system.hpp"

#include "assembly_cross_hierarchy_numeric_system.hpp"

#include "blcad/geometry/assembly_cylindrical_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_cylindrical_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_prismatic_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_revolute_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<std::size_t>
append_local_joint_drive_residuals(const Project& project,
                                   const AssemblyLocalJointIdentity& identity,
                                   double length_residual_scale_mm, NumericVector& residuals) {
  auto local_project =
      make_cross_hierarchy_local_evaluation_view(project, identity.assembly_document);
  if (local_project.has_error()) {
    return Result<std::size_t>::failure(local_project.error());
  }
  const AssemblyJoint* joint = local_project.value().assembly().find_joint(identity.joint);
  if (joint == nullptr) {
    return Result<std::size_t>::failure(
        validation_error(identity.joint.value(),
                         "cross-hierarchy motion local joint must exist in containing assembly"));
  }
  if (joint->type() == AssemblyJointType::Revolute) {
    const AssemblyRevoluteJointEquationBuilder builder;
    auto equation =
        builder.build(local_project.value(), *joint,
                      joint->find_coordinate_slot(AssemblyJointCoordinateRole::Rotation)->value());
    if (equation.has_error())
      return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
  }
  if (joint->type() == AssemblyJointType::Prismatic) {
    const AssemblyPrismaticJointEquationBuilder builder;
    auto equation = builder.build(
        local_project.value(), *joint,
        joint->find_coordinate_slot(AssemblyJointCoordinateRole::Translation)->value());
    if (equation.has_error())
      return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
  }
  const AssemblyCylindricalJointEquationBuilder builder;
  auto equation =
      builder.build(local_project.value(), *joint,
                    joint->find_coordinate_slot(AssemblyJointCoordinateRole::Translation)->value(),
                    joint->find_coordinate_slot(AssemblyJointCoordinateRole::Rotation)->value());
  if (equation.has_error())
    return Result<std::size_t>::failure(equation.error());
  return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
}

[[nodiscard]] Result<std::size_t> append_cross_joint_drive_residuals(
    const Project& project, const AssemblyProjectCrossHierarchyJointIdentity& identity,
    AssemblyJointId selected_joint,
    const std::vector<AssemblyJointCoordinateDrive>& selected_coordinates,
    double length_residual_scale_mm, NumericVector& residuals) {
  const AssemblyHierarchyJoint* joint = project.find_cross_hierarchy_joint(identity.joint);
  if (joint == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        identity.joint.value(), "cross-hierarchy motion joint must exist in project"));
  }
  const auto requested = [&](AssemblyJointCoordinateRole role) -> const Quantity& {
    if (joint->id() != selected_joint)
      return joint->find_coordinate_slot(role)->value();
    return std::find_if(selected_coordinates.begin(), selected_coordinates.end(),
                        [role](const auto& value) { return value.role == role; })
        ->requested_value;
  };
  if (joint->type() == AssemblyJointType::Revolute) {
    const AssemblyHierarchyRevoluteJointEquationBuilder builder;
    auto equation =
        builder.build(project, *joint, requested(AssemblyJointCoordinateRole::Rotation));
    if (equation.has_error())
      return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
  }
  if (joint->type() == AssemblyJointType::Prismatic) {
    const AssemblyHierarchyPrismaticJointEquationBuilder builder;
    auto equation =
        builder.build(project, *joint, requested(AssemblyJointCoordinateRole::Translation));
    if (equation.has_error())
      return Result<std::size_t>::failure(equation.error());
    return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
  }
  const AssemblyHierarchyCylindricalJointEquationBuilder builder;
  auto equation =
      builder.build(project, *joint, requested(AssemblyJointCoordinateRole::Translation),
                    requested(AssemblyJointCoordinateRole::Rotation));
  if (equation.has_error())
    return Result<std::size_t>::failure(equation.error());
  return append_scaled_residuals(equation.value().residual, length_residual_scale_mm, residuals);
}

} // namespace

Result<NumericVector> evaluate_cross_hierarchy_motion_group_residuals(
    const Project& project, const AssemblyCrossHierarchyMotionGroup& motion_group,
    AssemblyJointId selected_cross_hierarchy_joint,
    std::vector<AssemblyJointCoordinateDrive> requested_coordinates,
    double length_residual_scale_mm) {
  NumericVector residuals;
  residuals.reserve(motion_group.relationships.size() * 9U);

  for (const AssemblyMotionRelationshipIdentity& relationship : motion_group.relationships) {
    Result<std::size_t> appended = std::visit(
        [&](const auto& identity) -> Result<std::size_t> {
          using Identity = std::decay_t<decltype(identity)>;
          if constexpr (std::is_same_v<Identity, AssemblyLocalRelationshipIdentity>) {
            return append_cross_hierarchy_local_relationship_residuals(
                project, identity, length_residual_scale_mm, residuals);
          } else if constexpr (std::is_same_v<Identity, AssemblyLocalJointIdentity>) {
            return append_local_joint_drive_residuals(project, identity, length_residual_scale_mm,
                                                      residuals);
          } else if constexpr (std::is_same_v<Identity,
                                              AssemblyProjectCrossHierarchyRelationshipIdentity>) {
            return append_cross_hierarchy_project_relationship_residuals(
                project, identity, length_residual_scale_mm, residuals);
          } else {
            return append_cross_joint_drive_residuals(
                project, identity, selected_cross_hierarchy_joint, requested_coordinates,
                length_residual_scale_mm, residuals);
          }
        },
        relationship);
    if (appended.has_error()) {
      return Result<NumericVector>::failure(appended.error());
    }
  }

  return Result<NumericVector>::success(std::move(residuals));
}

} // namespace blcad::geometry::detail
