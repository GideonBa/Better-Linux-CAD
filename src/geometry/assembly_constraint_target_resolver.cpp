#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include "blcad/geometry/workplane_resolver.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr std::string_view k_feature_reference_prefix = "feature.";

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
  if (!source_feature.starts_with(k_feature_reference_prefix) ||
      source_feature.size() == k_feature_reference_prefix.size() || !face.has_value()) {
    return Result<SemanticFaceReference>::failure(validation_error(
        std::string(semantic_reference), "unsupported assembly semantic reference family"));
  }

  return SemanticFaceReference::create(FeatureId(std::string(source_feature)), face.value());
}

[[nodiscard]] DatumPlaneId
make_temporary_workplane_id(const PartDocument& document,
                            const SemanticFaceReference& face_reference) {
  const std::string base_id = "__assembly_target_resolution__." +
                              face_reference.source_feature().value() + "." +
                              std::string(to_string(face_reference.face()));
  std::string candidate = base_id;
  std::size_t suffix = 0U;
  while (document.has_workplane_id(DatumPlaneId(candidate))) {
    ++suffix;
    candidate = base_id + "." + std::to_string(suffix);
  }
  return DatumPlaneId(std::move(candidate));
}

} // namespace

Result<ResolvedAssemblyConstraintTarget>
AssemblyConstraintTargetResolver::resolve(const Project& project,
                                          const AssemblyConstraintTarget& target) const {
  const ComponentInstance* component =
      project.assembly().find_component_instance(target.component_instance());
  if (component == nullptr) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(validation_error(
        target.component_instance().value(),
        "assembly constraint target component instance must exist in project assembly"));
  }

  const PartDocument* part = project.find_part_document(component->referenced_part_document());
  if (part == nullptr) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(validation_error(
        component->id().value(),
        "assembly constraint target referenced part must resolve to an owned project part document"));
  }

  auto face_reference = parse_generated_face_reference(target.semantic_reference());
  if (face_reference.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(face_reference.error());
  }

  const Feature* source_feature = part->find_feature(face_reference.value().source_feature());
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

  PartDocument resolution_document = *part;
  const DatumPlaneId workplane_id =
      make_temporary_workplane_id(resolution_document, face_reference.value());
  auto workplane = DerivedWorkplane::create_on_feature_face(
      workplane_id, "AssemblyTargetResolution", face_reference.value());
  if (workplane.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(workplane.error());
  }
  auto added = resolution_document.add_derived_workplane(workplane.value());
  if (added.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(added.error());
  }

  const WorkplaneResolver workplane_resolver;
  auto resolved_workplane = workplane_resolver.resolve(resolution_document, workplane_id);
  if (resolved_workplane.has_error()) {
    return Result<ResolvedAssemblyConstraintTarget>::failure(resolved_workplane.error());
  }

  const ResolvedWorkplane& plane = resolved_workplane.value();
  return Result<ResolvedAssemblyConstraintTarget>::success(ResolvedAssemblyConstraintTarget{
      component->id(), component->referenced_part_document(),
      face_reference.value().source_feature(), face_reference.value().face(),
      ComponentLocalPlanarDescriptor{plane.origin, plane.x_axis, plane.y_axis, plane.normal},
      component->transform()});
}

} // namespace blcad::geometry
