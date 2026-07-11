#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/spatial.hpp"

#include <vector>

namespace blcad::geometry {

// Evaluates one authored rigid-transform chain exactly in inner-to-outer order.
// Each transform retains the existing active fixed-axis X-then-Y-then-Z degree
// convention; the chain is never collapsed back into Euler angles.
class AssemblyHierarchyTransformEvaluator {
public:
  [[nodiscard]] Point3 evaluate_point(
      const std::vector<RigidTransform>& transforms_inner_to_outer,
      const Point3& local_point) const noexcept;

  [[nodiscard]] Vector3 evaluate_vector(
      const std::vector<RigidTransform>& transforms_inner_to_outer,
      const Vector3& local_vector) const noexcept;
};

} // namespace blcad::geometry
