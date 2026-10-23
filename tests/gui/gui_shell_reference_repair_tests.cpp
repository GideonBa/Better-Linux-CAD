#include "../support/missing_reference_fixture.hpp"
#include "../support/reference_repair_fixture.hpp"
#include "../support/repair_open_fixture.hpp"
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
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTimer>
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
void load_batch(ShellWindow& window, const QTemporaryDir& directory) {
  auto part = test::reference_repair_fixture();
  for (const std::string suffix : {"a", "b"}) {
    const auto plane = DatumPlaneId("plane." + suffix);
    REQUIRE(part.add_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
        plane, "Plane " + suffix,
        reference_checked(
            SemanticFaceReference::create(FeatureId("feature.old"), SemanticFace::Top))))));
    auto sketch =
        reference_checked(Sketch::create(SketchId("sketch." + suffix), "Sketch " + suffix, plane));
    REQUIRE(sketch.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("line"), {0, 0}, {3, 0}))));
    REQUIRE(
        sketch.add_reference(reference_checked(ProjectedSketchPoint::create_from_semantic_vertex(
            SketchEntityId("point"),
            reference_checked(SemanticVertexReference::create(FeatureId("feature.old"),
                                                              SemanticVertex::TopFrontRight))))));
    REQUIRE(sketch.add_reference(reference_checked(ProjectedSketchLine::create_from_semantic_edge(
        SketchEntityId("edge"), reference_checked(SemanticEdgeReference::create(
                                    FeatureId("feature.old"), SemanticEdge::TopFront))))));
    REQUIRE(part.add_sketch(sketch));
  }
  QFile file(directory.filePath("batch.blcad.json"));
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
void queue_repair(ShellWindow& window, const QString& reference, const QString& target) {
  choose(window, "reference", reference);
  choose(window, "replacement", target);
  REQUIRE(control<QPushButton>(window, "stage").isEnabled());
  control<QPushButton>(window, "stage").click();
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
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
void ribbon_command(ShellWindow& window, QAction& command, int tab) {
  window.ribbon().set_current_tab(tab);
  qApp->processEvents();
  for (auto* button : window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() == &command) {
      for (auto* parent = button->parentWidget(); parent; parent = parent->parentWidget())
        if (auto* scroll = qobject_cast<QScrollArea*>(parent))
          scroll->ensureWidgetVisible(button, 8, 8);
      qApp->processEvents();
      REQUIRE(button->visibleRegion().contains(button->rect().center()));
      click(*button);
      return;
    }
  FAIL("Ribbon button missing: " + command.objectName().toStdString());
}
void ribbon(ShellWindow& window) {
  ribbon_command(window, action(window), 0);
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

TEST_CASE(
    "Reference repair batches resolve dependent plane and projected inputs in one transaction",
    "[gui][block136-reference-repair-batches]") {
  ShellWindow window;
  QTemporaryDir directory;
  load_batch(window, directory);
  const auto before = snapshot(window);
  const auto revision = window.session().presentation_revision();
  begin(window, "sketch.a/point");
  CHECK(control<QComboBox>(window, "replacement").count() == 0);
  queue_repair(window, "plane.a/", "feature.new.face.top");
  REQUIRE(control<QListWidget>(window, "pending").count() == 1);
  CHECK(window.reference_repair_tools().preview().has_error());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  // Choices now resolve against the staged upstream plane, while the file stays broken.
  queue_repair(window, "sketch.a/point", "feature.new.vertex.top_front_right");
  queue_repair(window, "sketch.a/edge", "feature.new.edge.top_front");
  queue_repair(window, "plane.b/", "feature.new.face.bottom");
  queue_repair(window, "sketch.b/point", "point.fallback");
  REQUIRE(control<QListWidget>(window, "pending").count() == 5);
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK(window.session().presentation_revision() == revision);
  preview(window);
  for (const std::string id :
       {"derived-workplane/plane.a", "derived-workplane/plane.b", "sketch/sketch.a/entity/point",
        "sketch/sketch.a/entity/edge", "sketch/sketch.b/entity/point"})
    CHECK(window.viewport().feature_preview_replaces(id));
  apply(window);
  const auto after = snapshot(window);
  CHECK(after != before);
  CHECK(window.session().undo_label() == "Repair references");
  CHECK(control<QListWidget>(window, "pending").count() == 0);
  const auto& part = *window.session().part_document();
  CHECK(part.find_sketch(SketchId("sketch.a"))
            ->projected_points()
            .front()
            .semantic_vertex()
            ->source_feature() == FeatureId("feature.new"));
  CHECK(part.find_sketch(SketchId("sketch.b"))->projected_points().front().construction_point() ==
        ConstructionPointId("point.fallback"));
  CHECK(part.find_sketch(SketchId("sketch.refs"))->constraints().size() == 2);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().can_undo());
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  REQUIRE(window.save_to(directory.filePath("repaired-batch.blcad.json")));
  const auto saved = bytes(directory.filePath("repaired-batch.blcad.json"));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("repaired-batch.blcad.json")));
  CHECK(snapshot(reopened) == after);
  REQUIRE(reopened.save_to({}));
  CHECK(bytes(directory.filePath("repaired-batch.blcad.json")) == saved);
}

TEST_CASE("Repair list edits upsert scoped identities remove individual entries and reset staging",
          "[gui][block136-reference-repair-batches]") {
  ShellWindow window;
  QTemporaryDir directory;
  load_batch(window, directory);
  const auto before = snapshot(window);
  begin(window, "plane.a/");
  queue_repair(window, "plane.a/", "feature.new.face.top");
  queue_repair(window, "plane.b/", "feature.new.face.bottom");
  queue_repair(window, "sketch.a/point", "point.fallback");
  queue_repair(window, "sketch.b/point", "feature.new.vertex.top_front_left");
  auto& pending = control<QListWidget>(window, "pending");
  REQUIRE(pending.count() == 4);
  CHECK(pending.item(0)->text().contains("new / Fläche oben"));
  queue_repair(window, "sketch.a/point", "feature.new.vertex.top_back_right");
  REQUIRE(pending.count() == 4);
  CHECK(pending.item(2)->toolTip() == "feature.new.vertex.top_back_right");
  CHECK(pending.item(3)->toolTip() == "feature.new.vertex.top_front_left");
  preview(window);
  pending.setCurrentRow(0);
  control<QPushButton>(window, "remove").click();
  REQUIRE(pending.count() == 3);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK(window.reference_repair_tools().preview().has_error());
  CHECK(snapshot(window) == before);
  queue_repair(window, "plane.a/", "feature.new.face.top");
  preview(window);
  control<QPushButton>(window, "clear").click();
  CHECK(pending.count() == 0);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  choose(window, "reference", "sketch.a/point");
  CHECK(control<QComboBox>(window, "replacement").count() == 0);
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  window.reference_repair_tools().cancel();
}

TEST_CASE("Cyclic batch edits preserve the previous valid list and revoke its preview",
          "[gui][block136-reference-repair-batches]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory, false, false, true);
  const auto before = snapshot(window);
  begin(window);
  queue_repair(window, "sketch.refs/point", "feature.new.vertex.top_front_left");
  queue_repair(window, "sketch.refs/edge", "line.fallback");
  preview(window);
  const auto revision = window.session().presentation_revision();
  queue_repair(window, "sketch.refs/point", "feature.consumer.vertex.top_front_right");
  auto& pending = control<QListWidget>(window, "pending");
  REQUIRE(pending.count() == 2);
  CHECK(pending.item(0)->toolTip() == "feature.new.vertex.top_front_left");
  CHECK(control<QLabel>(window, "summary").text().contains("cycle"));
  CHECK(window.session().presentation_revision() == revision);
  CHECK(snapshot(window) == before);
  // Selecting another reference discards the failed draft, retaining the staged list.
  choose(window, "reference", "sketch.refs/edge");
  preview(window);
  apply(window);
  CHECK(window.session().undo_label() == "Repair references");
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Whole repair batches recompute multiple solids and reject downstream bounds atomically",
          "[gui][block136-reference-repair-batches]") {
  ShellWindow window;
  QTemporaryDir directory;
  load(window, directory, false, true);
  REQUIRE(window.session().commit_part_transaction("Plane consumers", [](PartDocument& part) {
    auto second_plane =
        part.add_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
            DatumPlaneId("plane.second"), "Second",
            reference_checked(
                SemanticFaceReference::create(FeatureId("feature.new"), SemanticFace::Top)))));
    if (!second_plane)
      return second_plane;
    for (const std::string suffix : {"one", "two"}) {
      auto sketch = reference_checked(
          Sketch::create(SketchId("face." + suffix), "Face " + suffix,
                         DatumPlaneId(suffix == "one" ? "plane" : "plane.second")));
      REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
          ProfileId("face.profile." + suffix), ParameterId("width"), ParameterId("height")))));
      auto added = part.add_sketch(sketch);
      if (!added)
        return added;
      added = part.add_feature(reference_checked(
          Feature::create_additive_extrude(FeatureId("face.solid." + suffix), "Solid " + suffix,
                                           sketch.id(), ParameterId("depth"))));
      if (!added)
        return added;
    }
    return Result<std::size_t>::success(4);
  }));
  window.refresh();
  REQUIRE(window.save_to(directory.filePath("solids.blcad.json")));
  const auto before = snapshot(window);
  const auto bounds = [&](const std::string& suffix) {
    const auto* shape =
        window.session().part_shape_cache()->find_feature_shape(FeatureId("face.solid." + suffix));
    REQUIRE(shape);
    return reference_checked(geometry::BodyTransformAdapter{}.bounds(*shape));
  };
  begin(window, "plane/");
  queue_repair(window, "plane/", "feature.new.face.bottom");
  queue_repair(window, "plane.second/", "feature.old.face.front");
  control<QPushButton>(window, "preview").click();
  CHECK(control<QLabel>(window, "summary").text().contains("bounds"));
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  for (const std::string suffix : {"one", "two"})
    CHECK(bounds(suffix).minimum.z == Catch::Approx(8).margin(1e-6));
  queue_repair(window, "plane.second/", "feature.old.face.bottom");
  REQUIRE(control<QListWidget>(window, "pending").count() == 2);
  preview(window);
  CHECK(window.viewport().feature_preview_replaces("derived-workplane/plane"));
  CHECK(window.viewport().feature_preview_replaces("derived-workplane/plane.second"));
  CHECK(snapshot(window) == before);
  apply(window);
  const auto after = snapshot(window);
  for (const std::string suffix : {"one", "two"}) {
    CHECK(bounds(suffix).minimum.z == Catch::Approx(-8).margin(1e-6));
    CHECK(bounds(suffix).maximum.z == Catch::Approx(0).margin(1e-6));
  }
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  for (const std::string suffix : {"one", "two"})
    CHECK(bounds(suffix).minimum.z == Catch::Approx(8).margin(1e-6));
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  REQUIRE(window.save_to({}));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("solids.blcad.json")));
  CHECK(snapshot(reopened) == after);
}

TEST_CASE("Pending repair batches expire without publishing on every task exit",
          "[gui][block136-reference-repair-batches]") {
  for (int exit = 0; exit < 8; ++exit) {
    CAPTURE(exit);
    ShellWindow window;
    QTemporaryDir directory;
    load_batch(window, directory);
    const auto before = snapshot(window);
    begin(window, "plane.a/");
    queue_repair(window, "plane.a/", "feature.new.face.top");
    queue_repair(window, "plane.b/", "feature.new.face.bottom");
    preview(window);
    switch (exit) {
    case 0:
      control<QPushButton>(window, "cancel").click();
      break;
    case 1:
      window.findChild<QDockWidget*>("blcad.shell.reference_repair_task")->close();
      break;
    case 2: {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&control<QListWidget>(window, "pending"), &escape);
      break;
    }
    case 3:
      REQUIRE(window.save_to(directory.filePath("pending.blcad.json")));
      break;
    case 4:
      CHECK_FALSE(window.undo());
      break;
    case 5:
      CHECK_FALSE(window.redo());
      break;
    case 6:
      REQUIRE(window.new_part("Replacement"));
      break;
    case 7:
      REQUIRE(window.session().commit_part_transaction("External", [](PartDocument& part) {
        return part.add_construction_point(reference_checked(ConstructionPoint::create_explicit(
            ConstructionPointId("external"), "External", {2, 3, 4})));
      }));
      CHECK(window.reference_repair_tools().apply().has_error());
      window.refresh();
      break;
    }
    CHECK_FALSE(window.reference_repair_tools().active());
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(control<QListWidget>(window, "pending").count() == 0);
    if (exit < 6) {
      CHECK(snapshot(window) == before);
      CHECK_FALSE(window.session().dirty());
    }
    if (exit == 3) {
      ShellWindow reopened;
      REQUIRE(reopened.open_path(directory.filePath("pending.blcad.json")));
      CHECK(snapshot(reopened) == before);
    }
  }
}

TEST_CASE(
    "Native ribbon popup and repair list input commits a corrected multi-plane batch atomically",
    "[gui][block136-reference-repair-batches][shell-repair-batch-input]") {
  ShellWindow window;
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  QTemporaryDir directory;
  load_batch(window, directory);
  const auto before = snapshot(window);
  ribbon(window);
  const auto stage_input = [&](const QString& reference, const QString& target) {
    mouse_choose(control<QComboBox>(window, "reference"), reference);
    mouse_choose(control<QComboBox>(window, "replacement"), target);
    click(control<QPushButton>(window, "stage"));
  };
  stage_input("plane.a/", "feature.new.face.top");
  click(control<QPushButton>(window, "preview"));
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  stage_input("sketch.a/point", "feature.new.vertex.top_front_right");
  stage_input("sketch.a/edge", "feature.new.edge.top_front");
  stage_input("plane.b/", "feature.new.face.bottom");
  stage_input("sketch.b/point", "point.fallback");
  auto& pending = control<QListWidget>(window, "pending");
  REQUIRE(pending.count() == 5);
  pending.scrollToItem(pending.item(0));
  qApp->processEvents();
  click(*pending.viewport(), pending.visualItemRect(pending.item(0)).center());
  click(control<QPushButton>(window, "remove"));
  REQUIRE(pending.count() == 4);
  stage_input("plane.a/", "feature.new.face.top");
  REQUIRE(pending.count() == 5);
  click(control<QPushButton>(window, "preview"));
  REQUIRE(control<QPushButton>(window, "apply").isEnabled());
  REQUIRE(window.viewport().feature_preview_active());
  auto* fit = window.findChild<QAction*>("blcad.action.fit");
  REQUIRE(fit);
  ribbon_command(window, *fit, 2);
  CHECK(window.viewport().feature_preview_active());
  CHECK(pending.count() == 5);
  CHECK(snapshot(window) == before);
  if (QApplication::platformName() == "xcb") {
    const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) {
      REQUIRE(QDir().mkpath(captures));
      CHECK(window.grab().save(QDir(captures).filePath("reference-batch-preview.png")));
    }
  }
  click(control<QPushButton>(window, "apply"));
  REQUIRE_FALSE(window.reference_repair_tools().active());
  const auto after = snapshot(window);
  CHECK(after != before);
  CHECK(window.session().undo_label() == "Repair references");
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().can_undo());
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  REQUIRE(window.save_to(directory.filePath("native-batch.blcad.json")));
  const auto saved = bytes(directory.filePath("native-batch.blcad.json"));
  ribbon(window);
  stage_input("sketch.a/point", "point.fallback");
  click(control<QPushButton>(window, "clear"));
  CHECK(pending.count() == 0);
  click(control<QPushButton>(window, "cancel"));
  CHECK(snapshot(window) == after);
  CHECK_FALSE(window.session().dirty());
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("native-batch.blcad.json")));
  REQUIRE(reopened.save_to({}));
  CHECK(snapshot(reopened) == after);
  CHECK(bytes(directory.filePath("native-batch.blcad.json")) == saved);
}

namespace {
QString repair_open_file(const QTemporaryDir& directory) {
  const auto path = directory.filePath("failed-open.blcad.json");
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly));
  const auto json = reference_checked(serialize_part_document_to_json(test::repair_open_fixture()));
  REQUIRE(file.write(QByteArray::fromStdString(json)) == static_cast<qint64>(json.size()));
  file.close();
  return path;
}
void file_input(ShellWindow& window, const char* action_name, const QString& path) {
  auto* command = window.findChild<QAction*>(QStringLiteral("blcad.action.") + action_name);
  REQUIRE(command);
  bool selected = false;
  bool timed_out = false;
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, [&] {
    timed_out = true;
    if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
      dialog->reject();
  });
  QTimer input;
  QObject::connect(&input, &QTimer::timeout, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (!dialog)
      return;
    input.stop();
    auto* filename = dialog->findChild<QLineEdit*>("fileNameEdit");
    REQUIRE(filename);
    filename->setFocus();
    QKeyEvent select(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
    QApplication::sendEvent(filename, &select);
    QKeyEvent type(QEvent::KeyPress, 0, Qt::NoModifier, path);
    QApplication::sendEvent(filename, &type);
    selected = true;
    QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(filename, &accept);
  });
  watchdog.start(5000);
  input.start(10);
  ribbon_command(window, *command, 0);
  input.stop();
  watchdog.stop();
  CHECK_FALSE(timed_out);
  REQUIRE(selected);
}
} // namespace
TEST_CASE(
    "Missing-source shell repair cancels and saves only committed intent during partial staging",
    "[gui][block136-missing-reference]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = directory.filePath("missing.blcad.json");
  {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    const auto input = test::missing_reference_fixture_json();
    REQUIRE(file.write(QByteArray::fromStdString(input)) == static_cast<qint64>(input.size()));
  }
  REQUIRE(window.open_path(path, GuiDocumentOpenMode::ReferenceRepair));
  REQUIRE(window.reference_repair_tools().active());
  const auto before = snapshot(window);
  queue_repair(window, "sketch.refs/point", "feature.new.vertex.top_front_right");
  control<QPushButton>(window, "preview").click();
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  window.reference_repair_tools().cancel();
  CHECK(snapshot(window) == before);
  CHECK(window.session().reference_repair_mode());
  REQUIRE(window.reference_repair_tools().begin());
  queue_repair(window, "plane/", "feature.new.face.top");
  const auto saved = directory.filePath("missing-saved.blcad.json");
  REQUIRE(window.save_to(saved));
  CHECK(bytes(saved).toStdString() == before);
  CHECK_FALSE(window.reference_repair_tools().active());
  CHECK_FALSE(read_part_document_json_file(saved.toStdString()));
  CHECK_FALSE(window.open_path(saved));
  CHECK(snapshot(window) == before);
  REQUIRE(window.open_path(saved, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(window.session().reference_repair_mode());
  CHECK_FALSE(window.session().dirty());
}

TEST_CASE("Native input repairs six missing face point and line sources in one saved history step",
          "[gui][block136-missing-reference][shell-missing-reference-input]") {
  const bool old_dialogs = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
  struct RestoreDialogs {
    bool old;
    ~RestoreDialogs() {
      QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, old);
    }
  } restore{old_dialogs};
  ShellWindow window;
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  QTemporaryDir directory;
  const auto path = directory.filePath("six-missing.blcad.json");
  {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    const auto input = test::missing_reference_fixture_json();
    REQUIRE(file.write(QByteArray::fromStdString(input)) == static_cast<qint64>(input.size()));
  }
  file_input(window, "open_repair", path);
  REQUIRE(window.reference_repair_tools().active());
  REQUIRE(window.session().reference_repair_mode());
  const auto before = snapshot(window);
  const std::vector<std::pair<QString, QString>> changes{
      {"sketch.refs/point", "feature.new.vertex.top_front_right"},
      {"plane/", "feature.new.face.top"},
      {"sketch.refs/point.construction", "feature.new.vertex.bottom_back_left"},
      {"sketch.refs/edge", "feature.new.edge.top_front"},
      {"plane.second/", "feature.new.face.top"},
      {"sketch.refs/line.construction", "feature.new.edge.top_front"}};
  for (std::size_t index = 0; index < changes.size(); ++index) {
    mouse_choose(control<QComboBox>(window, "reference"), changes[index].first);
    mouse_choose(control<QComboBox>(window, "replacement"), changes[index].second);
    click(control<QPushButton>(window, "stage"));
    REQUIRE(control<QListWidget>(window, "pending").count() == static_cast<int>(index + 1));
    click(control<QPushButton>(window, "preview"));
    CHECK(control<QPushButton>(window, "apply").isEnabled() == (index + 1 == changes.size()));
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().part_shape_cache());
  }
  REQUIRE(window.viewport().feature_preview_active());
  auto* fit = window.findChild<QAction*>("blcad.action.fit");
  REQUIRE(fit);
  ribbon_command(window, *fit, 2);
  if (QApplication::platformName() == "xcb") {
    const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) {
      REQUIRE(QDir().mkpath(captures));
      CHECK(window.grab().save(QDir(captures).filePath("missing-reference-preview.png")));
    }
  }
  click(control<QPushButton>(window, "apply"));
  CHECK_FALSE(window.session().reference_repair_mode());
  REQUIRE(window.session().part_shape_cache());
  const auto after = snapshot(window);
  auto* undo = window.findChild<QAction*>("blcad.action.undo");
  auto* redo = window.findChild<QAction*>("blcad.action.redo");
  REQUIRE(undo);
  REQUIRE(redo);
  ribbon_command(window, *undo, 0);
  CHECK(snapshot(window) == before);
  CHECK(window.session().reference_repair_mode());
  CHECK_FALSE(window.session().part_shape_cache());
  CHECK(window.viewport().presentation_count() == 0);
  CHECK_FALSE(window.session().can_undo());
  ribbon_command(window, *redo, 0);
  CHECK(snapshot(window) == after);
  CHECK_FALSE(window.session().reference_repair_mode());
  CHECK(window.viewport().presentation_count() > 0);
  const auto saved = directory.filePath("six-repaired.blcad.json");
  file_input(window, "save_as", saved);
  CHECK_FALSE(window.session().dirty());
  file_input(window, "open", saved);
  CHECK(snapshot(window) == after);
  CHECK(window.session().derived_results_fresh());
}
TEST_CASE(
    "Repair opening keeps failed Part intent and preview saves distinct from repaired geometry",
    "[gui][block136-repair-open]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = repair_open_file(directory);
  const auto original = bytes(path);
  REQUIRE(window.new_part("Previous"));
  REQUIRE(window.save_to(directory.filePath("previous.blcad.json")));
  const auto previous = snapshot(window);
  CHECK_FALSE(window.open_path(path));
  CHECK(snapshot(window) == previous);
  REQUIRE(window.open_path(path, GuiDocumentOpenMode::ReferenceRepair));
  CHECK(window.session().reference_repair_mode());
  CHECK(window.windowTitle().contains("[Reparatur]"));
  CHECK(window.session().part_shape_cache() == nullptr);
  CHECK(window.viewport().presentation_count() == 0);
  REQUIRE(window.reference_repair_tools().active());
  const auto before = snapshot(window);
  CHECK_FALSE(window.create_sketch_on_selection());
  queue_repair(window, "plane/", "feature.new.face.top");
  control<QPushButton>(window, "preview").click();
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  queue_repair(window, "plane.second/", "feature.new.face.top");
  preview(window);
  CHECK(window.session().part_shape_cache() == nullptr);
  CHECK(window.session().reference_repair_mode());
  CHECK(snapshot(window) == before);
  REQUIRE(window.save_to(directory.filePath("preview-copy.blcad.json")));
  CHECK_FALSE(window.reference_repair_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(bytes(directory.filePath("preview-copy.blcad.json")) == original);
  CHECK(bytes(path) == original);
  CHECK_FALSE(window.session().dirty());
  auto* repair = window.findChild<QAction*>("blcad.action.repair_references");
  REQUIRE(repair);
  CHECK(repair->isEnabled());
  for (const auto* name : {"create_sketch", "extrude", "create_construction"}) {
    auto* command = window.findChild<QAction*>(QStringLiteral("blcad.action.") + name);
    REQUIRE(command);
    CHECK_FALSE(command->isEnabled());
  }
  begin(window, "plane/");
  queue_repair(window, "plane/", "feature.new.face.top");
  queue_repair(window, "plane.second/", "feature.new.face.top");
  preview(window);
  apply(window);
  const auto after = snapshot(window);
  CHECK_FALSE(window.session().reference_repair_mode());
  CHECK_FALSE(window.windowTitle().contains("[Reparatur]"));
  CHECK(window.session().derived_results_fresh());
  REQUIRE(window.session().part_shape_cache());
  CHECK(window.session().part_shape_cache()->feature_shape_count() == 4);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  CHECK(window.session().reference_repair_mode());
  CHECK(window.session().part_shape_cache() == nullptr);
  CHECK(window.viewport().presentation_count() == 0);
  CHECK_FALSE(window.viewport().feature_preview_active());
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  CHECK_FALSE(window.session().reference_repair_mode());
  REQUIRE(window.save_to(directory.filePath("repaired-open.blcad.json")));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("repaired-open.blcad.json")));
  CHECK(snapshot(reopened) == after);
}
TEST_CASE("Repair-open cancellation and failed file replacement preserve committed recovery state",
          "[gui][block136-repair-open]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = repair_open_file(directory);
  REQUIRE(window.open_path(path, GuiDocumentOpenMode::ReferenceRepair));
  const auto before = snapshot(window);
  queue_repair(window, "plane/", "feature.new.face.top");
  control<QPushButton>(window, "cancel").click();
  CHECK(snapshot(window) == before);
  CHECK(window.session().reference_repair_mode());
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.open_path(directory.filePath("missing.blcad.json"),
                               GuiDocumentOpenMode::ReferenceRepair));
  CHECK(snapshot(window) == before);
  CHECK(window.session().reference_repair_mode());
  REQUIRE(window.new_part("Next"));
  CHECK_FALSE(window.session().reference_repair_mode());
  CHECK(window.session().derived_results_fresh());
}
TEST_CASE(
    "Native file input repairs a failed-recompute Part and restores both history presentations",
    "[gui][block136-repair-open][shell-repair-open-input]") {
  const bool old_dialogs = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
  struct RestoreDialogs {
    bool old;
    ~RestoreDialogs() {
      QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, old);
    }
  } restore{old_dialogs};
  ShellWindow window;
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  QTemporaryDir directory;
  const auto path = repair_open_file(directory);
  file_input(window, "open_repair", path);
  REQUIRE(window.session().reference_repair_mode());
  REQUIRE(window.reference_repair_tools().active());
  const auto before = snapshot(window);
  const auto stage = [&](const QString& plane) {
    mouse_choose(control<QComboBox>(window, "reference"), plane);
    mouse_choose(control<QComboBox>(window, "replacement"), "feature.new.face.top");
    click(control<QPushButton>(window, "stage"));
  };
  stage("plane/");
  click(control<QPushButton>(window, "preview"));
  CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
  stage("plane.second/");
  click(control<QPushButton>(window, "preview"));
  REQUIRE(control<QPushButton>(window, "apply").isEnabled());
  REQUIRE(window.viewport().feature_preview_active());
  auto* fit = window.findChild<QAction*>("blcad.action.fit");
  REQUIRE(fit);
  ribbon_command(window, *fit, 2);
  CHECK(snapshot(window) == before);
  CHECK(window.session().part_shape_cache() == nullptr);
  if (QApplication::platformName() == "xcb") {
    const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) {
      REQUIRE(QDir().mkpath(captures));
      CHECK(window.grab().save(QDir(captures).filePath("repair-open-preview.png")));
    }
  }
  click(control<QPushButton>(window, "apply"));
  CHECK_FALSE(window.session().reference_repair_mode());
  REQUIRE(window.session().part_shape_cache());
  const auto after = snapshot(window);
  auto* undo = window.findChild<QAction*>("blcad.action.undo");
  REQUIRE(undo);
  ribbon_command(window, *undo, 0);
  CHECK(window.session().reference_repair_mode());
  CHECK(window.session().part_shape_cache() == nullptr);
  CHECK(window.viewport().presentation_count() == 0);
  CHECK(snapshot(window) == before);
  auto* redo = window.findChild<QAction*>("blcad.action.redo");
  REQUIRE(redo);
  ribbon_command(window, *redo, 0);
  CHECK_FALSE(window.session().reference_repair_mode());
  CHECK(snapshot(window) == after);
  CHECK(window.viewport().presentation_count() > 0);
  const auto saved = directory.filePath("native-repaired.blcad.json");
  file_input(window, "save_as", saved);
  CHECK_FALSE(window.session().dirty());
  file_input(window, "open", saved);
  CHECK_FALSE(window.session().reference_repair_mode());
  CHECK(snapshot(window) == after);
  CHECK(window.session().derived_results_fresh());
}
