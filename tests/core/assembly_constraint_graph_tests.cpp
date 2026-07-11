#include "blcad/core/assembly_constraint_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

ComponentInstance make_instance(const char* id, double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 2.0, 3.0};
  transform.rotation_deg = Vector3{4.0, 5.0, 6.0};
  auto instance = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.shared"),
                                            ComponentVisibility::Visible,
                                            ComponentSuppressionState::Active,
                                            ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint make_constraint(const char* id,
                                   const char* component_a,
                                   const char* component_b,
                                   AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Mate,
      make_target(component_a, "feature.base.top"),
      make_target(component_b, "feature.base.bottom"), state);
  REQUIRE(constraint);
  return constraint.value();
}

AssemblyDocument make_graph_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.graph"), "GraphAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));

  REQUIRE(assembly.value().add_component_instance(make_instance("component.z", 50.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.c", 30.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.a", 10.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.d", 40.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.b", 20.0)));

  return assembly.value();
}

std::vector<std::string> id_values(const std::vector<ComponentInstanceId>& ids) {
  std::vector<std::string> values;
  values.reserve(ids.size());
  for (const auto& id : ids) {
    values.push_back(id.value());
  }
  return values;
}

std::vector<std::string> constraint_id_values(const std::vector<AssemblyConstraintId>& ids) {
  std::vector<std::string> values;
  values.reserve(ids.size());
  for (const auto& id : ids) {
    values.push_back(id.value());
  }
  return values;
}

} // namespace

TEST_CASE("AssemblyConstraintGraph includes all nodes and active constraints only",
          "[core][assembly-constraint-graph]") {
  auto assembly = make_graph_assembly();
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.zeta", "component.a", "component.b")));
  REQUIRE(assembly.add_constraint(make_constraint("constraint.inactive", "component.b", "component.c",
                                                  AssemblyConstraintState::Inactive)));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.alpha", "component.b", "component.a")));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.beta", "component.c", "component.d")));

  auto graph = AssemblyConstraintGraph::build(assembly);
  REQUIRE(graph);

  CHECK(graph.value().node_count() == 5U);
  CHECK(graph.value().edge_count() == 3U);
  CHECK(id_values(graph.value().nodes()) ==
        std::vector<std::string>{"component.a", "component.b", "component.c", "component.d",
                                 "component.z"});

  REQUIRE(graph.value().edges().size() == 3U);
  CHECK(graph.value().edges()[0].constraint().value() == "constraint.alpha");
  CHECK(graph.value().edges()[0].component_a().value() == "component.b");
  CHECK(graph.value().edges()[0].component_b().value() == "component.a");
  CHECK(graph.value().edges()[1].constraint().value() == "constraint.beta");
  CHECK(graph.value().edges()[2].constraint().value() == "constraint.zeta");
}

TEST_CASE("AssemblyConstraintGraph adjacency is deterministic and preserves multi edges",
          "[core][assembly-constraint-graph]") {
  auto assembly = make_graph_assembly();
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.zeta", "component.a", "component.b")));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.alpha", "component.a", "component.b")));
  REQUIRE(assembly.add_constraint(make_constraint("constraint.inactive", "component.a", "component.c",
                                                  AssemblyConstraintState::Inactive)));

  auto graph = AssemblyConstraintGraph::build(assembly);
  REQUIRE(graph);

  auto adjacent_a = graph.value().adjacent_constraints(ComponentInstanceId("component.a"));
  REQUIRE(adjacent_a);
  CHECK(constraint_id_values(adjacent_a.value()) ==
        std::vector<std::string>{"constraint.alpha", "constraint.zeta"});

  auto adjacent_b = graph.value().adjacent_constraints(ComponentInstanceId("component.b"));
  REQUIRE(adjacent_b);
  CHECK(constraint_id_values(adjacent_b.value()) ==
        std::vector<std::string>{"constraint.alpha", "constraint.zeta"});

  auto adjacent_isolated = graph.value().adjacent_constraints(ComponentInstanceId("component.z"));
  REQUIRE(adjacent_isolated);
  CHECK(adjacent_isolated.value().empty());

  CHECK(graph.value().adjacent_constraints(ComponentInstanceId("")).has_error());
  CHECK(graph.value().adjacent_constraints(ComponentInstanceId("component.missing")).has_error());
}

TEST_CASE("AssemblyConstraintGraph connected components are deterministic and include isolated nodes",
          "[core][assembly-constraint-graph]") {
  auto assembly = make_graph_assembly();
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.cd", "component.d", "component.c")));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.ab.second", "component.b", "component.a")));
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.ab.first", "component.a", "component.b")));
  REQUIRE(assembly.add_constraint(make_constraint("constraint.bc.inactive", "component.b", "component.c",
                                                  AssemblyConstraintState::Inactive)));

  auto graph = AssemblyConstraintGraph::build(assembly);
  REQUIRE(graph);

  const auto groups = graph.value().connected_components();
  REQUIRE(groups.size() == 3U);
  CHECK(id_values(groups[0]) == std::vector<std::string>{"component.a", "component.b"});
  CHECK(id_values(groups[1]) == std::vector<std::string>{"component.c", "component.d"});
  CHECK(id_values(groups[2]) == std::vector<std::string>{"component.z"});
}

TEST_CASE("AssemblyConstraintGraph construction and queries do not mutate assembly intent",
          "[core][assembly-constraint-graph]") {
  auto assembly = make_graph_assembly();
  REQUIRE(assembly.add_constraint(
      make_constraint("constraint.active", "component.a", "component.b")));
  REQUIRE(assembly.add_constraint(make_constraint("constraint.inactive", "component.c", "component.d",
                                                  AssemblyConstraintState::Inactive)));

  const RigidTransform transform_a =
      assembly.find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform transform_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();
  const std::size_t constraint_count = assembly.constraint_count();

  auto graph = AssemblyConstraintGraph::build(assembly);
  REQUIRE(graph);
  REQUIRE(graph.value().adjacent_constraints(ComponentInstanceId("component.a")));
  const auto groups = graph.value().connected_components();
  REQUIRE_FALSE(groups.empty());

  CHECK(assembly.find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        transform_a);
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        transform_b);
  CHECK(assembly.constraint_count() == constraint_count);
  CHECK(assembly.find_constraint(AssemblyConstraintId("constraint.active"))->state() ==
        AssemblyConstraintState::Active);
  CHECK(assembly.find_constraint(AssemblyConstraintId("constraint.inactive"))->state() ==
        AssemblyConstraintState::Inactive);
}
