#include "blcad/geometry/assembly_spherical_joint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include <utility>

namespace blcad::geometry {
namespace {
Error validation_error(std::string id, std::string message) {
  return Error::validation(std::move(id), std::move(message));
}
Point3 equation_point(const AssemblyResolvedGeometricTarget& target, const Point3& point) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly)
    return point;
  return AssemblyHierarchyTransformEvaluator{}.evaluate_point(target.transforms_inner_to_outer,
                                                              point);
}
Vector3 difference(const Point3& target, const Point3& source) noexcept {
  return {target.x - source.x, target.y - source.y, target.z - source.z};
}
} // namespace

Result<AssemblySphericalJointEquationDescriptor>
AssemblySphericalJointEquationBuilder::build(const Project& project,
                                             const AssemblyJoint& joint) const {
  if (joint.state() != AssemblyJointState::Active)
    return Result<AssemblySphericalJointEquationDescriptor>::failure(validation_error(
        joint.id().value(), "spherical equation construction requires an active joint"));
  const AssemblyConstraintTargetResolver resolver;
  auto a = resolver.resolve_geometric(project, joint.target_a());
  if (a.has_error())
    return Result<AssemblySphericalJointEquationDescriptor>::failure(a.error());
  auto b = resolver.resolve_geometric(project, joint.target_b());
  if (b.has_error())
    return Result<AssemblySphericalJointEquationDescriptor>::failure(b.error());
  return build(joint.id(), joint.type(), a.value(), b.value());
}

Result<AssemblySphericalJointEquationDescriptor> AssemblySphericalJointEquationBuilder::build(
    AssemblyJointId joint, AssemblyJointType type, const AssemblyResolvedGeometricTarget& target_a,
    const AssemblyResolvedGeometricTarget& target_b) const {
  if (type != AssemblyJointType::Spherical)
    return Result<AssemblySphericalJointEquationDescriptor>::failure(validation_error(
        joint.value(), "spherical equation construction requires a Spherical joint"));
  const AssemblyJointTargetCompatibilityResolver resolver;
  auto compatibility = resolver.resolve(type, target_a, target_b);
  if (compatibility.has_error())
    return Result<AssemblySphericalJointEquationDescriptor>::failure(compatibility.error());
  auto projected_a = project_point(target_a);
  if (projected_a.has_error())
    return Result<AssemblySphericalJointEquationDescriptor>::failure(projected_a.error());
  auto projected_b = project_point(target_b);
  if (projected_b.has_error())
    return Result<AssemblySphericalJointEquationDescriptor>::failure(projected_b.error());
  const Point3 point_a = equation_point(target_a, projected_a.value().point);
  const Point3 point_b = equation_point(target_b, projected_b.value().point);
  return Result<AssemblySphericalJointEquationDescriptor>::success(
      {joint,
       compatibility.value(),
       {target_a, compatibility.value().target_a_capability, point_a},
       {target_b, compatibility.value().target_b_capability, point_b},
       {difference(point_b, point_a)}});
}

} // namespace blcad::geometry
