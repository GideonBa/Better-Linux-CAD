#pragma once

#include "blcad/core/dependency_graph.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class InvalidationStatus {
  Clean,
  Changed,
  Dirty,
  Error,
};

[[nodiscard]] std::string_view to_string(InvalidationStatus status) noexcept;

struct InvalidationEntry {
  std::string node_id;
  InvalidationStatus status;
};

class InvalidationState {
public:
  using NodeId = std::string;

  [[nodiscard]] static InvalidationState from_graph(const DependencyGraph& graph);

  [[nodiscard]] Result<std::size_t> track_node(NodeId node_id);
  [[nodiscard]] Result<std::size_t> sync_from_graph(const DependencyGraph& graph);
  [[nodiscard]] Result<std::vector<NodeId>> mark_changed(const DependencyGraph& graph,
                                                         std::string_view node_id);

  void mark_all_clean() noexcept;

  [[nodiscard]] bool has_node(std::string_view node_id) const noexcept;
  [[nodiscard]] const InvalidationEntry* find(std::string_view node_id) const noexcept;
  [[nodiscard]] std::vector<NodeId> nodes_with_status(InvalidationStatus status) const;
  [[nodiscard]] const std::vector<InvalidationEntry>& entries() const noexcept;
  [[nodiscard]] std::size_t node_count() const noexcept;

private:
  [[nodiscard]] std::size_t node_index(std::string_view node_id) const noexcept;
  void set_status(std::string_view node_id, InvalidationStatus status) noexcept;

  std::vector<InvalidationEntry> entries_;
};

} // namespace blcad
