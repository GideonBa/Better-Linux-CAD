#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad::geometry {

enum class AssemblyGeometricTargetSourceKind {
  GeneratedPlanarFace,
  GeneratedCylindricalFace,
  GeneratedLinearEdge,
  GeneratedCircularEdge,
  GeneratedVertex,
  DatumPlane,
  DatumAxis,
  ConstructionLine,
  ConstructionPoint,
  CircularFeatureSeat,
};

[[nodiscard]] std::string_view to_string(AssemblyGeometricTargetSourceKind kind) noexcept;

enum class AssemblyGeometricTargetCapability {
  Plane,
  Axis,
  Line,
  Point,
  Circle,
  Cylinder,
  Frame,
};

[[nodiscard]] std::string_view to_string(AssemblyGeometricTargetCapability capability) noexcept;

enum class AssemblyGeometricTargetCoordinateSpace {
  ComponentLocal,
  RootAssembly,
};

[[nodiscard]] std::string_view to_string(AssemblyGeometricTargetCoordinateSpace space) noexcept;

struct AssemblyLocalGeometricTargetEndpointIdentity {
  ComponentInstanceId component_instance;
  std::string semantic_reference;

  friend bool operator==(const AssemblyLocalGeometricTargetEndpointIdentity&,
                         const AssemblyLocalGeometricTargetEndpointIdentity&) = default;
};

struct AssemblyHierarchyGeometricTargetEndpointIdentity {
  std::vector<SubassemblyInstanceId> occurrence_path;
  ComponentInstanceId component_instance;
  std::string semantic_reference;

  friend bool operator==(const AssemblyHierarchyGeometricTargetEndpointIdentity&,
                         const AssemblyHierarchyGeometricTargetEndpointIdentity&) = default;
};

using AssemblyGeometricTargetEndpointIdentity =
    std::variant<AssemblyLocalGeometricTargetEndpointIdentity,
                 AssemblyHierarchyGeometricTargetEndpointIdentity>;

// Derived source-model identity metadata. The exact endpoint semantic-reference
// string remains authoritative; these fields retain already-resolved typed model
// identities for diagnostics and future presentation without persisting Geometry
// query output.
struct AssemblyGeometricTargetSourceMetadata {
  DocumentId referenced_part_document;
  std::optional<FeatureId> source_feature;
  std::optional<ProfileId> source_profile;
  std::optional<SemanticFace> semantic_face;
  std::optional<SemanticAxis> semantic_axis;
  std::optional<SemanticSeatingPlane> semantic_seating_plane;

  friend bool operator==(const AssemblyGeometricTargetSourceMetadata&,
                         const AssemblyGeometricTargetSourceMetadata&) = default;
};

// All directions/axes in resolved target descriptors are finite unit vectors.
// Plane, Circle, and Frame axes are orthonormal and right-handed.
struct AssemblyPlanarTargetDescriptor {
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  friend bool operator==(const AssemblyPlanarTargetDescriptor&,
                         const AssemblyPlanarTargetDescriptor&) = default;
};

struct AssemblyAxisTargetDescriptor {
  Point3 origin;
  Vector3 direction;

  friend bool operator==(const AssemblyAxisTargetDescriptor&,
                         const AssemblyAxisTargetDescriptor&) = default;
};

struct AssemblyLineTargetDescriptor {
  Point3 origin;
  Vector3 direction;

  friend bool operator==(const AssemblyLineTargetDescriptor&,
                         const AssemblyLineTargetDescriptor&) = default;
};

struct AssemblyPointTargetDescriptor {
  Point3 point;

  friend bool operator==(const AssemblyPointTargetDescriptor&,
                         const AssemblyPointTargetDescriptor&) = default;
};

struct AssemblyCircularEdgeTargetDescriptor {
  Point3 center;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;
  double radius_mm = 0.0;

  friend bool operator==(const AssemblyCircularEdgeTargetDescriptor&,
                         const AssemblyCircularEdgeTargetDescriptor&) = default;
};

struct AssemblyCylindricalSurfaceTargetDescriptor {
  Point3 axis_origin;
  Vector3 axis_direction;
  double radius_mm = 0.0;

  friend bool operator==(const AssemblyCylindricalSurfaceTargetDescriptor&,
                         const AssemblyCylindricalSurfaceTargetDescriptor&) = default;
};

struct AssemblyFrameTargetDescriptor {
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 z_axis;

  friend bool operator==(const AssemblyFrameTargetDescriptor&,
                         const AssemblyFrameTargetDescriptor&) = default;
};

using AssemblyGeometricTargetDescriptor =
    std::variant<AssemblyPlanarTargetDescriptor,
                 AssemblyAxisTargetDescriptor,
                 AssemblyLineTargetDescriptor,
                 AssemblyPointTargetDescriptor,
                 AssemblyCircularEdgeTargetDescriptor,
                 AssemblyCylindricalSurfaceTargetDescriptor,
                 AssemblyFrameTargetDescriptor>;

struct AssemblyResolvedGeometricTarget {
  AssemblyGeometricTargetEndpointIdentity endpoint;
  AssemblyGeometricTargetSourceKind source_kind =
      AssemblyGeometricTargetSourceKind::GeneratedPlanarFace;
  AssemblyGeometricTargetSourceMetadata source_metadata;
  AssemblyGeometricTargetDescriptor descriptor;
  std::vector<AssemblyGeometricTargetCapability> capabilities;
  AssemblyGeometricTargetCoordinateSpace coordinate_space =
      AssemblyGeometricTargetCoordinateSpace::ComponentLocal;
  std::vector<RigidTransform> transforms_inner_to_outer;

  friend bool operator==(const AssemblyResolvedGeometricTarget&,
                         const AssemblyResolvedGeometricTarget&) = default;
};

// Canonical capability order is Plane, Axis, Line, Point, Circle, Cylinder,
// Frame. This order is the sole source for resolved-target capability vectors.
[[nodiscard]] std::vector<AssemblyGeometricTargetCapability>
assembly_geometric_target_capabilities(AssemblyGeometricTargetSourceKind kind);

[[nodiscard]] Result<std::size_t>
validate_resolved_geometric_target(const AssemblyResolvedGeometricTarget& target);

[[nodiscard]] Result<AssemblyPlanarTargetDescriptor>
project_plane(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyAxisTargetDescriptor>
project_axis(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyLineTargetDescriptor>
project_line(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyPointTargetDescriptor>
project_point(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyCircularEdgeTargetDescriptor>
project_circle(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyCylindricalSurfaceTargetDescriptor>
project_cylinder(const AssemblyResolvedGeometricTarget& target);
[[nodiscard]] Result<AssemblyFrameTargetDescriptor>
project_frame(const AssemblyResolvedGeometricTarget& target);

} // namespace blcad::geometry
