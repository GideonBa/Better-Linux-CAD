#pragma once

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/geometry/assembly_import_validation.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

namespace blcad::gui {

// An operation owns its context. Unsaved authored documents retain their legacy
// path; external geometry always requires the saved containing document root.
[[nodiscard]] inline Result<std::unique_ptr<geometry::StepImportTopologyContext>>
make_step_import_context(const GuiDocumentSession& session) {
  const auto imported = [](const PartDocument& part) {
    return part.is_reference_part() || !part.imported_body_features().empty();
  };
  const bool needs_root =
      (session.part_document() && imported(*session.part_document())) ||
      (session.project() && std::any_of(session.project()->part_documents().begin(),
                                        session.project()->part_documents().end(), imported));
  if (session.file_path().empty()) {
    if (needs_root)
      return Result<std::unique_ptr<geometry::StepImportTopologyContext>>::failure(
          Error::validation("gui.step_import", "Save the document before using imported geometry"));
    return Result<std::unique_ptr<geometry::StepImportTopologyContext>>::success(nullptr);
  }
  std::error_code error;
  const auto path = std::filesystem::absolute(session.file_path(), error).lexically_normal();
  if (error || path.parent_path().empty())
    return Result<std::unique_ptr<geometry::StepImportTopologyContext>>::failure(
        Error::validation("gui.step_import", "Saved document asset root cannot be resolved"));
  return Result<std::unique_ptr<geometry::StepImportTopologyContext>>::success(
      std::make_unique<geometry::StepImportTopologyContext>(path.parent_path()));
}

template <typename Query>
[[nodiscard]] auto query_with_step_import_context(const GuiDocumentSession& session, Query query)
    -> std::invoke_result_t<Query, geometry::StepImportTopologyContext*> {
  using QueryResult = std::invoke_result_t<Query, geometry::StepImportTopologyContext*>;
  auto context = make_step_import_context(session);
  if (!context)
    return QueryResult::failure(context.error());
  auto result = query(context.value().get());
  if (result && context.value()) {
    auto fresh = context.value()->validate_sources();
    if (!fresh)
      return QueryResult::failure(fresh.error());
  }
  return result;
}

template <typename Relationship>
[[nodiscard]] Result<std::size_t>
validate_root_assembly_targets(const Project& project, const Relationship& relationship,
                               geometry::StepImportTopologyContext* context) {
  geometry::AssemblyConstraintTargetResolver resolver{context};
  auto a = resolver.resolve_geometric(project, relationship.target_a());
  auto b = resolver.resolve_geometric(project, relationship.target_b());
  if (!a)
    return Result<std::size_t>::failure(a.error());
  if (!b)
    return Result<std::size_t>::failure(b.error());
  auto compatible = [&] {
    if constexpr (std::is_same_v<decltype(relationship.type()), AssemblyConstraintType>)
      return geometry::AssemblyTargetCompatibilityResolver{}.resolve(relationship.type(), a.value(),
                                                                     b.value());
    else
      return geometry::AssemblyJointTargetCompatibilityResolver{}.resolve(relationship.type(),
                                                                          a.value(), b.value());
  }();
  return compatible ? Result<std::size_t>::success(2)
                    : Result<std::size_t>::failure(compatible.error());
}

template <typename Relationship>
[[nodiscard]] Result<std::size_t>
validate_hierarchy_assembly_targets(const Project& project, const Relationship& relationship,
                                    geometry::StepImportTopologyContext* context) {
  if constexpr (std::is_same_v<decltype(relationship.type()), AssemblyConstraintType>) {
    auto query = geometry::AssemblyHierarchyConstraintQuery::create(relationship);
    if (!query)
      return Result<std::size_t>::failure(query.error());
    auto equation =
        geometry::AssemblyHierarchyConstraintEquationBuilder{context}.build(project, query.value());
    return equation ? Result<std::size_t>::success(2)
                    : Result<std::size_t>::failure(equation.error());
  } else {
    const auto resolve =
        [&](const auto& endpoint) -> Result<geometry::AssemblyResolvedGeometricTarget> {
      auto target = geometry::AssemblyHierarchyConstraintTarget::create(
          endpoint.occurrence_path(), endpoint.component_instance(), endpoint.semantic_reference());
      if (!target)
        return Result<geometry::AssemblyResolvedGeometricTarget>::failure(target.error());
      return geometry::AssemblyHierarchyConstraintTargetResolver{context}.resolve_geometric(
          project, target.value());
    };
    auto a = resolve(relationship.target_a()), b = resolve(relationship.target_b());
    if (!a)
      return Result<std::size_t>::failure(a.error());
    if (!b)
      return Result<std::size_t>::failure(b.error());
    auto compatible = geometry::AssemblyJointTargetCompatibilityResolver{}.resolve(
        relationship.type(), a.value(), b.value());
    return compatible ? Result<std::size_t>::success(2)
                      : Result<std::size_t>::failure(compatible.error());
  }
}

[[nodiscard]] inline Result<std::size_t>
validate_visible_step_imports(const Project& project,
                              geometry::StepImportTopologyContext* context) {
  auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(project);
  if (!leaves)
    return Result<std::size_t>::failure(leaves.error());
  std::vector<DocumentId> parts;
  for (const auto& leaf : leaves.value())
    if (std::find(parts.begin(), parts.end(), leaf.referenced_part_document) == parts.end())
      parts.push_back(leaf.referenced_part_document);
  return parts.empty() ? Result<std::size_t>::success(0)
                       : geometry::validate_imported_assembly_inputs(project, context, parts);
}

// Revalidation stays inside the candidate transaction, before model/history
// publication. A stale external file cannot turn a successful commit into an
// error reported only after publication.
template <typename Mutation>
[[nodiscard]] Result<std::size_t>
commit_project_with_step_import_context(GuiDocumentSession& session, std::string label,
                                        Mutation mutation) {
  auto context = make_step_import_context(session);
  if (!context)
    return Result<std::size_t>::failure(context.error());
  auto* import_context = context.value().get();
  return session.commit_project_transaction(std::move(label), [mutation = std::move(mutation),
                                                               import_context](
                                                                  Project& candidate) mutable {
    auto changed = [&] {
      if constexpr (std::is_invocable_v<Mutation, Project&, geometry::StepImportTopologyContext*>)
        return mutation(candidate, import_context);
      else
        return mutation(candidate);
    }();
    if (!changed)
      return changed;
    auto valid = validate_visible_step_imports(candidate, import_context);
    if (!valid)
      return valid;
    if (import_context) {
      auto fresh = import_context->validate_sources();
      if (!fresh)
        return fresh;
    }
    return changed;
  });
}
} // namespace blcad::gui
