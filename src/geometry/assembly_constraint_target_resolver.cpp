#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include "blcad/geometry/workplane_resolver.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    return Result<ResolvedTargetContext>::failure(validation_error(
        component->id().value(),
        "assembly constraint target referenced part must resolve to an owned project part document"));
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

[[nodiscard]] Vector3 extrude_direction(const ResolvedWorkplane& workplane,
                                         ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return negate(workplane.normal);
  }
  return workplane.normal;
}

[[nodiscard]] ComponentLocalPlanarDescriptor
seating_plane(const ResolvedWorkplane& workplane,
              Point3 axis_origin,
              ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return ComponentLocalPlanarDescriptor{axis_origin, workplane.x_axis, negate(workplane.y_axis),
                                          negate(workplane.normal)};
  }
  return ComponentLocalPlanarDescriptor{axis_origin, workplane.x_axis, workplane.y_axis,
                                        workplane.normal};
}

[[nodiscard]] Result<ResolvedAssemblyConstraintTarget> resolve_generated_planar_target(
    const ResolvedTargetContext& context,
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
    const ResolvedTargetContext& context,
    const AssemblyConstraintTarget& target,
    const FeatureId& source_feature_id,
    std::string_view family_name) {
  const PartDocument& part = *context.part;
  const Feature* source_feature = part.find_feature(source_feature_id);
  if (source_feature == nullptr) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(), std::string(family_name) +
                                         " source feature must exist in referenced part document"));
  }
  if (source_feature->type() != FeatureType::SubtractiveExtrude) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(),
        std::string(family_name) + " source feature must be a subtractive extrude"));
  }

  const Sketch* sketch = part.find_sketch(source_feature->input_sketch());
  if (sketch == nullptr) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(), std::string(family_name) +
                                         " source feature input sketch must exist"));
  }
  if (sketch->circle_profiles().size() != 1U || sketch->profile_count() != 1U) {
    return Result<ResolvedCircularFeatureGeometry>::failure(validation_error(
        target.semantic_reference(), std::string(family_name) +
                                         " requires exactly one CircleProfile in the source sketch"));
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

} // namespace

Result<AssemblyResolvedGeometricTarget> AssemblyConstraintTargetResolver::resolve_geometric(
    const Project& project, const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(context.error());
  }

  const std::size_t separator = target.semantic_reference().rfind('.');
  if (separator == std::string::npos || separator == 0U ||
      separator + 1U >= target.semantic_reference().size()) {
    return Result<AssemblyResolvedGeometricTarget>::failure(validation_error(
        target.semantic_reference(), "assembly geometric target semantic reference is malformed"));
  }
  const std::string_view source_role =
      std::string_view(target.semantic_reference()).substr(separator + 1U);

  const AssemblyLocalGeometricTargetEndpointIdentity endpoint{
      target.component_instance(), target.semantic_reference()};

  if (parse_semantic_face(source_role).has_value()) {
    auto face_reference = parse_generated_face_reference(target.semantic_reference());
    if (face_reference.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(face_reference.error());
    }
    auto resolved = resolve_generated_planar_target(context.value(), target, face_reference.value());
    if (resolved.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(resolved.error());
    }
    const auto& value = resolved.value();
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
        AssemblyGeometricTargetSourceMetadata{value.referenced_part_document,
                                              value.source_feature,
                                              std::nullopt,
                                              value.face,
                                              std::nullopt,
                                              std::nullopt},
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
    auto circular = resolve_circular_feature_geometry(
        context.value(), target, axis_reference.value().source_feature(),
        "generated-axis assembly target");
    if (circular.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
    }
    const auto& value = circular.value();
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
        AssemblyGeometricTargetSourceMetadata{context.value().component->referenced_part_document(),
                                              value.source_feature,
                                              value.source_profile,
                                              std::nullopt,
                                              axis_reference.value().axis(),
                                              std::nullopt},
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
    auto circular = resolve_circular_feature_geometry(
        context.value(), target, seating_reference.value().source_feature(),
        "generated-seat assembly target");
    if (circular.has_error()) {
      return Result<AssemblyResolvedGeometricTarget>::failure(circular.error());
    }
    const auto& value = circular.value();
    return validated_geometric_target(AssemblyResolvedGeometricTarget{
        endpoint,
        AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
        AssemblyGeometricTargetSourceMetadata{context.value().component->referenced_part_document(),
                                              value.source_feature,
                                              value.source_profile,
                                              std::nullopt,
                                              SemanticAxis::Primary,
                                              seating_reference.value().plane()},
        AssemblyFrameTargetDescriptor{value.local_seating_plane.origin,
                                      value.local_seating_plane.x_axis,
                                      value.local_seating_plane.y_axis,
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
  return resolve_generated_planar_target(context.value(), target, face_reference.value());
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
  auto circular = resolve_circular_feature_geometry(
      context.value(), target, axis_reference.value().source_feature(),
      "generated-axis assembly target");
  if (circular.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(circular.error());
  }

  const ComponentInstance& component = *context.value().component;
  const auto& value = circular.value();
  return Result<ResolvedAssemblyAxisConstraintTarget>::success(
      ResolvedAssemblyAxisConstraintTarget{
          component.id(), component.referenced_part_document(), value.source_feature,
          value.source_profile, axis_reference.value().axis(), value.local_axis,
          component.transform()});
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
  auto circular = resolve_circular_feature_geometry(
      context.value(), target, seating_reference.value().source_feature(),
      "generated-seat assembly target");
  if (circular.has_error()) {
    return Result<ResolvedAssemblyInsertConstraintTarget>::failure(circular.error());
  }

  const ComponentInstance& component = *context.value().component;
  const auto& value = circular.value();
  return Result<ResolvedAssemblyInsertConstraintTarget>::success(
      ResolvedAssemblyInsertConstraintTarget{
          component.id(), component.referenced_part_document(), value.source_feature,
          value.source_profile, SemanticAxis::Primary, seating_reference.value().plane(),
          value.local_axis, value.local_seating_plane, component.transform()});
}

} // namespace blcad::geometry
