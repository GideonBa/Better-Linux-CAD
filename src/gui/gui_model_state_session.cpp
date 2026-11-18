#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"

#include <nlohmann/json.hpp>

namespace blcad::gui {
namespace {
Error state_error(std::string message) {
  return Error::validation("gui.model_states", std::move(message));
}
} // namespace

const ModelStateId& GuiDocumentSession::active_model_state() const noexcept {
  return active_model_state_;
}
const PartDocument* GuiDocumentSession::effective_part_document() const noexcept {
  return effective_part_document_ ? &*effective_part_document_ : part_document();
}
const Project* GuiDocumentSession::effective_project() const noexcept {
  return effective_project_ ? &*effective_project_ : project();
}

Result<std::size_t> GuiDocumentSession::activate_model_state(ModelStateId state) {
  if (!has_document() || project_part_context_active() || reference_repair_mode_ ||
      task_.active() || part_workflow_base_ || history_prefix_count_)
    return Result<std::size_t>::failure(
        state_error("Apply or cancel the active task and return to the "
                    "complete model before switching states"));
  if (state == active_model_state_ && derived_results_fresh())
    return Result<std::size_t>::success(0);
  return rebuild_model_state_view(std::move(state));
}

Result<std::size_t> GuiDocumentSession::rebuild_model_state_view(ModelStateId state) {
  if (const auto* primary = part_document()) {
    auto evaluated = primary->evaluate_model_state(state);
    if (!evaluated)
      return Result<std::size_t>::failure(evaluated.error());
    auto cache = geometry::ShapeCache::create(ShapeCacheId("gui.part.state." + state.value()));
    if (!cache)
      return Result<std::size_t>::failure(cache.error());
    auto computed = recompute_part(evaluated.value(), cache.value());
    if (!computed)
      return computed;
    effective_part_document_ = state == primary_model_state_id()
                                   ? std::nullopt
                                   : std::optional<PartDocument>(std::move(evaluated.value()));
    effective_project_.reset();
    part_shape_cache_ = std::move(cache.value());
    project_part_caches_.clear();
    active_model_state_ = std::move(state);
    recompute_status_ = GuiRecomputeStatus::Fresh;
    selection_.clear();
    ++presentation_revision_;
    return computed;
  }
  const auto* primary = project();
  if (!primary)
    return Result<std::size_t>::failure(state_error("Model states require an active document"));
  auto assembly = primary->assembly().evaluate_model_state(state);
  if (!assembly)
    return Result<std::size_t>::failure(assembly.error());
  auto evaluated = *primary;
  evaluated.assembly() = std::move(assembly.value());
  if (state != primary_model_state_id()) {
    // Sibling state definitions belong to the authored root, not this evaluated
    // root. Only changed state parameters propagate through existing bindings.
    auto cleared = evaluated.assembly().replace_model_states({});
    if (!cleared)
      return cleared;
    for (const auto& binding : evaluated.assembly().bindings()) {
      const auto* before = primary->assembly().find_parameter(binding.assembly_parameter());
      const auto* after = evaluated.assembly().find_parameter(binding.assembly_parameter());
      if (before && after && before->value() != after->value()) {
        auto* part = evaluated.find_part_document(binding.part_document());
        if (!part)
          return Result<std::size_t>::failure(state_error("State binding Part is missing"));
        auto changed = part->set_parameter_value(binding.part_parameter(), after->value());
        if (!changed)
          return Result<std::size_t>::failure(changed.error());
      }
    }
  }
  if (active_position_ != master_position_id()) {
    if (!evaluated.find_assembly_position(active_position_))
      active_position_ = master_position_id();
    else {
      auto positioned = evaluated.evaluate_assembly_position(active_position_);
      if (!positioned)
        return Result<std::size_t>::failure(positioned.error());
      evaluated = std::move(positioned.value());
    }
  }
  if (active_design_view_ != default_design_view_id()) {
    if (!evaluated.find_assembly_design_view(active_design_view_))
      active_design_view_ = default_design_view_id();
    else {
      auto displayed = evaluated.evaluate_assembly_design_view(active_design_view_);
      if (!displayed)
        return Result<std::size_t>::failure(displayed.error());
      evaluated = std::move(displayed.value());
    }
  }
  auto caches = recompute_project(evaluated);
  if (!caches)
    return Result<std::size_t>::failure(caches.error());
  effective_project_ = state == primary_model_state_id() &&
                               active_position_ == master_position_id() &&
                               active_design_view_ == default_design_view_id()
                           ? std::nullopt
                           : std::optional<Project>(std::move(evaluated));
  effective_part_document_.reset();
  project_part_caches_ = std::move(caches.value());
  part_shape_cache_.reset();
  active_model_state_ = std::move(state);
  recompute_status_ = GuiRecomputeStatus::Fresh;
  selection_.clear();
  ++presentation_revision_;
  return Result<std::size_t>::success(project_part_caches_.size());
}

Result<std::uintmax_t>
GuiDocumentSession::save_model_state_member(const std::filesystem::path& path, ModelStateId state,
                                            DocumentId new_id, std::string new_name,
                                            std::optional<DocumentId> project_part) const {
  if (!has_document() || task_.active() || part_workflow_base_ || reference_repair_mode_)
    return Result<std::uintmax_t>::failure(
        state_error("Close the current task before exporting a member"));
  const auto* primary =
      project_part && project() ? project()->find_part_document(*project_part) : part_document();
  if (!primary || new_id.empty() || new_id == primary->id() || new_name.empty())
    return Result<std::uintmax_t>::failure(
        state_error("Choose a Part, a new document identity and a name"));
  if (project_part && !project())
    return Result<std::uintmax_t>::failure(
        state_error("Choose Project members from the root Assembly"));
  if (path.empty())
    return Result<std::uintmax_t>::failure(state_error("Member output path must not be empty"));
  std::error_code access;
  const auto output = std::filesystem::absolute(path, access).lexically_normal();
  if (access)
    return Result<std::uintmax_t>::failure(state_error("Member output cannot be resolved"));
  if (!file_path_.empty() &&
      output == std::filesystem::absolute(file_path_, access).lexically_normal())
    return Result<std::uintmax_t>::failure(
        state_error("A member copy must preserve the source document"));
  for (const auto& candidate :
       {output, std::filesystem::path(output.string() + ".sketch-constraints.json"),
        std::filesystem::path(output.string() + ".sketch-dimensions.json")}) {
    if (std::filesystem::exists(std::filesystem::symlink_status(candidate, access)))
      return Result<std::uintmax_t>::failure(
          state_error("A member copy requires new output and sidecar paths"));
  }
  std::optional<Project> evaluated_project;
  if (project_part) {
    auto imports = make_step_import_context(*this);
    if (!imports)
      return Result<std::uintmax_t>::failure(imports.error());
    auto evaluated =
        geometry::evaluate_project_geometry(*effective_project(), imports.value().get());
    if (!evaluated)
      return Result<std::uintmax_t>::failure(evaluated.error());
    evaluated_project = std::move(evaluated.value());
    primary = evaluated_project->find_part_document(*project_part);
    if (!primary)
      return Result<std::uintmax_t>::failure(
          state_error("The selected member Part no longer exists"));
  }
  auto member = primary->evaluate_model_state(state);
  if (!member)
    return Result<std::uintmax_t>::failure(member.error());
  auto cleared = member.value().replace_model_states({});
  if (!cleared)
    return Result<std::uintmax_t>::failure(cleared.error());
  auto serialized = serialize_part_document_to_json(member.value());
  if (!serialized)
    return Result<std::uintmax_t>::failure(serialized.error());
  auto json = nlohmann::json::parse(serialized.value());
  json["document"] = {{"id", new_id.value()}, {"name", std::move(new_name)}};
  auto independent = deserialize_part_document_from_json(json.dump());
  if (!independent)
    return Result<std::uintmax_t>::failure(independent.error());
  auto temporary = *this;
  temporary.document_ = std::move(independent.value());
  temporary.project_part_context_base_.reset();
  temporary.project_part_context_occurrence_.reset();
  temporary.project_part_context_scene_.reset();
  temporary.project_part_context_initial_.reset();
  temporary.project_part_context_transforms_.clear();
  temporary.project_part_sketch_catalogs_.clear();
  temporary.saved_project_part_sketch_catalogs_.reset();
  temporary.part_workflow_base_.reset();
  temporary.history_command_base_.reset();
  temporary.history_prefix_count_.reset();
  temporary.effective_part_document_.reset();
  temporary.effective_project_.reset();
  temporary.active_model_state_ = primary_model_state_id();
  temporary.active_position_ = master_position_id();
  temporary.active_design_view_ = default_design_view_id();
  (void)temporary.task_.cancel();
  temporary.workspace_ = GuiWorkspace::Part;
  if (project_part) {
    temporary.sketch_constraint_catalogs_.clear();
    temporary.sketch_dimension_catalogs_.clear();
    for (const auto& catalogs : project_part_sketch_catalogs_)
      if (catalogs.part == *project_part) {
        temporary.sketch_constraint_catalogs_ = catalogs.constraints;
        temporary.sketch_dimension_catalogs_ = catalogs.dimensions;
      }
  }
  for (auto& catalog : temporary.sketch_constraint_catalogs_) {
    auto rebound = SketchConstraintCatalog::create(new_id, catalog.sketch(), catalog.constraints());
    if (!rebound)
      return Result<std::uintmax_t>::failure(rebound.error());
    catalog = std::move(rebound.value());
  }
  for (auto& catalog : temporary.sketch_dimension_catalogs_) {
    auto rebound = SketchDimensionCatalog::create(new_id, catalog.sketch(), catalog.dimensions());
    if (!rebound)
      return Result<std::uintmax_t>::failure(rebound.error());
    catalog = std::move(rebound.value());
  }
  auto recomputed = temporary.recompute();
  if (!recomputed)
    return Result<std::uintmax_t>::failure(recomputed.error());
  // The existing Save As asset-root policy requires matching immutable STEP
  // assets at the destination. Pinned engineering resources remain embedded.
  return temporary.save_as(output);
}
} // namespace blcad::gui

namespace blcad::gui {
Result<std::size_t> GuiDocumentSession::commit_model_state_table(DocumentId owner,
                                                                 std::vector<ModelState> states) {
  if (!has_document() || project_part_context_active() || reference_repair_mode_ ||
      part_workflow_base_ || history_prefix_count_ ||
      (task_.active() && task_.command_id() != "model_states.manage"))
    return Result<std::size_t>::failure(
        state_error("Close the current workflow before editing model states"));
  auto candidate = *this;
  const auto selected = active_model_state_;
  const auto selected_position = active_position_;
  const auto selected_view = active_design_view_;
  candidate.active_position_ = master_position_id();
  candidate.active_design_view_ = default_design_view_id();
  candidate.active_model_state_ = primary_model_state_id();
  candidate.effective_part_document_.reset();
  candidate.effective_project_.reset();
  auto primary = candidate.recompute();
  if (!primary)
    return primary;
  const bool part_owner = candidate.part_document() ||
                          (candidate.project() && candidate.project()->find_part_document(owner));
  auto changed =
      candidate.part_document()
          ? candidate.commit_part_transaction("Manage Model States",
                                              [&](PartDocument& part) {
                                                if (owner != part.id())
                                                  return Result<std::size_t>::failure(
                                                      state_error("The state owner changed"));
                                                return part.replace_model_states(states);
                                              })
          : candidate.commit_project_transaction("Manage Model States", [&](Project& project) {
              if (auto* part = project.find_part_document(owner))
                return part->replace_model_states(states);
              if (owner == project.assembly().id())
                return project.assembly().replace_model_states(states);
              return Result<std::size_t>::failure(
                  state_error("Choose a Part or the root Assembly"));
            });
  if (!changed)
    return changed;
  if (part_owner) {
    const auto* part = candidate.part_document() ? candidate.part_document()
                                                 : candidate.project()->find_part_document(owner);
    for (const auto& state : part->model_states()) {
      auto effective = part->evaluate_model_state(state.id);
      if (!effective)
        return Result<std::size_t>::failure(effective.error());
      auto cache =
          geometry::ShapeCache::create(ShapeCacheId("gui.state.table." + state.id.value()));
      if (!cache)
        return Result<std::size_t>::failure(cache.error());
      auto computed = candidate.recompute_part(effective.value(), cache.value());
      if (!computed)
        return computed;
    }
  } else {
    for (const auto& state : states) {
      auto computed = candidate.rebuild_model_state_view(state.id);
      if (!computed)
        return computed;
    }
  }
  candidate.active_position_ = selected_position;
  candidate.active_design_view_ = selected_view;
  auto restored = candidate.rebuild_model_state_view(selected);
  if (!restored)
    return Result<std::size_t>::failure(
        state_error("The active state would become unavailable or invalid. "
                    "Return to Primary before deleting it. " +
                    restored.error().message()));
  *this = std::move(candidate);
  return changed;
}
} // namespace blcad::gui
