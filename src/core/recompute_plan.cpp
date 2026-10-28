#include "blcad/core/recompute_plan.hpp"

#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace blcad {

Result<RecomputePlan>
RecomputePlan::from_graph_and_invalidation_state(const DependencyGraph& graph,
                                                 const InvalidationState& invalidation_state) {
  const auto ordered = graph.topological_order();
  if (ordered.has_error()) {
    return Result<RecomputePlan>::failure(ordered.error());
  }

  std::vector<RecomputeStep> steps;

  for (const auto& node_id : ordered.value()) {
    const auto* entry = invalidation_state.find(node_id);
    if (entry != nullptr && entry->status == InvalidationStatus::Dirty) {
      steps.push_back(RecomputeStep{node_id});
    }
  }

  return Result<RecomputePlan>::success(RecomputePlan(std::move(steps)));
}

Result<RecomputePlan>
RecomputePlan::from_graph_and_changed_nodes(const DependencyGraph& graph,
                                            const std::vector<NodeId>& changed) {
  auto ordered = graph.topological_order();
  if (!ordered)
    return Result<RecomputePlan>::failure(ordered.error());
  std::unordered_map<NodeId, std::vector<NodeId>> dependents;
  for (const auto& [source, dependent] : graph.dependencies())
    dependents[source].push_back(dependent);
  std::unordered_set<NodeId> affected;
  std::vector<NodeId> pending;
  for (const auto& node : changed) {
    if (!graph.has_node(node))
      return Result<RecomputePlan>::failure(
          Error::dependency(node, "recompute root must exist in dependency graph"));
    if (affected.insert(node).second)
      pending.push_back(node);
  }
  for (std::size_t index = 0; index < pending.size(); ++index)
    for (const auto& dependent : dependents[pending[index]])
      if (affected.insert(dependent).second)
        pending.push_back(dependent);
  std::vector<RecomputeStep> steps;
  for (const auto& node : ordered.value())
    if (affected.contains(node))
      steps.push_back({node});
  return Result<RecomputePlan>::success(RecomputePlan(std::move(steps)));
}

bool RecomputePlan::empty() const noexcept {
  return steps_.empty();
}

std::size_t RecomputePlan::step_count() const noexcept {
  return steps_.size();
}

const std::vector<RecomputeStep>& RecomputePlan::steps() const noexcept {
  return steps_;
}

std::vector<RecomputePlan::NodeId> RecomputePlan::node_ids() const {
  std::vector<NodeId> nodes;
  nodes.reserve(steps_.size());

  for (const auto& step : steps_) {
    nodes.push_back(step.node_id);
  }

  return nodes;
}

bool RecomputePlan::contains(std::string_view node_id) const noexcept {
  for (const auto& step : steps_) {
    if (step.node_id == node_id) {
      return true;
    }
  }

  return false;
}

RecomputePlan::RecomputePlan(std::vector<RecomputeStep> steps) : steps_(std::move(steps)) {}

} // namespace blcad
