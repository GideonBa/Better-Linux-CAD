#include "../support/reference_repair_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/sketch_reference_projector.hpp"
#include "blcad/gui/shell/shell_reference_repair_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QToolButton>
#include <catch2/catch_approx.hpp>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
namespace {
template <typename T> T& control(ShellWindow& window, const char* name) {
  auto* result = window.findChild<T*>(QStringLiteral("blcad.reference_repair.") + name);
  REQUIRE(result);
  return *result;
}
QAction& action(ShellWindow& window) {
  auto* result = window.findChild<QAction*>("blcad.action.repair_references");
  REQUIRE(result);
  return *result;
}
std::string snapshot(const ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
void load(ShellWindow& window, const QTemporaryDir& directory, bool broken = true,
          bool plane = false, bool downstream = false) {
  auto part = test::reference_repair_fixture(broken, plane, downstream);
  QFile file(directory.filePath("input.blcad.json"));
  REQUIRE(file.open(QIODevice::WriteOnly));
  const auto json = reference_checked(serialize_part_document_to_json(part));
  REQUIRE(file.write(QByteArray::fromStdString(json)) == static_cast<qint64>(json.size()));
  file.close();
  REQUIRE(window.open_path(file.fileName()));
}
void choose(ShellWindow& window, const char* control_name, const QString& identity) {
  auto& combo = control<QComboBox>(window, control_name);
  const auto index = combo.findData(identity);
  INFO(identity.toStdString());
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void begin(ShellWindow& window, const QString& id = "sketch.refs/point") {
  REQUIRE(action(window).isEnabled());
  action(window).trigger();
  REQUIRE(window.reference_repair_tools().active());
  choose(window, "reference", id);
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
}
void preview(ShellWindow& window) {
  REQUIRE(control<QPushButton>(window, "preview").isEnabled());
  control<QPushButton>(window, "preview").click();
  INFO(control<QLabel>(window, "summary").text().toStdString());
  REQUIRE(control<QPushButton>(window, "apply").isEnabled());
  REQUIRE(window.viewport().feature_preview_active());
}
void apply(ShellWindow& window) {
  control<QPushButton>(window, "apply").click();
  REQUIRE_FALSE(window.reference_repair_tools().active());
  REQUIRE_FALSE(window.session().task().active());
  REQUIRE_FALSE(window.viewport().feature_preview_active());
}
void click(QWidget& widget, QPoint point = {}) {
  REQUIRE(widget.isVisible());
  if (point.isNull())
    point = widget.rect().center();
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, QPointF(point), widget.mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
  }
  qApp->processEvents();
}
void mouse_choose(QComboBox& combo, const QString& id) {
  const auto row = combo.findData(id);
  INFO(id.toStdString());
  REQUIRE(row >= 0);
  combo.showPopup();
  qApp->processEvents();
  auto* view = combo.view();
  const auto index = combo.model()->index(row, combo.modelColumn());
  view->scrollTo(index);
  qApp->processEvents();
  click(*view->viewport(), view->visualRect(index).center());
  REQUIRE(combo.currentIndex() == row);
}
void ribbon(ShellWindow& window) {
  window.ribbon().set_current_tab(0);
  qApp->processEvents();
  for (auto* button : window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() == &action(window)) {
      for (auto* parent = button->parentWidget(); parent; parent = parent->parentWidget())
        if (auto* scroll = qobject_cast<QScrollArea*>(parent))
          scroll->ensureWidgetVisible(button, 8, 8);
      qApp->processEvents();
      REQUIRE(button->visibleRegion().contains(button->rect().center()));
      click(*button);
      return;
    }
  FAIL("Repair ribbon button missing");
}
} // namespace

TEST_CASE("Lost projected references require explicit preview and keep constraints history and "
          "files atomic",
          "[gui][block136-reference-repair]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory);
  const auto before = snapshot(window);
  const auto revision = window.session().presentation_revision();
  begin(window);
  CHECK(control<QLabel>(window, "diagnostic").text().startsWith("Verloren:"));
  CHECK(window.reference_repair_tools().apply().has_error());
  CHECK(control<QComboBox>(window, "replacement").currentIndex() == -1);
  choose(window, "replacement", "feature.new.vertex.top_front_right");
  preview(window);
  CHECK(window.viewport().feature_preview_replaces("sketch/sketch.refs/entity/point"));
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.session().can_undo());
  CHECK(window.session().presentation_revision() == revision);
  // A replacement change revokes the approval granted by Preview.
  choose(window, "replacement", "point.fallback");
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(window.reference_repair_tools().apply().has_error());
  preview(window);
  apply(window);
  const auto after_point = snapshot(window);
  CHECK(after_point != before);
  CHECK(window.session().undo_label() == "Repair reference");
  begin(window, "sketch.refs/edge");
  CHECK(control<QComboBox>(window, "replacement").findData("feature.new.edge.front_right") == -1);
  CHECK(control<QComboBox>(window, "replacement").findData("feature.old.edge.top_front") == -1);
  choose(window, "replacement", "feature.new.edge.top_front");
  preview(window);
  apply(window);
  const auto after_line = snapshot(window);
  const auto* sketch = window.session().part_document()->find_sketch(SketchId("sketch.refs"));
  REQUIRE(sketch);
  CHECK(sketch->constraints().size() == 2);
  CHECK(sketch->projected_points().front().construction_point() ==
        ConstructionPointId("point.fallback"));
  CHECK(sketch->projected_lines().front().semantic_edge()->source_feature() ==
        FeatureId("feature.new"));
  REQUIRE(window.save_to(directory.filePath("repaired.blcad.json")));
  const auto saved = bytes(directory.filePath("repaired.blcad.json"));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == after_point);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after_point);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after_line);
  CHECK_FALSE(window.session().dirty());
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("repaired.blcad.json")));
  CHECK(snapshot(reopened) == after_line);
  REQUIRE(reopened.save_to({}));
  CHECK(bytes(directory.filePath("repaired.blcad.json")) == saved);
  begin(window);
  CHECK(control<QLabel>(window, "diagnostic").text().startsWith("Aufgelöst"));
  CHECK(control<QComboBox>(window, "replacement").findData("point.fallback") == -1);
  window.reference_repair_tools().cancel();
}

TEST_CASE("Reference repair expires on cancel close Escape save history and document replacement",
          "[gui][block136-reference-repair]") {
  for (int exit = 0; exit < 7; ++exit) {
    CAPTURE(exit);
    ShellWindow window;
    QTemporaryDir directory;
    load(window, directory);
    const auto before = snapshot(window);
    begin(window);
    choose(window, "replacement", "point.fallback");
    preview(window);
    switch (exit) {
    case 0:
      control<QPushButton>(window, "cancel").click();
      break;
    case 1: {
      auto* dock = window.findChild<QDockWidget*>("blcad.shell.reference_repair_task");
      REQUIRE(dock);
      dock->close();
      break;
    }
    case 2: {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&control<QComboBox>(window, "replacement"), &escape);
      break;
    }
    case 3:
      REQUIRE(window.save_to(directory.filePath("preview.blcad.json")));
      break;
    case 4:
      CHECK_FALSE(window.undo());
      break;
    case 5:
      REQUIRE(window.new_part("Replacement"));
      break;
    case 6: {
      REQUIRE(window.session().commit_part_transaction("External edit", [](PartDocument& part) {
        return part.add_construction_point(reference_checked(ConstructionPoint::create_explicit(
            ConstructionPointId("external"), "External", {3, 4, 5})));
      }));
      CHECK(window.reference_repair_tools().apply().has_error());
      window.refresh();
      break;
    }
    }
    CHECK_FALSE(window.reference_repair_tools().active());
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    if (exit < 5) {
      CHECK(snapshot(window) == before);
      CHECK_FALSE(window.session().dirty());
    }
    if (exit == 3) {
      ShellWindow reopened;
      REQUIRE(reopened.open_path(directory.filePath("preview.blcad.json")));
      CHECK(snapshot(reopened) == before);
    }
  }
}

TEST_CASE("Repair rejects cyclic downstream sources and recomputes a valid replacement",
          "[gui][block136-reference-repair]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory, false, false, true);
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(window.session().derived_results_fresh());
  const auto before = snapshot(window);
  begin(window);
  choose(window, "replacement", "feature.new.vertex.top_front_left");
  preview(window);
  const auto revision = window.session().presentation_revision();
  choose(window, "replacement", "feature.consumer.vertex.top_front_right");
  control<QPushButton>(window, "preview").click();
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().can_undo());
  CHECK(window.session().presentation_revision() == revision);
  choose(window, "replacement", "feature.new.vertex.top_front_left");
  preview(window);
  apply(window);
  const auto* sketch = window.session().part_document()->find_sketch(SketchId("sketch.refs"));
  auto resolved = reference_checked(geometry::SketchReferenceProjector{}.resolve_point(
      *window.session().part_document(), *sketch, sketch->projected_points().front(),
      *window.session().part_shape_cache()));
  CHECK(resolved.position.x == Catch::Approx(20));
  CHECK(resolved.position.y == Catch::Approx(5));
  CHECK(window.session().derived_results_fresh());
  CHECK(window.session().part_shape_cache()->feature_shape_count() == 3);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Lost face workplane repair preserves its identity and undoable placement",
          "[gui][block136-reference-repair]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory, true, true);
  const auto before = snapshot(window);
  begin(window, "plane/");
  CHECK(control<QLabel>(window, "diagnostic").text().startsWith("Verloren:"));
  auto& replacements = control<QComboBox>(window, "replacement");
  CHECK(replacements.count() == 6);
  choose(window, "replacement", "feature.new.face.top");
  preview(window);
  apply(window);
  const auto* plane =
      window.session().part_document()->find_derived_workplane(DatumPlaneId("plane"));
  REQUIRE(plane);
  CHECK(plane->name() == "Face plane");
  CHECK(plane->face_reference().source_feature() == FeatureId("feature.new"));
  auto frame = reference_checked(
      geometry::WorkplaneResolver{}.resolve(*window.session().part_document(), plane->id()));
  CHECK(frame.origin.z == Catch::Approx(8));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  REQUIRE(window.save_to(directory.filePath("plane.blcad.json")));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("plane.blcad.json")));
  CHECK(snapshot(reopened) == snapshot(window));
}

TEST_CASE("Workplane repair rejects downstream bounds failures and previews a reversed plane solid",
          "[gui][block136-reference-repair]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory, false, true);
  REQUIRE(window.session().commit_part_transaction("Plane consumer", [](PartDocument& part) {
    auto sketch = reference_checked(
        Sketch::create(SketchId("sketch.face"), "Face consumer", DatumPlaneId("plane")));
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId("profile.face"), ParameterId("width"), ParameterId("height")))));
    auto added = part.add_sketch(sketch);
    if (!added)
      return added;
    return part.add_feature(reference_checked(Feature::create_additive_extrude(
        FeatureId("feature.face"), "Face solid", sketch.id(), ParameterId("depth"))));
  }));
  window.refresh();
  REQUIRE(window.save_to(directory.filePath("consumer.blcad.json")));
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  const auto bounds = [&] {
    auto* shape =
        window.session().part_shape_cache()->find_feature_shape(FeatureId("feature.face"));
    REQUIRE(shape);
    return reference_checked(geometry::BodyTransformAdapter{}.bounds(*shape));
  };
  CHECK(bounds().minimum.z == Catch::Approx(8).margin(1e-6));
  begin(window, "plane/");
  choose(window, "replacement", "feature.new.face.bottom");
  preview(window);
  choose(window, "replacement", "feature.new.face.front");
  control<QPushButton>(window, "preview").click();
  CHECK(control<QLabel>(window, "summary").text().contains("bounds"));
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  window.reference_repair_tools().cancel();
  CHECK(window.session().undo_label() == undo);
  CHECK(snapshot(window) == before);
  begin(window, "plane/");
  choose(window, "replacement", "feature.new.face.bottom");
  preview(window);
  CHECK(window.viewport().feature_preview_replaces("derived-workplane/plane"));
  CHECK(snapshot(window) == before);
  apply(window);
  CHECK(bounds().maximum.z == Catch::Approx(0).margin(1e-6));
  CHECK(bounds().minimum.z == Catch::Approx(-8).margin(1e-6));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}

TEST_CASE("Repair command guards empty project sketch and occupied task contexts",
          "[gui][block136-reference-repair]") {
  ShellWindow window;
  CHECK_FALSE(action(window).isEnabled());
  CHECK(window.reference_repair_tools().begin().has_error());
  REQUIRE(window.new_project("Assembly"));
  CHECK_FALSE(action(window).isEnabled());
  REQUIRE(window.session().close(true));
  REQUIRE(window.new_part("Empty"));
  action(window).trigger();
  CHECK(control<QComboBox>(window, "reference").count() == 0);
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(action(window).isEnabled());
  window.reference_repair_tools().cancel();
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  CHECK_FALSE(action(window).isEnabled());
  CHECK(window.reference_repair_tools().begin().has_error());
}

TEST_CASE(
    "Ribbon and popup input repair multiple imported references with native disposable previews",
    "[gui][block136-reference-repair][shell-reference-repair-input]") {
  ShellWindow window;
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  QTemporaryDir directory;
  load(window, directory);
  const auto before = snapshot(window);
  for (const auto& [reference, target] :
       {std::pair{QString("sketch.refs/point"), QString("feature.new.vertex.top_front_right")},
        std::pair{QString("sketch.refs/edge"), QString("line.fallback")}}) {
    ribbon(window);
    mouse_choose(control<QComboBox>(window, "reference"), reference);
    mouse_choose(control<QComboBox>(window, "replacement"), target);
    CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
    click(control<QPushButton>(window, "preview"));
    REQUIRE(window.viewport().feature_preview_active());
    REQUIRE(control<QPushButton>(window, "apply").isEnabled());
    if (QApplication::platformName() == "xcb") {
      const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
      if (!captures.isEmpty()) {
        REQUIRE(QDir().mkpath(captures));
        CHECK(window.grab().save(QDir(captures).filePath(reference.endsWith("point")
                                                             ? "reference-point-preview.png"
                                                             : "reference-line-preview.png")));
      }
    }
    click(control<QPushButton>(window, "apply"));
    REQUIRE_FALSE(window.reference_repair_tools().active());
  }
  const auto repaired = snapshot(window);
  REQUIRE(window.save_to(directory.filePath("ui.blcad.json")));
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  REQUIRE(window.redo());
  CHECK(snapshot(window) == repaired);
  ribbon(window);
  mouse_choose(control<QComboBox>(window, "reference"), "sketch.refs/point");
  mouse_choose(control<QComboBox>(window, "replacement"), "point.fallback");
  click(control<QPushButton>(window, "preview"));
  click(control<QPushButton>(window, "cancel"));
  CHECK(snapshot(window) == repaired);
  CHECK_FALSE(window.session().dirty());
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("ui.blcad.json")));
  CHECK(snapshot(reopened) == repaired);
}
