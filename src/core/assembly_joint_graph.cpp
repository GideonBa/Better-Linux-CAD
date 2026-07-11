#include "blcad/core/assembly_joint_graph.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] bool component_id_less(const ComponentInstanceId& lhs,
                                     const ComponentInstanceId& rhs) {
  return lhs.value() < rhs.value();
}

[[nodiscard]] std::size_t node_index(const std::vector<ComponentInstanceId>& nodes,
                                     const ComponentInstanceId& component) {
  const auto found = std::find(nodes.begin(), nodes.end(), component);
  return static_cast<std::size_t>(found - nodes.begin());
}

} // namespace

AssemblyJointGraphEdge::AssemblyJointGraphEdge(AssemblyJointId joint,
                                               ComponentInstanceId component_a,
                                               ComponentInstanceId component_b)
    : joint_(std::move(joint)), component_a_(std::move(component_a)),
      component_b_(std::move(component_b)) {}

const AssemblyJointId& AssemblyJointGraphEdge::joint() const noexcept { return joint_; }

const ComponentInstanceId& AssemblyJointGraphEdge::component_a() const noexcept {
  return component_a_;
}

const ComponentInstanceId& AssemblyJointGraphEdge::component_b() const noexcept {
  return component_b_;
}

Result<AssemblyJointGraph> AssemblyJointGraph::build(const AssemblyDocument& assembly) {
  std::vector<ComponentInstanceId> nodes;
  nodes.reserve(assembly.component_instance_count());
  for (const ComponentInstance& instance : assembly.component_instances()) {
    nodes.push_back(instance.id());
  }
  std::sort(nodes.begin(), nodes.end(), component_id_less);

  std::vector<AssemblyJointGraphEdge> edges;
  edges.reserve(assembly.joint_count());
  for (const AssemblyJoint& joint : assembly.joints()) {
    if (joint.state() == AssemblyJointState::Inactive) {
      continue;
    }

    const ComponentInstanceId& component_a = joint.target_a().component_instance();
    const ComponentInstanceId& component_b = joint.target_b().component_instance();
    if (assembly.find_component_instance(component_a) == nullptr) {
      return Result<AssemblyJointGraph>::failure(Error::validation(
          joint.id().value(), "assembly joint graph edge target A component instance must exist"));
    }
    if (assembly.find_component_instance(component_b) == nullptr) {
      return Result<AssemblyJointGraph>::failure(Error::validation(
          joint.id().value(), "assembly joint graph edge target B component instance must exist"));
    }

    edges.emplace_back(joint.id(), component_a, component_b);
  }
  std::sort(edges.begin(), edges.end(), [](const AssemblyJointGraphEdge& lhs,
                                           const AssemblyJointGraphEdge& rhs) {
    return lhs.joint().value() < rhs.joint().value();
  });

  return Result<AssemblyJointGraph>::success(
      AssemblyJointGraph(std::move(nodes), std::move(edges)));
}

AssemblyJointGraph::AssemblyJointGraph(std::vector<ComponentInstanceId> nodes,
                                       std::vector<AssemblyJointGraphEdge> edges)
    : nodes_(std::move(nodes)), edges_(std::move(edges)) {}

const std::vector<ComponentInstanceId>& AssemblyJointGraph::nodes() const noexcept {
  return nodes_;
}

const std::vector<AssemblyJointGraphEdge>& AssemblyJointGraph::edges() const noexcept {
  return edges_;
}

std::size_t AssemblyJointGraph::node_count() const noexcept { return nodes_.size(); }

std::size_t AssemblyJointGraph::edge_count() const noexcept { return edges_.size(); }

Result<std::vector<AssemblyJointId>>
AssemblyJointGraph::adjacent_joints(ComponentInstanceId component) const {
  if (find_node(component) == nullptr) {
    const auto object_id = component.empty() ? std::string("assembly_joint_graph") : component.value();
    return Result<std::vector<AssemblyJointId>>::failure(
        Error::validation(object_id, "component instance must exist in assembly joint graph"));
  }

  std::vector<AssemblyJointId> adjacent;
  for (const AssemblyJointGraphEdge& edge : edges_) {
    if (edge.component_a() == component || edge.component_b() == component) {
      adjacent.push_back(edge.joint());
    }
  }
  return Result<std::vector<AssemblyJointId>>::success(std::move(adjacent));
}

std::vector<std::vector<ComponentInstanceId>> AssemblyJointGraph::connected_components() const {
  std::vector<std::vector<ComponentInstanceId>> groups;
  std::vector<bool> visited(nodes_.size(), false);

  for (std::size_t start = 0U; start < nodes_.size(); ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<std::size_t> pending{start};
    visited[start] = true;
    std::vector<ComponentInstanceId> group;

    while (!pending.empty()) {
      const std::size_t current = pending.back();
      pending.pop_back();
      group.push_back(nodes_[current]);

      std::vector<std::size_t> neighbors;
      for (const AssemblyJointGraphEdge& edge : edges_) {
        const ComponentInstanceId* neighbor = nullptr;
        if (edge.component_a() == nodes_[current]) {
          neighbor = &edge.component_b();
        } else if (edge.component_b() == nodes_[current]) {
          neighbor = &edge.component_a();
        }
        if (neighbor == nullptr) {
          continue;
        }

        const std::size_t index = node_index(nodes_, *neighbor);
        if (index < nodes_.size() && !visited[index]) {
          neighbors.push_back(index);
        }
      }

      std::sort(neighbors.begin(), neighbors.end(), [this](std::size_t lhs, std::size_t rhs) {
        return component_id_less(nodes_[lhs], nodes_[rhs]);
      });
      neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
      for (const std::size_t neighbor : neighbors) {
        if (!visited[neighbor]) {
          visited[neighbor] = true;
          pending.push_back(neighbor);
        }
      }
    }

    std::sort(group.begin(), group.end(), component_id_less);
    groups.push_back(std::move(group));
  }

  return groups;
}

const ComponentInstanceId*
AssemblyJointGraph::find_node(ComponentInstanceId component) const noexcept {
  const auto found = std::find(nodes_.begin(), nodes_.end(), component);
  return found == nodes_.end() ? nullptr : &*found;
}

} // namespace blcad
