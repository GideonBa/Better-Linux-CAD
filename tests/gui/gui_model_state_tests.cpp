#include "../support/engineering_resource_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/step_import_source.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_physical_properties.hpp"
#include "blcad/gui/shell/shell_model_state_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace history = blcad::test::history_fixture;
namespace resources = blcad::test::engineering_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
ModelState state(std::string id = "wide", double width = 30) {
  ModelState result{ModelStateId(id), id, {}, {}, {}, {}, {}};
  result.parameters = {{ParameterId("width"), checked(Quantity::length_mm(width))}};
  return result;
}
PartDocument part() {
  auto result = history::native_base();
  resources::annotate(result);
  REQUIRE(result.replace_model_states({state(), state("same", 30)}));
  return result;
}
std::string text(const PartDocument& p) {
  return checked(serialize_part_document_to_json(p));
}
std::string bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll().toStdString();
}
double volume(const GuiDocumentSession& s) {
  const auto* shape = s.part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
QString seed(GuiDocumentSession& s, const QTemporaryDir& dir) {
  const auto path = dir.filePath("variants.blcad.json");
  REQUIRE(write_part_document_json_file(part(), path.toStdString()));
  REQUIRE(s.open_file(path.toStdString()));
  return path;
}
template <typename T> T& widget(ShellWindow& w, const char* id) {
  auto* result = w.findChild<T*>(QStringLiteral("blcad.model_states.") + id);
  REQUIRE(result);
  return *result;
}
void click(QPushButton& button) {
  for (auto* parent = button.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* area = qobject_cast<QScrollArea*>(parent))
      area->ensureWidgetVisible(&button);
  QApplication::processEvents();
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  QTest::mouseClick(&button, Qt::LeftButton);
}
int parameter_row(QTableWidget& table, const QString& name) {
  for (int row = 0; row < table.rowCount(); ++row)
    if (table.item(row, 1)->text() == name)
      return row;
  FAIL("Parameter row is missing");
  return -1;
}
} // namespace
TEST_CASE("Model-state activation is transient atomic and read-only while Save retains Primary",
          "[gui][model-state][parity-b190][parity-b191]") {
  QTemporaryDir dir;
  GuiDocumentSession session;
  const auto path = seed(session, dir);
  const auto primary = text(*session.part_document());
  const auto original = bytes(path);
  REQUIRE(session.activate_model_state(ModelStateId("wide")));
  CHECK(session.active_model_state() == ModelStateId("wide"));
  CHECK(session.part_document()->find_parameter(ParameterId("width"))->value().millimeters() == 20);
  CHECK(session.effective_part_document()
            ->find_parameter(ParameterId("width"))
            ->value()
            .millimeters() == 30);
  CHECK(volume(session) == Catch::Approx(2400));
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(
      *session.effective_part_document()));
  CHECK_FALSE(
      session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.can_undo());
  CHECK_FALSE(session.commit_part_transaction("Unexpected Primary edit", [](PartDocument& p) {
    auto changed = p.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(40)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  REQUIRE(session.save());
  CHECK(bytes(path) == original);
  CHECK(text(*session.part_document()) == primary);
  REQUIRE(session.task().begin("pending.task"));
  REQUIRE(session.task().begin_parameter_editing());
  CHECK_FALSE(session.activate_model_state(primary_model_state_id()));
  CHECK(session.active_model_state() == ModelStateId("wide"));
  REQUIRE(session.task().cancel());
  auto previous = *session.part_shape_cache();
  REQUIRE(session.activate_model_state(ModelStateId("same")));
  CHECK_FALSE(previous.validate_complete_geometry_result(*session.effective_part_document()));
  CHECK(volume(session) == Catch::Approx(2400));
  REQUIRE(session.activate_model_state(primary_model_state_id()));
  CHECK(volume(session) == Catch::Approx(1600));
  REQUIRE(session.open_file(path.toStdString()));
  CHECK(session.active_model_state() == primary_model_state_id());
}
TEST_CASE("State-table publication validates every member and preserves the active view atomically",
          "[gui][model-state][parity-b191]") {
  QTemporaryDir dir;
  GuiDocumentSession s;
  seed(s, dir);
  REQUIRE(s.activate_model_state(ModelStateId("wide")));
  auto states = s.part_document()->model_states();
  states.push_back(state("third", 40));
  REQUIRE(s.commit_model_state_table(s.part_document()->id(), states));
  CHECK(s.active_model_state() == ModelStateId("wide"));
  CHECK(volume(s) == Catch::Approx(2400));
  CHECK(s.undo_label() == "Manage Model States");
  const auto before = text(*s.part_document());
  states.erase(states.begin());
  CHECK_FALSE(s.commit_model_state_table(s.part_document()->id(), states));
  CHECK(text(*s.part_document()) == before);
  CHECK(volume(s) == Catch::Approx(2400));
  REQUIRE(s.undo());
  CHECK(s.part_document()->model_states().size() == 2);
  CHECK(s.active_model_state() == ModelStateId("wide"));
  REQUIRE(s.redo());
  CHECK(s.part_document()->model_states().size() == 3);
}
TEST_CASE(
    "Member export creates an independent selected Part and preserves source files and resources",
    "[gui][model-state][parity-b191]") {
  QTemporaryDir dir;
  GuiDocumentSession s;
  const auto path = seed(s, dir);
  REQUIRE(s.commit_part_sketch_intent_transaction(
      "Seed owned catalogs",
      [](PartDocument& part, std::vector<SketchConstraintCatalog>& constraints,
         std::vector<SketchDimensionCatalog>& dimensions) {
        constraints.push_back(
            checked(SketchConstraintCatalog::create(part.id(), SketchId("profile"))));
        dimensions.push_back(
            checked(SketchDimensionCatalog::create(part.id(), SketchId("profile"))));
        return Result<std::size_t>::success(2);
      }));
  REQUIRE(s.save());
  const auto source = bytes(path);
  const auto authored = text(*s.part_document());
  const auto output = dir.filePath("wide-member.blcad.json");
  REQUIRE(s.save_model_state_member(output.toStdString(), ModelStateId("wide"),
                                    DocumentId("new.member"), "Wide member"));
  GuiDocumentSession member;
  REQUIRE(member.open_file(output.toStdString()));
  CHECK(member.part_document()->id() == DocumentId("new.member"));
  CHECK(member.part_document()->model_states().empty());
  CHECK(member.part_document()->engineering_resources() ==
        s.part_document()->engineering_resources());
  REQUIRE(member.sketch_constraint_catalogs().size() == 1);
  REQUIRE(member.sketch_dimension_catalogs().size() == 1);
  CHECK(member.sketch_constraint_catalogs().front().document() == DocumentId("new.member"));
  CHECK(member.sketch_dimension_catalogs().front().document() == DocumentId("new.member"));
  CHECK(volume(member) == Catch::Approx(2400));
  CHECK(volume(s) == Catch::Approx(1600));
  CHECK(bytes(path) == source);
  CHECK(text(*s.part_document()) == authored);
  CHECK_FALSE(s.dirty());
  CHECK_FALSE(s.save_model_state_member(output.toStdString(), ModelStateId("same"),
                                        DocumentId("another"), "Another"));
  CHECK_FALSE(s.save_model_state_member(path.toStdString(), ModelStateId("wide"),
                                        DocumentId("another"), "Another"));
  CHECK_FALSE(s.save_model_state_member(dir.filePath("missing/member.blcad.json").toStdString(),
                                        ModelStateId("wide"), DocumentId("another"), "Another"));
  CHECK(bytes(path) == source);
  CHECK(text(*s.part_document()) == authored);
}
TEST_CASE("The owned state table retains invalid cells and commits corrected typed overrides once",
          "[gui][model-state][parity-b191]") {
  QTemporaryDir dir;
  ShellWindow window;
  const auto path = dir.filePath("table.blcad.json");
  REQUIRE(write_part_document_json_file(part(), path.toStdString()));
  REQUIRE(window.open_path(path));
  const auto before = text(*window.session().part_document());
  REQUIRE(window.model_state_tools().begin());
  REQUIRE(window.model_state_tools().create_state(ModelStateId("new"), "New"));
  auto& table = widget<QTableWidget>(window, "parameters");
  const auto row = parameter_row(table, "width");
  table.item(row, 0)->setCheckState(Qt::Checked);
  table.item(row, 3)->setText("invalid");
  CHECK_FALSE(window.model_state_tools().stage());
  CHECK(table.item(row, 3)->text() == "invalid");
  CHECK(text(*window.session().part_document()) == before);
  table.item(row, 3)->setText("45");
  REQUIRE(window.model_state_tools().stage());
  REQUIRE(window.model_state_tools().preview());
  CHECK(text(*window.session().part_document()) == before);
  REQUIRE(window.model_state_tools().apply());
  CHECK_FALSE(window.model_state_tools().active());
  CHECK(window.session().undo_label() == "Manage Model States");
  REQUIRE(window.model_state_tools().activate_state(ModelStateId("new")));
  CHECK(volume(window.session()) == Catch::Approx(3600));
  REQUIRE(window.model_state_tools().activate_state(primary_model_state_id()));
  REQUIRE(window.undo());
  CHECK(text(*window.session().part_document()) == before);
}
TEST_CASE("Native state-table interaction previews typed edits switches views and reopens Primary",
          "[gui][model-state-native][parity-b191]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Native Qt/OCCT state interaction requires xcb");
  QTemporaryDir dir;
  ShellWindow window;
  const auto path = dir.filePath("native-state.blcad.json");
  REQUIRE(write_part_document_json_file(part(), path.toStdString()));
  REQUIRE(window.open_path(path));
  window.show();
  window.raise();
  window.activateWindow();
  REQUIRE(QTest::qWaitForWindowActive(&window));
  REQUIRE(QTest::qWaitForWindowExposed(&window));
  window.model_state_tools().action().trigger();
  REQUIRE(window.model_state_tools().active());
  click(widget<QPushButton>(window, "create"));
  QApplication::processEvents();
  auto& table = widget<QTableWidget>(window, "parameters");
  const auto row = parameter_row(table, "width");
  const auto override_cell = table.visualItemRect(table.item(row, 0));
  QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(override_cell.left() + 10, override_cell.center().y()));
  REQUIRE(table.item(row, 0)->checkState() == Qt::Checked);
  table.setCurrentCell(row, 3);
  table.editItem(table.item(row, 3));
  QApplication::processEvents();
  auto* editor = QApplication::focusWidget();
  REQUIRE(editor);
  REQUIRE(table.isAncestorOf(editor));
  REQUIRE(editor->isVisible());
  QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClicks(editor, "40");
  QTest::keyClick(editor, Qt::Key_Return);
  click(widget<QPushButton>(window, "stage"));
  click(widget<QPushButton>(window, "preview"));
  REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
  const auto preview_capture = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!preview_capture.isEmpty()) {
    REQUIRE(QDir().mkpath(preview_capture));
    REQUIRE(window.grab().save(preview_capture + "/model-states-preview.png"));
  }
  click(widget<QPushButton>(window, "apply"));
  REQUIRE_FALSE(window.model_state_tools().active());
  const auto id = window.session().part_document()->model_states().back().id;
  REQUIRE(window.model_state_tools().activate_state(id));
  CHECK(window.windowTitle().contains(
      QString::fromStdString(window.session().part_document()->model_states().back().name)));
  CHECK(volume(window.session()) == Catch::Approx(3200));
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  CHECK(window.session().active_model_state() == primary_model_state_id());
  CHECK(volume(window.session()) == Catch::Approx(1600));
  const auto capture = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!capture.isEmpty()) {
    REQUIRE(QDir().mkpath(capture));
    REQUIRE(window.grab().save(capture + "/model-states-primary.png"));
  }
}

TEST_CASE("Physical Properties and standard STEP export consume the selected state view",
          "[gui][model-state][parity-b191]") {
  QTemporaryDir dir;
  GuiDocumentSession s;
  seed(s, dir);
  auto states = s.part_document()->model_states();
  states.front().materials = {{std::nullopt, resources::key("synthetic.material", "r2")}};
  REQUIRE(s.commit_model_state_table(s.part_document()->id(), states));
  REQUIRE(s.activate_model_state(ModelStateId("wide")));
  GuiPhysicalPropertiesController physical;
  REQUIRE(physical.begin(s));
  REQUIRE(physical.refresh(s));
  auto view = checked(physical.view(s));
  REQUIRE(view.known_mass);
  CHECK(view.volume == Catch::Approx(2400));
  CHECK(view.known_mass->mass == Catch::Approx(.0144));
  const auto output = dir.filePath("selected.step");
  REQUIRE(GuiAnalysisExportWorkbench{}.export_step(
      s, {GuiStepExportMode::PartMultiBody, output.toStdString()}));
  const auto report = checked(inspect_step_import_asset(dir.path().toStdString(), "selected.step"));
  REQUIRE(report.observed_sha256_digest());
  const auto source = checked(
      StepImportSource::create(StepImportSourceId("export"), "Selected", "selected.step",
                               *report.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported =
      checked(geometry::StepImportReader{}.read(source, dir.path().toStdString()));
  double volume = 0;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      volume += body.volume_mm3;
  CHECK(volume == Catch::Approx(2400));
  REQUIRE(s.activate_model_state(ModelStateId("same")));
  CHECK_FALSE(physical.view(s));
  REQUIRE(physical.refresh(s));
  CHECK(checked(physical.view(s)).known_mass->mass == Catch::Approx(.0096));
  CHECK(s.part_document()->default_material() == resources::key());
}

TEST_CASE(
    "Root Assembly state views preserve repeated Part selections and separate identical contexts",
    "[gui][model-state][parity-b190][parity-b191]") {
  QTemporaryDir dir;
  auto primary_part = part();
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(primary_part.id()));
  REQUIRE(root.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("first"), "First", primary_part.id()))));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("second"), "Second", primary_part.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {{100, 0, 0}, {}},
      ModelStateId("wide")))));
  ModelState first{ModelStateId("first-view"), "First view", {}, {}, {}, {}, {}};
  first.components = {{ComponentInstanceId("first"), true, {}}};
  auto second = first;
  second.id = ModelStateId("second-view");
  second.name = "Second view";
  REQUIRE(root.replace_model_states({first, second}));
  auto project = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(project.add_part_document(primary_part));
  const auto path = dir.filePath("states.blcad.project.json");
  REQUIRE(write_project_json_file(project, path.toStdString()));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path.toStdString()));
  const auto before = checked(serialize_project_to_json(*s.project()));
  REQUIRE(s.activate_model_state(first.id));
  CHECK(s.project()
            ->assembly()
            .find_component_instance(ComponentInstanceId("first"))
            ->suppression_state() == ComponentSuppressionState::Active);
  CHECK(s.effective_project()->assembly().effective_model_state() == first.id);
  auto physical =
      checked(geometry::BodyResultInspector{}.inspect_physical_properties(*s.effective_project()));
  CHECK(physical.data().occurrences.size() == 1);
  CHECK(physical.data().volume_mm3 == Catch::Approx(2400));
  REQUIRE(s.activate_model_state(second.id));
  CHECK_FALSE(physical.validate_current(*s.effective_project()));
  CHECK(checked(serialize_project_to_json(*s.project())) == before);
  CHECK_FALSE(s.dirty());
  const auto output = dir.filePath("root-state.step");
  REQUIRE(GuiAnalysisExportWorkbench{}.export_step(
      s, {GuiStepExportMode::AssemblyStructured, output.toStdString()}));
  REQUIRE(s.save());
  CHECK(bytes(path) == before);
}

namespace {
Project history_scope_project(PartDocument member, bool representations) {
  resources::annotate(member);
  auto root = checked(AssemblyDocument::create(DocumentId("scope.root"), "Scope root"));
  REQUIRE(root.add_member_part(member.id()));
  for (const char* id : {"first", "second"})
    REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId(id), id, member.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Grounded,
        {{std::string(id) == "first" ? 0. : 100., 0, 0}, {}}))));
  auto project = checked(Project::create(DocumentId("scope.project"), "Scope Project", root));
  REQUIRE(project.add_part_document(member));
  REQUIRE(project.synchronize_engineering_resources());
  if (representations) {
    AssemblyPosition position{AssemblyPositionId("moved"), "Moved", {}, {}, {}, {}, {}};
    position.components = {{ComponentInstanceId("first"), member.id(), {{0, 0, 20}, {}}},
                           {ComponentInstanceId("second"), member.id(), {{120, 0, 0}, {}}}};
    REQUIRE(project.replace_assembly_positions({position}));
    AssemblyDesignView view{AssemblyDesignViewId("second"), "Second only", {}, {}};
    view.components = {{{{}, ComponentInstanceId("first"), member.id()}, false},
                       {{{}, ComponentInstanceId("second"), member.id()}, true}};
    REQUIRE(project.replace_assembly_design_views({view}));
  }
  return project;
}
ModelState history_scope_state(bool machined = false) {
  ModelState result{ModelStateId("manufacturing"),
                    "Manufacturing",
                    {},
                    {},
                    {},
                    {{ComponentInstanceId("first"), true, {}}},
                    {}};
  if (machined)
    result.components.push_back({ComponentInstanceId("second"), {}, ModelStateId("machined")});
  return result;
}

auto history_scope_scene(const GuiDocumentSession& s) {
  return checked(geometry::ViewportSceneBuilder{}.build_project_for_states(
      *s.effective_project(),
      [&](DocumentId id, ModelStateId state) { return s.project_part_shape_cache(id, state); }));
}
void history_scope_poses(const GuiDocumentSession& s, bool moved, std::size_t visible) {
  auto scene = history_scope_scene(s);
  REQUIRE(scene.size() == visible);
  const auto second = std::find_if(scene.begin(), scene.end(), [](const auto& item) {
    return item.semantic_id.ends_with("/second");
  });
  REQUIRE(second != scene.end());
  CHECK(blcad::test::classify_solid_point(second->shape, {moved ? 125. : 105., 2, 4}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(second->shape, {moved ? 105. : 125., 2, 4}) ==
        blcad::test::SolidPointLocation::Outside);
}
} // namespace

TEST_CASE(
    "Undo composes surviving representations after the active manufacturing state disappears",
    "[gui][model-state][assembly-representation-session][history-scope-fallback][parity-b198]") {
  const bool retained_representations = GENERATE(false, true);
  CAPTURE(retained_representations);
  QTemporaryDir directory;
  auto baseline = history_scope_project(history::native_base(), retained_representations);
  const auto initial = checked(serialize_project_to_json(baseline));
  const auto path = directory.filePath("scope.blcad.project.json");
  REQUIRE(write_project_json_file(baseline, path.toStdString()));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path.toStdString()));
  const auto added = s.commit_project_transaction("Add manufacturing scope", [&](Project& p) {
    if (!retained_representations) {
      const auto all = history_scope_project(history::native_base(), true);
      auto positions = p.replace_assembly_positions(all.assembly_positions());
      if (!positions)
        return positions;
      auto views = p.replace_assembly_design_views(all.assembly_design_views());
      if (!views)
        return views;
    }
    return p.assembly().replace_model_states({history_scope_state()});
  });
  INFO((added ? std::string{} : added.error().message()));
  REQUIRE(added);
  const auto edited = checked(serialize_project_to_json(*s.project()));
  REQUIRE(s.activate_model_state(ModelStateId("manufacturing")));
  REQUIRE(s.activate_assembly_position(AssemblyPositionId("moved")));
  REQUIRE(s.activate_assembly_design_view(AssemblyDesignViewId("second")));
  history_scope_poses(s, true, 1);
  const auto selected =
      checked(geometry::BodyResultInspector{}.inspect_physical_properties(*s.effective_project()));
  CHECK(selected.data().occurrences.size() == 1);
  REQUIRE(s.undo());
  CHECK(checked(serialize_project_to_json(*s.project())) == initial);
  CHECK(s.active_model_state() == primary_model_state_id());
  CHECK(s.active_position() ==
        (retained_representations ? AssemblyPositionId("moved") : master_position_id()));
  CHECK(s.active_design_view() ==
        (retained_representations ? AssemblyDesignViewId("second") : default_design_view_id()));
  CHECK(s.derived_results_fresh());
  CHECK_FALSE(selected.validate_current(*s.effective_project()));
  const auto physical =
      checked(geometry::BodyResultInspector{}.inspect_physical_properties(*s.effective_project()));
  CHECK(physical.data().occurrences.size() == 2);
  CHECK(physical.data().volume_mm3 == Catch::Approx(3200));
  REQUIRE(physical.data().volume_centroid_mm);
  CHECK(physical.data().volume_centroid_mm->z == Catch::Approx(retained_representations ? 14 : 4));
  REQUIRE(physical.data().complete_mass());
  CHECK(physical.data().complete_mass()->mass_kg == Catch::Approx(.0128));
  history_scope_poses(s, retained_representations, retained_representations ? 1 : 2);
  REQUIRE(s.redo());
  CHECK(checked(serialize_project_to_json(*s.project())) == edited);
  CHECK(s.active_model_state() == primary_model_state_id());
  CHECK(s.active_position() ==
        (retained_representations ? AssemblyPositionId("moved") : master_position_id()));
  CHECK(s.active_design_view() ==
        (retained_representations ? AssemblyDesignViewId("second") : default_design_view_id()));
  history_scope_poses(s, retained_representations, retained_representations ? 1 : 2);
  REQUIRE(s.activate_model_state(ModelStateId("manufacturing")));
  REQUIRE(s.activate_assembly_position(AssemblyPositionId("moved")));
  REQUIRE(s.activate_assembly_design_view(AssemblyDesignViewId("second")));
  history_scope_poses(s, true, 1);
  CHECK(checked(serialize_project_to_json(*s.project())) == edited);
}

TEST_CASE(
    "A failed selected-state Undo rebuild retains the accepted scope and certified caches",
    "[gui][model-state][assembly-representation-session][history-scope-fallback][parity-b198]") {
  auto member = history::native_base();
  REQUIRE(member.add_parameter(checked(Parameter::create_length(
      ParameterId("bore.diameter"), "Bore diameter", checked(Quantity::length_mm(30))))));
  auto sketch = checked(Sketch::create(SketchId("bore.sketch"), "Bore", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("circle"), ParameterId("bore.diameter")))));
  REQUIRE(member.add_sketch(sketch));
  auto cut = checked(Feature::create_subtractive_extrude(FeatureId("bore"), "Bore", sketch.id(),
                                                         FeatureId("base")));
  REQUIRE(member.add_feature(checked(cut.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.base"), {}))))));
  PartFeatureHistoryIntent suppressed;
  for (const auto& entry : checked(member.executable_history()))
    suppressed.order.push_back(entry.node_id);
  suppressed.suppressed = {"bore"};
  REQUIRE(member.apply_history_intent(suppressed));
  REQUIRE(member.replace_model_states(
      {{ModelStateId("machined"), "Machined", {}, {{"bore", false}}, {}, {}, {}}}));
  auto baseline = history_scope_project(member, true);
  REQUIRE(baseline.assembly().replace_model_states({history_scope_state(true)}));
  QTemporaryDir directory;
  const auto path = directory.filePath("failure.blcad.project.json");
  REQUIRE(write_project_json_file(baseline, path.toStdString()));
  GuiDocumentSession s;
  REQUIRE(s.open_file(path.toStdString()));
  REQUIRE(s.commit_project_transaction("Make bore feasible", [&](Project& p) {
    auto changed =
        p.find_part_document(member.id())
            ->set_parameter_value(ParameterId("bore.diameter"), checked(Quantity::length_mm(2)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  REQUIRE(s.activate_model_state(ModelStateId("manufacturing")));
  REQUIRE(s.activate_assembly_position(AssemblyPositionId("moved")));
  REQUIRE(s.activate_assembly_design_view(AssemblyDesignViewId("second")));
  const auto before = checked(serialize_project_to_json(*s.project()));
  const auto effective = checked(serialize_project_to_json(*s.effective_project()));
  const auto revision = s.model_revision();
  const auto properties =
      checked(geometry::BodyResultInspector{}.inspect_physical_properties(*s.effective_project()));
  CHECK(properties.data().volume_mm3 == Catch::Approx(1600 - 8 * std::numbers::pi).margin(1e-6));
  const auto rejected = s.undo();
  INFO((rejected ? "unexpected success" : rejected.error().message()));
  REQUIRE_FALSE(rejected);
  CHECK(checked(serialize_project_to_json(*s.project())) == before);
  CHECK(checked(serialize_project_to_json(*s.effective_project())) == effective);
  CHECK(s.active_model_state() == ModelStateId("manufacturing"));
  CHECK(s.active_position() == AssemblyPositionId("moved"));
  CHECK(s.active_design_view() == AssemblyDesignViewId("second"));
  CHECK(s.model_revision() == revision);
  CHECK(s.undo_label() == "Make bore feasible");
  CHECK(s.derived_results_fresh());
  CHECK(properties.validate_current(*s.effective_project()));
  history_scope_poses(s, true, 1);
}
