#include "blcad/core/invalidation_state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;

namespace {

bool contains(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
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

InvalidationStatus status_of(const InvalidationState& state, const std::string& node_id) {
  const auto* entry = state.find(node_id);
  REQUIRE(entry != nullptr);
  return entry->status;
}

} // namespace

TEST_CASE("InvalidationStatus has stable text names", "[core][invalidation]") {
  CHECK(to_string(InvalidationStatus::Clean) == "clean");
  CHECK(to_string(InvalidationStatus::Changed) == "changed");
  CHECK(to_string(InvalidationStatus::Dirty) == "dirty");
  CHECK(to_string(InvalidationStatus::Error) == "error");
}

TEST_CASE("InvalidationState tracks graph nodes as clean", "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  const auto state = InvalidationState::from_graph(graph);

  CHECK(state.node_count() == graph.node_count());
  CHECK(state.has_node("part.width"));
  CHECK(state.has_node("feature.center_hole_cut"));
  CHECK(state.nodes_with_status(InvalidationStatus::Clean).size() == graph.node_count());
  CHECK(state.nodes_with_status(InvalidationStatus::Dirty).empty());
}

TEST_CASE("InvalidationState rejects empty tracked node ids", "[core][invalidation]") {
  InvalidationState state;

  const auto tracked = state.track_node("");

  REQUIRE(tracked.has_error());
  CHECK(tracked.error().category() == ErrorCategory::Validation);
  CHECK(tracked.error().object_id() == "invalidation_state");
  CHECK(tracked.error().message() == "invalidation node id must not be empty");
}

TEST_CASE("InvalidationState marks changed nodes and transitive dependents",
          "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  auto state = InvalidationState::from_graph(graph);

  const auto affected = state.mark_changed(graph, "part.width");

  REQUIRE(affected);
  CHECK(affected.value().size() == 4);
  CHECK(affected.value()[0] == "part.width");
  CHECK(contains(affected.value(), "sketch.base"));
  CHECK(contains(affected.value(), "feature.base_extrude"));
  CHECK(contains(affected.value(), "feature.center_hole_cut"));

  CHECK(status_of(state, "part.width") == InvalidationStatus::Changed);
  CHECK(status_of(state, "sketch.base") == InvalidationStatus::Dirty);
  CHECK(status_of(state, "feature.base_extrude") == InvalidationStatus::Dirty);
  CHECK(status_of(state, "feature.center_hole_cut") == InvalidationStatus::Dirty);
  CHECK(status_of(state, "part.height") == InvalidationStatus::Clean);
  CHECK(status_of(state, "part.thickness") == InvalidationStatus::Clean);
}

TEST_CASE("InvalidationState marks unrelated dependency branches independently",
          "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  auto state = InvalidationState::from_graph(graph);

  const auto affected = state.mark_changed(graph, "part.hole_diameter");

  REQUIRE(affected);
  CHECK(affected.value().size() == 3);
  CHECK(affected.value()[0] == "part.hole_diameter");
  CHECK(contains(affected.value(), "sketch.hole"));
  CHECK(contains(affected.value(), "feature.center_hole_cut"));

  CHECK(status_of(state, "part.hole_diameter") == InvalidationStatus::Changed);
  CHECK(status_of(state, "sketch.hole") == InvalidationStatus::Dirty);
  CHECK(status_of(state, "feature.center_hole_cut") == InvalidationStatus::Dirty);
  CHECK(status_of(state, "sketch.base") == InvalidationStatus::Clean);
  CHECK(status_of(state, "feature.base_extrude") == InvalidationStatus::Clean);
}

TEST_CASE("InvalidationState can mark all tracked nodes clean again", "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  auto state = InvalidationState::from_graph(graph);

  REQUIRE(state.mark_changed(graph, "part.width"));
  state.mark_all_clean();

  CHECK(state.nodes_with_status(InvalidationStatus::Clean).size() == graph.node_count());
  CHECK(state.nodes_with_status(InvalidationStatus::Changed).empty());
  CHECK(state.nodes_with_status(InvalidationStatus::Dirty).empty());
}

TEST_CASE("InvalidationState rejects changed nodes outside the dependency graph",
          "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  auto state = InvalidationState::from_graph(graph);

  const auto missing = state.mark_changed(graph, "part.missing");

  REQUIRE(missing.has_error());
  CHECK(missing.error().category() == ErrorCategory::Dependency);
  CHECK(missing.error().object_id() == "part.missing");
  CHECK(missing.error().message() == "changed node must exist in dependency graph");
}

TEST_CASE("InvalidationState rejects empty changed node ids", "[core][invalidation]") {
  const auto graph = make_reference_plate_graph();
  auto state = InvalidationState::from_graph(graph);

  const auto missing = state.mark_changed(graph, "");

  REQUIRE(missing.has_error());
  CHECK(missing.error().category() == ErrorCategory::Validation);
  CHECK(missing.error().object_id() == "invalidation_state");
  CHECK(missing.error().message() == "changed node id must not be empty");
}
