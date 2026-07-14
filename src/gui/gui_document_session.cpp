#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <utility>

namespace blcad::gui {
namespace {

Error session_error(std::string message) {
  return Error::validation("gui.document_session", std::move(message));
}

bool is_project_path(const std::filesystem::path& path) {
  return path.filename().string().ends_with(".blcad.project.json");
}

} // namespace

Result<std::size_t> GuiDocumentSession::create_part(DocumentId id, std::string name,
                                                    bool discard_dirty) {
  if (const auto allowed = reject_replacement_if_needed(discard_dirty); allowed.has_error())
    return allowed;
  auto created = PartDocument::create(std::move(id), std::move(name));
  if (created.has_error()) {
    record_error(created.error());
    return Result<std::size_t>::failure(created.error());
  }
  document_ = std::move(created.value());
  display_name_ = std::get<PartDocument>(document_).name();
  file_path_.clear();
  workspace_ = GuiWorkspace::Part;
  auto recomputed = recompute();
  if (recomputed.has_error())
    return recomputed;
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  initialize_history(json.value(), "New Part", false);
  return Result<std::size_t>::success(1);
}

Result<std::size_t> GuiDocumentSession::create_project(DocumentId id, std::string name,
                                                       bool discard_dirty) {
  if (const auto allowed = reject_replacement_if_needed(discard_dirty); allowed.has_error())
    return allowed;
  const auto assembly_id = DocumentId(id.value() + ".assembly");
  auto assembly = AssemblyDocument::create(assembly_id, name + " Assembly");
  if (assembly.has_error()) {
    record_error(assembly.error());
    return Result<std::size_t>::failure(assembly.error());
  }
  auto created = Project::create(std::move(id), std::move(name), std::move(assembly.value()));
  if (created.has_error()) {
    record_error(created.error());
    return Result<std::size_t>::failure(created.error());
  }
  document_ = std::move(created.value());
  display_name_ = std::get<Project>(document_).name();
  file_path_.clear();
  workspace_ = GuiWorkspace::Assembly;
  auto recomputed = recompute();
  if (recomputed.has_error())
    return recomputed;
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  initialize_history(json.value(), "New Project", false);
  return Result<std::size_t>::success(1);
}

Result<std::size_t> GuiDocumentSession::open_file(const std::filesystem::path& path,
                                                  bool discard_dirty) {
  if (const auto allowed = reject_replacement_if_needed(discard_dirty); allowed.has_error())
    return allowed;
  if (path.empty())
    return Result<std::size_t>::failure(session_error("document path must not be empty"));

  Document candidate;
  if (is_project_path(path)) {
    auto read = read_project_json_file(path);
    if (read.has_error()) {
      record_error(read.error());
      return Result<std::size_t>::failure(read.error());
    }
    candidate = std::move(read.value());
  } else {
    auto read = read_part_document_json_file(path);
    if (read.has_error()) {
      record_error(read.error());
      return Result<std::size_t>::failure(read.error());
    }
    candidate = std::move(read.value());
  }

  std::optional<geometry::ShapeCache> part_cache;
  std::vector<ProjectPartCache> project_caches;
  if (auto* part = std::get_if<PartDocument>(&candidate)) {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.part." + part->id().value()));
    if (cache.has_error())
      return Result<std::size_t>::failure(cache.error());
    auto result = recompute_part(*part, cache.value());
    if (result.has_error()) {
      record_error(result.error());
      return result;
    }
    part->mark_all_clean();
    part_cache = std::move(cache.value());
  } else {
    auto* active_project = std::get_if<Project>(&candidate);
    auto caches = recompute_project(*active_project);
    if (caches.has_error()) {
      record_error(caches.error());
      return Result<std::size_t>::failure(caches.error());
    }
    for (auto& part : active_project->part_documents())
      part.mark_all_clean();
    project_caches = std::move(caches.value());
  }

  document_ = std::move(candidate);
  file_path_ = path;
  part_shape_cache_ = std::move(part_cache);
  project_part_caches_ = std::move(project_caches);
  ++presentation_revision_;
  recompute_status_ = GuiRecomputeStatus::Fresh;
  if (auto* part = part_document()) {
    display_name_ = part->name();
    workspace_ = GuiWorkspace::Part;
  } else {
    display_name_ = project()->name();
    workspace_ = GuiWorkspace::Assembly;
  }
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  initialize_history(json.value(), "Open", true);
  record_information(path.string(), "document opened");
  return Result<std::size_t>::success(1);
}

Result<std::uintmax_t> GuiDocumentSession::save() {
  if (file_path_.empty()) {
    const auto error = session_error("save requires a document path; use save_as");
    record_error(error);
    return Result<std::uintmax_t>::failure(error);
  }
  return save_as(file_path_);
}

Result<std::uintmax_t> GuiDocumentSession::save_as(const std::filesystem::path& path) {
  if (!has_document()) {
    const auto error = session_error("save requires an active document");
    record_error(error);
    return Result<std::uintmax_t>::failure(error);
  }
  Result<std::uintmax_t> written = part_document()
                                       ? write_part_document_json_file(*part_document(), path)
                                       : write_project_json_file(*project(), path);
  if (written.has_error()) {
    record_error(written.error());
    return written;
  }
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::uintmax_t>::failure(json.error());
  file_path_ = path;
  saved_json_ = json.value();
  record_information(path.string(), "document saved");
  return written;
}

Result<std::size_t> GuiDocumentSession::recompute() {
  if (auto* part = part_document()) {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.part." + part->id().value()));
    if (cache.has_error()) {
      record_error(cache.error());
      return Result<std::size_t>::failure(cache.error());
    }
    const auto result = recompute_part(*part, cache.value());
    if (result.has_error()) {
      recompute_status_ = GuiRecomputeStatus::Failed;
      record_error(result.error());
      return result;
    }
    part_shape_cache_ = std::move(cache.value());
    project_part_caches_.clear();
    recompute_status_ = GuiRecomputeStatus::Fresh;
    part->mark_all_clean();
    ++presentation_revision_;
    return result;
  }
  if (auto* active_project = project()) {
    auto caches = recompute_project(*active_project);
    if (caches.has_error()) {
      recompute_status_ = GuiRecomputeStatus::Failed;
      record_error(caches.error());
      return Result<std::size_t>::failure(caches.error());
    }
    std::size_t executed = 0;
    for (const auto& cache : caches.value())
      executed += cache.cache.feature_shape_count();
    project_part_caches_ = std::move(caches.value());
    part_shape_cache_.reset();
    for (auto& part : active_project->part_documents())
      part.mark_all_clean();
    recompute_status_ = GuiRecomputeStatus::Fresh;
    ++presentation_revision_;
    return Result<std::size_t>::success(executed);
  }
  const auto error = session_error("recompute requires an active document");
  record_error(error);
  return Result<std::size_t>::failure(error);
}

Result<std::size_t> GuiDocumentSession::commit_part_transaction(std::string label,
                                                                const GuiPartMutation& mutation) {
  if (!part_document() || !mutation || label.empty())
    return Result<std::size_t>::failure(session_error("invalid Part transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_part_document_from_json(serialized.value());
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto changed = mutation(cloned.value());
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  auto cache =
      geometry::ShapeCache::create(ShapeCacheId("gui.part." + cloned.value().id().value()));
  if (cache.has_error())
    return Result<std::size_t>::failure(cache.error());
  auto executed = recompute_part(cloned.value(), cache.value());
  if (executed.has_error()) {
    recompute_status_ = GuiRecomputeStatus::Failed;
    record_error(executed.error());
    return executed;
  }
  cloned.value().mark_all_clean();
  document_ = std::move(cloned.value());
  part_shape_cache_ = std::move(cache.value());
  recompute_status_ = GuiRecomputeStatus::Fresh;
  ++presentation_revision_;
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  append_history(json.value(), std::move(label));
  selection_.clear();
  return changed;
}

Result<std::size_t>
GuiDocumentSession::commit_project_transaction(std::string label,
                                               const GuiProjectMutation& mutation) {
  if (!project() || !mutation || label.empty())
    return Result<std::size_t>::failure(session_error("invalid Project transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_project_from_json(serialized.value());
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto changed = mutation(cloned.value());
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  auto caches = recompute_project(cloned.value());
  if (caches.has_error()) {
    recompute_status_ = GuiRecomputeStatus::Failed;
    record_error(caches.error());
    return Result<std::size_t>::failure(caches.error());
  }
  for (auto& part : cloned.value().part_documents())
    part.mark_all_clean();
  document_ = std::move(cloned.value());
  project_part_caches_ = std::move(caches.value());
  recompute_status_ = GuiRecomputeStatus::Fresh;
  ++presentation_revision_;
  const auto json = serialize_current();
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  append_history(json.value(), std::move(label));
  selection_.clear();
  return changed;
}

Result<std::size_t> GuiDocumentSession::undo() {
  if (!can_undo())
    return Result<std::size_t>::failure(session_error("nothing to undo"));
  return restore_history(history_index_ - 1);
}

Result<std::size_t> GuiDocumentSession::redo() {
  if (!can_redo())
    return Result<std::size_t>::failure(session_error("nothing to redo"));
  return restore_history(history_index_ + 1);
}

bool GuiDocumentSession::close(bool discard_dirty) noexcept {
  if (task_.active() || (dirty() && !discard_dirty))
    return false;
  reset_document();
  return true;
}

bool GuiDocumentSession::set_workspace(GuiWorkspace workspace) noexcept {
  if (task_.active())
    return false;
  workspace_ = workspace;
  return true;
}

void GuiDocumentSession::set_derived_results_fresh(bool fresh) noexcept {
  recompute_status_ = fresh ? GuiRecomputeStatus::Fresh : GuiRecomputeStatus::Stale;
}

void GuiDocumentSession::clear_diagnostics() noexcept {
  diagnostics_.clear();
}

GuiDocumentKind GuiDocumentSession::document_kind() const noexcept {
  if (std::holds_alternative<PartDocument>(document_))
    return GuiDocumentKind::Part;
  if (std::holds_alternative<Project>(document_))
    return GuiDocumentKind::Project;
  return GuiDocumentKind::None;
}

std::string_view GuiDocumentSession::display_name() const noexcept {
  return display_name_;
}
const std::filesystem::path& GuiDocumentSession::file_path() const noexcept {
  return file_path_;
}
GuiWorkspace GuiDocumentSession::workspace() const noexcept {
  return workspace_;
}
bool GuiDocumentSession::has_document() const noexcept {
  return document_kind() != GuiDocumentKind::None;
}

bool GuiDocumentSession::dirty() const noexcept {
  if (!has_document() || history_.empty())
    return false;
  return !saved_json_.has_value() || history_[history_index_].json != *saved_json_;
}

bool GuiDocumentSession::derived_results_fresh() const noexcept {
  return recompute_status_ == GuiRecomputeStatus::Fresh;
}
GuiRecomputeStatus GuiDocumentSession::recompute_status() const noexcept {
  return recompute_status_;
}
bool GuiDocumentSession::can_undo() const noexcept {
  return history_index_ > 0 && !task_.active();
}
bool GuiDocumentSession::can_redo() const noexcept {
  return !history_.empty() && history_index_ + 1 < history_.size() && !task_.active();
}
std::string_view GuiDocumentSession::undo_label() const noexcept {
  return can_undo() ? history_[history_index_].label : std::string_view{};
}
std::string_view GuiDocumentSession::redo_label() const noexcept {
  return can_redo() ? history_[history_index_ + 1].label : std::string_view{};
}

GuiCommandContext GuiDocumentSession::command_context() const noexcept {
  return GuiCommandContext{document_kind(),        workspace_,
                           task_.stage(),          selection_.size(),
                           selection_.kind_mask(), derived_results_fresh()};
}

PartDocument* GuiDocumentSession::part_document() noexcept {
  return std::get_if<PartDocument>(&document_);
}
const PartDocument* GuiDocumentSession::part_document() const noexcept {
  return std::get_if<PartDocument>(&document_);
}
Project* GuiDocumentSession::project() noexcept {
  return std::get_if<Project>(&document_);
}
const Project* GuiDocumentSession::project() const noexcept {
  return std::get_if<Project>(&document_);
}
const geometry::ShapeCache* GuiDocumentSession::part_shape_cache() const noexcept {
  return part_shape_cache_ ? &*part_shape_cache_ : nullptr;
}
const std::vector<GuiDiagnostic>& GuiDocumentSession::diagnostics() const noexcept {
  return diagnostics_;
}
std::size_t GuiDocumentSession::presentation_revision() const noexcept {
  return presentation_revision_;
}
GuiSelectionModel& GuiDocumentSession::selection() noexcept {
  return selection_;
}
const GuiSelectionModel& GuiDocumentSession::selection() const noexcept {
  return selection_;
}
GuiTaskState& GuiDocumentSession::task() noexcept {
  return task_;
}
const GuiTaskState& GuiDocumentSession::task() const noexcept {
  return task_;
}

Result<std::string> GuiDocumentSession::serialize_current() const {
  if (const auto* part = part_document())
    return serialize_part_document_to_json(*part);
  if (const auto* active_project = project())
    return serialize_project_to_json(*active_project);
  return Result<std::string>::failure(session_error("no active document"));
}

Result<std::size_t> GuiDocumentSession::recompute_part(PartDocument& document,
                                                       geometry::ShapeCache& cache) const {
  const auto summary = geometry::GeometryRecomputeExecutor{}.execute_document(document, cache);
  if (summary.has_error())
    return Result<std::size_t>::failure(summary.error());
  return Result<std::size_t>::success(summary.value().executed_feature_count);
}

Result<std::vector<GuiDocumentSession::ProjectPartCache>>
GuiDocumentSession::recompute_project(Project& active_project) const {
  std::vector<ProjectPartCache> caches;
  caches.reserve(active_project.part_document_count());
  for (auto& part : active_project.part_documents()) {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.project." + part.id().value()));
    if (cache.has_error())
      return Result<std::vector<ProjectPartCache>>::failure(cache.error());
    const auto result = recompute_part(part, cache.value());
    if (result.has_error())
      return Result<std::vector<ProjectPartCache>>::failure(result.error());
    caches.push_back(ProjectPartCache{part.id(), std::move(cache.value())});
  }
  return Result<std::vector<ProjectPartCache>>::success(std::move(caches));
}

Result<std::size_t> GuiDocumentSession::restore_history(std::size_t index) {
  const auto& entry = history_[index];
  Document restored;
  if (entry.kind == GuiDocumentKind::Part) {
    auto part = deserialize_part_document_from_json(entry.json);
    if (part.has_error())
      return Result<std::size_t>::failure(part.error());
    restored = std::move(part.value());
  } else {
    auto active_project = deserialize_project_from_json(entry.json);
    if (active_project.has_error())
      return Result<std::size_t>::failure(active_project.error());
    restored = std::move(active_project.value());
  }
  Document previous = std::move(document_);
  document_ = std::move(restored);
  const auto result = recompute();
  if (result.has_error()) {
    document_ = std::move(previous);
    return result;
  }
  history_index_ = index;
  selection_.clear();
  return Result<std::size_t>::success(1);
}

Result<std::size_t> GuiDocumentSession::reject_replacement_if_needed(bool discard_dirty) const {
  if (task_.active())
    return Result<std::size_t>::failure(session_error("active task must be applied or cancelled"));
  if (dirty() && !discard_dirty)
    return Result<std::size_t>::failure(session_error("dirty document requires save or discard"));
  return Result<std::size_t>::success(0);
}

void GuiDocumentSession::initialize_history(std::string json, std::string label, bool saved) {
  history_ = {{document_kind(), std::move(json), std::move(label)}};
  history_index_ = 0;
  saved_json_ = saved ? std::optional<std::string>(history_.front().json) : std::nullopt;
  selection_.clear();
}

void GuiDocumentSession::append_history(std::string json, std::string label) {
  history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1),
                 history_.end());
  history_.push_back({document_kind(), std::move(json), std::move(label)});
  history_index_ = history_.size() - 1;
}

void GuiDocumentSession::record_error(const Error& error) {
  diagnostics_.push_back(
      {GuiDiagnosticSeverity::Error, error.category(), error.object_id(), error.message()});
}

void GuiDocumentSession::record_information(std::string object_id, std::string message) {
  diagnostics_.push_back({GuiDiagnosticSeverity::Information, ErrorCategory::Internal,
                          std::move(object_id), std::move(message)});
}

void GuiDocumentSession::reset_document() noexcept {
  ++presentation_revision_;
  document_ = std::monostate{};
  display_name_.clear();
  file_path_.clear();
  workspace_ = GuiWorkspace::Part;
  recompute_status_ = GuiRecomputeStatus::Unavailable;
  part_shape_cache_.reset();
  project_part_caches_.clear();
  history_.clear();
  history_index_ = 0;
  saved_json_.reset();
  diagnostics_.clear();
  selection_.clear();
}

} // namespace blcad::gui
