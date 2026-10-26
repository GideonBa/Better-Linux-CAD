#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cmath>

using namespace blcad;
using namespace blcad::gui;
namespace {
using K = geometry::MeasureSelectionKind;
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
void fixture(ShellWindow& window) {
  REQUIRE(window.new_part("Measure inputs"));
  auto setup =
      window.session().commit_part_transaction("Measurement solids", [](PartDocument& part) {
        for (auto [id, value] : std::vector<std::pair<std::string, double>>{
                 {"width", 10}, {"depth", 20}, {"diameter", 8}})
          REQUIRE(part.add_parameter(checked(
              Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
        for (const std::string name : {"box", "cylinder"}) {
          auto sketch =
              checked(Sketch::create(SketchId("sketch." + name), name, DatumPlaneId("datum.xy")));
          if (name == "box")
            REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
                ProfileId("profile.box"), ParameterId("width"), ParameterId("width")))));
          else
            REQUIRE(sketch.add_profile(checked(CircleProfile::create(
                ProfileId("profile.cylinder"), ParameterId("diameter"), {30, 0}))));
          REQUIRE(part.add_sketch(std::move(sketch)));
          REQUIRE(part.add_body(checked(Body::create(BodyId(name), name, BodyKind::Solid))));
          auto feature = checked(Feature::create_additive_extrude(FeatureId("feature." + name),
                                                                  name, SketchId("sketch." + name),
                                                                  ParameterId("depth")));
          auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                  std::nullopt, BodyId(name)));
          REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
        }
        return Result<std::size_t>::success(1);
      });
  REQUIRE(setup);
  window.refresh();
  window.resize(1280, 960);
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  qApp->processEvents();
}
QAction& action(ShellWindow& window, const char* id) {
  auto* found = window.measure_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(found);
  return *found;
}
QPushButton& button(ShellWindow& window, const char* id) {
  auto* found = window.findChild<QPushButton*>(QStringLiteral("blcad.measure.") + id);
  REQUIRE(found);
  return *found;
}
template <typename Predicate>
std::string target(ShellMeasureTools& tools, K kind, Predicate predicate) {
  const auto& targets = tools.targets();
  auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& item) {
    return item.kind == kind && item.label.starts_with("box") && predicate(item);
  });
  REQUIRE(found != targets.end());
  return found->semantic_id;
}
std::string plane(ShellMeasureTools& tools, double z) {
  return target(tools, K::Plane, [=](const auto& item) {
    const auto& descriptor = std::get<geometry::AssemblyPlanarTargetDescriptor>(*item.descriptor);
    return std::abs(descriptor.normal.z) > .99 && std::abs(item.anchor.z - z) < 1e-6;
  });
}
void select(ShellMeasureTools& tools, std::size_t role, const std::string& identity) {
  const auto& targets = tools.targets();
  auto found = std::find_if(targets.begin(), targets.end(),
                            [&](const auto& item) { return item.semantic_id == identity; });
  REQUIRE(found != targets.end());
  const auto index = std::distance(targets.begin(), found);
  auto& editor = tools.target_editor(role);
  const int row = editor.findData(static_cast<int>(index));
  REQUIRE(row > 0);
  editor.setCurrentIndex(row);
}
void capture(ShellWindow& window, const QString& name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  CHECK(window.grab().save(QDir(directory).filePath(name + ".png")));
}
} // namespace
TEST_CASE("Measure distance selects displayed faces and leaves document history and saved bytes "
          "unchanged",
          "[gui][measure][block136-measure]") {
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("before.blcad.json")));
  const auto before = snapshot(window);
  const auto undo_label = std::string(window.session().undo_label());
  const bool redo = window.session().can_redo();
  auto& tools = window.measure_tools();
  REQUIRE(action(window, "measure_distance").isEnabled());
  action(window, "measure_distance").trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.result());
  REQUIRE(window.viewport().select_semantic(plane(tools, 0)));
  CHECK(tools.pick_editor().currentIndex() == 1);
  REQUIRE(window.viewport().select_semantic(plane(tools, 20)));
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(20));
  REQUIRE(tools.result()->overlay_points.size() == 2);
  CHECK(tools.result()->overlay_points[0].z == Catch::Approx(0));
  CHECK(tools.result()->overlay_points[1].z == Catch::Approx(20));
  button(window, "copy").click();
  CHECK(QApplication::clipboard()->text() == "Distance: 20 mm");
  CHECK(snapshot(window) == before);

  CHECK_FALSE(window.session().dirty());
  capture(window, "measure-distance");
  REQUIRE(window.save_to(directory.filePath("after.blcad.json")));
  QFile original(directory.filePath("before.blcad.json")),
      saved(directory.filePath("after.blcad.json"));
  REQUIRE(original.open(QIODevice::ReadOnly));
  REQUIRE(saved.open(QIODevice::ReadOnly));
  CHECK(original.readAll() == saved.readAll());
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(directory.filePath("after.blcad.json").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*opened.part_document())) == before);
  tools.cancel();
  CHECK_FALSE(tools.active());
  CHECK(window.session().undo_label() == undo_label);
  CHECK(window.session().can_redo() == redo);
  CHECK(snapshot(window) == before);
}
TEST_CASE("Measure kind controls report angles radii diameters and Body volume",
          "[gui][measure][block136-measure]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.measure_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(GuiMeasureKind::Angle));
  select(tools, 0, plane(tools, 20));
  const auto side = target(tools, K::Plane, [](const auto& item) {
    return std::abs(std::get<geometry::AssemblyPlanarTargetDescriptor>(*item.descriptor).normal.x) >
           .99;
  });
  select(tools, 1, side);
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(90));
  select(tools, 1, plane(tools, 20));
  CHECK_FALSE(tools.result());
  CHECK_FALSE(button(window, "copy").isEnabled());
  CHECK(window.statusBar()->currentMessage().isEmpty());
  tools.kind_editor().setCurrentIndex(static_cast<int>(GuiMeasureKind::RadiusDiameter));
  for (auto kind : {K::Circle, K::Cylinder}) {
    auto found = std::find_if(tools.targets().begin(), tools.targets().end(),
                              [=](const auto& item) { return item.kind == kind; });
    REQUIRE(found != tools.targets().end());
    select(tools, 0, found->semantic_id);
    REQUIRE(tools.result());
    CHECK(tools.result()->primary_value == Catch::Approx(4));
    CHECK(tools.result()->secondary_value == Catch::Approx(8));
  }
  capture(window, "measure-radius");
  tools.kind_editor().setCurrentIndex(static_cast<int>(GuiMeasureKind::BodySummary));
  REQUIRE(window.viewport().select_semantic("body/box"));
  REQUIRE(tools.result());
  CHECK(tools.result()->volume_mm3 == Catch::Approx(2000));
  CHECK(tools.result()->solid_count == 1);
  capture(window, "measure-body");
  button(window, "clear").click();
  CHECK_FALSE(tools.result());
  CHECK(tools.target_editor(0).currentIndex() == 0);
  CHECK(snapshot(window) == before);
}
TEST_CASE("Measure invalidates stale and replaced topology instead of reusing old descriptors",
          "[gui][measure][block136-measure]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.measure_tools();
  REQUIRE(tools.begin(GuiMeasureKind::Distance));
  const auto old = plane(tools, 20);
  select(tools, 0, plane(tools, 0));
  select(tools, 1, old);
  REQUIRE(tools.result());
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(tools.result());
  CHECK(tools.targets().empty());
  CHECK_FALSE(button(window, "copy").isEnabled());
  CHECK(window.statusBar()->currentMessage().isEmpty());
  window.session().set_derived_results_fresh(true);
  window.refresh();
  CHECK_FALSE(tools.result());
  CHECK(plane(tools, 20) != old);
  CHECK_FALSE(window.viewport().select_semantic(old));
  const auto before = snapshot(window);
  const auto resized = window.session().commit_part_transaction("Resize", [](PartDocument& part) {
    auto changed = part.set_parameter_value(ParameterId("depth"), Quantity::length_mm(40).value());
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  });
  REQUIRE(resized);
  window.refresh();
  CHECK_FALSE(tools.result());
  select(tools, 0, plane(tools, 0));
  select(tools, 1, plane(tools, 40));
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(40));
  REQUIRE(window.undo());
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK_FALSE(tools.active());
}
TEST_CASE("Measure filters and every exit restore selection scope without a model transaction",
          "[gui][measure][block136-measure]") {
  int exit = 0;
  SECTION("Close button") {}
  SECTION("Escape") {
    exit = 1;
  }
  SECTION("Dock close") {
    exit = 2;
  }
  ShellWindow window;
  fixture(window);
  auto& tools = window.measure_tools();
  const auto before = snapshot(window);
  window.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/box"});
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  REQUIRE(tools.begin(GuiMeasureKind::Distance));
  tools.filter_editor().setCurrentIndex(1);
  CHECK_FALSE(window.viewport().select_semantic(plane(tools, 20)));
  const auto point = target(tools, K::Point, [](const auto&) { return true; });
  REQUIRE(window.viewport().select_semantic(point));
  auto* overlay = window.viewport().findChild<QWidget*>("blcad.measure.overlay");
  REQUIRE(overlay);
  CHECK(overlay->isVisible());
  if (exit == 0)
    button(window, "close").click();
  else if (exit == 1) {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &escape);
  } else {
    auto* dock = window.findChild<QDockWidget*>("blcad.shell.measure_task");
    REQUIRE(dock);
    dock->close();
  }
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(overlay->isVisible());
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(snapshot(window) == before);
}
TEST_CASE("Measure rejects empty stale and busy contexts and preserves the active feature task",
          "[gui][measure][block136-measure]") {
  ShellWindow empty;
  CHECK_FALSE(empty.measure_tools().begin(GuiMeasureKind::Distance));
  REQUIRE(empty.new_part("Empty"));
  CHECK_FALSE(action(empty, "measure_distance").isEnabled());
  CHECK_FALSE(empty.measure_tools().begin(GuiMeasureKind::Distance));
  ShellWindow window;
  fixture(window);
  const auto before = snapshot(window);
  REQUIRE(window.feature_tools().begin_body_operation(ShellFeatureTools::Command::BodyTranslate));
  const auto task = std::string(window.session().task().command_id());
  CHECK_FALSE(window.measure_tools().begin(GuiMeasureKind::Distance));
  CHECK(window.session().task().command_id() == task);
  window.feature_tools().cancel();
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(action(window, "measure_distance").isEnabled());
  CHECK_FALSE(window.measure_tools().begin(GuiMeasureKind::Distance));
  CHECK(snapshot(window) == before);
}
TEST_CASE("Measure reads posed assembly components in root coordinates",
          "[gui][measure][block136-measure]") {
  ShellWindow window;
  REQUIRE(window.open_path(QStringLiteral(BLCAD_SOURCE_DIR) +
                           "/examples/revolute_joint.blcad.project.json"));
  REQUIRE(window.session().commit_project_transaction("Place free component", [](Project& project) {
    return project.assembly().set_component_instance_transform(
        ComponentInstanceId("component.plate.free"), {{100, 0, 0}, {}});
  }));
  window.refresh();
  const auto before = checked(serialize_project_to_json(*window.session().project()));
  auto& tools = window.measure_tools();
  REQUIRE(tools.begin(GuiMeasureKind::Distance));
  auto first = std::find_if(tools.targets().begin(), tools.targets().end(), [](const auto& item) {
    return item.kind == K::Point && item.label.starts_with("Grounded Plate");
  });
  REQUIRE(first != tools.targets().end());
  auto second = std::find_if(tools.targets().begin(), tools.targets().end(), [&](const auto& item) {
    return item.kind == K::Point && item.label.starts_with("Free Plate") &&
           distance(item.anchor, {first->anchor.x + 100, first->anchor.y, first->anchor.z}) < 1e-6;
  });
  REQUIRE(second != tools.targets().end());
  select(tools, 0, first->semantic_id);
  select(tools, 1, second->semantic_id);
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(100));
  tools.cancel();
  CHECK(checked(serialize_project_to_json(*window.session().project())) == before);
  CHECK_FALSE(action(window, "measure_body").isEnabled());
}
TEST_CASE(
    "Native Measure picks model vertices and follows camera changes without modifying the part",
    "[gui][measure][block136-measure][native-shell-measure]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT measurement picks");
  ShellWindow window;
  fixture(window);
  auto& tools = window.measure_tools();
  const auto before = snapshot(window);
  auto& distance_action = action(window, "measure_distance");
  QToolButton* open = nullptr;
  for (auto* candidate : window.findChildren<QToolButton*>())
    if (candidate->defaultAction() == &distance_action)
      open = candidate;
  REQUIRE(open);
  QWidget* ancestor = open->parentWidget();
  while (ancestor && !qobject_cast<QScrollArea*>(ancestor))
    ancestor = ancestor->parentWidget();
  REQUIRE(ancestor);
  static_cast<QScrollArea*>(ancestor)->ensureWidgetVisible(open);
  qApp->processEvents();
  REQUIRE(open->isVisible());
  open->click();
  REQUIRE(tools.active());
  tools.filter_editor().setCurrentIndex(1);
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  const auto click = [&](Point3 point) {
    const auto screen = checked(window.viewport().model_to_screen(point));
    const QPointF local(screen.x, screen.y);
    QMouseEvent press(QEvent::MouseButtonPress, local,
                      window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, local,
                        window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &press);
    QApplication::sendEvent(&window.viewport(), &release);
    qApp->processEvents();
  };
  click({-5, -5, 20});
  REQUIRE(tools.target_editor(0).currentIndex() > 0);
  click({5, -5, 20});
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(10));
  const auto first = tools.target_editor(0).currentData().toInt();
  const auto second = tools.target_editor(1).currentData().toInt();
  CHECK(tools.targets().at(first).anchor.z == Catch::Approx(20));
  CHECK(tools.targets().at(second).anchor.z == Catch::Approx(20));
  auto* overlay = window.viewport().findChild<QWidget*>("blcad.measure.overlay");
  REQUIRE(overlay);
  REQUIRE(overlay->isVisible());
  capture(window, "native-measure-distance");
  window.viewport().set_standard_view(GuiStandardView::Front);
  qApp->processEvents();
  CHECK(overlay->isVisible());
  CHECK(tools.result()->primary_value == Catch::Approx(10));
  CHECK(snapshot(window) == before);
  tools.kind_editor().setCurrentIndex(static_cast<int>(GuiMeasureKind::RadiusDiameter));
  tools.filter_editor().setCurrentIndex(2);
  click({34, 0, 20});
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(4));
  CHECK(tools.targets().at(tools.target_editor(0).currentData().toInt()).kind == K::Circle);
  tools.filter_editor().setCurrentIndex(3);
  click({32.828427, 2.828427, 10});
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Catch::Approx(4));
  CHECK(tools.targets().at(tools.target_editor(0).currentData().toInt()).kind == K::Cylinder);
  capture(window, "native-measure-radius");
  auto& close = button(window, "close");
  REQUIRE(close.isVisible());
  REQUIRE(window.childAt(close.mapTo(&window, close.rect().center())) == &close);
  close.click();
  CHECK_FALSE(tools.active());
  CHECK_FALSE(overlay->isVisible());
}
