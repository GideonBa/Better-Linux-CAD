#include "../support/adaptive_project_fixture.hpp"
#include "../support/engineering_resource_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace history = blcad::test::history_fixture;
namespace resources = blcad::test::engineering_fixture;
namespace {
void show_native_window(ShellWindow& window) {
  REQUIRE(window.screen());
  const QSize requested =
      qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  auto bounds = window.screen()->availableGeometry().adjusted(12, 12, -12, -12);
  window.resize(requested.boundedTo(bounds.size() - QSize(24, 48)));
  window.move(bounds.topLeft());
  window.show();
  window.raise();
  window.activateWindow();
  REQUIRE(QTest::qWaitForWindowExposed(&window));
  REQUIRE(QTest::qWaitForWindowActive(&window));
  QApplication::processEvents();
  bounds = window.screen()->availableGeometry().adjusted(12, 12, -12, -12);
  const auto decoration = window.frameGeometry().size() - window.size();
  window.resize(requested.boundedTo(bounds.size() - decoration));
  QApplication::processEvents();
  window.move(window.pos() + bounds.topLeft() - window.frameGeometry().topLeft());
  REQUIRE(QTest::qWaitFor([&] { return bounds.contains(window.frameGeometry()); }, 2000));
  REQUIRE(window.screen()->availableGeometry().contains(window.frameGeometry()));
}
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
ProjectDesignOccurrence occurrence() {
  return {{SubassemblyInstanceId("mount")}, ComponentInstanceId("nested"), DocumentId("history")};
}
Project project() {
  auto part = history::native_base();
  resources::annotate(part);
  auto sketch =
      checked(Sketch::create(SketchId("source.sketch"), "Source Sketch", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_entity(
      checked(LineSegment::create(SketchEntityId("source.line"), {-2, 0}, {2, 0}))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root Assembly"));
  REQUIRE(root.add_member_part(part.id()));
  REQUIRE(root.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("direct"), "Direct", part.id()))));
  auto child = checked(AssemblyDocument::create(DocumentId("child"), "Child"));
  REQUIRE(child.add_member_part(part.id()));
  REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("nested"), "Nested", part.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {{2, 0, 0}, {}}))));
  REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("mount"), "Rotated parent", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, {{20, 30, 40}, {0, 0, 90}}))));
  auto result = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(result.add_part_document(std::move(part)));
  REQUIRE(result.add_child_assembly_document(std::move(child)));
  return result;
}
QString seed(GuiDocumentSession& s, const QTemporaryDir& directory, Project data = project()) {
  const auto path = directory.filePath("context.blcad.project.json");
  REQUIRE(write_project_json_file(data, path.toStdString()));
  REQUIRE(s.open_file(path.toStdString()));
  return path;
}
std::string snapshot(const Project& p) {
  return checked(serialize_project_to_json(p));
}
std::string part_text(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
std::string bytes(const std::filesystem::path& path) {
  QFile file(QString::fromStdString(path.string()));
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll().toStdString();
}
void width(GuiDocumentSession& s, double value) {
  REQUIRE(s.commit_part_transaction("Change context width", [&](PartDocument& p) {
    auto changed = p.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(value)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto* cache = s.part_shape_cache();
  REQUIRE(cache);
  (void)checked(cache->validate_complete_geometry_result(*s.part_document()));
}
void catalogs(GuiDocumentSession& s) {
  REQUIRE(s.commit_part_sketch_intent_transaction(
      "Constrain Source Sketch",
      [](PartDocument& p, std::vector<SketchConstraintCatalog>& constraints,
         std::vector<SketchDimensionCatalog>& dimensions) {
        const auto entity = checked(SketchConstraintIntentTarget::entity("entity/source.line"));
        constraints = {checked(SketchConstraintCatalog::create(
            p.id(), SketchId("source.sketch"),
            {checked(SketchConstraintIntent::create(SketchConstraintId("horizontal"),
                                                    SketchSolverConstraintKind::Horizontal,
                                                    {entity}))}))};
        dimensions = {checked(SketchDimensionCatalog::create(
            p.id(), SketchId("source.sketch"),
            {checked(SketchDimensionIntent::create(SketchDimensionId("reference.length"),
                                                   SketchDimensionKind::Length,
                                                   SketchDimensionMode::Reference, {entity}))}))};
        return Result<std::size_t>::success(2);
      }));
}
double current_width(const GuiDocumentSession& s) {
  return s.part_document()->find_parameter(ParameterId("width"))->value().millimeters();
}
} // namespace

TEST_CASE("In-context Part cancellation preserves exact rooted definitions resources catalogs and "
          "history",
          "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  GuiDocumentSession s;
  const auto path = seed(s, directory);
  const auto before = snapshot(*s.project());
  const auto saved = bytes(path.toStdString());
  const auto revision = s.model_revision();
  REQUIRE(s.begin_project_part_context(occurrence()));
  CHECK(s.project_part_context_active());
  REQUIRE(s.context_occurrence());
  CHECK(*s.context_occurrence() == occurrence());
  CHECK(s.project() == nullptr);
  REQUIRE(s.part_document());
  REQUIRE(s.project_part_context_root());
  CHECK(s.project_part_context_root()->id() == DocumentId("project"));
  const auto world = geometry::AssemblyHierarchyTransformEvaluator{}.evaluate_point(
      s.project_part_context_transforms(), {1, 0, 0});
  CHECK(world.x == Catch::Approx(20));
  CHECK(world.y == Catch::Approx(33));
  CHECK(world.z == Catch::Approx(40));
  width(s, 30);
  catalogs(s);
  CHECK(current_width(s) == 30);
  CHECK(s.dirty());
  REQUIRE(s.save());
  CHECK(bytes(path.toStdString()) == saved);
  CHECK_FALSE(std::filesystem::exists(project_part_sketch_catalog_sidecar(path.toStdString())));
  s.cancel_project_part_context();
  CHECK_FALSE(s.project_part_context_active());
  CHECK(s.part_document() == nullptr);
  CHECK(snapshot(*s.project()) == before);
  CHECK(s.project_part_sketch_catalogs().empty());
  CHECK_FALSE(s.can_undo());
  CHECK_FALSE(s.dirty());
  CHECK(s.model_revision() > revision);
}

TEST_CASE(
    "In-context changes and child Sketch catalogs publish as one root Undo and reopen exactly",
    "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  GuiDocumentSession s;
  const auto path = seed(s, directory);
  const auto before = snapshot(*s.project());
  REQUIRE(s.begin_project_part_context(occurrence()));
  width(s, 30);
  catalogs(s);
  (void)checked(s.finish_project_part_context("Edit Part: Nested"));
  CHECK_FALSE(s.project_part_context_active());
  CHECK(s.undo_label() == "Edit Part: Nested");
  REQUIRE(s.project_part_sketch_catalogs().size() == 1);
  CHECK(s.project_part_sketch_catalogs().front().part == DocumentId("history"));
  CHECK(s.project_part_sketch_catalogs().front().constraints.front().constraints().size() == 1);
  auto physical =
      checked(geometry::BodyResultInspector{}.inspect_physical_properties(*s.effective_project()));
  CHECK(physical.data().volume_mm3 == Catch::Approx(4800));
  REQUIRE(physical.data().known_mass);
  CHECK(physical.data().known_mass->mass_kg == Catch::Approx(.0192));
  const auto after = snapshot(*s.project());
  const auto owned = s.project_part_sketch_catalogs();
  REQUIRE(s.undo());
  CHECK(snapshot(*s.project()) == before);
  CHECK(s.project_part_sketch_catalogs().empty());
  CHECK_FALSE(s.can_undo());
  REQUIRE(s.redo());
  CHECK(snapshot(*s.project()) == after);
  CHECK(s.project_part_sketch_catalogs() == owned);
  REQUIRE(s.save());
  const auto sidecar = project_part_sketch_catalog_sidecar(path.toStdString());
  REQUIRE(std::filesystem::exists(sidecar));
  const auto sidecar_bytes = bytes(sidecar);
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(snapshot(*reopened.project()) == after);
  CHECK(reopened.project_part_sketch_catalogs() == owned);
  CHECK_FALSE(reopened.dirty());
  REQUIRE(reopened.begin_project_part_context(occurrence()));
  REQUIRE(reopened.sketch_constraint_catalogs().size() == 1);
  REQUIRE(reopened.sketch_dimension_catalogs().size() == 1);
  CHECK(reopened.sketch_constraint_catalogs().front().document() == DocumentId("history"));
  CHECK(reopened.sketch_dimension_catalogs().front().dimensions().front().id() ==
        SketchDimensionId("reference.length"));
  CHECK(checked(reopened.finish_project_part_context("Unchanged Part")) == 0);
  REQUIRE(reopened.save());
  CHECK(bytes(sidecar) == sidecar_bytes);
}

TEST_CASE("Context guards preserve pending tasks source identity and the current selected scope",
          "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  GuiDocumentSession s;
  seed(s, directory);
  const auto before = snapshot(*s.project());
  REQUIRE(s.task().begin("pending"));
  CHECK_FALSE(s.begin_project_part_context(occurrence()));
  REQUIRE(s.task().cancel());
  auto invalid = occurrence();
  invalid.path = {SubassemblyInstanceId("missing")};
  CHECK_FALSE(s.begin_project_part_context(invalid));
  CHECK(snapshot(*s.project()) == before);
  REQUIRE(s.begin_project_part_context(occurrence()));
  CHECK_FALSE(s.create_part(DocumentId("replacement"), "Replacement", true));
  CHECK_FALSE(s.close(true));
  CHECK_FALSE(s.activate_model_state(primary_model_state_id()));
  const auto initial = part_text(*s.part_document());
  CHECK_FALSE(s.commit_part_transaction("Replace owner", [](PartDocument& p) {
    p = checked(PartDocument::create(DocumentId("other"), "Other"));
    return Result<std::size_t>::success(1);
  }));
  CHECK(part_text(*s.part_document()) == initial);
  REQUIRE(s.task().begin("pending.child"));
  CHECK_FALSE(s.finish_project_part_context("Invalid return"));
  REQUIRE(s.task().cancel());
  s.cancel_project_part_context();
  CHECK(snapshot(*s.project()) == before);
}

TEST_CASE("A failed Project sidecar Save and malformed reopen preserve all accepted model state",
          "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  GuiDocumentSession s;
  const auto path = seed(s, directory);
  REQUIRE(s.begin_project_part_context(occurrence()));
  catalogs(s);
  width(s, 30);
  (void)checked(s.finish_project_part_context("Edit Part"));
  REQUIRE(s.save());
  const auto sidecar = project_part_sketch_catalog_sidecar(path.toStdString());
  const auto saved_main = bytes(path.toStdString()), saved_sidecar = bytes(sidecar);
  const auto accepted = snapshot(*s.project());
  const auto saved_catalogs = s.project_part_sketch_catalogs();
  REQUIRE(s.begin_project_part_context(occurrence()));
  width(s, 40);
  (void)checked(s.finish_project_part_context("Second Part edit"));
  const auto changed = snapshot(*s.project());
  const auto preserved = std::filesystem::path(sidecar.string() + ".preserved");
  std::filesystem::rename(sidecar, preserved);
  REQUIRE(std::filesystem::create_directory(sidecar));
  CHECK_FALSE(s.save());
  CHECK(bytes(path.toStdString()) == saved_main);
  CHECK(bytes(preserved) == saved_sidecar);
  CHECK(std::filesystem::is_directory(sidecar));
  CHECK(snapshot(*s.project()) == changed);
  CHECK(s.dirty());
  REQUIRE(std::filesystem::remove(sidecar));
  std::filesystem::rename(preserved, sidecar);
  REQUIRE(s.undo());
  CHECK(snapshot(*s.project()) == accepted);
  CHECK(s.project_part_sketch_catalogs() == saved_catalogs);
  CHECK_FALSE(s.dirty());
  auto malformed = nlohmann::json::parse(saved_sidecar);
  malformed["parts"][0]["part"] = "missing.owner";
  {
    QFile file(QString::fromStdString(sidecar.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto data = malformed.dump();
    REQUIRE(file.write(data.data(), data.size()) == static_cast<qint64>(data.size()));
  }
  CHECK_FALSE(s.open_file(path.toStdString(), true));
  CHECK(snapshot(*s.project()) == accepted);
  CHECK(s.project_part_sketch_catalogs() == saved_catalogs);
}

TEST_CASE("Linked context constructions reject direct edits and failed downstream rebuild keeps "
          "the root checkpoint",
          "[gui][project-part-context][parity-b198]") {
  auto data = blcad::test::adaptive_project();
  const auto source_pose =
      data.assembly().find_component_instance(ComponentInstanceId("source"))->transform();
  REQUIRE(
      data.assembly().set_component_instance_transform(ComponentInstanceId("cover"), source_pose));
  auto* source = data.find_part_document(DocumentId("source"));
  REQUIRE(source->set_parameter_value(ParameterId("length"), checked(Quantity::length_mm(1))));
  auto* cover = data.find_part_document(DocumentId("cover"));
  REQUIRE(cover->add_parameter(checked(
      Parameter::create_length(ParameterId("bore"), "Bore", checked(Quantity::length_mm(2))))));
  REQUIRE(cover->add_parameter(checked(Parameter::create_length(
      ParameterId("cut.depth"), "Fixed cut depth", checked(Quantity::length_mm(2))))));
  auto sketch =
      checked(Sketch::create(SketchId("cut.sketch"), "Static bore", DatumPlaneId("datum.xy")));
  REQUIRE(
      sketch.add_profile(checked(CircleProfile::create(ProfileId("cut"), ParameterId("bore")))));
  REQUIRE(cover->add_sketch(sketch));
  auto cut = checked(Feature::create_subtractive_extrude(
      FeatureId("cut"), "Finite fixed-datum bore", SketchId("cut.sketch"), FeatureId("base"),
      checked(ExtrudeFeatureIntent::create(
          checked(ExtrudeExtentIntent::distance(ParameterId("cut.depth")))))));
  REQUIRE(cover->add_feature(
      checked(cut.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::Cut, BodyId("body.base"), std::nullopt))))));
  auto linked = checked(
      geometry::AdaptiveProjectEvaluator{}.bind(data, blcad::test::adaptive_plane_reference()));
  QTemporaryDir directory;
  GuiDocumentSession s;
  const auto path = seed(s, directory, std::move(linked.project));
  const auto before = snapshot(*s.project());
  const auto saved = bytes(path.toStdString());
  CHECK(s.project()
            ->find_part_document(DocumentId("source"))
            ->find_parameter(ParameterId("length"))
            ->value()
            .millimeters() == 1);
  const auto& initial_cover = *s.project()->find_part_document(DocumentId("cover"));
  auto proof = checked(geometry::ShapeCache::create(ShapeCacheId("context.finite.cut")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(initial_cover, proof));
  const auto* cut_shape = proof.find_body_shape(BodyId("body.base"));
  REQUIRE(cut_shape);
  CHECK(blcad::test::classify_solid_point(*cut_shape, {0, 0, 1.5}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(*cut_shape, {2, 0, 1.5}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(*cut_shape, {0, 0, 2.5}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(checked(geometry::BodyResultInspector{}.inspect_physical_properties(initial_cover))
            .data()
            .volume_mm3 == Catch::Approx(800 - std::numbers::pi).margin(1e-6));
  REQUIRE(s.begin_project_part_context({{}, ComponentInstanceId("cover"), DocumentId("cover")}));
  const auto original = part_text(*s.part_document());
  CHECK_FALSE(s.commit_part_transaction("Move linked plane", [](PartDocument& p) {
    return p.update_construction_plane(checked(
        ConstructionPlane::create_explicit(ConstructionPlaneId("linked.plane"), "Linked support",
                                           {0, 0, 2}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1})));
  }));
  CHECK(part_text(*s.part_document()) == original);
  s.cancel_project_part_context();
  REQUIRE(s.begin_project_part_context({{}, ComponentInstanceId("source"), DocumentId("source")}));
  CHECK(s.part_document()->find_parameter(ParameterId("length"))->value().millimeters() == 1);
  REQUIRE(s.commit_part_transaction("Source length", [](PartDocument& p) {
    auto result = p.set_parameter_value(ParameterId("length"), checked(Quantity::length_mm(8)));
    return result ? Result<std::size_t>::success(1) : Result<std::size_t>::failure(result.error());
  }));
  CHECK(s.part_document()->find_parameter(ParameterId("length"))->value().millimeters() == 8);
  const auto rejected = s.finish_project_part_context("Rejected downstream bore");
  INFO((rejected ? "unexpected success" : rejected.error().message()));
  CHECK_FALSE(rejected);
  CHECK(s.project_part_context_active());
  CHECK(bytes(path.toStdString()) == saved);
  s.cancel_project_part_context();
  CHECK(snapshot(*s.project()) == before);
}

TEST_CASE("Native root-to-Part navigation reopens owned Sketch constraints and dimensions",
          "[gui][project-part-context-native][parity-b198]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Native in-context Sketch navigation requires xcb");
  QTemporaryDir directory;
  const auto path = directory.filePath("native-context.blcad.project.json");
  REQUIRE(write_project_json_file(project(), path.toStdString()));
  ShellWindow window;
  REQUIRE(window.open_path(path));
  show_native_window(window);
  const auto configure_idle = [&] {
    auto* filter = window.findChild<QComboBox*>("blcad.selection.filter");
    REQUIRE(filter);
    filter->setFocus();
    QApplication::processEvents();
    REQUIRE(filter->hasFocus());
    QTest::keyClick(filter, Qt::Key_Home);
    for (int i = 0; i < 5; ++i)
      QTest::keyClick(filter, Qt::Key_Down);
    REQUIRE(filter->currentText() == "Body");
    window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body:body.base"});
    REQUIRE(window.viewport().set_plane_camera({12, 8, 5}, {0, 0, 1}, {0, 1, 0}));
    return window.viewport().camera_bookmark();
  };
  const auto restored_idle = [&](const GuiViewportCameraBookmark& expected, std::uint32_t mask) {
    CHECK(window.viewport().selection_filter_mask() == mask);
    REQUIRE(window.viewport().selection_semantic_scope());
    CHECK(*window.viewport().selection_semantic_scope() ==
          std::vector<std::string>{"body:body.base"});
    const auto actual = window.viewport().camera_bookmark();
    CHECK(actual.eye.x == Catch::Approx(expected.eye.x));
    CHECK(actual.eye.y == Catch::Approx(expected.eye.y));
    CHECK(actual.eye.z == Catch::Approx(expected.eye.z));
    CHECK(actual.target.x == Catch::Approx(expected.target.x));
    CHECK(actual.target.y == Catch::Approx(expected.target.y));
    CHECK(actual.target.z == Catch::Approx(expected.target.z));
    CHECK(actual.up_direction.x == Catch::Approx(expected.up_direction.x));
    CHECK(actual.up_direction.y == Catch::Approx(expected.up_direction.y));
    CHECK(actual.up_direction.z == Catch::Approx(expected.up_direction.z));
    CHECK(actual.scale == Catch::Approx(expected.scale));
    CHECK(actual.projection == expected.projection);
    CHECK(actual.plane_camera == expected.plane_camera);
  };
  const auto click_context = [&](const char* id) {
    auto* button = window.findChild<QPushButton*>(QString("blcad.advanced_assembly.") + id);
    REQUIRE(button);
    REQUIRE(button->isVisible());
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
    REQUIRE_FALSE(window.session().project_part_context_active());
  };
  const auto finish_camera = configure_idle();
  const auto finish_mask = window.viewport().selection_filter_mask();
  REQUIRE(window.enter_project_part_context(occurrence()));
  auto* breadcrumb = window.findChild<QLabel*>("blcad.advanced_assembly.breadcrumb");
  REQUIRE(breadcrumb);
  CHECK(breadcrumb->isVisible());
  CHECK(breadcrumb->text().contains("Project"));
  CHECK(breadcrumb->text().contains("Part coordinates"));
  catalogs(window.session());
  width(window.session(), 30);
  click_context("finish_part");
  restored_idle(finish_camera, finish_mask);
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  const auto cancel_camera = configure_idle();
  const auto cancel_mask = window.viewport().selection_filter_mask();
  REQUIRE(window.enter_project_part_context(occurrence()));
  REQUIRE(window.session().sketch_constraint_catalogs().size() == 1);
  REQUIRE(window.session().sketch_dimension_catalogs().size() == 1);
  QTreeWidgetItem* target = nullptr;
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->text(0) == "Source Sketch") {
      target = *it;
      break;
    }
  REQUIRE(target);
  for (auto* parent = target->parent(); parent; parent = parent->parent())
    parent->setExpanded(true);
  window.model_browser().scrollToItem(target);
  QApplication::processEvents();
  const auto source_sketch_point = window.model_browser().visualItemRect(target).center();
  QTest::mouseClick(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                    source_sketch_point);
  QTest::mouseDClick(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                     source_sketch_point);
  QTest::mouseRelease(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                      source_sketch_point);
  REQUIRE(window.active_sketch() == SketchId("source.sketch"));
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(checked(window.session().sketch_constraint_catalog(SketchId("source.sketch")))
            .constraints()
            .front()
            .id() == SketchConstraintId("horizontal"));
  CHECK(checked(window.session().sketch_dimension_catalog(SketchId("source.sketch")))
            .dimensions()
            .front()
            .id() == SketchDimensionId("reference.length"));
  const auto capture = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!capture.isEmpty()) {
    REQUIRE(QDir().mkpath(capture));
    REQUIRE(window.grab().save(capture + "/in-context-source-sketch.png"));
  }
  click_context("cancel_part");
  restored_idle(cancel_camera, cancel_mask);
  CHECK_FALSE(window.active_sketch());
  CHECK(window.session().project());
  CHECK(window.session().project_part_sketch_catalogs().size() == 1);
}

TEST_CASE("A legacy STEP source retains the Project child-Sketch sidecar path until real catalogs "
          "need it",
          "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  const auto path = directory.filePath("context.blcad.project.json");
  const auto asset = project_part_sketch_catalog_sidecar(path.toStdString());
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(7)), checked(Quantity::length_mm(5)),
      checked(Quantity::length_mm(3))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, asset.string()));
  const auto content = bytes(asset);
  CHECK(content.starts_with("ISO-10303-21;"));
  const auto inspected =
      checked(inspect_step_import_asset(directory.path().toStdString(), asset.filename().string()));
  REQUIRE(inspected.observed_sha256_digest());
  auto legacy = project();
  REQUIRE(legacy.add_step_import_source(checked(StepImportSource::create(
      StepImportSourceId("legacy.source"), "Legacy immutable STEP source",
      asset.filename().string(), *inspected.observed_sha256_digest(), StepImportMode::Reference))));
  REQUIRE(write_project_json_file(legacy, path.toStdString()));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path.toStdString()));
  CHECK(s.project_part_sketch_catalogs().empty());
  CHECK(snapshot(*s.project()) == snapshot(legacy));
  REQUIRE(s.save());
  CHECK(bytes(asset) == content);
  const auto original_main = bytes(path.toStdString());
  REQUIRE(s.begin_project_part_context(occurrence()));
  catalogs(s);
  (void)checked(s.finish_project_part_context("Constrain legacy Project Part"));
  REQUIRE(s.project_part_sketch_catalogs().size() == 1);
  const auto accepted = snapshot(*s.project());
  const auto accepted_catalogs = s.project_part_sketch_catalogs();
  const auto rejected = s.save();
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("immutable STEP source") != std::string::npos);
  CHECK(bytes(path.toStdString()) == original_main);
  CHECK(bytes(asset) == content);
  CHECK(snapshot(*s.project()) == accepted);
  CHECK(s.project_part_sketch_catalogs() == accepted_catalogs);
  CHECK(s.dirty());
  const auto relocated = directory.filePath("new-context.blcad.project.json");
  REQUIRE(s.save_as(relocated.toStdString()));
  CHECK(bytes(asset) == content);
  CHECK(bytes(path.toStdString()) == original_main);
  REQUIRE(std::filesystem::exists(project_part_sketch_catalog_sidecar(relocated.toStdString())));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(relocated.toStdString()));
  CHECK(snapshot(*reopened.project()) == accepted);
  CHECK(reopened.project_part_sketch_catalogs() == accepted_catalogs);
  CHECK_FALSE(reopened.dirty());
}

TEST_CASE("In-context Assembly parameter bindings preserve their root authoring authority",
          "[gui][project-part-context][parity-b198]") {
  auto data = project();
  REQUIRE(data.assembly().add_parameter(checked(
      Parameter::create_length(ParameterId("driver.width"), "Driver width",
                               checked(Quantity::length_mm(20)), ParameterScope::Assembly))));
  REQUIRE(data.assembly().add_binding(
      checked(ParameterBinding::create(ParameterBindingId("width.binding"), DocumentId("history"),
                                       ParameterId("width"), ParameterId("driver.width")))));
  QTemporaryDir directory;
  GuiDocumentSession s;
  seed(s, directory, data);
  REQUIRE(s.begin_project_part_context(occurrence()));
  const auto before = part_text(*s.part_document());
  CHECK_FALSE(s.commit_part_transaction("Override bound width", [](PartDocument& p) {
    auto changed = p.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(30)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  CHECK(part_text(*s.part_document()) == before);
  s.cancel_project_part_context();
  CHECK(snapshot(*s.project()) == snapshot(data));
}

TEST_CASE("Project child-Sketch sidecars reject unknown fields versions and cross-owner catalogs "
          "atomically",
          "[gui][project-part-context][parity-b198]") {
  QTemporaryDir directory;
  GuiDocumentSession s;
  const auto path = seed(s, directory);
  REQUIRE(s.begin_project_part_context(occurrence()));
  catalogs(s);
  (void)checked(s.finish_project_part_context("Constrain Project Part"));
  REQUIRE(s.save());
  const auto original_project = snapshot(*s.project());
  const auto original_catalogs = s.project_part_sketch_catalogs();
  const auto sidecar = project_part_sketch_catalog_sidecar(path.toStdString());
  const auto original_bytes = bytes(sidecar);
  using Json = nlohmann::json;
  const std::vector<std::pair<const char*, std::function<void(Json&)>>> invalid{
      {"unsupported version", [](Json& j) { j["version"] = 2; }},
      {"noninteger version", [](Json& j) { j["version"] = "1"; }},
      {"unknown root field", [](Json& j) { j["cache"] = "discarded"; }},
      {"different Project", [](Json& j) { j["project"] = "another-project"; }},
      {"duplicate owner", [](Json& j) { j["parts"].push_back(j["parts"][0]); }},
      {"unknown owner field", [](Json& j) { j["parts"][0]["cache"] = 1; }},
      {"nested owner mismatch",
       [](Json& j) { j["parts"][0]["constraints"]["document"] = "another-part"; }},
      {"unknown nested field", [](Json& j) { j["parts"][0]["dimensions"]["cache"] = 1; }}};
  for (const auto& [label, mutate] : invalid) {
    INFO(label);
    auto malformed = Json::parse(original_bytes);
    mutate(malformed);
    {
      QFile file(QString::fromStdString(sidecar.string()));
      REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
      const auto payload = malformed.dump();
      REQUIRE(file.write(payload.data(), payload.size()) == static_cast<qint64>(payload.size()));
    }
    CHECK_FALSE(s.open_file(path.toStdString(), true));
    CHECK(snapshot(*s.project()) == original_project);
    CHECK(s.project_part_sketch_catalogs() == original_catalogs);
    CHECK_FALSE(s.dirty());
  }
}
