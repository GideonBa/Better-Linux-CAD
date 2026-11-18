#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/shell/shell_direct_edit_tools.hpp"
#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numbers>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace {
using Tools = ShellDirectEditTools;
template <class T> T* field(Tools& t, const char* key) {
  auto* result = qobject_cast<T*>(t.field(key));
  REQUIRE(result);
  return result;
}
std::string saved(const ShellWindow& w) {
  return f::checked(serialize_part_document_to_json(*w.session().part_document()));
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
void write(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(file.write(bytes) == bytes.size());
}
struct Fixture {
  QTemporaryDir dir;
  QString asset() const {
    return dir.filePath("supplier.step");
  }
  void source(bool bore = true) {
    auto shape = f::checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
        f::checked(Quantity::length_mm(20)), f::checked(Quantity::length_mm(10)),
        f::checked(Quantity::length_mm(8))));
    if (bore)
      shape = f::checked(geometry::CircularCutAdapter{}.cut_circular_hole(
          shape, f::checked(Quantity::length_mm(4))));
    REQUIRE(geometry::StepExporter{}.write_step(shape, asset().toStdString()));
  }
  PartDocument part() {
    const auto source = f::checked(
        StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                                 f::checked(step_import_sha256(bytes(asset()).toStdString())),
                                 StepImportMode::EditableBody));
    const auto read =
        f::checked(geometry::StepImportReader{}.read(source, dir.path().toStdString()));
    std::size_t selected = read.products.size();
    for (std::size_t i = 0; i < read.products.size(); ++i)
      if (!read.products[i].bodies.empty()) {
        selected = i;
        break;
      }
    REQUIRE(selected < read.products.size());
    const auto catalog = f::checked(geometry::StepImportTopology{}.create_catalog(
        ImportedTopologyCatalogId("supplier.catalog"), read, selected));
    return f::checked(geometry::StepEditableCommands{}.create_definition(
        source, catalog, DocumentId("imported.direct"), "Imported housing",
        FeatureId("imported.base"), dir.path().toStdString()));
  }
  void open(ShellWindow& w) {
    source();
    const auto p = part();
    const auto path = dir.filePath("imported.blcad.json");
    {
      std::ofstream out(path.toStdString());
      out << f::checked(serialize_part_document_to_json(p));
    }
    REQUIRE(w.open_path(path));
  }
};
BodyId body(const ShellWindow& w) {
  return w.session()
      .part_document()
      ->imported_body_features()
      .front()
      .definition()
      .outputs()
      .front()
      .body;
}
double volume(const ShellWindow& w) {
  return geometry::RectangleExtrusionAdapter{}
      .summarize(*w.session().part_shape_cache()->find_body_shape(body(w)))
      .volume_mm3;
}
void bore(Tools& t) {
  auto* combo = field<QComboBox>(t, "face");
  int found = -1;
  for (int i = 0; i < combo->count(); ++i)
    if (combo->itemText(i).startsWith("Cylindrical face")) {
      REQUIRE(found < 0);
      found = i;
    }
  REQUIRE(found >= 0);
  combo->setCurrentIndex(found);
  field<QPushButton>(t, "add_face")->click();
}
void preview(Tools& t) {
  auto r = t.preview();
  INFO((r ? "ok" : r.error().message()));
  REQUIRE(r);
}
void step_refresh(ShellWindow& w, bool expected) {
  auto& t = w.step_import_tools();
  REQUIRE(t.begin());
  auto* operation = w.findChild<QComboBox*>("blcad.step_import.reference_operation");
  REQUIRE(operation);
  operation->setCurrentIndex(2);
  auto result = t.prepare_reference();
  INFO((result ? "ok" : result.error().message()));
  if (expected) {
    REQUIRE(result);
    REQUIRE(t.apply());
  } else {
    CHECK_FALSE(result);
    t.cancel();
  }
}
} // namespace
TEST_CASE("Native Direct Edit sizes heals and refreshes imported bores with exact "
          "source authority",
          "[gui][direct-edit-import][native-direct-edit][parity-b187][D01][D02]") {
  if (QGuiApplication::platformName() == "offscreen")
    SKIP("Requires native imported direct editing");
  ShellWindow w;
  Fixture fixture;
  fixture.open(w);
  w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380,
           std::min(850, w.screen()->availableGeometry().height() - 40));
  w.show();
  qApp->processEvents();
  w.viewport().fit_all();
  qApp->processEvents();
  REQUIRE(w.viewport().native_viewer_available());
  const auto baseline = saved(w);
  const auto source_bytes = bytes(fixture.asset());
  const auto base = w.session().part_document()->imported_body_features().front();
  auto& t = w.direct_edit_tools();
  REQUIRE(t.begin(Tools::Mode::Size));
  bore(t);
  field<QComboBox>(t, "measurement")->setCurrentIndex(1);
  field<QLineEdit>(t, "size")->setText("6");
  preview(t);
  CHECK(saved(w) == baseline);
  CHECK(bytes(fixture.asset()) == source_bytes);
  REQUIRE(t.apply());
  CHECK(volume(w) == Catch::Approx(1600 - 72 * std::numbers::pi).margin(1e-4));
  const auto sized = saved(w);
  REQUIRE(w.undo());
  CHECK(saved(w) == baseline);
  REQUIRE(w.redo());
  CHECK(saved(w) == sized);
  REQUIRE(t.begin_edit(FeatureId("direct_edit.1")));
  CHECK(field<QComboBox>(t, "measurement")->currentIndex() == 1);
  CHECK(field<QLineEdit>(t, "size")->text().toDouble() == 6);
  field<QLineEdit>(t, "size")->setText("1000");
  CHECK_FALSE(t.preview());
  CHECK(saved(w) == sized);
  field<QLineEdit>(t, "size")->setText("6");
  preview(t);
  t.cancel();
  REQUIRE(w.undo());
  REQUIRE(t.begin(Tools::Mode::DeleteHeal));
  auto* available = field<QComboBox>(t, "face");
  int exterior = -1;
  for (int i = 0; i < available->count(); ++i)
    if (available->itemText(i).startsWith("Planar face") && available->itemText(i).endsWith(", 8)"))
      exterior = i;
  REQUIRE(exterior >= 0);
  available->setCurrentIndex(exterior);
  field<QPushButton>(t, "add_face")->click();
  CHECK_FALSE(t.preview());
  CHECK(saved(w) == baseline);
  t.cancel();
  REQUIRE(t.begin(Tools::Mode::DeleteHeal));
  bore(t);
  preview(t);
  REQUIRE(t.apply());
  CHECK(volume(w) == Catch::Approx(1600).margin(1e-4));
  CHECK(bytes(fixture.asset()) == source_bytes);
  REQUIRE(w.undo());
  CHECK(saved(w) == baseline);
  REQUIRE(w.redo());
  REQUIRE(w.save_to(fixture.dir.filePath("healed.blcad.json")));
  REQUIRE(w.open_path(fixture.dir.filePath("healed.blcad.json")));
  CHECK(volume(w) == Catch::Approx(1600).margin(1e-4));
  const auto old = saved(w);
  const auto compatible = source_bytes + "\n/* compatible supplier revision */\n";
  write(fixture.asset(), compatible);
  step_refresh(w, true);
  CHECK(saved(w) != old);
  CHECK(w.session().part_document()->imported_body_features().front().definition() ==
        base.definition());
  CHECK(volume(w) == Catch::Approx(1600).margin(1e-4));
  const auto refreshed = saved(w);
  fixture.source(false);
  step_refresh(w, false);
  CHECK(saved(w) == refreshed);
  write(fixture.asset(), compatible);
  REQUIRE(w.save_to(fixture.dir.filePath("final.blcad.json")));
  REQUIRE(w.open_path(fixture.dir.filePath("final.blcad.json")));
  geometry::StepImportTopologyContext context(fixture.dir.path().toStdString());
  REQUIRE(geometry::StepExporter(&context).write_part_step(
      *w.session().part_document(), *w.session().part_shape_cache(),
      fixture.dir.filePath("result.step").toStdString()));
  const auto exported = f::checked(StepImportSource::create(
      StepImportSourceId("result"), "Result", "result.step",
      f::checked(step_import_sha256(bytes(fixture.dir.filePath("result.step")).toStdString())),
      StepImportMode::Reference));
  const auto read =
      f::checked(geometry::StepImportReader{}.read(exported, fixture.dir.path().toStdString()));
  double total = 0;
  for (const auto& p : read.products)
    for (const auto& b : p.bodies)
      total += b.volume_mm3;
  CHECK(total == Catch::Approx(1600).margin(1e-4));
  auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!root.isEmpty()) {
    REQUIRE(QDir().mkpath(root));
    REQUIRE(w.grab().save(root + "/direct-edit-import-healed.png"));
  }
  w.hide();
}
