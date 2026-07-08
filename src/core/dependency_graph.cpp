#include "blcad/core/dependency_graph.hpp"

#include <deque>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace blcad {

namespace {

constexpr auto missing_index = std::numeric_limits<std::size_t>::max();

} // namespace

Result<std::size_t> DependencyGraph::add_node(NodeId node_id) {
  if (node_id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("dependency_graph", "dependency node id must not be empty"));
  }

  const auto existing_index = node_index(node_id);
  if (existing_index != missing_index) {
    return Result<std::size_t>::success(existing_index);
  }

  nodes_.push_back(std::move(node_id));
  return Result<std::size_t>::success(nodes_.size() - 1);
}

Result<std::size_t> DependencyGraph::add_dependency(NodeId dependency, NodeId dependent) {
  if (dependency.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("dependency_graph", "dependency source node id must not be empty"));
  }

  if (dependent.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("dependency_graph", "dependency dependent node id must not be empty"));
  }

  if (dependency == dependent) {
    return Result<std::size_t>::failure(
        Error::dependency(dependency, "dependency edge must not point to itself"));
  }

  if (has_dependency(dependency, dependent)) {
    return Result<std::size_t>::failure(
        Error::dependency("dependency_graph", "dependency edge must be unique"));
  }

  const auto added_dependency = add_node(dependency);
  if (added_dependency.has_error()) {
    return Result<std::size_t>::failure(added_dependency.error());
  }

  const auto added_dependent = add_node(dependent);
  if (added_dependent.has_error()) {
    return Result<std::size_t>::failure(added_dependent.error());
  }

  dependencies_.emplace_back(std::move(dependency), std::move(dependent));
  return Result<std::size_t>::success(dependencies_.size() - 1);
}

bool DependencyGraph::has_node(std::string_view node_id) const noexcept {
  return node_index(node_id) != missing_index;
}

bool DependencyGraph::has_dependency(std::string_view dependency,
                                     std::string_view dependent) const noexcept {
  for (const auto& edge : dependencies_) {
    if (edge.first == dependency && edge.second == dependent) {
      return true;
    }
  }

  return false;
}

std::size_t DependencyGraph::node_count() const noexcept {
  return nodes_.size();
}

std::size_t DependencyGraph::dependency_count() const noexcept {
  return dependencies_.size();
}

const std::vector<DependencyGraph::NodeId>& DependencyGraph::nodes() const noexcept {
  return nodes_;
}

const std::vector<DependencyGraph::DependencyEdge>& DependencyGraph::dependencies() const noexcept {
  return dependencies_;
}

std::vector<DependencyGraph::NodeId>
DependencyGraph::direct_dependents(std::string_view node_id) const {
  std::vector<NodeId> dependents;

  for (const auto& edge : dependencies_) {
    if (edge.first == node_id) {
      dependents.push_back(edge.second);
    }
  }

  return dependents;
}

std::vector<DependencyGraph::NodeId>
DependencyGraph::transitive_dependents(std::string_view node_id) const {
  std::vector<NodeId> dependents;
  std::deque<NodeId> pending;
  std::unordered_set<std::string> visited;

  visited.insert(std::string(node_id));

  for (const auto& dependent : direct_dependents(node_id)) {
    pending.push_back(dependent);
  }

  while (!pending.empty()) {
    auto current = std::move(pending.front());
    pending.pop_front();

    if (!visited.insert(current).second) {
      continue;
    }

    dependents.push_back(current);

    for (const auto& dependent : direct_dependents(current)) {
      if (!visited.contains(dependent)) {
        pending.push_back(dependent);
      }
    }
  }

  return dependents;
}

Result<std::vector<DependencyGraph::NodeId>> DependencyGraph::topological_order() const {
  std::unordered_map<std::string, std::size_t> indegree;

  for (const auto& node : nodes_) {
    indegree.emplace(node, 0);
  }

  for (const auto& edge : dependencies_) {
    ++indegree[edge.second];
  }

  std::deque<NodeId> ready;
  for (const auto& node : nodes_) {
    if (indegree[node] == 0) {
      ready.push_back(node);
    }
  }

  std::vector<NodeId> ordered;
  ordered.reserve(nodes_.size());

  while (!ready.empty()) {
    auto current = std::move(ready.front());
    ready.pop_front();
    ordered.push_back(current);

    for (const auto& edge : dependencies_) {
      if (edge.first != ordered.back()) {
        continue;
      }

      auto& dependent_indegree = indegree[edge.second];
      --dependent_indegree;

      if (dependent_indegree == 0) {
        ready.push_back(edge.second);
      }
    }
  }

  if (ordered.size() != nodes_.size()) {
    return Result<std::vector<NodeId>>::failure(
        Error::dependency("dependency_graph", "dependency graph contains a cycle"));
  }

  return Result<std::vector<NodeId>>::success(std::move(ordered));
}

bool DependencyGraph::has_cycle() const {
  return topological_order().has_error();
}

std::size_t DependencyGraph::node_index(std::string_view node_id) const noexcept {
  for (std::size_t index = 0; index < nodes_.size(); ++index) {
    if (nodes_[index] == node_id) {
      return index;
    }
  }

  return missing_index;
}

} // namespace blcad
