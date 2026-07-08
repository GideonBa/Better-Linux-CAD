#include "blcad/core/recompute_plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using namespace blcad;

namespace {

std::size_t index_of(const std::vector<std::string>& values, const std::string& value) {
  const auto found = std::find(values.begin(), values.end(), value);
  REQUIRE(found != values.end());
  return static_cast<std::size_t>(std::distance(values.begin(), found));
}

DependencyGraph make_reference_plate_graph() {
  DependencyGraph graph;

  REQUIRE(graph.add_dependency("part.width", "sketch.base"));
  REQUIRE(graph.add_dependency("part.height", "sketch.base"));
  REQUIRE(graph.add_dependency("part.hole_diameter", "sketch.hole"));
  REQUIRE(graph.add_dependency("sketch.base", "feature.base_extrude"));
  REQUIRE(graph.add_dependency("part.thickness", "feature.base_extrude"));
  REQUIRE(graph.add_dependency("sketch.hole", "feature.center_hole_cut"));
  REQUIRE(graph.add_dependency("feature.base_extrude", "feature.center_hole_cut"));

  return graph;
}

} // namespace

TEST_CASE("RecomputePlan is empty for a clean invalidation state", "[core][recompute_plan]") {
  const auto graph = make_reference_plate_graph();
  const auto invalidation_state = InvalidationState::from_graph(graph);

  const auto plan = RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state);

  REQUIRE(plan);
  CHECK(plan.value().empty());
  CHECK(plan.value().step_count() == 0);
  CHECK(plan.value().node_ids().empty());
}

TEST_CASE("RecomputePlan lists dirty nodes in topological order after width changes",
          "[core][recompute_plan]") {
  const auto graph = make_reference_plate_graph();
  auto invalidation_state = InvalidationState::from_graph(graph);
  REQUIRE(invalidation_state.mark_changed(graph, "part.width"));

  const auto plan = RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state);

  REQUIRE(plan);
  CHECK(plan.value().step_count() == 3);
  CHECK(!plan.value().contains("part.width"));
  CHECK(plan.value().contains("sketch.base"));
  CHECK(plan.value().contains("feature.base_extrude"));
  CHECK(plan.value().contains("feature.center_hole_cut"));

  const auto nodes = plan.value().node_ids();
  CHECK(index_of(nodes, "sketch.base") < index_of(nodes, "feature.base_extrude"));
  CHECK(index_of(nodes, "feature.base_extrude") < index_of(nodes, "feature.center_hole_cut"));
}

TEST_CASE("RecomputePlan follows the changed parameter branch", "[core][recompute_plan]") {
  const auto graph = make_reference_plate_graph();
  auto invalidation_state = InvalidationState::from_graph(graph);
  REQUIRE(invalidation_state.mark_changed(graph, "part.hole_diameter"));

  const auto plan = RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state);

  REQUIRE(plan);
  CHECK(plan.value().step_count() == 2);
  CHECK(!plan.value().contains("part.hole_diameter"));
  CHECK(plan.value().contains("sketch.hole"));
  CHECK(plan.value().contains("feature.center_hole_cut"));
  CHECK(!plan.value().contains("sketch.base"));
  CHECK(!plan.value().contains("feature.base_extrude"));
}

TEST_CASE("RecomputePlan reports cycles from the dependency graph", "[core][recompute_plan]") {
  DependencyGraph graph;
  REQUIRE(graph.add_dependency("sketch.base", "feature.base_extrude"));
  REQUIRE(graph.add_dependency("feature.base_extrude", "sketch.base"));

  auto invalidation_state = InvalidationState::from_graph(graph);
  REQUIRE(invalidation_state.mark_changed(graph, "sketch.base"));

  const auto plan = RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state);

  REQUIRE(plan.has_error());
  CHECK(plan.error().category() == ErrorCategory::Dependency);
  CHECK(plan.error().object_id() == "dependency_graph");
  CHECK(plan.error().message() == "dependency graph contains a cycle");
}
