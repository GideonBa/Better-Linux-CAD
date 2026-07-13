#include "blcad/core/dependency_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;

namespace {

std::size_t index_of(const std::vector<std::string>& values, const std::string& value) {
  const auto found = std::find(values.begin(), values.end(), value);
  REQUIRE(found != values.end());
  return static_cast<std::size_t>(std::distance(values.begin(), found));
}

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

} // namespace

TEST_CASE("DependencyGraph starts empty and stores explicit nodes", "[core][dependency_graph]") {
  DependencyGraph graph;

  CHECK(graph.node_count() == 0);
  CHECK(graph.dependency_count() == 0);

  const auto added = graph.add_node("part.width");

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(graph.node_count() == 1);
  CHECK(graph.has_node("part.width"));
  CHECK(graph.nodes()[0] == "part.width");
}

TEST_CASE("DependencyGraph reuses existing nodes", "[core][dependency_graph]") {
  DependencyGraph graph;

  REQUIRE(graph.add_node("part.width"));
  const auto duplicate = graph.add_node("part.width");

  REQUIRE(duplicate);
  CHECK(duplicate.value() == 0);
  CHECK(graph.node_count() == 1);
}

TEST_CASE("DependencyGraph removes nodes and their incident edges", "[core][dependency_graph]") {
  DependencyGraph graph;
  REQUIRE(graph.add_dependency("part.width", "sketch.base"));
  REQUIRE(graph.add_dependency("sketch.base", "feature.base"));

  const auto removed = graph.remove_node("sketch.base");
  REQUIRE(removed);
  CHECK(removed.value() == 1U);
  CHECK_FALSE(graph.has_node("sketch.base"));
  CHECK(graph.has_node("part.width"));
  CHECK(graph.has_node("feature.base"));
  CHECK(graph.dependency_count() == 0U);
  REQUIRE(graph.remove_node("sketch.base"));
  CHECK(graph.remove_node("").has_error());
}

TEST_CASE("DependencyGraph rejects empty node ids", "[core][dependency_graph]") {
  DependencyGraph graph;

  const auto added = graph.add_node("");

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "dependency_graph");
  CHECK(added.error().message() == "dependency node id must not be empty");
}

TEST_CASE("DependencyGraph stores dependency edges and adds missing nodes",
          "[core][dependency_graph]") {
  DependencyGraph graph;

  const auto added = graph.add_dependency("part.width", "sketch.base");

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(graph.node_count() == 2);
  CHECK(graph.dependency_count() == 1);
  CHECK(graph.has_node("part.width"));
  CHECK(graph.has_node("sketch.base"));
  CHECK(graph.has_dependency("part.width", "sketch.base"));
  CHECK(graph.dependencies()[0].first == "part.width");
  CHECK(graph.dependencies()[0].second == "sketch.base");
}

TEST_CASE("DependencyGraph rejects invalid dependency edges", "[core][dependency_graph]") {
  DependencyGraph graph;

  const auto empty_source = graph.add_dependency("", "sketch.base");
  REQUIRE(empty_source.has_error());
  CHECK(empty_source.error().category() == ErrorCategory::Validation);
  CHECK(empty_source.error().object_id() == "dependency_graph");
  CHECK(empty_source.error().message() == "dependency source node id must not be empty");

  const auto empty_dependent = graph.add_dependency("part.width", "");
  REQUIRE(empty_dependent.has_error());
  CHECK(empty_dependent.error().category() == ErrorCategory::Validation);
  CHECK(empty_dependent.error().object_id() == "dependency_graph");
  CHECK(empty_dependent.error().message() == "dependency dependent node id must not be empty");

  const auto self_dependency = graph.add_dependency("part.width", "part.width");
  REQUIRE(self_dependency.has_error());
  CHECK(self_dependency.error().category() == ErrorCategory::Dependency);
  CHECK(self_dependency.error().object_id() == "part.width");
  CHECK(self_dependency.error().message() == "dependency edge must not point to itself");

  REQUIRE(graph.add_dependency("part.width", "sketch.base"));
  const auto duplicate = graph.add_dependency("part.width", "sketch.base");
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Dependency);
  CHECK(duplicate.error().object_id() == "dependency_graph");
  CHECK(duplicate.error().message() == "dependency edge must be unique");
}

TEST_CASE("DependencyGraph returns direct and transitive dependents", "[core][dependency_graph]") {
  const auto graph = make_reference_plate_graph();

  const auto direct = graph.direct_dependents("part.width");
  REQUIRE(direct.size() == 1);
  CHECK(direct[0] == "sketch.base");

  const auto transitive = graph.transitive_dependents("part.width");
  CHECK(contains(transitive, "sketch.base"));
  CHECK(contains(transitive, "feature.base_extrude"));
  CHECK(contains(transitive, "feature.center_hole_cut"));
  CHECK(!contains(transitive, "part.width"));
}

TEST_CASE("DependencyGraph returns empty dependents for unknown nodes",
          "[core][dependency_graph]") {
  const auto graph = make_reference_plate_graph();

  CHECK(graph.direct_dependents("part.missing").empty());
  CHECK(graph.transitive_dependents("part.missing").empty());
}

TEST_CASE("DependencyGraph provides a topological order for the reference plate",
          "[core][dependency_graph]") {
  const auto graph = make_reference_plate_graph();

  const auto ordered = graph.topological_order();

  REQUIRE(ordered);
  CHECK(ordered.value().size() == graph.node_count());
  CHECK(index_of(ordered.value(), "part.width") < index_of(ordered.value(), "sketch.base"));
  CHECK(index_of(ordered.value(), "part.height") < index_of(ordered.value(), "sketch.base"));
  CHECK(index_of(ordered.value(), "part.hole_diameter") < index_of(ordered.value(), "sketch.hole"));
  CHECK(index_of(ordered.value(), "sketch.base") <
        index_of(ordered.value(), "feature.base_extrude"));
  CHECK(index_of(ordered.value(), "part.thickness") <
        index_of(ordered.value(), "feature.base_extrude"));
  CHECK(index_of(ordered.value(), "sketch.hole") <
        index_of(ordered.value(), "feature.center_hole_cut"));
  CHECK(index_of(ordered.value(), "feature.base_extrude") <
        index_of(ordered.value(), "feature.center_hole_cut"));
}

TEST_CASE("DependencyGraph detects cycles without recomputing anything",
          "[core][dependency_graph]") {
  DependencyGraph graph;
  REQUIRE(graph.add_dependency("sketch.base", "feature.base_extrude"));
  REQUIRE(graph.add_dependency("feature.base_extrude", "sketch.base"));

  CHECK(graph.has_cycle());

  const auto ordered = graph.topological_order();
  REQUIRE(ordered.has_error());
  CHECK(ordered.error().category() == ErrorCategory::Dependency);
  CHECK(ordered.error().object_id() == "dependency_graph");
  CHECK(ordered.error().message() == "dependency graph contains a cycle");
}
