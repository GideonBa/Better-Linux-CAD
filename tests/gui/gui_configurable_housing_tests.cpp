#include "../support/adaptive_project_fixture.hpp"
#include "../support/assembly_hole_fixture.hpp"
#include "../support/direct_edit_integrated_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_hole_coordinator.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/shell/shell_model_state_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
using namespace blcad::geometry;
namespace h = blcad::test::history_fixture;
namespace a = blcad::test::assembly_hole;
namespace d = blcad::test::direct_integrated;
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
QPoint reveal_table_cell(QTableWidget& table, int row, int column, bool checkbox = false) {
  auto* cell = table.item(row, column);
  REQUIRE(cell);
  table.scrollToItem(cell, QAbstractItemView::PositionAtCenter);
  QApplication::processEvents();
  for (auto* parent = table.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* area = qobject_cast<QScrollArea*>(parent)) {
      const auto center = table.visualItemRect(cell).center();
      const auto target = table.viewport()->mapTo(area->widget(), center);
      area->ensureVisible(target.x(), target.y(), 20, 20);
    }
  QApplication::processEvents();
  const auto rectangle = table.visualItemRect(cell);
  const auto point =
      checkbox ? QPoint(rectangle.left() + 10, rectangle.center().y()) : rectangle.center();
  REQUIRE(table.viewport()->rect().contains(point));
  REQUIRE(table.viewport()->visibleRegion().contains(point));
  REQUIRE(table.screen());
  REQUIRE(table.screen()->availableGeometry().contains(table.viewport()->mapToGlobal(point)));
  return point;
}
std::string file_bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll().toStdString();
}
void write_bytes(const QString& path, const std::string& data) {
  std::ofstream file(path.toStdString(), std::ios::binary | std::ios::trunc);
  file << data;
  REQUIRE(file.good());
}
void click(QPushButton& button) {
  for (auto* parent = button.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* area = qobject_cast<QScrollArea*>(parent))
      area->ensureWidgetVisible(&button);
  QApplication::processEvents();
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  QTest::mouseClick(&button, Qt::LeftButton);
  QApplication::processEvents();
}
template <class T> Result<std::size_t> changed_count(Result<T> result) {
  return result ? Result<std::size_t>::success(1) : Result<std::size_t>::failure(result.error());
}
std::string saved(const Project& p) {
  return h::checked(serialize_project_to_json(p));
}
void supplier(const QString& path, bool lost_face = false) {
  auto shape = h::checked(RectangleExtrusionAdapter{}.make_extruded_rectangle(
      h::checked(Quantity::length_mm(20)), h::checked(Quantity::length_mm(10)),
      h::checked(Quantity::length_mm(lost_face ? 7 : 8))));
  if (lost_face)
    shape = h::checked(BodyTransformAdapter{}.translate(shape, {0, 0, 1}));
  REQUIRE(StepExporter{}.write_step(shape, path.toStdString()));
}
PartDocument imported(QTemporaryDir& dir) {
  supplier(dir.filePath("supplier.step"));
  auto source = h::checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step",
      h::checked(step_import_sha256(file_bytes(dir.filePath("supplier.step")))),
      StepImportMode::EditableBody));
  auto read = h::checked(StepImportReader{}.read(source, dir.path().toStdString()));
  std::size_t index{};
  while (index < read.products.size() && read.products[index].bodies.empty())
    ++index;
  REQUIRE(index < read.products.size());
  auto catalog = h::checked(StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("supplier.catalog"), read, index));
  return h::checked(StepEditableCommands{}.create_definition(
      source, catalog, DocumentId("housing"), "Imported configurable housing",
      FeatureId("imported.base"), dir.path().toStdString()));
}
ModelState tall(double depth = 4) {
  return {ModelStateId("tall"),
          "Tall / dense",
          {{ParameterId("boss.depth"), h::checked(Quantity::length_mm(depth))}},
          {},
          {{std::nullopt, resources::key("synthetic.material", "r2")}},
          {},
          {}};
}
// D02 carrier formula: the mounting Hole and feature Pattern precede the
// downstream Body Pattern, so each of its three solids inherits the two bores.
// The thin underside remains a separate unbored physical Body.
double housing_volume(double boss_depth, double direct = 1) {
  const double pi = std::numbers::pi;
  const double one = 200 * (8 + direct) + 4 * boss_depth + 92 + 1 -
                     pi * .01 * (8 + direct + boss_depth) - pi * .01 - (2. / 7) * .25 * 7.5 -
                     (1 - pi / 4) * 20 * (.04 + .06 + .09) / 3;
  return 3 * (one - 2 * pi * .8 * .8 * 4) + (200 - pi * .01) * .5;
}
double cover_volume(double thickness = 2) {
  return (std::numbers::pi * 64 - 2 * std::numbers::pi * 1.2 * 1.2) * thickness;
}
HoleFeature rebound(const HoleFeature& source, BodyId body, std::vector<HolePlacement> points) {
  return h::checked(HoleFeature::create(source.id(), source.name(), source.kind(), body,
                                        source.support(), std::move(points), source.diameter(),
                                        source.extent(), source.direction(), source.tip(),
                                        source.manufacturing(), source.thread()));
}
Project fixture(QTemporaryDir& dir) {
  StepImportTopologyContext imports(dir.path().toStdString());
  auto base = d::build(imported(dir), &imports);
  const auto body = base.imported_body_features().front().definition().outputs().front().body;
  h::accept(base.add_engineering_resource(a::table()));
  h::accept(base.add_construction_plane(h::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("hole.plane"), "Mounting entry",
                                         {0, 0, 8}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  a::length(base, "blind.depth", 4);
  a::length(base, "thread.length", 3);
  a::length(base, "layout.spacing", 6);
  h::accept(base.add_parameter(h::checked(Parameter::create_count(
      ParameterId("layout.count"), "Layout count", h::checked(Quantity::count(2))))));
  const auto threaded = rebound(a::threaded(), body, {{HolePlacementId("seed"), Point2{-5, -3}}});
  h::accept(base.add_hole_feature(threaded));
  h::accept(base.add_datum_axis(h::checked(
      DatumAxis::create_explicit(DatumAxisId("layout.axis"), "Housing Y", {}, {0, 1, 0}))));
  h::accept(base.add_linear_pattern_feature(h::checked(LinearPatternFeature::create(
      FeatureId("layout.pattern"), "Housing mounting pattern",
      {h::checked(PatternSourceReference::feature(FeatureId("mounting.hole")))},
      h::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                  DatumAxisId("layout.axis"))),
      ParameterId("layout.count"), LinearPatternExtentMode::Spacing, ParameterId("layout.spacing"),
      PatternDirectionSign::Positive,
      h::checked(
          FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, body, std::nullopt)),
      PatternPlacementOptions{}))));
  // The current Body alias makes late machining a dependency of the Body
  // Pattern; verify that contract before using the inherited-cut hand oracle.
  const auto order = h::checked(base.executable_history());
  const auto position = [&](const char* id) {
    return std::find_if(order.begin(), order.end(),
                        [&](const auto& entry) { return entry.node_id == id; });
  };
  REQUIRE(position("mounting.hole") != order.end());
  REQUIRE(position("layout.pattern") != order.end());
  REQUIRE(position("pattern") != order.end());
  REQUIRE(position("pattern.place") != order.end());
  CHECK(position("mounting.hole") < position("layout.pattern"));
  CHECK(position("layout.pattern") < position("pattern"));
  CHECK(position("pattern") < position("pattern.place"));
  h::accept(base.replace_model_states({tall()}));
  auto cover = blcad::test::adaptive_plate("cover.part", true);
  (void)h::checked(
      cover.set_parameter_value(ParameterId("length"), h::checked(Quantity::length_mm(2))));
  a::length(cover, "cover.diameter", 16);
  h::accept(cover.add_engineering_resource(a::table()));
  h::accept(cover.add_engineering_resource(resources::material()));
  h::accept(cover.set_default_material(resources::key()));
  h::accept(cover.add_construction_plane(h::checked(ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("hole.plane"), "Cover entry",
      h::checked(ConstructionRelation::create_plane_offset_from_plane(
          ConstructionRelationId("cover.entry"), DatumPlaneId("linked.plane"),
          ParameterId("length")))))));
  auto root = h::checked(AssemblyDocument::create(DocumentId("root"), "Housing root"));
  auto child = h::checked(AssemblyDocument::create(DocumentId("pair"), "Cover and housing"));
  h::accept(child.add_member_part(base.id()));
  h::accept(child.add_member_part(cover.id()));
  h::accept(child.add_component_instance(h::checked(
      ComponentInstance::create(ComponentInstanceId("base"), "Imported housing", base.id(),
                                ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                ComponentGroundingState::Grounded, {}, ModelStateId("tall")))));
  h::accept(child.add_component_instance(h::checked(
      ComponentInstance::create(ComponentInstanceId("cover"), "Adaptive cover", cover.id(),
                                ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                ComponentGroundingState::Grounded, {{2, 0, 0}, {0, 0, 90}}))));
  h::accept(root.add_subassembly_instance(h::checked(SubassemblyInstance::create(
      SubassemblyInstanceId("pair"), "Nonidentity parent", child.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, a::parent_pose()))));
  auto p =
      h::checked(Project::create(DocumentId("configurable.housing"), "Configurable housing", root));
  h::accept(p.add_child_assembly_document(child));
  h::accept(p.add_part_document(base));
  h::accept(p.add_part_document(cover));
  h::accept(p.synchronize_engineering_resources());
  const auto source = h::checked(make_hole_target_spelling(h::checked(HoleInstanceReference::create(
      FeatureId("boss.hole"), HolePlacementId("center"), HoleReferenceRole::Entry))));
  auto link = h::checked(ProjectDesignReference::create(
      ProjectDesignReferenceId("cover.support"), "Actual boss Hole entry",
      a::occurrence("base", "housing"), ModelStateId("tall"), ProjectDesignSemanticSource{source},
      a::occurrence("cover", "cover.part"), ConstructionPlaneId("linked.plane")));
  p = h::checked(AdaptiveProjectEvaluator(&imports).bind(p, link)).project;
  auto* top = p.find_part_document(DocumentId("cover.part"));
  const auto plane = h::checked(WorkplaneResolver{}.resolve(*top, DatumPlaneId("linked.plane")));
  CHECK(plane.origin.x == Catch::Approx(-2).margin(1e-7));
  CHECK(plane.origin.y == Catch::Approx(-3).margin(1e-7));
  CHECK(plane.origin.z == Catch::Approx(12).margin(1e-7));
  // Explicit inverse of cover-local -> base-local: (2-y, x, z).
  const auto local = [&](Point3 base_point) {
    const Point3 q{base_point.y, 2 - base_point.x, base_point.z};
    const Vector3 delta{q.x - plane.origin.x, q.y - plane.origin.y, q.z - plane.origin.z};
    return Point2{dot(delta, plane.x_axis), dot(delta, plane.y_axis)};
  };
  auto sketch = h::checked(
      Sketch::create(SketchId("profile"), "Cover source Sketch", DatumPlaneId("linked.plane")));
  h::accept(sketch.add_profile(h::checked(CircleProfile::create(
      ProfileId("rectangle"), ParameterId("cover.diameter"), local({0, 0, 12})))));
  h::accept(top->update_sketch(sketch));
  const auto clearance = a::clearance();
  const auto coordinated = h::checked(AssemblyHoleFeature::create(
      FeatureId("assembly.mounting"), "Housing-owned mounting layout and adaptive cover",
      {a::occurrence("base", "housing"), ModelStateId("tall"), FeatureId("mounting.hole"),
       FeatureId("layout.pattern")},
      {{"base", a::occurrence("base", "housing"), ModelStateId("tall"),
        AssemblyHoleParticipantRole::Threaded, AssemblyHoleDefinitionPolicy::SingleOccurrence,
        threaded},
       {"cover", a::occurrence("cover", "cover.part"), primary_model_state_id(),
        AssemblyHoleParticipantRole::Clearance, AssemblyHoleDefinitionPolicy::SingleOccurrence,
        clearance}}));
  auto result = h::checked(AssemblyHoleCoordinator(&imports).bind(p, coordinated)).project;
  auto selected = h::checked(
      result.find_part_document(DocumentId("housing"))->evaluate_model_state(ModelStateId("tall")));
  auto proof = h::checked(ShapeCache::create(ShapeCacheId("housing.inherited.bores")));
  REQUIRE(GeometryRecomputeExecutor(&imports).execute_document(selected, proof));
  const auto* machined = proof.find_body_shape(body);
  const auto* copies = proof.find_body_shape(BodyId("body.pattern"));
  REQUIRE(machined);
  REQUIRE(copies);
  for (double y : {-3., 3.}) {
    CHECK(blcad::test::classify_solid_point(*machined, {-5, y, 6}) ==
          blcad::test::SolidPointLocation::Outside);
    CHECK(blcad::test::classify_solid_point(*machined, {-3, y, 6}) ==
          blcad::test::SolidPointLocation::Inside);
    for (double x : {-5., 35.}) {
      CHECK(blcad::test::classify_solid_point(*copies, {x, y, 26}) ==
            blcad::test::SolidPointLocation::Outside);
      CHECK(blcad::test::classify_solid_point(*copies, {x + 2, y, 26}) ==
            blcad::test::SolidPointLocation::Inside);
    }
  }
  return result;
}
void physical(const GuiDocumentSession& s, const QTemporaryDir& dir, double boss = 4,
              double thickness = 2) {
  StepImportTopologyContext imports(dir.path().toStdString());
  const auto result = h::checked(
      BodyResultInspector{}.inspect_physical_properties(*s.effective_project(), &imports));
  REQUIRE(result.data().complete_mass());
  const auto primary = h::checked(BodyResultInspector{}.inspect_physical_properties(
      *s.project()->find_part_document(DocumentId("housing")), &imports));
  CHECK(primary.data().volume_mm3 == Catch::Approx(housing_volume(3)).margin(2e-3));
  REQUIRE(primary.data().complete_mass());
  CHECK(primary.data().complete_mass()->mass_kg ==
        Catch::Approx(housing_volume(3) * 4e-6).margin(2e-8));
  CHECK(result.data().volume_mm3 ==
        Catch::Approx(housing_volume(boss) + cover_volume(thickness)).margin(2e-3));
  CHECK(result.data().complete_mass()->mass_kg ==
        Catch::Approx(housing_volume(boss) * 6e-6 + cover_volume(thickness) * 4e-6).margin(2e-8));
}
} // namespace

TEST_CASE("Configurable imported housing connects advanced edits states "
          "adaptive cover and coordinated machining",
          "[gui][configurable-housing][parity-b198][X02]") {
  QTemporaryDir dir;
  auto p = fixture(dir);
  const auto immutable_bases =
      p.find_part_document(DocumentId("housing"))->imported_body_features();
  const auto original_source = file_bytes(dir.filePath("supplier.step"));
  const auto path = dir.filePath("housing.blcad.project.json");
  REQUIRE(write_project_json_file(p, path.toStdString()));
  ShellWindow w;
  REQUIRE(w.open_path(path));
  const bool native = QApplication::platformName() == "xcb";
  if (native) {
    show_native_window(w);
  }
  physical(w.session(), dir);
  const auto initial = saved(*w.session().project());
  if (native) {
    auto& tools = w.model_state_tools();
    REQUIRE(tools.begin(DocumentId("housing")));
    REQUIRE(tools.select_state(ModelStateId("tall")));
    QApplication::processEvents();
    auto* table = w.findChild<QTableWidget*>("blcad.model_states.parameters");
    REQUIRE(table);
    int row = -1;
    for (int i = 0; i < table->rowCount(); ++i)
      if (table->item(i, 1)->data(Qt::UserRole).toString() == "boss.depth" ||
          table->item(i, 1)->text() == "boss.depth") {
        row = i;
        break;
      }
    REQUIRE(row >= 0);
    (void)reveal_table_cell(*table, row, 3);
    table->setCurrentCell(row, 3);
    table->editItem(table->item(row, 3));
    QApplication::processEvents();
    auto* editor = QApplication::focusWidget();
    REQUIRE(editor);
    REQUIRE(table->isAncestorOf(editor));
    REQUIRE(editor->isVisible());
    REQUIRE(editor->visibleRegion().contains(editor->rect().center()));
    REQUIRE(editor->screen()->availableGeometry().contains(
        editor->mapToGlobal(editor->rect().center())));
    QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(editor, "4.5");
    QTest::keyClick(editor, Qt::Key_Return);
    for (const auto* id : {"stage", "preview", "apply"}) {
      auto* button = w.findChild<QPushButton*>(QString("blcad.model_states.") + id);
      REQUIRE(button);
      click(*button);
    }
    REQUIRE_FALSE(tools.active());
  } else
    REQUIRE(w.session().commit_model_state_table(DocumentId("housing"), {tall(4.5)}));
  physical(w.session(), dir, 4.5);
  const auto* top = w.session().project()->find_part_document(DocumentId("cover.part"));
  CHECK(top->find_construction_plane(ConstructionPlaneId("linked.plane"))->origin().z ==
        Catch::Approx(12.5).margin(1e-7));
  CHECK(w.session()
            .project()
            ->find_part_document(DocumentId("housing"))
            ->find_parameter(ParameterId("boss.depth"))
            ->value()
            .millimeters() == 3);
  REQUIRE(w.undo());
  CHECK(saved(*w.session().project()) == initial);
  REQUIRE(w.redo());
  REQUIRE(w.session().commit_project_transaction("Move shared mounting layout", [](Project& q) {
    return changed_count(q.find_part_document(DocumentId("housing"))
                             ->set_parameter_value(ParameterId("layout.spacing"),
                                                   h::checked(Quantity::length_mm(5))));
  }));
  const auto& cover = *w.session().project()->find_part_document(DocumentId("cover.part"));
  const auto* target = cover.find_hole_feature(FeatureId("layout.hole"));
  REQUIRE(target->placements().size() == 2);
  auto cover_cache = h::checked(ShapeCache::create(ShapeCacheId("housing.cover.oracles")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(cover, cover_cache));
  for (std::size_t i = 0; i < 2; ++i) {
    const auto frame = h::checked(HoleInstanceGeometry::resolve(
        cover,
        h::checked(HoleInstanceReference::create(target->id(), target->placements()[i].id,
                                                 HoleReferenceRole::Axis)),
        cover_cache));
    // Independently undo the known cover-local Rz90 plus x2 placement.
    CHECK(2 - frame.frame.origin.y == Catch::Approx(-5).margin(1e-7));
    CHECK(frame.frame.origin.x == Catch::Approx(i ? 2 : -3).margin(1e-7));
    CHECK(frame.radius_mm == Catch::Approx(1.2).margin(1e-7));
    CHECK(std::abs(frame.frame.normal.z) == Catch::Approx(1).margin(1e-7));
  }
  const auto accepted = saved(*w.session().project());
  const auto failed =
      w.session().commit_project_transaction("Rejected partial housing edit", [](Project& q) {
        auto changed = q.find_part_document(DocumentId("housing"))
                           ->set_parameter_value(ParameterId("layout.spacing"),
                                                 h::checked(Quantity::length_mm(5.5)));
        if (!changed)
          return changed_count(std::move(changed));
        return changed_count(q.find_part_document(DocumentId("cover.part"))
                                 ->set_parameter_value(ParameterId("cover.diameter"),
                                                       h::checked(Quantity::length_mm(2))));
      });
  CHECK_FALSE(failed);
  CHECK(saved(*w.session().project()) == accepted);
  physical(w.session(), dir, 4.5);
  const auto& base = *w.session().project()->find_part_document(DocumentId("housing"));
  const auto body = base.imported_body_features().front().definition().outputs().front().body;
  const auto recipe = a::threaded();
  const auto reverse_recipe = h::checked(HoleFeature::create(
      FeatureId("reverse.hole"), "Reverse", recipe.kind(), body, recipe.support(),
      recipe.placements(), recipe.diameter(), recipe.extent(), recipe.direction(), recipe.tip(),
      recipe.manufacturing(), recipe.thread()));
  const auto reverse = h::checked(AssemblyHoleFeature::create(
      FeatureId("reverse.feedback"), "Rejected cover-to-housing feedback",
      {a::occurrence("cover", "cover.part"),
       primary_model_state_id(),
       FeatureId("layout.hole"),
       {}},
      {{"base", a::occurrence("base", "housing"), ModelStateId("tall"),
        AssemblyHoleParticipantRole::Threaded, AssemblyHoleDefinitionPolicy::SingleOccurrence,
        reverse_recipe}}));
  const auto cycle = w.session().commit_project_transaction(
      "Reject reverse manufacturing driver", [&](Project& q) {
        auto features = q.assembly_hole_features();
        features.push_back(reverse);
        return q.replace_assembly_hole_features(std::move(features));
      });
  REQUIRE_FALSE(cycle);
  CHECK(cycle.error().message().find("cycle") != std::string::npos);
  CHECK(saved(*w.session().project()) == accepted);
  const auto compatible = original_source + "\n/* compatible X02 supplier revision */\n";
  write_bytes(dir.filePath("supplier.step"), compatible);
  auto refreshed = h::checked(
      StepEditableCommands{}.refresh(*w.session().project(), DocumentId("housing"),
                                     StepImportSourceId("supplier"), dir.path().toStdString()));
  REQUIRE(w.session().commit_project_transaction("Refresh housing source", [&](Project& q) {
    q = refreshed.project;
    return Result<std::size_t>::success(1);
  }));
  const auto repaired = saved(*w.session().project());
  supplier(dir.filePath("supplier.step"), true);
  CHECK_FALSE(StepEditableCommands{}.refresh(*w.session().project(), DocumentId("housing"),
                                             StepImportSourceId("supplier"),
                                             dir.path().toStdString()));
  CHECK(saved(*w.session().project()) == repaired);
  write_bytes(dir.filePath("supplier.step"), compatible);
  CHECK(
      w.session().project()->find_part_document(DocumentId("housing"))->imported_body_features() ==
      immutable_bases);
  REQUIRE(w.enter_project_part_context(a::occurrence("cover", "cover.part")));
  if (native) {
    QTreeWidgetItem* sketch{};
    for (QTreeWidgetItemIterator it(&w.model_browser()); *it; ++it)
      if ((*it)->text(0) == "Cover source Sketch") {
        sketch = *it;
        break;
      }
    REQUIRE(sketch);
    for (auto* parent = sketch->parent(); parent; parent = parent->parent())
      parent->setExpanded(true);
    w.model_browser().scrollToItem(sketch);
    QApplication::processEvents();
    const auto source_sketch_point = w.model_browser().visualItemRect(sketch).center();
    QTest::mouseClick(w.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                      source_sketch_point);
    QTest::mouseDClick(w.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                       source_sketch_point);
    QTest::mouseRelease(w.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                        source_sketch_point);
    REQUIRE(w.active_sketch() == SketchId("profile"));
    REQUIRE(w.finish_active_sketch());
  }
  REQUIRE(w.session().commit_part_transaction("Thicken adaptive cover", [](PartDocument& q) {
    return changed_count(
        q.set_parameter_value(ParameterId("length"), h::checked(Quantity::length_mm(2.5))));
  }));
  if (native) {
    auto* button = w.findChild<QPushButton*>("blcad.advanced_assembly.finish_part");
    REQUIRE(button);
    click(*button);
  } else
    REQUIRE(w.finish_project_part_context(true));
  REQUIRE_FALSE(w.session().project_part_context_active());
  CHECK(w.session().undo_label() == "Edit Part in Assembly");
  physical(w.session(), dir, 4.5, 2.5);
  REQUIRE(w.save_to({}));
  const auto complete = saved(*w.session().project());
  REQUIRE(w.open_path(path));
  CHECK(saved(*w.session().project()) == complete);
  physical(w.session(), dir, 4.5, 2.5);
  StepImportTopologyContext final_context(dir.path().toStdString());
  REQUIRE(AssemblyStructuredStepExporter(&final_context)
              .write_step(*w.session().effective_project(),
                          dir.filePath("housing.step").toStdString()));
  const auto exported = h::checked(StepImportSource::create(
      StepImportSourceId("export"), "Export", "housing.step",
      h::checked(step_import_sha256(file_bytes(dir.filePath("housing.step")))),
      StepImportMode::Reference));
  auto read = h::checked(StepImportReader{}.read(exported, dir.path().toStdString()));
  double volume{};
  for (const auto& product : read.products)
    for (const auto& body : product.bodies)
      volume += body.volume_mm3;
  CHECK(volume == Catch::Approx(housing_volume(4.5) + cover_volume(2.5)).margin(3e-3));
  CHECK(file_bytes(dir.filePath("supplier.step")) == compatible);
  const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (native && !captures.isEmpty()) {
    REQUIRE(QDir().mkpath(captures));
    REQUIRE(w.grab().save(captures + "/configurable-housing-X02.png"));
  }
}
