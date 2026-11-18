#include "blcad/core/assembly_hole_feature.hpp"
#include "blcad/core/project.hpp"
#include "hole_recipe_json.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace blcad {
namespace {
Error error(const std::string& id, const char* message) {
  return Error::validation(id, message);
}
std::string key(const ProjectDesignOccurrence& source) {
  std::string result;
  const auto append = [&](const std::string& value) {
    result += std::to_string(value.size()) + ":" + value + "/";
  };
  append(source.part.value());
  for (const auto& path : source.path)
    append(path.value());
  append(source.component.value());
  return result;
}
bool valid_occurrence(const ProjectDesignOccurrence& o) {
  return !o.component.empty() && !o.part.empty() &&
         std::none_of(o.path.begin(), o.path.end(), [](const auto& id) { return id.empty(); });
}
} // namespace
AssemblyHoleFeature::AssemblyHoleFeature(FeatureId id, std::string name, AssemblyHoleLayout layout,
                                         std::vector<AssemblyHoleParticipant> participants)
    : id_(std::move(id)), name_(std::move(name)), layout_(std::move(layout)),
      participants_(std::move(participants)) {}
Result<AssemblyHoleFeature>
AssemblyHoleFeature::create(FeatureId id, std::string name, AssemblyHoleLayout layout,
                            std::vector<AssemblyHoleParticipant> participants) {
  const auto fail = [&](const char* message) {
    return Result<AssemblyHoleFeature>::failure(error(id.value(), message));
  };
  if (id.empty() || name.empty() || !valid_occurrence(layout.source) || layout.state.empty() ||
      layout.hole.empty() || (layout.pattern && layout.pattern->empty()) || participants.empty())
    return fail("Assembly Hole requires identity, shared layout and explicit participants");
  std::set<std::string> ids, occurrences, definitions;
  for (const auto& participant : participants) {
    if (participant.id.empty() || !valid_occurrence(participant.occurrence) ||
        participant.state.empty() || !ids.insert(participant.id).second ||
        !occurrences.insert(key(participant.occurrence)).second ||
        !definitions.insert(participant.occurrence.part.value()).second)
      return fail("Assembly Hole participants and definition changes must be unique; select one "
                  "explicit driver per shared definition");
    if (participant.definition_policy != AssemblyHoleDefinitionPolicy::SingleOccurrence &&
        participant.definition_policy != AssemblyHoleDefinitionPolicy::AffectSharedDefinition)
      return fail("Assembly Hole requires an explicit shared-definition policy");
    if ((participant.role == AssemblyHoleParticipantRole::Threaded &&
         !participant.recipe.thread()) ||
        (participant.role == AssemblyHoleParticipantRole::Clearance &&
         participant.recipe.thread()) ||
        (participant.role == AssemblyHoleParticipantRole::Seat &&
         std::holds_alternative<std::monostate>(participant.recipe.manufacturing())) ||
        (participant.role != AssemblyHoleParticipantRole::Clearance &&
         participant.role != AssemblyHoleParticipantRole::Threaded &&
         participant.role != AssemblyHoleParticipantRole::Seat))
      return fail("Assembly Hole participant recipe does not match its declared role");
  }
  return Result<AssemblyHoleFeature>::success(AssemblyHoleFeature(
      std::move(id), std::move(name), std::move(layout), std::move(participants)));
}
Result<std::size_t> validate_assembly_hole_features(const Project& project) {
  if (project.assembly_hole_features().empty())
    return Result<std::size_t>::success(0);
  struct Leaf {
    ProjectDesignOccurrence occurrence;
    ModelStateId state;
  };
  std::vector<Leaf> leaves;
  std::set<std::string> ancestry;
  std::function<Result<std::size_t>(const AssemblyDocument&, ModelStateId,
                                    std::vector<SubassemblyInstanceId>)>
      collect;
  collect = [&](const AssemblyDocument& assembly, ModelStateId state,
                std::vector<SubassemblyInstanceId> path) -> Result<std::size_t> {
    if (!ancestry.insert(assembly.id().value()).second)
      return Result<std::size_t>::failure(
          error(assembly.id().value(), "Assembly Hole hierarchy is cyclic"));
    auto effective = assembly.evaluate_model_state(state);
    if (!effective)
      return Result<std::size_t>::failure(effective.error());
    for (const auto& component : effective.value().component_instances())
      if (component.suppression_state() == ComponentSuppressionState::Active)
        leaves.push_back({{path, component.id(), component.referenced_part_document()},
                          component.selected_model_state()});
    for (const auto& child : effective.value().subassembly_instances()) {
      if (child.suppression_state() != ComponentSuppressionState::Active)
        continue;
      const auto* definition = project.find_assembly_document(child.referenced_assembly_document());
      if (!definition)
        return Result<std::size_t>::failure(
            error(child.id().value(), "Assembly Hole child definition is missing"));
      auto nested = path;
      nested.push_back(child.id());
      auto added = collect(*definition, child.selected_model_state(), std::move(nested));
      if (!added)
        return added;
    }
    ancestry.erase(assembly.id().value());
    return Result<std::size_t>::success(leaves.size());
  };
  auto collected = collect(project.assembly(), project.assembly().effective_model_state(), {});
  if (!collected)
    return collected;
  const auto resolved = [&](const ProjectDesignOccurrence& o, const ModelStateId& state) {
    return std::find_if(leaves.begin(), leaves.end(), [&](const auto& leaf) {
             return leaf.occurrence == o && leaf.state == state;
           }) != leaves.end();
  };
  std::set<std::string> ids;
  std::map<std::pair<std::string, std::string>, std::size_t> owners;
  for (std::size_t index = 0; index < project.assembly_hole_features().size(); ++index) {
    const auto& feature = project.assembly_hole_features()[index];
    if (!ids.insert(feature.id().value()).second)
      return Result<std::size_t>::failure(
          error(feature.id().value(), "Assembly Hole identity is duplicated"));
    for (const auto& participant : feature.participants())
      if (!owners
               .emplace(
                   std::pair{participant.occurrence.part.value(), participant.recipe.id().value()},
                   index)
               .second)
        return Result<std::size_t>::failure(
            error(feature.id().value(), "Two Assembly Holes cannot own the same Part Hole"));
  }
  for (std::size_t index = 0; index < project.assembly_hole_features().size(); ++index) {
    const auto& feature = project.assembly_hole_features()[index];
    const auto& layout = feature.layout();
    const auto fail = [&](const char* message) {
      return Result<std::size_t>::failure(error(feature.id().value(), message));
    };
    if (!resolved(layout.source, layout.state))
      return fail("Assembly Hole layout occurrence/state is suppressed, missing or changed");
    const auto* source = project.find_part_document(layout.source.part);
    if (!source)
      return fail("Assembly Hole layout Part definition is missing");
    auto effective = source->evaluate_model_state(layout.state);
    if (!effective)
      return Result<std::size_t>::failure(effective.error());
    if (!effective.value().find_hole_feature(layout.hole))
      return fail("Assembly Hole layout Hole is unavailable");
    auto source_plan = effective.value().create_history_evaluation_plan();
    if (!source_plan)
      return Result<std::size_t>::failure(source_plan.error());
    const auto active = [](const PartHistoryEvaluationPlan& plan, const FeatureId& id) {
      return std::any_of(plan.entries.begin(), plan.entries.end(), [&](const auto& row) {
        return row.feature.node_id == id.value() && row.state == PartHistoryExecutionState::Active;
      });
    };
    if (!active(source_plan.value(), layout.hole) ||
        (layout.pattern && !active(source_plan.value(), *layout.pattern)))
      return fail("Assembly Hole layout Hole/pattern is suppressed or blocked");
    if (layout.pattern && !effective.value().find_linear_pattern_feature(*layout.pattern) &&
        !effective.value().find_circular_pattern_feature(*layout.pattern))
      return fail("Assembly Hole layout pattern is unavailable");
    if (layout.pattern) {
      const auto* linear = effective.value().find_linear_pattern_feature(*layout.pattern);
      const auto* circular = effective.value().find_circular_pattern_feature(*layout.pattern);
      const auto& sources = linear ? linear->sources() : circular->sources();
      if (sources.size() != 1 || !std::holds_alternative<FeatureId>(sources.front().source()) ||
          std::get<FeatureId>(sources.front().source()) != layout.hole)
        return fail("Assembly Hole layout pattern must explicitly copy its shared Hole feature");
    }
    auto prior = owners.find({layout.source.part.value(), layout.hole.value()});
    // The cover's original layout Hole may also be the same command's explicit participant.
    if (prior != owners.end() && prior->second > index)
      return fail("Assembly Hole layout depends on a later coordinated feature");
    for (const auto& participant : feature.participants()) {
      if (participant.occurrence.part == layout.source.part &&
          participant.recipe.id() != layout.hole)
        return fail("The source Part participant must reuse the declared layout Hole");
      if (!resolved(participant.occurrence, participant.state))
        return fail("Assembly Hole participant occurrence/state is suppressed, missing or changed");
      const auto* part = project.find_part_document(participant.occurrence.part);
      if (!part || part->is_reference_part())
        return fail("Assembly Hole requires explicitly editable participant definitions");
      const auto count = std::count_if(leaves.begin(), leaves.end(), [&](const auto& leaf) {
        return leaf.occurrence.part == part->id();
      });
      if (count > 1 &&
          participant.definition_policy != AssemblyHoleDefinitionPolicy::AffectSharedDefinition)
        return fail("Shared Part requires an explicit affect-shared-definition action or "
                    "independent conversion");
      if (const auto* current = part->find_hole_feature(participant.recipe.id())) {
        auto actual = detail::shared_hole_to_json(*current);
        auto declared = detail::shared_hole_to_json(participant.recipe);
        actual.erase("placements");
        declared.erase("placements");
        if (actual != declared)
          return fail(
              "Participant Hole recipe is owned by Assembly Hole; edit the coordinated command");
      }
      auto target = part->evaluate_model_state(participant.state);
      if (!target)
        return Result<std::size_t>::failure(target.error());
      if (target.value().find_hole_feature(participant.recipe.id())) {
        auto target_plan = target.value().create_history_evaluation_plan();
        if (!target_plan)
          return Result<std::size_t>::failure(target_plan.error());
        if (!active(target_plan.value(), participant.recipe.id()))
          return fail("Assembly Hole participant feature is suppressed or blocked");
      }
      auto valid = validate_hole_feature_inputs(target.value(), participant.recipe);
      if (!valid)
        return valid;
    }
  }
  std::vector<std::pair<DocumentId, DocumentId>> edges;
  for (const auto& feature : project.assembly_hole_features())
    for (const auto& participant : feature.participants())
      if (feature.layout().source.part != participant.occurrence.part)
        edges.emplace_back(feature.layout().source.part, participant.occurrence.part);
  auto order = project_design_dependency_order(project, edges);
  if (!order)
    return Result<std::size_t>::failure(order.error());
  return Result<std::size_t>::success(project.assembly_hole_features().size());
}
Result<std::size_t>
Project::replace_assembly_hole_features(std::vector<AssemblyHoleFeature> features) {
  auto candidate = *this;
  candidate.assembly_hole_features_ = std::move(features);
  auto valid = validate_assembly_hole_features(candidate);
  if (!valid)
    return valid;
  assembly_hole_features_ = std::move(candidate.assembly_hole_features_);
  return valid;
}
} // namespace blcad
