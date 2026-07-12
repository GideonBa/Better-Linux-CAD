#include "blcad/core/assembly_exchange_graph.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

using OccurrencePath = std::vector<SubassemblyInstanceId>;

[[nodiscard]] bool path_less(const OccurrencePath& lhs, const OccurrencePath& rhs) {
  return std::lexicographical_compare(
      lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
      [](const SubassemblyInstanceId& left, const SubassemblyInstanceId& right) {
        return left.value() < right.value();
      });
}

[[nodiscard]] bool contains_path(const std::vector<OccurrencePath>& paths,
                                 const OccurrencePath& path) {
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

void add_path(std::vector<OccurrencePath>& paths, OccurrencePath path) {
  if (!contains_path(paths, path)) {
    paths.push_back(std::move(path));
  }
}

[[nodiscard]] std::string path_text(const OccurrencePath& path) {
  if (path.empty()) {
    return "root";
  }

  std::string text;
  for (std::size_t index = 0U; index < path.size(); ++index) {
    if (index != 0U) {
      text += "/";
    }
    text += path[index].value();
  }
  return text;
}

[[nodiscard]] std::string assembly_product_name(const OccurrencePath& path) {
  return "blcad:assembly-occurrence:" + path_text(path);
}

[[nodiscard]] std::string component_occurrence_name(const OccurrencePath& path,
                                                    const ComponentInstanceId& component) {
  return "blcad:component-occurrence:" + path_text(path) + "/" + component.value();
}

[[nodiscard]] std::string part_product_name(const DocumentId& part_document) {
  return "blcad:part-definition:" + part_document.value();
}

[[nodiscard]] const AssemblyHierarchyOccurrenceDescriptor* find_hierarchy_occurrence(
    const AssemblyHierarchyTraversal& hierarchy, const OccurrencePath& path) {
  const auto found = std::find_if(
      hierarchy.occurrences().begin(), hierarchy.occurrences().end(),
      [&path](const AssemblyHierarchyOccurrenceDescriptor& occurrence) {
        return occurrence.occurrence_path == path;
      });
  return found == hierarchy.occurrences().end() ? nullptr : &*found;
}

[[nodiscard]] bool component_occurrence_less(const AssemblyExchangeComponentOccurrence& lhs,
                                             const AssemblyExchangeComponentOccurrence& rhs) {
  const auto& lhs_path = lhs.identity.containing_assembly_occurrence.occurrence_path;
  const auto& rhs_path = rhs.identity.containing_assembly_occurrence.occurrence_path;
  if (path_less(lhs_path, rhs_path)) {
    return true;
  }
  if (path_less(rhs_path, lhs_path)) {
    return false;
  }
  return lhs.identity.component_instance.value() < rhs.identity.component_instance.value();
}

} // namespace

Result<AssemblyExchangeGraph> AssemblyExchangeGraph::build(const Project& project) {
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (hierarchy.has_error()) {
    return Result<AssemblyExchangeGraph>::failure(hierarchy.error());
  }

  const AssemblyLeafOccurrenceResolver leaf_resolver;
  auto leaves = leaf_resolver.resolve(project);
  if (leaves.has_error()) {
    return Result<AssemblyExchangeGraph>::failure(leaves.error());
  }

  std::vector<OccurrencePath> exported_assembly_paths;
  add_path(exported_assembly_paths, {});
  for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
    for (std::size_t prefix_size = 1U; prefix_size <= leaf.subassembly_path.size(); ++prefix_size) {
      add_path(exported_assembly_paths,
               OccurrencePath(leaf.subassembly_path.begin(),
                              leaf.subassembly_path.begin() +
                                  static_cast<std::ptrdiff_t>(prefix_size)));
    }
  }
  std::sort(exported_assembly_paths.begin(), exported_assembly_paths.end(), path_less);

  std::vector<AssemblyExchangeAssemblyOccurrence> assembly_occurrences;
  assembly_occurrences.reserve(exported_assembly_paths.size());
  for (const OccurrencePath& path : exported_assembly_paths) {
    const AssemblyHierarchyOccurrenceDescriptor* occurrence =
        find_hierarchy_occurrence(hierarchy.value(), path);
    if (occurrence == nullptr) {
      return Result<AssemblyExchangeGraph>::failure(Error::validation(
          assembly_product_name(path),
          "assembly exchange occurrence path must resolve in the canonical hierarchy"));
    }

    std::optional<AssemblyExchangeAssemblyOccurrenceIdentity> parent_identity;
    RigidTransform transform_from_parent = identity_rigid_transform();
    if (!path.empty()) {
      if (!occurrence->via_subassembly_instance.has_value() ||
          occurrence->parent_transforms_inner_to_outer.empty()) {
        return Result<AssemblyExchangeGraph>::failure(Error::validation(
            assembly_product_name(path),
            "non-root assembly exchange occurrence must retain its direct hierarchy boundary"));
      }
      OccurrencePath parent_path = path;
      parent_path.pop_back();
      parent_identity = AssemblyExchangeAssemblyOccurrenceIdentity{std::move(parent_path)};
      transform_from_parent = occurrence->parent_transforms_inner_to_outer.front();
    }

    assembly_occurrences.push_back(AssemblyExchangeAssemblyOccurrence{
        AssemblyExchangeAssemblyOccurrenceIdentity{path}, occurrence->assembly_document,
        std::move(parent_identity), occurrence->via_subassembly_instance, transform_from_parent,
        assembly_product_name(path)});
  }

  std::vector<AssemblyExchangeComponentOccurrence> component_occurrences;
  component_occurrences.reserve(leaves.value().size());
  for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
    if (leaf.transforms_inner_to_outer.empty()) {
      return Result<AssemblyExchangeGraph>::failure(Error::validation(
          component_occurrence_name(leaf.subassembly_path, leaf.component_instance),
          "assembly exchange component occurrence must retain its direct component transform"));
    }
    component_occurrences.push_back(AssemblyExchangeComponentOccurrence{
        AssemblyExchangeComponentOccurrenceIdentity{
            AssemblyExchangeAssemblyOccurrenceIdentity{leaf.subassembly_path},
            leaf.component_instance},
        leaf.assembly_document,
        AssemblyExchangePartProductDefinitionIdentity{leaf.referenced_part_document},
        leaf.transforms_inner_to_outer,
        component_occurrence_name(leaf.subassembly_path, leaf.component_instance)});
  }
  std::sort(component_occurrences.begin(), component_occurrences.end(), component_occurrence_less);

  std::vector<DocumentId> part_documents;
  for (const AssemblyExchangeComponentOccurrence& occurrence : component_occurrences) {
    if (std::find(part_documents.begin(), part_documents.end(),
                  occurrence.part_product.part_document) == part_documents.end()) {
      part_documents.push_back(occurrence.part_product.part_document);
    }
  }
  std::sort(part_documents.begin(), part_documents.end(),
            [](const DocumentId& lhs, const DocumentId& rhs) {
              return lhs.value() < rhs.value();
            });

  std::vector<AssemblyExchangePartProductDefinition> part_product_definitions;
  part_product_definitions.reserve(part_documents.size());
  for (const DocumentId& part_document : part_documents) {
    part_product_definitions.push_back(AssemblyExchangePartProductDefinition{
        AssemblyExchangePartProductDefinitionIdentity{part_document},
        part_product_name(part_document)});
  }

  return Result<AssemblyExchangeGraph>::success(
      AssemblyExchangeGraph(std::move(assembly_occurrences),
                            std::move(component_occurrences),
                            std::move(part_product_definitions)));
}

AssemblyExchangeGraph::AssemblyExchangeGraph(
    std::vector<AssemblyExchangeAssemblyOccurrence> assembly_occurrences,
    std::vector<AssemblyExchangeComponentOccurrence> component_occurrences,
    std::vector<AssemblyExchangePartProductDefinition> part_product_definitions)
    : assembly_occurrences_(std::move(assembly_occurrences)),
      component_occurrences_(std::move(component_occurrences)),
      part_product_definitions_(std::move(part_product_definitions)) {}

const std::vector<AssemblyExchangeAssemblyOccurrence>&
AssemblyExchangeGraph::assembly_occurrences() const noexcept {
  return assembly_occurrences_;
}

const std::vector<AssemblyExchangeComponentOccurrence>&
AssemblyExchangeGraph::component_occurrences() const noexcept {
  return component_occurrences_;
}

const std::vector<AssemblyExchangePartProductDefinition>&
AssemblyExchangeGraph::part_product_definitions() const noexcept {
  return part_product_definitions_;
}

std::size_t AssemblyExchangeGraph::assembly_occurrence_count() const noexcept {
  return assembly_occurrences_.size();
}

std::size_t AssemblyExchangeGraph::component_occurrence_count() const noexcept {
  return component_occurrences_.size();
}

std::size_t AssemblyExchangeGraph::part_product_definition_count() const noexcept {
  return part_product_definitions_.size();
}

} // namespace blcad
