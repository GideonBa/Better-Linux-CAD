#include "blcad/core/assembly_joint_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

using namespace blcad;

namespace {

AssemblyConstraintTarget target(const char* component) {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component), "feature.hole.seat");
  REQUIRE(value);
  return value.value();
}

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyJoint joint(const char* id, const char* a, const char* b,
                    AssemblyJointState state = AssemblyJointState::Active) {
  auto value = AssemblyJoint::create(AssemblyJointId(id), id, AssemblyJointType::Revolute,
                                     target(a), target(b), state, angle(-90.0, id),
                                     angle(90.0, id), angle(0.0, id));
  REQUIRE(value);
  return value.value();
}

AssemblyDocument assembly_with_components(std::initializer_list<const char*> ids) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.joint_graph"), "JointGraph");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  for (const char* id : ids) {
    auto component = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.shared"));
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }
  return assembly.value();
}

} // namespace

TEST_CASE("AssemblyJointGraph derives sorted active multi-edges and isolated nodes",
          "[core][assembly-joint-graph]") {
  auto assembly = assembly_with_components({"component.c", "component.a", "component.b",
                                             "component.isolated"});
  REQUIRE(assembly.add_joint(joint("joint.z", "component.b", "component.c")));
  REQUIRE(assembly.add_joint(joint("joint.a", "component.a", "component.b")));
  REQUIRE(assembly.add_joint(
      joint("joint.inactive", "component.a", "component.c", AssemblyJointState::Inactive)));
  REQUIRE(assembly.add_joint(joint("joint.parallel", "component.a", "component.b")));

  const auto graph = AssemblyJointGraph::build(assembly);
  REQUIRE(graph);
  REQUIRE(graph.value().node_count() == 4U);
  REQUIRE(graph.value().edge_count() == 3U);
  CHECK(graph.value().nodes()[0].value() == "component.a");
  CHECK(graph.value().nodes()[1].value() == "component.b");
  CHECK(graph.value().nodes()[2].value() == "component.c");
  CHECK(graph.value().nodes()[3].value() == "component.isolated");
  CHECK(graph.value().edges()[0].joint().value() == "joint.a");
  CHECK(graph.value().edges()[1].joint().value() == "joint.parallel");
  CHECK(graph.value().edges()[2].joint().value() == "joint.z");

  const auto adjacent = graph.value().adjacent_joints(ComponentInstanceId("component.a"));
  REQUIRE(adjacent);
  REQUIRE(adjacent.value().size() == 2U);
  CHECK(adjacent.value()[0].value() == "joint.a");
  CHECK(adjacent.value()[1].value() == "joint.parallel");
  CHECK(graph.value().adjacent_joints(ComponentInstanceId("component.missing")).has_error());

  const auto groups = graph.value().connected_components();
  REQUIRE(groups.size() == 2U);
  REQUIRE(groups[0].size() == 3U);
  CHECK(groups[0][0].value() == "component.a");
  CHECK(groups[0][1].value() == "component.b");
  CHECK(groups[0][2].value() == "component.c");
  REQUIRE(groups[1].size() == 1U);
  CHECK(groups[1][0].value() == "component.isolated");
}

TEST_CASE("AssemblyJointGraph ordering is independent of joint insertion order",
          "[core][assembly-joint-graph]") {
  auto forward = assembly_with_components({"component.a", "component.b", "component.c"});
  auto reverse = assembly_with_components({"component.a", "component.b", "component.c"});

  REQUIRE(forward.add_joint(joint("joint.a", "component.a", "component.b")));
  REQUIRE(forward.add_joint(joint("joint.z", "component.b", "component.c")));
  REQUIRE(reverse.add_joint(joint("joint.z", "component.b", "component.c")));
  REQUIRE(reverse.add_joint(joint("joint.a", "component.a", "component.b")));

  const auto forward_graph = AssemblyJointGraph::build(forward);
  const auto reverse_graph = AssemblyJointGraph::build(reverse);
  REQUIRE(forward_graph);
  REQUIRE(reverse_graph);
  CHECK(forward_graph.value().nodes() == reverse_graph.value().nodes());
  REQUIRE(forward_graph.value().edges().size() == reverse_graph.value().edges().size());
  for (std::size_t index = 0U; index < forward_graph.value().edges().size(); ++index) {
    CHECK(forward_graph.value().edges()[index].joint() == reverse_graph.value().edges()[index].joint());
    CHECK(forward_graph.value().edges()[index].component_a() ==
          reverse_graph.value().edges()[index].component_a());
    CHECK(forward_graph.value().edges()[index].component_b() ==
          reverse_graph.value().edges()[index].component_b());
  }
  CHECK(forward_graph.value().connected_components() ==
        reverse_graph.value().connected_components());
}
