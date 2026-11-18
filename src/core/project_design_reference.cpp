#include "blcad/core/project_design_reference.hpp"
#include "blcad/core/project.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <type_traits>

namespace blcad {
namespace {
Error error(std::string message) {
  return Error::validation("project.design_reference", std::move(message));
}
bool occurrence_valid(const ProjectDesignOccurrence& value) {
  return !value.component.empty() && !value.part.empty() &&
         std::all_of(value.path.begin(), value.path.end(),
                     [](const auto& id) { return !id.empty(); });
}
Result<std::size_t> resolve(const Project& project, const ProjectDesignOccurrence& occurrence) {
  const AssemblyDocument* assembly = &project.assembly();
  auto effective = assembly->evaluate_model_state(assembly->effective_model_state());
  if (!effective)
    return Result<std::size_t>::failure(effective.error());
  for (const auto& segment : occurrence.path) {
    const auto* instance = effective.value().find_subassembly_instance(segment);
    if (!instance || instance->suppression_state() != ComponentSuppressionState::Active)
      return Result<std::size_t>::failure(error("Design occurrence path is missing or suppressed"));
    const auto selected = instance->selected_model_state();
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (!assembly)
      return Result<std::size_t>::failure(error("Design occurrence child is missing"));
    effective = assembly->evaluate_model_state(selected);
    if (!effective)
      return Result<std::size_t>::failure(effective.error());
  }
  const auto* component = effective.value().find_component_instance(occurrence.component);
  if (!component || component->referenced_part_document() != occurrence.part ||
      component->suppression_state() != ComponentSuppressionState::Active)
    return Result<std::size_t>::failure(
        error("Design component identity changed or is suppressed"));
  return Result<std::size_t>::success(1);
}
Result<std::size_t> validate(const Project& project, const ProjectDesignReference& ref) {
  auto source = resolve(project, ref.source());
  if (!source)
    return source;
  auto driver = resolve(project, ref.driver());
  if (!driver)
    return driver;
  const auto* source_part = project.find_part_document(ref.source().part);
  const auto* target_part = project.find_part_document(ref.driver().part);
  if (!source_part || !target_part || target_part->is_reference_part())
    return Result<std::size_t>::failure(
        error("Adaptive targets require an owned editable Part definition"));
  auto state = source_part->evaluate_model_state(ref.source_state());
  if (!state)
    return Result<std::size_t>::failure(state.error());
  if (const auto* path = std::get_if<ProjectDesignPathSource>(&ref.source_geometry()))
    if (!state.value().find_path_curve(path->path))
      return Result<std::size_t>::failure(
          error("Source PathCurve does not exist in the selected state"));
  const bool exists = std::visit(
      [&](const auto& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, ConstructionPointId>)
          return target_part->find_construction_point(target) != nullptr;
        else if constexpr (std::is_same_v<T, ConstructionLineId>)
          return target_part->find_construction_line(target) != nullptr;
        else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
          return target_part->find_construction_plane(target) != nullptr;
        else
          return target_part->find_path_curve(target.path) &&
                 target_part->find_sketch_3d(target.sketch);
      },
      ref.target());
  if (!exists)
    return Result<std::size_t>::failure(
        error("Adaptive target construction is missing; repair explicitly"));
  return Result<std::size_t>::success(1);
}
} // namespace

ProjectDesignReference::ProjectDesignReference(ProjectDesignReferenceId id, std::string name,
                                               ProjectDesignOccurrence source, ModelStateId state,
                                               ProjectDesignSource geometry,
                                               ProjectDesignOccurrence driver,
                                               ProjectDesignTarget target)
    : id_(std::move(id)), name_(std::move(name)), source_(std::move(source)),
      source_state_(std::move(state)), source_geometry_(std::move(geometry)),
      driver_(std::move(driver)), target_(std::move(target)) {}
Result<ProjectDesignReference>
ProjectDesignReference::create(ProjectDesignReferenceId id, std::string name,
                               ProjectDesignOccurrence source, ModelStateId state,
                               ProjectDesignSource geometry, ProjectDesignOccurrence driver,
                               ProjectDesignTarget target) {
  using R = Result<ProjectDesignReference>;
  if (id.empty() || name.empty() || !occurrence_valid(source) || !occurrence_valid(driver) ||
      state.empty())
    return R::failure(error("Design link requires stable identities, source "
                            "state and explicit driver"));
  if (source.part == driver.part)
    return R::failure(error("Cross-Part links cannot consume their target definition"));
  const bool path = std::holds_alternative<ProjectDesignPathSource>(geometry);
  if (path != std::holds_alternative<ProjectDerivedPathTarget>(target))
    return R::failure(error("PathCurve links require a typed derived PathCurve target"));
  if (path && std::get<ProjectDesignPathSource>(geometry).path.empty())
    return R::failure(error("Source path identity is empty"));
  if (!path) {
    const auto& spelling = std::get<ProjectDesignSemanticSource>(geometry).reference;
    auto endpoint =
        AssemblyHierarchyConstraintEndpoint::create(source.path, source.component, spelling);
    if (!endpoint)
      return R::failure(endpoint.error());
  }
  const bool target_valid = std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ProjectDerivedPathTarget>)
          return !value.path.empty() && !value.sketch.empty();
        else
          return !value.empty();
      },
      target);
  if (!target_valid)
    return R::failure(error("Target construction identity is empty"));
  return R::success(ProjectDesignReference(std::move(id), std::move(name), std::move(source),
                                           std::move(state), std::move(geometry), std::move(driver),
                                           std::move(target)));
}
std::string ProjectDesignReference::target_key() const {
  return std::visit(
      [](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ConstructionPointId>)
          return "point/" + value.value();
        else if constexpr (std::is_same_v<T, ConstructionLineId>)
          return "line/" + value.value();
        else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
          return "plane/" + value.value();
        else
          return "path/" + value.path.value();
      },
      target_);
}

Result<std::vector<DocumentId>> project_design_dependency_order(
    const Project& project,
    const std::vector<std::pair<DocumentId, DocumentId>>& additional_dependencies) {
  std::map<std::string, std::set<std::string>> edges;
  for (const auto& [source, target] : additional_dependencies) {
    if (!project.find_part_document(source) || !project.find_part_document(target))
      return Result<std::vector<DocumentId>>::failure(
          error("Additional Project dependency must resolve to owned Parts"));
    edges[source.value()].insert(target.value());
    edges.try_emplace(target.value());
  }
  for (const auto& ref : project.design_references()) {
    auto valid = validate(project, ref);
    if (!valid)
      return Result<std::vector<DocumentId>>::failure(valid.error());
    edges[ref.source().part.value()].insert(ref.driver().part.value());
    edges.try_emplace(ref.driver().part.value());
  }
  std::map<std::string, int> state;
  std::vector<DocumentId> result;
  std::function<bool(const std::string&)> visit = [&](const std::string& id) {
    if (state[id] == 1)
      return false;
    if (state[id] == 2)
      return true;
    state[id] = 1;
    for (const auto& next : edges[id])
      if (!visit(next))
        return false;
    state[id] = 2;
    result.emplace_back(id);
    return true;
  };
  for (const auto& [id, _] : edges)
    if (!visit(id))
      return Result<std::vector<DocumentId>>::failure(
          error("Directed cross-Part parameter/geometry dependency cycle"));
  std::reverse(result.begin(), result.end());
  return Result<std::vector<DocumentId>>::success(std::move(result));
}

const ProjectDesignReference*
Project::find_design_reference(ProjectDesignReferenceId id) const noexcept {
  const auto it = std::find_if(design_references_.begin(), design_references_.end(),
                               [&](const auto& value) { return value.id() == id; });
  return it == design_references_.end() ? nullptr : &*it;
}
Result<std::size_t> Project::set_design_reference(ProjectDesignReference ref) {
  Project candidate = *this;
  auto it = std::find_if(candidate.design_references_.begin(), candidate.design_references_.end(),
                         [&](const auto& value) { return value.id() == ref.id(); });
  if (it == candidate.design_references_.end())
    candidate.design_references_.push_back(std::move(ref));
  else
    *it = std::move(ref);
  auto valid = candidate.validate_design_references();
  if (!valid)
    return valid;
  auto machining = validate_assembly_hole_features(candidate);
  if (!machining)
    return machining;
  std::sort(candidate.design_references_.begin(), candidate.design_references_.end(),
            [](const auto& a, const auto& b) { return a.id().value() < b.id().value(); });
  *this = std::move(candidate);
  return Result<std::size_t>::success(1);
}
Result<std::size_t> Project::detach_design_reference(ProjectDesignReferenceId id) {
  const auto count =
      std::erase_if(design_references_, [&](const auto& ref) { return ref.id() == id; });
  if (!count)
    return Result<std::size_t>::failure(error("Design reference does not exist"));
  return Result<std::size_t>::success(count);
}
Result<std::size_t> Project::validate_design_references() const {
  std::set<std::pair<std::string, std::string>> targets;
  std::map<std::string, ProjectDesignOccurrence> drivers;
  for (const auto& ref : design_references_) {
    if (!targets.emplace(ref.driver().part.value(), ref.target_key()).second)
      return Result<std::size_t>::failure(error("One construction cannot have two design drivers"));
    auto [it, inserted] = drivers.emplace(ref.driver().part.value(), ref.driver());
    if (!inserted && it->second != ref.driver())
      return Result<std::size_t>::failure(
          error("A shared adaptive Part requires one explicit driving occurrence"));
  }
  auto order = project_design_dependency_order(*this);
  if (!order)
    return Result<std::size_t>::failure(order.error());
  return Result<std::size_t>::success(design_references_.size());
}
} // namespace blcad
