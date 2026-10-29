#include "blcad/geometry/step_editable_commands.hpp"

#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_import_validation.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <algorithm>
#include <set>

namespace blcad::geometry {
namespace {
Error invalid(std::string message) {
  return Error::validation("step.editable", std::move(message));
}

Result<std::size_t> validate_part(const PartDocument& part, StepImportTopologyContext& context) {
  auto cache = ShapeCache::create(ShapeCacheId("step.editable.candidate"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto result = GeometryRecomputeExecutor{&context}.execute_document(part, cache.value());
  if (!result)
    return Result<std::size_t>::failure(result.error());
  return context.validate_sources();
}

Result<std::size_t> validate_project(const Project& project, const std::filesystem::path& root) {
  auto structure = project.validate_assembly_structure();
  if (!structure)
    return structure;
  StepImportTopologyContext context(root);
  for (const auto& part : project.part_documents()) {
    auto valid = validate_part(part, context);
    if (!valid)
      return valid;
  }
  return validate_imported_assembly_inputs(project, context);
}

Result<StepImportSource> consumed_source(const PartDocument& part, StepImportSourceId id) {
  std::set<std::string> consumed;
  if (const auto* reference = part.reference_part_definition())
    consumed.insert(reference->source().value());
  for (const auto& feature : part.imported_body_features())
    consumed.insert(feature.definition().source().value());
  if (id.empty()) {
    if (consumed.size() != 1)
      return Result<StepImportSource>::failure(
          invalid("Choose the source consumed by this imported Part before refreshing"));
    id = StepImportSourceId(*consumed.begin());
  }
  const auto* source = part.find_step_import_source(id);
  if (!source || !consumed.contains(id.value()))
    return Result<StepImportSource>::failure(
        invalid("Refresh requires a source consumed by this owner's imported geometry"));
  return Result<StepImportSource>::success(*source);
}

bool same_asset(const StepImportSource& first, const StepImportSource& second,
                const std::filesystem::path& root) {
  if (first.project_relative_asset_path() == second.project_relative_asset_path())
    return true;
  std::error_code error;
  return std::filesystem::equivalent(root / first.project_relative_asset_path(),
                                     root / second.project_relative_asset_path(), error) &&
         !error;
}

template <class Owner>
Result<std::size_t>
refresh_owner(Owner& owner, const StepImportSource& selected, const std::filesystem::path& root,
              std::vector<DocumentId>& affected, std::vector<std::string>& diagnostics) {
  const auto sources = owner.step_import_sources();
  std::size_t count{};
  for (const auto& source : sources) {
    if (!same_asset(source, selected, root))
      continue;
    auto report = inspect_step_import_source(source, root);
    if (!report)
      return Result<std::size_t>::failure(report.error());
    if (!report.value().observed_sha256_digest())
      return Result<std::size_t>::failure(invalid(report.value().diagnostic()));
    auto refreshed = refresh_step_import_source(owner, root, report.value());
    if (!refreshed)
      return refreshed;
    diagnostics.push_back(owner.id().value() + "/" + source.id().value() + ": " +
                          source.project_relative_asset_path() + " → " +
                          *report.value().observed_sha256_digest());
    ++count;
  }
  if (count)
    affected.push_back(owner.id());
  return Result<std::size_t>::success(count);
}
} // namespace

Result<PartDocument> StepEditableCommands::create_definition(
    const StepImportSource& source, const ImportedTopologyCatalog& catalog, DocumentId id,
    std::string name, FeatureId feature, const std::filesystem::path& root) const {
  if (source.mode() != StepImportMode::EditableBody || catalog.source() != source.id())
    return Result<PartDocument>::failure(
        invalid("Editable import requires an EditableBody source and its saved catalog"));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries()) {
    if (entry.descriptor.kind != ImportedTopologyKind::Body)
      continue;
    if (!entry.descriptor.body_kind)
      return Result<PartDocument>::failure(invalid("Imported body has no supported kind"));
    outputs.push_back({entry.id, BodyId("body.imported." + std::to_string(outputs.size() + 1)),
                       *entry.descriptor.body_kind});
  }
  auto definition = ImportedPartDefinition::create(source.id(), catalog.id(), std::move(outputs));
  if (!definition)
    return Result<PartDocument>::failure(definition.error());
  auto base = ImportedBodyFeature::create(std::move(feature), "Imported body",
                                          std::move(definition.value()));
  if (!base)
    return Result<PartDocument>::failure(base.error());
  auto part = PartDocument::create_editable_import(std::move(id), std::move(name), source, catalog,
                                                   std::move(base.value()));
  if (!part)
    return part;
  StepImportTopologyContext context(root);
  auto valid = validate_part(part.value(), context);
  if (!valid)
    return Result<PartDocument>::failure(valid.error());
  return part;
}

Result<StepEditableProjectCandidate>
StepEditableCommands::insert(const Project& original, ImportedTopologyCatalogId catalog_id,
                             DocumentId id, std::string name, FeatureId feature,
                             DocumentId assembly, ComponentInstanceId instance,
                             RigidTransform transform, const std::filesystem::path& root) const {
  const auto* catalog = original.find_imported_topology_catalog(catalog_id);
  const auto* source = catalog ? original.find_step_import_source(catalog->source()) : nullptr;
  if (!catalog || !source)
    return Result<StepEditableProjectCandidate>::failure(
        invalid("Choose a saved EditableBody product catalog"));
  auto part = create_definition(*source, *catalog, id, std::move(name), std::move(feature), root);
  if (!part)
    return Result<StepEditableProjectCandidate>::failure(part.error());
  auto candidate = original;
  auto* target = candidate.find_assembly_document(assembly);
  if (!target)
    return Result<StepEditableProjectCandidate>::failure(invalid("Target Assembly is not owned"));
  auto added = candidate.add_part_document(std::move(part.value()));
  if (!added)
    return Result<StepEditableProjectCandidate>::failure(added.error());
  added = target->add_member_part(id);
  if (!added)
    return Result<StepEditableProjectCandidate>::failure(added.error());
  auto occurrence = ComponentInstance::create(
      instance, candidate.find_part_document(id)->name(), id, ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  if (!occurrence)
    return Result<StepEditableProjectCandidate>::failure(occurrence.error());
  added = target->add_component_instance(std::move(occurrence.value()));
  if (!added)
    return Result<StepEditableProjectCandidate>::failure(added.error());
  auto valid = validate_project(candidate, root);
  if (!valid)
    return Result<StepEditableProjectCandidate>::failure(valid.error());
  return Result<StepEditableProjectCandidate>::success(
      {std::move(candidate),
       id,
       {id, assembly},
       {"Created an Editable Part with a persistent Imported Body base and its first "
        "occurrence."}});
}

Result<StepEditablePartCandidate>
StepEditableCommands::refresh(const PartDocument& original, StepImportSourceId id,
                              const std::filesystem::path& root) const {
  auto source = consumed_source(original, std::move(id));
  if (!source)
    return Result<StepEditablePartCandidate>::failure(source.error());
  StepEditablePartCandidate result{original, {}, {}};
  auto changed =
      refresh_owner(result.part, source.value(), root, result.affected_owners, result.diagnostics);
  if (!changed)
    return Result<StepEditablePartCandidate>::failure(changed.error());
  StepImportTopologyContext context(root);
  auto valid = validate_part(result.part, context);
  if (!valid)
    return Result<StepEditablePartCandidate>::failure(valid.error());
  return Result<StepEditablePartCandidate>::success(std::move(result));
}

Result<StepEditableProjectCandidate>
StepEditableCommands::refresh(const Project& original, DocumentId part, StepImportSourceId id,
                              const std::filesystem::path& root) const {
  const auto* owner = original.find_part_document(part);
  if (!owner)
    return Result<StepEditableProjectCandidate>::failure(invalid("Imported Part is not owned"));
  auto source = consumed_source(*owner, std::move(id));
  if (!source)
    return Result<StepEditableProjectCandidate>::failure(source.error());
  StepEditableProjectCandidate result{original, part, {}, {}};
  auto changed = refresh_owner(result.project, source.value(), root, result.affected_owners,
                               result.diagnostics);
  if (!changed)
    return Result<StepEditableProjectCandidate>::failure(changed.error());
  for (auto& owned : result.project.part_documents()) {
    changed =
        refresh_owner(owned, source.value(), root, result.affected_owners, result.diagnostics);
    if (!changed)
      return Result<StepEditableProjectCandidate>::failure(changed.error());
  }
  auto valid = validate_project(result.project, root);
  if (!valid)
    return Result<StepEditableProjectCandidate>::failure(valid.error());
  return Result<StepEditableProjectCandidate>::success(std::move(result));
}
} // namespace blcad::geometry
