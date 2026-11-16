#include "../support/curve_surface_milestone_oracle.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curve_surface_milestone;
namespace {
const SketchId sid("section.0");
std::vector<std::string> snapshot(const GuiDocumentSession& session) {
  std::vector<std::string> result{
      serialize_part_document_to_json(*session.part_document()).value()};
  for (const auto& c : session.sketch_constraint_catalogs())
    result.push_back(serialize_sketch_constraint_catalog_to_json(c).value());
  for (const auto& c : session.sketch_dimension_catalogs())
    result.push_back(serialize_sketch_dimension_catalog_to_json(c).value());
  return result;
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* item = w.findChild<T*>(name);
  REQUIRE(item);
  return *item;
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(35, &loop, &QEventLoop::quit);
  loop.exec();
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(QString(name) + ".png")));
}
void verify(const geometry::GeometryShape& shape) {
  const auto report = f::physical(shape);
  CHECK(report.valid);
  CHECK(report.solids == 2);
  CHECK(report.material);
  CHECK(report.stations == 32);
  CHECK(report.boundary_error_mm < 1e-5);
  CHECK(report.normal_error_rad < 1e-4);
  CHECK(report.curvature_error_per_mm < 1e-4);
  CHECK(report.blend_error_mm < 1e-5);
}
void add_g2(GuiDocumentSession& session) {
  const auto topology =
      SketchTopology::migrate_legacy(*session.part_document()->find_sketch(sid)).value();
  const auto intent =
      SketchConstraintIntent::create(
          SketchConstraintId("milestone.smooth"), SketchSolverConstraintKind::Smooth,
          {SketchConstraintIntentTarget::entity("entity/upper.0").value(),
           SketchConstraintIntentTarget::point(topology.find_entity("entity/upper.0")->points()[3])
               .value(),
           SketchConstraintIntentTarget::entity("entity/upper.1").value(),
           SketchConstraintIntentTarget::point(topology.find_entity("entity/upper.1")->points()[0])
               .value()})
          .value();
  auto controller = GuiSketchConstraintController::create(session, sid, intent);
  REQUIRE(controller);
  REQUIRE(controller.value().preview().authoring.accepted);
  REQUIRE(controller.value().commit(session));
  REQUIRE(
      session.sketch_constraint_catalog(sid).value().find(SketchConstraintId("milestone.smooth")));
}
void workflow(bool native) {
  ShellWindow w;
  REQUIRE(w.new_part("Integrated curve and Surface model"));
  REQUIRE(w.session().commit_part_transaction("Connected model", [](PartDocument& part) {
    part = f::connected_fixture();
    return Result<std::size_t>::success(1);
  }));
  add_g2(w.session());
  w.refresh();
  if (native) {
    w.resize(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800));
    w.show();
    settle();
    w.viewport().fit_all();
    capture(w, "integrated-original");
  }
  const auto before = snapshot(w.session());
  const auto cache = w.session().part_shape_cache();
  const auto* original_ptr = cache->find_body_shape(BodyId("pattern.body"));
  const auto original = *original_ptr;
  const double volume_before = blcad::test::integrated_solid_volume(original).volume_mm3;
  verify(original);
  const auto undo_label = std::string(w.session().undo_label());
  auto bad =
      w.session().commit_part_transaction("Inconsistent rail anchor", [](PartDocument& part) {
        f::edit_connected_section_and_guide(part, .8, false);
        return Result<std::size_t>::success(1);
      });
  CHECK_FALSE(bad);
  CHECK(snapshot(w.session()) == before);
  CHECK(w.session().undo_label() == undo_label);
  CHECK(w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body")) == original_ptr);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("main")));
  REQUIRE(w.feature_tools().active());
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  w.feature_tools().spatial_continuity_editor().setCurrentIndex(2);
  CHECK(w.statusBar()->currentMessage().contains("longitudinal", Qt::CaseInsensitive));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(w.feature_tools().apply());
  if (native) {
    settle();
    control<QScrollArea>(w, "blcad.feature.inputs_scroll")
        .ensureWidgetVisible(&w.feature_tools().spatial_continuity_editor(), 0, 0);
    capture(w, "integrated-rejected-global-g2");
  }
  w.feature_tools().cancel();
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("offset")));
  INFO(control<QLabel>(w, "blcad.feature.summary").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  control<QCheckBox>(w, "blcad.surface.override_formulas").setChecked(true);
  control<QComboBox>(w, "blcad.surface.side").setCurrentIndex(1);
  const auto impossible = w.feature_tools().set_numeric_value(11);
  INFO((impossible ? "unexpected success"
                   : impossible.error().object_id() + ": " + impossible.error().message()));
  REQUIRE_FALSE(impossible);
  CHECK(impossible.error().object_id() == "offset");
  auto& editor = w.feature_tools().numeric_editor();
  editor.setText("11 mm");
  REQUIRE(QMetaObject::invokeMethod(&editor, "editingFinished", Qt::DirectConnection));
  CHECK(w.statusBar()->currentMessage().endsWith(
      QString::fromStdString(impossible.error().message())));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(w.feature_tools().apply());
  if (native) {
    settle();
    control<QScrollArea>(w, "blcad.feature.inputs_scroll").ensureWidgetVisible(&editor, 0, 0);
    capture(w, "integrated-rejected-offset");
  }
  w.feature_tools().cancel();
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().commit_part_transaction("Edit middle section and constrained rail",
                                              [](PartDocument& part) {
                                                f::edit_connected_section_and_guide(part, .2);
                                                return Result<std::size_t>::success(1);
                                              }));
  const auto section_changed = snapshot(w.session());
  CHECK(f::section_error(*w.session().part_shape_cache()->find_feature_shape(FeatureId("main")),
                         .2) < 1e-5);
  verify(*w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body")));
  const double section_volume =
      blcad::test::integrated_solid_volume(
          *w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body")))
          .volume_mm3;
  REQUIRE(w.session().commit_part_transaction("Extend quilt window", [](PartDocument& part) {
    const auto changed =
        part.set_parameter_value(ParameterId("window.height"), Quantity::length_mm(6).value());
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto after = snapshot(w.session());
  w.refresh();
  const auto final = *w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body"));
  verify(final);
  const double final_volume = blcad::test::integrated_solid_volume(final).volume_mm3;
  CHECK(final_volume > section_volume);
  CHECK(std::abs(final_volume - volume_before) > 1e-4);
  CHECK(blcad::test::integrated_solid_volume(
            *w.session().part_shape_cache()->find_feature_shape(FeatureId("thick")))
            .volume_mm3 == Catch::Approx((21 * std::acos(.8) + 3) * 9).margin(1e-5));
  if (native) {
    w.viewport().fit_all();
    settle();
    capture(w, "integrated-edited");
  }
  REQUIRE(w.undo());
  CHECK(snapshot(w.session()) == section_changed);
  REQUIRE(w.undo());
  CHECK(snapshot(w.session()) == before);
  verify(*w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body")));
  REQUIRE(w.session().redo());
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto json = files.filePath("connected.json");
  REQUIRE(w.save_to(json));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(json.toStdString()));
  CHECK(snapshot(reopened) == after);
  verify(*reopened.part_shape_cache()->find_body_shape(BodyId("pattern.body")));
  CHECK(blcad::test::integrated_solid_volume(
            *reopened.part_shape_cache()->find_body_shape(BodyId("pattern.body")))
            .volume_mm3 == Catch::Approx(final_volume).margin(1e-5));
  const auto output = files.filePath("connected.step");
  REQUIRE(w.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, output.toStdString()}));
  const auto evidence = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!evidence.isEmpty()) {
    REQUIRE(QDir().mkpath(evidence));
    QFile::remove(QDir(evidence).filePath("connected.step"));
    QFile::remove(QDir(evidence).filePath("connected.json"));
    REQUIRE(QFile::copy(output, QDir(evidence).filePath("connected.step")));
    REQUIRE(QFile::copy(json, QDir(evidence).filePath("connected.json")));
  }
  QFile file(output);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  auto source = StepImportSource::create(
                    StepImportSourceId("connected"), "Connected", "connected.step",
                    step_import_sha256(std::string_view(bytes.constData(), bytes.size())).value(),
                    StepImportMode::Reference)
                    .value();
  auto imported = geometry::StepImportReader{}.read(source, files.path().toStdString());
  INFO((imported ? "imported" : imported.error().object_id() + ": " + imported.error().message()));
  REQUIRE(imported);
  auto shapes = geometry::StepImportReader{}.preview_shapes(imported.value());
  REQUIRE(shapes);
  REQUIRE(shapes.value().size() == 2);
  const auto physical = f::reimported(shapes.value());
  CHECK(physical.valid);
  CHECK(physical.material);
  CHECK(physical.solids == 2);
  CHECK(physical.stations == 32);
  CHECK(physical.boundary_error_mm < 1e-5);
  CHECK(physical.normal_error_rad < 1e-4);
  CHECK(physical.curvature_error_per_mm < 1e-4);
  CHECK(physical.blend_error_mm < 1e-5);
  CHECK(physical.volume_mm3 == Catch::Approx(final_volume).margin(5e-5));
}
} // namespace
TEST_CASE("Connected curves and Surface milestone preserves physical geometry and complete history",
          "[gui][curve-surface-milestone][parity-b174]") {
  workflow(false);
}
TEST_CASE("Native integrated airfoil duct quilt workflow survives failures edits and STEP",
          "[gui][native-curve-surface-milestone][parity-b174]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT");
  workflow(true);
}
