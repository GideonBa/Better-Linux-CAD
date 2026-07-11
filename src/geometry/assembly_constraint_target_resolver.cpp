#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include "blcad/geometry/workplane_resolver.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace blcad::geometry {
namespace {

struct ResolvedTargetContext {
  const ComponentInstance* component = nullptr;
  const PartDocument* part = nullptr;
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

[[nodiscard]] Vector3 extrude_direction(const ResolvedWorkplane& workplane,
                                        ExtrudeDirection direction) noexcept {
  if (direction == ExtrudeDirection::OppositeSketchNormal) {
    return Vector3{-workplane.normal.x, -workplane.normal.y, -workplane.normal.z};
  }
  return workplane.normal;
}

} // namespace

Result<ResolvedAssemblyConstraintTarget>
AssemblyConstraintTargetResolver::resolve(const Project& project,
                                          const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(context.error());
  }

  const ComponentInstance& component = *context.value().component;
  const PartDocument& part = *context.value().part;
  auto face_reference = parse_generated_face_reference(target.semantic_reference());
  if (face_reference.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(face_reference.error());
  }

  const Feature* source_feature = part.find_feature(face_reference.value().source_feature());
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
  auto resolved_workplane =
      workplane_resolver.resolve_generated_face(part, face_reference.value());
  if (resolved_workplane.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(resolved_workplane.error());
  }

  const ResolvedWorkplane& plane = resolved_workplane.value();
  return Result<ResolvedAssemblyConstraintTarget>::success(ResolvedAssemblyConstraintTarget{
      component.id(), component.referenced_part_document(), face_reference.value().source_feature(),
      face_reference.value().face(),
      ComponentLocalPlanarDescriptor{plane.origin, plane.x_axis, plane.y_axis, plane.normal},
      component.transform()});
}

Result<ResolvedAssemblyAxisConstraintTarget>
AssemblyConstraintTargetResolver::resolve_axis(const Project& project,
                                               const AssemblyConstraintTarget& target) const {
  auto context = resolve_target_context(project, target);
  if (context.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(context.error());
  }

  const ComponentInstance& component = *context.value().component;
  const PartDocument& part = *context.value().part;
  auto axis_reference = parse_generated_axis_reference(target.semantic_reference());
  if (axis_reference.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(axis_reference.error());
  }

  const Feature* source_feature = part.find_feature(axis_reference.value().source_feature());
  if (source_feature == nullptr) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-axis assembly target source feature must exist in referenced part document"));
  }
  if (source_feature->type() != FeatureType::SubtractiveExtrude) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-axis assembly target source feature must be a subtractive extrude"));
  }

  const Sketch* sketch = part.find_sketch(source_feature->input_sketch());
  if (sketch == nullptr) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-axis assembly target source feature input sketch must exist"));
  }
  if (sketch->circle_profiles().size() != 1U || sketch->profile_count() != 1U) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-axis assembly target requires exactly one CircleProfile in the source sketch"));
  }

  const CircleProfile& circle = sketch->circle_profiles().front();
  const Parameter* diameter = part.find_parameter(circle.diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(validation_error(
        target.semantic_reference(),
        "generated-axis assembly target circle diameter must resolve to a length parameter"));
  }

  const WorkplaneResolver workplane_resolver;
  auto resolved_workplane = workplane_resolver.resolve_for_sketch(part, *sketch);
  if (resolved_workplane.has_error()) {
    return Result<ResolvedAssemblyAxisConstraintTarget>::failure(resolved_workplane.error());
  }

  const ResolvedWorkplane& workplane = resolved_workplane.value();
  return Result<ResolvedAssemblyAxisConstraintTarget>::success(
      ResolvedAssemblyAxisConstraintTarget{
          component.id(), component.referenced_part_document(),
          axis_reference.value().source_feature(), circle.id(), axis_reference.value().axis(),
          ComponentLocalAxisDescriptor{workplane_resolver.evaluate_point(workplane, circle.center()),
                                       extrude_direction(workplane, source_feature->direction())},
          component.transform()});
}

} // namespace blcad::geometry
