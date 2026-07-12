#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <variant>

using namespace blcad;
using namespace blcad::geometry;
using namespace blcad::geometry::test_support;
using Catch::Approx;

namespace {

AssemblyConstraintTarget local_target(const char* component, const char* reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), reference);
  REQUIRE(target);
  return target.value();
}

Project mixed_relationship_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.left", RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free)));
  REQUIRE(child.value().add_component_instance(component(
      "component.anchor", ComponentGroundingState::Grounded,
      RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}})));
  auto local = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.local.child_anchor"), "child_anchor",
      AssemblyConstraintType::Mate,
      local_target("component.child", "feature.base_extrude.top"),
      local_target("component.anchor", "feature.base_extrude.bottom"));
  REQUIRE(local);
  REQUIRE(child.value().add_constraint(local.value()));

  auto project = Project::create(DocumentId("project.mixed_diagnostics"), "MixedDiagnostics",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  auto cross = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.root_child"), "root_child",
      AssemblyConstraintType::Mate,
      endpoint({}, "component.root", "feature.base_extrude.top"),
      endpoint({"subassembly.left"}, "component.child", "feature.base_extrude.bottom"));
  REQUIRE(cross);
  REQUIRE(project.value().add_cross_hierarchy_constraint(cross.value()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("Cross-hierarchy diagnostics use exact Block-26 free-authority variable order",
          "[geometry][assembly-cross-hierarchy-diagnostics]") {
  const Project project = distance_project();
  const auto group = only_group(project);
  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer analyzer;
  auto diagnostics = analyzer.analyze(project, group);

  REQUIRE(diagnostics);
  CHECK(diagnostics.value().solve_state == AssemblySolveState::Converged);
  CHECK(diagnostics.value().consistency_classification ==
        AssemblyConstraintConsistencyClassification::LocallyConsistent);
  CHECK(diagnostics.value().dof_classification == AssemblyDofClassification::Underconstrained);
  CHECK(diagnostics.value().residual_rank_structure ==
        AssemblyResidualRankStructure::RedundantResidualComponents);
  CHECK(diagnostics.value().relationship_order == group.relationships);
  REQUIRE(diagnostics.value().fixed_authorities.size() == 1U);
  REQUIRE(diagnostics.value().variable_authorities.size() == 1U);

  const ComponentTransformAuthority expected_child{
      DocumentId("assembly.child"), ComponentInstanceId("component.child")};
  CHECK(diagnostics.value().variable_authorities.front() == expected_child);
  CHECK(diagnostics.value().rank_summary.rank_evaluated);
  CHECK(diagnostics.value().rank_summary.residual_component_count == 4U);
  CHECK(diagnostics.value().rank_summary.variable_count == 6U);
  CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
  CHECK(diagnostics.value().rank_summary.constrained_dof == 3U);
  CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);
  CHECK(diagnostics.value().rank_summary.residual_row_redundancy == 1U);
  CHECK(diagnostics.value().residual_summary.final_rms <= 1.0e-8);
}

TEST_CASE("Repeated cross-hierarchy occurrences share one six-variable diagnostics block",
          "[geometry][assembly-cross-hierarchy-diagnostics]") {
  const Project project = repeated_occurrence_project();
  const auto group = only_group(project);
  REQUIRE(group.relationships.size() == 2U);
  REQUIRE(group.authorities.size() == 2U);

  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer analyzer;
  auto diagnostics = analyzer.analyze(project, group);
  REQUIRE(diagnostics);
  REQUIRE(diagnostics.value().variable_authorities.size() == 1U);
  CHECK(diagnostics.value().rank_summary.variable_count == 6U);
  CHECK(diagnostics.value().rank_summary.variable_count != 12U);
  CHECK(diagnostics.value().rank_summary.residual_component_count == 8U);
  CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
  CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);

  const Project reverse = repeated_occurrence_project(true);
  auto reverse_diagnostics = analyzer.analyze(reverse, only_group(reverse));
  REQUIRE(reverse_diagnostics);
  CHECK(reverse_diagnostics.value() == diagnostics.value());
}

TEST_CASE("Cross-hierarchy diagnostics retain canonical mixed local and project relationship order",
          "[geometry][assembly-cross-hierarchy-diagnostics]") {
  const Project project = mixed_relationship_project();
  const auto group = only_group(project);
  REQUIRE(group.relationships.size() == 2U);
  REQUIRE(std::holds_alternative<AssemblyLocalRelationshipIdentity>(group.relationships[0]));
  REQUIRE(std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(
      group.relationships[1]));

  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer analyzer;
  auto diagnostics = analyzer.analyze(project, group);
  REQUIRE(diagnostics);
  CHECK(diagnostics.value().relationship_order == group.relationships);
  CHECK(diagnostics.value().rank_summary.residual_component_count == 8U);
  CHECK(diagnostics.value().rank_summary.variable_count == 6U);
  REQUIRE(diagnostics.value().variable_authorities.size() == 1U);
  CHECK(diagnostics.value().variable_authorities.front().assembly_document.value() ==
        "assembly.child");
  CHECK(diagnostics.value().variable_authorities.front().component_instance.value() ==
        "component.child");
}

TEST_CASE("Cross-hierarchy diagnostics classify all-grounded consistency before rank evaluation",
          "[geometry][assembly-cross-hierarchy-diagnostics]") {
  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer analyzer;

  SECTION("Inconsistent fixed geometry") {
    Project project = distance_project(20.0);
    REQUIRE(project.find_assembly_document(DocumentId("assembly.child"))
                ->set_component_instance_grounding_state(
                    ComponentInstanceId("component.child"), ComponentGroundingState::Grounded));
    auto diagnostics = analyzer.analyze(project, only_group(project));
    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::FixedGeometryInconsistent);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent);
    CHECK(diagnostics.value().dof_classification == AssemblyDofClassification::NotEvaluated);
    CHECK_FALSE(diagnostics.value().rank_summary.rank_evaluated);
    CHECK(diagnostics.value().rank_summary.variable_count == 0U);
    CHECK(diagnostics.value().variable_authorities.empty());
  }

  SECTION("Satisfied fixed geometry has no variable DOF") {
    Project project = distance_project(20.0);
    AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
    REQUIRE(child != nullptr);
    REQUIRE(child->set_component_instance_transform(
        ComponentInstanceId("component.child"),
        RigidTransform{Vector3{0.0, 0.0, 20.0}, Vector3{}}));
    REQUIRE(child->set_component_instance_grounding_state(
        ComponentInstanceId("component.child"), ComponentGroundingState::Grounded));
    auto diagnostics = analyzer.analyze(project, only_group(project));
    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::Converged);
    CHECK(diagnostics.value().dof_classification == AssemblyDofClassification::NoVariableDof);
    CHECK(diagnostics.value().rank_summary.rank_evaluated);
    CHECK(diagnostics.value().rank_summary.variable_count == 0U);
    CHECK(diagnostics.value().rank_summary.jacobian_rank == 0U);
    CHECK(diagnostics.value().rank_summary.remaining_dof == 0U);
    CHECK(diagnostics.value().rank_summary.residual_row_redundancy == 4U);
  }
}

TEST_CASE("Cross-hierarchy diagnostics validate rank tolerances through the shared options contract",
          "[geometry][assembly-cross-hierarchy-diagnostics]") {
  const Project project = distance_project();
  AssemblySolveDiagnosticsOptions options;
  options.rank_absolute_tolerance = 0.0;
  options.rank_relative_tolerance = 0.0;

  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer analyzer;
  CHECK(analyzer.analyze(project, only_group(project), options).has_error());

  options.rank_absolute_tolerance = std::numeric_limits<double>::quiet_NaN();
  options.rank_relative_tolerance = 1.0e-8;
  CHECK(analyzer.analyze(project, only_group(project), options).has_error());
}
