#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <algorithm>
#include <utility>

namespace blcad::geometry::detail {

[[nodiscard]] inline Result<std::size_t>
validate_history_evaluation(const PartDocument& document, const ShapeCache* cache = nullptr) {
  if (cache)
    return cache->validate_complete_history_result(document);
  if (document.authored_history_intent())
    return Result<std::size_t>::failure(Error::dependency(
        document.id().value(), "Feature history requires a matching evaluated cache."));
  return Result<std::size_t>::success(0);
}
// Modeling presentations may explicitly qualify a prefix. Omitting the scope
// retains the complete-result contract used by ordinary scene/inspection APIs.
[[nodiscard]] inline Result<std::size_t>
validate_history_modeling_scope(const PartDocument& document, const ShapeCache& cache,
                                std::optional<std::size_t> prefix_count) {
  if (!prefix_count)
    return validate_history_evaluation(document, &cache);
  const auto* plan = cache.history_plan();
  if (!plan || plan->prefix_count != prefix_count)
    return Result<std::size_t>::failure(Error::dependency(
        document.id().value(), "The requested history prefix does not match the evaluated scope."));
  return cache.validate_history_input(document);
}
[[nodiscard]] inline Result<std::size_t> validate_history_input(const PartDocument& document,
                                                                const ShapeCache& cache,
                                                                std::string_view executing = {},
                                                                std::string_view source = {}) {
  return cache.validate_history_input(document, executing, source);
}

// Authored local reference data is distinct from a fresh, posed model result.
[[nodiscard]] inline Result<std::size_t>
validate_history_source(const PartDocument& document, std::string source,
                        const ShapeCache* cache = nullptr) {
  if (cache) {
    const auto valid = cache->validate_history_input(document);
    if (!valid)
      return valid;
  }
  if (!document.authored_history_intent() && (!cache || !cache->has_history_scope()))
    return Result<std::size_t>::success(0);
  std::optional<PartHistoryEvaluationPlan> owned_plan;
  const PartHistoryEvaluationPlan* plan = cache ? cache->history_plan() : nullptr;
  if (!plan) {
    auto derived = document.create_history_evaluation_plan();
    if (!derived)
      return Result<std::size_t>::failure(derived.error());
    owned_plan = std::move(derived.value());
    plan = &*owned_plan;
  }
  std::vector<std::string> pending{std::move(source)};
  std::vector<std::string> visited;
  while (!pending.empty()) {
    auto node = std::move(pending.back());
    pending.pop_back();
    if (std::find(visited.begin(), visited.end(), node) != visited.end())
      continue;
    visited.push_back(node);
    if (node.starts_with("body:") && document.find_body(BodyId(node.substr(5)))) {
      const BodyId body(node.substr(5));
      const auto found = std::find_if(plan->bodies.begin(), plan->bodies.end(),
                                      [&](const auto& row) { return row.body == body; });
      const bool available = cache ? cache->find_body_shape(body) != nullptr
                                   : found != plan->bodies.end() && found->available;
      if (!available)
        return Result<std::size_t>::failure(
            Error::dependency(node, "Reference Body is unavailable."));
      continue;
    }
    const auto& entries = plan->entries;
    const auto entry = std::find_if(entries.begin(), entries.end(),
                                    [&](const auto& row) { return row.feature.node_id == node; });
    if (entry != entries.end()) {
      if (entry->state != PartHistoryExecutionState::Active)
        return Result<std::size_t>::failure(
            Error::dependency(node, "Reference producer is suppressed or blocked."));
      continue;
    }
    // Fixed datum planes are owned local geometry, not dependency graph nodes.
    if (document.find_datum_plane(DatumPlaneId(node)))
      continue;
    if (!document.dependency_graph().has_node(node))
      return Result<std::size_t>::failure(
          Error::dependency(node, "Reference resource is missing."));
    for (const auto& [parent, dependent] : document.dependency_graph().dependencies())
      if (dependent == node)
        pending.push_back(parent);
  }
  return Result<std::size_t>::success(0);
}

} // namespace blcad::geometry::detail
