#pragma once

#include "blcad/core/dependency_graph.hpp"
#include "blcad/core/invalidation_state.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

struct RecomputeStep {
  std::string node_id;
};

class RecomputePlan {
public:
  using NodeId = std::string;

  [[nodiscard]] static Result<RecomputePlan>
  from_graph_and_invalidation_state(const DependencyGraph& graph,
                                    const InvalidationState& invalidation_state);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t step_count() const noexcept;
  [[nodiscard]] const std::vector<RecomputeStep>& steps() const noexcept;
  [[nodiscard]] std::vector<NodeId> node_ids() const;
  [[nodiscard]] bool contains(std::string_view node_id) const noexcept;

private:
  explicit RecomputePlan(std::vector<RecomputeStep> steps);

  std::vector<RecomputeStep> steps_;
};

} // namespace blcad
