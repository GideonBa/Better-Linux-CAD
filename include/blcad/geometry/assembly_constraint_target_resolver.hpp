#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

namespace blcad::geometry {

struct ComponentLocalPlanarDescriptor {
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  friend bool operator==(const ComponentLocalPlanarDescriptor&,
                         const ComponentLocalPlanarDescriptor&) = default;
};

struct ResolvedAssemblyConstraintTarget {
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  SemanticFace face = SemanticFace::Top;
  ComponentLocalPlanarDescriptor local_plane;
  RigidTransform component_transform;

  friend bool operator==(const ResolvedAssemblyConstraintTarget&,
                         const ResolvedAssemblyConstraintTarget&) = default;
};

// Resolves supported persistent assembly target intent to component-local geometry.
// The component transform remains separate placement intent and is not applied here.
class AssemblyConstraintTargetResolver {
public:
  [[nodiscard]] Result<ResolvedAssemblyConstraintTarget>
  resolve(const Project& project, const AssemblyConstraintTarget& target) const;
};

} // namespace blcad::geometry
