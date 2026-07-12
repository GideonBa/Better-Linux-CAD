#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/geometry/assembly_hierarchy_revolute_joint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>

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

AssemblyHierarchyJoint joint(AssemblyHierarchyConstraintEndpoint target_a,
                             AssemblyHierarchyConstraintEndpoint target_b) {
  auto value = AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.cross.revolute"), "CrossRevolute",
      AssemblyJointType::Revolute, std::move(target_a), std::move(target_b),
      AssemblyJointState::Active, angle(-180.0, "joint.cross.revolute"),
      angle(180.0, "joint.cross.revolute"), angle(0.0, "joint.cross.revolute"));
  REQUIRE(value);
  return value.value();
}

Project root_child_project(RigidTransform boundary = identity_rigid_transform(),
                           RigidTransform child_transform = identity_rigid_transform()) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(occurrence("subassembly.left", boundary)));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free, child_transform)));

  auto project = Project::create(DocumentId("project.revolute_equation"), "RevoluteEquation",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

AssemblyHierarchyJoint root_child_joint(const char* root_reference = "feature.hole.seat") {
  return joint(endpoint({}, "component.root", root_reference),
               endpoint({"subassembly.left"}, "component.child", "feature.hole.seat"));
}

void check_zero_revolute_residual(const RevoluteJointResidualDescriptor& residual) {
  CHECK(residual.direction_alignment.x == Approx(0.0).margin(1.0e-12));
  CHECK(residual.direction_alignment.y == Approx(0.0).margin(1.0e-12));
  CHECK(residual.direction_alignment.z == Approx(0.0).margin(1.0e-12));
  CHECK(residual.axis_offset_mm.x == Approx(0.0).margin(1.0e-12));
  CHECK(residual.axis_offset_mm.y == Approx(0.0).margin(1.0e-12));
  CHECK(residual.axis_offset_mm.z == Approx(0.0).margin(1.0e-12));
  CHECK(residual.signed_seating_separation_mm == Approx(0.0).margin(1.0e-12));
  CHECK(residual.twist_alignment_sine == Approx(0.0).margin(1.0e-12));
  CHECK(residual.twist_alignment_cosine == Approx(0.0).margin(1.0e-12));
}

} // namespace

TEST_CASE("Hierarchy Revolute equations reuse directed axis seating and twist semantics",
          "[geometry][assembly-cross-hierarchy-revolute]") {
  const AssemblyHierarchyRevoluteJointEquationBuilder builder;

  SECTION("Root to child is satisfied at zero") {
    const Project project = root_child_project();
    auto equation = builder.build(project, root_child_joint(),
                                  angle(0.0, "joint.cross.revolute"));
    REQUIRE(equation);
    check_zero_revolute_residual(equation.value().residual);
  }

  SECTION("Signed requested coordinate follows root-space orientation") {
    const Project project = root_child_project(
        identity_rigid_transform(), RigidTransform{Vector3{}, Vector3{0.0, 0.0, 45.0}});
    const AssemblyHierarchyJoint value = root_child_joint();

    auto positive = builder.build(project, value, angle(45.0, "joint.cross.revolute"));
    REQUIRE(positive);
    CHECK(positive.value().residual.twist_alignment_sine == Approx(0.0).margin(1.0e-12));
    CHECK(positive.value().residual.twist_alignment_cosine == Approx(0.0).margin(1.0e-12));

    auto negative = builder.build(project, value, angle(-45.0, "joint.cross.revolute"));
    REQUIRE(negative);
    CHECK(std::abs(negative.value().residual.twist_alignment_sine) > 1.0e-6);
  }
}

TEST_CASE("Hierarchy Revolute endpoints use exact nested parent chains",
          "[geometry][assembly-cross-hierarchy-revolute]") {
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

  auto project = Project::create(DocumentId("project.nested_revolute"), "NestedRevolute",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(outer.value()));
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().validate_assembly_structure());

  const AssemblyHierarchyJoint value = joint(
      endpoint({}, "component.root", "feature.hole.seat"),
      endpoint({"subassembly.outer", "subassembly.inner"}, "component.child",
               "feature.hole.seat"));
  const AssemblyHierarchyRevoluteJointEquationBuilder builder;
  auto equation = builder.build(project.value(), value,
                                angle(35.0, "joint.cross.revolute"));
  REQUIRE(equation);
  CHECK(equation.value().residual.twist_alignment_sine == Approx(0.0).margin(1.0e-12));
  CHECK(equation.value().residual.twist_alignment_cosine == Approx(0.0).margin(1.0e-12));
}

TEST_CASE("Repeated same-authority Revolute endpoints retain distinct rooted paths",
          "[geometry][assembly-cross-hierarchy-revolute]") {
  Project project = root_child_project();
  auto right = SubassemblyInstance::create(
      SubassemblyInstanceId("subassembly.right"), "Right", DocumentId("assembly.child"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{Vector3{}, Vector3{0.0, 0.0, 30.0}});
  REQUIRE(right);
  REQUIRE(project.assembly().add_subassembly_instance(right.value()));
  REQUIRE(project.validate_assembly_structure());

  const AssemblyHierarchyJoint value = joint(
      endpoint({"subassembly.left"}, "component.child", "feature.hole.seat"),
      endpoint({"subassembly.right"}, "component.child", "feature.hole.seat"));
  const AssemblyHierarchyRevoluteJointEquationBuilder builder;
  const RigidTransform source = project.find_assembly_document(DocumentId("assembly.child"))
                                    ->find_component_instance(ComponentInstanceId("component.child"))
                                    ->transform();
  auto equation = builder.build(project, value, angle(30.0, "joint.cross.revolute"));
  REQUIRE(equation);
  CHECK(equation.value().target_a.occurrence_path != equation.value().target_b.occurrence_path);
  CHECK(equation.value().target_a.assembly_document == equation.value().target_b.assembly_document);
  CHECK(std::abs(equation.value().residual.twist_alignment_sine) <= 1.0e-12);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == source);
}

TEST_CASE("Hierarchy Revolute equation construction rejects non-seat target semantics",
          "[geometry][assembly-cross-hierarchy-revolute]") {
  const Project project = root_child_project();
  const AssemblyHierarchyRevoluteJointEquationBuilder builder;
  CHECK(builder.build(project, root_child_joint("feature.hole.axis"),
                      angle(0.0, "joint.cross.revolute"))
            .has_error());
}
