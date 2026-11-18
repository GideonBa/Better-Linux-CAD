#include "blcad/gui/gui_document_session.hpp"

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/gui/gui_step_import_context.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <type_traits>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("gui.project_part_context", std::move(message));
}
using json = nlohmann::json;
json record(const json& source, const char* collection, const std::string& id) {
  for (const auto& value : source.value(collection, json::array()))
    if (value.is_object() && value.value("id", std::string{}) == id)
      return value;
  return nullptr;
}
} // namespace
bool GuiDocumentSession::project_part_context_active() const noexcept {
  return bool(project_part_context_base_);
}
const Project* GuiDocumentSession::project_part_context_root() const noexcept {
  return project_part_context_scene_ ? &*project_part_context_scene_ : nullptr;
}
const std::optional<ProjectDesignOccurrence>&
GuiDocumentSession::context_occurrence() const noexcept {
  return project_part_context_occurrence_;
}
const std::vector<RigidTransform>&
GuiDocumentSession::project_part_context_transforms() const noexcept {
  return project_part_context_transforms_;
}
Result<std::size_t>
GuiDocumentSession::begin_project_part_context(ProjectDesignOccurrence occurrence) {
  if (!project() || project_part_context_base_ || part_workflow_base_ || task_.active() ||
      history_prefix_count_ || reference_repair_mode_ || !derived_results_fresh() ||
      active_model_state() != primary_model_state_id() ||
      active_position() != master_position_id() || active_design_view() != default_design_view_id())
    return Result<std::size_t>::failure(error("Edit Part requires an idle complete Project in "
                                              "Primary, Master position and Default design view"));
  auto imports = make_step_import_context(*this);
  if (!imports)
    return Result<std::size_t>::failure(imports.error());
  auto normalized = geometry::evaluate_project_geometry(*project(), imports.value().get());
  if (!normalized)
    return Result<std::size_t>::failure(normalized.error());
  auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(normalized.value(),
                                                         AssemblyLeafParticipation::PhysicalActive);
  if (!leaves)
    return Result<std::size_t>::failure(leaves.error());
  const auto found =
      std::find_if(leaves.value().begin(), leaves.value().end(), [&](const auto& leaf) {
        return leaf.subassembly_path == occurrence.path &&
               leaf.component_instance == occurrence.component &&
               leaf.referenced_part_document == occurrence.part;
      });
  if (found == leaves.value().end() || found->selected_model_state != primary_model_state_id())
    return Result<std::size_t>::failure(
        error("Choose an active rooted occurrence using its Primary Part state"));
  const auto* part = normalized.value().find_part_document(occurrence.part);
  if (!part || part->is_reference_part())
    return Result<std::size_t>::failure(
        error("Convert the Reference Part explicitly before in-context editing"));
  auto candidate = *this;
  candidate.project_part_context_base_ = std::make_shared<const GuiDocumentSession>(*this);
  candidate.project_part_context_occurrence_ = std::move(occurrence);
  candidate.project_part_context_initial_ = *part;
  candidate.project_part_context_transforms_ = found->transforms_inner_to_outer;
  candidate.document_ = *part;
  candidate.project_part_context_scene_ = std::move(normalized.value());
  candidate.display_name_ = candidate.part_document()->name();
  candidate.workspace_ = GuiWorkspace::Part;
  candidate.effective_part_document_.reset();
  candidate.effective_project_.reset();
  candidate.project_part_caches_.clear();
  candidate.sketch_constraint_catalogs_.clear();
  candidate.sketch_dimension_catalogs_.clear();
  for (const auto& catalogs : project_part_sketch_catalogs_)
    if (catalogs.part == candidate.part_document()->id()) {
      candidate.sketch_constraint_catalogs_ = catalogs.constraints;
      candidate.sketch_dimension_catalogs_ = catalogs.dimensions;
    }
  candidate.project_part_sketch_catalogs_.clear();
  candidate.saved_project_part_sketch_catalogs_.reset();
  auto computed = candidate.recompute();
  if (!computed)
    return computed;
  auto source = candidate.serialize_current();
  if (!source)
    return Result<std::size_t>::failure(source.error());
  candidate.initialize_history(source.value(), "Enter Part in Assembly", true);
  candidate.selection_.clear();
  const auto next = std::max(model_revision(), candidate.model_revision()) + 1;
  candidate.visibility_revision_ = std::max(visibility_revision_, candidate.visibility_revision_);
  candidate.presentation_revision_ = candidate.visibility_revision_ + next;
  *this = std::move(candidate);
  return computed;
}
Result<std::size_t>
GuiDocumentSession::validate_project_part_context_mutation(const PartDocument& candidate) const {
  if (!project_part_context_base_)
    return Result<std::size_t>::success(0);
  if (!project_part_context_initial_ || candidate.id() != project_part_context_initial_->id() ||
      candidate.mode() != project_part_context_initial_->mode())
    return Result<std::size_t>::failure(
        error("In-context editing must retain the selected editable Part identity"));
  auto before = serialize_part_document_to_json(*project_part_context_initial_);
  if (!before)
    return Result<std::size_t>::failure(before.error());
  auto after = serialize_part_document_to_json(candidate);
  if (!after)
    return Result<std::size_t>::failure(after.error());
  const auto original = json::parse(before.value()), changed = json::parse(after.value());
  const auto& root = *project_part_context_base_->project();
  std::vector<const AssemblyDocument*> assemblies{&root.assembly()};
  for (const auto& assembly : root.child_assembly_documents())
    assemblies.push_back(&assembly);
  for (const auto* assembly : assemblies)
    for (const auto& binding : assembly->bindings())
      if (binding.part_document() == candidate.id() &&
          record(original, "parameters", binding.part_parameter().value()) !=
              record(changed, "parameters", binding.part_parameter().value()))
        return Result<std::size_t>::failure(
            error("This parameter is bound to an Assembly parameter. Edit its Assembly driver at "
                  "the root"));
  for (const auto& link : root.design_references()) {
    if (link.driver().part != candidate.id())
      continue;
    const bool same = std::visit(
        [&](const auto& target) {
          using T = std::decay_t<decltype(target)>;
          if constexpr (std::is_same_v<T, ConstructionPointId>)
            return record(original, "construction_points", target.value()) ==
                   record(changed, "construction_points", target.value());
          else if constexpr (std::is_same_v<T, ConstructionLineId>)
            return record(original, "construction_lines", target.value()) ==
                   record(changed, "construction_lines", target.value());
          else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
            return record(original, "construction_planes", target.value()) ==
                   record(changed, "construction_planes", target.value());
          else
            return record(original, "path_curves", target.path.value()) ==
                       record(changed, "path_curves", target.path.value()) &&
                   record(original, "sketches_3d", target.sketch.value()) ==
                       record(changed, "sketches_3d", target.sketch.value());
        },
        link.target());
    if (!same)
      return Result<std::size_t>::failure(
          error("This construction is linked. Edit its Assembly source or detach the reference "
                "explicitly at the root"));
  }
  for (const auto& feature : root.assembly_hole_features())
    for (const auto& participant : feature.participants()) {
      if (participant.occurrence.part != candidate.id())
        continue;
      auto a = record(original, "hole_features", participant.recipe.id().value());
      auto b = record(changed, "hole_features", participant.recipe.id().value());
      if (a.is_object())
        a.erase("placements");
      if (b.is_object())
        b.erase("placements");
      if (a != b)
        return Result<std::size_t>::failure(error(
            "This Hole recipe is owned by Assembly Hole. Edit its participant recipe at the root"));
    }
  return Result<std::size_t>::success(1);
}
Result<std::size_t> GuiDocumentSession::finish_project_part_context(std::string label) {
  if (!project_part_context_base_ || !project_part_context_occurrence_ || !part_document() ||
      label.empty() || task_.active() || part_workflow_base_ || history_prefix_count_ ||
      active_model_state() != primary_model_state_id() || !derived_results_fresh())
    return Result<std::size_t>::failure(
        error("Apply or cancel the current Part command before returning to the Assembly"));
  auto owned = validate_project_part_context_mutation(*part_document());
  if (!owned)
    return owned;
  auto valid_cache = part_shape_cache_
                         ? part_shape_cache_->validate_complete_geometry_result(*part_document())
                         : Result<std::size_t>::failure(error("The Part result is unavailable"));
  if (!valid_cache)
    return valid_cache;
  auto source = serialize_current();
  if (!source)
    return Result<std::size_t>::failure(source.error());
  const auto& initial = history_.front();
  const bool changed = source.value() != initial.json ||
                       sketch_constraint_catalogs_ != initial.sketch_constraints ||
                       sketch_dimension_catalogs_ != initial.sketch_dimensions;
  auto accepted = *project_part_context_base_;
  if (changed) {
    const auto owner = project_part_context_occurrence_->part;
    std::erase_if(accepted.project_part_sketch_catalogs_,
                  [&](const auto& catalogs) { return catalogs.part == owner; });
    if (!sketch_constraint_catalogs_.empty() || !sketch_dimension_catalogs_.empty())
      accepted.project_part_sketch_catalogs_.push_back(
          {owner, sketch_constraint_catalogs_, sketch_dimension_catalogs_});
    std::sort(accepted.project_part_sketch_catalogs_.begin(),
              accepted.project_part_sketch_catalogs_.end(),
              [](const auto& a, const auto& b) { return a.part.value() < b.part.value(); });
    const auto part = *part_document();
    auto published = accepted.commit_project_transaction(std::move(label), [&](Project& project) {
      auto* target = project.find_part_document(owner);
      if (!target || target->is_reference_part())
        return Result<std::size_t>::failure(error("The selected Part owner is unavailable"));
      *target = part;
      return Result<std::size_t>::success(1);
    });
    if (!published)
      return published;
  }
  const auto next = std::max(model_revision(), accepted.model_revision()) + 1;
  accepted.visibility_revision_ = std::max(visibility_revision_, accepted.visibility_revision_);
  accepted.presentation_revision_ = accepted.visibility_revision_ + next;
  *this = std::move(accepted);
  return Result<std::size_t>::success(changed ? 1 : 0);
}
void GuiDocumentSession::cancel_project_part_context() {
  if (!project_part_context_base_)
    return;
  auto accepted = *project_part_context_base_;
  const auto next = std::max(model_revision(), accepted.model_revision()) + 1;
  accepted.visibility_revision_ = std::max(visibility_revision_, accepted.visibility_revision_);
  accepted.presentation_revision_ = accepted.visibility_revision_ + next;
  *this = std::move(accepted);
}
} // namespace blcad::gui
