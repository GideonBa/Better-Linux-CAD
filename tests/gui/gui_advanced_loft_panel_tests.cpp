#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/loft_section_refinement_fixture.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
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
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace {
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* p = w.findChild<T*>(name);
  REQUIRE(p);
  return *p;
}
std::string saved(ShellWindow& w) {
  return blcad::test::surface_offset::saved(*w.session().part_document());
}
QString number(double n) {
  return QString::number(n, 'g', 17);
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(35, &loop, &QEventLoop::quit);
  loop.exec();
}
LoftFeature fixture(ShellWindow& w) {
  REQUIRE(w.new_part("Constrained airfoil"));
  auto model = blcad::test::constrained_airfoil_fixture();
  const auto recipe = *model.find_loft_feature(FeatureId("main"));
  REQUIRE(model.remove_loft_feature(recipe.id()));
  REQUIRE(model.remove_body(BodyId("main")));
  REQUIRE(w.session().commit_part_transaction("Inputs", [&](PartDocument& p) {
    p = model;
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  return recipe;
}
QComboBox& cell(QTableWidget& table, int row, int column) {
  auto* b = qobject_cast<QComboBox*>(table.cellWidget(row, column));
  REQUIRE(b);
  return *b;
}
void choose(QComboBox& box, const QString& data) {
  const int i = box.findData(data);
  REQUIRE(i >= 0);
  box.setCurrentIndex(i);
}
void author(ShellWindow& w, const LoftFeature& recipe) {
  auto& tools = w.feature_tools();
  REQUIRE(tools.begin_spatial(ShellFeatureTools::Command::Loft));
  for (const auto& section : recipe.sections()) {
    const auto& p = std::get<ProfileRegionReference>(section.source());
    REQUIRE(tools.select_spatial_profile(p.sketch(), p.profile()));
  }
  for (const auto& guide : recipe.guide_curves()) {
    choose(tools.spatial_guide_editor(), QString::fromStdString(guide.value()));
    tools.spatial_add_guide_button().click();
  }
  choose(tools.spatial_path_editor(), QString::fromStdString(recipe.path_curve()->value()));
  control<QCheckBox>(w, "blcad.loft.constraints").setChecked(true);
  choose(control<QComboBox>(w, "blcad.loft.tolerance_binding"), "tolerance");
  auto& supports = control<QTableWidget>(w, "blcad.loft.supports");
  for (std::size_t i = 0; i < recipe.constraints()->patch_boundaries.size(); ++i) {
    control<QPushButton>(w, "blcad.loft.add_support").click();
    const auto& patch = recipe.constraints()->patch_boundaries[i];
    if (i == 0)
      supports.item(0, 7)->setText("2"); // Keep the unfinished draft invalid
                                         // until all roles are explicit.
    cell(supports, i, 0).setCurrentIndex(2);
    choose(cell(supports, i, 1), QString::fromStdString(patch.condition.section_identity));
    const auto& source = std::get<ResultFaceReference>(patch.condition.support.source());
    const auto targets = f::checked(geometry::FinishingSelectionBuilder::build(
        *w.session().part_document(), *w.session().part_shape_cache(), source.body(),
        PartFeatureInputRole::LoftBoundarySupport));
    const auto target = std::find_if(targets.begin(), targets.end(), [&](const auto& t) {
      const auto* ref =
          std::get_if<ResultFaceReference>(&std::get<FaceReference>(t.reference).source());
      return ref && ref->identity() == source.identity();
    });
    REQUIRE(target != targets.end());
    const auto label = w.session().part_document()->find_body(source.body())->name() +
                       " / Support Face " +
                       std::to_string(1 + std::size_t(target - targets.begin()));
    const int selected = cell(supports, i, 2).findText(QString::fromStdString(label));
    REQUIRE(selected >= 0);
    cell(supports, i, 2).setCurrentIndex(selected);
    choose(cell(supports, i, 3), QString::fromStdString(patch.from_guide.value()));
    choose(cell(supports, i, 4), QString::fromStdString(patch.to_guide.value()));
    cell(supports, i, 5).setCurrentIndex(1);
    supports.item(i, 6)->setCheckState(Qt::Checked);
  }
  auto& anchors = control<QTableWidget>(w, "blcad.loft.anchors");
  REQUIRE(anchors.rowCount() == 6);
  int row = 0;
  for (const auto& binding : recipe.constraints()->guides)
    for (const auto& anchor : binding.anchors) {
      choose(cell(anchors, row, 2), QString::fromStdString(anchor.boundary_entity->value()));
      anchors.item(row, 3)->setText(number(anchor.boundary_fraction));
      anchors.item(row, 4)->setText(number(anchor.guide_fraction));
      ++row;
    }
  supports.item(0, 7)->setText("1");
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
}
void physical_sections(const geometry::GeometryShape& shape) {
  for (std::size_t station = 0; station < 3; ++station)
    for (bool upper : {false, true})
      for (int i = 0; i <= 8; ++i) {
        const auto p = blcad::test::loft_refinement::model(
            station, blcad::test::loft_refinement::at(
                         blcad::test::loft_refinement::poles(station, upper), i / 8.0));
        CHECK(blcad::test::distance_to_faces(shape, p) < 1e-5);
      }
  CHECK(blcad::test::classify_solid_point(shape, blcad::test::loft_refinement::model(1, {0, 0})) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape, blcad::test::loft_refinement::model(1, {0, 30})) ==
        blcad::test::SolidPointLocation::Outside);
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(QString(name) + ".png")));
}
} // namespace
TEST_CASE("Native Loft fields author explicit G2 supports and shared editing "
          "retains exact constraints",
          "[gui][advanced-loft-panels][parity-b173]") {
  ShellWindow w;
  const auto recipe = fixture(w);
  const auto before = saved(w);
  author(w, recipe);
  CHECK(saved(w) == before);
  auto& supports = control<QTableWidget>(w, "blcad.loft.supports");
  supports.item(0, 7)->setText("2");
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(w.feature_tools().apply());
  CHECK(saved(w) == before);
  supports.item(0, 7)->setText("1");
  w.feature_tools().spatial_continuity_editor().setCurrentIndex(2);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(saved(w) == before);
  w.feature_tools().spatial_continuity_editor().setCurrentIndex(0);
  const auto feedback = control<QLabel>(w, "blcad.loft.measurements").text();
  CHECK(feedback.contains("G2 joins 4"));
  CHECK(feedback.contains("Curvature error"));
  CHECK(feedback.contains("before Body operation"));
  REQUIRE(w.feature_tools().apply());
  const auto after = saved(w);
  const auto id = w.session().part_document()->loft_features().back().id();
  const auto& feature = *w.session().part_document()->find_loft_feature(id);
  const auto* shape = w.session().part_shape_cache()->find_body_shape(
      feature.body_result_context().effective_produced_body());
  REQUIRE(shape);
  physical_sections(*shape);
  REQUIRE(w.feature_edit_tools().begin(id));
  REQUIRE(w.feature_tools().active());
  CHECK(control<QTableWidget>(w, "blcad.loft.supports").rowCount() == 4);
  CHECK(control<QTableWidget>(w, "blcad.loft.anchors").rowCount() == 6);
  CHECK_FALSE(control<QCheckBox>(w, "blcad.spatial.auto_align").isChecked());
  REQUIRE(w.feature_tools().apply());
  CHECK(saved(w) == after);
  REQUIRE(w.undo());
  CHECK(saved(w) == after);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
}
TEST_CASE("Native constrained airfoil preserves physical sections through edit "
          "history and STEP",
          "[gui][advanced-loft-panels][native-advanced-loft-panels][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Loft controls");
  ShellWindow w;
  const auto recipe = fixture(w);
  w.resize(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800));
  w.show();
  settle();
  w.viewport().fit_all();
  const auto before = saved(w);
  author(w, recipe);
  auto& supports = control<QTableWidget>(w, "blcad.loft.supports");
  supports.setCurrentCell(0, 2);
  const int original_face = cell(supports, 0, 2).currentIndex();
  cell(supports, 0, 2).setCurrentIndex(-1);
  w.feature_tools().spatial_pick_editor().setCurrentIndex(3);
  settle();
  auto point = blcad::test::loft_refinement::model(
      0, blcad::test::loft_refinement::at(blcad::test::loft_refinement::poles(0, true), .5));
  point.z -= 2.5;
  const auto screen = f::checked(w.viewport().model_to_screen(point));
  const QPointF local(screen.x, screen.y);
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&w.viewport(), &event);
    qApp->processEvents();
  }
  REQUIRE(cell(supports, 0, 2).currentIndex() == original_face);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  settle();
  capture(w, "native-g2-airfoil-loft");
  auto& scroll = control<QScrollArea>(w, "blcad.feature.inputs_scroll");
  auto& anchors = control<QTableWidget>(w, "blcad.loft.anchors");
  for (auto* table : {&anchors, &supports}) {
    const auto center = table->mapTo(scroll.widget(), table->rect().center());
    scroll.ensureVisible(center.x(), center.y(),
                         std::min(table->width() / 2 + 8, scroll.viewport()->width() / 2),
                         std::min(table->height() / 2 + 8, scroll.viewport()->height() / 2));
    settle();
    table->horizontalScrollBar()->setValue(0);
    CHECK(table->visibleRegion().boundingRect().height() >
          table->horizontalHeader()->height() + table->rowHeight(0));
    capture(w, table == &anchors ? "native-loft-anchor-fields" : "native-loft-support-fields");
    table->horizontalScrollBar()->setValue(table->horizontalScrollBar()->maximum());
    capture(w,
            table == &anchors ? "native-loft-anchor-fractions" : "native-loft-boundary-continuity");
  }
  scroll.ensureWidgetVisible(&control<QLabel>(w, "blcad.loft.measurements"), 0, 0);
  settle();
  capture(w, "native-loft-measurements");

  auto& apply = control<QPushButton>(w, "blcad.feature.apply");
  REQUIRE(apply.isVisible());
  REQUIRE(w.childAt(apply.mapTo(&w, apply.rect().center())) == &apply);
  apply.click();
  REQUIRE_FALSE(w.feature_tools().active());
  const auto after = saved(w);
  const auto id = w.session().part_document()->loft_features().back().id();
  REQUIRE(w.feature_edit_tools().begin(id));
  CHECK(control<QLabel>(w, "blcad.loft.measurements").text().contains("G2 joins 4"));
  w.feature_tools().cancel();
  CHECK(saved(w) == after);
  REQUIRE(w.undo());
  CHECK(saved(w) == before);
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto json = files.filePath("airfoil.json");
  REQUIRE(w.save_to(json));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(json.toStdString()));
  CHECK(blcad::test::surface_offset::saved(*opened.part_document()) == after);
  const auto path = files.filePath("airfoil.step");
  REQUIRE(w.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}));
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("airfoil"), "Airfoil", "airfoil.step",
      f::checked(step_import_sha256(std::string_view(bytes.constData(), bytes.size()))),
      StepImportMode::Reference));
  const auto imported =
      f::checked(geometry::StepImportReader{}.read(source, files.path().toStdString()));
  const auto shapes = f::checked(geometry::StepImportReader{}.preview_shapes(imported));
  std::size_t solids = 0;
  for (const auto& shape : shapes)
    if (geometry::RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1) {
      ++solids;
      physical_sections(shape);
    }
  CHECK(solids == 1);
  CHECK(saved(w) == after);
}
