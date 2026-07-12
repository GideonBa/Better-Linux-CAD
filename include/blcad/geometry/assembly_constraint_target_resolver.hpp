#pragma once

#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_document.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"

namespace blcad::geometry {

struct ComponentLocalPlanarDescriptor {
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  friend bool operator==(const ComponentLocalPlanarDescriptor&,
                         const ComponentLocalPlanarDescriptor&) = default;
};

struct ComponentLocalAxisDescriptor {
  Point3 origin;
  Vector3 direction;

  friend bool operator==(const ComponentLocalAxisDescriptor&,
                         const ComponentLocalAxisDescriptor&) = default;
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

struct ResolvedAssemblyAxisConstraintTarget {
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  ProfileId source_profile;
  SemanticAxis axis = SemanticAxis::Primary;
  ComponentLocalAxisDescriptor local_axis;
  RigidTransform component_transform;

  friend bool operator==(const ResolvedAssemblyAxisConstraintTarget&,
                         const ResolvedAssemblyAxisConstraintTarget&) = default;
};

struct ResolvedAssemblyInsertConstraintTarget {
  ComponentInstanceId component_instance;
  DocumentId referenced_part_document;
  FeatureId source_feature;
  ProfileId source_profile;
  SemanticAxis axis = SemanticAxis::Primary;
  SemanticSeatingPlane seating_plane = SemanticSeatingPlane::Primary;
  ComponentLocalAxisDescriptor local_axis;
  ComponentLocalPlanarDescriptor local_seating_plane;
  RigidTransform component_transform;

  friend bool operator==(const ResolvedAssemblyInsertConstraintTarget&,
                         const ResolvedAssemblyInsertConstraintTarget&) = default;
};

// Resolves supported persistent assembly target intent to component-local geometry.
// The typed resolve_geometric path classifies semantic source kind separately from
// solver capability. Existing family-specific methods remain compatibility APIs.
class AssemblyConstraintTargetResolver {
public:
  [[nodiscard]] Result<AssemblyResolvedGeometricTarget>
  resolve_geometric(const Project& project, const AssemblyConstraintTarget& target) const;

  [[nodiscard]] Result<ResolvedAssemblyConstraintTarget>
  resolve(const Project& project, const AssemblyConstraintTarget& target) const;

  [[nodiscard]] Result<ResolvedAssemblyAxisConstraintTarget>
  resolve_axis(const Project& project, const AssemblyConstraintTarget& target) const;

  [[nodiscard]] Result<ResolvedAssemblyInsertConstraintTarget>
  resolve_insert(const Project& project, const AssemblyConstraintTarget& target) const;
};

} // namespace blcad::geometry
