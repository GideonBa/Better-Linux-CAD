#include "blcad/gui/gui_interactive_extrude_revolve_binder.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
Parameter angle(const char* id, double value) {
  return Parameter::create_angle(ParameterId(id), id, Quantity::angle_deg(value, id).value()).value();
}

// A box Body (body.main, produced by feature.base) with driving parameters for
// every finishing family, plus a pull axis for Draft.
void build_finishing_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.finishing"), "Finishing"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("width", 20.0), length("height", 12.0), length("depth", 8.0),
                           length("radius", 2.0), length("chamfer.d", 2.0), length("chamfer.d2", 1.5),
                           length("wall", 1.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    for (auto parameter : {angle("chamfer.angle", 30.0), angle("draft.angle", 5.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    if (auto added = part.add_datum_axis(
            DatumAxis::create_explicit(DatumAxisId("axis.pull"), "Pull", {}, {0.0, 0.0, 1.0}).value());
        added.has_error())
      return added;
    auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
    if (auto added = sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                                 ParameterId("width"),
                                                                 ParameterId("height")).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(sketch)); added.has_error()) return added;
    if (auto added = part.add_body(Body::create(BodyId("body.main"), "Main", BodyKind::Solid).value());
        added.has_error())
      return added;
    auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                 SketchId("sketch.base"), ParameterId("depth"))
                    .value()
                    .with_body_result_context(
                        FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                         std::nullopt, BodyId("body.main")).value())
                    .value();
    return part.add_feature(std::move(base));
  }));
}

EdgeReference edge(SemanticEdge value, PartFeatureInputRole role = PartFeatureInputRole::FilletEdge) {
  return EdgeReference::create_linear(
             role, SemanticEdgeReference::create(FeatureId("feature.base"), value).value())
      .value();
}
FaceReference face(SemanticFace value, PartFeatureInputRole role) {
  return FaceReference::create_planar(
             role, SemanticFaceReference::create(FeatureId("feature.base"), value).value())
      .value();
}
AxisReference draft_pull() {
  return AxisReference::create_datum_axis(PartFeatureInputRole::DraftPullDirection,
                                          DatumAxisId("axis.pull"))
      .value();
}
PlaneReference draft_neutral() {
  return PlaneReference::create_datum_plane(PartFeatureInputRole::DraftNeutralPlane,
                                            DatumPlaneId("datum.xy"))
      .value();
}

GuiViewportManipulatorMapping planar_mapping() {
  GuiViewportCameraBookmark camera;
  camera.eye = {0.0, 0.0, 100.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.up_direction = {0.0, 1.0, 0.0};
  camera.scale = 200.0;
  camera.projection = GuiViewportProjection::Orthographic;
  return GuiViewportManipulatorMapping::create_camera(camera, 200.0, 200.0).value();
}
GuiFeatureManipulatorFrame planar_frame() {
  GuiFeatureManipulatorFrame frame;
  frame.axis = {1.0, 0.0, 0.0};
  return frame;
}

} // namespace

TEST_CASE("Block 125 interactive Fillet and Chamfer collect edges and commit one transaction",
          "[gui][interactive-finishing]") {
  GuiDocumentSession session;
  build_finishing_part(session);

  SECTION("fillet collects an ordered edge chain and applies with exact undo") {
    GuiInteractiveFinishingController fillet;
    REQUIRE(fillet.begin_fillet(session, BodyId("body.main"), FeatureId("feature.fillet"), "Fillet",
                                ParameterId("radius")));
    CHECK(fillet.kind() == GuiFinishingKind::Fillet);
    CHECK(fillet.first_value() == 2.0);
    REQUIRE(fillet.add_edge(edge(SemanticEdge::TopFront)));
    CHECK(fillet.add_edge(edge(SemanticEdge::TopFront)).has_error());  // duplicate rejected
    REQUIRE(fillet.add_edge(edge(SemanticEdge::TopBack)));
    CHECK(fillet.edge_count() == 2);
    REQUIRE(fillet.handles().size() == 1);
    CHECK(fillet.handles().front().id == std::string(kFilletRadiusHandleId));

    REQUIRE(fillet.preview(session));
    CHECK(session.part_document()->fillet_features().empty());
    REQUIRE(fillet.apply(session));
    CHECK_FALSE(fillet.active());
    CHECK(session.part_document()->fillet_features().size() == 1);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.main")) != nullptr);

    REQUIRE(session.undo());
    CHECK(session.part_document()->fillet_features().empty());
  }

  SECTION("chamfer modes expose the matching handles") {
    GuiInteractiveFinishingController equal;
    REQUIRE(equal.begin_chamfer_equal(session, BodyId("body.main"), FeatureId("feature.ch1"), "Ch",
                                      ParameterId("chamfer.d")));
    REQUIRE(equal.add_edge(edge(SemanticEdge::TopFront, PartFeatureInputRole::ChamferEdge)));
    CHECK(equal.handles().size() == 1);
    REQUIRE(equal.preview(session));

    GuiInteractiveFinishingController distance_angle;
    REQUIRE(distance_angle.begin_chamfer_distance_angle(session, BodyId("body.main"),
                                                        FeatureId("feature.ch2"), "Ch2",
                                                        ParameterId("chamfer.d"),
                                                        ParameterId("chamfer.angle")));
    REQUIRE(distance_angle.add_edge(edge(SemanticEdge::TopFront, PartFeatureInputRole::ChamferEdge)));
    REQUIRE(distance_angle.handles().size() == 2);
    CHECK(distance_angle.handles()[1].id == std::string(kChamferAngleHandleId));
    REQUIRE(distance_angle.preview(session));
    REQUIRE(distance_angle.apply(session));
    CHECK(session.part_document()->chamfer_features().size() == 1);
  }

  SECTION("fillet fails closed on missing body, parameter, and empty chain") {
    GuiInteractiveFinishingController fillet;
    CHECK(fillet.begin_fillet(session, BodyId("body.absent"), FeatureId("f"), "F",
                              ParameterId("radius")).has_error());
    CHECK(fillet.begin_fillet(session, BodyId("body.main"), FeatureId("f"), "F",
                              ParameterId("missing")).has_error());
    REQUIRE(fillet.begin_fillet(session, BodyId("body.main"), FeatureId("f"), "F",
                                ParameterId("radius")));
    CHECK(fillet.preview(session).has_error());  // no edges picked yet
  }
}

TEST_CASE("Block 125 interactive Shell and Draft collect faces and commit one transaction",
          "[gui][interactive-shell-draft]") {
  GuiDocumentSession session;
  build_finishing_part(session);

  SECTION("shell removes a face, toggles direction, and applies") {
    GuiInteractiveShellDraftController shell;
    REQUIRE(shell.begin_shell(session, BodyId("body.main"), FeatureId("feature.shell"), "Shell",
                              ParameterId("wall")));
    CHECK(shell.kind() == GuiShellDraftKind::Shell);
    CHECK(shell.thickness() == 1.0);
    REQUIRE(shell.add_face(face(SemanticFace::Top, PartFeatureInputRole::ShellRemovalFace)));
    CHECK(shell.add_face(face(SemanticFace::Top, PartFeatureInputRole::ShellRemovalFace)).has_error());
    CHECK(shell.face_count() == 1);
    shell.set_direction(ShellDirection::Outward);
    CHECK(shell.direction() == ShellDirection::Outward);
    shell.set_direction(ShellDirection::Inward);
    REQUIRE(shell.handles().size() == 1);
    CHECK(shell.handles().front().id == std::string(kShellThicknessHandleId));

    REQUIRE(shell.preview(session));
    REQUIRE(shell.apply(session));
    CHECK(session.part_document()->shell_features().size() == 1);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.main")) != nullptr);
  }

  SECTION("draft picks faces, pull direction, neutral plane, and applies") {
    GuiInteractiveShellDraftController draft;
    REQUIRE(draft.begin_draft(session, BodyId("body.main"), FeatureId("feature.draft"), "Draft",
                              draft_pull(), draft_neutral(), ParameterId("draft.angle")));
    CHECK(draft.kind() == GuiShellDraftKind::Draft);
    CHECK(draft.angle() == 5.0);
    REQUIRE(draft.add_face(face(SemanticFace::Right, PartFeatureInputRole::DraftFace)));
    REQUIRE(draft.handles().size() == 1);
    CHECK(draft.handles().front().id == std::string(kDraftAngleHandleId));
    REQUIRE(draft.preview(session));
    REQUIRE(draft.apply(session));
    CHECK(session.part_document()->draft_features().size() == 1);
    REQUIRE(session.recompute());
  }

  SECTION("shell and draft fail closed on missing parameters and empty faces") {
    GuiInteractiveShellDraftController shell;
    CHECK(shell.begin_shell(session, BodyId("body.main"), FeatureId("f"), "F",
                            ParameterId("draft.angle")).has_error());  // Angle, not Length
    REQUIRE(shell.begin_shell(session, BodyId("body.main"), FeatureId("f"), "F",
                              ParameterId("wall")));
    CHECK(shell.preview(session).has_error());  // no faces

    GuiInteractiveShellDraftController draft;
    CHECK(draft.begin_draft(session, BodyId("body.main"), FeatureId("f"), "F", draft_pull(),
                            draft_neutral(), ParameterId("wall")).has_error());  // Length, not Angle
  }
}

TEST_CASE("Block 125 edge-chain picking and manipulator drags match headless finishing edits",
          "[integration][edge-chain-picking]") {
  SECTION("ordered chain rejects duplicates and preserves order") {
    GuiDocumentSession session;
    build_finishing_part(session);
    GuiInteractiveFinishingController fillet;
    REQUIRE(fillet.begin_fillet(session, BodyId("body.main"), FeatureId("feature.fillet"), "Fillet",
                                ParameterId("radius")));
    REQUIRE(fillet.add_edge(edge(SemanticEdge::TopFront)));
    REQUIRE(fillet.add_edge(edge(SemanticEdge::TopBack)));
    CHECK(fillet.add_edge(edge(SemanticEdge::TopFront)).has_error());
    CHECK(fillet.edge_count() == 2);
  }

  SECTION("fillet radius drag folds into the same document as a typed radius") {
    GuiDocumentSession dragged;
    build_finishing_part(dragged);
    GuiInteractiveFinishingController controller;
    REQUIRE(controller.begin_fillet(dragged, BodyId("body.main"), FeatureId("feature.fillet"),
                                    "Fillet", ParameterId("radius")));
    controller.set_manipulator_frame(planar_frame());
    REQUIRE(controller.add_edge(edge(SemanticEdge::TopFront)));

    GuiViewportManipulatorLayer layer;
    REQUIRE(layer.set_mapping(planar_mapping()));
    REQUIRE(layer.set_handles(controller.handles()));
    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({101.0, 100.0});  // +1 mm -> radius 3
    REQUIRE(candidate.has_value());
    CHECK(controller.apply_manipulator(*candidate));
    const double radius = controller.first_value();
    CHECK(std::abs(radius - 3.0) < 1.0e-9);
    REQUIRE(controller.apply(dragged));

    GuiDocumentSession typed;
    build_finishing_part(typed);
    GuiInteractiveFinishingController headless;
    REQUIRE(headless.begin_fillet(typed, BodyId("body.main"), FeatureId("feature.fillet"), "Fillet",
                                  ParameterId("radius")));
    REQUIRE(headless.add_edge(edge(SemanticEdge::TopFront)));
    headless.set_first_value(radius);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }

  SECTION("shell thickness drag through the coordinator matches a typed thickness") {
    GuiDocumentSession dragged;
    build_finishing_part(dragged);
    GuiViewportManipulatorLayer layer;
    GuiInteractiveFeatureCoordinator coordinator(&dragged, &layer);
    REQUIRE(layer.set_mapping(planar_mapping()));
    coordinator.cancel();
    REQUIRE(coordinator.shell_draft().begin_shell(dragged, BodyId("body.main"),
                                                  FeatureId("feature.shell"), "Shell",
                                                  ParameterId("wall")));
    coordinator.shell_draft().set_manipulator_frame(planar_frame());
    REQUIRE(coordinator.shell_draft().add_face(
        face(SemanticFace::Top, PartFeatureInputRole::ShellRemovalFace)));
    coordinator.refresh_handles();
    REQUIRE(layer.handles().size() == 1);

    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({101.0, 100.0});  // +1 mm -> thickness 2
    REQUIRE(candidate.has_value());
    coordinator.on_candidate(*candidate);
    const double thickness = coordinator.shell_draft().thickness();
    CHECK(std::abs(thickness - 2.0) < 1.0e-9);
    REQUIRE(coordinator.apply());

    GuiDocumentSession typed;
    build_finishing_part(typed);
    GuiInteractiveShellDraftController headless;
    REQUIRE(headless.begin_shell(typed, BodyId("body.main"), FeatureId("feature.shell"), "Shell",
                                 ParameterId("wall")));
    REQUIRE(headless.add_face(face(SemanticFace::Top, PartFeatureInputRole::ShellRemovalFace)));
    headless.set_thickness(thickness);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }
}
