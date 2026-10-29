#include "blcad/geometry/step_reference_commands.hpp"

#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_import_validation.hpp"
#include "blcad/geometry/reference_part_geometry.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include <algorithm>

namespace blcad::geometry {
namespace {
Error invalid(std::string message) {
  return Error::validation("step.reference", std::move(message));
}
Result<std::size_t> validate_part(const PartDocument& part, const std::filesystem::path& root) {
  StepImportTopologyContext context(root);
  ReferencePartGeometry adapter;
  if (part.is_reference_part()) {
    auto result = adapter.resolve(part, context);
    if (!result)
      return Result<std::size_t>::failure(result.error());
  } else {
    for (const auto& feature : part.imported_body_features()) {
      auto result = adapter.resolve_imported_body_feature(part, feature, context);
      if (!result)
        return Result<std::size_t>::failure(result.error());
    }
  }
  return context.validate_sources();
}
Result<std::size_t> validate_project(const Project& project, const std::filesystem::path& root) {
  auto structure = project.validate_assembly_structure();
  if (!structure)
    return structure;
  StepImportTopologyContext context(root);
  return validate_imported_assembly_inputs(project, context);
}
Result<std::size_t> append_occurrence(Project& project, DocumentId part, DocumentId assembly,
                                      ComponentInstanceId id, RigidTransform transform) {
  auto* target = project.find_assembly_document(assembly);
  const auto* definition = project.find_part_document(part);
  if (!target || !definition || !definition->is_reference_part())
    return Result<std::size_t>::failure(invalid("Choose an owned Reference Part and assembly"));
  if (!target->has_member_part(part)) {
    auto member = target->add_member_part(part);
    if (!member)
      return member;
  }
  auto instance = ComponentInstance::create(
      id, definition->name(), part, ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, transform);
  if (!instance)
    return Result<std::size_t>::failure(instance.error());
  return target->add_component_instance(std::move(instance.value()));
}
} // namespace

Result<PartDocument> StepReferenceCommands::create_definition(
    const StepImportSource& source, const ImportedTopologyCatalog& catalog, DocumentId id,
    std::string name, const std::filesystem::path& root) const {
  if (source.mode() != StepImportMode::Reference || catalog.source() != source.id())
    return Result<PartDocument>::failure(
        invalid("Reference import requires a Reference source and its saved catalog"));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries()) {
    if (entry.descriptor.kind != ImportedTopologyKind::Body)
      continue;
    if (!entry.descriptor.body_kind)
      return Result<PartDocument>::failure(invalid("Imported body has no authored kind"));
    outputs.push_back({entry.id, BodyId("body.imported." + std::to_string(outputs.size() + 1)),
                       *entry.descriptor.body_kind});
  }
  auto definition = ImportedPartDefinition::create(source.id(), catalog.id(), std::move(outputs));
  if (!definition)
    return Result<PartDocument>::failure(definition.error());
  auto part = PartDocument::create_reference(std::move(id), std::move(name), source, catalog,
                                             std::move(definition.value()));
  if (!part)
    return part;
  auto valid = validate_part(part.value(), root);
  if (!valid)
    return Result<PartDocument>::failure(valid.error());
  return part;
}

Result<StepReferenceProjectCandidate>
StepReferenceCommands::insert(const Project& original, ImportedTopologyCatalogId catalog_id,
                              DocumentId new_part, std::string name, DocumentId assembly,
                              ComponentInstanceId instance, RigidTransform transform,
                              const std::filesystem::path& root) const {
  const auto* catalog = original.find_imported_topology_catalog(catalog_id);
  const auto* source = catalog ? original.find_step_import_source(catalog->source()) : nullptr;
  if (!catalog || !source || source->mode() != StepImportMode::Reference)
    return Result<StepReferenceProjectCandidate>::failure(
        invalid("Choose a saved Reference source topology catalog"));
  Project candidate = original;
  DocumentId selected;
  const auto complete_product_mapping = [&](const ImportedPartDefinition& definition) {
    const auto count =
        std::count_if(catalog->entries().begin(), catalog->entries().end(), [](const auto& entry) {
          return entry.descriptor.kind == ImportedTopologyKind::Body;
        });
    return definition.outputs().size() == static_cast<std::size_t>(count) &&
           std::all_of(definition.outputs().begin(), definition.outputs().end(),
                       [&](const auto& output) {
                         const auto* entry = catalog->find(output.imported_body);
                         return entry && entry->descriptor.kind == ImportedTopologyKind::Body &&
                                entry->descriptor.body_kind == output.kind;
                       });
  };
  for (const auto& part : original.part_documents()) {
    const auto* binding = part.reference_part_definition();
    const auto* bound_source = binding ? part.find_step_import_source(binding->source()) : nullptr;
    const auto* bound_catalog =
        binding ? part.find_imported_topology_catalog(binding->catalog()) : nullptr;
    if (binding && bound_source && bound_catalog && *bound_source == *source &&
        *bound_catalog == *catalog && complete_product_mapping(*binding)) {
      if (selected)
        return Result<StepReferenceProjectCandidate>::failure(invalid(
            "Several definitions share this exact binding; explicitly select a Reference Part"));
      selected = part.id();
    }
  }
  const bool reused = bool(selected);
  if (!reused) {
    auto part = create_definition(*source, *catalog, new_part, std::move(name), root);
    if (!part)
      return Result<StepReferenceProjectCandidate>::failure(part.error());
    auto added = candidate.add_part_document(std::move(part.value()));
    if (!added)
      return Result<StepReferenceProjectCandidate>::failure(added.error());
    selected = new_part;
  }
  auto added = append_occurrence(candidate, selected, assembly, instance, transform);
  if (!added)
    return Result<StepReferenceProjectCandidate>::failure(added.error());
  auto valid = validate_project(candidate, root);
  if (!valid)
    return Result<StepReferenceProjectCandidate>::failure(valid.error());
  return Result<StepReferenceProjectCandidate>::success(
      {std::move(candidate),
       selected,
       {selected, assembly},
       {reused ? "Reused the exact saved Reference Part binding; added one occurrence."
               : "Created an immutable Reference Part and its first occurrence."}});
}
Result<StepReferenceProjectCandidate>
StepReferenceCommands::repeat(const Project& original, DocumentId part, DocumentId assembly,
                              ComponentInstanceId instance, RigidTransform transform,
                              const std::filesystem::path& root) const {
  Project candidate = original;
  auto added = append_occurrence(candidate, part, assembly, instance, transform);
  if (!added)
    return Result<StepReferenceProjectCandidate>::failure(added.error());
  auto valid = validate_project(candidate, root);
  if (!valid)
    return Result<StepReferenceProjectCandidate>::failure(valid.error());
  return Result<StepReferenceProjectCandidate>::success(
      {std::move(candidate),
       part,
       {assembly},
       {"Added an occurrence of the existing Reference Part."}});
}
Result<StepReferenceProjectCandidate>
StepReferenceCommands::refresh(const Project& original, DocumentId part_id,
                               const std::filesystem::path& root) const {
  const auto* part = original.find_part_document(part_id);
  const auto* binding = part ? part->reference_part_definition() : nullptr;
  const auto* source = binding ? part->find_step_import_source(binding->source()) : nullptr;
  if (!source)
    return Result<StepReferenceProjectCandidate>::failure(
        invalid("Choose a Reference Part to refresh"));
  return StepEditableCommands{}.refresh(original, part_id, source->id(), root);
}
Result<StepReferencePartCandidate>
StepReferenceCommands::refresh(const PartDocument& original,
                               const std::filesystem::path& root) const {
  const auto* binding = original.reference_part_definition();
  const auto* source = binding ? original.find_step_import_source(binding->source()) : nullptr;
  if (!source)
    return Result<StepReferencePartCandidate>::failure(invalid("Open a Reference Part to refresh"));
  return StepEditableCommands{}.refresh(original, source->id(), root);
}
Result<StepReferencePartCandidate>
StepReferenceCommands::convert(const PartDocument& original, DocumentId id, std::string name,
                               FeatureId feature, const std::filesystem::path& root) const {
  const auto* binding = original.reference_part_definition();
  if (!binding)
    return Result<StepReferencePartCandidate>::failure(
        invalid("Choose a Reference Part to convert"));
  auto valid = validate_part(original, root);
  if (!valid)
    return Result<StepReferencePartCandidate>::failure(valid.error());
  std::vector<BodyId> bodies;
  for (std::size_t index = 0; index < binding->outputs().size(); ++index)
    bodies.emplace_back("body.editable.imported." + std::to_string(index + 1));
  auto converted = original.convert_reference_to_editable(std::move(id), std::move(name),
                                                          std::move(feature), std::move(bodies));
  if (!converted)
    return Result<StepReferencePartCandidate>::failure(converted.error());
  valid = validate_part(converted.value(), root);
  if (!valid)
    return Result<StepReferencePartCandidate>::failure(valid.error());
  return Result<StepReferencePartCandidate>::success(
      {std::move(converted.value()),
       {original.id()},
       {"Created a new Editable Part with a persistent Imported Body base feature. The Reference "
        "Part and all occurrences are preserved."}});
}
Result<StepReferenceProjectCandidate>
StepReferenceCommands::convert(const Project& original, DocumentId reference, DocumentId id,
                               std::string name, FeatureId feature,
                               const std::filesystem::path& root) const {
  const auto* part = original.find_part_document(reference);
  if (!part)
    return Result<StepReferenceProjectCandidate>::failure(
        invalid("Reference Part is not owned by this Project"));
  auto converted = convert(*part, id, std::move(name), std::move(feature), root);
  if (!converted)
    return Result<StepReferenceProjectCandidate>::failure(converted.error());
  Project candidate = original;
  auto added = candidate.add_part_document(std::move(converted.value().part));
  if (!added)
    return Result<StepReferenceProjectCandidate>::failure(added.error());
  auto valid = validate_project(candidate, root);
  if (!valid)
    return Result<StepReferenceProjectCandidate>::failure(valid.error());
  return Result<StepReferenceProjectCandidate>::success(
      {std::move(candidate), id, {id}, std::move(converted.value().diagnostics)});
}
} // namespace blcad::geometry
