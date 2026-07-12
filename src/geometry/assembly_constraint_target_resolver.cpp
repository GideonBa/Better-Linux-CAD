#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace blcad::geometry {
namespace {

struct ResolvedTargetContext {
  const ComponentInstance* component = nullptr;
  const PartDocument* part = nullptr;
};

struct ResolvedCircularFeatureGeometry {
  FeatureId source_feature;
  ProfileId source_profile;
  ComponentLocalAxisDescriptor local_axis;
  ComponentLocalPlanarDescriptor local_seating_plane;
  double radius_mm = 0.0;
};

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::optional<SemanticFace> parse_semantic_face(std::string_view face_name) noexcept {
  if (face_name == "top")
    return SemanticFace::Top;
  if (face_name == "bottom")
    return SemanticFace::Bottom;
  if (face_name == "right")
    return SemanticFace::Right;
  if (face_name == "left")
    return SemanticFace::Left;
  if (face_name == "front")
    return SemanticFace::Front;
  if (face_name == "back")
    return SemanticFace::Back;
  return std::nullopt;
}

[[nodiscard]] Result<ResolvedTargetContext>
resolve_target_context(const Project& project, const AssemblyConstraintTarget& target) {
  const ComponentInstance* component =
      project.assembly().find_component_instance(target.component_instance());
  if (component == nullptr) {
    return Result<ResolvedTargetContext>::failure(validation_error(
        target.component_instance().value(),
        "assembly constraint target component instance must exist in project assembly"));
  }

  const PartDocument* part = project.find_part_document(component->referenced_part_document());
  if (part == nullptr) {
    return Result<ResolvedTargetContext>::failure(
        validation_error(component->id().value(), "assembly constraint target referenced part must "
                                                  "resolve to an owned project part document"));
  }

  return Result<ResolvedTargetContext>::success(ResolvedTargetContext{component, part});
}

[[nodiscard]] Result<SemanticFaceReference>
parse_generated_face_reference(std::string_view semantic_reference) {
  const std::size_t separator = semantic_reference.rfind('.');
  if (separator == std::string_view::npos || separator == 0U ||
      separator + 1U >= semantic_reference.size()) {
    return Result<SemanticFaceReference>::failure(validation_error(
        std::string(semantic_reference), "assembly semantic reference is malformed"));
  }

  const std::string_view source_feature = semantic_reference.substr(0U, separator);
  const std::string_view face_name = semantic_reference.substr(separator + 1U);
  const auto face = parse_semantic_face(face_name);
  if (!face.has_value()) {
    return Result<SemanticFaceReference>::failure(validation_error(
        std::string(semantic_reference), "unsupported assembly semantic reference family"));
  }

  return SemanticFaceReference::create(FeatureId(std::string(source_feature)), face.value());
}

[[nodiscard]] Result<SemanticAxisReference>
parse_generated_axis_reference(std::string_view semantic_reference) {
  const std::size_t separator = semantic_reference.rfind('.');
  if (separator == std::string_view::npos || separator == 0U ||
      separator + 1U >= semantic_reference.size()) {
    return Result<SemanticAxisReference>::failure(validation_error(
        std::string(semantic_reference), "assembly semantic axis reference is malformed"));
  }

  const std::string_view source_feature = semantic_reference.substr(0U, separator);
  const std::string_view axis_name = semantic_reference.substr(separator + 1U);
  if (axis_name != "axis") {
    return Result<SemanticAxisReference>::failure(validation_error(
        std::string(semantic_reference), "unsupported assembly semantic axis reference family"));
  }

  return SemanticAxisReference::create(FeatureId(std::string(source_feature)));
}

[[nodiscard]] Result<SemanticSeatingPlaneReference>
parse_generated_seating_plane_reference(std::string_view semantic_reference) {
  const std::size_t separator = semantic_reference.rfind('.');
  if (separator == std::string_view::npos || separator == 0U ||
      separator + 1U >= semantic_reference.size()) {
    return Result<SemanticSeatingPlaneReference>::failure(validation_error(
        std::string(semantic_reference), "assembly semantic seating reference is malformed"));
  }

  const std::string_view source_feature = semantic_reference.substr(0U, separator);
  const std::string_view seat_name = semantic_reference.substr(separator + 1U);
  if (seat_name != "seat") {
    return Result<SemanticSeatingPlaneReference>::failure(validation_error(
        std::string(semantic_reference), "unsupported assembly semantic seating reference family"));
  }

  return SemanticSeatingPlaneReference::create(FeatureId(std::string(source_feature)));
}

[[nodiscard]] Vector3 negate(const Vector3& vector) noexcept {
  return Vector3{-vector.x, -vector.y, -vector.z};
}

[[nodiscard]] Vector3 cross(const Vector3& lhs, const Vector3& rhs) noexcept {
  return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                 lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] Vector3 extrude_direction(const ResolvedWorkplane& workplane,
                                        ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return negate(workplane.normal);
  }
  return workplane.normal;
}

[[nodiscard]] ComponentLocalPlanarDescriptor seating_plane(const ResolvedWorkplane& workplane,
                                                           Point3 axis_origin,
                                                           ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return ComponentLocalPlanarDescriptor{axis_origin, workplane.x_axis, negate(workplane.y_axis),
                                          negate(workplane.normal)};
  }
  return ComponentLocalPlanarDescriptor{axis_origin, workplane.x_axis, workplane.y_axis,
                                        workplane.normal};
}

[[nodiscard]] Result<ResolvedAssemblyConstraintTarget>
resolve_generated_planar_target(const ResolvedTargetContext& context,
                                const AssemblyConstraintTarget& target,
                                const SemanticFaceReference& face_reference) {
  const ComponentInstance& component = *context.component;
  const PartDocument& part = *context.part;
  const Feature* source_feature = part.find_feature(face_reference.source_feature());
  if (source_feature == nullptr) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-face assembly target source feature must exist in referenced part document"));
  }
  if (source_feature->type() != FeatureType::AdditiveExtrude) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-face assembly target source feature must be an additive extrude"));
  }

  const WorkplaneResolver workplane_resolver;
  auto resolved_workplane = workplane_resolver.resolve_generated_face(part, face_reference);
  if (resolved_workplane.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(resolved_workplane.error());
  }

  const ResolvedWorkplane& plane = resolved_workplane.value();
  return Result<ResolvedAssemblyConstraintTarget>::success(ResolvedAssemblyConstraintTarget{
      component.id(), component.referenced_part_document(), face_reference.source_feature(),
      face_reference.face(),
      ComponentLocalPlanarDescriptor{plane.origin, plane.x_axis, plane.y_axis, plane.normal},
      component.transform()});
}

[[nodiscard]] Result<ResolvedCircularFeatureGeometry> resolve_circular_feature_geometry(
    const ResolvedTargetContext& context, const AssemblyConstraintTarget& target,
    const FeatureId& source_feature_id, std::string_view family_name) {
  const PartDocument& part = *context.part;
  const Feature* source_feature = part.find_feature(source_feature_id);
  if (source_feature == nullptr) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(),
        std::string(family_name) + " source feature must exist in referenced part document"));
  }
  if (source_feature->type() != FeatureType::SubtractiveExtrude) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(),
        std::string(family_name) + " source feature must be a subtractive extrude"));
  }

  const Sketch* sketch = part.find_sketch(source_feature->input_sketch());
  if (sketch == nullptr) {
    return Result<ResolvedCircularFeatureGeometry>::failure(
        validation_error(target.semantic_reference(),
                         std::string(family_name) + " source feature input sketch must exist"));
  }
  if (sketch->circle_profiles().size() != 1U || sketch->profile_count() != 1U) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(),
        std::string(family_name) + " requires exactly one CircleProfile in the source sketch"));
  }

  const CircleProfile& circle = sketch->circle_profiles().front();
  const Parameter* diameter = part.find_parameter(circle.diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(),
        std::string(family_name) + " circle diameter must resolve to a length parameter"));
  }

  const WorkplaneResolver workplane_resolver;
  auto resolved_workplane = workplane_resolver.resolve_for_sketch(part, *sketch);
  if (resolved_workplane.has_error()) {
    return Result<ResolvedCircularFeatureGeometry>::failure(resolved_workplane.error());
  }

  const ResolvedWorkplane& workplane = resolved_workplane.value();
  const Point3 axis_origin = workplane_resolver.evaluate_point(workplane, circle.center());
  const Vector3 axis_direction = extrude_direction(workplane, source_feature->direction());
  return Result<ResolvedCircularFeatureGeometry>::success(ResolvedCircularFeatureGeometry{
      source_feature_id, circle.id(), ComponentLocalAxisDescriptor{axis_origin, axis_direction},
      seating_plane(workplane, axis_origin, source_feature->direction()),
      diameter->value().millimeters() / 2.0});
}

[[nodiscard]] Result<AssemblyResolvedGeometricTarget>
validated_geometric_target(AssemblyResolvedGeometricTarget target) {
  auto valid = validate_resolved_geometric_target(target);
  if (valid.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(valid.error());
  }
  return Result<AssemblyResolvedGeometricTarget>::success(std::move(target));
}

[[nodiscard]] AssemblyGeometricTargetSourceMetadata
reference_source_metadata(const ComponentInstance& component) {
  return AssemblyGeometricTargetSourceMetadata{component.referenced_part_document(),
                                               std::nullopt,
                                               std::nullopt,
                                               std::nullopt,
                                               std::nullopt,
                                               std::nullopt};
}

[[nodiscard]] Result<AssemblyResolvedGeometricTarget> resolved_reference_target(
    const ResolvedTargetContext& context, const AssemblyConstraintTarget& target,
    AssemblyGeometricTargetSourceKind source_kind, AssemblyGeometricTargetDescriptor descriptor) {
  const ComponentInstance& component = *context.component;
  return validated_geometric_target(
      AssemblyResolvedGeometricTarget{AssemblyLocalGeometricTargetEndpointIdentity{
                                          target.component_instance(), target.semantic_reference()},
                                      source_kind,
                                      reference_source_metadata(component),
                                      std::move(descriptor),
                                      assembly_geometric_target_capabilities(source_kind),
                                      AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
                                      {component.transform()}});
}

[[nodiscard]] Result<AssemblyResolvedGeometricTarget>
resolve_reference_target(const ResolvedTargetContext& context,
                         const AssemblyConstraintTarget& target,
                         const AssemblyReferenceTargetIdentity& identity) {
  const PartDocument& part = *context.part;

  return std::visit(
      [&](const auto& id) -> Result<AssemblyResolvedGeometricTarget> {
        using Id = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<Id, DatumPlaneId>) {
          const WorkplaneResolver resolver;
          auto resolved = resolver.resolve(part, id);
          if (resolved.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
          }
          const ResolvedWorkplane& plane = resolved.value();
          return resolved_reference_target(
              context, target, AssemblyGeometricTargetSourceKind::DatumPlane,
              AssemblyPlanarTargetDescriptor{plane.origin, plane.x_axis, plane.y_axis,
                                             plane.normal});
        } else if constexpr (std::is_same_v<Id, DatumAxisId>) {
          const DatumAxis* axis = part.find_datum_axis(id);
          if (axis == nullptr) {
            return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
                target.semantic_reference(),
                "datum-axis assembly target must exist in referenced part document"));
          }
          if (axis->kind() == DatumAxisKind::Explicit) {
            return resolved_reference_target(
                context, target, AssemblyGeometricTargetSourceKind::DatumAxis,
                AssemblyAxisTargetDescriptor{axis->origin(), axis->direction()});
          }

          const ConstructionLineResolver resolver;
          auto resolved = resolver.resolve(part, axis->source_construction_line());
          if (resolved.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
          }
          return resolved_reference_target(
              context, target, AssemblyGeometricTargetSourceKind::DatumAxis,
              AssemblyAxisTargetDescriptor{resolved.value().point, resolved.value().direction});
        } else if constexpr (std::is_same_v<Id, ConstructionLineId>) {
          const ConstructionLineResolver resolver;
          auto resolved = resolver.resolve(part, id);
          if (resolved.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
          }
          return resolved_reference_target(
              context, target, AssemblyGeometricTargetSourceKind::ConstructionLine,
              AssemblyLineTargetDescriptor{resolved.value().point, resolved.value().direction});
        } else {
          const ConstructionPointResolver resolver;
          auto resolved = resolver.resolve(part, id);
          if (resolved.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
          }
          return resolved_reference_target(
              context, target, AssemblyGeometricTargetSourceKind::ConstructionPoint,
              AssemblyPointTargetDescriptor{resolved.value().position});
        }
      },
      identity);
}

// Axis-aligned part-local extent of a supported RectangularAdditiveExtrude box.
// The rectangle profile's local coordinates map directly onto part-local x/y and
// the extrude thickness runs along +z, matching the existing generated planar
// face resolution convention. Right = +x (width), Front = +y (height),
// Top = +z (thickness).
struct RectangularBoxExtent {
  double center_x = 0.0;
  double center_y = 0.0;
  double width_mm = 0.0;
  double height_mm = 0.0;
  double thickness_mm = 0.0;
};

[[nodiscard]] double dot(const Point3& point, const Vector3& axis) noexcept {
  return point.x * axis.x + point.y * axis.y + point.z * axis.z;
}

[[nodiscard]] Result<RectangularBoxExtent>
resolve_rectangular_box_extent(const PartDocument& part, const FeatureId& feature_id,
                               const AssemblyConstraintTarget& target,
                               std::string_view context_name) {
  const Feature* feature = part.find_feature(feature_id);
  if (feature == nullptr || feature->type() != FeatureType::AdditiveExtrude) {
    return Result<RectangularBoxExtent>::failure(validation_error(
        target.semantic_reference(),
        std::string(context_name) + " box feature must be a rectangular additive extrude"));
  }
  const Sketch* sketch = part.find_sketch(feature->input_sketch());
  if (sketch == nullptr || sketch->rectangle_profiles().size() != 1U ||
      sketch->profile_count() != 1U) {
    return Result<RectangularBoxExtent>::failure(validation_error(
        target.semantic_reference(),
        std::string(context_name) + " box feature requires exactly one rectangle profile"));
  }

  const RectangleProfile& rectangle = sketch->rectangle_profiles().front();
  const Parameter* width = part.find_parameter(rectangle.width_parameter());
  const Parameter* height = part.find_parameter(rectangle.height_parameter());
  const Parameter* thickness = part.find_parameter(feature->length_parameter());
  if (width == nullptr || height == nullptr || thickness == nullptr) {
    return Result<RectangularBoxExtent>::failure(validation_error(
        target.semantic_reference(),
        std::string(context_name) + " box dimensions must resolve to part parameters"));
  }

  return Result<RectangularBoxExtent>::success(
      RectangularBoxExtent{rectangle.center().x, rectangle.center().y, width->value().millimeters(),
                           height->value().millimeters(), thickness->value().millimeters()});
}

[[nodiscard]] AssemblyLineTargetDescriptor
generated_linear_edge_descriptor(SemanticEdge edge, const RectangularBoxExtent& box) {
  const double x_lo = box.center_x - box.width_mm / 2.0;
  const double x_hi = box.center_x + box.width_mm / 2.0;
  const double y_lo = box.center_y - box.height_mm / 2.0;
  const double y_hi = box.center_y + box.height_mm / 2.0;
  const double z_lo = 0.0;
  const double z_hi = box.thickness_mm;
  const double z_mid = box.thickness_mm / 2.0;
  const Vector3 along_x{1.0, 0.0, 0.0};
  const Vector3 along_y{0.0, 1.0, 0.0};
  const Vector3 along_z{0.0, 0.0, 1.0};
  switch (edge) {
  case SemanticEdge::TopFront:
    return AssemblyLineTargetDescriptor{Point3{box.center_x, y_hi, z_hi}, along_x};
  case SemanticEdge::TopBack:
    return AssemblyLineTargetDescriptor{Point3{box.center_x, y_lo, z_hi}, along_x};
  case SemanticEdge::TopRight:
    return AssemblyLineTargetDescriptor{Point3{x_hi, box.center_y, z_hi}, along_y};
  case SemanticEdge::TopLeft:
    return AssemblyLineTargetDescriptor{Point3{x_lo, box.center_y, z_hi}, along_y};
  case SemanticEdge::BottomFront:
    return AssemblyLineTargetDescriptor{Point3{box.center_x, y_hi, z_lo}, along_x};
  case SemanticEdge::BottomBack:
    return AssemblyLineTargetDescriptor{Point3{box.center_x, y_lo, z_lo}, along_x};
  case SemanticEdge::BottomRight:
    return AssemblyLineTargetDescriptor{Point3{x_hi, box.center_y, z_lo}, along_y};
  case SemanticEdge::BottomLeft:
    return AssemblyLineTargetDescriptor{Point3{x_lo, box.center_y, z_lo}, along_y};
  case SemanticEdge::FrontRight:
    return AssemblyLineTargetDescriptor{Point3{x_hi, y_hi, z_mid}, along_z};
  case SemanticEdge::FrontLeft:
    return AssemblyLineTargetDescriptor{Point3{x_lo, y_hi, z_mid}, along_z};
  case SemanticEdge::BackRight:
    return AssemblyLineTargetDescriptor{Point3{x_hi, y_lo, z_mid}, along_z};
  case SemanticEdge::BackLeft:
    return AssemblyLineTargetDescriptor{Point3{x_lo, y_lo, z_mid}, along_z};
  }
  return AssemblyLineTargetDescriptor{Point3{box.center_x, box.center_y, 0.0}, along_x};
}

[[nodiscard]] Point3 generated_vertex_descriptor(SemanticVertex vertex,
                                                 const RectangularBoxExtent& box) {
  const double x_lo = box.center_x - box.width_mm / 2.0;
  const double x_hi = box.center_x + box.width_mm / 2.0;
  const double y_lo = box.center_y - box.height_mm / 2.0;
  const double y_hi = box.center_y + box.height_mm / 2.0;
  const double z_lo = 0.0;
  const double z_hi = box.thickness_mm;
  switch (vertex) {
  case SemanticVertex::TopFrontRight:
    return Point3{x_hi, y_hi, z_hi};
  case SemanticVertex::TopFrontLeft:
    return Point3{x_lo, y_hi, z_hi};
  case SemanticVertex::TopBackRight:
    return Point3{x_hi, y_lo, z_hi};
  case SemanticVertex::TopBackLeft:
    return Point3{x_lo, y_lo, z_hi};
  case SemanticVertex::BottomFrontRight:
    return Point3{x_hi, y_hi, z_lo};
  case SemanticVertex::BottomFrontLeft:
    return Point3{x_lo, y_hi, z_lo};
  case SemanticVertex::BottomBackRight:
    return Point3{x_hi, y_lo, z_lo};
  case SemanticVertex::BottomBackLeft:
    return Point3{x_lo, y_lo, z_lo};
  }
  return Point3{box.center_x, box.center_y, 0.0};
}

// Both rims are coaxial circles at the two box faces the through-all cut passes
// through. Projecting the box corners onto the hole axis yields the two rim
// planes; source_rim is the plane nearest the sketched circle, opposite_rim the
// far plane. This mirrors the CircularCutAdapter's bounding-box axis extent.
[[nodiscard]] Point3 generated_circular_rim_center(const ComponentLocalAxisDescriptor& axis,
                                                   const RectangularBoxExtent& box,
                                                   SemanticCircularEdge rim) {
  const double x_lo = box.center_x - box.width_mm / 2.0;
  const double x_hi = box.center_x + box.width_mm / 2.0;
  const double y_lo = box.center_y - box.height_mm / 2.0;
  const double y_hi = box.center_y + box.height_mm / 2.0;
  const std::array<Point3, 8> corners{
      Point3{x_lo, y_lo, 0.0},          Point3{x_lo, y_lo, box.thickness_mm},
      Point3{x_lo, y_hi, 0.0},          Point3{x_lo, y_hi, box.thickness_mm},
      Point3{x_hi, y_lo, 0.0},          Point3{x_hi, y_lo, box.thickness_mm},
      Point3{x_hi, y_hi, 0.0},          Point3{x_hi, y_hi, box.thickness_mm}};

  double s_min = dot(corners.front(), axis.direction);
  double s_max = s_min;
  for (const Point3& corner : corners) {
    const double projection = dot(corner, axis.direction);
    s_min = std::min(s_min, projection);
    s_max = std::max(s_max, projection);
  }

  const double s0 = dot(axis.origin, axis.direction);
  const bool min_is_near = std::abs(s_min - s0) <= std::abs(s_max - s0);
  const double near = min_is_near ? s_min : s_max;
  const double far = min_is_near ? s_max : s_min;
  const double s = (rim == SemanticCircularEdge::SourceRim) ? near : far;
  const double delta = s - s0;
  return Point3{axis.origin.x + axis.direction.x * delta, axis.origin.y + axis.direction.y * delta,
                axis.origin.z + axis.direction.z * delta};
}

[[nodiscard]] Result<AssemblyResolvedGeometricTarget>
resolve_generated_topology_target(const ResolvedTargetContext& context,
                                  const AssemblyConstraintTarget& target,
                                  const GeneratedTopologyReferenceIdentity& identity) {
  // Block-35 producer identity/recovery contract. Geometry resolution starts
  // from validated semantic producer identity, not kernel topology order.
  auto validated = validate_generated_topology_reference(*context.part, identity);
  if (validated.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(validated.error());
  }

  const ComponentInstance& component = *context.component;
  const AssemblyLocalGeometricTargetEndpointIdentity endpoint{target.component_instance(),
                                                              target.semantic_reference()};

  return std::visit(
      [&](const auto& reference) -> Result<AssemblyResolvedGeometricTarget> {
        using Reference = std::decay_t<decltype(reference)>;

        if constexpr (std::is_same_v<Reference, SemanticCylindricalFaceReference>) {
          auto circular = resolve_circular_feature_geometry(
              context, target, reference.source_feature(), "generated cylindrical-face target");
          if (circular.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
          }
          const auto& value = circular.value();
          return validated_geometric_target(AssemblyResolvedGeometricTarget{
              endpoint, AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
              AssemblyGeometricTargetSourceMetadata{component.referenced_part_document(),
                                                    value.source_feature, value.source_profile,
                                                    std::nullopt, std::nullopt, std::nullopt},
              AssemblyCylindricalSurfaceTargetDescriptor{value.local_axis.origin,
                                                         value.local_axis.direction, value.radius_mm},
              assembly_geometric_target_capabilities(
                  AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace),
              AssemblyGeometricTargetCoordinateSpace::ComponentLocal, {component.transform()}});
        } else if constexpr (std::is_same_v<Reference, SemanticCircularEdgeReference>) {
          auto circular = resolve_circular_feature_geometry(
              context, target, reference.source_feature(), "generated circular-edge target");
          if (circular.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
          }
          const Feature* source_feature = context.part->find_feature(reference.source_feature());
          if (source_feature == nullptr) {
            return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
                target.semantic_reference(),
                "generated circular-edge target source feature must exist"));
          }
          auto box = resolve_rectangular_box_extent(*context.part, source_feature->target_feature(),
                                                    target, "generated circular-edge target");
          if (box.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(box.error());
          }
          const auto& value = circular.value();
          const Point3 center =
              generated_circular_rim_center(value.local_axis, box.value(), reference.edge());
          return validated_geometric_target(AssemblyResolvedGeometricTarget{
              endpoint, AssemblyGeometricTargetSourceKind::GeneratedCircularEdge,
              AssemblyGeometricTargetSourceMetadata{component.referenced_part_document(),
                                                    value.source_feature, value.source_profile,
                                                    std::nullopt, std::nullopt, std::nullopt},
              AssemblyCircularEdgeTargetDescriptor{center, value.local_seating_plane.x_axis,
                                                   value.local_seating_plane.y_axis,
                                                   value.local_axis.direction, value.radius_mm},
              assembly_geometric_target_capabilities(
                  AssemblyGeometricTargetSourceKind::GeneratedCircularEdge),
              AssemblyGeometricTargetCoordinateSpace::ComponentLocal, {component.transform()}});
        } else if constexpr (std::is_same_v<Reference, SemanticEdgeReference>) {
          auto box = resolve_rectangular_box_extent(*context.part, reference.source_feature(),
                                                    target, "generated linear-edge target");
          if (box.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(box.error());
          }
          return validated_geometric_target(AssemblyResolvedGeometricTarget{
              endpoint, AssemblyGeometricTargetSourceKind::GeneratedLinearEdge,
              AssemblyGeometricTargetSourceMetadata{component.referenced_part_document(),
                                                    reference.source_feature(), std::nullopt,
                                                    std::nullopt, std::nullopt, std::nullopt},
              generated_linear_edge_descriptor(reference.edge(), box.value()),
              assembly_geometric_target_capabilities(
                  AssemblyGeometricTargetSourceKind::GeneratedLinearEdge),
              AssemblyGeometricTargetCoordinateSpace::ComponentLocal, {component.transform()}});
        } else {
          auto box = resolve_rectangular_box_extent(*context.part, reference.source_feature(),
                                                    target, "generated vertex target");
          if (box.has_error()) {
            return Result<AssemblyResolvedGeometricTarget>::failure(box.error());
          }
          return validated_geometric_target(AssemblyResolvedGeometricTarget{
              endpoint, AssemblyGeometricTargetSourceKind::GeneratedVertex,
              AssemblyGeometricTargetSourceMetadata{component.referenced_part_document(),
                                                    reference.source_feature(), std::nullopt,
                                                    std::nullopt, std::nullopt, std::nullopt},
              AssemblyPointTargetDescriptor{
                  generated_vertex_descriptor(reference.vertex(), box.value())},
              assembly_geometric_target_capabilities(
                  AssemblyGeometricTargetSourceKind::GeneratedVertex),
              AssemblyGeometricTargetCoordinateSpace::ComponentLocal, {component.transform()}});
        }
      },
      identity);
}

} // namespace

Result<AssemblyResolvedGeometricTarget>
AssemblyConstraintTargetResolver::resolve_geometric(const Project& project,
                                                    const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(context.error());
  }

  if (is_assembly_reference_target_spelling(target.semantic_reference())) {
    auto identity = parse_assembly_reference_target_spelling(target.semantic_reference());
    if (identity.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(identity.error());
    }
    return resolve_reference_target(context.value(), target, identity.value());
  }

  // Canonical topo: generated-topology producer identity is parsed before the
  // legacy feature-role grammar; valid topo: spellings contain no '.'.
  if (is_generated_topology_target_spelling(target.semantic_reference())) {
    auto identity = parse_generated_topology_target_spelling(target.semantic_reference());
    if (identity.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(identity.error());
    }
    return resolve_generated_topology_target(context.value(), target, identity.value());
  }

  const std::size_t separator = target.semantic_reference().rfind('.');
  if (separator == std::string::npos || separator == 0U ||
      separator + 1U >= target.semantic_reference().size()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
        target.semantic_reference(), "assembly geometric target semantic reference is malformed"));
  }
  const std::string_view source_role =
      std::string_view(target.semantic_reference()).substr(separator + 1U);
  const AssemblyLocalGeometricTargetEndpointIdentity endpoint{target.component_instance(),
                                                              target.semantic_reference()};

  if (parse_semantic_face(source_role).has_value()) {
    auto face_reference = parse_generated_face_reference(target.semantic_reference());
    if (face_reference.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(face_reference.error());
    }
    auto resolved =
        resolve_generated_planar_target(context.value(), target, face_reference.value());
    if (resolved.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
    }
    const auto& value = resolved.value();
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
        AssemblyGeometricTargetSourceMetadata{value.referenced_part_document, value.source_feature,
                                              std::nullopt, value.face, std::nullopt, std::nullopt},
        AssemblyPlanarTargetDescriptor{value.local_plane.origin, value.local_plane.x_axis,
                                       value.local_plane.y_axis, value.local_plane.normal},
        assembly_geometric_target_capabilities(
            AssemblyGeometricTargetSourceKind::GeneratedPlanarFace),
        AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
        {value.component_transform}});
  }

  if (source_role == "axis") {
    auto axis_reference = parse_generated_axis_reference(target.semantic_reference());
    if (axis_reference.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(axis_reference.error());
    }
    auto circular = resolve_circular_feature_geometry(context.value(), target,
                                                      axis_reference.value().source_feature(),
                                                      "generated-axis assembly target");
    if (circular.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
    }
    const auto& value = circular.value();
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
        AssemblyGeometricTargetSourceMetadata{
            context.value().component->referenced_part_document(), value.source_feature,
            value.source_profile, std::nullopt, axis_reference.value().axis(), std::nullopt},
        AssemblyCylindricalSurfaceTargetDescriptor{value.local_axis.origin,
                                                   value.local_axis.direction, value.radius_mm},
        assembly_geometric_target_capabilities(
            AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace),
        AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
        {context.value().component->transform()}});
  }

  if (source_role == "seat") {
    auto seating_reference = parse_generated_seating_plane_reference(target.semantic_reference());
    if (seating_reference.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(seating_reference.error());
    }
    auto circular = resolve_circular_feature_geometry(context.value(), target,
                                                      seating_reference.value().source_feature(),
                                                      "generated-seat assembly target");
    if (circular.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
    }
    const auto& value = circular.value();
    const Vector3 frame_y = cross(value.local_axis.direction, value.local_seating_plane.x_axis);
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
        AssemblyGeometricTargetSourceMetadata{context.value().component->referenced_part_document(),
                                              value.source_feature, value.source_profile,
                                              std::nullopt, SemanticAxis::Primary,
                                              seating_reference.value().plane()},
        AssemblyFrameTargetDescriptor{value.local_seating_plane.origin,
                                      value.local_seating_plane.x_axis, frame_y,
                                      value.local_axis.direction},
        assembly_geometric_target_capabilities(
            AssemblyGeometricTargetSourceKind::CircularFeatureSeat),
        AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
        {context.value().component->transform()}});
  }

  return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
      target.semantic_reference(), "unsupported assembly geometric target semantic source"));
}

Result<ResolvedAssemblyConstraintTarget>
AssemblyConstraintTargetResolver::resolve(const Project& project,
                                          const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(context.error());
  }
  auto face_reference = parse_generated_face_reference(target.semantic_reference());
  if (face_reference.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(face_reference.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(typed.error());
  }
  auto plane = project_plane(typed.value());
  if (plane.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(plane.error());
  }
  return Result<ResolvedAssemblyConstraintTarget>::success(ResolvedAssemblyConstraintTarget{
      target.component_instance(), context.value().component->referenced_part_document(),
      face_reference.value().source_feature(), face_reference.value().face(),
      ComponentLocalPlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                     plane.value().y_axis, plane.value().normal},
      context.value().component->transform()});
}

Result<ResolvedAssemblyAxisConstraintTarget>
AssemblyConstraintTargetResolver::resolve_axis(const Project& project,
                                               const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(context.error());
  }
  auto axis_reference = parse_generated_axis_reference(target.semantic_reference());
  if (axis_reference.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(axis_reference.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(typed.error());
  }
  auto axis = project_axis(typed.value());
  if (axis.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(axis.error());
  }
  if (!typed.value().source_metadata.source_profile.has_value()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(), "generated-axis assembly target source profile is missing"));
  }
  return Result<ResolvedAssemblyAxisConstraintTarget>::success(ResolvedAssemblyAxisConstraintTarget{
      target.component_instance(), context.value().component->referenced_part_document(),
      axis_reference.value().source_feature(), typed.value().source_metadata.source_profile.value(),
      axis_reference.value().axis(),
      ComponentLocalAxisDescriptor{axis.value().origin, axis.value().direction},
      context.value().component->transform()});
}

Result<ResolvedAssemblyInsertConstraintTarget>
AssemblyConstraintTargetResolver::resolve_insert(const Project& project,
                                                 const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(context.error());
  }
  auto seating_reference = parse_generated_seating_plane_reference(target.semantic_reference());
  if (seating_reference.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(seating_reference.error());
  }
  auto typed = resolve_geometric(project, target);
  if (typed.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(typed.error());
  }
  auto axis = project_axis(typed.value());
  if (axis.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(axis.error());
  }
  auto plane = project_plane(typed.value());
  if (plane.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(plane.error());
  }
  if (!typed.value().source_metadata.source_profile.has_value()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(validation_error(
        target.semantic_reference(), "generated-seat assembly target source profile is missing"));
  }
  return Result<ResolvedAssemblyInsertConstraintTarget>::success(
      ResolvedAssemblyInsertConstraintTarget{
          target.component_instance(), context.value().component->referenced_part_document(),
          seating_reference.value().source_feature(),
          typed.value().source_metadata.source_profile.value(), SemanticAxis::Primary,
          seating_reference.value().plane(),
          ComponentLocalAxisDescriptor{axis.value().origin, axis.value().direction},
          ComponentLocalPlanarDescriptor{plane.value().origin, plane.value().x_axis,
                                         plane.value().y_axis, plane.value().normal},
          context.value().component->transform()});
}

} // namespace blcad::geometry
