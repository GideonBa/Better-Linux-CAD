#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_constraint_catalog_system.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/gui/gui_pending_feature_inputs.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace blcad::gui {
namespace {

Error session_error(std::string message) {
  return Error::validation("gui.document_session", std::move(message));
}

// Compare canonical intent, rather than trusting a mutation callback's dirty
// flags. Unknown fields, removals and changed existing input edges fall back
// to full execution. A Body result alias follows its new producing feature.
std::optional<std::vector<std::string>> transaction_geometry_roots(const PartDocument& before,
                                                                   const PartDocument& after,
                                                                   const std::string& before_json,
                                                                   const std::string& after_json) {
  const auto& previous_graph = before.dependency_graph();
  const auto& next_graph = after.dependency_graph();
  const std::unordered_set<std::string> previous_nodes(previous_graph.nodes().begin(),
                                                       previous_graph.nodes().end());
  for (const auto& node : previous_nodes)
    if (!next_graph.has_node(node))
      return std::nullopt;
  auto previous_edges = previous_graph.dependencies();
  auto next_edges = next_graph.dependencies();
  std::vector<std::string> roots;
  std::unordered_set<std::string> body_aliases;
  for (const auto& body : after.bodies()) {
    const auto alias = "body:" + body.id().value();
    body_aliases.insert(alias);
    const auto producers = [&](const auto& edges) {
      std::vector<std::string> result;
      for (const auto& [source, target] : edges)
        if (target == alias)
          result.push_back(source);
      std::sort(result.begin(), result.end());
      return result;
    };
    const auto old_producers = producers(previous_edges);
    const auto new_producers = producers(next_edges);
    if (old_producers != new_producers) {
      roots.push_back(alias);
      // Recompute the new final producer even when it already existed (for
      // example, reordered intent); an alias is not itself an executable node.
      roots.insert(roots.end(), new_producers.begin(), new_producers.end());
    }
  }
  std::erase_if(previous_edges,
                [&](const auto& edge) { return body_aliases.contains(edge.second); });
  std::erase_if(next_edges, [&](const auto& edge) {
    return body_aliases.contains(edge.second) || !previous_nodes.contains(edge.first) ||
           !previous_nodes.contains(edge.second);
  });
  std::sort(previous_edges.begin(), previous_edges.end());
  std::sort(next_edges.begin(), next_edges.end());
  if (previous_edges != next_edges)
    return std::nullopt;

  using Json = nlohmann::json;
  const auto previous = Json::parse(before_json, nullptr, false);
  const auto next = Json::parse(after_json, nullptr, false);
  if (!previous.is_object() || !next.is_object())
    return std::nullopt;
  const std::unordered_set<std::string> records{
      "parameters",         "datum_planes",        "datum_axes",         "construction_points",
      "construction_lines", "construction_planes", "derived_workplanes", "result_face_workplanes",
      "sketches",           "sketches_3d",         "path_curves",        "features",
      "revolve_features",   "sweep_features",      "loft_features",      "surface_features",
      "part_patterns",      "mirror_features",     "edge_treatments",    "shell_features",
      "draft_features",     "body_booleans",       "body_transforms"};
  std::unordered_set<std::string> keys;
  for (const auto& [key, value] : previous.items())
    keys.insert(key);
  for (const auto& [key, value] : next.items())
    keys.insert(key);
  for (const auto& key : keys) {
    if (previous.contains(key) && next.contains(key) && previous.at(key) == next.at(key))
      continue;
    // Core reconstructs feature ordering as dependency edges. Existing edges
    // were checked above; newly inserted producers are included below.
    if (key == "feature_order")
      continue;
    if (!records.contains(key))
      return std::nullopt;
    const auto old_records = previous.value(key, Json::array());
    const auto new_records = next.value(key, Json::array());
    if (!old_records.is_array() || !new_records.is_array())
      return std::nullopt;
    std::unordered_map<std::string, Json> old_by_id;
    for (const auto& record : old_records) {
      if (!record.is_object() || !record.contains("id") || !record.at("id").is_string() ||
          !old_by_id.emplace(record.at("id").get<std::string>(), record).second)
        return std::nullopt;
    }
    std::unordered_set<std::string> seen;
    for (const auto& record : new_records) {
      if (!record.is_object() || !record.contains("id") || !record.at("id").is_string())
        return std::nullopt;
      const auto id = record.at("id").get<std::string>();
      if (!next_graph.has_node(id) || !seen.insert(id).second)
        return std::nullopt;
      const auto old = old_by_id.find(id);
      if (old == old_by_id.end() || old->second != record)
        roots.push_back(id);
    }
    for (const auto& [id, record] : old_by_id)
      if (!seen.contains(id))
        return std::nullopt;
  }
  return roots;
}

bool is_project_path(const std::filesystem::path& path) {
  return path.filename().string().ends_with(".blcad.project.json");
}

std::filesystem::path constraint_sidecar_path(const std::filesystem::path& path) {
  return std::filesystem::path(path.string() + ".sketch-constraints.json");
}

std::filesystem::path dimension_sidecar_path(const std::filesystem::path& path) {
  return std::filesystem::path(path.string() + ".sketch-dimensions.json");
}

Result<std::string> read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return Result<std::string>::failure(
        Error::validation(path.string(), "could not open Sketch sidecar"));
  return Result<std::string>::success(
      std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()));
}

Error save_transaction_error(const std::filesystem::path& path, std::string message) {
  return Error::validation(path.empty() ? "gui.document_session.save" : path.string(),
                           std::move(message));
}

struct SaveTransactionFile {
  SaveTransactionFile(std::filesystem::path target_path, std::optional<std::string> file_content)
      : target(std::move(target_path)), content(std::move(file_content)) {}

  std::filesystem::path target;
  std::optional<std::string> content;
  std::filesystem::path staged;
  std::filesystem::path backup;
  std::optional<std::filesystem::perms> original_permissions;
  bool backed_up{false};
  bool installed{false};
};

std::filesystem::path transaction_path(const std::filesystem::path& target, std::string_view role) {
  static std::atomic<std::uint64_t> sequence{0U};
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto nonce = sequence.fetch_add(1U, std::memory_order_relaxed);
  const auto directory =
      target.has_parent_path() ? target.parent_path() : std::filesystem::path(".");
  return directory / (".blcad-save-" + std::string(role) + "-" + std::to_string(timestamp) + "-" +
                      std::to_string(nonce));
}

void remove_if_present(const std::filesystem::path& path) noexcept {
  if (path.empty())
    return;
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

std::string rollback_save_transaction(std::vector<SaveTransactionFile>& files) noexcept {
  std::string failures;
  for (auto& file : files) {
    if (!file.installed)
      continue;
    std::error_code error;
    std::filesystem::remove(file.target, error);
    if (error) {
      if (!failures.empty())
        failures += "; ";
      failures += "could not remove replacement " + file.target.string() + ": " + error.message();
    }
    file.installed = false;
  }
  for (auto file = files.rbegin(); file != files.rend(); ++file) {
    if (!file->backed_up)
      continue;
    std::error_code error;
    std::filesystem::rename(file->backup, file->target, error);
    if (error) {
      if (!failures.empty())
        failures += "; ";
      failures += "could not restore " + file->target.string() + ": " + error.message();
    } else {
      file->backed_up = false;
    }
  }
  for (const auto& file : files)
    remove_if_present(file.staged);
  return failures;
}

Result<std::uintmax_t> write_save_transaction(std::vector<SaveTransactionFile> files) {
  if (files.empty())
    return Result<std::uintmax_t>::failure(
        save_transaction_error({}, "save transaction requires at least one file"));

  std::uintmax_t total = 0U;
  for (auto& file : files) {
    if (file.target.empty())
      return Result<std::uintmax_t>::failure(
          save_transaction_error({}, "document path must not be empty"));

    std::error_code error;
    const auto status = std::filesystem::symlink_status(file.target, error);
    if (error && status.type() != std::filesystem::file_type::not_found)
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, "could not inspect save target: " + error.message()));
    if (status.type() != std::filesystem::file_type::not_found &&
        status.type() != std::filesystem::file_type::regular)
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, "save target exists but is not a regular file"));
    if (status.type() == std::filesystem::file_type::regular &&
        status.permissions() != std::filesystem::perms::unknown)
      file.original_permissions = status.permissions();

    file.backup = transaction_path(file.target, "backup");
    if (file.content)
      file.staged = transaction_path(file.target, "staged");
  }

  for (auto& file : files) {
    if (!file.content)
      continue;
    std::ofstream stream(file.staged, std::ios::binary | std::ios::trunc);
    if (!stream) {
      for (const auto& staged_file : files)
        remove_if_present(staged_file.staged);
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, "could not stage save transaction"));
    }
    stream.write(file.content->data(), static_cast<std::streamsize>(file.content->size()));
    stream.flush();
    if (!stream) {
      stream.close();
      for (const auto& staged_file : files)
        remove_if_present(staged_file.staged);
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, "could not write staged save transaction"));
    }
    stream.close();
    if (!stream) {
      for (const auto& staged_file : files)
        remove_if_present(staged_file.staged);
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, "could not close staged save transaction"));
    }
    if (file.original_permissions) {
      std::error_code permission_error;
      std::filesystem::permissions(file.staged, *file.original_permissions,
                                   std::filesystem::perm_options::replace, permission_error);
      if (permission_error) {
        for (const auto& staged_file : files)
          remove_if_present(staged_file.staged);
        return Result<std::uintmax_t>::failure(
            save_transaction_error(file.target, "could not preserve save-target permissions: " +
                                                    permission_error.message()));
      }
    }
    total += static_cast<std::uintmax_t>(file.content->size());
  }

  for (auto& file : files) {
    std::error_code status_error;
    const auto status = std::filesystem::symlink_status(file.target, status_error);
    if (status_error && status.type() != std::filesystem::file_type::not_found) {
      const auto rollback = rollback_save_transaction(files);
      std::string message =
          "could not inspect save target before commit: " + status_error.message();
      if (!rollback.empty())
        message += "; rollback incomplete: " + rollback;
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, std::move(message)));
    }
    if (status.type() == std::filesystem::file_type::not_found)
      continue;
    if (status.type() != std::filesystem::file_type::regular) {
      const auto rollback = rollback_save_transaction(files);
      std::string message = "save target changed to a non-regular file before commit";
      if (!rollback.empty())
        message += "; rollback incomplete: " + rollback;
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, std::move(message)));
    }
    std::error_code rename_error;
    std::filesystem::rename(file.target, file.backup, rename_error);
    if (rename_error) {
      const auto rollback = rollback_save_transaction(files);
      std::string message = "could not back up save target: " + rename_error.message();
      if (!rollback.empty())
        message += "; rollback incomplete: " + rollback;
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, std::move(message)));
    }
    file.backed_up = true;
  }

  for (auto& file : files) {
    if (!file.content)
      continue;
    std::error_code error;
    std::filesystem::rename(file.staged, file.target, error);
    if (error) {
      const auto rollback = rollback_save_transaction(files);
      std::string message = "could not commit staged save transaction: " + error.message();
      if (!rollback.empty())
        message += "; rollback incomplete: " + rollback;
      return Result<std::uintmax_t>::failure(
          save_transaction_error(file.target, std::move(message)));
    }
    file.installed = true;
    file.staged.clear();
  }

  for (const auto& file : files)
    remove_if_present(file.backup);
  return Result<std::uintmax_t>::success(total);
}

template <typename Catalog> void sort_catalogs(std::vector<Catalog>& catalogs) {
  std::sort(catalogs.begin(), catalogs.end(), [](const auto& left, const auto& right) {
    return left.sketch().value() < right.sketch().value();
  });
}

// Sidecars predate shared fitted-spline junctions. Remap only aliases explicitly
// reported by topology migration; arbitrary missing references must still fail
// before replacing the active document.
Result<std::size_t> migrate_sketch_sidecar_points(const PartDocument& document,
                                                  std::vector<SketchConstraintCatalog>& constraints,
                                                  std::vector<SketchDimensionCatalog>& dimensions) {
  const auto migrate_targets = [](const SketchTopology& topology,
                                  const SketchTopologyMigrationReport& migration,
                                  std::vector<SketchConstraintIntentTarget> targets)
      -> Result<std::vector<SketchConstraintIntentTarget>> {
    for (auto& target : targets) {
      if (target.kind() != SketchConstraintIntentTargetKind::Point ||
          topology.find_point(SketchPointId(target.id())) != nullptr)
        continue;
      const auto found = std::find_if(
          migration.identity_changes().begin(), migration.identity_changes().end(),
          [&target](const auto& change) { return change.previous_identity == target.id(); });
      if (found == migration.identity_changes().end() ||
          topology.find_point(SketchPointId(found->canonical_identity)) == nullptr)
        return Result<std::vector<SketchConstraintIntentTarget>>::failure(
            Error::validation(target.id(), "Sketch sidecar references an unknown point"));
      auto canonical =
          SketchConstraintIntentTarget::point(SketchPointId(found->canonical_identity));
      if (!canonical)
        return Result<std::vector<SketchConstraintIntentTarget>>::failure(canonical.error());
      target = std::move(canonical.value());
    }
    return Result<std::vector<SketchConstraintIntentTarget>>::success(std::move(targets));
  };
  std::size_t migrated = 0U;
  for (auto& catalog : constraints) {
    const auto* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(catalog.sketch().value(), "Sketch sidecar target does not exist"));
    SketchTopologyMigrationReport migration;
    auto topology = SketchTopology::migrate_legacy(*sketch, &migration);
    if (!topology)
      return Result<std::size_t>::failure(topology.error());
    std::vector<SketchConstraintIntent> remapped;
    for (const auto& intent : catalog.constraints()) {
      auto targets = migrate_targets(topology.value(), migration, intent.targets());
      if (!targets)
        return Result<std::size_t>::failure(targets.error());
      const bool changed = targets.value() != intent.targets();
      migrated += changed;
      // A formerly explicit join is now guaranteed by shared topology. Other
      // degenerate intents must be rejected rather than silently discarded.
      if (changed && intent.kind() == SketchSolverConstraintKind::Coincident &&
          targets.value()[0] == targets.value()[1])
        continue;
      auto replacement = SketchConstraintIntent::create(
          intent.id(), intent.kind(), std::move(targets.value()), intent.source());
      if (!replacement)
        return Result<std::size_t>::failure(replacement.error());
      remapped.push_back(std::move(replacement.value()));
    }
    auto replacement =
        SketchConstraintCatalog::create(catalog.document(), catalog.sketch(), std::move(remapped));
    if (!replacement)
      return Result<std::size_t>::failure(replacement.error());
    catalog = std::move(replacement.value());
  }
  for (auto& catalog : dimensions) {
    const auto* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(catalog.sketch().value(), "Sketch sidecar target does not exist"));
    SketchTopologyMigrationReport migration;
    auto topology = SketchTopology::migrate_legacy(*sketch, &migration);
    if (!topology)
      return Result<std::size_t>::failure(topology.error());
    std::vector<SketchDimensionIntent> remapped;
    for (const auto& intent : catalog.dimensions()) {
      auto targets = migrate_targets(topology.value(), migration, intent.targets());
      if (!targets)
        return Result<std::size_t>::failure(targets.error());
      migrated += targets.value() != intent.targets();
      auto replacement = SketchDimensionIntent::create(intent.id(), intent.kind(), intent.mode(),
                                                       std::move(targets.value()),
                                                       intent.parameter(), intent.label_offset());
      if (!replacement)
        return Result<std::size_t>::failure(replacement.error());
      remapped.push_back(std::move(replacement.value()));
    }
    auto replacement =
        SketchDimensionCatalog::create(catalog.document(), catalog.sketch(), std::move(remapped));
    if (!replacement)
      return Result<std::size_t>::failure(replacement.error());
    catalog = std::move(replacement.value());
  }
  return Result<std::size_t>::success(migrated);
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
  sketch_constraint_catalogs_.clear();
  sketch_dimension_catalogs_.clear();
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
  return Result<std::size_t>::success(1U);
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
  sketch_constraint_catalogs_.clear();
  sketch_dimension_catalogs_.clear();
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
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> GuiDocumentSession::open_file(const std::filesystem::path& path,
                                                  bool discard_dirty, GuiDocumentOpenMode mode) {
  if (const auto allowed = reject_replacement_if_needed(discard_dirty); allowed.has_error())
    return allowed;
  if (path.empty())
    return Result<std::size_t>::failure(session_error("document path must not be empty"));

  if (mode == GuiDocumentOpenMode::ReferenceRepair && is_project_path(path))
    return Result<std::size_t>::failure(session_error("reference repair opening requires a Part"));
  std::optional<Error> repair_failure;
  Document candidate;
  std::vector<SketchConstraintCatalog> candidate_constraints;
  std::vector<SketchDimensionCatalog> candidate_dimensions;
  if (is_project_path(path)) {
    auto read = read_project_json_file(path);
    if (read.has_error()) {
      record_error(read.error());
      return Result<std::size_t>::failure(read.error());
    }
    candidate = std::move(read.value());
  } else {
    auto read = read_part_document_json_file(path, mode == GuiDocumentOpenMode::ReferenceRepair
                                                       ? PartDocumentReadMode::ReferenceRepair
                                                       : PartDocumentReadMode::Strict);
    if (read.has_error()) {
      record_error(read.error());
      return Result<std::size_t>::failure(read.error());
    }
    candidate = std::move(read.value());

    const auto constraint_path = constraint_sidecar_path(path);
    if (std::filesystem::exists(constraint_path)) {
      auto content = read_text_file(constraint_path);
      if (content.has_error()) {
        record_error(content.error());
        return Result<std::size_t>::failure(content.error());
      }
      auto parsed = deserialize_sketch_constraint_catalogs_from_json(content.value());
      if (parsed.has_error()) {
        record_error(parsed.error());
        return Result<std::size_t>::failure(parsed.error());
      }
      candidate_constraints = std::move(parsed.value());
    }

    const auto dimension_path = dimension_sidecar_path(path);
    if (std::filesystem::exists(dimension_path)) {
      auto content = read_text_file(dimension_path);
      if (content.has_error()) {
        record_error(content.error());
        return Result<std::size_t>::failure(content.error());
      }
      auto parsed = deserialize_sketch_dimension_catalogs_from_json(content.value());
      if (parsed.has_error()) {
        record_error(parsed.error());
        return Result<std::size_t>::failure(parsed.error());
      }
      candidate_dimensions = std::move(parsed.value());
    }
  }

  std::optional<geometry::ShapeCache> part_cache;
  std::vector<ProjectPartCache> project_caches;
  if (auto* part = std::get_if<PartDocument>(&candidate)) {
    auto migrated =
        migrate_sketch_sidecar_points(*part, candidate_constraints, candidate_dimensions);
    if (!migrated) {
      record_error(migrated.error());
      return migrated;
    }
    auto valid_constraints = validate_constraint_catalogs(*part, candidate_constraints);
    if (valid_constraints.has_error()) {
      record_error(valid_constraints.error());
      return valid_constraints;
    }
    auto valid_dimensions = validate_dimension_catalogs(*part, candidate_dimensions);
    if (valid_dimensions.has_error()) {
      record_error(valid_dimensions.error());
      return valid_dimensions;
    }
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.part." + part->id().value()));
    if (cache.has_error())
      return Result<std::size_t>::failure(cache.error());
    auto result = recompute_part(*part, cache.value());
    if (result.has_error()) {
      if (mode != GuiDocumentOpenMode::ReferenceRepair) {
        record_error(result.error());
        return result;
      }
      repair_failure = result.error();
    } else {
      part->mark_all_clean();
      part_cache = std::move(cache.value());
    }
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
  sketch_constraint_catalogs_ = std::move(candidate_constraints);
  sketch_dimension_catalogs_ = std::move(candidate_dimensions);
  file_path_ = path;
  part_shape_cache_ = std::move(part_cache);
  project_part_caches_ = std::move(project_caches);
  ++presentation_revision_;
  reference_repair_mode_ = repair_failure.has_value();
  recompute_status_ = repair_failure ? GuiRecomputeStatus::Failed : GuiRecomputeStatus::Fresh;
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
  diagnose_step_import_sources();
  if (repair_failure)
    record_error(*repair_failure);
  return Result<std::size_t>::success(1U);
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
  if (path.empty()) {
    const auto error = session_error("document path must not be empty");
    record_error(error);
    return Result<std::uintmax_t>::failure(error);
  }

  const auto protected_sources = validate_step_import_save_as(*this, path);
  if (!protected_sources) {
    record_error(protected_sources.error());
    return Result<std::uintmax_t>::failure(protected_sources.error());
  }

  std::optional<std::string> constraint_json;
  std::optional<std::string> dimension_json;
  if (const auto* part = part_document()) {
    auto valid_constraints = validate_constraint_catalogs(*part, sketch_constraint_catalogs_);
    if (valid_constraints.has_error()) {
      record_error(valid_constraints.error());
      return Result<std::uintmax_t>::failure(valid_constraints.error());
    }
    auto valid_dimensions = validate_dimension_catalogs(*part, sketch_dimension_catalogs_);
    if (valid_dimensions.has_error()) {
      record_error(valid_dimensions.error());
      return Result<std::uintmax_t>::failure(valid_dimensions.error());
    }
    if (!sketch_constraint_catalogs_.empty()) {
      auto serialized =
          serialize_sketch_constraint_catalogs_to_json(part->id(), sketch_constraint_catalogs_);
      if (serialized.has_error()) {
        record_error(serialized.error());
        return Result<std::uintmax_t>::failure(serialized.error());
      }
      constraint_json = std::move(serialized.value());
    }
    if (!sketch_dimension_catalogs_.empty()) {
      auto serialized =
          serialize_sketch_dimension_catalogs_to_json(part->id(), sketch_dimension_catalogs_);
      if (serialized.has_error()) {
        record_error(serialized.error());
        return Result<std::uintmax_t>::failure(serialized.error());
      }
      dimension_json = std::move(serialized.value());
    }
  }

  auto main_json = serialize_current();
  if (main_json.has_error()) {
    record_error(main_json.error());
    return Result<std::uintmax_t>::failure(main_json.error());
  }

  std::vector<SaveTransactionFile> files;
  files.push_back({path, main_json.value()});
  if (part_document()) {
    files.push_back({constraint_sidecar_path(path), std::move(constraint_json)});
    files.push_back({dimension_sidecar_path(path), std::move(dimension_json)});
  }
  auto written = write_save_transaction(std::move(files));
  if (written.has_error()) {
    record_error(written.error());
    return written;
  }
  file_path_ = path;
  saved_json_ = main_json.value();
  saved_sketch_constraint_catalogs_ = sketch_constraint_catalogs_;
  saved_sketch_dimension_catalogs_ = sketch_dimension_catalogs_;
  record_information(path.string(), "document saved");
  diagnose_step_import_sources();
  return written;
}

Result<std::size_t> GuiDocumentSession::recompute() {
  if (auto* part = part_document()) {
    auto valid_constraints = validate_constraint_catalogs(*part, sketch_constraint_catalogs_);
    if (valid_constraints.has_error()) {
      record_error(valid_constraints.error());
      return valid_constraints;
    }
    auto valid_dimensions = validate_dimension_catalogs(*part, sketch_dimension_catalogs_);
    if (valid_dimensions.has_error()) {
      record_error(valid_dimensions.error());
      return valid_dimensions;
    }
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
    reference_repair_mode_ = false;
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
    std::size_t executed = 0U;
    for (const auto& cache : caches.value())
      executed += cache.cache.feature_shape_count();
    project_part_caches_ = std::move(caches.value());
    part_shape_cache_.reset();
    reference_repair_mode_ = false;
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
  if (reference_repair_mode_)
    return Result<std::size_t>::failure(session_error("complete reference repair before modeling"));
  return commit_part_repair_transaction(std::move(label), mutation);
}

Result<std::size_t>
GuiDocumentSession::commit_part_repair_transaction(std::string label,
                                                   const GuiPartMutation& mutation) {
  if (!part_document() || !mutation || label.empty())
    return Result<std::size_t>::failure(session_error("invalid Part transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_part_document_from_json(
      serialized.value(), reference_repair_mode_ ? PartDocumentReadMode::ReferenceRepair
                                                 : PartDocumentReadMode::Strict);
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto changed = mutation(cloned.value());
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  return publish_part_state(std::move(cloned.value()), sketch_constraint_catalogs_,
                            sketch_dimension_catalogs_, std::move(label), changed.value());
}

Result<std::size_t>
GuiDocumentSession::commit_part_constraint_transaction(std::string label,
                                                       const GuiPartConstraintMutation& mutation) {
  if (reference_repair_mode_)
    return Result<std::size_t>::failure(session_error("complete reference repair before modeling"));
  if (!part_document() || !mutation || label.empty())
    return Result<std::size_t>::failure(
        session_error("invalid Part/Sketch-constraint transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_part_document_from_json(serialized.value());
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto constraints = sketch_constraint_catalogs_;
  auto changed = mutation(cloned.value(), constraints);
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  return publish_part_state(std::move(cloned.value()), std::move(constraints),
                            sketch_dimension_catalogs_, std::move(label), changed.value());
}

Result<std::size_t>
GuiDocumentSession::commit_part_dimension_transaction(std::string label,
                                                      const GuiPartDimensionMutation& mutation) {
  if (reference_repair_mode_)
    return Result<std::size_t>::failure(session_error("complete reference repair before modeling"));
  if (!part_document() || !mutation || label.empty())
    return Result<std::size_t>::failure(session_error("invalid Part/Sketch-dimension transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_part_document_from_json(serialized.value());
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto dimensions = sketch_dimension_catalogs_;
  auto changed = mutation(cloned.value(), dimensions);
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  return publish_part_state(std::move(cloned.value()), sketch_constraint_catalogs_,
                            std::move(dimensions), std::move(label), changed.value());
}

Result<std::size_t> GuiDocumentSession::commit_part_sketch_intent_transaction(
    std::string label, const GuiPartSketchIntentMutation& mutation) {
  if (reference_repair_mode_)
    return Result<std::size_t>::failure(session_error("complete reference repair before modeling"));
  if (!part_document() || !mutation || label.empty())
    return Result<std::size_t>::failure(session_error("invalid Part/Sketch-intent transaction"));
  const auto serialized = serialize_current();
  if (serialized.has_error())
    return Result<std::size_t>::failure(serialized.error());
  auto cloned = deserialize_part_document_from_json(serialized.value());
  if (cloned.has_error())
    return Result<std::size_t>::failure(cloned.error());
  auto constraints = sketch_constraint_catalogs_;
  auto dimensions = sketch_dimension_catalogs_;
  auto changed = mutation(cloned.value(), constraints, dimensions);
  if (changed.has_error()) {
    record_error(changed.error());
    return changed;
  }
  return publish_part_state(std::move(cloned.value()), std::move(constraints),
                            std::move(dimensions), std::move(label), changed.value());
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
  const auto json = serialize_project_to_json(cloned.value());
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  document_ = std::move(cloned.value());
  sketch_constraint_catalogs_.clear();
  sketch_dimension_catalogs_.clear();
  project_part_caches_ = std::move(caches.value());
  part_shape_cache_.reset();
  recompute_status_ = GuiRecomputeStatus::Fresh;
  reference_repair_mode_ = false;
  ++presentation_revision_;
  append_history(json.value(), std::move(label));
  selection_.clear();
  return changed;
}

void GuiDocumentSession::restore_creation_checkpoint(GuiDocumentSession checkpoint) {
  checkpoint.file_path_ = std::move(file_path_);
  checkpoint.saved_json_ = std::move(saved_json_);
  checkpoint.saved_sketch_constraint_catalogs_ = std::move(saved_sketch_constraint_catalogs_);
  checkpoint.saved_sketch_dimension_catalogs_ = std::move(saved_sketch_dimension_catalogs_);
  const auto next_model_revision = std::max(model_revision(), checkpoint.model_revision()) + 1;
  checkpoint.visibility_revision_ = std::max(visibility_revision_, checkpoint.visibility_revision_);
  checkpoint.presentation_revision_ = checkpoint.visibility_revision_ + next_model_revision;
  *this = std::move(checkpoint);
}

Result<std::size_t> GuiDocumentSession::undo() {
  if (!can_undo())
    return Result<std::size_t>::failure(session_error("nothing to undo"));
  return restore_history(history_index_ - 1U);
}

Result<std::size_t> GuiDocumentSession::redo() {
  if (!can_redo())
    return Result<std::size_t>::failure(session_error("nothing to redo"));
  return restore_history(history_index_ + 1U);
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
  if (reference_repair_mode_)
    return;
  recompute_status_ = fresh ? GuiRecomputeStatus::Fresh : GuiRecomputeStatus::Stale;
}

void GuiDocumentSession::report_error(const Error& error) {
  record_error(error);
}

void GuiDocumentSession::clear_diagnostics() noexcept {
  if (diagnostics_.empty())
    return;
  diagnostics_.clear();
  ++presentation_revision_;
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
  const auto& current = history_[history_index_];
  return !saved_json_ || current.json != *saved_json_ || !saved_sketch_constraint_catalogs_ ||
         current.sketch_constraints != *saved_sketch_constraint_catalogs_ ||
         !saved_sketch_dimension_catalogs_ ||
         current.sketch_dimensions != *saved_sketch_dimension_catalogs_;
}

bool GuiDocumentSession::reference_repair_mode() const noexcept {
  return reference_repair_mode_;
}

bool GuiDocumentSession::derived_results_fresh() const noexcept {
  return recompute_status_ == GuiRecomputeStatus::Fresh;
}

GuiRecomputeStatus GuiDocumentSession::recompute_status() const noexcept {
  return recompute_status_;
}

bool GuiDocumentSession::can_undo() const noexcept {
  return history_index_ > 0U && !task_.active();
}

bool GuiDocumentSession::can_redo() const noexcept {
  return !history_.empty() && history_index_ + 1U < history_.size() && !task_.active();
}

std::string_view GuiDocumentSession::undo_label() const noexcept {
  return can_undo() ? history_[history_index_].label : std::string_view{};
}

std::string_view GuiDocumentSession::redo_label() const noexcept {
  return can_redo() ? history_[history_index_ + 1U].label : std::string_view{};
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

std::size_t GuiDocumentSession::model_revision() const noexcept {
  return presentation_revision_ - visibility_revision_;
}

Result<SketchConstraintCatalog>
GuiDocumentSession::sketch_constraint_catalog(SketchId sketch) const {
  const auto* part = part_document();
  if (part == nullptr)
    return Result<SketchConstraintCatalog>::failure(
        session_error("Sketch constraint catalog requires an active Part document"));
  if (part->find_sketch(sketch) == nullptr)
    return Result<SketchConstraintCatalog>::failure(
        Error::validation(sketch.value(), "Sketch constraint catalog target does not exist"));
  const auto found =
      std::find_if(sketch_constraint_catalogs_.begin(), sketch_constraint_catalogs_.end(),
                   [&sketch](const auto& catalog) { return catalog.sketch() == sketch; });
  if (found != sketch_constraint_catalogs_.end())
    return Result<SketchConstraintCatalog>::success(*found);
  return SketchConstraintCatalog::create(part->id(), std::move(sketch));
}

const std::vector<SketchConstraintCatalog>&
GuiDocumentSession::sketch_constraint_catalogs() const noexcept {
  return sketch_constraint_catalogs_;
}

Result<SketchDimensionCatalog> GuiDocumentSession::sketch_dimension_catalog(SketchId sketch) const {
  const auto* part = part_document();
  if (part == nullptr)
    return Result<SketchDimensionCatalog>::failure(
        session_error("Sketch dimension catalog requires an active Part document"));
  if (part->find_sketch(sketch) == nullptr)
    return Result<SketchDimensionCatalog>::failure(
        Error::validation(sketch.value(), "Sketch dimension catalog target does not exist"));
  const auto found =
      std::find_if(sketch_dimension_catalogs_.begin(), sketch_dimension_catalogs_.end(),
                   [&sketch](const auto& catalog) { return catalog.sketch() == sketch; });
  if (found != sketch_dimension_catalogs_.end())
    return Result<SketchDimensionCatalog>::success(*found);
  return SketchDimensionCatalog::create(part->id(), std::move(sketch));
}

const std::vector<SketchDimensionCatalog>&
GuiDocumentSession::sketch_dimension_catalogs() const noexcept {
  return sketch_dimension_catalogs_;
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
    auto part = deserialize_part_document_from_json(
        entry.json, entry.allow_failed_recompute ? PartDocumentReadMode::ReferenceRepair
                                                 : PartDocumentReadMode::Strict);
    if (part.has_error())
      return Result<std::size_t>::failure(part.error());
    auto valid_constraints = validate_constraint_catalogs(part.value(), entry.sketch_constraints);
    if (valid_constraints.has_error())
      return valid_constraints;
    auto valid_dimensions = validate_dimension_catalogs(part.value(), entry.sketch_dimensions);
    if (valid_dimensions.has_error())
      return valid_dimensions;
    restored = std::move(part.value());
  } else {
    auto active_project = deserialize_project_from_json(entry.json);
    if (active_project.has_error())
      return Result<std::size_t>::failure(active_project.error());
    restored = std::move(active_project.value());
  }

  std::optional<geometry::ShapeCache> restored_cache;
  std::vector<ProjectPartCache> restored_project_caches;
  std::optional<Error> repair_failure;
  if (auto* part = std::get_if<PartDocument>(&restored)) {
    const auto retained =
        std::find_if(part_history_geometry_.begin(), part_history_geometry_.end(),
                     [&](const auto& item) { return item.history_index == index; });
    const bool reuse = !entry.allow_failed_recompute && part->step_import_sources().empty() &&
                       retained != part_history_geometry_.end();
    auto cache = reuse
                     ? Result<geometry::ShapeCache>::success(retained->cache)
                     : geometry::ShapeCache::create(ShapeCacheId("gui.part." + part->id().value()));
    if (!cache)
      return Result<std::size_t>::failure(cache.error());
    auto computed = reuse ? Result<std::size_t>::success(0) : recompute_part(*part, cache.value());
    if (!computed) {
      if (!entry.allow_failed_recompute) {
        record_error(computed.error());
        return computed;
      }
      repair_failure = computed.error();
    } else {
      part->mark_all_clean();
      restored_cache = std::move(cache.value());
    }
  } else {
    auto caches = recompute_project(std::get<Project>(restored));
    if (!caches) {
      record_error(caches.error());
      return Result<std::size_t>::failure(caches.error());
    }
    restored_project_caches = std::move(caches.value());
    for (auto& part : std::get<Project>(restored).part_documents())
      part.mark_all_clean();
  }
  document_ = std::move(restored);
  sketch_constraint_catalogs_ = entry.sketch_constraints;
  sketch_dimension_catalogs_ = entry.sketch_dimensions;
  part_shape_cache_ = std::move(restored_cache);
  project_part_caches_ = std::move(restored_project_caches);
  reference_repair_mode_ = repair_failure.has_value();
  recompute_status_ = repair_failure ? GuiRecomputeStatus::Failed : GuiRecomputeStatus::Fresh;
  ++presentation_revision_;
  if (repair_failure)
    record_error(*repair_failure);
  history_index_ = index;
  remember_part_history_geometry();
  selection_.clear();
  diagnose_step_import_sources();
  return Result<std::size_t>::success(1U);
}

void GuiDocumentSession::diagnose_step_import_sources() {
  auto inspected = GuiStepImportWorkbench{}.inspect_sources(*this);
  if (!inspected) {
    record_error(inspected.error());
    return;
  }
  for (const auto& item : inspected.value())
    if (item.report.state() != StepImportFreshness::Available) {
      diagnostics_.push_back({GuiDiagnosticSeverity::Warning, ErrorCategory::Validation,
                              item.owner_document.value() + "/" + item.report.source().id().value(),
                              "STEP source " + std::string(to_string(item.report.state())) + ": " +
                                  item.report.diagnostic() +
                                  "; inspect or refresh the source explicitly"});
      ++presentation_revision_;
    }
}

Result<std::size_t> GuiDocumentSession::reject_replacement_if_needed(bool discard_dirty) const {
  if (task_.active())
    return Result<std::size_t>::failure(session_error("active task must be applied or cancelled"));
  if (dirty() && !discard_dirty)
    return Result<std::size_t>::failure(session_error("dirty document requires save or discard"));
  return Result<std::size_t>::success(0U);
}

Result<std::size_t> GuiDocumentSession::validate_constraint_catalogs(
    const PartDocument& document, const std::vector<SketchConstraintCatalog>& catalogs) const {
  std::vector<std::string> sketches;
  sketches.reserve(catalogs.size());
  for (const auto& catalog : catalogs) {
    if (catalog.document() != document.id())
      return Result<std::size_t>::failure(Error::validation(
          catalog.sketch().value(), "Sketch constraint catalog belongs to another Part"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          catalog.sketch().value(), "Sketch constraint catalog target Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<std::size_t>::failure(topology.error());
    auto system = SketchConstraintCatalogSystemBuilder::build(document, topology.value(), catalog);
    if (system.has_error())
      return Result<std::size_t>::failure(system.error());
    sketches.push_back(catalog.sketch().value());
  }
  std::sort(sketches.begin(), sketches.end());
  if (std::adjacent_find(sketches.begin(), sketches.end()) != sketches.end())
    return Result<std::size_t>::failure(
        session_error("Sketch constraint catalogs must be unique per Sketch"));
  return Result<std::size_t>::success(catalogs.size());
}

Result<std::size_t> GuiDocumentSession::validate_dimension_catalogs(
    const PartDocument& document, const std::vector<SketchDimensionCatalog>& catalogs) const {
  std::vector<std::string> sketches;
  sketches.reserve(catalogs.size());
  for (const auto& catalog : catalogs) {
    if (catalog.document() != document.id())
      return Result<std::size_t>::failure(Error::validation(
          catalog.sketch().value(), "Sketch dimension catalog belongs to another Part"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          catalog.sketch().value(), "Sketch dimension catalog target Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<std::size_t>::failure(topology.error());
    for (const auto& dimension : catalog.dimensions()) {
      auto measurement =
          SketchDimensionMeasurementEvaluator::measure(topology.value(), dimension, &document);
      if (measurement.has_error())
        return Result<std::size_t>::failure(measurement.error());
      if (dimension.driving()) {
        auto converted = SketchDimensionSolverAdapter::to_solver_constraint(
            document, topology.value(), dimension);
        if (converted.has_error())
          return Result<std::size_t>::failure(converted.error());
      }
    }
    sketches.push_back(catalog.sketch().value());
  }
  std::sort(sketches.begin(), sketches.end());
  if (std::adjacent_find(sketches.begin(), sketches.end()) != sketches.end())
    return Result<std::size_t>::failure(
        session_error("Sketch dimension catalogs must be unique per Sketch"));
  return Result<std::size_t>::success(catalogs.size());
}

Result<std::size_t> GuiDocumentSession::publish_part_state(
    PartDocument document, std::vector<SketchConstraintCatalog> constraints,
    std::vector<SketchDimensionCatalog> dimensions, std::string label, std::size_t changed_count) {
  sort_catalogs(constraints);
  sort_catalogs(dimensions);
  auto valid_constraints = validate_constraint_catalogs(document, constraints);
  if (valid_constraints.has_error()) {
    record_error(valid_constraints.error());
    return valid_constraints;
  }
  auto valid_dimensions = validate_dimension_catalogs(document, dimensions);
  if (valid_dimensions.has_error()) {
    record_error(valid_dimensions.error());
    return valid_dimensions;
  }
  auto json = serialize_part_document_to_json(document);
  if (json.has_error())
    return Result<std::size_t>::failure(json.error());
  auto cache = recompute_part_candidate(document, json.value());
  if (!cache) {
    record_error(cache.error());
    return Result<std::size_t>::failure(cache.error());
  }
  document.mark_all_clean();

  document_ = std::move(document);
  sketch_constraint_catalogs_ = std::move(constraints);
  sketch_dimension_catalogs_ = std::move(dimensions);
  part_shape_cache_ = std::move(cache.value());
  project_part_caches_.clear();
  recompute_status_ = GuiRecomputeStatus::Fresh;
  reference_repair_mode_ = false;
  ++presentation_revision_;
  append_history(std::move(json.value()), std::move(label));
  selection_.clear();
  return Result<std::size_t>::success(changed_count);
}

Result<geometry::ShapeCache>
GuiDocumentSession::recompute_part_candidate(PartDocument& document,
                                             const std::string& json) const {
  const auto full = [&]() -> Result<geometry::ShapeCache> {
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.part." + document.id().value()));
    if (!cache)
      return cache;
    auto computed = recompute_part(document, cache.value());
    if (!computed)
      return Result<geometry::ShapeCache>::failure(computed.error());
    return cache;
  };
  const auto* current = part_document();
  const auto has_legacy_aggregate = [](const PartDocument& part) {
    return std::any_of(part.features().begin(), part.features().end(),
                       [](const auto& feature) { return !feature.body_result_context(); });
  };
  if (!current || history_.empty() || reference_repair_mode_ ||
      recompute_status_ != GuiRecomputeStatus::Fresh || !part_shape_cache_ ||
      current->id() != document.id() || current->bodies().empty() || document.bodies().empty() ||
      !current->step_import_sources().empty() || !document.step_import_sources().empty() ||
      has_legacy_aggregate(*current) || has_legacy_aggregate(document))
    return full();
  const auto baseline = serialize_current();
  if (!baseline || baseline.value() != history_[history_index_].json)
    return full();
  auto roots = transaction_geometry_roots(*current, document, baseline.value(), json);
  if (!roots)
    return full();
  auto candidate = document;
  candidate.mark_all_clean();
  return detail::recompute_preview_geometry(*this, candidate, *roots);
}

void GuiDocumentSession::remember_part_history_geometry() {
  std::erase_if(part_history_geometry_, [&](const auto& item) {
    return item.history_index == history_index_ || item.history_index >= history_.size();
  });
  if (!part_document() || !part_shape_cache_ || recompute_status_ != GuiRecomputeStatus::Fresh ||
      reference_repair_mode_ || !part_document()->step_import_sources().empty())
    return;
  // Derived BReps are runtime-only and shared immutably with the working cache.
  // Retain four recently visited states, never the entire unbounded history.
  constexpr std::size_t retained_states = 4;
  if (part_history_geometry_.size() == retained_states)
    part_history_geometry_.erase(part_history_geometry_.begin());
  part_history_geometry_.push_back({history_index_, *part_shape_cache_});
}

void GuiDocumentSession::initialize_history(std::string json, std::string label, bool saved) {
  hidden_presentations_.clear();
  history_ = {{document_kind(), std::move(json), sketch_constraint_catalogs_,
               sketch_dimension_catalogs_, std::move(label), reference_repair_mode_}};
  history_index_ = 0U;
  part_history_geometry_.clear();
  remember_part_history_geometry();
  saved_json_ = saved ? std::optional<std::string>(history_.front().json) : std::nullopt;
  saved_sketch_constraint_catalogs_ =
      saved
          ? std::optional<std::vector<SketchConstraintCatalog>>(history_.front().sketch_constraints)
          : std::nullopt;
  saved_sketch_dimension_catalogs_ =
      saved ? std::optional<std::vector<SketchDimensionCatalog>>(history_.front().sketch_dimensions)
            : std::nullopt;
  selection_.clear();
}

void GuiDocumentSession::append_history(std::string json, std::string label) {
  history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1U),
                 history_.end());
  history_.push_back({document_kind(), std::move(json), sketch_constraint_catalogs_,
                      sketch_dimension_catalogs_, std::move(label)});
  history_index_ = history_.size() - 1U;
  remember_part_history_geometry();
}

void GuiDocumentSession::record_error(const Error& error) {
  diagnostics_.push_back(
      {GuiDiagnosticSeverity::Error, error.category(), error.object_id(), error.message()});
  ++presentation_revision_;
}

void GuiDocumentSession::record_information(std::string object_id, std::string message) {
  diagnostics_.push_back({GuiDiagnosticSeverity::Information, ErrorCategory::Internal,
                          std::move(object_id), std::move(message)});
  ++presentation_revision_;
}

void GuiDocumentSession::reset_document() noexcept {
  ++presentation_revision_;
  document_ = std::monostate{};
  reference_repair_mode_ = false;
  display_name_.clear();
  file_path_.clear();
  workspace_ = GuiWorkspace::Part;
  recompute_status_ = GuiRecomputeStatus::Unavailable;
  part_shape_cache_.reset();
  project_part_caches_.clear();
  sketch_constraint_catalogs_.clear();
  sketch_dimension_catalogs_.clear();
  history_.clear();
  part_history_geometry_.clear();
  history_index_ = 0U;
  saved_json_.reset();
  saved_sketch_constraint_catalogs_.reset();
  saved_sketch_dimension_catalogs_.reset();
  diagnostics_.clear();
  selection_.clear();
  hidden_presentations_.clear();
}

Result<std::size_t> GuiDocumentSession::set_presentation_visible(std::string semantic_id,
                                                                 bool visible) {
  if (!has_document() || semantic_id.empty() || semantic_id.find('\0') != std::string::npos)
    return Result<std::size_t>::failure(Error::validation(
        "gui.visibility", "presentation visibility requires a document and a semantic identity"));
  if (visible) {
    const auto count = std::erase_if(hidden_presentations_, [&semantic_id](const auto& hidden) {
      return semantic_id == hidden ||
             (semantic_id.starts_with(hidden) && semantic_id.size() > hidden.size() &&
              semantic_id[hidden.size()] == '/');
    });
    if (count != 0U) {
      ++presentation_revision_;
      ++visibility_revision_;
    }
    return Result<std::size_t>::success(count);
  }
  const auto found =
      std::find(hidden_presentations_.begin(), hidden_presentations_.end(), semantic_id);
  if (found != hidden_presentations_.end())
    return Result<std::size_t>::success(0U);
  hidden_presentations_.push_back(std::move(semantic_id));
  std::sort(hidden_presentations_.begin(), hidden_presentations_.end());
  selection_.clear();
  ++presentation_revision_;
  ++visibility_revision_;
  return Result<std::size_t>::success(1U);
}

bool GuiDocumentSession::presentation_visible(std::string_view semantic_id) const noexcept {
  if (const auto role = semantic_id.find(kGuiSketchHitRoleMarker); role != std::string_view::npos)
    semantic_id = semantic_id.substr(0U, role);
  return std::none_of(hidden_presentations_.begin(), hidden_presentations_.end(),
                      [semantic_id](const auto& hidden) {
                        return semantic_id == hidden || (semantic_id.starts_with(hidden) &&
                                                         semantic_id.size() > hidden.size() &&
                                                         semantic_id[hidden.size()] == '/');
                      });
}

std::size_t GuiDocumentSession::show_all_presentations() noexcept {
  const std::size_t count = hidden_presentations_.size();
  hidden_presentations_.clear();
  if (count != 0U) {
    ++presentation_revision_;
    ++visibility_revision_;
  }
  return count;
}

const std::vector<std::string>& GuiDocumentSession::hidden_presentations() const noexcept {
  return hidden_presentations_;
}

} // namespace blcad::gui
