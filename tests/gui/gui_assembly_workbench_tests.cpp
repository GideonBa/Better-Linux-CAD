#include "blcad/gui/gui_assembly_workbench.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;

namespace {
Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
PartDocument make_part() {
  auto part = PartDocument::create(DocumentId("part.block"), "Block").value();
  REQUIRE(part.add_parameter(length("width", 20)));
  REQUIRE(part.add_parameter(length("height", 12)));
  REQUIRE(part.add_parameter(length("depth", 10)));
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
      ParameterId("width"), ParameterId("height")).value()));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(Body::create(BodyId("body.main"), "Main", BodyKind::Solid).value()));
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.main")).value();
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "Base",
      SketchId("sketch.base"), ParameterId("depth")).value().with_body_result_context(context).value();
  REQUIRE(part.add_feature(feature));
  return part;
}
AssemblyConstraintTarget target(const char* component, const char* semantic) {
  return AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic).value();
}
} // namespace

TEST_CASE("Block 103 GUI authors repeated Parts nested hierarchy and solves a relationship",
          "[gui][assembly-authoring][gui][assembly-relationships][gui][assembly-motion]") {
  GuiDocumentSession session;
  GuiAssemblyWorkbench workbench;
  REQUIRE(session.create_project(DocumentId("project.gui"), "GUI Assembly"));
  REQUIRE(workbench.add_part(session, make_part()));
  REQUIRE(workbench.add_root_member_part(session, DocumentId("part.block")));
  REQUIRE(workbench.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.fixed"), "Fixed", DocumentId("part.block"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Grounded).value()));
  REQUIRE(workbench.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.free"), "Free", DocumentId("part.block"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, {{0, 0, 20}, {0, 0, 0}}).value()));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child").value();
  REQUIRE(child.add_member_part(DocumentId("part.block")));
  REQUIRE(child.add_component_instance(ComponentInstance::create(ComponentInstanceId("child.block"),
      "Child block", DocumentId("part.block")).value()));
  REQUIRE(workbench.add_child_assembly(session, child));
  REQUIRE(workbench.add_root_subassembly(session, SubassemblyInstance::create(
      SubassemblyInstanceId("sub.child"), "Nested child", DocumentId("assembly.child")).value()));

  auto mate = AssemblyConstraint::create(AssemblyConstraintId("constraint.mate"), "Mate",
      AssemblyConstraintType::Mate,
      target("component.fixed", "feature.base_extrude.top"),
      target("component.free", "feature.base_extrude.bottom")).value();
  REQUIRE(workbench.preview_root_constraint(session, mate));
  CHECK(session.project()->assembly().constraint_count() == 0);
  REQUIRE(workbench.apply_root_constraint(session, mate));
  const std::vector<ComponentInstanceId> group = {ComponentInstanceId("component.fixed"),
                                                   ComponentInstanceId("component.free")};
  auto diagnostics = workbench.inspect_root_group(session, group);
  REQUIRE(diagnostics);
  CHECK(diagnostics.value().rank_summary.remaining_dof <= 6);
  auto solved = workbench.solve_root_group(session, group);
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  REQUIRE(workbench.apply_solve(session, solved.value()));
  CHECK(session.project()->assembly().find_component_instance(ComponentInstanceId("component.free"))
            ->transform().translation_mm.z == Catch::Approx(10.0).margin(1.0e-7));
  CHECK(session.project()->validate_assembly_structure());
}

TEST_CASE("Block 103 GUI stores joint families coordinates and state edits atomically",
          "[gui][assembly-authoring][gui][assembly-motion]") {
  GuiDocumentSession session;
  GuiAssemblyWorkbench workbench;
  REQUIRE(session.create_project(DocumentId("project.joint"), "Joint"));
  REQUIRE(workbench.add_part(session, make_part()));
  REQUIRE(workbench.add_root_member_part(session, DocumentId("part.block")));
  REQUIRE(workbench.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.a"), "A", DocumentId("part.block")).value()));
  REQUIRE(workbench.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.b"), "B", DocumentId("part.block")).value()));
  auto joint = AssemblyJoint::create(AssemblyJointId("joint.revolute"), "Revolute",
      AssemblyJointType::Revolute, target("component.a", "feature.hole.axis"),
      target("component.b", "feature.hole.axis"), AssemblyJointState::Active,
      Quantity::angle_deg(-90).value(), Quantity::angle_deg(90).value(),
      Quantity::angle_deg(0).value()).value();
  REQUIRE(workbench.preview_root_joint(session, joint));
  REQUIRE(workbench.apply_root_joint(session, joint));
  REQUIRE(workbench.set_root_joint_coordinate(session, AssemblyJointId("joint.revolute"),
      AssemblyJointCoordinateRole::Rotation, Quantity::angle_deg(30).value()));
  CHECK(session.project()->assembly().find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() == 30);
  REQUIRE(workbench.set_component_visibility(session, ComponentInstanceId("component.b"),
                                              ComponentVisibility::Hidden));
  REQUIRE(workbench.set_component_suppression(session, ComponentInstanceId("component.b"),
                                               ComponentSuppressionState::Suppressed));
  CHECK(session.project()->assembly().find_component_instance(ComponentInstanceId("component.b"))
            ->suppression_state() == ComponentSuppressionState::Suppressed);
}

TEST_CASE("Block 103 prompts identify relationship joint and hierarchy capabilities",
          "[gui][assembly-authoring][gui][assembly-relationships][gui][assembly-motion]") {
  CHECK(GuiAssemblyWorkbench::prompt_for(GuiAssemblyCommand::Relationship).required_capability ==
        "relationship_compatible_targets");
  CHECK(GuiAssemblyWorkbench::prompt_for(GuiAssemblyCommand::Joint).text.find("coordinate") !=
        std::string::npos);
  CHECK(GuiAssemblyWorkbench::prompt_for(GuiAssemblyCommand::InsertSubassembly).required_capability ==
        "subassembly_definition");
}
