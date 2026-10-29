#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <set>
#include <string>
#include <vector>

namespace blcad::geometry::detail {

inline Result<std::size_t> validate_imported_feature_inputs(const PartDocument& document,
                                                            const ShapeCache& cache,
                                                            const std::string& consumer,
                                                            StepImportTopologyContext* context) {
  if (document.imported_body_features().empty())
    return Result<std::size_t>::success(0);
  auto observed = observe_imported_part_sources(document, context);
  if (!observed)
    return observed;
  std::set<std::string> ancestors;
  std::vector<std::string> pending{consumer};
  for (std::size_t index = 0; index < pending.size(); ++index)
    for (const auto& [source, dependent] : document.dependency_graph().dependencies())
      if (dependent == pending[index] && ancestors.insert(source).second)
        pending.push_back(source);
  for (const auto& base : document.imported_body_features())
    if (ancestors.contains(base.id().value())) {
      auto owned = cache.validated_imported_body_result(document, base);
      if (!owned)
        return Result<std::size_t>::failure(owned.error());
    }
  return observed;
}

inline Result<const CachedBodyTransformState*>
body_transform_before(const PartDocument& document, const ShapeCache& cache, const BodyId& body,
                      const std::string& consumer, bool include_consumer = false) {
  std::set<std::string> ancestors;
  if (include_consumer)
    ancestors.insert(consumer);
  std::vector<std::string> pending{consumer};
  for (std::size_t index = 0; index < pending.size(); ++index)
    for (const auto& [source, dependent] : document.dependency_graph().dependencies())
      if (dependent == pending[index] && ancestors.insert(source).second)
        pending.push_back(source);
  auto order = document.dependency_graph().topological_order();
  if (!order)
    return Result<const CachedBodyTransformState*>::failure(order.error());
  const CachedBodyTransformState* result = nullptr;
  for (const auto& node : order.value()) {
    const auto* transform = document.find_body_transform(BodyTransformId(node));
    if (!transform || transform->body() != body || !ancestors.contains(node))
      continue;
    result = cache.find_body_transform_state(transform->id());
    if (!result)
      return Result<const CachedBodyTransformState*>::failure(
          Error::geometry(node, "preceding Body transform requires its computed placement state"));
  }
  return Result<const CachedBodyTransformState*>::success(result);
}

inline Result<ImportedTopologyDescriptor>
imported_descriptor_on_body(const PartDocument& document, const ShapeCache& cache,
                            const ImportedTopologyReference& reference,
                            const ImportedTopologyResolution& current, const FeatureId& consumer,
                            bool include_consumer = false) {
  const auto* catalog = document.find_imported_topology_catalog(reference.catalog());
  const auto* entry = catalog ? catalog->find(reference.entry()) : nullptr;
  if (!entry || !current.descriptor)
    return Result<ImportedTopologyDescriptor>::failure(
        Error::geometry(consumer.value(), "imported topology requires its owned current witness"));
  const std::optional<ImportedTopologyId> owner =
      entry->descriptor.kind == ImportedTopologyKind::Body ? entry->id : entry->parent_body;
  const ImportedBodyFeature* base = nullptr;
  const ImportedBodyOutput* output = nullptr;
  for (const auto& candidate : document.imported_body_features())
    if (candidate.definition().catalog() == catalog->id())
      for (const auto& binding : candidate.definition().outputs())
        if (owner && binding.imported_body == *owner) {
          if (output)
            return Result<ImportedTopologyDescriptor>::failure(
                Error::geometry(consumer.value(), "imported topology Body placement is ambiguous"));
          base = &candidate;
          output = &binding;
        }
  // An external catalog may supply an uninstantiated input to an ordinary Part (139).
  if (!output)
    return Result<ImportedTopologyDescriptor>::success(*current.descriptor);
  auto owned = cache.validated_imported_body_result(document, *base);
  if (!owned)
    return Result<ImportedTopologyDescriptor>::failure(owned.error());
  FeatureId operation = consumer;
  if (operation.empty()) {
    const auto* result = cache.find_body_result(output->body);
    if (!result)
      return Result<ImportedTopologyDescriptor>::failure(Error::geometry(
          output->body.value(), "imported selection requires its current Body result"));
    operation = result->source_feature_id;
    include_consumer = true;
  }
  auto placement =
      body_transform_before(document, cache, output->body, operation.value(), include_consumer);
  if (!placement)
    return Result<ImportedTopologyDescriptor>::failure(placement.error());
  if (!placement.value())
    return Result<ImportedTopologyDescriptor>::success(*current.descriptor);
  return StepImportTopology{}.placed_descriptor(current.shape,
                                                placement.value()->cumulative_transform);
}
} // namespace blcad::geometry::detail
