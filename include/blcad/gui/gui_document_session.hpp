#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/gui/gui_selection_model.hpp"
#include "blcad/gui/gui_task_state.hpp"
#include "blcad/gui/gui_types.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad::gui {

enum class GuiRecomputeStatus { Unavailable, Stale, Fresh, Failed };
enum class GuiDiagnosticSeverity { Information, Warning, Error };

struct GuiDiagnostic {
  GuiDiagnosticSeverity severity{GuiDiagnosticSeverity::Information};
  ErrorCategory category{ErrorCategory::Internal};
  std::string object_id;
  std::string message;
};

using GuiPartMutation = std::function<Result<std::size_t>(PartDocument&)>;
using GuiPartConstraintMutation =
    std::function<Result<std::size_t>(PartDocument&, std::vector<SketchConstraintCatalog>&)>;
using GuiProjectMutation = std::function<Result<std::size_t>(Project&)>;

class GuiDocumentSession {
public:
  [[nodiscard]] Result<std::size_t> create_part(DocumentId id, std::string name,
                                                 bool discard_dirty = false);
  [[nodiscard]] Result<std::size_t> create_project(DocumentId id, std::string name,
                                                    bool discard_dirty = false);
  [[nodiscard]] Result<std::size_t> open_file(const std::filesystem::path& path,
                                               bool discard_dirty = false);
  [[nodiscard]] Result<std::uintmax_t> save();
  [[nodiscard]] Result<std::uintmax_t> save_as(const std::filesystem::path& path);
  [[nodiscard]] Result<std::size_t> recompute();
  [[nodiscard]] Result<std::size_t> commit_part_transaction(std::string label,
                                                             const GuiPartMutation& mutation);
  [[nodiscard]] Result<std::size_t>
  commit_part_constraint_transaction(std::string label,
                                     const GuiPartConstraintMutation& mutation);
  [[nodiscard]] Result<std::size_t> commit_project_transaction(std::string label,
                                                                const GuiProjectMutation& mutation);
  [[nodiscard]] Result<std::size_t> undo();
  [[nodiscard]] Result<std::size_t> redo();

  [[nodiscard]] bool close(bool discard_dirty = false) noexcept;
  [[nodiscard]] bool set_workspace(GuiWorkspace workspace) noexcept;
  void set_derived_results_fresh(bool fresh) noexcept;
  void clear_diagnostics() noexcept;

  [[nodiscard]] GuiDocumentKind document_kind() const noexcept;
  [[nodiscard]] std::string_view display_name() const noexcept;
  [[nodiscard]] const std::filesystem::path& file_path() const noexcept;
  [[nodiscard]] GuiWorkspace workspace() const noexcept;
  [[nodiscard]] bool has_document() const noexcept;
  [[nodiscard]] bool dirty() const noexcept;
  [[nodiscard]] bool derived_results_fresh() const noexcept;
  [[nodiscard]] GuiRecomputeStatus recompute_status() const noexcept;
  [[nodiscard]] bool can_undo() const noexcept;
  [[nodiscard]] bool can_redo() const noexcept;
  [[nodiscard]] std::string_view undo_label() const noexcept;
  [[nodiscard]] std::string_view redo_label() const noexcept;
  [[nodiscard]] GuiCommandContext command_context() const noexcept;

  [[nodiscard]] PartDocument* part_document() noexcept;
  [[nodiscard]] const PartDocument* part_document() const noexcept;
  [[nodiscard]] Project* project() noexcept;
  [[nodiscard]] const Project* project() const noexcept;
  [[nodiscard]] const geometry::ShapeCache* part_shape_cache() const noexcept;
  [[nodiscard]] const std::vector<GuiDiagnostic>& diagnostics() const noexcept;
  [[nodiscard]] std::size_t presentation_revision() const noexcept;
  [[nodiscard]] Result<SketchConstraintCatalog>
  sketch_constraint_catalog(SketchId sketch) const;
  [[nodiscard]] const std::vector<SketchConstraintCatalog>&
  sketch_constraint_catalogs() const noexcept;

  [[nodiscard]] GuiSelectionModel& selection() noexcept;
  [[nodiscard]] const GuiSelectionModel& selection() const noexcept;
  [[nodiscard]] GuiTaskState& task() noexcept;
  [[nodiscard]] const GuiTaskState& task() const noexcept;

private:
  struct HistoryEntry {
    GuiDocumentKind kind{GuiDocumentKind::None};
    std::string json;
    std::vector<SketchConstraintCatalog> sketch_constraints;
    std::string label;
  };

  struct ProjectPartCache {
    DocumentId part_id;
    geometry::ShapeCache cache;
  };

  using Document = std::variant<std::monostate, PartDocument, Project>;

  [[nodiscard]] Result<std::string> serialize_current() const;
  [[nodiscard]] Result<std::size_t> recompute_part(PartDocument& document,
                                                    geometry::ShapeCache& cache) const;
  [[nodiscard]] Result<std::vector<ProjectPartCache>> recompute_project(Project& project) const;
  [[nodiscard]] Result<std::size_t> restore_history(std::size_t index);
  [[nodiscard]] Result<std::size_t> reject_replacement_if_needed(bool discard_dirty) const;
  [[nodiscard]] Result<std::size_t>
  validate_constraint_catalogs(const PartDocument& document,
                               const std::vector<SketchConstraintCatalog>& catalogs) const;
  void initialize_history(std::string json, std::string label, bool saved);
  void append_history(std::string json, std::string label);
  void record_error(const Error& error);
  void record_information(std::string object_id, std::string message);
  void reset_document() noexcept;

  Document document_;
  std::string display_name_;
  std::filesystem::path file_path_;
  GuiWorkspace workspace_{GuiWorkspace::Part};
  GuiRecomputeStatus recompute_status_{GuiRecomputeStatus::Unavailable};
  std::optional<geometry::ShapeCache> part_shape_cache_;
  std::vector<ProjectPartCache> project_part_caches_;
  std::vector<SketchConstraintCatalog> sketch_constraint_catalogs_;
  std::vector<HistoryEntry> history_;
  std::size_t history_index_{0};
  std::optional<std::string> saved_json_;
  std::optional<std::vector<SketchConstraintCatalog>> saved_sketch_constraint_catalogs_;
  std::vector<GuiDiagnostic> diagnostics_;
  GuiSelectionModel selection_;
  GuiTaskState task_;
  std::size_t presentation_revision_{0};
};

} // namespace blcad::gui
