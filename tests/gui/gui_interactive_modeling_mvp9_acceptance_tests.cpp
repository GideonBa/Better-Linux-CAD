#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_feature_edit.hpp"
#include "blcad/gui/gui_interactive_assembly.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_interactive_pattern_body.hpp"
#include "blcad/gui/gui_sketch_create.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length_parameter(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value())
      .value();
}

GuiSketchPlaneView xy_plane() {
  return {
      DatumPlaneId("datum.xy"), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
}

EdgeReference top_front_edge() {
  return EdgeReference::create_linear(
             PartFeatureInputRole::FilletEdge,
             SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront)
                 .value())
      .value();
}

FaceReference top_face(const char* feature) {
  return FaceReference::create_planar(
             PartFeatureInputRole::ShellRemovalFace,
             SemanticFaceReference::create(FeatureId(feature), SemanticFace::Top).value())
      .value();
}

AxisReference x_axis() {
  return AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.x"))
      .value();
}

AssemblyConstraintTarget assembly_target(const char* component, const char* semantic) {
  return AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic).value();
}

AssemblyJointCoordinateSlot translation_slot() {
  return AssemblyJointCoordinateSlot::create(
             AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
             Quantity::linear_displacement_mm(0.0, "joint.prismatic").value(),
             Quantity::linear_displacement_mm(-20.0, "joint.prismatic").value(),
             Quantity::linear_displacement_mm(20.0, "joint.prismatic").value())
      .value();
}

Feature tool_feature() {
  return Feature::create_additive_extrude(FeatureId("feature.tool"), "Boolean tool",
                                          SketchId("sketch.tool"), ParameterId("depth"))
      .value()
      .with_body_result_context(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                 std::nullopt, BodyId("body.tool"))
                                    .value())
      .value();
}

GuiViewportManipulatorMapping orthographic_mapping() {
  GuiViewportCameraBookmark camera;
  camera.eye = {0.0, 0.0, 100.0};
  camera.target = {};
  camera.up_direction = {0.0, 1.0, 0.0};
  camera.scale = 200.0;
  camera.projection = GuiViewportProjection::Orthographic;
  return GuiViewportManipulatorMapping::create_camera(camera, 200.0, 200.0).value();
}

template <typename Function> long long elapsed_ms(Function&& function) {
  const auto start = std::chrono::steady_clock::now();
  function();
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               start)
      .count();
}

TEST_CASE("Block 131 assembly acceptance places relates drives and analyzes components",
          "[integration][interactive-modeling][integration][gui-headless-equivalence]") {
  GuiDocumentSession session;
  REQUIRE(session.open_file(std::filesystem::path(BLCAD_SOURCE_DIR) /
                            "examples/revolute_joint.blcad.project.json"));
  REQUIRE(session.recompute());

  const auto place = [&session](const char* id, double x, bool grounded) {
    GuiInteractiveAssemblyPlacementController controller;
    auto begun = controller.begin_insert_component(session, ComponentInstanceId(id), id,
                                                   DocumentId("part.revolute_plate"));
    if (begun.has_error())
      return begun;
    controller.set_transform({{x, 0.0, 0.0}, {}});
    controller.set_grounding(grounded ? ComponentGroundingState::Grounded
                                      : ComponentGroundingState::Free);
    return controller.apply(session);
  };
  REQUIRE(place("component.mate.fixed", 40.0, true));
  REQUIRE(place("component.mate.free", 40.0, false));
  REQUIRE(place("component.slider.fixed", 80.0, true));
  REQUIRE(place("component.slider.free", 80.0, false));

  GuiInteractiveAssemblyRelationshipController mate;
  REQUIRE(mate.begin(session, AssemblyConstraintId("constraint.mate.block131"), "Mate",
                     AssemblyConstraintType::Mate));
  REQUIRE(mate.select_first(session,
                            assembly_target("component.mate.fixed", "feature.base_extrude.top")));
  const auto compatible = mate.compatible_second_targets(
      session, {assembly_target("component.mate.free", "feature.base_extrude.bottom"),
                assembly_target("component.mate.free", "feature.hole.axis")});
  REQUIRE(compatible.size() == 1);
  REQUIRE(mate.select_second(session, compatible.front()));
  REQUIRE(mate.preview(session));
  REQUIRE(mate.apply(session));

  GuiInteractiveAssemblyJointController prismatic;
  REQUIRE(prismatic.begin(session, AssemblyJointId("joint.prismatic"), "Prismatic",
                          AssemblyJointType::Prismatic, {translation_slot()}));
  REQUIRE(prismatic.select_first(session,
                                 assembly_target("component.slider.fixed", "feature.hole.seat")));
  REQUIRE(prismatic.select_second(session,
                                  assembly_target("component.slider.free", "feature.hole.seat")));
  REQUIRE(prismatic.preview(session));
  REQUIRE(prismatic.apply(session));

  GuiInteractiveAssemblyMotionController revolute_motion;
  REQUIRE(revolute_motion.begin(session, AssemblyJointId("joint.revolute")));
  REQUIRE(revolute_motion.set_value(session, 20.0));
  REQUIRE(revolute_motion.preview(session));
  REQUIRE(revolute_motion.apply(session));

  GuiInteractiveAssemblyMotionController slider_motion;
  REQUIRE(slider_motion.begin(session, AssemblyJointId("joint.prismatic")));
  REQUIRE(slider_motion.set_value(session, 5.0));
  auto slider_preview = slider_motion.preview(session);
  REQUIRE(slider_preview);
  CHECK(slider_preview.value().hud.solve_state == geometry::AssemblySolveState::Converged);
  REQUIRE(slider_motion.apply(session));

  REQUIRE(session.recompute());
  GuiAnalysisExportWorkbench analysis;
  REQUIRE(analysis.inspect_dof(session, {ComponentInstanceId("component.slider.free")}));
  REQUIRE(analysis.analyze_interference(session));

  const auto saved =
      std::filesystem::temp_directory_path() / "blcad-block131-assembly.blcad.project.json";
  std::filesystem::remove(saved);
  REQUIRE(session.save_as(saved));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(saved));
  REQUIRE(reopened.recompute());
  CHECK(serialize_project_to_json(*reopened.project()).value() ==
        serialize_project_to_json(*session.project()).value());
  std::filesystem::remove(saved);
}

} // namespace

TEST_CASE("Block 131 bracket traverses the mouse-first Part workflow and round-trips",
          "[integration][interactive-modeling][integration][gui-headless-equivalence]") {
  GuiDocumentSession session;
  GuiSketchWorkbench sketches;
  REQUIRE(session.create_part(DocumentId("part.block131.bracket"), "Block 131 bracket"));
  REQUIRE(session.commit_part_transaction("Seed bracket", [](PartDocument& part) {
    for (auto parameter : {length_parameter("depth", 8.0), length_parameter("depth.edited", 10.0),
                           length_parameter("width", 20.0), length_parameter("height", 12.0),
                           length_parameter("radius", 1.0), length_parameter("wall", 0.8),
                           length_parameter("tool.width", 8.0),
                           length_parameter("tool.height", 8.0), length_parameter("spacing", 24.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error())
        return added;
    if (auto added =
            part.add_parameter(Parameter::create_count(ParameterId("instances"), "Instances",
                                                       Quantity::count(2.0, "instances").value())
                                   .value());
        added.has_error())
      return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error())
      return added;
    if (auto added = part.add_datum_axis(
            DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1.0, 0.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(
            Sketch::create(SketchId("sketch.bracket"), "Bracket outline", DatumPlaneId("datum.xy"))
                .value());
        added.has_error())
      return added;
    auto base =
        Sketch::create(SketchId("sketch.base"), "Stable bracket profile", DatumPlaneId("datum.xy"))
            .value();
    if (auto added =
            base.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                      ParameterId("width"), ParameterId("height"))
                                 .value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(base)); added.has_error())
      return added;
    auto tool =
        Sketch::create(SketchId("sketch.tool"), "Boolean tool", DatumPlaneId("datum.xy")).value();
    if (auto added = tool.add_profile(
            RectangleProfile::create(ProfileId("profile.tool"), ParameterId("tool.width"),
                                     ParameterId("tool.height"), {2.0, 0.0})
                .value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(tool)); added.has_error())
      return added;
    for (const char* id : {"body.main", "body.tool", "body.pattern", "body.boolean"})
      if (auto added = part.add_body(Body::create(BodyId(id), id, BodyKind::Solid).value());
          added.has_error())
        return added;
    return part.add_feature(tool_feature());
  }));

  auto rectangle = GuiSketchCreateController::begin(GuiSketchCreateTool::CornerRectangle,
                                                    SketchId("sketch.bracket"), xy_plane());
  REQUIRE(rectangle);
  REQUIRE(rectangle.value().add_pick({{-10.0, -6.0}, GuiSketchSnapKind::Grid}));
  REQUIRE(rectangle.value().add_pick({{10.0, 6.0}, GuiSketchSnapKind::Grid}));
  REQUIRE(rectangle.value().commit(session, sketches));

  GuiInteractiveExtrudeController extrude;
  REQUIRE(extrude.begin(session, SketchId("sketch.base"), ProfileId("profile.base"),
                        FeatureId("feature.base"), "Bracket base", ParameterId("depth"),
                        BodyId("body.main")));
  GuiViewportManipulatorCandidate extent;
  extent.handle_id = std::string(kExtrudeExtentHandleId);
  extent.value_kind = GuiViewportManipulatorValueKind::Distance;
  extent.scalar_value = 9.0;
  REQUIRE(extrude.apply_manipulator(extent));
  REQUIRE(extrude.preview(session));
  REQUIRE(extrude.apply(session));

  GuiInteractiveFinishingController fillet;
  REQUIRE(fillet.begin_fillet(session, BodyId("body.main"), FeatureId("feature.fillet"),
                              "Bracket fillet", ParameterId("radius")));
  REQUIRE(fillet.add_edge(top_front_edge()));
  GuiViewportManipulatorCandidate radius;
  radius.handle_id = std::string(kFilletRadiusHandleId);
  radius.value_kind = GuiViewportManipulatorValueKind::Distance;
  radius.scalar_value = 1.25;
  REQUIRE(fillet.apply_manipulator(radius));
  const auto fillet_preview = fillet.preview(session);
  const std::string fillet_diagnostic =
      fillet_preview.has_error() ? fillet_preview.error().message() : "fillet preview ok";
  INFO(fillet_diagnostic);
  REQUIRE(fillet_preview);
  REQUIRE(fillet.apply(session));

  GuiInteractiveShellDraftController shell;
  REQUIRE(shell.begin_shell(session, BodyId("body.tool"), FeatureId("feature.shell"),
                            "Bracket shell", ParameterId("wall")));
  REQUIRE(shell.add_face(top_face("feature.tool")));
  REQUIRE(shell.preview(session));
  REQUIRE(shell.apply(session));

  GuiInteractivePatternMirrorController pattern;
  REQUIRE(pattern.begin_linear_pattern(
      session, FeatureId("feature.pattern"), "Bracket pattern", x_axis(), ParameterId("instances"),
      LinearPatternExtentMode::Spacing, ParameterId("spacing"), BodyId("body.pattern")));
  REQUIRE(pattern.add_source_body(BodyId("body.main")));
  REQUIRE(pattern.handles().size() == 2);
  REQUIRE(pattern.preview(session)); // transient pattern ghosts consume this candidate
  REQUIRE(pattern.apply(session));

  GuiInteractiveBodyOperationController boolean;
  REQUIRE(boolean.begin_boolean(session, FeatureId("feature.boolean"), BodyBooleanOperation::Add,
                                BodyId("body.pattern"), BodyBooleanResultMode::NewBody,
                                BodyId("body.boolean")));
  REQUIRE(boolean.add_tool_body(session, BodyId("body.tool")));
  boolean.set_keep_tool_bodies(true);
  REQUIRE(boolean.preview(session));
  REQUIRE(boolean.apply(session));
  REQUIRE(session.recompute());

  GuiFeatureEditController edit;
  REQUIRE(edit.begin(session, FeatureId("feature.base")));
  auto edited =
      Feature::create_additive_extrude(FeatureId("feature.base"), "Bracket base",
                                       SketchId("sketch.base"), ParameterId("depth.edited"))
          .value()
          .with_body_result_context(
              FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                               BodyId("body.main"))
                  .value())
          .value();
  REQUIRE(edit.commit(session, edited, &PartDocument::update_feature));
  REQUIRE(session.recompute());

  const auto stem = std::filesystem::temp_directory_path() / "blcad-block131-bracket";
  const auto document_path = std::filesystem::path(stem.string() + ".blcad.json");
  const auto step_path = std::filesystem::path(stem.string() + ".step");
  std::filesystem::remove(document_path);
  std::filesystem::remove(step_path);
  REQUIRE(session.save_as(document_path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(document_path));
  REQUIRE(reopened.recompute());
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() ==
        serialize_part_document_to_json(*session.part_document()).value());
  GuiAnalysisExportWorkbench export_workbench;
  auto exported =
      export_workbench.export_step(reopened, {GuiStepExportMode::PartMultiBody, step_path});
  const std::string export_diagnostic =
      exported.has_error() ? exported.error().message() : "STEP export ok";
  INFO(export_diagnostic);
  REQUIRE(exported);
  CHECK(exported.value().written_bytes > 0);
  CHECK(std::filesystem::is_regular_file(step_path));
  std::filesystem::remove(document_path);
  std::filesystem::remove(step_path);
}

TEST_CASE(
    "Block 131 measures manipulator hit and preview budgets by model size",
    "[integration][interactive-modeling][performance][manipulator-hit][performance][preview]") {
  for (const std::size_t handle_count : {6U, 300U}) {
    GuiViewportManipulatorLayer layer;
    REQUIRE(layer.set_mapping(orthographic_mapping()));
    std::vector<GuiViewportManipulatorHandle> handles;
    handles.reserve(handle_count);
    for (std::size_t index = 0; index < handle_count; ++index) {
      GuiViewportManipulatorHandle handle;
      handle.id = "budget.handle." + std::to_string(index);
      handle.origin = {static_cast<double>(index % 20U) * 2.0,
                       static_cast<double>(index / 20U) * 2.0, 0.0};
      handle.axis = {1.0, 0.0, 0.0};
      handles.push_back(std::move(handle));
    }
    REQUIRE(layer.set_handles(std::move(handles)));
    const auto duration = elapsed_ms([&] {
      for (std::size_t iteration = 0; iteration < 200U; ++iteration)
        (void)layer.hits_at({100.0 + static_cast<double>(iteration % 5U), 100.0});
    });
    INFO("manipulator handles=" << handle_count << ", 200 hit tests=" << duration << " ms");
    CHECK(duration < 5000);
  }

  for (const std::size_t extra_components : {0U, 80U}) {
    GuiDocumentSession session;
    REQUIRE(session.open_file(std::filesystem::path(BLCAD_SOURCE_DIR) /
                              "examples/revolute_joint.blcad.project.json"));
    if (extra_components != 0U) {
      REQUIRE(session.commit_project_transaction(
          "Seed preview budget", [extra_components](Project& project) {
            std::size_t changed = 0U;
            for (std::size_t index = 0; index < extra_components; ++index) {
              auto instance = ComponentInstance::create(
                  ComponentInstanceId("component.budget." + std::to_string(index)), "Budget",
                  DocumentId("part.revolute_plate"), ComponentVisibility::Visible,
                  ComponentSuppressionState::Suppressed, ComponentGroundingState::Free);
              if (instance.has_error())
                return Result<std::size_t>::failure(instance.error());
              auto added = project.assembly().add_component_instance(std::move(instance.value()));
              if (added.has_error())
                return added;
              changed += added.value();
            }
            return Result<std::size_t>::success(changed);
          }));
    }
    GuiInteractiveAssemblyPlacementController placement;
    REQUIRE(placement.begin_edit_component(session, ComponentInstanceId("component.plate.free")));
    const auto duration = elapsed_ms([&] {
      for (std::size_t iteration = 0; iteration < 100U; ++iteration)
        REQUIRE(placement.preview(session));
    });
    const auto model_size = session.project()->assembly().component_instance_count();
    INFO("assembly components=" << model_size << ", 100 placement previews=" << duration << " ms");
    CHECK(duration < 5000);
  }
}
