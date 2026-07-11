#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <vector>

namespace blcad {

class AssemblyConstraintGraphEdge {
public:
  AssemblyConstraintGraphEdge(AssemblyConstraintId constraint,
                              ComponentInstanceId component_a,
                              ComponentInstanceId component_b);

  [[nodiscard]] const AssemblyConstraintId& constraint() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_a() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_b() const noexcept;

private:
  AssemblyConstraintId constraint_;
  ComponentInstanceId component_a_;
  ComponentInstanceId component_b_;
};

// Read-only connectivity derived from AssemblyDocument relationship intent.
// Every component instance is a node and every active assembly constraint is a
// distinct edge. Semantic target geometry and component placement are untouched.
class AssemblyConstraintGraph {
public:
  [[nodiscard]] static Result<AssemblyConstraintGraph>
  build(const AssemblyDocument& assembly);

  [[nodiscard]] const std::vector<ComponentInstanceId>& nodes() const noexcept;
  [[nodiscard]] const std::vector<AssemblyConstraintGraphEdge>& edges() const noexcept;
  [[nodiscard]] std::size_t node_count() const noexcept;
  [[nodiscard]] std::size_t edge_count() const noexcept;

  [[nodiscard]] Result<std::vector<AssemblyConstraintId>>
  adjacent_constraints(ComponentInstanceId component) const;

  [[nodiscard]] std::vector<std::vector<ComponentInstanceId>>
  connected_components() const;

private:
  AssemblyConstraintGraph(std::vector<ComponentInstanceId> nodes,
                          std::vector<AssemblyConstraintGraphEdge> edges);

  [[nodiscard]] const ComponentInstanceId*
  find_node(ComponentInstanceId component) const noexcept;

  std::vector<ComponentInstanceId> nodes_;
  std::vector<AssemblyConstraintGraphEdge> edges_;
};

} // namespace blcad
