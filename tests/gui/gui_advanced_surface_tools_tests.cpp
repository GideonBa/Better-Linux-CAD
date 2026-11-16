#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace {
using C = ShellFeatureTools::Command;
template <class T> T& control(ShellWindow& window, const char* name) {
  auto* value = window.findChild<T*>(name);
  REQUIRE(value);
  return *value;
}
std::string saved(ShellWindow& window) {
  return blcad::test::surface_offset::saved(*window.session().part_document());
}
void settle() {
  QEventLoop loop;
  QTimer::singleShot(35, &loop, &QEventLoop::quit);
  loop.exec();
}
void fixture(ShellWindow& window, bool stitch = false) {
  REQUIRE(window.new_part("Advanced Surface"));
  REQUIRE(window.session().commit_part_transaction("Inputs", [&](PartDocument& p) {
    p = f::combination();
    std::vector<BodyId> hidden;
    for (const auto& body : p.bodies())
      if (stitch ? (body.id() != BodyId("left.extend.body") && body.id() != BodyId("tangent.body"))
                 : body.id() != BodyId("quilt.body"))
        hidden.push_back(body.id());
    for (const auto& id : hidden)
      f::checked(p.set_body_visibility(id, BodyVisibility::Hidden));
    return Result<std::size_t>::success(1);
  }));
  window.refresh();
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  window.resize(size.boundedTo(window.screen()->availableGeometry().size() - QSize(40, 40)));
  window.show();
  settle();
  window.viewport().fit_all();
}
void choose(QComboBox& box, const QString& label) {
  const auto row = box.findText(label);
  INFO(label.toStdString());
  REQUIRE(row >= 0);
  box.setCurrentIndex(row);
}
double area() {
  return (20 * std::acos(.8) + 3) * 7;
}
double inside() {
  return (9.75 * std::acos(.8) + .5 * (3 * std::sqrt(91.) + 100 * std::asin(.3)) - 28.5) * 7;
}
double volume(ShellWindow& window, BodyId id) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(id);
  REQUIRE(shape);
  return blcad::test::integrated_solid_volume(*shape).volume_mm3;
}
QPushButton& apply(ShellWindow& window) {
  return control<QPushButton>(window, "blcad.feature.apply");
}
void capture(ShellWindow& window, const QString& name) {
  const auto path = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (path.isEmpty())
    return;
  REQUIRE(QDir().mkpath(path));
  CHECK(window.grab().save(QDir(path).filePath(name + ".png")));
}
} // namespace
TEST_CASE(
    "Advanced Surface controls execute every thickness Body mode with independent material volumes",
    "[gui][advanced-surface-tools][parity-b173]") {
  const auto mode = GENERATE(0, 1, 2, 3);
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = saved(window);
  REQUIRE(tools.begin_surface(C::Thicken));
  choose(tools.surface_target_editor(), "Quilt");
  control<QComboBox>(window, "blcad.surface.side").setCurrentIndex(2);
  control<QComboBox>(window, "blcad.surface.operation").setCurrentIndex(mode);
  auto& target = control<QComboBox>(window, "blcad.surface.operation_target");
  if (mode) {
    const auto row = target.findData("source.body");
    REQUIRE(row >= 0);
    target.setCurrentIndex(row);
  }
  INFO("Body mode " << mode);
  INFO(control<QLabel>(window, "blcad.feature.summary").text().toStdString());
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(apply(window).isEnabled());
  CHECK(saved(window) == before);
  REQUIRE(tools.apply());
  const auto& feature =
      std::get<SurfaceOffsetFeature>(window.session().part_document()->surface_features().back());
  CHECK(int(feature.body_result_context().operation_mode()) == mode);
  CHECK(feature.parameters().side == SurfaceOffsetSide::Symmetric);
  CHECK_FALSE(feature.parameters().maximum_deviation);
  const double base = 2000 * std::numbers::pi;
  const auto expected = mode == 0   ? area()
                        : mode == 1 ? base + area() - inside()
                        : mode == 2 ? base - inside()
                                    : inside();
  CHECK(volume(window, feature.result_body()) == Catch::Approx(expected).margin(5e-5));
  CHECK(volume(window, BodyId("thick.body")) == Catch::Approx(area()).margin(5e-5));
  const auto after = saved(window);
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
  REQUIRE(window.session().redo());
  CHECK(saved(window) == after);
}
TEST_CASE("Advanced Surface inactive budgets and Cancel preserve source selection and valid model",
          "[gui][advanced-surface-tools][parity-b173]") {
  ShellWindow window;
  fixture(window);
  auto& viewport = window.viewport();
  auto& tools = window.feature_tools();
  const GuiSelection previous{GuiSelectionKind::Body, "body/quilt.body"};
  REQUIRE(window.session().selection().add(previous));
  viewport.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  viewport.set_selection_semantic_scope(std::vector<std::string>{previous.semantic_id});
  REQUIRE(viewport.select_semantic(previous.semantic_id));
  const auto mask = viewport.selection_filter_mask();
  const auto scope = viewport.selection_semantic_scope();
  const auto before = saved(window);
  REQUIRE(tools.begin_surface(C::Thicken));
  choose(tools.surface_target_editor(), "Quilt");
  auto& approximation = control<QComboBox>(window, "blcad.surface.approximation");
  auto& deviation = control<QLineEdit>(window, "blcad.surface.deviation");
  deviation.setText("invalid");
  approximation.setCurrentIndex(1);
  CHECK_FALSE(apply(window).isEnabled());
  CHECK_FALSE(viewport.feature_preview_active());
  CHECK(viewport.geometry_diagnostic_count() > 0);
  CHECK(saved(window) == before);
  approximation.setCurrentIndex(0);
  REQUIRE(apply(window).isEnabled());
  CHECK(deviation.isHidden());
  CHECK(viewport.feature_preview_active());
  control<QLineEdit>(window, "blcad.surface.tolerance").setText("0");
  CHECK_FALSE(apply(window).isEnabled());
  control<QLineEdit>(window, "blcad.surface.tolerance").setText("0.000001");
  REQUIRE(apply(window).isEnabled());
  tools.cancel();
  CHECK(saved(window) == before);
  CHECK(viewport.geometry_diagnostic_count() == 0);
  CHECK(viewport.selection_filter_mask() == mask);
  CHECK(viewport.selection_semantic_scope() == scope);
  CHECK(window.session().selection().contains(previous.kind, previous.semantic_id));
}
TEST_CASE("Native Stitch to symmetric Thicken retains exact release, history and physical STEP",
          "[gui][advanced-surface-tools][native-advanced-surface][parity-b173]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT picking and precise release");
  ShellWindow window;
  fixture(window, true);
  auto& tools = window.feature_tools();
  const auto original = saved(window);
  for (int tab = 0; tab < window.ribbon().tab_count(); ++tab)
    if (window.ribbon().tab_title(tab) == "Surface")
      window.ribbon().set_current_tab(tab);
  REQUIRE(tools.begin_surface(C::SurfaceStitch));
  for (const auto& label : {QString("left.extend"), QString("tangent")}) {
    choose(tools.surface_target_editor(), label);
    REQUIRE(tools.append_surface_input(tools.surface_target_editor().currentData().toInt()));
  }
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto quilt_name =
      surface_feature_name(window.session().part_document()->surface_features().back());
  const auto stitched = saved(window);
  REQUIRE(tools.begin_surface(C::Thicken));
  choose(tools.surface_target_editor(), QString::fromStdString(quilt_name));
  control<QComboBox>(window, "blcad.surface.side").setCurrentIndex(2);
  REQUIRE(window.viewport().feature_preview_active());
  settle();
  CHECK(window.viewport().geometry_diagnostic_count() > 0);
  CHECK(saved(window) == stitched);
  const auto handles = tools.manipulators().presentations();
  REQUIRE(handles.size() == 1);
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point, Qt::MouseButtons buttons) {
    const QPointF local(point.x, point.y);
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  };
  const auto endpoint = handles.front().endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE(tools.manipulators().active_handle_id() == kSurfaceOffsetHandleId);
  const GuiSketchScreenPoint farther{endpoint.x + (endpoint.x - handles.front().anchor.x) / 2,
                                     endpoint.y + (endpoint.y - handles.front().anchor.y) / 2};
  send(QEvent::MouseMove, farther, Qt::LeftButton);
  constexpr double distance = 1.23456789012345;
  REQUIRE(tools.set_numeric_text("1.23456789012345 mm"));
  CHECK(tools.coordinator().surface().distance() == distance);
  send(QEvent::MouseButtonRelease, farther, Qt::NoButton);
  CHECK(tools.coordinator().surface().distance() == distance);
  CHECK(saved(window) == stitched);
  capture(window, "advanced-symmetric-thicken");
  auto& commit = apply(window);
  REQUIRE(commit.isEnabled());
  REQUIRE(commit.isVisible());
  REQUIRE(window.childAt(commit.mapTo(&window, commit.rect().center())) == &commit);
  commit.click();
  REQUIRE_FALSE(tools.active());
  const auto feature =
      std::get<SurfaceOffsetFeature>(window.session().part_document()->surface_features().back());
  const auto body = feature.result_body();
  const auto after = saved(window);
  CHECK(volume(window, body) == Catch::Approx(distance * area()).margin(5e-5));
  REQUIRE(window.undo());
  CHECK(saved(window) == stitched);
  REQUIRE(window.undo());
  CHECK(saved(window) == original);
  REQUIRE(window.session().redo());
  REQUIRE(window.session().redo());
  CHECK(saved(window) == after);
  QTemporaryDir files;
  REQUIRE(files.isValid());
  const auto part = files.filePath("surface.json");
  REQUIRE(window.save_to(part));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(part.toStdString()));
  CHECK(blcad::test::surface_offset::saved(*opened.part_document()) == after);
  const auto output = files.filePath("surface.step");
  REQUIRE(
      window.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, output.toStdString()}));
  QFile file(output);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("roundtrip"), "Roundtrip", "surface.step",
      f::checked(step_import_sha256(std::string_view(bytes.constData(), bytes.size()))),
      StepImportMode::Reference));
  const auto geometry =
      f::checked(blcad::geometry::StepImportReader{}.read(source, files.path().toStdString()));
  const auto shapes = f::checked(blcad::geometry::StepImportReader{}.preview_shapes(geometry));
  std::size_t solids = 0;
  for (const auto& shape : shapes)
    if (blcad::geometry::RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1) {
      ++solids;
      CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
            Catch::Approx(distance * area()).margin(5e-5));
      CHECK(blcad::test::classify_solid_point(shape, {10, 0, 10}) ==
            blcad::test::SolidPointLocation::Inside);
      CHECK(blcad::test::classify_solid_point(shape, {12, 0, 10}) ==
            blcad::test::SolidPointLocation::Outside);
    }
  CHECK(solids == 1);
  CHECK(saved(window) == after);
}
