#include "../support/material_physical_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_material_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::material_physical_fixture;
namespace resources = blcad::test::engineering_fixture;
namespace {
template <class T> T checked(Result<T> r) {
  INFO((r ? std::string{} : r.error().message()));
  REQUIRE(r);
  return std::move(r.value());
}
template <class T> T& field(ShellWindow& w, const char* name) {
  auto* result = w.findChild<T*>(QString("blcad.material.") + name);
  REQUIRE(result);
  return *result;
}
void settle() {
  for (int i = 0; i < 6; ++i)
    qApp->processEvents();
}
void show(ShellWindow& w) {
  const QSize desired(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 1024 : 1280,
                      qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 640 : 800);
  const auto size = QApplication::platformName() == "offscreen"
                        ? desired
                        : desired.boundedTo(w.screen()->availableGeometry().size() - QSize(48, 48));
  w.resize(size);
  w.show();
  settle();
  w.resize(size);
  settle();
  if (QApplication::platformName() == "xcb")
    REQUIRE(w.viewport().native_viewer_available());
}
QString open(ShellWindow& w, QTemporaryDir& dir,
             PartDocument p = fixture::part("material.part", true, true)) {
  const auto path = dir.filePath("materials.blcad.json");
  REQUIRE(write_part_document_json_file(p, path.toStdString()));
  REQUIRE(w.open_path(path));
  show(w);
  return path;
}
QString open(ShellWindow& w, QTemporaryDir& dir, Project p) {
  const auto path = dir.filePath("materials.blcad.project.json");
  REQUIRE(write_project_json_file(p, path.toStdString()));
  REQUIRE(w.open_path(path));
  show(w);
  return path;
}
std::string source(const ShellWindow& w) {
  return w.session().part_document()
             ? checked(serialize_part_document_to_json(*w.session().part_document()))
             : checked(serialize_project_to_json(*w.session().project()));
}
void select_revision(ShellWindow& w, const char* rev, const char* combo = "material") {
  auto& choices = field<QComboBox>(w, combo);
  const auto index = choices.findText(QString(" @ ") + rev, Qt::MatchContains);
  REQUIRE(index >= 0);
  choices.setCurrentIndex(index);
}
void context(ShellWindow& w, const char* id, const char* owner = nullptr) {
  QTreeWidgetItem* selected = nullptr;
  for (QTreeWidgetItemIterator it(&w.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == id &&
        (!owner || (*it)->data(0, ShellHistoryBrowser::owner_role).toString() == owner)) {
      selected = *it;
      break;
    }
  REQUIRE(selected);
  w.model_browser().setCurrentItem(selected);
  w.model_browser().scrollToItem(selected);
  settle();
  bool found = false;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (!menu)
      return;
    for (auto* action : menu->actions())
      if (action->objectName() == "blcad.context.material") {
        found = true;
        CHECK(action->isEnabled());
        action->trigger();
        menu->close();
        return;
      }
    menu->close();
  });
  timer.start(10);
  QTimer::singleShot(1000, &w, [] {
    if (auto* p = QApplication::activePopupWidget())
      p->close();
  });
  const auto point = w.model_browser().visualItemRect(selected).center();
  QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                          w.model_browser().viewport()->mapToGlobal(point));
  QApplication::sendEvent(w.model_browser().viewport(), &event);
  timer.stop();
  settle();
  REQUIRE(found);
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  settle();
  REQUIRE(w.grab().save(QDir(dir).filePath(QString(name) + ".png")));
}
} // namespace

TEST_CASE("Material browser assignment is pending, saved atomically and undone once",
          "[gui][parity-b151][material-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto path = open(w, dir);
  const auto before = source(w);
  const auto shape = w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  const auto owner = *shape;
  context(w, "body.base");
  CHECK(field<QComboBox>(w, "body").currentData().toString() == "body.base");
  select_revision(w, "r2", "assignment");
  field<QPushButton>(w, "assign").click();
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().dirty());
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.save_to(path));
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(path.toStdString())))) == before);
  REQUIRE(w.material_tools().active());
  field<QPushButton>(w, "apply").click();
  CHECK_FALSE(w.material_tools().active());
  CHECK_FALSE(w.session().task().active());
  const auto after = source(w);
  CHECK(after != before);
  CHECK(w.session().part_document()->effective_material_for_body(BodyId("body.base"))->key() ==
        resources::key("synthetic.material", "r2"));
  REQUIRE(w.session().part_shape_cache()->find_body_shape(BodyId("body.base")));
  CHECK(geometry::OcctShapeView::get(*w.session().part_shape_cache()->find_body_shape(
            BodyId("body.base"))) == geometry::OcctShapeView::get(owner));
  CHECK(checked(geometry::BodyResultInspector{}.inspect_physical_properties(
                    *w.session().part_document()))
            .data()
            .known_mass->mass_kg == Catch::Approx(.0192));
  REQUIRE(w.save_to(path));
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  ShellWindow reopened;
  REQUIRE(reopened.open_path(path));
  CHECK(source(reopened) == after);
  const auto* body = w.browser_model().find("body.base");
  REQUIRE(body);
  CHECK(std::any_of(body->properties.begin(), body->properties.end(), [](const auto& p) {
    return p.key == "material_assignment" && p.value == "Body override";
  }));
}

TEST_CASE("Material no-op, Cancel and stale context do not publish pending intent",
          "[gui][parity-b151][material-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  REQUIRE(w.material_tools().begin());
  CHECK(checked(w.material_tools().apply()) == 0);
  CHECK_FALSE(w.session().can_undo());
  CHECK_FALSE(w.session().dirty());
  REQUIRE(w.material_tools().begin());
  select_revision(w, "r2", "assignment");
  REQUIRE(w.material_tools().stage_assignment());
  const auto before = source(w);
  w.material_tools().cancel();
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
  CHECK_FALSE(w.material_tools().begin(DocumentId("foreign"), BodyId("body.base")));
  REQUIRE(w.material_tools().begin());
  REQUIRE(w.session().commit_part_transaction("External metadata", [](auto& p) {
    return p.set_default_material(resources::key("synthetic.material", "r2"));
  }));
  w.refresh();
  CHECK_FALSE(w.material_tools().active());
  CHECK_FALSE(w.session().task().active());
}

TEST_CASE("Material editor preserves immutable snapshots and property provenance",
          "[gui][parity-b151][material-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  REQUIRE(w.material_tools().begin());
  select_revision(w, "r1");
  CHECK(field<QLabel>(w, "details").text().contains("Density: User-defined"));
  field<QLineEdit>(w, "density").setText("8000");
  CHECK_FALSE(field<QPushButton>(w, "apply").isEnabled());
  CHECK_FALSE(w.material_tools().stage_snapshot());
  CHECK(w.session()
            .part_document()
            ->engineering_resources()
            .find_material(resources::key())
            ->properties()
            .density->value.kg_per_m3() == 4000);
  field<QLineEdit>(w, "revision").setText("r3");
  field<QComboBox>(w, "origin").setCurrentIndex(2);
  field<QLineEdit>(w, "reference").clear();
  CHECK_FALSE(w.material_tools().stage_snapshot());
  field<QLineEdit>(w, "reference").setText("Synthetic standard example only");
  field<QLineEdit>(w, "source").setText("New synthetic source");
  REQUIRE(w.material_tools().stage_snapshot());
  REQUIRE(w.material_tools().apply());
  const auto& catalog = w.session().part_document()->engineering_resources();
  CHECK(catalog.find_material(resources::key())->properties().density->value.kg_per_m3() == 4000);
  const auto* r3 = catalog.find_material(resources::key("synthetic.material", "r3"));
  REQUIRE(r3);
  CHECK(r3->properties().density->provenance.origin() == EngineeringDataOrigin::Normative);
  CHECK(r3->properties().density->provenance.source() == "New synthetic source");
  CHECK(r3->properties().young_modulus->provenance == resources::provenance());
  CHECK(w.session().part_document()->default_material() == resources::key());
  REQUIRE(w.undo());
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE(
    "Project material adoption updates explicitly checked definitions and reuses member geometry",
    "[gui][parity-b151][material-shell][material-project-scene]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto path = open(w, dir, fixture::project(true));
  const auto before = source(w);
  CHECK(w.viewport().scene_semantic_ids().size() >= 1);
  const auto* cache = w.session().project_part_shape_cache(DocumentId("material.part"));
  REQUIRE(cache);
  const auto* shape = cache->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  const auto owner = *shape;
  REQUIRE(w.material_tools().begin(DocumentId("material.part")));
  CHECK(field<QLabel>(w, "effective").text().contains("all its occurrences"));
  select_revision(w, "r1");
  field<QLineEdit>(w, "revision").setText("r3");
  field<QLineEdit>(w, "density").setText("8000");
  CHECK_FALSE(w.material_tools().stage_snapshot(true));
  auto& members = field<QListWidget>(w, "members");
  REQUIRE(members.count() == 2);
  for (int i = 0; i < members.count(); ++i)
    if (members.item(i)->data(Qt::UserRole).toString() == "material.part")
      members.item(i)->setCheckState(Qt::Checked);
  REQUIRE(w.material_tools().stage_snapshot(true));
  CHECK(source(w) == before);
  REQUIRE(w.material_tools().apply());
  const auto* root = w.session().project();
  REQUIRE(root);
  CHECK(root->find_part_document(DocumentId("material.part"))->default_material() ==
        resources::key("synthetic.material", "r3"));
  CHECK(root->find_part_document(DocumentId("unselected"))->default_material() == resources::key());
  CHECK(geometry::OcctShapeView::get(*w.session()
                                          .project_part_shape_cache(DocumentId("material.part"))
                                          ->find_body_shape(BodyId("body.base"))) ==
        geometry::OcctShapeView::get(owner));
  CHECK_FALSE(w.viewport().scene_semantic_ids().empty());
  REQUIRE(w.save_to(path));
  const auto after = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  ShellWindow reopened;
  REQUIRE(reopened.open_path(path));
  CHECK(source(reopened) == after);
}

TEST_CASE("Material changes at End of Part remain a child of the existing parent workflow",
          "[gui][parity-b151][material-shell][material-history-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto path = open(w, dir);
  const auto before = source(w);
  REQUIRE(w.history_tools().begin_prefix(1));
  REQUIRE(w.material_tools().begin());
  CHECK(w.session().part_history_command_active());
  select_revision(w, "r2", "assignment");
  REQUIRE(w.material_tools().stage_assignment());
  CHECK_FALSE(w.history_tools().apply());
  REQUIRE(w.material_tools().apply());
  CHECK(w.history_tools().prefix_active());
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK_FALSE(w.session().derived_results_fresh());
  REQUIRE(w.save_to(path));
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(path.toStdString())))) == before);
  w.history_tools().cancel();
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.history_tools().begin_prefix(1));
  REQUIRE(w.material_tools().begin());
  select_revision(w, "r2", "assignment");
  REQUIRE(w.material_tools().stage_assignment());
  REQUIRE(w.material_tools().apply());
  REQUIRE(w.history_tools().apply());
  CHECK(w.session().derived_results_fresh());
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE("Material task keeps assignment and footer reachable in ordinary and compact layouts",
          "[gui][parity-b151][material-shell][material-layout]") {
  auto part = fixture::part();
  const auto long_name = std::string(220, 'M') + " <b>plain material</b>";
  auto material = checked(EngineeringMaterial::create(resources::key("long.material"), long_name,
                                                      resources::provenance()));
  REQUIRE(part.add_engineering_resource(material));
  REQUIRE(part.set_default_material(material.key()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  w.material_tools().action().trigger();
  settle();
  REQUIRE(w.material_tools().active());
  auto* dock = w.findChild<QDockWidget*>("blcad.shell.material_task");
  REQUIRE(dock);
  const auto panel = dock->widget()->rect();
  for (const char* id : {"apply", "cancel"}) {
    auto& button = field<QPushButton>(w, id);
    CHECK(button.isVisible());
    CHECK(panel.contains(button.mapTo(dock->widget(), QPoint(0, 0))));
    CHECK(panel.contains(button.mapTo(dock->widget(), button.rect().bottomRight())));
  }
  CHECK(w.viewport().width() >= 96);
  CHECK(w.viewport().height() >= 96);
  CHECK(field<QLabel>(w, "effective").textFormat() == Qt::PlainText);
  CHECK(field<QLabel>(w, "effective").toolTip().contains("<b>plain material</b>"));
  auto* scroll = dock->findChild<QScrollArea*>();
  REQUIRE(scroll);
  CHECK(scroll->horizontalScrollBar()->maximum() == 0);
  for (const char* id : {"assign", "density", "reference", "adopt", "remove"}) {
    auto& widget = field<QWidget>(w, id);
    // QLineEdit reports its cursor rectangle to ensureWidgetVisible; include
    // enough margin for the complete editor frame, not only its text cursor.
    scroll->ensureWidgetVisible(&widget, 0, widget.height() / 2 + 8);
    settle();
    const auto top = widget.mapTo(scroll->viewport(), QPoint(0, 0));
    const auto bottom = widget.mapTo(scroll->viewport(), widget.rect().bottomRight());
    INFO(id << ": " << top.x() << "," << top.y() << " to " << bottom.x() << "," << bottom.y()
            << "; viewport " << scroll->viewport()->width() << "x" << scroll->viewport()->height());
    CHECK(scroll->viewport()->rect().contains(widget.mapTo(scroll->viewport(), QPoint(0, 0))));
    CHECK(scroll->viewport()->rect().contains(
        widget.mapTo(scroll->viewport(), widget.rect().bottomRight())));
    CHECK(field<QPushButton>(w, "apply").isVisible());
    CHECK(field<QPushButton>(w, "cancel").isVisible());
  }
  scroll->verticalScrollBar()->setValue(0);
  settle();
  CHECK(field<QPushButton>(w, "assign").width() >=
        field<QPushButton>(w, "assign")
                .fontMetrics()
                .horizontalAdvance(field<QPushButton>(w, "assign").text()) +
            12);
  capture(w, "material-assignment-long-name");
  select_revision(w, "r1");
  field<QLineEdit>(w, "revision").setText("r3");
  field<QLineEdit>(w, "density").clear();
  REQUIRE(w.material_tools().stage_snapshot());
  select_revision(w, "r3", "assignment");
  REQUIRE(w.material_tools().stage_assignment());
  REQUIRE(w.material_tools().apply());
  CHECK(checked(geometry::BodyResultInspector{}.inspect_physical_properties(
                    *w.session().part_document()))
            .data()
            .mass_status == geometry::PhysicalMassStatus::Unavailable);
}

TEST_CASE("Project Body context qualifies repeated local IDs by the selected owning Part",
          "[gui][parity-b151][material-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, fixture::project(true));
  const auto* node = w.browser_model().find("body.base", "unselected");
  REQUIRE(node);
  CHECK(node->owner_document_id == "unselected");
  CHECK_FALSE(w.browser_model().find("body.base", "foreign"));
  context(w, "body.base", "unselected");
  CHECK(field<QComboBox>(w, "part").currentData().toString() == "unselected");
  CHECK(field<QComboBox>(w, "body").currentData().toString() == "body.base");
  select_revision(w, "r2", "assignment");
  REQUIRE(w.material_tools().stage_assignment());
  REQUIRE(w.material_tools().apply());
  const auto* root = w.session().project();
  REQUIRE(root);
  CHECK(root->find_part_document(DocumentId("unselected"))
            ->effective_material_for_body(BodyId("body.base"))
            ->key() == resources::key("synthetic.material", "r2"));
  CHECK(root->find_part_document(DocumentId("material.part"))
            ->effective_material_for_body(BodyId("body.base"))
            ->key() == resources::key());
  REQUIRE(w.undo());
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE("Reference Parts expose Material read-only while the Project library stays editable",
          "[gui][parity-b151][material-shell][material-reference-shell]") {
  QTemporaryDir dir;
  const auto native = fixture::part();
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("reference.material")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(native, cache));
  const auto path = dir.filePath("supplier.step");
  REQUIRE(geometry::StepExporter{}.write_part_step(native, cache, path.toStdString()));
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto supplier = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step",
      checked(step_import_sha256(bytes.toStdString())), StepImportMode::Reference));
  const auto imported =
      checked(geometry::StepImportReader{}.read(supplier, dir.path().toStdString()));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog"), imported, 0));
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Body)
      outputs.push_back({entry.id, BodyId("import.body." + std::to_string(outputs.size())),
                         *entry.descriptor.body_kind});
  const auto definition =
      checked(ImportedPartDefinition::create(supplier.id(), catalog.id(), outputs));
  const auto reference = checked(PartDocument::create_reference(
      DocumentId("reference"), "Reference", supplier, catalog, definition));
  ShellWindow w;
  open(w, dir, reference);
  const auto before = source(w);
  REQUIRE(w.material_tools().begin());
  CHECK(field<QLabel>(w, "effective").text().contains("read-only"));
  CHECK_FALSE(field<QPushButton>(w, "assign").isEnabled());
  CHECK_FALSE(field<QPushButton>(w, "snapshot").isEnabled());
  CHECK(field<QLineEdit>(w, "density").isReadOnly());
  CHECK_FALSE(w.material_tools().stage_assignment());
  CHECK(checked(w.material_tools().apply()) == 0);
  CHECK(source(w) == before);
  auto assembly = checked(AssemblyDocument::create(DocumentId("reference.root"), "Root"));
  REQUIRE(assembly.add_member_part(reference.id()));
  REQUIRE(assembly.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("ref"), "Reference", reference.id()))));
  auto project =
      checked(Project::create(DocumentId("reference.project"), "Reference Project", assembly));
  REQUIRE(project.add_part_document(reference));
  REQUIRE(project.add_engineering_resource(resources::material()));
  ShellWindow root;
  open(root, dir, project);
  CHECK_FALSE(root.viewport().scene_semantic_ids().empty());
  const auto* reference_part = root.session().project()->find_part_document(reference.id());
  REQUIRE(reference_part);
  const auto* reference_cache = root.session().project_part_shape_cache(reference.id());
  REQUIRE(reference_cache);
  const auto owned_result = checked(reference_cache->validated_reference_result(*reference_part));
  REQUIRE_FALSE(owned_result->bodies.empty());
  const auto original_shape = owned_result->bodies.front().shape;
  REQUIRE(root.material_tools().begin(reference.id()));
  CHECK_FALSE(field<QPushButton>(root, "assign").isEnabled());
  CHECK(field<QPushButton>(root, "snapshot").isEnabled());
  REQUIRE(field<QListWidget>(root, "members").count() == 1);
  CHECK_FALSE(field<QListWidget>(root, "members").item(0)->flags().testFlag(Qt::ItemIsEnabled));
  select_revision(root, "r1");
  field<QLineEdit>(root, "revision").setText("r3");
  REQUIRE(root.material_tools().stage_snapshot());
  REQUIRE(root.material_tools().apply());
  const auto result_after =
      checked(root.session()
                  .project_part_shape_cache(reference.id())
                  ->validated_reference_result(
                      *root.session().project()->find_part_document(reference.id())));
  REQUIRE_FALSE(result_after->bodies.empty());
  CHECK(geometry::OcctShapeView::get(result_after->bodies.front().shape) ==
        geometry::OcctShapeView::get(original_shape));
  CHECK(checked(serialize_part_document_to_json(
            *root.session().project()->find_part_document(reference.id()))) == before);
  CHECK(root.session().project()->engineering_resources().find_material(
      resources::key("synthetic.material", "r3")));
}
