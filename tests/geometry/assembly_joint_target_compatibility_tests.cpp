#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/geometry/assembly_hierarchy_revolute_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_joint_target_compatibility.hpp"
#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

using namespace blcad;
using namespace blcad::geometry;
namespace ts = blcad::geometry::test_support;
using Catch::Approx;

namespace {

constexpr const char* kCompatibilityObjectId = "geometry.assembly_joint_target_compatibility";

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyResolvedGeometricTarget frame_target(const char* reference) {
  return AssemblyResolvedGeometricTarget{
      AssemblyHierarchyGeometricTargetEndpointIdentity{
          {}, ComponentInstanceId("component.synthetic"), reference},
      AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
      AssemblyGeometricTargetSourceMetadata{DocumentId("part.synthetic"),
                                            FeatureId("feature.synthetic"),
                                            ProfileId("profile.synthetic"), std::nullopt,
                                            SemanticAxis::Primary, SemanticSeatingPlane::Primary},
      AssemblyFrameTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                    Vector3{0.0, 0.0, 1.0}},
      assembly_geometric_target_capabilities(
          AssemblyGeometricTargetSourceKind::CircularFeatureSeat),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {identity_rigid_transform()}};
}

AssemblyResolvedGeometricTarget axis_target(const char* reference) {
  return AssemblyResolvedGeometricTarget{
      AssemblyHierarchyGeometricTargetEndpointIdentity{
          {}, ComponentInstanceId("component.synthetic"), reference},
      AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
      AssemblyGeometricTargetSourceMetadata{
          DocumentId("part.synthetic"), FeatureId("feature.synthetic"),
          ProfileId("profile.synthetic"), std::nullopt, SemanticAxis::Primary, std::nullopt},
      AssemblyCylindricalSurfaceTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 1.0}, 2.0},
      assembly_geometric_target_capabilities(
          AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {identity_rigid_transform()}};
}

AssemblyConstraintTarget local_target(const char* component, const char* reference) {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component), reference);
  REQUIRE(value);
  return value.value();
}

AssemblyJoint local_joint(const char* reference_a = "feature.hole.seat",
                          const char* reference_b = "feature.hole.seat") {
  auto value = AssemblyJoint::create(
      AssemblyJointId("joint.local.revolute"), "LocalRevolute", AssemblyJointType::Revolute,
      local_target("component.a", reference_a), local_target("component.b", reference_b),
      AssemblyJointState::Active, angle(-180.0, "joint.local.revolute"),
      angle(180.0, "joint.local.revolute"), angle(0.0, "joint.local.revolute"));
  REQUIRE(value);
  return value.value();
}

Project local_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.local"), "Local");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(assembly.value().add_component_instance(
      ts::component("component.a", ComponentGroundingState::Grounded)));
  REQUIRE(assembly.value().add_component_instance(
      ts::component("component.b", ComponentGroundingState::Free)));
  auto project = Project::create(DocumentId("project.local"), "Local", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  return project.value();
}

AssemblyHierarchyJoint hierarchy_joint(const char* reference_a = "feature.hole.seat",
                                       const char* reference_b = "feature.hole.seat") {
  auto value = AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.cross.revolute"), "CrossRevolute", AssemblyJointType::Revolute,
      ts::endpoint({}, "component.root", reference_a),
      ts::endpoint({"subassembly.left"}, "component.child", reference_b),
      AssemblyJointState::Active, angle(-180.0, "joint.cross.revolute"),
      angle(180.0, "joint.cross.revolute"), angle(0.0, "joint.cross.revolute"));
  REQUIRE(value);
  return value.value();
}

Project hierarchy_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      ts::component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      ts::occurrence("subassembly.left", identity_rigid_transform())));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      ts::component("component.child", ComponentGroundingState::Free)));

  auto project = Project::create(DocumentId("project.cross"), "Cross", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("Joint target compatibility freezes oriented Frame requirements",
          "[geometry][assembly-joint-target-compatibility]") {
  const AssemblyJointTargetCompatibilityResolver resolver;
  const auto frame_frame = resolver.resolve(AssemblyJointType::Revolute, frame_target("frame.a"),
                                            frame_target("frame.b"));
  REQUIRE(frame_frame);
  CHECK(frame_frame.value().joint_type == AssemblyJointType::Revolute);
  CHECK(frame_frame.value().target_a_capability == AssemblyGeometricTargetCapability::Frame);
  CHECK(frame_frame.value().target_b_capability == AssemblyGeometricTargetCapability::Frame);

  const auto axis_axis =
      resolver.resolve(AssemblyJointType::Revolute, axis_target("axis.a"), axis_target("axis.b"));
  REQUIRE(axis_axis.has_error());
  CHECK(axis_axis.error().object_id() == kCompatibilityObjectId);
  CHECK(axis_axis.error().message().find("reference X direction") != std::string::npos);
}

TEST_CASE("Prismatic Frame equations preserve orientation and drive signed axial translation",
          "[geometry][assembly-prismatic-joint]") {
  const AssemblyJointTargetCompatibilityResolver compatibility;
  REQUIRE(compatibility.resolve(AssemblyJointType::Prismatic, frame_target("frame.a"),
                                frame_target("frame.b")));
  CHECK(compatibility
            .resolve(AssemblyJointType::Prismatic, axis_target("axis.a"), axis_target("axis.b"))
            .has_error());
  REQUIRE(compatibility.resolve(AssemblyJointType::Cylindrical, frame_target("frame.a"),
                                frame_target("frame.b")));
  CHECK(compatibility
            .resolve(AssemblyJointType::Cylindrical, axis_target("axis.a"), axis_target("axis.b"))
            .has_error());

  auto requested = Quantity::linear_displacement_mm(15.0, "joint.slider");
  REQUIRE(requested);
  auto target_b = frame_target("frame.b");
  std::get<AssemblyFrameTargetDescriptor>(target_b.descriptor).origin.z = 15.0;
  const AssemblyPrismaticJointEquationBuilder builder;
  auto equation = builder.build(AssemblyJointId("joint.slider"), AssemblyJointType::Prismatic,
                                frame_target("frame.a"), target_b, requested.value());
  REQUIRE(equation);
  CHECK(equation.value().requested_translation_mm == 15.0);
  CHECK(equation.value().residual.direction_alignment == Vector3{});
  CHECK(equation.value().residual.transverse_offset_mm == Vector3{});
  CHECK(equation.value().residual.orientation_alignment_sine == Approx(0.0));
  CHECK(equation.value().residual.orientation_alignment_cosine == Approx(0.0));
  CHECK(equation.value().residual.translation_error_mm == Approx(0.0));
}

TEST_CASE("Prismatic root-space equations use the shared signed translation convention",
          "[geometry][assembly-cross-hierarchy-prismatic-motion]") {
  auto requested = Quantity::linear_displacement_mm(-8.0, "joint.cross.slider");
  REQUIRE(requested);
  auto target_b = frame_target("frame.b");
  std::get<AssemblyFrameTargetDescriptor>(target_b.descriptor).origin.z = -8.0;
  const AssemblyPrismaticJointEquationBuilder builder;
  auto equation = builder.build(AssemblyJointId("joint.cross.slider"), AssemblyJointType::Prismatic,
                                frame_target("frame.a"), target_b, requested.value());
  REQUIRE(equation);
  CHECK(equation.value().residual.translation_error_mm == Approx(0.0));
}

TEST_CASE("Revolute equations consume compatibility before Frame projection",
          "[geometry][assembly-joint-target-compatibility]") {
  const AssemblyRevoluteJointEquationBuilder builder;
  const auto equation = builder.build(AssemblyJointId("joint.synthetic"),
                                      AssemblyJointType::Revolute, frame_target("frame.a"),
                                      frame_target("frame.b"), angle(0.0, "joint.synthetic"));
  REQUIRE(equation);
  CHECK(equation.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Frame);
  CHECK(equation.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Frame);
  CHECK(equation.value().target_a.selected_capability == AssemblyGeometricTargetCapability::Frame);
  CHECK(equation.value().target_a.equation_space_frame.x_axis == Vector3{1.0, 0.0, 0.0});
  CHECK(equation.value().residual.twist_alignment_sine == Approx(0.0).margin(1.0e-12));

  const auto axis_only =
      builder.build(AssemblyJointId("joint.axis"), AssemblyJointType::Revolute,
                    axis_target("axis.a"), axis_target("axis.b"), angle(0.0, "joint.axis"));
  REQUIRE(axis_only.has_error());
  CHECK(axis_only.error().object_id() == kCompatibilityObjectId);
}

TEST_CASE("Local seat Revolute uses Frame compatibility without changing target scope",
          "[geometry][assembly-joint-target-compatibility]") {
  const Project project = local_project();
  const AssemblyRevoluteJointEquationBuilder builder;
  const auto equation = builder.build(project, local_joint(), angle(0.0, "joint.local.revolute"));
  REQUIRE(equation);
  CHECK(equation.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Frame);
  CHECK(equation.value().target_a.target.coordinate_space ==
        AssemblyGeometricTargetCoordinateSpace::ComponentLocal);
  CHECK(std::holds_alternative<AssemblyLocalGeometricTargetEndpointIdentity>(
      equation.value().target_a.target.endpoint));

  const auto axis_only =
      builder.build(project, local_joint("feature.hole.axis", "feature.hole.axis"),
                    angle(0.0, "joint.local.revolute"));
  REQUIRE(axis_only.has_error());
  CHECK(axis_only.error().object_id() == kCompatibilityObjectId);
}

TEST_CASE("Cross-hierarchy seat Revolute agrees with local Frame compatibility",
          "[geometry][assembly-cross-hierarchy-joint-target-compatibility]") {
  const Project local = local_project();
  const Project cross = hierarchy_project();
  const auto local_equation = AssemblyRevoluteJointEquationBuilder{}.build(
      local, local_joint(), angle(0.0, "joint.local.revolute"));
  const auto cross_equation = AssemblyHierarchyRevoluteJointEquationBuilder{}.build(
      cross, hierarchy_joint(), angle(0.0, "joint.cross.revolute"));
  REQUIRE(local_equation);
  REQUIRE(cross_equation);
  CHECK(cross_equation.value().compatibility == local_equation.value().compatibility);
  CHECK(cross_equation.value().target_a.target.coordinate_space ==
        AssemblyGeometricTargetCoordinateSpace::RootAssembly);
  CHECK(std::holds_alternative<AssemblyHierarchyGeometricTargetEndpointIdentity>(
      cross_equation.value().target_a.target.endpoint));
  CHECK(cross_equation.value().residual == local_equation.value().residual);

  const auto axis_only = AssemblyHierarchyRevoluteJointEquationBuilder{}.build(
      cross, hierarchy_joint("feature.hole.axis", "feature.hole.axis"),
      angle(0.0, "joint.cross.revolute"));
  REQUIRE(axis_only.has_error());
  CHECK(axis_only.error().object_id() == kCompatibilityObjectId);
}
