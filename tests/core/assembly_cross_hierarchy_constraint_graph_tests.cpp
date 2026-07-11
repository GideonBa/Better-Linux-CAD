#include "blcad/core/assembly_cross_hierarchy_constraint_graph.hpp"

#include "blcad/core/project.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;

namespace {

AssemblyConstraintTarget local_target(const char* component, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint local_constraint(const char* id,
                                    const char* component_a,
                                    const char* component_b,
                                    AssemblyConstraintState state =
                                        AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Mate,
      local_target(component_a, "feature.local.a"),
      local_target(component_b, "feature.local.b"), state);
  REQUIRE(constraint);
  return constraint.value();
}

AssemblyHierarchyConstraintEndpoint hierarchy_endpoint(
    std::initializer_list<const char*> path,
    const char* component,
    const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* occurrence : path) {
    occurrence_path.emplace_back(occurrence);
  }
  auto endpoint = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference);
  REQUIRE(endpoint);
  return endpoint.value();
}

AssemblyHierarchyConstraint cross_constraint(
    const char* id,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Concentric,
      std::move(target_a), std::move(target_b), state);
  REQUIRE(constraint);
  return constraint.value();
}

ComponentInstance component(const char* id,
                            ComponentVisibility visibility = ComponentVisibility::Visible,
                            ComponentSuppressionState suppression =
                                ComponentSuppressionState::Active) {
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), visibility, suppression,
      ComponentGroundingState::Free);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance subassembly(const char* id,
                                const char* child,
                                ComponentVisibility visibility = ComponentVisibility::Visible,
                                ComponentSuppressionState suppression =
                                    ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(child), visibility, suppression);
  REQUIRE(instance);
  return instance.value();
}

AssemblyDocument make_root() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(component("component.root.a")));
  REQUIRE(root.value().add_component_instance(component("component.root.b")));
  REQUIRE(root.value().add_component_instance(component("component.root.c")));
  REQUIRE(root.value().add_component_instance(component("component.root.d")));
  REQUIRE(root.value().add_component_instance(component("component.root.e")));
  REQUIRE(root.value().add_subassembly_instance(
      subassembly("subassembly.left", "assembly.gearbox")));
  REQUIRE(root.value().add_subassembly_instance(
      subassembly("subassembly.right", "assembly.gearbox")));
  REQUIRE(root.value().add_subassembly_instance(
      subassembly("subassembly.outer", "assembly.outer")));
  REQUIRE(root.value().add_constraint(
      local_constraint("constraint.local.root_only", "component.root.d", "component.root.e")));
  REQUIRE(root.value().add_constraint(local_constraint(
      "constraint.local.root_inactive", "component.root.a", "component.root.b",
      AssemblyConstraintState::Inactive)));
  return root.value();
}

AssemblyDocument make_gearbox() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.gearbox"), "Gearbox");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(component("component.shaft")));
  REQUIRE(assembly.value().add_component_instance(component("component.bearing")));
  REQUIRE(assembly.value().add_constraint(local_constraint(
      "constraint.local.shaft_bearing", "component.shaft", "component.bearing")));
  return assembly.value();
}

AssemblyDocument make_outer() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.outer"), "Outer");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_subassembly_instance(
      subassembly("subassembly.inner", "assembly.nested")));
  return assembly.value();
}

AssemblyDocument make_nested() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.nested"), "Nested");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(component("component.leaf")));
  return assembly.value();
}

std::vector<AssemblyHierarchyConstraint> cross_constraints() {
  std::vector<AssemblyHierarchyConstraint> constraints;
  constraints.push_back(cross_constraint(
      "constraint.cross.left_root",
      hierarchy_endpoint({"subassembly.left"}, "component.shaft", "feature.bore.axis"),
      hierarchy_endpoint({}, "component.root.a", "feature.root.axis")));
  constraints.push_back(cross_constraint(
      "constraint.cross.right_root",
      hierarchy_endpoint({"subassembly.right"}, "component.shaft", "feature.bore.axis"),
      hierarchy_endpoint({}, "component.root.b", "feature.root.axis")));
  constraints.push_back(cross_constraint(
      "constraint.cross.same_authority",
      hierarchy_endpoint({"subassembly.left"}, "component.shaft", "feature.left.axis"),
      hierarchy_endpoint({"subassembly.right"}, "component.shaft", "feature.right.axis")));
  constraints.push_back(cross_constraint(
      "constraint.cross.nested_root",
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.leaf",
                         "feature.leaf.axis"),
      hierarchy_endpoint({}, "component.root.c", "feature.root.axis")));
  constraints.push_back(cross_constraint(
      "constraint.cross.inactive",
      hierarchy_endpoint({"subassembly.left"}, "component.bearing", "feature.bearing.axis"),
      hierarchy_endpoint({}, "component.root.c", "feature.root.axis"),
      AssemblyConstraintState::Inactive));
  return constraints;
}

Project graph_project(bool reverse_insertion = false) {
  auto project = Project::create(DocumentId("project.cross.graph"), "CrossGraph", make_root());
  REQUIRE(project);

  auto shared_part = PartDocument::create(DocumentId("part.shared"), "Shared");
  REQUIRE(shared_part);
  REQUIRE(project.value().add_part_document(std::move(shared_part.value())));

  std::vector<AssemblyDocument> children;
  children.push_back(make_gearbox());
  children.push_back(make_outer());
  children.push_back(make_nested());
  if (reverse_insertion) {
    std::reverse(children.begin(), children.end());
  }
  for (AssemblyDocument& child : children) {
    REQUIRE(project.value().add_child_assembly_document(std::move(child)));
  }

  auto constraints = cross_constraints();
  if (reverse_insertion) {
    std::reverse(constraints.begin(), constraints.end());
  }
  for (AssemblyHierarchyConstraint& constraint : constraints) {
    REQUIRE(project.value().add_cross_hierarchy_constraint(std::move(constraint)));
  }

  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

bool is_local(const AssemblyRelationshipIdentity& relationship,
              std::string_view assembly,
              std::string_view constraint) {
  if (!std::holds_alternative<AssemblyLocalRelationshipIdentity>(relationship)) {
    return false;
  }
  const auto& local = std::get<AssemblyLocalRelationshipIdentity>(relationship);
  return local.assembly_document.value() == assembly && local.constraint.value() == constraint;
}

bool is_cross(const AssemblyRelationshipIdentity& relationship, std::string_view constraint) {
  if (!std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(relationship)) {
    return false;
  }
  return std::get<AssemblyProjectCrossHierarchyRelationshipIdentity>(relationship)
             .constraint.value() == constraint;
}

std::size_t relationship_occurrences(const AssemblyCrossHierarchyConstraintGraph& graph,
                                     std::string_view assembly,
                                     std::string_view constraint) {
  return static_cast<std::size_t>(std::count_if(
      graph.relationships().begin(), graph.relationships().end(),
      [assembly, constraint](const AssemblyRelationshipIdentity& relationship) {
        return is_local(relationship, assembly, constraint);
      }));
}

std::size_t cross_incidence_count(const AssemblyCrossHierarchyConstraintGraph& graph,
                                  std::string_view constraint) {
  return static_cast<std::size_t>(std::count_if(
      graph.incidences().begin(), graph.incidences().end(),
      [constraint](const AssemblyRelationshipAuthorityIncidence& incidence) {
        return is_cross(incidence.relationship, constraint);
      }));
}

bool group_contains(const AssemblyCrossHierarchySolveGroup& group,
                    std::string_view constraint) {
  return std::any_of(group.relationships.begin(), group.relationships.end(),
                     [constraint](const AssemblyRelationshipIdentity& relationship) {
                       return is_cross(relationship, constraint);
                     });
}

} // namespace

TEST_CASE("Cross-hierarchy incidence graph derives shared transform authorities and solve groups",
          "[core][assembly-cross-hierarchy-graph]") {
  const Project project = graph_project();
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);

  CHECK(graph.value().relationship_count() == 6U);
  CHECK(graph.value().authority_count() == 8U);
  CHECK(graph.value().incidence_count() == 11U);
  CHECK(graph.value().endpoint_mapping_count() == 8U);
  CHECK(graph.value().solve_group_count() == 2U);

  CHECK(relationship_occurrences(graph.value(), "assembly.gearbox",
                                 "constraint.local.shaft_bearing") == 1U);
  CHECK(cross_incidence_count(graph.value(), "constraint.cross.same_authority") == 1U);

  const auto mapping_begin = std::find_if(
      graph.value().endpoint_mappings().begin(), graph.value().endpoint_mappings().end(),
      [](const AssemblyCrossHierarchyEndpointAuthorityMapping& mapping) {
        return mapping.constraint.value() == "constraint.cross.same_authority";
      });
  REQUIRE(mapping_begin != graph.value().endpoint_mappings().end());
  const auto mapping_next = std::next(mapping_begin);
  REQUIRE(mapping_next != graph.value().endpoint_mappings().end());
  CHECK(mapping_begin->role == AssemblyHierarchyConstraintEndpointRole::TargetA);
  CHECK(mapping_next->role == AssemblyHierarchyConstraintEndpointRole::TargetB);
  REQUIRE(mapping_begin->endpoint.occurrence_path().size() == 1U);
  REQUIRE(mapping_next->endpoint.occurrence_path().size() == 1U);
  CHECK(mapping_begin->endpoint.occurrence_path()[0].value() == "subassembly.left");
  CHECK(mapping_next->endpoint.occurrence_path()[0].value() == "subassembly.right");
  CHECK(mapping_begin->authority == mapping_next->authority);
  CHECK(mapping_begin->authority.assembly_document.value() == "assembly.gearbox");
  CHECK(mapping_begin->authority.component_instance.value() == "component.shaft");

  REQUIRE(graph.value().solve_groups().size() == 2U);
  const auto& shared_group = graph.value().solve_groups()[0];
  CHECK(group_contains(shared_group, "constraint.cross.left_root"));
  CHECK(group_contains(shared_group, "constraint.cross.right_root"));
  CHECK(group_contains(shared_group, "constraint.cross.same_authority"));
  CHECK(std::any_of(shared_group.relationships.begin(), shared_group.relationships.end(),
                    [](const AssemblyRelationshipIdentity& relationship) {
                      return is_local(relationship, "assembly.gearbox",
                                      "constraint.local.shaft_bearing");
                    }));
  CHECK(shared_group.authorities.size() == 4U);

  const auto& nested_group = graph.value().solve_groups()[1];
  CHECK(group_contains(nested_group, "constraint.cross.nested_root"));
  CHECK(nested_group.authorities.size() == 2U);

  CHECK(std::none_of(graph.value().solve_groups().begin(), graph.value().solve_groups().end(),
                     [](const AssemblyCrossHierarchySolveGroup& group) {
                       return std::any_of(
                           group.relationships.begin(), group.relationships.end(),
                           [](const AssemblyRelationshipIdentity& relationship) {
                             return is_local(relationship, "assembly.root",
                                             "constraint.local.root_only");
                           });
                     }));
}

TEST_CASE("Cross-hierarchy incidence maps nested endpoints to reached document authorities",
          "[core][assembly-cross-hierarchy-graph]") {
  const Project project = graph_project();
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);

  const auto mapping = std::find_if(
      graph.value().endpoint_mappings().begin(), graph.value().endpoint_mappings().end(),
      [](const AssemblyCrossHierarchyEndpointAuthorityMapping& candidate) {
        return candidate.constraint.value() == "constraint.cross.nested_root" &&
               candidate.role == AssemblyHierarchyConstraintEndpointRole::TargetA;
      });
  REQUIRE(mapping != graph.value().endpoint_mappings().end());
  REQUIRE(mapping->endpoint.occurrence_path().size() == 2U);
  CHECK(mapping->endpoint.occurrence_path()[0].value() == "subassembly.outer");
  CHECK(mapping->endpoint.occurrence_path()[1].value() == "subassembly.inner");
  CHECK(mapping->authority.assembly_document.value() == "assembly.nested");
  CHECK(mapping->authority.component_instance.value() == "component.leaf");
}

TEST_CASE("Cross-hierarchy incidence suppression filters only affected relationships",
          "[core][assembly-cross-hierarchy-graph]") {
  Project path_suppressed = graph_project();
  REQUIRE(path_suppressed.assembly().set_subassembly_instance_suppression_state(
      SubassemblyInstanceId("subassembly.left"), ComponentSuppressionState::Suppressed));
  auto path_graph = AssemblyCrossHierarchyConstraintGraph::build(path_suppressed);
  REQUIRE(path_graph);

  CHECK(std::none_of(path_graph.value().relationships().begin(),
                     path_graph.value().relationships().end(),
                     [](const AssemblyRelationshipIdentity& relationship) {
                       return is_cross(relationship, "constraint.cross.left_root") ||
                              is_cross(relationship, "constraint.cross.same_authority");
                     }));
  CHECK(std::any_of(path_graph.value().relationships().begin(),
                    path_graph.value().relationships().end(),
                    [](const AssemblyRelationshipIdentity& relationship) {
                      return is_cross(relationship, "constraint.cross.right_root");
                    }));
  CHECK(relationship_occurrences(path_graph.value(), "assembly.gearbox",
                                 "constraint.local.shaft_bearing") == 1U);
  CHECK(path_graph.value().solve_group_count() == 2U);

  Project component_suppressed = graph_project();
  AssemblyDocument* gearbox =
      component_suppressed.find_assembly_document(DocumentId("assembly.gearbox"));
  REQUIRE(gearbox != nullptr);
  REQUIRE(gearbox->set_component_instance_suppression_state(
      ComponentInstanceId("component.shaft"), ComponentSuppressionState::Suppressed));
  auto component_graph = AssemblyCrossHierarchyConstraintGraph::build(component_suppressed);
  REQUIRE(component_graph);

  CHECK(relationship_occurrences(component_graph.value(), "assembly.gearbox",
                                 "constraint.local.shaft_bearing") == 0U);
  CHECK(std::none_of(component_graph.value().relationships().begin(),
                     component_graph.value().relationships().end(),
                     [](const AssemblyRelationshipIdentity& relationship) {
                       return is_cross(relationship, "constraint.cross.left_root") ||
                              is_cross(relationship, "constraint.cross.right_root") ||
                              is_cross(relationship, "constraint.cross.same_authority");
                     }));
  CHECK(component_graph.value().solve_group_count() == 1U);
  CHECK(group_contains(component_graph.value().solve_groups()[0],
                       "constraint.cross.nested_root"));
}

TEST_CASE("Cross-hierarchy incidence ignores visibility and excludes inactive relationships",
          "[core][assembly-cross-hierarchy-graph]") {
  const Project baseline_project = graph_project();
  auto baseline = AssemblyCrossHierarchyConstraintGraph::build(baseline_project);
  REQUIRE(baseline);

  Project hidden = graph_project();
  REQUIRE(hidden.assembly().set_subassembly_instance_visibility(
      SubassemblyInstanceId("subassembly.left"), ComponentVisibility::Hidden));
  AssemblyDocument* gearbox = hidden.find_assembly_document(DocumentId("assembly.gearbox"));
  REQUIRE(gearbox != nullptr);
  REQUIRE(gearbox->set_component_instance_visibility(ComponentInstanceId("component.shaft"),
                                                      ComponentVisibility::Hidden));
  auto hidden_graph = AssemblyCrossHierarchyConstraintGraph::build(hidden);
  REQUIRE(hidden_graph);

  CHECK(hidden_graph.value().relationships() == baseline.value().relationships());
  CHECK(hidden_graph.value().authorities() == baseline.value().authorities());
  CHECK(hidden_graph.value().incidences() == baseline.value().incidences());
  CHECK(hidden_graph.value().endpoint_mappings() == baseline.value().endpoint_mappings());
  CHECK(hidden_graph.value().solve_groups() == baseline.value().solve_groups());

  CHECK(std::none_of(hidden_graph.value().relationships().begin(),
                     hidden_graph.value().relationships().end(),
                     [](const AssemblyRelationshipIdentity& relationship) {
                       return is_cross(relationship, "constraint.cross.inactive") ||
                              is_local(relationship, "assembly.root",
                                       "constraint.local.root_inactive");
                     }));
}

TEST_CASE("Cross-hierarchy incidence and groups are insertion-order independent",
          "[core][assembly-cross-hierarchy-graph]") {
  const Project forward_project = graph_project(false);
  const Project reverse_project = graph_project(true);
  auto forward = AssemblyCrossHierarchyConstraintGraph::build(forward_project);
  auto reverse = AssemblyCrossHierarchyConstraintGraph::build(reverse_project);
  REQUIRE(forward);
  REQUIRE(reverse);

  CHECK(forward.value().relationships() == reverse.value().relationships());
  CHECK(forward.value().authorities() == reverse.value().authorities());
  CHECK(forward.value().incidences() == reverse.value().incidences());
  CHECK(forward.value().endpoint_mappings() == reverse.value().endpoint_mappings());
  CHECK(forward.value().solve_groups() == reverse.value().solve_groups());
}

TEST_CASE("Cross-hierarchy incidence validates project structure and never mutates source intent",
          "[core][assembly-cross-hierarchy-graph]") {
  Project project = graph_project();
  const RigidTransform root_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.root.a"))->transform();
  const RigidTransform child_transform = project
                                             .find_assembly_document(DocumentId("assembly.gearbox"))
                                             ->find_component_instance(
                                                 ComponentInstanceId("component.shaft"))
                                             ->transform();
  const RigidTransform boundary_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
          ->transform();

  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.root.a"))
            ->transform() == root_transform);
  CHECK(project.find_assembly_document(DocumentId("assembly.gearbox"))
            ->find_component_instance(ComponentInstanceId("component.shaft"))
            ->transform() == child_transform);
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
            ->transform() == boundary_transform);

  Project invalid = graph_project();
  REQUIRE(invalid.add_cross_hierarchy_constraint(cross_constraint(
      "constraint.cross.invalid_component",
      hierarchy_endpoint({"subassembly.left"}, "component.missing", "feature.missing.axis"),
      hierarchy_endpoint({}, "component.root.a", "feature.root.axis"))));
  CHECK(AssemblyCrossHierarchyConstraintGraph::build(invalid).has_error());
}
