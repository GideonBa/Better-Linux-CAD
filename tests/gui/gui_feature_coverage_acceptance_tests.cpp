#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_modeling_workspace.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/main_window.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QLineEdit>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {
const std::filesystem::path source_dir(BLCAD_SOURCE_DIR);

nlohmann::json read_json(const std::filesystem::path& path) {
  std::ifstream stream(path);
  REQUIRE(stream.good());
  nlohmann::json value;
  stream >> value;
  return value;
}

GuiViewportManipulatorMapping orthographic_manipulator_mapping() {
  GuiViewportCameraBookmark camera;
  camera.eye = {0.0, 0.0, 100.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.up_direction = {0.0, 1.0, 0.0};
  camera.scale = 200.0;
  camera.projection = GuiViewportProjection::Orthographic;
  return GuiViewportManipulatorMapping::create_camera(camera, 200.0, 200.0).value();
}
} // namespace

TEST_CASE("Block 105 coverage manifest accounts for every implemented GUI feature family",
          "[integration][gui-feature-coverage]") {
  const auto manifest = read_json(source_dir / "docs/gui-feature-coverage-manifest-mvp7.json");
  CHECK(manifest.at("schema") == "blcad.gui-feature-coverage.v1");
  CHECK(manifest.at("implemented_through") == 105);
  const std::set<std::string> required = {
      "document-session", "viewport-selection", "browser-properties", "parameters-expressions",
      "datum-construction-workplanes", "planar-sketch-reference-repair",
      "bodies-extrude-revolve-holes", "pattern-mirror-finishing-shell-draft",
      "body-boolean-transform", "sketch3d-pathcurve", "sweep-path-extrude-loft",
      "surface-features", "assembly-project-hierarchy", "assembly-relationships-joints",
      "assembly-solve-motion", "assembly-analysis", "part-step-export", "assembly-step-export"};
  std::set<std::string> covered;
  for (const auto& family : manifest.at("families")) {
    const std::string name = family.at("family");
    CHECK(covered.insert(name).second);
    for (const char* field : {"command", "inspection", "presentation", "persistence", "test",
                              "disposition"})
      CHECK_FALSE(family.at(field).get<std::string>().empty());
    CHECK(family.at("owner").get<int>() >= 95);
    CHECK(family.at("owner").get<int>() <= 104);
    if (family.at("disposition") == "read_only")
      CHECK(family.at("core_read_only") == true);
  }
  CHECK(covered == required);
  for (const auto& sample : manifest.at("sample_documents"))
    CHECK(std::filesystem::is_regular_file(source_dir / sample.get<std::string>()));
}

TEST_CASE("Block 105 GUI Part sample is canonically equivalent to headless load and recompute",
          "[integration][gui-feature-coverage][integration][gui-headless-equivalence]") {
  const auto path = source_dir / "examples/bolt_circle_plate.blcad.json";
  auto headless = read_part_document_json_file(path);
  REQUIRE(headless);
  GuiDocumentSession gui;
  REQUIRE(gui.open_file(path));
  REQUIRE(gui.part_document() != nullptr);
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() ==
        serialize_part_document_to_json(headless.value()).value());

  auto cache = geometry::ShapeCache::create(ShapeCacheId("acceptance.headless")).value();
  auto executed = geometry::GeometryRecomputeExecutor{}.execute_document(headless.value(), cache);
  REQUIRE(executed);
  REQUIRE(gui.recompute());
  CHECK(gui.part_shape_cache()->feature_shape_count() == cache.feature_shape_count());
  CHECK(gui.part_shape_cache()->body_shape_count() == cache.body_shape_count());
  CHECK(gui.part_shape_cache()->has_final_shape() == cache.has_final_shape());

  const std::string original = serialize_part_document_to_json(*gui.part_document()).value();
  const Parameter& parameter = gui.part_document()->parameters().front();
  REQUIRE(parameter.type() == ParameterType::Length);
  GuiPartFoundationWorkbench foundation;
  REQUIRE(foundation.set_parameter_value(
      gui, parameter.id(),
      Quantity::length_mm(parameter.value().millimeters() + 1.0, parameter.id().value()).value()));
  const std::string edited = serialize_part_document_to_json(*gui.part_document()).value();
  CHECK(edited != original);
  REQUIRE(gui.undo());
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() == original);
  REQUIRE(gui.redo());
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() == edited);
  CHECK(gui.derived_results_fresh());
}

TEST_CASE("Block 105 GUI Project sample equals headless intent and survives recompute",
          "[integration][gui-feature-coverage][integration][gui-headless-equivalence]") {
  const auto path = source_dir / "examples/nested_subassembly.blcad.project.json";
  auto headless = read_project_json_file(path);
  REQUIRE(headless);
  GuiDocumentSession gui;
  REQUIRE(gui.open_file(path));
  REQUIRE(gui.project() != nullptr);
  CHECK(serialize_project_to_json(*gui.project()).value() ==
        serialize_project_to_json(headless.value()).value());
  REQUIRE(gui.recompute());
  CHECK(gui.project()->validate_assembly_structure());
  CHECK(gui.project()->assembly().subassembly_instance_count() > 0);
}

TEST_CASE("Block 105 integrated fail-closed paths publish no partial GUI state",
          "[integration][gui-feature-coverage]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.fail_closed"), "Fail closed"));
  GuiPartFoundationWorkbench foundation;
  REQUIRE(foundation.create_parameter(
      session, Parameter::create_length(ParameterId("depth"), "Depth",
                                        Quantity::length_mm(10).value())
                   .value()));
  const auto before_revision = session.presentation_revision();
  auto invalid = Feature::create_additive_extrude(FeatureId("feature.invalid"), "Invalid",
                                                   SketchId("missing.sketch"),
                                                   ParameterId("depth"))
                     .value();
  CHECK(foundation.preview_extrude(session, invalid).has_error());
  CHECK(session.part_document()->feature_count() == 0);
  CHECK(session.presentation_revision() == before_revision);
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateSketch).required_capability ==
        "planar_workplane");
  session.set_derived_results_fresh(false);
  GuiAnalysisExportWorkbench export_workbench;
  const auto output = std::filesystem::temp_directory_path() / "blcad-block105-invalid.step";
  std::filesystem::remove(output);
  CHECK(export_workbench
            .preflight_step_export(session, {GuiStepExportMode::PartMultiBody, output})
            .has_error());
  CHECK_FALSE(std::filesystem::exists(output));
  std::filesystem::remove(output);
}

TEST_CASE("Block 122 capability preselection drives the contextual modeling mini-toolbar",
          "[gui][modeling-workspace][gui][in-context-command]") {
  GuiDocumentSession session;
  GuiModelingWorkspace workspace;
  REQUIRE(session.create_part(DocumentId("part.modeling_workspace"), "Modeling Workspace"));
  REQUIRE(workspace.set_area(session, GuiModelingArea::Part));
  workspace.apply_selection_filter(session, GuiModelingSelectionFilter::Profiles);
  REQUIRE(workspace.set_preselection(
      session, {{GuiSelectionKind::SketchEntity, "profile-region:sketch.main:profile.outer"},
                {"ProfileRegion"}}));

  const auto enabled = workspace.enabled_commands(session);
  REQUIRE(enabled.size() == 5);
  CHECK(enabled.front().id == "part.extrude");
  const auto mini_toolbar = workspace.mini_toolbar_commands(session);
  REQUIRE(mini_toolbar.size() == 4);
  CHECK(mini_toolbar[0].label == "Extrude");
  CHECK(mini_toolbar[1].label == "Revolve");
  CHECK(mini_toolbar[2].label == "Sweep");
  CHECK(mini_toolbar[3].label == "Loft");

  REQUIRE(workspace.begin_command(session, GuiModelingCommand::Extrude));
  CHECK(session.selection().empty());
  CHECK(session.task().command_id() == "part.extrude");
  REQUIRE(workspace.cancel_command(session));
  REQUIRE(workspace.preselection().has_value());
  CHECK(workspace.preselection()->supports("ProfileRegion"));
  CHECK(session.selection().contains(GuiSelectionKind::SketchEntity,
                                     "profile-region:sketch.main:profile.outer"));
  CHECK(workspace.mini_toolbar_commands(session).front().id == "part.extrude");

  REQUIRE(workspace.begin_command(session, GuiModelingCommand::Extrude));
  REQUIRE(session.task().begin_parameter_editing());
  REQUIRE(session.task().show_preview());
  REQUIRE(workspace.mark_committed(session));
  CHECK(workspace.last_repeatable_command_id() == "part.extrude");
  REQUIRE(workspace.repeat_last_command(session));
  CHECK(session.task().command_id() == "part.extrude");
  REQUIRE(workspace.cancel_command(session));
}

TEST_CASE("Block 122 Finish Sketch handoff and selection filters fail closed",
          "[gui][modeling-workspace][gui][in-context-command]") {
  GuiDocumentSession session;
  GuiModelingWorkspace workspace;
  GuiPartFoundationWorkbench foundation;
  GuiSketchWorkbench sketches;
  REQUIRE(session.create_part(DocumentId("part.finish_handoff"), "Finish Handoff"));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.profile"), "Profile Plane"));
  REQUIRE(foundation.create_parameter(
      session, Parameter::create_length(ParameterId("profile.diameter"), "Profile Diameter",
                                        Quantity::length_mm(20).value())
                   .value()));
  REQUIRE(sketches.create_sketch(
      session, Sketch::create(SketchId("sketch.profile"), "Profile",
                              DatumPlaneId("datum.profile"))
                   .value()));
  REQUIRE(sketches.add_profile(
      session, SketchId("sketch.profile"),
      CircleProfile::create(ProfileId("profile.finished"), ParameterId("profile.diameter"))
          .value()));

  CHECK_FALSE(workspace.finish_sketch_handoff(session, SketchId("sketch.profile"),
                                              ProfileId("profile.missing")));
  REQUIRE(workspace.finish_sketch_handoff(session, SketchId("sketch.profile"),
                                          ProfileId("profile.finished")));
  REQUIRE(workspace.preselection().has_value());
  CHECK(workspace.preselection()->supports("ProfileRegion"));
  CHECK(workspace.mini_toolbar_commands(session).front().id == "part.extrude");

  workspace.apply_selection_filter(session, GuiModelingSelectionFilter::Faces);
  CHECK_FALSE(workspace.preselection().has_value());
  CHECK_FALSE(workspace.set_preselection(
      session, {{GuiSelectionKind::Edge, "edge:feature.one:outer"}, {"Edge"}}));
  workspace.apply_selection_filter(session, GuiModelingSelectionFilter::Edges);
  REQUIRE(workspace.set_preselection(
      session, {{GuiSelectionKind::Edge, "edge:feature.one:outer"}, {"Edge"}}));
  const auto commands = workspace.mini_toolbar_commands(session);
  REQUIRE(commands.size() == 2);
  CHECK(commands[0].id == "part.fillet");
  CHECK(commands[1].id == "part.chamfer");
}

TEST_CASE("Block 122 exposes visible transient workspace and camera controls through MainWindow",
          "[gui][modeling-workspace][gui][view-navigation-aids]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  REQUIRE(window.session().create_part(DocumentId("part.navigation"), "Navigation"));
  auto* viewport = window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
  REQUIRE(viewport != nullptr);

  auto& workspace = window.modeling_workspace();
  auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("blcad.modeling_toolbar"));
  auto* tabs = window.findChild<QTabBar*>(QStringLiteral("blcad.modeling_tabs"));
  auto* filter = window.findChild<QComboBox*>(QStringLiteral("blcad.modeling_selection_filter"));
  auto* mini_toolbar =
      window.findChild<QFrame*>(QStringLiteral("blcad.modeling_mini_toolbar"));
  auto* view_cube = window.findChild<QToolButton*>(QStringLiteral("blcad.view_cube"));
  auto* bookmarks = window.findChild<QComboBox*>(QStringLiteral("blcad.camera_bookmarks"));
  REQUIRE(toolbar != nullptr);
  REQUIRE(tabs != nullptr);
  REQUIRE(filter != nullptr);
  REQUIRE(mini_toolbar != nullptr);
  REQUIRE(view_cube != nullptr);
  REQUIRE(bookmarks != nullptr);
  CHECK(tabs->count() == 3);
  CHECK(tabs->tabText(0) == QStringLiteral("Part"));
  CHECK(tabs->tabText(1) == QStringLiteral("Surface"));
  CHECK(tabs->tabText(2) == QStringLiteral("Assembly"));
  CHECK(filter->count() == 8);

  REQUIRE(workspace.set_area(window.session(), GuiModelingArea::Part));
  CHECK(GuiModelingWorkspace::tabs()[0] == "Part");
  CHECK(GuiModelingWorkspace::tabs()[1] == "Surface");
  CHECK(GuiModelingWorkspace::tabs()[2] == "Assembly");

  workspace.apply_selection_filter(window.session(), *viewport,
                                   GuiModelingSelectionFilter::Bodies);
  CHECK(window.session().selection().filter_mask() ==
        selection_kind_bit(GuiSelectionKind::Body));
  CHECK(viewport->selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));

  // "All" must publish the unfiltered sentinel so activating the modeling
  // workspace never narrows viewport selectability below the Block-106 default.
  workspace.apply_selection_filter(window.session(), *viewport,
                                   GuiModelingSelectionFilter::All);
  CHECK(window.session().selection().filter_mask() == 0xFFFFFFFFU);
  CHECK(viewport->selection_filter_mask() == 0xFFFFFFFFU);

  workspace.capture_home_view(*viewport);
  REQUIRE(workspace.activate_view_cube_target(*viewport, GuiViewCubeTarget::Front));
  REQUIRE(workspace.save_camera_bookmark("front inspection", *viewport));
  REQUIRE(workspace.activate_view_cube_target(*viewport, GuiViewCubeTarget::Top));
  REQUIRE(workspace.restore_camera_bookmark("front inspection", *viewport));
  REQUIRE(workspace.activate_view_cube_target(*viewport, GuiViewCubeTarget::Home));
  REQUIRE(workspace.camera_bookmarks().size() == 1);
  CHECK(workspace.camera_bookmarks().front().name == "front inspection");
  REQUIRE(workspace.remove_camera_bookmark("front inspection"));
  CHECK(workspace.camera_bookmarks().empty());
}

TEST_CASE("Block 123 linear manipulators are zoom-stable and release the exact final sample",
          "[gui][viewport-manipulators]") {
  GuiViewportManipulatorLayer layer;
  REQUIRE(layer.set_mapping(orthographic_manipulator_mapping()));
  GuiViewportManipulatorHandle linear;
  linear.id = "distance.arrow";
  linear.kind = GuiViewportManipulatorKind::Linear;
  linear.origin = {0.0, 0.0, 0.0};
  linear.axis = {1.0, 0.0, 0.0};
  linear.reference_value = 10.0;
  REQUIRE(layer.set_handles({linear}));

  const auto presentation = layer.presentations();
  REQUIRE(presentation.size() == 1);
  CHECK(std::abs(std::hypot(presentation.front().endpoint.x - presentation.front().anchor.x,
                            presentation.front().endpoint.y - presentation.front().anchor.y) -
                 68.0) < 1.0e-9);
  REQUIRE(layer.begin_drag({160.0, 100.0}));
  REQUIRE(layer.update_drag({175.0, 100.0}));
  REQUIRE(layer.candidate().has_value());
  CHECK(std::abs(layer.candidate()->scalar_value - 25.0) < 1.0e-9);

  const auto released = layer.end_drag({180.0, 100.0});
  REQUIRE(released.has_value());
  CHECK(std::abs(released->scalar_value - 30.0) < 1.0e-9);
  CHECK(released->source == GuiViewportManipulatorInputSource::Drag);
  CHECK(layer.active_handle_id().empty());
}

TEST_CASE("Block 123 hit ties and numeric override are deterministic and non-mutating",
          "[gui][viewport-manipulators][gui][manipulator-numeric-coupling]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.manipulator"), "Manipulator"));
  const std::string before = serialize_part_document_to_json(*session.part_document()).value();

  GuiViewportManipulatorLayer layer;
  REQUIRE(layer.set_mapping(orthographic_manipulator_mapping()));
  GuiViewportManipulatorHandle second;
  second.id = "b.handle";
  second.origin = {0.0, 0.0, 0.0};
  second.axis = {1.0, 0.0, 0.0};
  GuiViewportManipulatorHandle first = second;
  first.id = "a.handle";
  REQUIRE(layer.set_handles({second, first}));

  const auto hits = layer.hits_at({160.0, 100.0});
  REQUIRE(hits.size() == 2);
  CHECK(hits[0].handle_id == "a.handle");
  CHECK(hits[1].handle_id == "b.handle");
  REQUIRE(layer.begin_drag({160.0, 100.0}));
  REQUIRE(layer.update_drag({170.0, 100.0}));
  REQUIRE(layer.set_numeric_input("42.5 mm"));
  REQUIRE(layer.candidate().has_value());
  CHECK(layer.candidate()->source == GuiViewportManipulatorInputSource::Numeric);
  CHECK(std::abs(layer.candidate()->scalar_value - 42.5) < 1.0e-9);
  CHECK(layer.numeric_text() == "42.5 mm");
  REQUIRE(layer.update_drag({190.0, 100.0}));
  CHECK(std::abs(layer.candidate()->scalar_value - 42.5) < 1.0e-9);
  const auto released = layer.end_drag({195.0, 100.0});
  REQUIRE(released.has_value());
  CHECK(std::abs(released->scalar_value - 42.5) < 1.0e-9);
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
}

TEST_CASE("Block 123 supports angular radial triad and pattern candidate families",
          "[gui][viewport-manipulators][gui][manipulator-numeric-coupling]") {
  GuiViewportManipulatorLayer layer;
  REQUIRE(layer.set_mapping(orthographic_manipulator_mapping()));

  GuiViewportManipulatorHandle angle;
  angle.id = "angle.wheel";
  angle.kind = GuiViewportManipulatorKind::Angular;
  angle.origin = {0.0, 0.0, 0.0};
  angle.plane_normal = {0.0, 0.0, 1.0};
  angle.reference_direction = {1.0, 0.0, 0.0};
  angle.display_size_dip = 50.0;
  REQUIRE(layer.set_handles({angle}));
  REQUIRE(layer.begin_drag({150.0, 100.0}));
  const auto angle_candidate = layer.end_drag({100.0, 50.0});
  REQUIRE(angle_candidate.has_value());
  CHECK(angle_candidate->value_kind == GuiViewportManipulatorValueKind::Angle);
  CHECK(std::abs(angle_candidate->scalar_value - 90.0) < 1.0e-9);

  GuiViewportManipulatorHandle radial;
  radial.id = "radius.handle";
  radial.kind = GuiViewportManipulatorKind::Radial;
  radial.origin = {0.0, 0.0, 0.0};
  radial.plane_normal = {0.0, 0.0, 1.0};
  radial.reference_direction = {1.0, 0.0, 0.0};
  radial.reference_value = 10.0;
  radial.display_size_dip = 50.0;
  REQUIRE(layer.set_handles({radial}));
  REQUIRE(layer.begin_drag({150.0, 100.0}));
  const auto radius_candidate = layer.end_drag({170.0, 100.0});
  REQUIRE(radius_candidate.has_value());
  CHECK(radius_candidate->value_kind == GuiViewportManipulatorValueKind::Radius);
  CHECK(std::abs(radius_candidate->scalar_value - 30.0) < 1.0e-9);

  const auto translate =
      GuiViewportManipulatorLayer::translation_triad("move", {0.0, 0.0, 0.0});
  const auto rotate =
      GuiViewportManipulatorLayer::rotation_triad("rotate", {0.0, 0.0, 0.0});
  CHECK(translate.size() == 3);
  CHECK(rotate.size() == 3);
  CHECK(translate[0].kind == GuiViewportManipulatorKind::TranslateAxis);
  CHECK(rotate[2].kind == GuiViewportManipulatorKind::RotateAxis);

  GuiViewportManipulatorHandle count;
  count.id = "pattern.count";
  count.kind = GuiViewportManipulatorKind::PatternCount;
  count.origin = {0.0, 0.0, 0.0};
  count.axis = {1.0, 0.0, 0.0};
  count.reference_value = 4.0;
  count.count_model_step = 12.0;
  REQUIRE(layer.set_handles({count}));
  REQUIRE(layer.begin_drag({150.0, 100.0}));
  const auto count_candidate = layer.end_drag({174.0, 100.0});
  REQUIRE(count_candidate.has_value());
  CHECK(count_candidate->value_kind == GuiViewportManipulatorValueKind::Count);
  CHECK(count_candidate->count_value == 6);
}

TEST_CASE("Block 123 exposes the reusable manipulator overlay and numeric HUD through MainWindow",
          "[gui][viewport-manipulators][gui][manipulator-numeric-coupling]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  REQUIRE(window.session().create_part(DocumentId("part.manipulator.shell"),
                                       "Manipulator Shell"));
  auto& layer = window.viewport_manipulators();
  GuiViewportManipulatorHandle handle;
  handle.id = "shell.distance";
  handle.kind = GuiViewportManipulatorKind::Linear;
  handle.origin = {0.0, 0.0, 0.0};
  handle.axis = {1.0, 0.0, 0.0};
  REQUIRE(layer.set_handles({handle}));
  window.refresh_viewport_manipulators();
  qApp->processEvents();

  auto* overlay =
      window.findChild<QWidget*>(QStringLiteral("blcad.viewport_manipulator_overlay"));
  auto* hud = window.findChild<QLineEdit*>(QStringLiteral("blcad.manipulator_numeric_hud"));
  REQUIRE(overlay != nullptr);
  REQUIRE(hud != nullptr);
  CHECK_FALSE(overlay->isHidden());
  CHECK_FALSE(hud->isHidden());
  CHECK(layer.presentations().size() == 1);
}
