#pragma once

#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

class DependencyGraph {
public:
  using NodeId = std::string;
  using DependencyEdge = std::pair<NodeId, NodeId>;

  [[nodiscard]] Result<std::size_t> add_node(NodeId node_id);
  [[nodiscard]] Result<std::size_t> add_dependency(NodeId dependency, NodeId dependent);
  // Removes every edge whose dependent is the given node and returns how many
  // were removed. Nodes stay in the graph; only incoming edges are dropped.
  // Used when an expression formula is rewritten and its inputs change.
  [[nodiscard]] std::size_t remove_dependencies_of_dependent(std::string_view dependent);

  [[nodiscard]] bool has_node(std::string_view node_id) const noexcept;
  [[nodiscard]] bool has_dependency(std::string_view dependency,
                                    std::string_view dependent) const noexcept;
  [[nodiscard]] std::size_t node_count() const noexcept;
  [[nodiscard]] std::size_t dependency_count() const noexcept;

  [[nodiscard]] const std::vector<NodeId>& nodes() const noexcept;
  [[nodiscard]] const std::vector<DependencyEdge>& dependencies() const noexcept;

  [[nodiscard]] std::vector<NodeId> direct_dependents(std::string_view node_id) const;
  [[nodiscard]] std::vector<NodeId> transitive_dependents(std::string_view node_id) const;
  [[nodiscard]] Result<std::vector<NodeId>> topological_order() const;
  [[nodiscard]] bool has_cycle() const;

private:
  [[nodiscard]] std::size_t node_index(std::string_view node_id) const noexcept;

  std::vector<NodeId> nodes_;
  std::vector<DependencyEdge> dependencies_;
};

} // namespace blcad
