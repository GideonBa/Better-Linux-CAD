#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using namespace blcad::geometry::test_support;
using Catch::Approx;

namespace {

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyConstraintTarget local_target(const char* component_id, const char* reference) {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component_id), reference);
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyJoint cross_joint(
    const char* id = "joint.cross.revolute", double coordinate = 0.0,
    AssemblyJointState state = AssemblyJointState::Active,
    AssemblyHierarchyConstraintEndpoint target_a =
        endpoint({}, "component.root", "feature.hole.seat"),
    AssemblyHierarchyConstraintEndpoint target_b =
        endpoint({"subassembly.left"}, "component.child", "feature.hole.seat")) {
  auto value = AssemblyHierarchyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Revolute, std::move(target_a),
      std::move(target_b), state, angle(-90.0, id), angle(90.0, id), angle(coordinate, id));
  REQUIRE(value);
  return value.value();
}

AssemblyJoint local_joint(const char* id, double coordinate = 0.0) {
  auto value = AssemblyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Revolute,
      local_target("component.child", "feature.hole.seat"),
      local_target("component.anchor", "feature.hole.seat"), AssemblyJointState::Active,
      angle(-90.0, id), angle(90.0, id), angle(coordinate, id));
  REQUIRE(value);
  return value.value();
}

AssemblyConstraint local_concentric(const char* id) {
  auto value = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Concentric,
      local_target("component.child", "feature.hole.axis"),
      local_target("component.anchor", "feature.hole.axis"));
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraint cross_concentric(const char* id) {
  auto value = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Concentric,
      endpoint({}, "component.root", "feature.hole.axis"),
      endpoint({"subassembly.left"}, "component.child", "feature.hole.axis"));
  REQUIRE(value);
  return value.value();
}

Project motion_project(RigidTransform left_boundary = identity_rigid_transform(),
                       bool repeated = false,
                       RigidTransform right_boundary = identity_rigid_transform()) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.left", left_boundary)));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(
        occurrence("subassembly.right", right_boundary)));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free)));
  REQUIRE(child.value().add_component_instance(
      component("component.anchor", ComponentGroundingState::Grounded)));

  auto project = Project::create(DocumentId("project.cross_motion"), "CrossMotion", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

void add_joint(Project& project, AssemblyHierarchyJoint joint) {
  REQUIRE(project.add_cross_hierarchy_joint(std::move(joint)));
  REQUIRE(project.validate_assembly_structure());
}

AssemblyCrossHierarchyJointMotionResult move(Project const& project, const char* id, double degrees) {
  const AssemblyCrossHierarchyJointMotionSolver solver;
  auto requested = Quantity::angle_deg(degrees, id);
  REQUIRE(requested);
  auto result = solver.move(project, AssemblyJointId(id), requested.value());
  REQUIRE(result);
  return result.value();
}

RigidTransform child_transform(const Project& project) {
  return project.find_assembly_document(DocumentId("assembly.child"))
      ->find_component_instance(ComponentInstanceId("component.child"))
      ->transform();
}

RigidTransform left_boundary(const Project& project) {
  return project.assembly()
      .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
      ->transform();
}

Project mixed_motion_project(double second_cross_coordinate = 0.0) {
  Project project = motion_project();
  AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  REQUIRE(child->add_constraint(local_concentric("constraint.local.concentric")));
  REQUIRE(child->add_joint(local_joint("joint.local.hold", 0.0)));
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_concentric("constraint.cross.concentric")));
  REQUIRE(project.add_cross_hierarchy_joint(cross_joint("joint.cross.selected", 0.0)));
  REQUIRE(project.add_cross_hierarchy_joint(
      cross_joint("joint.cross.hold", second_cross_coordinate)));
  REQUIRE(project.validate_assembly_structure());
  return project;
}

} // namespace

TEST_CASE("Cross-hierarchy Revolute motion solves one authority-scoped root-to-child drive",
          "[geometry][assembly-cross-hierarchy-motion]") {
  Project project = motion_project();
  add_joint(project, cross_joint());
  const RigidTransform source_child = child_transform(project);
  const RigidTransform source_boundary = left_boundary(project);

  const auto result = move(project, "joint.cross.revolute", 45.0);
  REQUIRE(result.converged());
  CHECK(result.source_coordinate_deg == 0.0);
  CHECK(result.requested_coordinate_deg == 45.0);
  REQUIRE(result.fixed_authorities.size() == 1U);
  REQUIRE(result.proposed_transforms.size() == 1U);
  CHECK(result.proposed_transforms.front().authority.assembly_document.value() == "assembly.child");
  CHECK(result.proposed_transforms.front().authority.component_instance.value() == "component.child");
  CHECK(result.proposed_transforms.front().proposed_transform.rotation_deg.z ==
        Approx(45.0).margin(1.0e-4));
  CHECK(result.residual_summary.residual_component_count == 9U);
  CHECK(result.residual_summary.final_rms <= 1.0e-8);
  CHECK(child_transform(project) == source_child);
  CHECK(left_boundary(project) == source_boundary);
  CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.cross.revolute"))
            ->coordinate_deg() == 0.0);
}

TEST_CASE("Nested cross-hierarchy Revolute motion evaluates exact parent transform chains",
          "[geometry][assembly-cross-hierarchy-motion]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  auto outer_occurrence = SubassemblyInstance::create(
      SubassemblyInstanceId("subassembly.outer"), "Outer", DocumentId("assembly.outer"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{Vector3{}, Vector3{0.0, 0.0, 15.0}});
  REQUIRE(outer_occurrence);
  REQUIRE(root.value().add_subassembly_instance(outer_occurrence.value()));

  auto outer = AssemblyDocument::create(DocumentId("assembly.outer"), "Outer");
  REQUIRE(outer);
  auto inner_occurrence = SubassemblyInstance::create(
      SubassemblyInstanceId("subassembly.inner"), "Inner", DocumentId("assembly.child"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{Vector3{}, Vector3{0.0, 0.0, 20.0}});
  REQUIRE(inner_occurrence);
  REQUIRE(outer.value().add_subassembly_instance(inner_occurrence.value()));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(component("component.child")));

  auto project = Project::create(DocumentId("project.nested_motion"), "NestedMotion", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(outer.value()));
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  add_joint(project.value(), cross_joint(
      "joint.cross.nested", 0.0, AssemblyJointState::Active,
      endpoint({}, "component.root", "feature.hole.seat"),
      endpoint({"subassembly.outer", "subassembly.inner"}, "component.child",
               "feature.hole.seat")));

  const auto result = move(project.value(), "joint.cross.nested", 60.0);
  REQUIRE(result.converged());
  REQUIRE(result.proposed_transforms.size() == 1U);
  CHECK(result.proposed_transforms.front().proposed_transform.rotation_deg.z ==
        Approx(25.0).margin(1.0e-4));
}

TEST_CASE("Repeated occurrences share one authority variable block and proposal in motion",
          "[geometry][assembly-cross-hierarchy-motion]") {
  Project project = motion_project(identity_rigid_transform(), true,
                                   RigidTransform{Vector3{}, Vector3{0.0, 0.0, 30.0}});
  REQUIRE(project.add_cross_hierarchy_constraint(
      cross_concentric("constraint.cross.anchor")));
  add_joint(project, cross_joint(
      "joint.cross.same_authority", 0.0, AssemblyJointState::Active,
      endpoint({"subassembly.left"}, "component.child", "feature.hole.seat"),
      endpoint({"subassembly.right"}, "component.child", "feature.hole.seat")));

  const auto result = move(project, "joint.cross.same_authority", 30.0);
  REQUIRE(result.converged());
  REQUIRE(result.authority_snapshots.size() == 2U);
  REQUIRE(result.proposed_transforms.size() == 1U);
  CHECK(result.proposed_transforms.front().authority.assembly_document.value() == "assembly.child");
  CHECK(result.residual_summary.residual_component_count == 15U);
}

TEST_CASE("Combined motion keeps canonical four-family order and authored holding drives",
          "[geometry][assembly-cross-hierarchy-motion]") {
  Project project = mixed_motion_project();
  const auto result = move(project, "joint.cross.selected", 0.0);
  REQUIRE(result.converged());
  REQUIRE(result.relationships.size() == 5U);
  CHECK(std::holds_alternative<AssemblyLocalRelationshipIdentity>(result.relationships[0]));
  CHECK(std::holds_alternative<AssemblyLocalJointIdentity>(result.relationships[1]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(
      result.relationships[2]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyJointIdentity>(
      result.relationships[3]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyJointIdentity>(
      result.relationships[4]));
  CHECK(result.residual_summary.residual_component_count == 39U);

  Project contradictory = mixed_motion_project(30.0);
  const auto held = move(contradictory, "joint.cross.selected", 0.0);
  CHECK_FALSE(held.converged());
  CHECK(held.residual_summary.final_rms > 1.0e-6);
}

TEST_CASE("Cross-hierarchy motion applies transforms and selected coordinate atomically",
          "[geometry][assembly-cross-hierarchy-motion]") {
  Project project = motion_project();
  add_joint(project, cross_joint());
  const RigidTransform source_boundary = left_boundary(project);
  const auto result = move(project, "joint.cross.revolute", 45.0);
  REQUIRE(result.converged());

  const AssemblyCrossHierarchyJointMotionResultApplier applier;
  auto applied = applier.apply(project, result);
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(child_transform(project).rotation_deg.z == Approx(45.0).margin(1.0e-4));
  CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.cross.revolute"))
            ->coordinate_deg() == 45.0);
  CHECK(left_boundary(project) == source_boundary);
}

TEST_CASE("Cross-hierarchy motion application protects complete relationship and path inputs",
          "[geometry][assembly-cross-hierarchy-motion]") {
  const AssemblyCrossHierarchyJointMotionResultApplier applier;

  SECTION("Selected joint coordinate changed") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto result = move(project, "joint.cross.revolute", 45.0);
    REQUIRE(project.set_cross_hierarchy_joint_coordinate(
        AssemblyJointId("joint.cross.revolute"), angle(10.0, "joint.cross.revolute")));
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
  }

  SECTION("Local holding joint changed") {
    Project project = mixed_motion_project();
    const auto result = move(project, "joint.cross.selected", 0.0);
    AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
    REQUIRE(child != nullptr);
    REQUIRE(child->set_joint_coordinate(AssemblyJointId("joint.local.hold"),
                                        angle(10.0, "joint.local.hold")));
    CHECK(applier.apply(project, result).has_error());
  }

  SECTION("Geometric relationship changed") {
    Project project = mixed_motion_project();
    const auto result = move(project, "joint.cross.selected", 0.0);
    project.cross_hierarchy_constraints().clear();
    REQUIRE(project.validate_assembly_structure());
    CHECK(applier.apply(project, result).has_error());
  }

  SECTION("Boundary transform changed") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto result = move(project, "joint.cross.revolute", 45.0);
    REQUIRE(project.assembly().set_subassembly_instance_transform(
        SubassemblyInstanceId("subassembly.left"),
        RigidTransform{Vector3{}, Vector3{0.0, 0.0, 5.0}}));
    CHECK(applier.apply(project, result).has_error());
  }

  SECTION("Visibility remains outside freshness") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto result = move(project, "joint.cross.revolute", 45.0);
    REQUIRE(project.assembly().set_subassembly_instance_visibility(
        SubassemblyInstanceId("subassembly.left"), ComponentVisibility::Hidden));
    auto applied = applier.apply(project, result);
    REQUIRE(applied);
    CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.cross.revolute"))
              ->coordinate_deg() == 45.0);
  }

  SECTION("PartDocument model intent changed") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto result = move(project, "joint.cross.revolute", 45.0);
    auto thickness = Quantity::length_mm(10.0, "part.thickness");
    REQUIRE(thickness);
    REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
                ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));
    CHECK(applier.apply(project, result).has_error());
  }

  SECTION("New active relationship joins motion group") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto result = move(project, "joint.cross.revolute", 45.0);
    REQUIRE(project.add_cross_hierarchy_constraint(
        cross_concentric("constraint.cross.new")));
    REQUIRE(project.validate_assembly_structure());
    CHECK(applier.apply(project, result).has_error());
  }
}

TEST_CASE("Cross-hierarchy motion rejects invalid selection and tampered results fail closed",
          "[geometry][assembly-cross-hierarchy-motion]") {
  const AssemblyCrossHierarchyJointMotionSolver solver;
  const AssemblyCrossHierarchyJointMotionResultApplier applier;

  SECTION("Requested coordinate outside limits") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    CHECK(solver.move(project, AssemblyJointId("joint.cross.revolute"),
                      angle(120.0, "joint.cross.revolute"))
              .has_error());
  }

  SECTION("Inactive selected joint") {
    Project project = motion_project();
    add_joint(project, cross_joint("joint.cross.inactive", 0.0, AssemblyJointState::Inactive));
    CHECK(solver.move(project, AssemblyJointId("joint.cross.inactive"),
                      angle(0.0, "joint.cross.inactive"))
              .has_error());
  }

  SECTION("No grounded authority") {
    Project project = motion_project();
    REQUIRE(project.assembly().set_component_instance_grounding_state(
        ComponentInstanceId("component.root"), ComponentGroundingState::Free));
    add_joint(project, cross_joint());
    CHECK(solver.move(project, AssemblyJointId("joint.cross.revolute"),
                      angle(45.0, "joint.cross.revolute"))
              .has_error());
  }

  SECTION("Missing snapshots and proposals") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    const auto valid = move(project, "joint.cross.revolute", 45.0);

    auto missing_relationship = valid;
    missing_relationship.relationship_snapshots.clear();
    CHECK(applier.apply(project, missing_relationship).has_error());

    auto missing_boundary = valid;
    missing_boundary.hierarchy_boundary_snapshots.clear();
    CHECK(applier.apply(project, missing_boundary).has_error());

    auto missing_semantic = valid;
    missing_semantic.semantic_target_part_snapshots.clear();
    CHECK(applier.apply(project, missing_semantic).has_error());

    auto missing_proposal = valid;
    missing_proposal.proposed_transforms.clear();
    CHECK(applier.apply(project, missing_proposal).has_error());
  }

  SECTION("Invalid proposed transform") {
    Project project = motion_project();
    add_joint(project, cross_joint());
    auto result = move(project, "joint.cross.revolute", 45.0);
    result.proposed_transforms.front().proposed_transform.rotation_deg.x =
        std::numeric_limits<double>::quiet_NaN();
    CHECK(applier.apply(project, result).has_error());
    CHECK(child_transform(project) == identity_rigid_transform());
    CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.cross.revolute"))
              ->coordinate_deg() == 0.0);
  }
}
