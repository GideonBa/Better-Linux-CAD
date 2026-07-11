#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"

#include "blcad/geometry/assembly_transform_evaluator.hpp"

namespace blcad::geometry {

Point3 AssemblyHierarchyTransformEvaluator::evaluate_point(
    const std::vector<RigidTransform>& transforms_inner_to_outer,
    const Point3& local_point) const noexcept {
  const AssemblyTransformEvaluator evaluator;
  Point3 evaluated = local_point;
  for (const RigidTransform& transform : transforms_inner_to_outer) {
    evaluated = evaluator.evaluate_point(transform, evaluated);
  }
  return evaluated;
}

Vector3 AssemblyHierarchyTransformEvaluator::evaluate_vector(
    const std::vector<RigidTransform>& transforms_inner_to_outer,
    const Vector3& local_vector) const noexcept {
  const AssemblyTransformEvaluator evaluator;
  Vector3 evaluated = local_vector;
  for (const RigidTransform& transform : transforms_inner_to_outer) {
    evaluated = evaluator.evaluate_vector(transform, evaluated);
  }
  return evaluated;
}

} // namespace blcad::geometry
