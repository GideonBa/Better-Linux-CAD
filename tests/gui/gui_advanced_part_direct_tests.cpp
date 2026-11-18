#include "../support/direct_edit_integrated_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/shell/shell_direct_edit_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QScreen>
#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace combined = blcad::test::direct_integrated;
namespace {
std::string bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll().toStdString();
}
void write(const QString& path, const std::string& value) {
  std::ofstream out(path.toStdString(), std::ios::binary | std::ios::trunc);
  out << value;
  REQUIRE(out.good());
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
void source(const QString& path, bool incompatible = false) {
  auto shape = f::checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      f::checked(Quantity::length_mm(20)), f::checked(Quantity::length_mm(10)),
      f::checked(Quantity::length_mm(incompatible ? 7 : 8))));
  if (incompatible)
    shape = f::checked(geometry::BodyTransformAdapter{}.translate(shape, {0, 0, 1}));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.toStdString()));
}
PartDocument imported(QTemporaryDir& dir) {
  source(dir.filePath("supplier.step"));
  auto source = f::checked(
      StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                               f::checked(step_import_sha256(bytes(dir.filePath("supplier.step")))),
                               StepImportMode::EditableBody));
  const auto read = f::checked(geometry::StepImportReader{}.read(source, dir.path().toStdString()));
  std::size_t index = read.products.size();
  for (std::size_t i = 0; i < read.products.size(); ++i)
    if (!read.products[i].bodies.empty()) {
      index = i;
      break;
    }
  REQUIRE(index < read.products.size());
  const auto catalog = f::checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("supplier.catalog"), read, index));
  return f::checked(geometry::StepEditableCommands{}.create_definition(
      source, catalog, DocumentId("advanced.import"), "Advanced housing",
      FeatureId("imported.base"), dir.path().toStdString()));
}
double single(double direct = 1) {
  const double pi = std::numbers::pi;
  return 200 * (8 + direct) + 12 + 92 + 1 - pi * .01 * (8 + direct + 3) - pi * .01 -
         (2. / 7) * .25 * 7.5 - (1 - pi / 4) * 20 * (.04 + .06 + .09) / 3;
}
double expected(double direct = 1) {
  return 3 * single(direct) + (200 - std::numbers::pi * .01) * .5;
}
double total(const geometry::ShapeCache& c) {
  double out = 0;
  for (const auto& body : c.body_shapes()) {
    const auto shape = geometry::RectangleExtrusionAdapter{}.summarize(body.shape);
    if (shape.solid_count)
      out += shape.volume_mm3;
  }
  return out;
}
} // namespace

TEST_CASE("Advanced Part combines imported Direct Edit with supported bosses Web rounds Emboss and "
          "Body history",
          "[gui][advanced-part-direct][parity-b188][D02]") {
  const bool native = QGuiApplication::platformName() != "offscreen";
  QTemporaryDir dir;
  geometry::StepImportTopologyContext imports(dir.path().toStdString());
  const bool converted = GENERATE(false, true);
  auto seed = imported(dir);
  const auto initial = converted ? seed : f::native_base();
  const auto immutable_bases = initial.imported_body_features();
  const auto source_bytes = bytes(dir.filePath("supplier.step"));
  auto p = combined::build(initial, &imports);
  const auto model = saved(p);
  auto c = f::checked(geometry::ShapeCache::create(ShapeCacheId("combined")));
  combined::compute(p, c, &imports);
  CHECK(total(c) == Catch::Approx(expected()).margin(1e-3));
  const auto mass =
      f::checked(geometry::BodyResultInspector{}.inspect_physical_properties(p, &imports));
  REQUIRE(mass.data().complete_mass());
  CHECK(mass.data().complete_mass()->mass_kg == Catch::Approx(expected() * 4e-6).margin(1e-8));
  CHECK(bytes(dir.filePath("supplier.step")) == source_bytes);
  CHECK(p.imported_body_features() == immutable_bases);
  REQUIRE(p.find_hole_feature(FeatureId("boss.hole"))->thread());
  auto changed = p;
  REQUIRE(changed.set_parameter_value(ParameterId("direct.distance"),
                                      f::checked(Quantity::length_mm(1.5))));
  const auto plan = f::checked(changed.create_recompute_plan());
  auto incremental = geometry::GeometryRecomputeExecutor(&imports).execute_plan(changed, plan, c);
  INFO((incremental ? "ok" : incremental.error().message()));
  REQUIRE(incremental);
  auto fresh = f::checked(geometry::ShapeCache::create(ShapeCacheId("combined.fresh")));
  combined::compute(changed, fresh, &imports);
  CHECK(total(c) == Catch::Approx(total(fresh)).margin(1e-5));
  CHECK(total(fresh) == Catch::Approx(expected(1.5)).margin(1e-3));
  PartFeatureHistoryIntent history;
  for (const auto& entry : f::checked(p.executable_history()))
    history.order.push_back(entry.node_id);
  auto suppressed = history;
  suppressed.suppressed = {"pattern", "pattern.place"};
  auto reduced = f::checked(p.with_history_intent(suppressed));
  auto reduced_cache =
      f::checked(geometry::ShapeCache::create(ShapeCacheId("combined.suppressed")));
  combined::compute(reduced, reduced_cache, &imports);
  CHECK(total(reduced_cache) ==
        Catch::Approx(single() + (200 - std::numbers::pi * .01) * .5).margin(1e-3));
  auto reordered = history;
  auto placed = std::find(reordered.order.begin(), reordered.order.end(), "pattern.place");
  auto skin = std::find(reordered.order.begin(), reordered.order.end(), "skin.offset");
  REQUIRE(placed != reordered.order.end());
  REQUIRE(skin != reordered.order.end());
  std::iter_swap(placed, skin);
  auto safe = f::checked(p.with_history_intent(reordered));
  auto safe_cache = f::checked(geometry::ShapeCache::create(ShapeCacheId("combined.reorder")));
  combined::compute(safe, safe_cache, &imports);
  CHECK(total(safe_cache) == Catch::Approx(expected()).margin(1e-3));
  std::swap(history.order.front(), history.order[1]);
  CHECK_FALSE(p.with_history_intent(history));
  CHECK(saved(p) == model);
  const auto path = dir.filePath("advanced.blcad.json");
  write(path, model);
  ShellWindow w;
  REQUIRE(w.open_path(path));
  if (native) {
    w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380,
             std::min(850, w.screen()->availableGeometry().height() - 40));
    w.show();
    qApp->processEvents();
    w.viewport().fit_all();
    qApp->processEvents();
    REQUIRE(w.viewport().native_viewer_available());
  }
  auto& edit = w.direct_edit_tools();
  REQUIRE(edit.begin_edit(FeatureId("direct")));
  auto* distance = qobject_cast<QLineEdit*>(edit.field("size"));
  REQUIRE(distance);
  distance->setText("1.5");
  auto preview = edit.preview();
  INFO((preview ? "ok" : preview.error().message()));
  REQUIRE(preview);
  REQUIRE(edit.apply());
  CHECK(total(*w.session().part_shape_cache()) == Catch::Approx(expected(1.5)).margin(1e-3));
  const auto applied = saved(*w.session().part_document());
  REQUIRE(w.undo());
  CHECK(saved(*w.session().part_document()) == model);
  REQUIRE(w.redo());
  CHECK(saved(*w.session().part_document()) == applied);
  REQUIRE(w.save_to(path));
  REQUIRE(w.open_path(path));
  CHECK(saved(*w.session().part_document()) == applied);
  if (converted) {
    const auto compatible = source_bytes + "\n/* compatible advanced supplier revision */\n";
    write(dir.filePath("supplier.step"), compatible);
    const auto refreshed = f::checked(geometry::StepEditableCommands{}.refresh(
        *w.session().part_document(), StepImportSourceId("supplier"), dir.path().toStdString()));
    CHECK(refreshed.part.imported_body_features() == immutable_bases);
    CHECK(refreshed.part.direct_edit_feature_count() == 1);
    source(dir.filePath("supplier.step"), true);
    CHECK_FALSE(geometry::StepEditableCommands{}.refresh(
        refreshed.part, StepImportSourceId("supplier"), dir.path().toStdString()));
    CHECK(saved(*w.session().part_document()) == applied);
    write(dir.filePath("supplier.step"), compatible);
    write(path, saved(refreshed.part));
    REQUIRE(w.open_path(path));
  }
  geometry::StepImportTopologyContext final_context(dir.path().toStdString());
  REQUIRE(geometry::StepExporter(&final_context)
              .write_part_step(*w.session().part_document(), *w.session().part_shape_cache(),
                               dir.filePath("advanced.step").toStdString()));
  auto export_source = f::checked(
      StepImportSource::create(StepImportSourceId("export"), "Export", "advanced.step",
                               f::checked(step_import_sha256(bytes(dir.filePath("advanced.step")))),
                               StepImportMode::Reference));
  const auto read =
      f::checked(geometry::StepImportReader{}.read(export_source, dir.path().toStdString()));
  double exported = 0;
  for (const auto& product : read.products)
    for (const auto& body : product.bodies)
      exported += body.volume_mm3;
  CHECK(exported == Catch::Approx(expected(1.5)).margin(2e-3));
  const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (native && !captures.isEmpty()) {
    REQUIRE(QDir().mkpath(captures));
    REQUIRE(w.grab().save(captures + "/advanced-part-direct.png"));
  }
  w.hide();
}
