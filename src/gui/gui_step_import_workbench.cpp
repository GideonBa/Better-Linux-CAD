#include "blcad/gui/gui_step_import_workbench.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/reference_part_geometry.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include <QFile>
#include <QTemporaryFile>
#include <algorithm>

namespace blcad::gui {
namespace {
Error invalid(std::string message) {
  return Error::validation("gui.step_import", std::move(message));
}
Result<std::filesystem::path> document_path(const GuiDocumentSession& session) {
  if (!session.has_document() || session.file_path().empty())
    return Result<std::filesystem::path>::failure(
        invalid("Save the document before registering or inspecting STEP sources"));
  std::error_code error;
  auto path = std::filesystem::absolute(session.file_path(), error);
  if (error)
    return Result<std::filesystem::path>::failure(invalid("Document path cannot be resolved"));
  return Result<std::filesystem::path>::success(path.lexically_normal());
}
Result<std::string> snapshot(const GuiDocumentSession& session) {
  if (const auto* part = session.part_document())
    return serialize_part_document_to_json(*part);
  if (const auto* project = session.project())
    return serialize_project_to_json(*project);
  return Result<std::string>::failure(invalid("An active document is required"));
}
const StepImportSource* source(const GuiDocumentSession& session, StepImportSourceId id) {
  return session.part_document() ? session.part_document()->find_step_import_source(id)
         : session.project()     ? session.project()->find_step_import_source(id)
                                 : nullptr;
}
const ImportedTopologyCatalog* topology_catalog(const GuiDocumentSession& session,
                                                ImportedTopologyCatalogId id) {
  return session.part_document() ? session.part_document()->find_imported_topology_catalog(id)
         : session.project()     ? session.project()->find_imported_topology_catalog(id)
                                 : nullptr;
}
struct OwnedSource {
  DocumentId owner;
  StepImportSource source;
  bool embedded_part;
};
std::vector<OwnedSource> sources(const GuiDocumentSession& session) {
  std::vector<OwnedSource> values;
  const auto append = [&values](const auto& owner, bool embedded_part = false) {
    for (const auto& item : owner.step_import_sources())
      values.push_back({owner.id(), item, embedded_part});
  };
  if (const auto* part = session.part_document())
    append(*part);
  if (const auto* project = session.project()) {
    append(*project);
    for (const auto& part : project->part_documents())
      append(part, true);
  }
  return values;
}
bool same_file(const std::filesystem::path& first, const std::filesystem::path& second) {
  std::error_code error;
  const auto absolute_first = std::filesystem::absolute(first, error).lexically_normal();
  if (error)
    return false;
  const auto absolute_second = std::filesystem::absolute(second, error).lexically_normal();
  if (error)
    return false;
  if (absolute_first == absolute_second)
    return true;
  if (std::filesystem::equivalent(absolute_first, absolute_second, error) && !error)
    return true;
  error.clear();
  const auto a = std::filesystem::weakly_canonical(absolute_first, error);
  if (error)
    return false;
  const auto b = std::filesystem::weakly_canonical(absolute_second, error);
  return !error && a == b;
}
std::vector<std::filesystem::path> save_files(const GuiDocumentSession& session,
                                              const std::filesystem::path& path) {
  std::vector<std::filesystem::path> values{path};
  if (session.part_document()) {
    values.emplace_back(path.string() + ".sketch-constraints.json");
    values.emplace_back(path.string() + ".sketch-dimensions.json");
  }
  return values;
}
Result<GuiStepImportPreview> prepare_existing(const GuiDocumentSession& session,
                                              StepImportSourceId id,
                                              GuiStepImportOperation operation) {
  const auto path = document_path(session);
  if (!path)
    return Result<GuiStepImportPreview>::failure(path.error());
  const auto* current = source(session, id);
  if (!current)
    return Result<GuiStepImportPreview>::failure(invalid("STEP source does not exist"));
  auto report = inspect_step_import_source(*current, path.value().parent_path());
  if (!report)
    return Result<GuiStepImportPreview>::failure(report.error());
  auto candidate = *current;
  if (operation == GuiStepImportOperation::Refresh) {
    if (!report.value().observed_sha256_digest())
      return Result<GuiStepImportPreview>::failure(invalid(report.value().diagnostic()));
    auto updated = current->with_sha256_digest(*report.value().observed_sha256_digest());
    if (!updated)
      return Result<GuiStepImportPreview>::failure(updated.error());
    candidate = std::move(updated.value());
  }
  auto json = snapshot(session);
  if (!json)
    return Result<GuiStepImportPreview>::failure(json.error());
  return Result<GuiStepImportPreview>::success(
      {operation, std::move(candidate), std::move(report.value()), path.value(), json.value()});
}

Result<std::size_t> validate_new_topology_product(const GuiDocumentSession& session,
                                                  const geometry::StepImportGeometry& imported,
                                                  std::size_t selected,
                                                  const ImportedTopologyCatalog& candidate) {
  const auto rejected = [] {
    return Result<std::size_t>::failure(
        invalid("Selected STEP product already has a topology catalog or indistinguishable "
                "capture evidence; inspect the saved catalog"));
  };
  if (selected >= imported.products.size())
    return rejected();
  // This is a disposable evidence query, never a different accepted source or persistent product.
  const auto scoped_product = [&](std::size_t index) {
    return geometry::StepImportGeometry{
        imported.source, imported.units, {imported.products[index]}, {}, {}};
  };
  std::vector<std::optional<std::string>> product_keys(imported.products.size());
  product_keys[selected] = candidate.product_key();
  const geometry::StepImportTopology adapter;
  const auto& catalogs = session.part_document()
                             ? session.part_document()->imported_topology_catalogs()
                             : session.project()->imported_topology_catalogs();
  for (const auto& accepted : catalogs) {
    if (accepted.source() != imported.source.id())
      continue;
    auto current = adapter.recover(accepted, imported);
    if (!current)
      return Result<std::size_t>::failure(current.error());
    bool ambiguous = false, resolved_elsewhere = false;
    for (const auto& saved : accepted.entries()) {
      if (saved.descriptor.kind != ImportedTopologyKind::Body)
        continue;
      const auto* entry = current.value().find(saved.id);
      if (!entry)
        continue;
      if (entry->status == ImportedTopologyRecoveryStatus::Resolved) {
        if (entry->product_index == selected)
          return rejected();
        resolved_elsewhere = true;
      }
      ambiguous = ambiguous || entry->status == ImportedTopologyRecoveryStatus::Ambiguous;
    }
    if (accepted.product_key() == candidate.product_key())
      return rejected();
    if (!ambiguous || resolved_elsewhere)
      continue;
    // Full-source recovery first restricts ownership to exact baseline product witnesses. A
    // selected-only fallback must not invent ownership that this global rule already excludes.
    bool exact_product_elsewhere = false;
    for (std::size_t index = 0; index < imported.products.size(); ++index) {
      if (index == selected || imported.products[index].is_assembly ||
          imported.products[index].bodies.empty())
        continue;
      if (!product_keys[index]) {
        auto probe = adapter.create_catalog(candidate.id(), scoped_product(index), 0);
        if (!probe)
          return Result<std::size_t>::failure(probe.error());
        product_keys[index] = probe.value().product_key();
      }
      if (*product_keys[index] == accepted.product_key()) {
        exact_product_elsewhere = true;
        break;
      }
    }
    if (exact_product_elsewhere)
      continue;
    auto scoped = adapter.recover(accepted, scoped_product(selected));
    if (!scoped)
      return Result<std::size_t>::failure(scoped.error());
    for (const auto& saved : accepted.entries()) {
      if (saved.descriptor.kind != ImportedTopologyKind::Body)
        continue;
      const auto* entry = scoped.value().find(saved.id);
      if (entry && entry->status != ImportedTopologyRecoveryStatus::Lost)
        return rejected();
    }
  }
  return Result<std::size_t>::success(1);
}

Result<GuiStepImportGeometryPreview>
inspect_geometry_source(const GuiDocumentSession& session, StepImportSource item, bool registered) {
  auto path = document_path(session);
  if (!path)
    return Result<GuiStepImportGeometryPreview>::failure(path.error());
  auto json = snapshot(session);
  if (!json)
    return Result<GuiStepImportGeometryPreview>::failure(json.error());
  auto report = inspect_step_import_source(item, path.value().parent_path());
  if (!report)
    return Result<GuiStepImportGeometryPreview>::failure(report.error());
  if (report.value().state() != StepImportFreshness::Available)
    return Result<GuiStepImportGeometryPreview>::failure(invalid(report.value().diagnostic()));
  const geometry::StepImportReader reader;
  auto imported = reader.read(item, path.value().parent_path());
  if (!imported)
    return Result<GuiStepImportGeometryPreview>::failure(imported.error());
  auto shapes = reader.preview_shapes(imported.value());
  if (!shapes)
    return Result<GuiStepImportGeometryPreview>::failure(shapes.error());
  GuiStepImportGeometryPreview result{std::move(item),
                                      std::move(report.value()),
                                      path.value(),
                                      json.value(),
                                      registered,
                                      std::move(imported.value()),
                                      std::move(shapes.value())};
  auto valid = GuiStepImportWorkbench{}.validate_inspection(session, result);
  if (!valid)
    return Result<GuiStepImportGeometryPreview>::failure(valid.error());
  return Result<GuiStepImportGeometryPreview>::success(std::move(result));
}
} // namespace

Result<GuiStepImportPreview>
GuiStepImportWorkbench::prepare_register(const GuiDocumentSession& session, std::string name,
                                         std::string relative_path, StepImportMode mode) const {
  const auto path = document_path(session);
  if (!path)
    return Result<GuiStepImportPreview>::failure(path.error());
  const auto asset = inspect_step_import_asset(path.value().parent_path(), relative_path);
  if (!asset)
    return Result<GuiStepImportPreview>::failure(asset.error());
  if (!asset.value().observed_sha256_digest())
    return Result<GuiStepImportPreview>::failure(invalid(asset.value().diagnostic()));
  const auto absolute_asset =
      path.value().parent_path() / asset.value().project_relative_asset_path();
  for (const auto& output : save_files(session, path.value()))
    if (same_file(absolute_asset, output))
      return Result<GuiStepImportPreview>::failure(
          invalid("A STEP source must not alias its document or Sketch sidecars"));
  std::size_t next = 1;
  while (source(session, StepImportSourceId("step.source.gui." + std::to_string(next))))
    ++next;
  auto candidate = StepImportSource::create(
      StepImportSourceId("step.source.gui." + std::to_string(next)), std::move(name),
      asset.value().project_relative_asset_path(), *asset.value().observed_sha256_digest(), mode);
  if (!candidate)
    return Result<GuiStepImportPreview>::failure(candidate.error());
  const auto validate_registry = [&]() -> Result<std::size_t> {
    if (const auto* part = session.part_document()) {
      auto copy = *part;
      return copy.add_step_import_source(candidate.value());
    }
    auto copy = *session.project();
    return copy.add_step_import_source(candidate.value());
  };
  auto valid = validate_registry();
  if (!valid)
    return Result<GuiStepImportPreview>::failure(valid.error());
  auto report = inspect_step_import_source(candidate.value(), path.value().parent_path());
  if (!report)
    return Result<GuiStepImportPreview>::failure(report.error());
  if (report.value().state() != StepImportFreshness::Available)
    return Result<GuiStepImportPreview>::failure(
        invalid("Source changed during inspection; inspect again"));
  auto json = snapshot(session);
  if (!json)
    return Result<GuiStepImportPreview>::failure(json.error());
  return Result<GuiStepImportPreview>::success(
      {GuiStepImportOperation::Register, std::move(candidate.value()), std::move(report.value()),
       path.value(), json.value()});
}
Result<GuiStepImportPreview>
GuiStepImportWorkbench::prepare_refresh(const GuiDocumentSession& session,
                                        StepImportSourceId id) const {
  return prepare_existing(session, std::move(id), GuiStepImportOperation::Refresh);
}
Result<GuiStepImportPreview>
GuiStepImportWorkbench::prepare_remove(const GuiDocumentSession& session,
                                       StepImportSourceId id) const {
  return prepare_existing(session, std::move(id), GuiStepImportOperation::Remove);
}
Result<StepImportFreshnessReport> GuiStepImportWorkbench::inspect(const GuiDocumentSession& session,
                                                                  StepImportSourceId id) const {
  const auto path = document_path(session);
  if (!path)
    return Result<StepImportFreshnessReport>::failure(path.error());
  const auto* current = source(session, std::move(id));
  if (!current)
    return Result<StepImportFreshnessReport>::failure(invalid("STEP source does not exist"));
  return inspect_step_import_source(*current, path.value().parent_path());
}
Result<std::vector<GuiStepImportSourceStatus>>
GuiStepImportWorkbench::inspect_sources(const GuiDocumentSession& session) const {
  const auto values = sources(session);
  if (values.empty())
    return Result<std::vector<GuiStepImportSourceStatus>>::success({});
  const auto path = document_path(session);
  if (!path)
    return Result<std::vector<GuiStepImportSourceStatus>>::failure(path.error());
  std::vector<GuiStepImportSourceStatus> reports;
  for (const auto& [owner, current, embedded_part] : values) {
    auto report = inspect_step_import_source(current, path.value().parent_path());
    if (!report)
      return Result<std::vector<GuiStepImportSourceStatus>>::failure(report.error());
    reports.push_back({owner, std::move(report.value()), embedded_part});
  }
  return Result<std::vector<GuiStepImportSourceStatus>>::success(std::move(reports));
}
Result<GuiStepImportGeometryPreview>
GuiStepImportWorkbench::inspect_geometry(const GuiDocumentSession& session,
                                         StepImportSourceId id) const {
  const auto* current = source(session, std::move(id));
  if (!current)
    return Result<GuiStepImportGeometryPreview>::failure(invalid("STEP source does not exist"));
  return inspect_geometry_source(session, *current, true);
}
Result<GuiStepImportGeometryPreview>
GuiStepImportWorkbench::inspect_geometry(const GuiDocumentSession& session, std::string name,
                                         std::string relative_path, StepImportMode mode) const {
  auto draft = prepare_register(session, std::move(name), std::move(relative_path), mode);
  if (!draft)
    return Result<GuiStepImportGeometryPreview>::failure(draft.error());
  return inspect_geometry_source(session, draft.value().source, false);
}
Result<std::size_t>
GuiStepImportWorkbench::validate_inspection(const GuiDocumentSession& session,
                                            const GuiStepImportGeometryPreview& preview) const {
  const auto path = document_path(session);
  const auto json = snapshot(session);
  if (!path || !json || path.value() != preview.document_path ||
      json.value() != preview.document_json ||
      preview.report.root_directory() != path.value().parent_path() ||
      preview.report.source() != preview.source || preview.geometry.source != preview.source ||
      preview.report.state() != StepImportFreshness::Available)
    return Result<std::size_t>::failure(
        invalid("Document or STEP inspection changed; inspect again"));
  if (preview.registered_source) {
    const auto* accepted = source(session, preview.source.id());
    if (!accepted || *accepted != preview.source)
      return Result<std::size_t>::failure(invalid("STEP source intent changed; inspect again"));
  }
  const auto report = inspect_step_import_source(preview.source, path.value().parent_path());
  if (!report || report.value().state() != StepImportFreshness::Available ||
      report.value().observed_sha256_digest() != preview.report.observed_sha256_digest())
    return Result<std::size_t>::failure(
        invalid("STEP source changed or became unavailable; inspect again"));
  return Result<std::size_t>::success(preview.preview_shapes.size());
}
Result<GuiStepImportTopologyPreview>
GuiStepImportWorkbench::prepare_topology(const GuiDocumentSession& session, StepImportSourceId id,
                                         std::size_t product_index) const {
  auto inspection = inspect_geometry(session, std::move(id));
  if (!inspection)
    return Result<GuiStepImportTopologyPreview>::failure(inspection.error());
  std::size_t next = 1;
  while (topology_catalog(session,
                          ImportedTopologyCatalogId("step.catalog.gui." + std::to_string(next))))
    ++next;
  const geometry::StepImportTopology adapter;
  auto catalog =
      adapter.create_catalog(ImportedTopologyCatalogId("step.catalog.gui." + std::to_string(next)),
                             inspection.value().geometry, product_index);
  if (!catalog)
    return Result<GuiStepImportTopologyPreview>::failure(catalog.error());
  auto duplicate = validate_new_topology_product(session, inspection.value().geometry,
                                                 product_index, catalog.value());
  if (!duplicate)
    return Result<GuiStepImportTopologyPreview>::failure(duplicate.error());
  auto valid =
      session.part_document()
          ? PartDocument(*session.part_document()).add_imported_topology_catalog(catalog.value())
          : Project(*session.project()).add_imported_topology_catalog(catalog.value());
  if (!valid)
    return Result<GuiStepImportTopologyPreview>::failure(valid.error());
  auto recovery = adapter.recover(catalog.value(), inspection.value().geometry);
  if (!recovery)
    return Result<GuiStepImportTopologyPreview>::failure(recovery.error());
  return Result<GuiStepImportTopologyPreview>::success(
      {std::move(inspection.value()), std::move(catalog.value()), std::move(recovery.value()),
       false, product_index});
}
Result<GuiStepImportTopologyPreview>
GuiStepImportWorkbench::inspect_topology(const GuiDocumentSession& session,
                                         ImportedTopologyCatalogId id) const {
  const auto* catalog = topology_catalog(session, std::move(id));
  if (!catalog)
    return Result<GuiStepImportTopologyPreview>::failure(
        invalid("Imported topology catalog does not exist"));
  auto inspection = inspect_geometry(session, catalog->source());
  if (!inspection)
    return Result<GuiStepImportTopologyPreview>::failure(inspection.error());
  auto recovery = geometry::StepImportTopology{}.recover(*catalog, inspection.value().geometry);
  if (!recovery)
    return Result<GuiStepImportTopologyPreview>::failure(recovery.error());
  return Result<GuiStepImportTopologyPreview>::success(
      {std::move(inspection.value()), *catalog, std::move(recovery.value()), true, std::nullopt});
}
Result<std::size_t>
GuiStepImportWorkbench::validate_topology(const GuiDocumentSession& session,
                                          const GuiStepImportTopologyPreview& preview) const {
  auto valid = validate_inspection(session, preview.inspection);
  if (!valid)
    return valid;
  if (preview.catalog.source() != preview.inspection.source.id() ||
      preview.recovery.catalog != preview.catalog.id() ||
      preview.recovery.source != preview.catalog.source() ||
      preview.recovery.current_sha256_digest != preview.inspection.source.sha256_digest())
    return Result<std::size_t>::failure(
        invalid("Imported topology evidence does not match the source"));
  const auto* current = topology_catalog(session, preview.catalog.id());
  if (preview.accepted_catalog ? !current || *current != preview.catalog : current != nullptr)
    return Result<std::size_t>::failure(
        invalid("Imported topology catalog changed; inspect again"));
  return Result<std::size_t>::success(preview.catalog.entries().size());
}
Result<std::size_t>
GuiStepImportWorkbench::apply_topology(GuiDocumentSession& session,
                                       const GuiStepImportTopologyPreview& preview) const {
  auto valid = validate_topology(session, preview);
  if (!valid)
    return valid;
  if (preview.accepted_catalog)
    return Result<std::size_t>::success(0);
  // Recreate capture from the verified current bytes and compare immutable intent;
  // editable public preview records cannot smuggle altered descriptors into Apply.
  auto inspected = inspect_geometry(session, preview.catalog.source());
  if (!inspected)
    return Result<std::size_t>::failure(inspected.error());
  if (!preview.captured_product_index)
    return Result<std::size_t>::failure(invalid("Captured STEP product is no longer available"));
  auto expected = geometry::StepImportTopology{}.create_catalog(
      preview.catalog.id(), inspected.value().geometry, *preview.captured_product_index);
  if (!expected || expected.value() != preview.catalog)
    return Result<std::size_t>::failure(
        invalid("Imported topology capture changed; prepare it again"));
  const auto mutation = [&](auto& document) -> Result<std::size_t> {
    auto duplicate = validate_new_topology_product(
        session, inspected.value().geometry, *preview.captured_product_index, expected.value());
    if (!duplicate)
      return duplicate;
    auto current = validate_topology(session, preview);
    if (!current)
      return current;
    return document.add_imported_topology_catalog(preview.catalog);
  };
  return session.part_document()
             ? session.commit_part_transaction("Capture imported topology catalog", mutation)
             : session.commit_project_transaction("Capture imported topology catalog", mutation);
}
Result<std::size_t> GuiStepImportWorkbench::apply(GuiDocumentSession& session,
                                                  const GuiStepImportPreview& preview) const {
  const auto path = document_path(session);
  const auto json = snapshot(session);
  if (!path || !json || path.value() != preview.document_path ||
      json.value() != preview.document_json)
    return Result<std::size_t>::failure(invalid("Document or asset root changed; inspect again"));
  const auto& inspected = preview.report.source();
  if (preview.report.root_directory() != path.value().parent_path())
    return Result<std::size_t>::failure(invalid("Source inspection belongs to another asset root"));
  auto current = inspect_step_import_source(inspected, path.value().parent_path());
  if (!current || current.value().state() != preview.report.state() ||
      current.value().observed_sha256_digest() != preview.report.observed_sha256_digest())
    return Result<std::size_t>::failure(
        invalid("Source bytes or availability changed; inspect again"));
  if (preview.operation == GuiStepImportOperation::Refresh && preview.source == inspected &&
      preview.report.state() == StepImportFreshness::Available) {
    const auto* accepted = source(session, inspected.id());
    if (accepted && *accepted == inspected)
      return Result<std::size_t>::success(0);
  }
  const auto mutation = [&](auto& document) -> Result<std::size_t> {
    auto evidence = inspect_step_import_source(inspected, path.value().parent_path());
    if (!evidence || evidence.value().state() != preview.report.state() ||
        evidence.value().observed_sha256_digest() != preview.report.observed_sha256_digest())
      return Result<std::size_t>::failure(invalid("Source changed before commit; inspect again"));
    switch (preview.operation) {
    case GuiStepImportOperation::Register:
      if (preview.source != inspected || preview.report.state() != StepImportFreshness::Available)
        return Result<std::size_t>::failure(invalid("Invalid registration evidence"));
      for (const auto& output : save_files(session, path.value()))
        if (same_file(path.value().parent_path() / inspected.project_relative_asset_path(), output))
          return Result<std::size_t>::failure(invalid("STEP source aliases a document output"));
      return document.add_step_import_source(preview.source);
    case GuiStepImportOperation::Refresh: {
      if (!preview.report.observed_sha256_digest())
        return Result<std::size_t>::failure(invalid("Source is unavailable"));
      auto expected = inspected.with_sha256_digest(*preview.report.observed_sha256_digest());
      if (!expected || expected.value() != preview.source)
        return Result<std::size_t>::failure(invalid("Invalid refresh evidence"));
      return refresh_step_import_source(document, path.value().parent_path(), preview.report);
    }
    case GuiStepImportOperation::Remove:
      if (preview.source != inspected)
        return Result<std::size_t>::failure(invalid("Invalid removal evidence"));
      return document.remove_step_import_source(inspected);
    }
    return Result<std::size_t>::failure(invalid("Unsupported source operation"));
  };
  const auto label = preview.operation == GuiStepImportOperation::Register  ? "Register STEP source"
                     : preview.operation == GuiStepImportOperation::Refresh ? "Refresh STEP source"
                                                                            : "Remove STEP source";
  return session.part_document() ? session.commit_part_transaction(label, mutation)
                                 : session.commit_project_transaction(label, mutation);
}

namespace {
Result<std::string> reference_candidate_json(const GuiStepReferencePreview& value) {
  return std::visit(
      [](const auto& candidate) -> Result<std::string> {
        if constexpr (requires { candidate.project; })
          return serialize_project_to_json(candidate.project);
        else
          return serialize_part_document_to_json(candidate.part);
      },
      value.candidate);
}
} // namespace
Result<GuiStepReferencePreview>
GuiStepImportWorkbench::prepare_reference(const GuiDocumentSession& session,
                                          GuiStepReferenceRequest request) const {
  auto path = document_path(session);
  auto json = snapshot(session);
  if (!path || !json)
    return Result<GuiStepReferencePreview>::failure(!path ? path.error() : json.error());
  const auto root = path.value().parent_path();
  const geometry::StepReferenceCommands commands;
  const geometry::StepEditableCommands editable;
  auto candidate = [&]() -> Result<std::variant<geometry::StepReferencePartCandidate,
                                                geometry::StepReferenceProjectCandidate>> {
    using Value =
        std::variant<geometry::StepReferencePartCandidate, geometry::StepReferenceProjectCandidate>;
    if (const auto* project = session.project()) {
      const auto result = [&]() -> Result<geometry::StepReferenceProjectCandidate> {
        switch (request.operation) {
        case GuiStepReferenceOperation::Insert: {
          const auto* catalog = project->find_imported_topology_catalog(request.catalog);
          const auto* source =
              catalog ? project->find_step_import_source(catalog->source()) : nullptr;
          if (source && source->mode() == StepImportMode::EditableBody)
            return editable.insert(*project, request.catalog, request.new_part, request.name,
                                   request.base_feature, request.target_assembly, request.instance,
                                   request.transform, root);
          return commands.insert(*project, request.catalog, request.new_part, request.name,
                                 request.target_assembly, request.instance, request.transform,
                                 root);
        }
        case GuiStepReferenceOperation::Repeat:
          return commands.repeat(*project, request.reference_part, request.target_assembly,
                                 request.instance, request.transform, root);
        case GuiStepReferenceOperation::Refresh:
          return editable.refresh(*project, request.reference_part, request.refresh_source, root);
        case GuiStepReferenceOperation::Convert:
          return commands.convert(*project, request.reference_part, request.new_part, request.name,
                                  request.base_feature, root);
        }
        return Result<geometry::StepReferenceProjectCandidate>::failure(
            invalid("Unknown Reference Part operation"));
      }();
      return result ? Result<Value>::success(Value(result.value()))
                    : Result<Value>::failure(result.error());
    }
    const auto& part = *session.part_document();
    auto result = [&]() -> Result<geometry::StepReferencePartCandidate> {
      switch (request.operation) {
      case GuiStepReferenceOperation::Insert: {
        const auto* catalog = part.find_imported_topology_catalog(request.catalog);
        const auto* source = catalog ? part.find_step_import_source(catalog->source()) : nullptr;
        if (!source)
          return Result<geometry::StepReferencePartCandidate>::failure(
              invalid("Choose a saved imported product topology catalog"));
        auto created = source->mode() == StepImportMode::EditableBody
                           ? editable.create_definition(*source, *catalog, request.new_part,
                                                        request.name, request.base_feature, root)
                           : commands.create_definition(*source, *catalog, request.new_part,
                                                        request.name, root);
        if (!created)
          return Result<geometry::StepReferencePartCandidate>::failure(created.error());
        return Result<geometry::StepReferencePartCandidate>::success(
            {std::move(created.value()),
             {},
             {source->mode() == StepImportMode::EditableBody
                  ? "Create a new Editable Part with an Imported Body base. The current Part is "
                    "preserved."
                  : "Create a new immutable Reference Part file. The current Part is preserved."}});
      }
      case GuiStepReferenceOperation::Refresh:
        return editable.refresh(part, request.refresh_source, root);
      case GuiStepReferenceOperation::Convert:
        return commands.convert(part, request.new_part, request.name, request.base_feature, root);
      case GuiStepReferenceOperation::Repeat:
        return Result<geometry::StepReferencePartCandidate>::failure(
            invalid("Occurrences require a Project"));
      }
      return Result<geometry::StepReferencePartCandidate>::failure(
          invalid("Unknown Reference Part operation"));
    }();
    return result ? Result<Value>::success(Value(result.value()))
                  : Result<Value>::failure(result.error());
  }();
  if (!candidate)
    return Result<GuiStepReferencePreview>::failure(candidate.error());
  const auto* selected = std::visit(
      [](const auto& value) -> const PartDocument* {
        if constexpr (requires { value.project; })
          return value.project.find_part_document(value.part);
        else
          return &value.part;
      },
      candidate.value());
  if (!selected)
    return Result<GuiStepReferencePreview>::failure(invalid("Prepared Reference Part is missing"));
  geometry::StepImportTopologyContext context(root);
  const geometry::ReferencePartGeometry adapter;
  std::vector<geometry::GeometryShape> shapes;
  if (selected->is_reference_part()) {
    auto resolved = adapter.resolve(*selected, context);
    if (!resolved)
      return Result<GuiStepReferencePreview>::failure(resolved.error());
    for (const auto& body : resolved.value().bodies)
      shapes.push_back(body.shape);
  } else {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.step_import.preview"));
    if (!cache)
      return Result<GuiStepReferencePreview>::failure(cache.error());
    auto result =
        geometry::GeometryRecomputeExecutor{&context}.execute_document(*selected, cache.value());
    if (!result)
      return Result<GuiStepReferencePreview>::failure(result.error());
    for (const auto& body : cache.value().body_shapes())
      shapes.push_back(body.shape);
  }
  auto valid = context.validate_sources();
  if (!valid)
    return Result<GuiStepReferencePreview>::failure(valid.error());
  if (session.project() && (request.operation == GuiStepReferenceOperation::Insert ||
                            request.operation == GuiStepReferenceOperation::Repeat)) {
    const geometry::BodyTransformAdapter transforms;
    for (auto& shape : shapes) {
      const auto& rotation = request.transform.rotation_deg;
      for (const auto& [axis, angle] : std::array<std::pair<Vector3, double>, 3>{
               std::pair{Vector3{1, 0, 0}, rotation.x}, std::pair{Vector3{0, 1, 0}, rotation.y},
               std::pair{Vector3{0, 0, 1}, rotation.z}}) {
        if (angle == 0)
          continue;
        auto rotated = transforms.rotate(shape, {}, axis, angle);
        if (!rotated)
          return Result<GuiStepReferencePreview>::failure(rotated.error());
        shape = std::move(rotated.value());
      }
      auto translated = transforms.translate(shape, request.transform.translation_mm);
      if (!translated)
        return Result<GuiStepReferencePreview>::failure(translated.error());
      shape = std::move(translated.value());
    }
  }
  return Result<GuiStepReferencePreview>::success({std::move(request), path.value(), json.value(),
                                                   std::move(candidate.value()),
                                                   std::move(shapes)});
}
Result<std::size_t>
GuiStepImportWorkbench::validate_reference(const GuiDocumentSession& session,
                                           const GuiStepReferencePreview& preview) const {
  auto path = document_path(session);
  auto json = snapshot(session);
  if (!path || !json || path.value() != preview.document_path ||
      json.value() != preview.document_json)
    return Result<std::size_t>::failure(invalid("Reference Part preview is stale; prepare again"));
  auto fresh = prepare_reference(session, preview.request);
  if (!fresh)
    return Result<std::size_t>::failure(fresh.error());
  auto expected = reference_candidate_json(fresh.value());
  auto actual = reference_candidate_json(preview);
  if (!expected || !actual || expected.value() != actual.value())
    return Result<std::size_t>::failure(
        invalid("Imported Part candidate or source changed; prepare again"));
  return Result<std::size_t>::success(preview.preview_shapes.size());
}
Result<std::size_t>
GuiStepImportWorkbench::apply_reference(GuiDocumentSession& session,
                                        const GuiStepReferencePreview& preview) const {
  auto valid = validate_reference(session, preview);
  if (!valid)
    return valid;
  if (session.part_document() && preview.request.operation != GuiStepReferenceOperation::Refresh)
    return Result<std::size_t>::failure(
        invalid("Choose a new copy destination; the current Part is preserved"));
  auto fresh = prepare_reference(session, preview.request);
  if (!fresh)
    return Result<std::size_t>::failure(fresh.error());
  auto candidate_json = reference_candidate_json(fresh.value());
  if (candidate_json && candidate_json.value() == preview.document_json)
    return Result<std::size_t>::success(0);
  const auto label = preview.request.operation == GuiStepReferenceOperation::Insert
                         ? "Insert STEP Part"
                     : preview.request.operation == GuiStepReferenceOperation::Repeat
                         ? "Insert Reference Part occurrence"
                     : preview.request.operation == GuiStepReferenceOperation::Refresh
                         ? "Refresh STEP Imported Parts"
                         : "Convert Reference to new Editable Part";
  if (const auto* project =
          std::get_if<geometry::StepReferenceProjectCandidate>(&fresh.value().candidate))
    return session.commit_project_transaction(label, [&](Project& target) {
      target = project->project;
      return Result<std::size_t>::success(1);
    });
  const auto& part = std::get<geometry::StepReferencePartCandidate>(fresh.value().candidate);
  return session.commit_part_transaction(label, [&](PartDocument& target) {
    target = part.part;
    return Result<std::size_t>::success(1);
  });
}
Result<std::size_t>
GuiStepImportWorkbench::save_editable_copy(const GuiDocumentSession& session,
                                           const GuiStepReferencePreview& preview,
                                           const std::filesystem::path& destination) const {
  auto valid = validate_reference(session, preview);
  if (!valid)
    return valid;
  if (!session.part_document() ||
      (preview.request.operation != GuiStepReferenceOperation::Convert &&
       preview.request.operation != GuiStepReferenceOperation::Insert))
    return Result<std::size_t>::failure(
        invalid("Only a new standalone Part copy can be saved here"));
  std::error_code error;
  const auto output = std::filesystem::absolute(destination, error).lexically_normal();
  if (error || output.empty() || output == preview.document_path)
    return Result<std::size_t>::failure(invalid("Choose a new Part file"));
  const auto output_root = std::filesystem::weakly_canonical(output.parent_path(), error);
  if (error)
    return Result<std::size_t>::failure(invalid("New Part directory cannot be resolved"));
  const auto original_root =
      std::filesystem::weakly_canonical(preview.document_path.parent_path(), error);
  if (error || output_root != original_root)
    return Result<std::size_t>::failure(
        invalid("Save the new copy beside the original document to preserve its STEP asset root"));
  valid = validate_step_import_save_as(session, output);
  if (!valid)
    return valid;
  for (const auto& file : save_files(session, output))
    if (std::filesystem::exists(std::filesystem::symlink_status(file, error)))
      return Result<std::size_t>::failure(
          invalid("New Part copy must not overwrite an existing file or sidecar"));
  auto json = reference_candidate_json(preview);
  if (!json)
    return Result<std::size_t>::failure(json.error());
  QFile file(QString::fromStdString(output.string()));
  if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    return Result<std::size_t>::failure(
        invalid("New Part destination already exists or cannot be created"));
  const auto count = file.write(json.value().data(), static_cast<qint64>(json.value().size()));
  if (count != static_cast<qint64>(json.value().size()) || !file.flush()) {
    file.remove();
    return Result<std::size_t>::failure(invalid("New Part copy could not be written"));
  }
  return Result<std::size_t>::success(1);
}
Result<GuiStructuredStepImportPreview>
GuiStepImportWorkbench::prepare_structured_project(const GuiDocumentSession& session,
                                                   GuiStructuredStepImportRequest request) const {
  auto path = document_path(session);
  auto json = snapshot(session);
  if (!path || !json)
    return Result<GuiStructuredStepImportPreview>::failure(!path ? path.error() : json.error());
  const auto* selected = source(session, request.source);
  if (!selected)
    return Result<GuiStructuredStepImportPreview>::failure(
        invalid("Choose a registered source owned by the current document"));
  auto candidate = geometry::StepStructuredImportCommands{}.create_project(
      *selected, request.new_project, request.name, path.value().parent_path());
  if (!candidate)
    return Result<GuiStructuredStepImportPreview>::failure(candidate.error());
  geometry::StepImportTopologyContext context(path.value().parent_path());
  auto scene = geometry::ViewportSceneBuilder{&context}.build_project(candidate.value().project);
  if (!scene)
    return Result<GuiStructuredStepImportPreview>::failure(scene.error());
  std::vector<geometry::GeometryShape> shapes;
  for (auto& item : scene.value())
    if (item.kind == geometry::ViewportSceneKind::Component)
      shapes.push_back(std::move(item.shape));
  if (shapes.empty())
    return Result<GuiStructuredStepImportPreview>::failure(
        invalid("Structured STEP project has no visible body occurrences"));
  auto valid = context.validate_sources();
  if (!valid)
    return Result<GuiStructuredStepImportPreview>::failure(valid.error());
  return Result<GuiStructuredStepImportPreview>::success(
      {std::move(request), path.value(), json.value(), *selected, std::move(candidate.value()),
       std::move(shapes)});
}

Result<std::size_t> GuiStepImportWorkbench::validate_structured_project(
    const GuiDocumentSession& session, const GuiStructuredStepImportPreview& preview) const {
  const auto path = document_path(session);
  const auto json = snapshot(session);
  if (!path || !json || path.value() != preview.document_path ||
      json.value() != preview.document_json)
    return Result<std::size_t>::failure(
        invalid("Structured Project preview is stale; prepare again"));
  auto fresh = prepare_structured_project(session, preview.request);
  if (!fresh)
    return Result<std::size_t>::failure(fresh.error());
  const auto expected = serialize_project_to_json(fresh.value().candidate.project);
  const auto actual = serialize_project_to_json(preview.candidate.project);
  if (fresh.value().source != preview.source || !expected || !actual ||
      expected.value() != actual.value())
    return Result<std::size_t>::failure(
        invalid("Structured Project candidate or source changed; prepare again"));
  return Result<std::size_t>::success(fresh.value().preview_shapes.size());
}

Result<std::size_t> GuiStepImportWorkbench::save_structured_project_copy(
    const GuiDocumentSession& session, const GuiStructuredStepImportPreview& preview,
    const std::filesystem::path& destination) const {
  auto valid = validate_structured_project(session, preview);
  if (!valid)
    return valid;
  std::error_code error;
  const auto output = std::filesystem::absolute(destination, error).lexically_normal();
  if (error || output.empty() || same_file(output, preview.document_path))
    return Result<std::size_t>::failure(invalid("Choose a new Project file"));
  const auto output_root = std::filesystem::weakly_canonical(output.parent_path(), error);
  if (error)
    return Result<std::size_t>::failure(invalid("New Project directory cannot be resolved"));
  const auto original_root =
      std::filesystem::weakly_canonical(preview.document_path.parent_path(), error);
  if (error || output_root != original_root)
    return Result<std::size_t>::failure(invalid(
        "Save the new Project beside the current document to preserve its STEP asset root"));
  valid = validate_step_import_output_path(session, output);
  if (!valid)
    return valid;
  // Preserve any existing user file, dangling symlink or standalone sidecar.
  for (const auto& file :
       std::vector<std::filesystem::path>{output, output.string() + ".sketch-constraints.json",
                                          output.string() + ".sketch-dimensions.json"}) {
    const auto status = std::filesystem::symlink_status(file, error);
    if (std::filesystem::exists(status))
      return Result<std::size_t>::failure(
          invalid("New Project copy must not overwrite an existing file or sidecar"));
    if (error && error != std::errc::no_such_file_or_directory)
      return Result<std::size_t>::failure(invalid("New Project destination cannot be inspected"));
    error.clear();
  }
  auto json = serialize_project_to_json(preview.candidate.project);
  if (!json)
    return Result<std::size_t>::failure(json.error());
  QTemporaryFile staged(
      QString::fromStdString((output_root / ".blcad-structured-project-XXXXXX").string()));
  if (!staged.open() ||
      staged.write(json.value().data(), static_cast<qint64>(json.value().size())) !=
          static_cast<qint64>(json.value().size()) ||
      !staged.flush())
    return Result<std::size_t>::failure(invalid("New Project copy could not be staged"));
  staged.close();
  // Re-read the source and compare the entire public candidate immediately
  // before publication. A hard-link publish is atomic and refuses overwrites;
  // the temporary name is removed by QTemporaryFile on every exit path.
  valid = validate_structured_project(session, preview);
  if (!valid)
    return valid;
  valid = validate_step_import_output_path(session, output);
  if (!valid)
    return valid;
  std::filesystem::create_hard_link(staged.fileName().toStdString(), output, error);
  if (error)
    return Result<std::size_t>::failure(
        invalid("New Project destination already exists or cannot be published"));
  return Result<std::size_t>::success(1);
}

Result<std::size_t> validate_step_import_output_path(const GuiDocumentSession& session,
                                                     const std::filesystem::path& output) {
  const auto values = sources(session);
  if (values.empty())
    return Result<std::size_t>::success(0);
  const auto path = document_path(session);
  if (!path)
    return Result<std::size_t>::failure(path.error());
  for (const auto& [owner, item, embedded_part] : values)
    if (same_file(path.value().parent_path() / item.project_relative_asset_path(), output))
      return Result<std::size_t>::failure(invalid("Output must not overwrite STEP source " +
                                                  owner.value() + "/" + item.id().value()));
  return Result<std::size_t>::success(values.size());
}
Result<std::size_t> validate_step_import_save_as(const GuiDocumentSession& session,
                                                 const std::filesystem::path& destination) {
  const auto values = sources(session);
  if (values.empty())
    return Result<std::size_t>::success(0);
  const auto old_path = document_path(session);
  if (!old_path)
    return Result<std::size_t>::failure(old_path.error());
  std::error_code error;
  const auto new_path = std::filesystem::absolute(destination, error).lexically_normal();
  if (error)
    return Result<std::size_t>::failure(invalid("Save destination cannot be resolved"));
  for (const auto& output : save_files(session, new_path)) {
    auto safe = validate_step_import_output_path(session, output);
    if (!safe)
      return safe;
    for (const auto& [owner, item, embedded_part] : values)
      if (same_file(new_path.parent_path() / item.project_relative_asset_path(), output))
        return Result<std::size_t>::failure(invalid("Save destination aliases a STEP asset"));
  }
  if (new_path.parent_path() != old_path.value().parent_path())
    for (const auto& [owner, item, embedded_part] : values) {
      auto report = inspect_step_import_source(item, new_path.parent_path());
      if (!report || report.value().state() != StepImportFreshness::Available)
        return Result<std::size_t>::failure(invalid(
            "Save As requires matching STEP assets at the new relative location: " + owner.value() +
            "/" + item.project_relative_asset_path()));
    }
  return Result<std::size_t>::success(values.size());
}
Result<std::size_t> validate_step_import_definition_sources(const GuiDocumentSession& session,
                                                            const PartDocument& definition) {
  if (definition.step_import_sources().empty())
    return Result<std::size_t>::success(0);
  const auto path = document_path(session);
  if (!path)
    return Result<std::size_t>::failure(path.error());
  for (const auto& item : definition.step_import_sources()) {
    auto report = inspect_step_import_source(item, path.value().parent_path());
    if (!report || report.value().state() != StepImportFreshness::Available)
      return Result<std::size_t>::failure(invalid(
          "Definition import requires matching STEP assets in the target document directory"));
    for (const auto& output : save_files(session, path.value()))
      if (same_file(path.value().parent_path() / item.project_relative_asset_path(), output))
        return Result<std::size_t>::failure(
            invalid("Definition source aliases the target document"));
  }
  return Result<std::size_t>::success(definition.step_import_sources().size());
}
} // namespace blcad::gui
