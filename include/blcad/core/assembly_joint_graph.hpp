#pragma once

#include "blcad/core/assembly_document.hpp"
#include "blcad/core/assembly_joint.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <vector>

namespace blcad {

class AssemblyJointGraphEdge {
public:
  AssemblyJointGraphEdge(AssemblyJointId joint, ComponentInstanceId component_a,
                         ComponentInstanceId component_b);

  [[nodiscard]] const AssemblyJointId& joint() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_a() const noexcept;
  [[nodiscard]] const ComponentInstanceId& component_b() const noexcept;

private:
  AssemblyJointId joint_;
  ComponentInstanceId component_a_;
  ComponentInstanceId component_b_;
};

// Read-only connectivity derived from persistent joint intent. Inactive joints
// do not participate. Connectivity and ordering are regenerated and unpersisted.
class AssemblyJointGraph {
public:
  [[nodiscard]] static Result<AssemblyJointGraph> build(const AssemblyDocument& assembly);

  [[nodiscard]] const std::vector<ComponentInstanceId>& nodes() const noexcept;
  [[nodiscard]] const std::vector<AssemblyJointGraphEdge>& edges() const noexcept;
  [[nodiscard]] std::size_t node_count() const noexcept;
  [[nodiscard]] std::size_t edge_count() const noexcept;

  [[nodiscard]] Result<std::vector<AssemblyJointId>>
  adjacent_joints(ComponentInstanceId component) const;

  [[nodiscard]] std::vector<std::vector<ComponentInstanceId>> connected_components() const;

private:
  AssemblyJointGraph(std::vector<ComponentInstanceId> nodes,
                     std::vector<AssemblyJointGraphEdge> edges);

  [[nodiscard]] const ComponentInstanceId*
  find_node(ComponentInstanceId component) const noexcept;

  std::vector<ComponentInstanceId> nodes_;
  std::vector<AssemblyJointGraphEdge> edges_;
};

} // namespace blcad
