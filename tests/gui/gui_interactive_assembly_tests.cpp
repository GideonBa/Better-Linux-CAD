#include "blcad/gui/gui_interactive_assembly.hpp"

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value())
      .value();
}

PartDocument assembly_part() {
  auto part = PartDocument::create(DocumentId("part.plate"), "Plate").value();
  REQUIRE(part.add_parameter(length("width", 20.0)));
  REQUIRE(part.add_parameter(length("height", 12.0)));
  REQUIRE(part.add_parameter(length("depth", 4.0)));
  REQUIRE(part.add_parameter(length("hole.diameter", 4.0)));
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));

  auto base = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(base.add_profile(RectangleProfile::create(ProfileId("profile.base"), ParameterId("width"),
                                                    ParameterId("height"))
                               .value()));
  REQUIRE(part.add_sketch(std::move(base)));
  REQUIRE(part.add_feature(Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                            SketchId("sketch.base"),
                                                            ParameterId("depth"))
                               .value()));

  auto hole = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy")).value();
  REQUIRE(hole.add_profile(
      CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"), Point2{})
          .value()));
  REQUIRE(part.add_sketch(std::move(hole)));
  REQUIRE(part.add_feature(Feature::create_subtractive_extrude(
                               FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"),
                               FeatureId("feature.base"), SubtractiveExtrudeDepth::ThroughAll,
                               ExtrudeDirection::SketchNormal)
                               .value()));
  return part;
}

AssemblyConstraintTarget target(const char* component, const char* semantic) {
  return AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic).value();
}

void build_project(GuiDocumentSession& session, bool with_components = true) {
  REQUIRE(session.create_project(DocumentId("project.interactive"), "Interactive Assembly"));
  REQUIRE(session.commit_project_transaction(
      "Setup Assembly", [with_components](Project& project) -> Result<std::size_t> {
        auto added_part = project.add_part_document(assembly_part());
        if (added_part.has_error())
          return added_part;
        if (!with_components)
          return added_part;
        auto member = project.assembly().add_member_part(DocumentId("part.plate"));
        if (member.has_error())
          return member;
        auto fixed = ComponentInstance::create(
            ComponentInstanceId("component.fixed"), "Fixed", DocumentId("part.plate"),
            ComponentVisibility::Visible, ComponentSuppressionState::Active,
            ComponentGroundingState::Grounded);
        auto moving = ComponentInstance::create(ComponentInstanceId("component.moving"), "Moving",
                                                DocumentId("part.plate"));
        if (fixed.has_error())
          return Result<std::size_t>::failure(fixed.error());
        if (moving.has_error())
          return Result<std::size_t>::failure(moving.error());
        auto a = project.assembly().add_component_instance(std::move(fixed.value()));
        if (a.has_error())
          return a;
        auto b = project.assembly().add_component_instance(std::move(moving.value()));
        if (b.has_error())
          return b;
        return Result<std::size_t>::success(added_part.value() + member.value() + a.value() +
                                            b.value());
      }));
}

AssemblyJointCoordinateSlot rotation_slot(const char* id = "joint.revolute") {
  return AssemblyJointCoordinateSlot::create(
             AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateKind::Angular,
             Quantity::angle_deg(0.0, id).value(), Quantity::angle_deg(-90.0, id).value(),
             Quantity::angle_deg(90.0, id).value())
      .value();
}

} // namespace

TEST_CASE("Block 130 inserts and edits Assembly occurrences through a candidate triad",
          "[gui][interactive-assembly-placement]") {
  GuiDocumentSession session;
  build_project(session, false);

  GuiInteractiveAssemblyPlacementController placement;
  REQUIRE(placement.begin_insert_component(session, ComponentInstanceId("component.placed"),
                                           "Placed", DocumentId("part.plate")));
  REQUIRE(placement.handles().size() == 6U);
  CHECK(placement.apply_manipulator({"assembly.placement.translate.x",
                                     GuiViewportManipulatorValueKind::Translation,
                                     GuiViewportManipulatorInputSource::Drag,
                                     15.0,
                                     {},
                                     0}));
  CHECK(placement.apply_manipulator({"assembly.placement.rotate.z",
                                     GuiViewportManipulatorValueKind::Rotation,
                                     GuiViewportManipulatorInputSource::Numeric,
                                     30.0,
                                     {},
                                     0}));
  placement.set_grounding(ComponentGroundingState::Grounded);
  REQUIRE(placement.preview(session));
  CHECK(session.project()->assembly().component_instance_count() == 0U);
  REQUIRE(placement.apply(session));

  const auto* placed = session.project()->assembly().find_component_instance(
      ComponentInstanceId("component.placed"));
  REQUIRE(placed != nullptr);
  CHECK(placed->transform().translation_mm.x == Approx(15.0));
  CHECK(placed->transform().rotation_deg.z == Approx(30.0));
  CHECK(placed->grounding_state() == ComponentGroundingState::Grounded);
  CHECK(session.project()->assembly().has_member_part(DocumentId("part.plate")));

  REQUIRE(placement.begin_edit_component(session, ComponentInstanceId("component.placed")));
  placement.set_visibility(ComponentVisibility::Hidden);
  placement.set_suppression(ComponentSuppressionState::Suppressed);
  REQUIRE(placement.apply(session));
  placed = session.project()->assembly().find_component_instance(
      ComponentInstanceId("component.placed"));
  REQUIRE(placed != nullptr);
  CHECK(placed->visibility() == ComponentVisibility::Hidden);
  CHECK(placed->suppression_state() == ComponentSuppressionState::Suppressed);
  REQUIRE(session.undo());
  CHECK(session.project()
            ->assembly()
            .find_component_instance(ComponentInstanceId("component.placed"))
            ->visibility() == ComponentVisibility::Visible);

  REQUIRE(session.commit_project_transaction("Add child definition", [](Project& project) {
    return project.add_child_assembly_document(
        AssemblyDocument::create(DocumentId("assembly.child"), "Child").value());
  }));
  REQUIRE(placement.begin_insert_subassembly(session, SubassemblyInstanceId("subassembly.placed"),
                                             "Placed child", DocumentId("assembly.child")));
  placement.set_transform({{0.0, 20.0, 0.0}, {0.0, 0.0, 45.0}});
  REQUIRE(placement.apply(session));
  const auto* subassembly = session.project()->assembly().find_subassembly_instance(
      SubassemblyInstanceId("subassembly.placed"));
  REQUIRE(subassembly != nullptr);
  CHECK(subassembly->transform().translation_mm.y == Approx(20.0));

  GuiViewportManipulatorLayer layer;
  GuiInteractiveAssemblyCoordinator coordinator(&session, &layer);
  REQUIRE(coordinator.placement().begin_edit_component(session,
                                                       ComponentInstanceId("component.placed")));
  coordinator.refresh_handles();
  CHECK(layer.handles().size() == 6U);
  coordinator.on_candidate({"assembly.placement.translate.y",
                            GuiViewportManipulatorValueKind::Translation,
                            GuiViewportManipulatorInputSource::Numeric,
                            8.0,
                            {},
                            0});
  REQUIRE(coordinator.apply());
  CHECK(session.project()
            ->assembly()
            .find_component_instance(ComponentInstanceId("component.placed"))
            ->transform()
            .translation_mm.y == Approx(8.0));
}

TEST_CASE(
    "Block 130 relationship picking filters capabilities and applies its solved pose atomically",
    "[gui][interactive-relationships]") {
  GuiDocumentSession session;
  build_project(session);

  GuiInteractiveAssemblyRelationshipController relationship;
  REQUIRE(relationship.begin(session, AssemblyConstraintId("constraint.mate"), "Mate",
                             AssemblyConstraintType::Mate));
  REQUIRE(relationship.select_first(session, target("component.fixed", "feature.base.top")));
  const auto compatible = relationship.compatible_second_targets(
      session, {target("component.moving", "feature.base.bottom"),
                target("component.moving", "feature.hole.axis")});
  REQUIRE(compatible.size() == 1U);
  CHECK(compatible.front().semantic_reference() == "feature.base.bottom");
  CHECK(relationship.select_second(session, target("component.moving", "feature.hole.axis"))
            .has_error());
  REQUIRE(relationship.select_second(session, compatible.front()));

  auto preview = relationship.preview(session);
  REQUIRE(preview);
  REQUIRE(preview.value().solved_pose.converged());
  CHECK(session.project()->assembly().constraint_count() == 0U);
  REQUIRE(relationship.apply(session));
  CHECK(session.project()->assembly().constraint_count() == 1U);
  REQUIRE(session.undo());
  CHECK(session.project()->assembly().constraint_count() == 0U);
}

TEST_CASE("Block 130 joint authoring previews oriented frames and motion drives one coordinate",
          "[gui][interactive-joint-motion]") {
  GuiDocumentSession session;
  build_project(session);

  GuiInteractiveAssemblyJointController authoring;
  REQUIRE(authoring.begin(session, AssemblyJointId("joint.revolute"), "Revolute",
                          AssemblyJointType::Revolute, {rotation_slot()}));
  REQUIRE(authoring.select_first(session, target("component.fixed", "feature.hole.seat")));
  const auto compatible = authoring.compatible_second_targets(
      session, {target("component.moving", "feature.hole.seat"),
                target("component.moving", "feature.hole.axis")});
  REQUIRE(compatible.size() == 1U);
  REQUIRE(authoring.select_second(session, compatible.front()));
  auto joint_preview = authoring.preview(session);
  REQUIRE(joint_preview);
  REQUIRE(joint_preview.value().frame_a.has_value());
  REQUIRE(joint_preview.value().frame_b.has_value());
  CHECK(session.project()->assembly().joint_count() == 0U);
  REQUIRE(authoring.apply(session));

  GuiInteractiveAssemblyMotionController motion;
  REQUIRE(motion.begin(session, AssemblyJointId("joint.revolute")));
  const auto controls = motion.coordinate_controls(session);
  REQUIRE(controls.size() == 1U);
  CHECK(controls.front().lower_limit == Approx(-90.0));
  CHECK(controls.front().upper_limit == Approx(90.0));
  auto handle = motion.handle(session, *joint_preview.value().frame_a);
  REQUIRE(handle.has_value());
  CHECK(handle->kind == GuiViewportManipulatorKind::Angular);
  CHECK(motion.apply_manipulator(session, {"assembly.motion.coordinate",
                                           GuiViewportManipulatorValueKind::Angle,
                                           GuiViewportManipulatorInputSource::Drag,
                                           30.0,
                                           {},
                                           0}));
  auto posed = motion.preview(session);
  REQUIRE(posed);
  CHECK(posed.value().hud.solve_state == geometry::AssemblySolveState::Converged);
  CHECK(posed.value().hud.joint_dof == 1U);
  CHECK(posed.value().hud.undriven_dof == 0U);
  CHECK(session.project()
            ->assembly()
            .find_joint(AssemblyJointId("joint.revolute"))
            ->coordinate_deg() == Approx(0.0));
  REQUIRE(motion.apply(session));
  CHECK(session.project()
            ->assembly()
            .find_joint(AssemblyJointId("joint.revolute"))
            ->coordinate_deg() == Approx(30.0));
}

TEST_CASE("Block 130 rejects Spherical as a selected drive", "[gui][interactive-joint-motion]") {
  GuiDocumentSession session;
  build_project(session);
  REQUIRE(session.commit_project_transaction("Add passive Spherical", [](Project& project) {
    auto joint = AssemblyJoint::create(
        AssemblyJointId("joint.spherical"), "Spherical", AssemblyJointType::Spherical,
        target("component.fixed", "construction_point.center"),
        target("component.moving", "construction_point.center"), AssemblyJointState::Active,
        std::vector<AssemblyJointCoordinateSlot>{});
    if (joint.has_error())
      return Result<std::size_t>::failure(joint.error());
    return project.assembly().add_joint(std::move(joint.value()));
  }));
  GuiInteractiveAssemblyMotionController motion;
  CHECK(motion.begin(session, AssemblyJointId("joint.spherical")).has_error());
}
