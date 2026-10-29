#include "blcad/geometry/assembly_import_validation.hpp"

#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "assembly_cross_hierarchy_motion_numeric_system.hpp"

#include <algorithm>
#include <set>
#include <string>

namespace blcad::geometry {
namespace {
struct Endpoint {
  const PartDocument* part{};
  bool active{};
  std::string semantic;
};

Endpoint endpoint(const Project& project, const AssemblyDocument& assembly,
                  const AssemblyConstraintTarget& target) {
  const auto* component = assembly.find_component_instance(target.component_instance());
  if (!component)
    return {};
  return {project.find_part_document(component->referenced_part_document()),
          component->suppression_state() == ComponentSuppressionState::Active,
          target.semantic_reference()};
}

Endpoint endpoint(const Project& project, const AssemblyHierarchyConstraintEndpoint& target) {
  const auto* assembly = &project.assembly();
  for (const auto& occurrence : target.occurrence_path()) {
    const auto* instance = assembly->find_subassembly_instance(occurrence);
    if (!instance || instance->suppression_state() != ComponentSuppressionState::Active)
      return {};
    assembly = project.find_assembly_document(instance->referenced_assembly_document());
    if (!assembly)
      return {};
  }
  auto local =
      AssemblyConstraintTarget::create(target.component_instance(), target.semantic_reference());
  return local ? endpoint(project, *assembly, local.value()) : Endpoint{};
}

bool imported(const Endpoint& value) {
  return value.part &&
         (value.part->is_reference_part() || !value.part->imported_body_features().empty() ||
          is_imported_topology_target_spelling(value.semantic));
}
} // namespace

Result<std::size_t>
validate_imported_assembly_inputs(const Project& project, StepImportTopologyContext* context,
                                  const std::vector<DocumentId>& participating_parts) {
  const auto selected = [&](const PartDocument* part) {
    return part && (participating_parts.empty() ||
                    std::find(participating_parts.begin(), participating_parts.end(), part->id()) !=
                        participating_parts.end());
  };
  const auto relevant = [&](const Endpoint& first, const Endpoint& second) {
    return first.active && second.active && selected(first.part) && selected(second.part) &&
           (imported(first) || imported(second));
  };
  AssemblyCrossHierarchyMotionGroup relationships;
  const auto append_local = [&](const AssemblyDocument& assembly) {
    for (const auto& constraint : assembly.constraints())
      if (constraint.state() == AssemblyConstraintState::Active &&
          relevant(endpoint(project, assembly, constraint.target_a()),
                   endpoint(project, assembly, constraint.target_b())))
        relationships.relationships.emplace_back(
            AssemblyLocalRelationshipIdentity{assembly.id(), constraint.id()});
    for (const auto& joint : assembly.joints())
      if (joint.state() == AssemblyJointState::Active &&
          relevant(endpoint(project, assembly, joint.target_a()),
                   endpoint(project, assembly, joint.target_b())))
        relationships.relationships.emplace_back(
            AssemblyLocalJointIdentity{assembly.id(), joint.id()});
  };
  append_local(project.assembly());
  for (const auto& child : project.child_assembly_documents())
    append_local(child);
  for (const auto& constraint : project.cross_hierarchy_constraints())
    if (constraint.state() == AssemblyConstraintState::Active &&
        relevant(endpoint(project, constraint.target_a()),
                 endpoint(project, constraint.target_b())))
      relationships.relationships.emplace_back(
          AssemblyProjectCrossHierarchyRelationshipIdentity{constraint.id()});
  for (const auto& joint : project.cross_hierarchy_joints())
    if (joint.state() == AssemblyJointState::Active &&
        relevant(endpoint(project, joint.target_a()), endpoint(project, joint.target_b())))
      relationships.relationships.emplace_back(
          AssemblyProjectCrossHierarchyJointIdentity{joint.id()});
  const bool has_imported_result = std::any_of(
      project.part_documents().begin(), project.part_documents().end(), [&](const auto& part) {
        return selected(&part) &&
               (part.is_reference_part() || !part.imported_body_features().empty());
      });
  // A legacy solve may deliberately evaluate a local subset. Preserve that contract: unrelated
  // project definitions do not become new validation inputs merely because this guard exists.
  if (!has_imported_result && relationships.relationships.empty()) {
    if (context)
      return context->validate_sources();
    return Result<std::size_t>::success(0);
  }
  for (auto valid : {project.validate_assembly_structure(), project.validate_assembly_constraints(),
                     project.validate_cross_hierarchy_constraints(),
                     project.validate_assembly_joints(), project.validate_cross_hierarchy_joints()})
    if (!valid)
      return Result<std::size_t>::failure(valid.error());
  if (!participating_parts.empty()) {
    auto hierarchy = AssemblyHierarchyTraversal::build(project);
    if (!hierarchy)
      return Result<std::size_t>::failure(hierarchy.error());
    std::set<std::string> active_assemblies;
    for (const auto& occurrence : hierarchy.value().occurrences())
      if (occurrence.active_path)
        active_assemblies.insert(occurrence.assembly_document.value());
    // A shared Part can occur in the root and in unrelated child definitions.
    // Their local relationships belong to this filtered operation only when an
    // active rooted occurrence exists; a full-scope command retains all intent.
    std::erase_if(relationships.relationships, [&](const auto& relationship) {
      return std::visit(
          [&](const auto& identity) {
            if constexpr (requires { identity.assembly_document; })
              return !active_assemblies.contains(identity.assembly_document.value());
            return false;
          },
          relationship);
    });
  }
  std::size_t observed = 0;
  for (const auto& part : project.part_documents()) {
    if (!selected(&part))
      continue;
    auto valid = observe_imported_part_sources(part, context);
    if (!valid)
      return Result<std::size_t>::failure(valid.error());
    observed += valid.value();
  }
  if (!relationships.relationships.empty()) {
    if (!context)
      return Result<std::size_t>::failure(Error::geometry(
          project.id().value(), "imported Assembly inputs require an explicit source context"));
    // Empty selected drive means every joint uses its authored coordinate slots. Only residuals
    // are evaluated; no numeric solve or component/joint mutation occurs here.
    auto valid = detail::evaluate_cross_hierarchy_motion_group_residuals(
        project, relationships, AssemblyJointId{}, {}, 1.0, context);
    if (!valid)
      return Result<std::size_t>::failure(valid.error());
  }
  if (context) {
    auto fresh = context->validate_sources();
    if (!fresh)
      return Result<std::size_t>::failure(fresh.error());
  }
  return Result<std::size_t>::success(observed + relationships.relationships.size());
}
} // namespace blcad::geometry
