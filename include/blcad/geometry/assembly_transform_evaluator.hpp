#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

namespace blcad::geometry {

struct AssemblySpacePlanarDescriptor {
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  friend bool operator==(const AssemblySpacePlanarDescriptor&,
                         const AssemblySpacePlanarDescriptor&) = default;
};

struct AssemblySpaceAxisDescriptor {
  Point3 origin;
  Vector3 direction;

  friend bool operator==(const AssemblySpaceAxisDescriptor&,
                         const AssemblySpaceAxisDescriptor&) = default;
};

// Evaluates persisted component free-placement intent in assembly coordinates.
// rotation_deg uses active right-handed fixed-axis rotations in X, then Y, then Z order.
// Points are rotated and translated; vectors and axis directions are rotated only.
class AssemblyTransformEvaluator {
public:
  [[nodiscard]] Point3 evaluate_point(const RigidTransform& transform,
                                      const Point3& component_local_point) const noexcept;
  [[nodiscard]] Vector3 evaluate_vector(const RigidTransform& transform,
                                        const Vector3& component_local_vector) const noexcept;
  [[nodiscard]] AssemblySpacePlanarDescriptor evaluate_plane(
      const RigidTransform& transform,
      const ComponentLocalPlanarDescriptor& component_local_plane) const noexcept;
  [[nodiscard]] AssemblySpaceAxisDescriptor evaluate_axis(
      const RigidTransform& transform,
      const ComponentLocalAxisDescriptor& component_local_axis) const noexcept;
};

} // namespace blcad::geometry
