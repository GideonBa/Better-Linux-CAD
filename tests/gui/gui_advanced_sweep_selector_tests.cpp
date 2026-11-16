#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
using C = ShellFeatureTools::Command;
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* p = w.findChild<T*>(name);
  REQUIRE(p);
  return *p;
}
std::string saved(ShellWindow& w) {
  return blcad::test::surface_offset::saved(*w.session().part_document());
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(35, &loop, &QEventLoop::quit);
  loop.exec();
}
void show(ShellWindow& w) {
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  w.resize(size.boundedTo(w.screen()->availableGeometry().size() - QSize(40, 40)));
  w.show();
  settle();
  w.viewport().fit_all();
}
void fixture(ShellWindow& w, bool touching = false) {
  REQUIRE(w.new_part("Material Sweep"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [&](PartDocument& p) {
    p = sf::straight_sweep();
    f::checked(p.remove_sweep_feature(FeatureId("driven.sweep")));
    f::checked(p.remove_body(BodyId("driven.body")));
    f::length(p, "outer.diameter", 4);
    f::length(p, "inner.diameter", touching ? 4 : 2);
    auto sketch =
        f::checked(Sketch::create(SketchId("sweep.profile"), "Hollow", DatumPlaneId("datum.xy")));
    f::checked(sketch.add_profile(
        f::checked(CircleProfile::create(ProfileId("outer"), ParameterId("outer.diameter")))));
    f::checked(sketch.add_profile(
        f::checked(CircleProfile::create(ProfileId("inner"), ParameterId("inner.diameter")))));
    f::checked(p.update_sketch(sketch));
    return p.set_body_visibility(BodyId("base.body"), BodyVisibility::Hidden);
  }));
  w.refresh();
}
void sweep(ShellWindow& w) {
  REQUIRE(w.feature_tools().begin_spatial(C::Sweep));
  auto& source = control<QComboBox>(w, "blcad.spatial.source");
  const auto row = source.findText("Hollow / outer");
  REQUIRE(row >= 0);
  source.setCurrentIndex(row);
  auto& path = control<QComboBox>(w, "blcad.spatial.path");
  const auto trajectory = path.findData("driven.path");
  REQUIRE(trajectory >= 0);
  path.setCurrentIndex(trajectory);
}
void capture(ShellWindow& w, const QString& name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(name + ".png")));
}
void roundtrip(ShellWindow& w, double expected, Point3 inside, Point3 outside) {
  const auto after = saved(w);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto json = files.filePath("selector.json");
  REQUIRE(w.save_to(json));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(json.toStdString()));
  CHECK(blcad::test::surface_offset::saved(*opened.part_document()) == after);
  const auto step = files.filePath("selector.step");
  REQUIRE(w.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, step.toStdString()}));
  QFile file(step);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("selector"), "Selector", "selector.step",
      f::checked(step_import_sha256(std::string_view(bytes.constData(), bytes.size()))),
      StepImportMode::Reference));
  const auto imported =
      f::checked(geometry::StepImportReader{}.read(source, files.path().toStdString()));
  const auto shapes = f::checked(geometry::StepImportReader{}.preview_shapes(imported));
  std::size_t count = 0;
  for (const auto& shape : shapes)
    if (geometry::RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1) {
      ++count;
      CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
            Catch::Approx(expected).margin(1e-5));
      CHECK(blcad::test::classify_solid_point(shape, inside) ==
            blcad::test::SolidPointLocation::Inside);
      CHECK(blcad::test::classify_solid_point(shape, outside) ==
            blcad::test::SolidPointLocation::Outside);
    }
  CHECK(count == 1);
  CHECK(saved(w) == after);
}
} // namespace
TEST_CASE("Sweep region controls preserve actual holes and reject touching contours before Apply",
          "[gui][advanced-sweep-selectors][parity-b173]") {
  SECTION("Material region and mode changes") {
    ShellWindow w;
    fixture(w);
    const auto before = saved(w);
    sweep(w);
    control<QComboBox>(w, "blcad.sweep.region_policy").setCurrentIndex(1);
    auto& holes = control<QListWidget>(w, "blcad.sweep.preserved_holes");
    REQUIRE(holes.count() == 1);
    CHECK(holes.item(0)->text() == "inner");
    REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
    CHECK_FALSE(w.viewport().scene_presentation_visible("body/base.body"));
    control<QComboBox>(w, "blcad.sweep.region_policy").setCurrentIndex(0);
    REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
    control<QComboBox>(w, "blcad.sweep.region_policy").setCurrentIndex(1);
    CHECK(saved(w) == before);
    REQUIRE(w.feature_tools().apply());
    const auto& feature = w.session().part_document()->sweep_features().back();
    CHECK(feature.profile().region_policy() == SweepRegionPolicy::MaterialRegion);
    const auto* shape = w.session().part_shape_cache()->find_body_shape(
        feature.body_result_context().effective_produced_body());
    REQUIRE(shape);
    CHECK(blcad::test::integrated_solid_volume(*shape).volume_mm3 ==
          Catch::Approx(30 * std::numbers::pi).margin(1e-5));
    CHECK(blcad::test::classify_solid_point(*shape, {0, 0, 5}) ==
          blcad::test::SolidPointLocation::Outside);
    const auto after = saved(w);
    REQUIRE(w.undo());
    CHECK(saved(w) == before);
    REQUIRE(w.session().redo());
    CHECK(saved(w) == after);
  }
  SECTION("Touching loops remain an explicit failure") {
    ShellWindow w;
    fixture(w, true);
    const auto before = saved(w);
    sweep(w);
    control<QComboBox>(w, "blcad.sweep.region_policy").setCurrentIndex(1);
    CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
    CHECK(saved(w) == before);
    w.feature_tools().cancel();
    CHECK(saved(w) == before);
  }
}
TEST_CASE("Current Body edges author native paths and Cancel removes temporary selection geometry",
          "[gui][advanced-sweep-selectors][parity-b173]") {
  ShellWindow w;
  REQUIRE(w.new_part("Native paths"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::part();
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  auto& viewport = w.viewport();
  viewport.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  REQUIRE(viewport.select_semantic("body/source.body"));
  const auto previous = w.session().selection();
  const auto mask = viewport.selection_filter_mask();
  const auto count = viewport.presentation_count();
  const auto before = saved(w);
  const auto edges = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("source.body"),
      PartFeatureInputRole::PathSegment));
  const auto edge = std::find_if(edges.begin(), edges.end(), [](const auto& target) {
    const auto* ref = std::get_if<EdgeReference>(&target.reference);
    if (!ref)
      return false;
    const auto* curve = std::get_if<ResultEdgeReference>(&ref->source());
    return curve && curve->curve_type() == 1;
  });
  REQUIRE(edge != edges.end());
  REQUIRE(w.feature_tools().begin_spatial(C::PathCurve));
  control<QComboBox>(w, "blcad.spatial.execution_policy").setCurrentIndex(1);
  REQUIRE(viewport.select_semantic(edge->semantic_id));
  CHECK(control<QListWidget>(w, "blcad.spatial.order").count() == 1);
  CHECK(viewport.presentation_count() > count);
  w.feature_tools().cancel();
  CHECK(saved(w) == before);
  CHECK(viewport.presentation_count() == count);
  CHECK(viewport.selection_filter_mask() == mask);
  CHECK(w.session().selection().size() == previous.size());
  for (const auto& entry : previous.entries())
    CHECK(w.session().selection().contains(entry.kind, entry.semantic_id));
}
TEST_CASE("Native hollow Sweep controls retain material sections through physical STEP exchange",
          "[gui][advanced-sweep-selectors][native-advanced-sweep-selectors][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Sweep controls");
  ShellWindow w;
  fixture(w);
  show(w);
  const auto before = saved(w);
  sweep(w);
  control<QComboBox>(w, "blcad.sweep.region_policy").setCurrentIndex(1);
  settle();
  REQUIRE(control<QListWidget>(w, "blcad.sweep.preserved_holes").count() == 1);
  capture(w, "native-material-region-sweep");
  auto& commit = control<QPushButton>(w, "blcad.feature.apply");
  REQUIRE(commit.isVisible());
  REQUIRE(w.childAt(commit.mapTo(&w, commit.rect().center())) == &commit);
  commit.click();
  REQUIRE_FALSE(w.feature_tools().active());
  const auto after = saved(w);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
  roundtrip(w, 30 * std::numbers::pi, {1.5, 0, 5}, {0, 0, 5});
}
TEST_CASE("Native model edge clicks create a curve-preserving arc path with exact history",
          "[gui][advanced-sweep-selectors][native-advanced-sweep-selectors][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native edge picking");
  ShellWindow w;
  REQUIRE(w.new_part("Native arc path"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::part();
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  show(w);
  const auto before = saved(w);
  const auto edges = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("source.body"),
      PartFeatureInputRole::PathSegment));
  const auto edge = std::find_if(edges.begin(), edges.end(), [](const auto& target) {
    const auto* ref = std::get_if<EdgeReference>(&target.reference);
    if (!ref)
      return false;
    const auto* curve = std::get_if<ResultEdgeReference>(&ref->source());
    return curve && curve->curve_type() == 1 &&
           std::all_of(curve->samples().begin(), curve->samples().end(),
                       [](Point3 p) { return std::abs(p.z - 20) < 1e-7; });
  });
  REQUIRE(edge != edges.end());
  const auto witness =
      std::get<ResultEdgeReference>(std::get<EdgeReference>(edge->reference).source());
  REQUIRE(w.feature_tools().begin_spatial(C::PathCurve));
  control<QComboBox>(w, "blcad.spatial.execution_policy").setCurrentIndex(1);
  settle();
  const auto p = f::checked(w.viewport().model_to_screen(witness.samples()[8]));
  const QPointF local(p.x, p.y);
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&w.viewport(), &event);
    qApp->processEvents();
  }
  REQUIRE(control<QListWidget>(w, "blcad.spatial.order").count() == 1);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(control<QComboBox>(w, "blcad.spatial.source").currentText() ==
        QString::fromStdString(edge->label));
  capture(w, "native-body-arc-path");
  REQUIRE(w.feature_tools().apply());
  const auto& path = w.session().part_document()->path_curves().back();
  CHECK(path.execution_policy().kind() == PathGeometryPolicyKind::CurvePreserving);
  REQUIRE(path.segments().size() == 1);
  CHECK(path.segments().front().source_kind() == PathSegmentSourceKind::FeatureEdge);
  const auto spans = f::checked(geometry::GeometryRecomputeExecutor{}.resolve_path_segments(
      *w.session().part_document(), path.id(), *w.session().part_shape_cache()));
  REQUIRE(spans.size() == 1);
  CHECK(std::abs(spans.front().start.z - 20) < 1e-7);
  CHECK(std::abs(spans.front().end.z - 20) < 1e-7);
  CHECK(f::checked(geometry::SweepAdapter{}.measure_path_length(path.id(), spans)) ==
        Catch::Approx(5 * std::numbers::pi).margin(1e-7));
  const auto after = saved(w);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
}
