#include "blcad/geometry/assembly_geometric_target.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double kDirectionTolerance = 1.0e-9;
constexpr const char* kGeometricTargetId = "geometry.assembly_geometric_target";

[[nodiscard]] Error validation_error(std::string message) {
  return Error::validation(kGeometricTargetId, std::move(message));
}

[[nodiscard]] bool finite(double value) noexcept {
  return std::isfinite(value);
}

[[nodiscard]] bool finite(const Point3& point) noexcept {
  return finite(point.x) && finite(point.y) && finite(point.z);
}

[[nodiscard]] bool finite(const Vector3& vector) noexcept {
  return finite(vector.x) && finite(vector.y) && finite(vector.z);
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y,
                 lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] double magnitude(const Vector3& vector) noexcept {
  return std::sqrt(dot(vector, vector));
}

[[nodiscard]] bool unit_vector(const Vector3& vector) noexcept {
  return finite(vector) && std::abs(magnitude(vector) - 1.0) <= kDirectionTolerance;
}

[[nodiscard]] bool orthogonal(const Vector3& lhs, const Vector3& rhs) noexcept {
  return std::abs(dot(lhs, rhs)) <= kDirectionTolerance;
}

[[nodiscard]] bool right_handed_frame(const Vector3& x_axis,
                                      const Vector3& y_axis,
                                      const Vector3& z_axis) noexcept {
  return unit_vector(x_axis) && unit_vector(y_axis) && unit_vector(z_axis) &&
         orthogonal(x_axis, y_axis) && orthogonal(x_axis, z_axis) &&
         orthogonal(y_axis, z_axis) &&
         dot(cross(x_axis, y_axis), z_axis) >= 1.0 - kDirectionTolerance;
}

[[nodiscard]] bool finite(const RigidTransform& transform) noexcept {
  return finite(transform.translation_mm) && finite(transform.rotation_deg);
}

[[nodiscard]] bool has_capability(const AssemblyResolvedGeometricTarget& target,
                                  AssemblyGeometricTargetCapability capability) {
  return std::find(target.capabilities.begin(), target.capabilities.end(), capability) !=
         target.capabilities.end();
}

[[nodiscard]] Result<std::size_t>
validate_endpoint_and_space(const AssemblyResolvedGeometricTarget& target) {
  if (const auto* local =
          std::get_if<AssemblyLocalGeometricTargetEndpointIdentity>(&target.endpoint)) {
    if (local->component_instance.empty() || local->semantic_reference.empty()) {
      return Result<std::size_t>::failure(
          validation_error("local geometric target endpoint identity must not be empty"));
    }
    if (target.coordinate_space != AssemblyGeometricTargetCoordinateSpace::ComponentLocal) {
      return Result<std::size_t>::failure(validation_error(
          "local geometric target endpoint must use component-local coordinate space"));
    }
    if (target.transforms_inner_to_outer.size() != 1U) {
      return Result<std::size_t>::failure(validation_error(
          "local geometric target must retain exactly its direct component transform context"));
    }
    return Result<std::size_t>::success(1U);
  }

  const auto* hierarchy =
      std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(&target.endpoint);
  if (hierarchy == nullptr || hierarchy->component_instance.empty() ||
      hierarchy->semantic_reference.empty()) {
    return Result<std::size_t>::failure(
        validation_error("hierarchy geometric target endpoint identity must not be empty"));
  }
  for (const SubassemblyInstanceId& occurrence : hierarchy->occurrence_path) {
    if (occurrence.empty()) {
      return Result<std::size_t>::failure(
          validation_error("hierarchy geometric target occurrence path ids must not be empty"));
    }
  }
  if (target.coordinate_space != AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return Result<std::size_t>::failure(
        validation_error("hierarchy geometric target endpoint must use root-assembly coordinate space"));
  }
  if (target.transforms_inner_to_outer.empty()) {
    return Result<std::size_t>::failure(validation_error(
        "hierarchy geometric target must retain component and parent transform context"));
  }
  return Result<std::size_t>::success(target.transforms_inner_to_outer.size());
}

[[nodiscard]] Result<std::size_t>
validate_source_metadata(const AssemblyResolvedGeometricTarget& target) {
  const AssemblyGeometricTargetSourceMetadata& source = target.source_metadata;
  if (source.referenced_part_document.empty()) {
    return Result<std::size_t>::failure(
        validation_error("resolved geometric target referenced part document must not be empty"));
  }

  switch (target.source_kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
    if (!source.source_feature.has_value() || !source.semantic_face.has_value()) {
      return Result<std::size_t>::failure(validation_error(
          "GeneratedPlanarFace target must retain source feature and semantic face metadata"));
    }
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    if (!source.source_feature.has_value() || !source.source_profile.has_value()) {
      return Result<std::size_t>::failure(validation_error(
          "GeneratedCylindricalFace target must retain source feature and profile metadata"));
    }
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
    if (!source.source_feature.has_value()) {
      return Result<std::size_t>::failure(validation_error(
          "generated topology target must retain source feature metadata"));
    }
    break;
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    if (!source.source_feature.has_value() || !source.source_profile.has_value() ||
        !source.semantic_axis.has_value() || !source.semantic_seating_plane.has_value()) {
      return Result<std::size_t>::failure(validation_error(
          "CircularFeatureSeat target must retain feature, profile, axis, and seating metadata"));
    }
    break;
  case AssemblyGeometricTargetSourceKind::DatumPlane:
  case AssemblyGeometricTargetSourceKind::DatumAxis:
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    // Exact reference-geometry source ids/grammar are intentionally introduced in
    // Block 32. The endpoint semantic-reference string remains exact identity here.
    break;
  }

  return Result<std::size_t>::success(1U);
}

[[nodiscard]] bool descriptor_matches_source_kind(const AssemblyResolvedGeometricTarget& target) {
  switch (target.source_kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
  case AssemblyGeometricTargetSourceKind::DatumPlane:
    return std::holds_alternative<AssemblyPlanarTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    return std::holds_alternative<AssemblyCylindricalSurfaceTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
    return std::holds_alternative<AssemblyLineTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
    return std::holds_alternative<AssemblyCircularEdgeTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    return std::holds_alternative<AssemblyPointTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::DatumAxis:
    return std::holds_alternative<AssemblyAxisTargetDescriptor>(target.descriptor);
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    return std::holds_alternative<AssemblyFrameTargetDescriptor>(target.descriptor);
  }
  return false;
}

[[nodiscard]] Result<std::size_t>
validate_descriptor(const AssemblyGeometricTargetDescriptor& descriptor) {
  return std::visit(
      [](const auto& value) -> Result<std::size_t> {
        using Descriptor = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Descriptor, AssemblyPlanarTargetDescriptor>) {
          if (!finite(value.origin) ||
              !right_handed_frame(value.x_axis, value.y_axis, value.normal)) {
            return Result<std::size_t>::failure(validation_error(
                "Plane target descriptor must be finite, orthonormal, and right-handed"));
          }
        } else if constexpr (std::is_same_v<Descriptor, AssemblyAxisTargetDescriptor>) {
          if (!finite(value.origin) || !unit_vector(value.direction)) {
            return Result<std::size_t>::failure(validation_error(
                "Axis target descriptor must have finite origin and unit non-degenerate direction"));
          }
        } else if constexpr (std::is_same_v<Descriptor, AssemblyLineTargetDescriptor>) {
          if (!finite(value.origin) || !unit_vector(value.direction)) {
            return Result<std::size_t>::failure(validation_error(
                "Line target descriptor must have finite origin and unit non-degenerate direction"));
          }
        } else if constexpr (std::is_same_v<Descriptor, AssemblyPointTargetDescriptor>) {
          if (!finite(value.point)) {
            return Result<std::size_t>::failure(
                validation_error("Point target descriptor must be finite"));
          }
        } else if constexpr (std::is_same_v<Descriptor, AssemblyCircularEdgeTargetDescriptor>) {
          if (!finite(value.center) || !finite(value.radius_mm) || value.radius_mm <= 0.0 ||
              !right_handed_frame(value.x_axis, value.y_axis, value.normal)) {
            return Result<std::size_t>::failure(validation_error(
                "Circle target descriptor must have finite right-handed frame and positive radius"));
          }
        } else if constexpr (
            std::is_same_v<Descriptor, AssemblyCylindricalSurfaceTargetDescriptor>) {
          if (!finite(value.axis_origin) || !unit_vector(value.axis_direction) ||
              !finite(value.radius_mm) || value.radius_mm <= 0.0) {
            return Result<std::size_t>::failure(validation_error(
                "Cylinder target descriptor must have finite unit axis and positive radius"));
          }
        } else if constexpr (std::is_same_v<Descriptor, AssemblyFrameTargetDescriptor>) {
          if (!finite(value.origin) ||
              !right_handed_frame(value.x_axis, value.y_axis, value.z_axis)) {
            return Result<std::size_t>::failure(validation_error(
                "Frame target descriptor must be finite, orthonormal, and right-handed"));
          }
        }
        return Result<std::size_t>::success(1U);
      },
      descriptor);
}

[[nodiscard]] Error unsupported_projection(AssemblyGeometricTargetCapability capability) {
  return validation_error("resolved geometric target source does not expose " +
                          std::string(to_string(capability)) + " capability");
}

} // namespace

std::string_view to_string(AssemblyGeometricTargetSourceKind kind) noexcept {
  switch (kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
    return "GeneratedPlanarFace";
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    return "GeneratedCylindricalFace";
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
    return "GeneratedLinearEdge";
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
    return "GeneratedCircularEdge";
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
    return "GeneratedVertex";
  case AssemblyGeometricTargetSourceKind::DatumPlane:
    return "DatumPlane";
  case AssemblyGeometricTargetSourceKind::DatumAxis:
    return "DatumAxis";
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
    return "ConstructionLine";
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    return "ConstructionPoint";
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    return "CircularFeatureSeat";
  }
  return "Unknown";
}

std::string_view to_string(AssemblyGeometricTargetCapability capability) noexcept {
  switch (capability) {
  case AssemblyGeometricTargetCapability::Plane:
    return "Plane";
  case AssemblyGeometricTargetCapability::Axis:
    return "Axis";
  case AssemblyGeometricTargetCapability::Line:
    return "Line";
  case AssemblyGeometricTargetCapability::Point:
    return "Point";
  case AssemblyGeometricTargetCapability::Circle:
    return "Circle";
  case AssemblyGeometricTargetCapability::Cylinder:
    return "Cylinder";
  case AssemblyGeometricTargetCapability::Frame:
    return "Frame";
  }
  return "Unknown";
}

std::string_view to_string(AssemblyGeometricTargetCoordinateSpace space) noexcept {
  switch (space) {
  case AssemblyGeometricTargetCoordinateSpace::ComponentLocal:
    return "ComponentLocal";
  case AssemblyGeometricTargetCoordinateSpace::RootAssembly:
    return "RootAssembly";
  }
  return "Unknown";
}

std::vector<AssemblyGeometricTargetCapability>
assembly_geometric_target_capabilities(AssemblyGeometricTargetSourceKind kind) {
  switch (kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
  case AssemblyGeometricTargetSourceKind::DatumPlane:
    return {AssemblyGeometricTargetCapability::Plane};
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    return {AssemblyGeometricTargetCapability::Axis,
            AssemblyGeometricTargetCapability::Cylinder};
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
    return {AssemblyGeometricTargetCapability::Line};
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
    return {AssemblyGeometricTargetCapability::Axis,
            AssemblyGeometricTargetCapability::Point,
            AssemblyGeometricTargetCapability::Circle};
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    return {AssemblyGeometricTargetCapability::Point};
  case AssemblyGeometricTargetSourceKind::DatumAxis:
    return {AssemblyGeometricTargetCapability::Axis,
            AssemblyGeometricTargetCapability::Line};
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    return {AssemblyGeometricTargetCapability::Plane,
            AssemblyGeometricTargetCapability::Axis,
            AssemblyGeometricTargetCapability::Frame};
  }
  return {};
}

Result<std::size_t>
validate_resolved_geometric_target(const AssemblyResolvedGeometricTarget& target) {
  auto endpoint = validate_endpoint_and_space(target);
  if (endpoint.has_error()) {
    return Result<std::size_t>::failure(endpoint.error());
  }
  auto source = validate_source_metadata(target);
  if (source.has_error()) {
    return Result<std::size_t>::failure(source.error());
  }
  if (!descriptor_matches_source_kind(target)) {
    return Result<std::size_t>::failure(validation_error(
        "resolved geometric target descriptor does not match semantic source kind"));
  }
  auto descriptor = validate_descriptor(target.descriptor);
  if (descriptor.has_error()) {
    return Result<std::size_t>::failure(descriptor.error());
  }
  if (target.capabilities != assembly_geometric_target_capabilities(target.source_kind)) {
    return Result<std::size_t>::failure(validation_error(
        "resolved geometric target capabilities must match canonical source capability order"));
  }
  for (const RigidTransform& transform : target.transforms_inner_to_outer) {
    if (!finite(transform)) {
      return Result<std::size_t>::failure(
          validation_error("resolved geometric target transform context must be finite"));
    }
  }
  return Result<std::size_t>::success(target.capabilities.size());
}

Result<AssemblyPlanarTargetDescriptor>
project_plane(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyPlanarTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Plane)) {
    return Result<AssemblyPlanarTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Plane));
  }
  if (const auto* plane = std::get_if<AssemblyPlanarTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyPlanarTargetDescriptor>::success(*plane);
  }
  const auto* frame = std::get_if<AssemblyFrameTargetDescriptor>(&target.descriptor);
  return Result<AssemblyPlanarTargetDescriptor>::success(
      AssemblyPlanarTargetDescriptor{frame->origin, frame->x_axis, frame->y_axis, frame->z_axis});
}

Result<AssemblyAxisTargetDescriptor>
project_axis(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyAxisTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Axis)) {
    return Result<AssemblyAxisTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Axis));
  }
  if (const auto* axis = std::get_if<AssemblyAxisTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyAxisTargetDescriptor>::success(*axis);
  }
  if (const auto* circle = std::get_if<AssemblyCircularEdgeTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyAxisTargetDescriptor>::success(
        AssemblyAxisTargetDescriptor{circle->center, circle->normal});
  }
  if (const auto* cylinder =
          std::get_if<AssemblyCylindricalSurfaceTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyAxisTargetDescriptor>::success(
        AssemblyAxisTargetDescriptor{cylinder->axis_origin, cylinder->axis_direction});
  }
  const auto* frame = std::get_if<AssemblyFrameTargetDescriptor>(&target.descriptor);
  return Result<AssemblyAxisTargetDescriptor>::success(
      AssemblyAxisTargetDescriptor{frame->origin, frame->z_axis});
}

Result<AssemblyLineTargetDescriptor>
project_line(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyLineTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Line)) {
    return Result<AssemblyLineTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Line));
  }
  if (const auto* line = std::get_if<AssemblyLineTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyLineTargetDescriptor>::success(*line);
  }
  const auto* axis = std::get_if<AssemblyAxisTargetDescriptor>(&target.descriptor);
  return Result<AssemblyLineTargetDescriptor>::success(
      AssemblyLineTargetDescriptor{axis->origin, axis->direction});
}

Result<AssemblyPointTargetDescriptor>
project_point(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyPointTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Point)) {
    return Result<AssemblyPointTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Point));
  }
  if (const auto* point = std::get_if<AssemblyPointTargetDescriptor>(&target.descriptor)) {
    return Result<AssemblyPointTargetDescriptor>::success(*point);
  }
  const auto* circle = std::get_if<AssemblyCircularEdgeTargetDescriptor>(&target.descriptor);
  return Result<AssemblyPointTargetDescriptor>::success(AssemblyPointTargetDescriptor{circle->center});
}

Result<AssemblyCircularEdgeTargetDescriptor>
project_circle(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyCircularEdgeTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Circle)) {
    return Result<AssemblyCircularEdgeTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Circle));
  }
  return Result<AssemblyCircularEdgeTargetDescriptor>::success(
      std::get<AssemblyCircularEdgeTargetDescriptor>(target.descriptor));
}

Result<AssemblyCylindricalSurfaceTargetDescriptor>
project_cylinder(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyCylindricalSurfaceTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Cylinder)) {
    return Result<AssemblyCylindricalSurfaceTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Cylinder));
  }
  return Result<AssemblyCylindricalSurfaceTargetDescriptor>::success(
      std::get<AssemblyCylindricalSurfaceTargetDescriptor>(target.descriptor));
}

Result<AssemblyFrameTargetDescriptor>
project_frame(const AssemblyResolvedGeometricTarget& target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyFrameTargetDescriptor>::failure(valid.error());
  }
  if (!has_capability(target, AssemblyGeometricTargetCapability::Frame)) {
    return Result<AssemblyFrameTargetDescriptor>::failure(
        unsupported_projection(AssemblyGeometricTargetCapability::Frame));
  }
  return Result<AssemblyFrameTargetDescriptor>::success(
      std::get<AssemblyFrameTargetDescriptor>(target.descriptor));
}

} // namespace blcad::geometry
