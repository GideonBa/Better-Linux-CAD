#include "blcad/gui/gui_step_import_workbench.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

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
