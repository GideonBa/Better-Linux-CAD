#include "blcad/core/invalidation_state.hpp"

#include <limits>
#include <utility>

namespace blcad {

namespace {

constexpr auto missing_index = std::numeric_limits<std::size_t>::max();

} // namespace

std::string_view to_string(InvalidationStatus status) noexcept {
  switch (status) {
  case InvalidationStatus::Clean:
    return "clean";
  case InvalidationStatus::Changed:
    return "changed";
  case InvalidationStatus::Dirty:
    return "dirty";
  case InvalidationStatus::Error:
    return "error";
  }

  return "error";
}

InvalidationState InvalidationState::from_graph(const DependencyGraph& graph) {
  InvalidationState state;
  for (const auto& node : graph.nodes()) {
    (void)state.track_node(node);
  }

  return state;
}

Result<std::size_t> InvalidationState::track_node(NodeId node_id) {
  if (node_id.empty()) {
    return Result<std::size_t>::failure(
        Error::validation("invalidation_state", "invalidation node id must not be empty"));
  }

  const auto existing_index = node_index(node_id);
  if (existing_index != missing_index) {
    return Result<std::size_t>::success(existing_index);
  }

  entries_.push_back(InvalidationEntry{std::move(node_id), InvalidationStatus::Clean});
  return Result<std::size_t>::success(entries_.size() - 1);
}

Result<std::size_t> InvalidationState::sync_from_graph(const DependencyGraph& graph) {
  for (const auto& node : graph.nodes()) {
    const auto tracked = track_node(node);
    if (tracked.has_error()) {
      return Result<std::size_t>::failure(tracked.error());
    }
  }

  return Result<std::size_t>::success(entries_.size());
}

Result<std::vector<InvalidationState::NodeId>>
InvalidationState::mark_changed(const DependencyGraph& graph, std::string_view node_id) {
  if (node_id.empty()) {
    return Result<std::vector<NodeId>>::failure(
        Error::validation("invalidation_state", "changed node id must not be empty"));
  }

  if (!graph.has_node(node_id)) {
    return Result<std::vector<NodeId>>::failure(
        Error::dependency(std::string(node_id), "changed node must exist in dependency graph"));
  }

  const auto synced = sync_from_graph(graph);
  if (synced.has_error()) {
    return Result<std::vector<NodeId>>::failure(synced.error());
  }

  std::vector<NodeId> affected_nodes;
  affected_nodes.push_back(std::string(node_id));
  set_status(node_id, InvalidationStatus::Changed);

  for (const auto& dependent : graph.transitive_dependents(node_id)) {
    affected_nodes.push_back(dependent);

    const auto* entry = find(dependent);
    if (entry != nullptr && entry->status != InvalidationStatus::Changed &&
        entry->status != InvalidationStatus::Error) {
      set_status(dependent, InvalidationStatus::Dirty);
    }
  }

  return Result<std::vector<NodeId>>::success(std::move(affected_nodes));
}

void InvalidationState::mark_all_clean() noexcept {
  for (auto& entry : entries_) {
    entry.status = InvalidationStatus::Clean;
  }
}

bool InvalidationState::has_node(std::string_view node_id) const noexcept {
  return node_index(node_id) != missing_index;
}

const InvalidationEntry* InvalidationState::find(std::string_view node_id) const noexcept {
  const auto index = node_index(node_id);
  if (index == missing_index) {
    return nullptr;
  }

  return &entries_[index];
}

std::vector<InvalidationState::NodeId>
InvalidationState::nodes_with_status(InvalidationStatus status) const {
  std::vector<NodeId> nodes;

  for (const auto& entry : entries_) {
    if (entry.status == status) {
      nodes.push_back(entry.node_id);
    }
  }

  return nodes;
}

const std::vector<InvalidationEntry>& InvalidationState::entries() const noexcept {
  return entries_;
}

std::size_t InvalidationState::node_count() const noexcept {
  return entries_.size();
}

std::size_t InvalidationState::node_index(std::string_view node_id) const noexcept {
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    if (entries_[index].node_id == node_id) {
      return index;
    }
  }

  return missing_index;
}

void InvalidationState::set_status(std::string_view node_id, InvalidationStatus status) noexcept {
  const auto index = node_index(node_id);
  if (index != missing_index) {
    entries_[index].status = status;
  }
}

} // namespace blcad
