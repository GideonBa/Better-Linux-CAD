#include "../support/material_physical_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/gui_physical_properties.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_material_tools.hpp"
#include "blcad/gui/shell/shell_physical_properties_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

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
template <class T> T& field(ShellWindow& w, const char* id) {
  auto* value = w.findChild<T*>(QString("blcad.physical.") + id);
  REQUIRE(value);
  return *value;
}
void settle() {
  for (int i = 0; i < 6; ++i)
    qApp->processEvents();
}
struct ClipboardRestore {
  QString previous = QApplication::clipboard()->text();
  ~ClipboardRestore() {
    QApplication::clipboard()->setText(previous);
  }
};
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
  w.viewport().fit_all();
  settle();
}
QString open(ShellWindow& w, QTemporaryDir& dir, PartDocument p = fixture::part()) {
  const auto path = dir.filePath("physical.blcad.json");
  REQUIRE(write_part_document_json_file(p, path.toStdString()));
  REQUIRE(w.open_path(path));
  show(w);
  return path;
}
QString open(ShellWindow& w, QTemporaryDir& dir, Project p) {
  const auto path = dir.filePath("physical.blcad.project.json");
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
void begin(ShellWindow& w) {
  int inspect = -1;
  for (int i = 0; i < w.ribbon().tab_count(); ++i)
    if (w.ribbon().tab_title(i) == "Inspect")
      inspect = i;
  REQUIRE(inspect >= 0);
  REQUIRE(w.ribbon().tab_enabled(inspect));
  w.ribbon().set_current_tab(inspect);
  settle();
  QToolButton* button = nullptr;
  for (auto* candidate : w.ribbon().findChildren<QToolButton*>())
    if (candidate->defaultAction() &&
        candidate->defaultAction()->objectName() == "blcad.action.physical_properties")
      button = candidate;
  REQUIRE(button);
  for (auto* parent = button->parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
      scroll->ensureWidgetVisible(button);
      break;
    }
  settle();
  REQUIRE(button->isVisible());
  REQUIRE(button->isEnabled());
  button->click();
  settle();
  REQUIRE(w.physical_properties_tools().active());
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  settle();
  REQUIRE(w.grab().save(QDir(dir).filePath(QString(name) + ".png")));
}
void numeric(ShellWindow& w, const char* id, double expected) {
  const auto value = field<QLabel>(w, id).text().section(' ', 0, 0).toDouble();
  CHECK(value == Catch::Approx(expected).epsilon(1e-9));
}
void centroid(ShellWindow& w, const char* id, Point3 expected, const char* unit,
              bool subset = false) {
  const auto label = field<QLabel>(w, id).text();
  INFO(label.toStdString());
  CHECK(label.endsWith(QString(" ") + unit));
  CHECK(label.startsWith("Known subset: ") == subset);
  const auto coordinates = label.section('(', 1, 1).section(')', 0, 0).split(',');
  REQUIRE(coordinates.size() == 3);
  const std::array values{expected.x, expected.y, expected.z};
  for (int i = 0; i < 3; ++i) {
    bool ok = false;
    const double value = coordinates[i].toDouble(&ok);
    REQUIRE(ok);
    CHECK(value == Catch::Approx(values[i]).epsilon(1e-9).margin(1e-10));
  }
}
PartDocument reference(QTemporaryDir& dir) {
  auto native = fixture::part();
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("physical.supplier")));
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
  return checked(PartDocument::create_reference(DocumentId("reference"), "Reference", supplier,
                                                catalog, definition));
}
} // namespace

TEST_CASE("Physical Properties uses the Inspect task, typed units and the real clipboard without "
          "authored edits",
          "[gui][parity-b151][physical-properties-shell]") {
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = source(w);
  const auto revision = w.session().model_revision();
  const auto shape = *w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  const auto camera = w.viewport().camera_bookmark();
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  begin(w);
  CHECK(w.session().task().command_id() == GuiPhysicalPropertiesController::task_id);
  numeric(w, "volume", 1600);
  numeric(w, "area", 880);
  numeric(w, "mass", .0064);
  CHECK(field<QLabel>(w, "status").text().startsWith("Complete mass"));
  field<QComboBox>(w, "length_unit").setCurrentIndex(1);
  field<QComboBox>(w, "mass_unit").setCurrentIndex(1);
  numeric(w, "volume", 1.6);
  numeric(w, "area", 8.8);
  numeric(w, "mass", 6.4);
  centroid(w, "mass_centroid", {0, 0, .4}, "cm");
  CHECK(field<QLabel>(w, "materials").text().contains("4 g/cm³"));
  CHECK(field<QLabel>(w, "materials").text().contains("Synthetic resource fixture"));
  CHECK(field<QTableWidget>(w, "inertia").item(0, 0)->text().toDouble() ==
        Catch::Approx(.8746666666667));
  field<QPushButton>(w, "copy").click();
  const auto copied = QApplication::clipboard()->text();
  CHECK(copied.contains("Mass: 6.4 g"));
  CHECK(copied.contains("Solid volume: 1.6 cm³"));
  CHECK(copied.contains("Frame: Part coordinates"));
  CHECK(copied.contains("Requested relative integration tolerance:"));
  CHECK(copied.contains("Central inertia [g·cm²]"));
  CHECK(source(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().dirty());
  CHECK_FALSE(w.session().can_undo());
  CHECK(geometry::OcctShapeView::get(*w.session().part_shape_cache()->find_body_shape(
            BodyId("body.base"))) == geometry::OcctShapeView::get(shape));
  field<QPushButton>(w, "close").click();
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.physical_properties_tools().active());
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  CHECK(w.viewport().camera_bookmark().eye == camera.eye);
  CHECK(w.viewport().camera_bookmark().target == camera.target);
}

TEST_CASE("Physical Properties exposes qualified repeated occurrences and Body scopes in root "
          "coordinates",
          "[gui][parity-b151][physical-properties-shell]") {
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, fixture::project());
  const auto before = source(w);
  begin(w);
  auto& scopes = field<QComboBox>(w, "scope");
  REQUIRE(scopes.count() == 7);
  numeric(w, "volume", 6400);
  numeric(w, "mass", .032);
  CHECK(field<QLabel>(w, "status").text().contains("Hidden active results included"));
  REQUIRE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Solid Bodies: 4"));
  centroid(w, "mass_centroid", {24, 9, 4}, "mm");
  const auto right = scopes.findText("Right", Qt::MatchContains);
  REQUIRE(right >= 0);
  scopes.setCurrentIndex(right);
  numeric(w, "volume", 3200);
  numeric(w, "mass", .016);
  centroid(w, "mass_centroid", {30, 18, 4}, "mm");
  REQUIRE(scopes.itemText(6).contains("Right"));
  scopes.setCurrentIndex(6);
  numeric(w, "volume", 1600);
  numeric(w, "mass", .0096);
  centroid(w, "mass_centroid", {30, 30, 4}, "mm");
  REQUIRE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Right"));
  CHECK(field<QLabel>(w, "frame").text().contains("Root assembly coordinates"));
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().dirty());
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE("Physical readout invalidates after Save and refreshes without changing history or "
          "clipboard on failure",
          "[gui][parity-b151][physical-properties-shell]") {
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  const auto path = open(w, dir);
  begin(w);
  field<QCheckBox>(w, "show_centroid").setChecked(true);
  auto* overlay = w.findChild<QWidget*>("blcad.physical.overlay");
  REQUIRE(overlay);
  REQUIRE(overlay->isVisible());
  QApplication::clipboard()->setText("sentinel");
  REQUIRE(w.save_to(path));
  CHECK(w.physical_properties_tools().active());
  CHECK_FALSE(field<QPushButton>(w, "copy").isEnabled());
  CHECK_FALSE(overlay->isVisible());
  CHECK(field<QLabel>(w, "mass").text().isEmpty());
  CHECK_FALSE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text() == "sentinel");
  const auto revision = w.session().model_revision();
  const auto before = source(w);
  REQUIRE(w.physical_properties_tools().update_query());
  numeric(w, "mass", .0064);
  CHECK(overlay->isVisible());
  CHECK(w.session().model_revision() == revision);
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&w, &escape);
  CHECK_FALSE(w.physical_properties_tools().active());
  CHECK_FALSE(overlay->isVisible());
  CHECK_FALSE(w.session().task().active());
}

TEST_CASE("Material task changes produce explicitly labelled partial physical values and one "
          "accepted Undo",
          "[gui][parity-b151][physical-properties-shell][material-physical-shell]") {
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, fixture::part("physical.part", true, true));
  const auto before = source(w);
  begin(w);
  numeric(w, "mass", .016);
  w.physical_properties_tools().cancel();
  const auto shape = *w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(w.material_tools().begin());
  auto* assignment = w.findChild<QComboBox*>("blcad.material.assignment");
  REQUIRE(assignment);
  assignment->setCurrentIndex(0);
  REQUIRE(w.material_tools().stage_assignment());
  REQUIRE(w.material_tools().apply());
  CHECK(geometry::OcctShapeView::get(*w.session().part_shape_cache()->find_body_shape(
            BodyId("body.base"))) == geometry::OcctShapeView::get(shape));
  begin(w);
  CHECK(field<QLabel>(w, "status").text().contains("Partial mass"));
  CHECK(field<QLabel>(w, "mass").text() == "Known subset: 0.0096 kg");
  centroid(w, "volume_centroid", {15, 0, 4}, "mm");
  centroid(w, "mass_centroid", {30, 0, 4}, "mm", true);
  CHECK(field<QLabel>(w, "inertia_unit").text().contains("Known-subset"));
  CHECK(field<QLabel>(w, "principals").text().contains("Known subset"));
  CHECK(field<QLabel>(w, "materials").text().contains("Unassigned Bodies: 1"));
  REQUIRE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Known-subset Mass: 0.0096 kg"));
  REQUIRE(w.undo());
  CHECK_FALSE(w.physical_properties_tools().active());
  CHECK(source(w) == before);
  CHECK_FALSE(w.session().can_undo());
  begin(w);
  numeric(w, "mass", .016);
  CHECK(field<QLabel>(w, "status").text().startsWith("Complete mass"));
}

TEST_CASE("Physical scope loss requires an explicit new scope and empty or Surface scopes never "
          "invent mass",
          "[gui][parity-b151][physical-properties-shell]") {
  SECTION("lost Body") {
    ShellWindow w;
    QTemporaryDir dir;
    open(w, dir, fixture::part("physical.part", true, true));
    begin(w);
    auto& scopes = field<QComboBox>(w, "scope");
    REQUIRE(scopes.count() == 3);
    scopes.setCurrentIndex(2);
    REQUIRE(w.session().commit_part_transaction("Suppress scoped Body", [](auto& p) {
      return p.apply_history_intent({{"base", "second", "shift"}, {"second", "shift"}});
    }));
    w.refresh();
    CHECK_FALSE(field<QPushButton>(w, "copy").isEnabled());
    CHECK_FALSE(w.physical_properties_tools().update_query());
    CHECK(scopes.currentIndex() == -1);
    CHECK(field<QLabel>(w, "mass").text().isEmpty());
    scopes.setCurrentIndex(0);
    numeric(w, "volume", 1600);
    numeric(w, "mass", .0064);
    CHECK(field<QPushButton>(w, "copy").isEnabled());
  }
  SECTION("Surface") {
    ClipboardRestore clipboard;
    ShellWindow w;
    QTemporaryDir dir;
    open(w, dir, fixture::surface());
    begin(w);
    numeric(w, "area", 50);
    CHECK(field<QLabel>(w, "volume").text() == "Not applicable");
    CHECK(field<QLabel>(w, "mass").text() == "Not applicable");
    CHECK(field<QLabel>(w, "status").text().contains("not applicable"));
    field<QCheckBox>(w, "show_centroid").setChecked(true);
    field<QComboBox>(w, "centroid_kind").setCurrentIndex(0);
    REQUIRE(w.findChild<QWidget*>("blcad.physical.overlay"));
    CHECK_FALSE(w.findChild<QWidget*>("blcad.physical.overlay")->isVisible());
    REQUIRE(w.physical_properties_tools().copy());
    CHECK(QApplication::clipboard()->text().contains("Solid volume: Not applicable"));
    CHECK_FALSE(QApplication::clipboard()->text().contains("\nMass: 0"));
  }
  SECTION("assigned material without density") {
    ShellWindow w;
    QTemporaryDir dir;
    auto part = fixture::part();
    const auto material = checked(EngineeringMaterial::create(
        resources::key("unknown.density"), "Density unknown", resources::provenance()));
    REQUIRE(part.add_engineering_resource(material));
    REQUIRE(part.set_default_material(material.key()));
    open(w, dir, part);
    begin(w);
    CHECK(field<QLabel>(w, "status").text().contains("unavailable"));
    CHECK(field<QLabel>(w, "mass").text() == "Unavailable");
    CHECK(field<QLabel>(w, "materials").text().contains("Density unavailable"));
    CHECK(field<QLabel>(w, "materials").text().contains("Synthetic resource fixture"));
    CHECK(field<QLabel>(w, "materials").text().contains("Unassigned Bodies: 0"));
  }
  SECTION("empty") {
    ShellWindow w;
    REQUIRE(w.new_part("Empty"));
    show(w);
    begin(w);
    CHECK(field<QLabel>(w, "status").text().contains("not applicable"));
    CHECK(field<QLabel>(w, "mass_centroid").text() == "Not applicable");
    CHECK_FALSE(w.session().can_undo());
  }
}

TEST_CASE("Physical Properties refuses missing busy and prefix contexts without stealing tasks",
          "[gui][parity-b151][physical-properties-shell]") {
  ShellWindow w;
  CHECK_FALSE(w.physical_properties_tools().begin());
  CHECK_FALSE(w.session().task().active());
  QTemporaryDir dir;
  open(w, dir, fixture::part("physical.part", true, true));
  REQUIRE(w.session().task().begin("foreign"));
  CHECK_FALSE(w.physical_properties_tools().begin());
  CHECK(w.session().task().command_id() == "foreign");
  REQUIRE(w.session().task().cancel());
  REQUIRE(w.history_tools().begin_prefix(1));
  CHECK_FALSE(w.physical_properties_tools().begin());
  CHECK(w.history_tools().prefix_active());
  w.history_tools().cancel();
  begin(w);
  CHECK_FALSE(w.material_tools().begin());
  CHECK(w.physical_properties_tools().active());
}

TEST_CASE("Physical Copy and centroid reject supplier byte changes without a model revision and "
          "recover by Refresh",
          "[gui][parity-b151][physical-properties-shell][physical-supplier-shell]") {
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, reference(dir));
  const auto before = source(w);
  const auto revision = w.session().model_revision();
  begin(w);
  numeric(w, "volume", 1600);
  CHECK(field<QLabel>(w, "status").text().contains("unavailable"));
  field<QComboBox>(w, "centroid_kind").setCurrentIndex(0);
  field<QCheckBox>(w, "show_centroid").setChecked(true);
  auto* overlay = w.findChild<QWidget*>("blcad.physical.overlay");
  REQUIRE(overlay);
  REQUIRE(overlay->isVisible());
  const auto path = dir.filePath("supplier.step");
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto original = file.readAll();
  file.close();
  REQUIRE(file.open(QIODevice::Append));
  REQUIRE(file.write("\n") == 1);
  file.close();
  QApplication::clipboard()->setText("sentinel");
  CHECK_FALSE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text() == "sentinel");
  CHECK_FALSE(field<QPushButton>(w, "copy").isEnabled());
  CHECK_FALSE(overlay->isVisible());
  CHECK_FALSE(w.physical_properties_tools().update_query());
  CHECK(source(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().dirty());
  REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(file.write(original) == original.size());
  file.close();
  REQUIRE(w.physical_properties_tools().update_query());
  numeric(w, "volume", 1600);
  CHECK(overlay->isVisible());
  REQUIRE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Solid volume: 1600 mm³"));
  CHECK(source(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE(
    "Native centroid triad follows canonical scope and camera and stays fixed across display units",
    "[gui][parity-b151][physical-properties-shell][physical-centroid-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT camera projection");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, fixture::part("physical.part", true, true));
  const auto before = source(w);
  const auto revision = w.session().model_revision();
  begin(w);
  field<QCheckBox>(w, "show_centroid").setChecked(true);
  settle();
  auto* overlay = w.findChild<QWidget*>("blcad.physical.overlay");
  REQUIRE(overlay);
  REQUIRE(overlay->isVisible());
  CHECK(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
  const auto marker = [&](Point3 expected) {
    w.physical_properties_tools().refresh_presentation();
    settle();
    const auto screen = checked(w.viewport().model_to_screen(expected));
    const auto image = overlay->grab().toImage();
    const auto dpr = image.devicePixelRatio();
    const int cx = static_cast<int>(std::lround(screen.x * dpr)),
              cy = static_cast<int>(std::lround(screen.y * dpr));
    bool found = false;
    for (int y = cy - 6; y <= cy + 6; ++y)
      for (int x = cx - 6; x <= cx + 6; ++x)
        if (image.rect().contains(x, y)) {
          const auto c = image.pixelColor(x, y);
          if (c.alpha() > 128 && c.red() > 230 && c.green() > 150 && c.green() < 230 &&
              c.blue() < 100)
            found = true;
        }
    CHECK(found);
  };
  marker({18, 0, 4});
  capture(w, "physical-centroid-mass");
  field<QComboBox>(w, "length_unit").setCurrentIndex(1);
  field<QComboBox>(w, "mass_unit").setCurrentIndex(1);
  marker({18, 0, 4});
  centroid(w, "mass_centroid", {1.8, 0, .4}, "cm");
  field<QComboBox>(w, "centroid_kind").setCurrentIndex(0);
  marker({15, 0, 4});
  w.viewport().set_standard_view(GuiStandardView::Front);
  settle();
  marker({15, 0, 4});
  capture(w, "physical-centroid-front");
  CHECK(source(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().can_undo());
  w.physical_properties_tools().cancel();
  CHECK_FALSE(overlay->isVisible());
}

TEST_CASE("Physical task keeps scope units tensors and footer reachable at ordinary compact and "
          "scaled sizes",
          "[gui][parity-b151][physical-properties-shell][physical-properties-layout]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, fixture::project());
  begin(w);
  auto* dock = w.findChild<QDockWidget*>("blcad.shell.physical_properties_task");
  REQUIRE(dock);
  auto* scroll = dock->findChild<QScrollArea*>();
  REQUIRE(scroll);
  CHECK(scroll->horizontalScrollBar()->maximum() == 0);
  CHECK(field<QLabel>(w, "status").text().startsWith("Complete mass: 0.032 kg"));
  CHECK(field<QLabel>(w, "status").isVisible());
  CHECK(w.viewport().width() >= 96);
  CHECK(w.viewport().height() >= 96);
  const auto footer = [&] {
    for (const char* name : {"refresh", "copy", "close"}) {
      auto& button = field<QPushButton>(w, name);
      CHECK(button.isVisible());
      CHECK(dock->widget()->rect().contains(button.mapTo(dock->widget(), QPoint(0, 0))));
      CHECK(dock->widget()->rect().contains(
          button.mapTo(dock->widget(), button.rect().bottomRight())));
    }
  };
  footer();
  for (const char* name : {"scope", "length_unit", "mass_unit", "inertia", "show_centroid"}) {
    auto& widget = field<QWidget>(w, name);
    scroll->ensureWidgetVisible(&widget, 0, widget.height() / 2 + 8);
    settle();
    const auto top = widget.mapTo(scroll->viewport(), QPoint(0, 0));
    const auto bottom = widget.mapTo(scroll->viewport(), widget.rect().bottomRight());
    INFO(name << ": " << top.x() << "," << top.y() << " to " << bottom.x() << "," << bottom.y()
              << " in " << scroll->viewport()->width() << "x" << scroll->viewport()->height());
    CHECK(scroll->viewport()->rect().contains(top));
    CHECK(scroll->viewport()->rect().contains(bottom));
    footer();
  }
  scroll->verticalScrollBar()->setValue(0);
  settle();
  capture(w, "physical-properties-root");
  CHECK(field<QLabel>(w, "frame").textFormat() == Qt::PlainText);
  CHECK(field<QTableWidget>(w, "inertia").editTriggers() == QAbstractItemView::NoEditTriggers);
  field<QPushButton>(w, "close").click();
  CHECK_FALSE(w.physical_properties_tools().active());
}

TEST_CASE("Native Material revision Physical Properties and End of Part form one portable atomic "
          "workflow",
          "[gui][parity-b151][material-physical-combined]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native combined Material/Physical Properties acceptance");
  ClipboardRestore clipboard;
  ShellWindow w;
  QTemporaryDir dir;
  const auto path = open(w, dir, fixture::project(true));
  const auto original = source(w);
  begin(w);
  numeric(w, "mass", .032);
  field<QComboBox>(w, "length_unit").setCurrentIndex(1);
  field<QComboBox>(w, "mass_unit").setCurrentIndex(1);
  numeric(w, "mass", 32);
  centroid(w, "mass_centroid", {2.4, .9, .4}, "cm");
  w.physical_properties_tools().cancel();
  const auto old_shape = *w.session()
                              .project_part_shape_cache(DocumentId("material.part"))
                              ->find_body_shape(BodyId("body.base"));
  const auto unused_shape = *w.session()
                                 .project_part_shape_cache(DocumentId("unselected"))
                                 ->find_body_shape(BodyId("body.base"));
  REQUIRE(w.material_tools().begin(DocumentId("material.part")));
  auto* targets = w.findChild<QComboBox*>("blcad.material.part");
  REQUIRE(targets);
  REQUIRE(targets->count() == 2);
  CHECK(targets->itemText(0) != targets->itemText(1));
  CHECK(targets->currentText().contains("material.part"));
  CHECK(targets->toolTip().contains("material.part"));
  auto* library = w.findChild<QComboBox*>("blcad.material.material");
  REQUIRE(library);
  const auto r1 = library->findText(" @ r1", Qt::MatchContains);
  REQUIRE(r1 >= 0);
  library->setCurrentIndex(r1);
  for (const auto& [name, value] : {std::pair{"revision", "r3"},
                                    {"density", "8000"},
                                    {"source", "Synthetic manufacturer fixture"}}) {
    auto* input = w.findChild<QLineEdit*>(QString("blcad.material.") + name);
    REQUIRE(input);
    input->setText(value);
  }
  auto* origin = w.findChild<QComboBox*>("blcad.material.origin");
  REQUIRE(origin);
  origin->setCurrentIndex(1);
  auto* members = w.findChild<QListWidget*>("blcad.material.members");
  REQUIRE(members);
  REQUIRE(members->count() == 2);
  for (int i = 0; i < members->count(); ++i)
    members->item(i)->setCheckState(
        members->item(i)->data(Qt::UserRole).toString() == "material.part" ? Qt::Checked
                                                                           : Qt::Unchecked);
  auto* material_dock = w.findChild<QDockWidget*>("blcad.shell.material_task");
  REQUIRE(material_dock);
  auto* material_scroll = material_dock->findChild<QScrollArea*>();
  REQUIRE(material_scroll);
  const auto material_click = [&](const char* id) {
    auto* button = w.findChild<QPushButton*>(QString("blcad.material.") + id);
    REQUIRE(button);
    REQUIRE(button->isEnabled());
    if (id != std::string_view("apply"))
      material_scroll->ensureWidgetVisible(button, 0, button->height());
    settle();
    REQUIRE(button->isVisible());
    button->click();
    settle();
  };
  material_click("adopt");
  CHECK(source(w) == original);
  CHECK_FALSE(w.session().dirty());
  capture(w, "material-physical-selected-revision");
  material_click("apply");
  REQUIRE_FALSE(w.material_tools().active());
  const auto* root = w.session().project();
  REQUIRE(root);
  CHECK(root->find_part_document(DocumentId("material.part"))->default_material() ==
        resources::key("synthetic.material", "r3"));
  CHECK(root->find_part_document(DocumentId("unselected"))->default_material() == resources::key());
  CHECK(root->find_part_document(DocumentId("material.part"))
            ->effective_material_for_body(BodyId("body.second"))
            ->key() == resources::key("synthetic.material", "r2"));
  CHECK(geometry::OcctShapeView::get(*w.session()
                                          .project_part_shape_cache(DocumentId("material.part"))
                                          ->find_body_shape(BodyId("body.base"))) ==
        geometry::OcctShapeView::get(old_shape));
  CHECK(geometry::OcctShapeView::get(*w.session()
                                          .project_part_shape_cache(DocumentId("unselected"))
                                          ->find_body_shape(BodyId("body.base"))) ==
        geometry::OcctShapeView::get(unused_shape));
  const auto revised = source(w);
  REQUIRE(w.session().can_undo());
  begin(w);
  numeric(w, "mass", 44.8);
  numeric(w, "volume", 6.4);
  centroid(w, "mass_centroid", {2.1428571428571429, .6428571428571429, .4}, "cm");
  centroid(w, "volume_centroid", {2.25, .75, .4}, "cm");
  REQUIRE(w.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Mass: 44.8 g"));
  REQUIRE(w.save_to(path));
  CHECK_FALSE(field<QPushButton>(w, "copy").isEnabled());
  REQUIRE(w.physical_properties_tools().update_query());
  numeric(w, "mass", 44.8);
  capture(w, "material-physical-root-revision");
  REQUIRE(w.undo());
  CHECK_FALSE(w.physical_properties_tools().active());
  CHECK(source(w) == original);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.redo());
  CHECK(source(w) == revised);
  ShellWindow reopened;
  REQUIRE(reopened.open_path(path));
  show(reopened);
  CHECK(source(reopened) == revised);
  begin(reopened);
  numeric(reopened, "mass", .0448);
  reopened.physical_properties_tools().cancel();
  const auto standalone =
      *reopened.session().project()->find_part_document(DocumentId("material.part"));
  ShellWindow part;
  const auto part_path = open(part, dir, standalone);
  REQUIRE(QFile::remove(path)); // Standalone material snapshots survive without the Project file.
  const auto standalone_source = source(part);
  begin(part);
  numeric(part, "mass", .0224);
  part.physical_properties_tools().cancel();
  REQUIRE(part.history_tools().begin_prefix(1));
  REQUIRE(part.material_tools().begin());
  auto* assignment = part.findChild<QComboBox*>("blcad.material.assignment");
  REQUIRE(assignment);
  const auto r2 = assignment->findText(" @ r2", Qt::MatchContains);
  REQUIRE(r2 >= 0);
  assignment->setCurrentIndex(r2);
  REQUIRE(part.material_tools().stage_assignment());
  REQUIRE(part.material_tools().apply());
  CHECK_FALSE(part.physical_properties_tools().begin());
  REQUIRE(part.save_to(part_path));
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(part_path.toStdString())))) == standalone_source);
  part.history_tools().cancel();
  CHECK(source(part) == standalone_source);
  CHECK_FALSE(part.session().can_undo());
  REQUIRE(part.history_tools().begin_prefix(1));
  REQUIRE(part.material_tools().begin());
  assignment->setCurrentIndex(0);
  REQUIRE(part.material_tools().stage_assignment());
  REQUIRE(part.material_tools().apply());
  REQUIRE(part.history_tools().apply());
  REQUIRE(part.session().derived_results_fresh());
  begin(part);
  CHECK(field<QLabel>(part, "mass").text() == "Known subset: 0.0096 kg");
  centroid(part, "mass_centroid", {30, 0, 4}, "mm", true);
  centroid(part, "volume_centroid", {15, 0, 4}, "mm");
  REQUIRE(part.physical_properties_tools().copy());
  CHECK(QApplication::clipboard()->text().contains("Known-subset Mass: 0.0096 kg"));
  capture(part, "material-physical-prefix-partial");
  REQUIRE(part.undo());
  CHECK_FALSE(part.physical_properties_tools().active());
  CHECK(source(part) == standalone_source);
  CHECK_FALSE(part.session().can_undo());
}
