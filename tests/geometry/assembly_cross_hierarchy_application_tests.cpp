#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <utility>
#include <vector>

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

Project local_semantic_freshness_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.local_freshness"), "LocalFreshness");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(assembly.value().add_component_instance(
      component("component.a", ComponentGroundingState::Grounded)));
  REQUIRE(assembly.value().add_component_instance(
      component("component.b", ComponentGroundingState::Free)));
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.local.distance"), "local_distance",
      AssemblyConstraintType::Distance,
      local_target("component.a", "feature.base_extrude.top"),
      local_target("component.b", "feature.base_extrude.top"),
      AssemblyConstraintState::Active, distance(20.0, "constraint.local.distance"));
  REQUIRE(constraint);
  REQUIRE(assembly.value().add_constraint(constraint.value()));

  auto project = Project::create(DocumentId("project.local_freshness"), "LocalFreshness",
                                 assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

std::vector<ComponentInstanceId> local_group() {
  return {ComponentInstanceId("component.a"), ComponentInstanceId("component.b")};
}

RigidTransform child_transform(const Project& project) {
  return project.find_assembly_document(DocumentId("assembly.child"))
      ->find_component_instance(ComponentInstanceId("component.child"))
      ->transform();
}

RigidTransform left_boundary_transform(const Project& project) {
  return project.assembly()
      .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
      ->transform();
}

} // namespace

TEST_CASE("Cross-hierarchy solve result applies fresh authority proposals atomically",
          "[geometry][assembly-cross-hierarchy-application]") {
  Project project = distance_project();
  const RigidTransform source_child = child_transform(project);
  const RigidTransform source_boundary = left_boundary_transform(project);
  const auto result = solve_only_group(project);

  REQUIRE(result.converged());
  REQUIRE(result.authority_snapshots.size() == 2U);
  REQUIRE(result.relationship_snapshots.size() == 1U);
  REQUIRE(result.hierarchy_boundary_snapshots.size() == 1U);
  REQUIRE(result.semantic_target_part_snapshots.size() == 1U);
  REQUIRE(result.proposed_transforms.size() == 1U);
  CHECK(child_transform(project) == source_child);

  const AssemblyCrossHierarchySolveResultApplier applier;
  auto applied = applier.apply(project, result);
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(child_transform(project).translation_mm.z == Approx(20.0).margin(1.0e-5));
  CHECK(left_boundary_transform(project) == source_boundary);
}

TEST_CASE("Cross-hierarchy application ignores visibility because visibility is not solve input",
          "[geometry][assembly-cross-hierarchy-application]") {
  Project project = distance_project();
  const auto result = solve_only_group(project);
  REQUIRE(project.assembly().set_subassembly_instance_visibility(
      SubassemblyInstanceId("subassembly.left"), ComponentVisibility::Hidden));
  REQUIRE(project.find_assembly_document(DocumentId("assembly.child"))
              ->set_component_instance_visibility(ComponentInstanceId("component.child"),
                                                  ComponentVisibility::Hidden));

  const AssemblyCrossHierarchySolveResultApplier applier;
  auto applied = applier.apply(project, result);
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(child_transform(project).translation_mm.z == Approx(20.0).margin(1.0e-5));
}

TEST_CASE("Cross-hierarchy application rejects stale transform-authority inputs",
          "[geometry][assembly-cross-hierarchy-application]") {
  const AssemblyCrossHierarchySolveResultApplier applier;

  SECTION("Direct transform changed") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    REQUIRE(project.find_assembly_document(DocumentId("assembly.child"))
                ->set_component_instance_transform(
                    ComponentInstanceId("component.child"),
                    RigidTransform{Vector3{1.0, 0.0, 0.0}, Vector3{}}));
    const RigidTransform changed = child_transform(project);
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == changed);
  }

  SECTION("Grounding changed") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    REQUIRE(project.find_assembly_document(DocumentId("assembly.child"))
                ->set_component_instance_grounding_state(
                    ComponentInstanceId("component.child"), ComponentGroundingState::Grounded));
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Suppression changed") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    REQUIRE(project.find_assembly_document(DocumentId("assembly.child"))
                ->set_component_instance_suppression_state(
                    ComponentInstanceId("component.child"),
                    ComponentSuppressionState::Suppressed));
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }
}

TEST_CASE("Cross-hierarchy application protects complete relationship inputs and participation",
          "[geometry][assembly-cross-hierarchy-application]") {
  const Project source = distance_project();
  const auto result = solve_only_group(source);
  const AssemblyCrossHierarchySolveResultApplier applier;

  SECTION("Distance value changed") {
    Project changed = distance_project(25.0);
    CHECK(applier.apply(changed, result).has_error());
    CHECK(child_transform(changed) == identity_rigid_transform());
  }

  SECTION("Target changed") {
    Project changed = distance_project(20.0, false, false, "feature.base_extrude.bottom");
    CHECK(applier.apply(changed, result).has_error());
    CHECK(child_transform(changed) == identity_rigid_transform());
  }

  SECTION("Relationship removed") {
    Project changed = distance_project();
    changed.cross_hierarchy_constraints().clear();
    REQUIRE(changed.validate_assembly_structure());
    CHECK(applier.apply(changed, result).has_error());
    CHECK(child_transform(changed) == identity_rigid_transform());
  }

  SECTION("New active relationship changed current solve group") {
    Project changed = distance_project(20.0, false, true);
    CHECK(applier.apply(changed, result).has_error());
    CHECK(child_transform(changed) == identity_rigid_transform());
  }
}

TEST_CASE("Cross-hierarchy application protects every participating hierarchy path boundary",
          "[geometry][assembly-cross-hierarchy-application]") {
  const AssemblyCrossHierarchySolveResultApplier applier;

  SECTION("Boundary transform changed") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    const RigidTransform changed{Vector3{0.0, 0.0, 3.0}, Vector3{}};
    REQUIRE(project.assembly().set_subassembly_instance_transform(
        SubassemblyInstanceId("subassembly.left"), changed));
    CHECK(applier.apply(project, result).has_error());
    CHECK(left_boundary_transform(project) == changed);
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Boundary suppression changed") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    REQUIRE(project.assembly().set_subassembly_instance_suppression_state(
        SubassemblyInstanceId("subassembly.left"), ComponentSuppressionState::Suppressed));
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }
}

TEST_CASE("Semantic target-producing PartDocument model intent invalidates solve results",
          "[geometry][assembly-cross-hierarchy-application][geometry][assembly-semantic-freshness]") {
  SECTION("Cross-hierarchy result") {
    Project project = distance_project();
    const auto result = solve_only_group(project);
    auto thickness = Quantity::length_mm(10.0, "part.thickness");
    REQUIRE(thickness);
    REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
                ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));

    const AssemblyCrossHierarchySolveResultApplier applier;
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Ordinary local result uses the same exact PartDocument freshness contract") {
    Project project = local_semantic_freshness_project();
    const AssemblyRigidBodySolver solver;
    auto result = solver.solve(project, local_group());
    REQUIRE(result);
    REQUIRE(result.value().converged());
    REQUIRE(result.value().semantic_target_part_snapshots.size() == 1U);

    auto thickness = Quantity::length_mm(10.0, "part.thickness");
    REQUIRE(thickness);
    REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
                ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));
    const RigidTransform source_b =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

    const AssemblySolveResultApplier applier;
    CHECK(applier.apply(project, result.value()).has_error());
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == source_b);
  }
}

TEST_CASE("Cross-hierarchy application rejects incomplete or contradictory result snapshots",
          "[geometry][assembly-cross-hierarchy-application]") {
  const AssemblyCrossHierarchySolveResultApplier applier;

  SECTION("Duplicate authority snapshot") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.authority_snapshots.push_back(result.authority_snapshots.front());
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Duplicate proposal") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.proposed_transforms.push_back(result.proposed_transforms.front());
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Missing free-authority proposal") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.proposed_transforms.clear();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Missing relationship snapshot") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.relationship_snapshots.clear();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Missing hierarchy boundary snapshot") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.hierarchy_boundary_snapshots.clear();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Missing semantic target part snapshot") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.semantic_target_part_snapshots.clear();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Invalid proposed transform") {
    Project project = distance_project();
    auto result = solve_only_group(project);
    result.proposed_transforms.front().proposed_transform.translation_mm.x =
        std::numeric_limits<double>::quiet_NaN();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }
}
