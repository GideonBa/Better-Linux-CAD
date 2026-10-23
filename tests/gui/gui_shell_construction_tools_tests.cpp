#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/gui/shell/shell_construction_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;
using Kind = ShellConstructionTools::Kind;
namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
template <typename T> T& control(ShellWindow& window, const char* name) {
  auto* result = window.findChild<T*>(QStringLiteral("blcad.construction.") + name);
  REQUIRE(result);
  return *result;
}
QAction& action(ShellWindow& window, const char* name) {
  auto* result = window.findChild<QAction*>(QStringLiteral("blcad.action.") + name);
  REQUIRE(result);
  return *result;
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
void select(QComboBox& combo, const std::string& id) {
  const auto index = combo.findData(QString::fromStdString(id));
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void begin(ShellWindow& window, Kind kind) {
  action(window, "create_construction").trigger();
  REQUIRE(window.construction_tools().active());
  control<QComboBox>(window, "kind").setCurrentIndex(static_cast<int>(kind));
}
void apply(ShellWindow& window) {
  REQUIRE(control<QPushButton>(window, "apply").isEnabled());
  control<QPushButton>(window, "apply").click();
  REQUIRE_FALSE(window.construction_tools().active());
  REQUIRE_FALSE(window.session().task().active());
  REQUIRE_FALSE(window.viewport().feature_preview_active());
}
void origin(ShellWindow& window, Point3 point) {
  control<QDoubleSpinBox>(window, "origin.x").setValue(point.x);
  control<QDoubleSpinBox>(window, "origin.y").setValue(point.y);
  control<QDoubleSpinBox>(window, "origin.z").setValue(point.z);
}
std::string point(ShellWindow& window, Point3 position) {
  begin(window, Kind::Point);
  origin(window, position);
  apply(window);
  return window.session().part_document()->construction_points().back().id().value();
}
void authored_body(ShellWindow& window) {
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({0, 0}));
  REQUIRE(window.sketch_tools().add_pick({20, 10}));
  REQUIRE(window.finish_active_sketch());
  window.feature_tools().extrude_action().trigger();
  REQUIRE(window.feature_tools().apply());
}
QTreeWidgetItem* tree_item(QTreeWidgetItem* item, const QString& id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int i = 0; i < item->childCount(); ++i)
    if (auto* found = tree_item(item->child(i), id))
      return found;
  return nullptr;
}
void select_tree(ShellWindow& window, const std::string& id) {
  auto& tree = window.model_browser();
  QTreeWidgetItem* found{};
  for (int i = 0; i < tree.topLevelItemCount() && !found; ++i)
    found = tree_item(tree.topLevelItem(i), QString::fromStdString(id));
  REQUIRE(found);
  tree.setCurrentItem(found);
}
void click(QWidget& widget) {
  const QPointF local(widget.rect().center());
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, widget.mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
  }
  qApp->processEvents();
}
} // namespace

TEST_CASE("Construction editor previews explicit geometry and commits one reversible operation",
          "[gui][block136-construction][shell-construction-explicit]") {
  for (auto kind : {Kind::Point, Kind::Line, Kind::Axis, Kind::Plane}) {
    CAPTURE(static_cast<int>(kind));
    ShellWindow window;
    REQUIRE_FALSE(action(window, "create_construction").isEnabled());
    REQUIRE(window.new_part("Konstruktion"));
    QTemporaryDir directory;
    REQUIRE(window.save_to(directory.filePath("before.blcad.json")));
    const auto before = snapshot(window);
    const auto revision = window.session().presentation_revision();
    const auto undo = std::string(window.session().undo_label());
    begin(window, kind);
    origin(window, {12, -8, 5});
    control<QLineEdit>(window, "name").setText("Hilfe");
    REQUIRE(window.viewport().feature_preview_count() == 1);
    CHECK(snapshot(window) == before);
    CHECK(window.session().presentation_revision() == revision);
    CHECK(window.session().undo_label().empty());
    CHECK_FALSE(window.session().dirty());
    REQUIRE(window.save_to(directory.filePath("preview.blcad.json")));
    ShellWindow preview_file;
    REQUIRE(preview_file.open_path(directory.filePath("preview.blcad.json")));
    CHECK(snapshot(preview_file) == before);
    // Saving changes the session revision and expires the disposable task.
    CHECK_FALSE(window.construction_tools().active());
    CHECK(window.session().undo_label() == undo);
    begin(window, kind);
    origin(window, {12, -8, 5});
    control<QLineEdit>(window, "name").setText("Hilfe");
    apply(window);
    const auto after = snapshot(window);
    CHECK(after != before);
    CHECK(window.session().dirty());
    const auto& part = *window.session().part_document();
    std::string semantic;
    switch (kind) {
    case Kind::Point:
      CHECK(part.construction_points().back().position() == Point3{12, -8, 5});
      semantic = "construction-point/" + part.construction_points().back().id().value();
      break;
    case Kind::Line:
      CHECK(part.construction_lines().back().point() == Point3{12, -8, 5});
      semantic = "construction-line/" + part.construction_lines().back().id().value();
      break;
    case Kind::Axis:
      CHECK(part.datum_axes().back().origin() == Point3{12, -8, 5});
      semantic = "datum-axis/" + part.datum_axes().back().id().value();
      break;
    case Kind::Plane:
      CHECK(part.construction_planes().back().origin() == Point3{12, -8, 5});
      semantic = "construction-plane/" + part.construction_planes().back().id().value();
      break;
    default:
      FAIL("Unexpected kind");
    }
    REQUIRE(window.viewport().select_semantic(semantic));
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().dirty());
    REQUIRE(window.redo());
    CHECK(snapshot(window) == after);
    REQUIRE(window.save_to(directory.filePath("after.blcad.json")));
    ShellWindow reopened;
    REQUIRE(reopened.open_path(directory.filePath("after.blcad.json")));
    CHECK(snapshot(reopened) == after);
    CHECK(reopened.viewport().select_semantic(semantic));
  }
}

TEST_CASE("Construction relationships resolve picked points lines and three-point workplanes",
          "[gui][block136-construction][shell-construction-relations]") {
  ShellWindow window;
  REQUIRE(window.new_part("Beziehungen"));
  const auto a = point(window, {2, 3, 7});
  const auto b = point(window, {12, 3, 7});
  const auto c = point(window, {2, 13, 7});
  begin(window, Kind::LineThroughPoints);
  select(control<QComboBox>(window, "point1"), a);
  select(control<QComboBox>(window, "point2"), b);
  apply(window);
  const auto line = window.session().part_document()->construction_lines().back().id();
  auto resolved = checked(
      geometry::ConstructionLineResolver{}.resolve(*window.session().part_document(), line));
  CHECK(resolved.point == Point3{2, 3, 7});
  CHECK(resolved.direction == Vector3{1, 0, 0});
  REQUIRE(window.viewport().select_semantic("construction-line/" + line.value()));
  begin(window, Kind::AxisFromLine);
  select(control<QComboBox>(window, "line"), line.value());
  apply(window);
  const auto axis = window.session().part_document()->datum_axes().back().id();
  CHECK(window.session().part_document()->datum_axes().back().source_construction_line() == line);
  REQUIRE(window.viewport().select_semantic("datum-axis/" + axis.value()));
  const auto before = snapshot(window);
  begin(window, Kind::PlaneThroughPoints);
  select(control<QComboBox>(window, "point1"), a);
  select(control<QComboBox>(window, "point2"), b);
  select(control<QComboBox>(window, "point3"), c);
  apply(window);
  const auto plane = window.session().part_document()->construction_planes().back().workplane_id();
  const auto frame =
      checked(geometry::WorkplaneResolver{}.resolve(*window.session().part_document(), plane));
  CHECK(frame.origin == Point3{2, 3, 7});
  CHECK(frame.normal == Vector3{0, 0, 1});
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  select_tree(window, plane.value());
  REQUIRE(action(window, "create_sketch").isEnabled());
  action(window, "create_sketch").trigger();
  REQUIRE(window.active_sketch());
  CHECK(window.session().part_document()->find_sketch(*window.active_sketch())->workplane() ==
        plane);
  const auto mapped =
      checked(GuiSketchWorkbench{}.plane_view(window.session(), *window.active_sketch()));
  CHECK(mapped.plane_to_model({4, 5}) == Point3{6, 8, 7});
}

TEST_CASE("Offset workplane provisions its parameter atomically and reuses existing bindings",
          "[gui][block136-construction][shell-construction-offset]") {
  ShellWindow window;
  REQUIRE(window.new_part("Versatz"));
  const auto before = snapshot(window);
  action(window, "create_workplane").trigger();
  REQUIRE(window.construction_tools().active());
  select(control<QComboBox>(window, "plane"), "datum.xz");
  control<QDoubleSpinBox>(window, "offset").setValue(15);
  CHECK(window.session().part_document()->parameter_count() == 0);
  CHECK(window.viewport().feature_preview_count() == 1);
  apply(window);
  const auto& part = *window.session().part_document();
  REQUIRE(part.parameter_count() == 1);
  auto plane = part.construction_planes().back().workplane_id();
  const auto parameter = part.parameters().back().id();
  CHECK(checked(geometry::WorkplaneResolver{}.resolve(part, plane)).origin == Point3{0, -15, 0});
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  CHECK(window.session().part_document()->parameter_count() == 0);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  begin(window, Kind::OffsetPlane);
  select(control<QComboBox>(window, "plane"), plane.value());
  select(control<QComboBox>(window, "parameter"), parameter.value());
  CHECK_FALSE(control<QDoubleSpinBox>(window, "offset").isEnabled());
  apply(window);
  CHECK(window.session().part_document()->parameter_count() == 1);
  const auto chained =
      window.session().part_document()->construction_planes().back().workplane_id();
  CHECK(checked(geometry::WorkplaneResolver{}.resolve(*window.session().part_document(), chained))
            .origin == Point3{0, -30, 0});
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("planes.blcad.json")));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("planes.blcad.json")));
  CHECK(snapshot(reopened) == snapshot(window));
  REQUIRE(reopened.viewport().select_semantic("construction-plane/" + chained.value()));
  REQUIRE(reopened.create_sketch_on_selection());
  CHECK(reopened.session().part_document()->find_sketch(*reopened.active_sketch())->workplane() ==
        chained);
}

TEST_CASE("Derived face planes remain semantic intent and support downstream sketch extrusion",
          "[gui][block136-construction][shell-construction-face]") {
  ShellWindow window;
  REQUIRE(window.new_part("Flächenebene"));
  authored_body(window);
  begin(window, Kind::FacePlane);
  auto& faces = control<QComboBox>(window, "face");
  REQUIRE(faces.count() == 6);
  for (int i = 0; i < faces.count(); ++i)
    if (faces.itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(SemanticFace::Top))
      faces.setCurrentIndex(i);
  apply(window);
  const auto& derived = window.session().part_document()->derived_workplanes().back();
  const auto plane = derived.id();
  CHECK(derived.face_reference().face() == SemanticFace::Top);
  CHECK(derived.face_reference().source_feature() ==
        window.session().part_document()->features().front().id());
  select_tree(window, plane.value());
  REQUIRE(action(window, "create_sketch").isEnabled());
  action(window, "create_sketch").trigger();
  REQUIRE(window.active_sketch());
  CHECK(window.session().part_document()->find_sketch(*window.active_sketch())->workplane() ==
        plane);
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({-3, -2}));
  REQUIRE(window.sketch_tools().add_pick({3, 2}));
  REQUIRE(window.finish_active_sketch());
  window.feature_tools().extrude_action().trigger();
  checked(window.feature_tools().apply());
  CHECK(window.session().part_document()->feature_count() == 2);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("face.blcad.json")));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("face.blcad.json")));
  CHECK(snapshot(reopened) == snapshot(window));
  CHECK(reopened.viewport().select_semantic("derived-workplane/" + plane.value()));
}

TEST_CASE("Invalid construction candidates remove previews and leave history unchanged",
          "[gui][block136-construction][shell-construction-invalid]") {
  ShellWindow window;
  REQUIRE(window.new_part("Ungültig"));
  for (auto kind : {Kind::Line, Kind::Plane, Kind::LineThroughPoints, Kind::AxisFromLine,
                    Kind::PlaneThroughPoints, Kind::FacePlane}) {
    CAPTURE(static_cast<int>(kind));
    const auto before = snapshot(window);
    const auto revision = window.session().presentation_revision();
    const auto undo = std::string(window.session().undo_label());
    begin(window, kind);
    if (kind == Kind::Line)
      control<QDoubleSpinBox>(window, "direction.x").setValue(0);
    if (kind == Kind::Plane) {
      control<QDoubleSpinBox>(window, "y_direction.x").setValue(1);
      control<QDoubleSpinBox>(window, "y_direction.y").setValue(0);
    }
    CHECK_FALSE(control<QPushButton>(window, "apply").isEnabled());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(window.construction_tools().apply().has_error());
    CHECK(snapshot(window) == before);
    CHECK(window.session().presentation_revision() == revision);
    control<QPushButton>(window, "cancel").click();
    CHECK(window.session().undo_label() == undo);
  }
  const auto a = point(window, {0, 0, 0});
  const auto b = point(window, {10, 0, 0});
  const auto c = point(window, {20, 0, 0});
  begin(window, Kind::PlaneThroughPoints);
  select(control<QComboBox>(window, "point1"), a);
  select(control<QComboBox>(window, "point2"), b);
  select(control<QComboBox>(window, "point3"), c);
  CHECK(window.construction_tools().apply().has_error());
  CHECK_FALSE(window.viewport().feature_preview_active());
  control<QComboBox>(window, "kind").setCurrentIndex(static_cast<int>(Kind::LineThroughPoints));
  select(control<QComboBox>(window, "point2"), a);
  CHECK(window.construction_tools().apply().has_error());
  CHECK_FALSE(window.viewport().feature_preview_active());
  window.construction_tools().cancel();
}

TEST_CASE("Construction cancel Escape close and stale revision discard only transient intent",
          "[gui][block136-construction][shell-construction-lifecycle]") {
  ShellWindow window;
  REQUIRE(window.new_part("Abbrechen"));
  const auto before = snapshot(window);
  for (int mode = 0; mode < 3; ++mode) {
    begin(window, Kind::OffsetPlane);
    REQUIRE(window.viewport().feature_preview_active());
    CHECK_FALSE(action(window, "create_construction").isEnabled());
    CHECK(window.construction_tools().begin(Kind::Point).has_error());
    if (mode == 0)
      control<QPushButton>(window, "cancel").click();
    if (mode == 1) {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&control<QLineEdit>(window, "name"), &escape);
    }
    if (mode == 2) {
      auto* dock = window.findChild<QDockWidget*>("blcad.shell.construction_task");
      REQUIRE(dock);
      dock->close();
    }
    CHECK_FALSE(window.construction_tools().active());
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(snapshot(window) == before);
  }
  begin(window, Kind::OffsetPlane);
  REQUIRE(window.session().commit_part_transaction("External change", [](PartDocument& part) {
    return part.add_construction_point(checked(ConstructionPoint::create_explicit(
        ConstructionPointId("external.point"), "External", {0, 0, 8})));
  }));
  CHECK(window.construction_tools().apply().has_error());
  window.refresh();
  CHECK_FALSE(window.construction_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(window.session().part_document()->parameter_count() == 0);
  begin(window, Kind::Point);
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(window.construction_tools().active());
  CHECK_FALSE(action(window, "create_construction").isEnabled());
  window.session().set_derived_results_fresh(true);
  window.refresh();
  begin(window, Kind::OffsetPlane);
  REQUIRE(window.undo());
  CHECK_FALSE(window.construction_tools().active());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(window.session().part_document()->find_construction_point(
      ConstructionPointId("external.point")));
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("part.blcad.json")));
  REQUIRE(window.new_project("Projekt"));
  CHECK_FALSE(action(window, "create_construction").isEnabled());
}

TEST_CASE("Construction identifiers avoid existing objects and generated parameter collisions",
          "[gui][block136-construction][shell-construction-identity]") {
  ShellWindow window;
  REQUIRE(window.new_part("Identitäten"));
  REQUIRE(window.session().commit_part_transaction("Reserved identity", [](PartDocument& part) {
    return part.add_parameter(checked(Parameter::create_length(
        ParameterId("construction.shell.1.offset"), "Reserved", checked(Quantity::length_mm(4)))));
  }));
  REQUIRE(window.session().commit_part_transaction("Suffix identity", [](PartDocument& part) {
    return part.add_construction_point(checked(ConstructionPoint::create_explicit(
        ConstructionPointId("nested/construction.shell.2"), "Suffix", {0, 0, 8})));
  }));
  window.refresh();
  begin(window, Kind::OffsetPlane);
  control<QLineEdit>(window, "name").setText("   ");
  CHECK(window.construction_tools().apply().has_error());
  CHECK_FALSE(window.viewport().feature_preview_active());
  control<QLineEdit>(window, "name").setText("Versatz");
  apply(window);
  CHECK(window.session().part_document()->construction_planes().back().id().value() ==
        "construction.shell.2");
  CHECK(window.session()
            .part_document()
            ->find_parameter(ParameterId("construction.shell.1.offset"))
            ->value()
            .millimeters() == 4);
}

TEST_CASE(
    "Native construction task remains usable and previews an offset workplane at desktop size",
    "[gui][block136-construction][shell-construction-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Native construction task requires Qt xcb and OCCT");
  ShellWindow window;
  REQUIRE(window.new_part("Native Konstruktion"));
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  REQUIRE(window.viewport().native_viewer_available());
  QToolButton* entry{};
  for (auto* button : window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() == &action(window, "create_workplane"))
      entry = button;
  REQUIRE(entry);
  REQUIRE(entry->isVisible());
  auto* ancestor = entry->parentWidget();
  while (ancestor && !qobject_cast<QScrollArea*>(ancestor))
    ancestor = ancestor->parentWidget();
  REQUIRE(ancestor);
  static_cast<QScrollArea*>(ancestor)->ensureWidgetVisible(entry);
  qApp->processEvents();
  REQUIRE(window.childAt(entry->mapTo(&window, entry->rect().center())) == entry);
  click(*entry);
  qApp->processEvents();
  REQUIRE(window.construction_tools().active());
  auto& offset = control<QDoubleSpinBox>(window, "offset");
  offset.setFocus();
  offset.selectAll();
  for (const auto& text : {QStringLiteral("2"), QStringLiteral("5")}) {
    QKeyEvent key(QEvent::KeyPress, text.at(0).unicode(), Qt::NoModifier, text);
    QApplication::sendEvent(&offset, &key);
  }
  // Apply must consume the pending value without Enter, Tab or focus-out.
  auto& apply_button = control<QPushButton>(window, "apply");
  qApp->processEvents();
  REQUIRE(apply_button.isVisible());
  REQUIRE(apply_button.isEnabled());
  REQUIRE(window.childAt(apply_button.mapTo(&window, apply_button.rect().center())) ==
          &apply_button);
  CHECK(window.rect().contains(apply_button.mapTo(&window, apply_button.rect().bottomRight())));
  REQUIRE(window.viewport().feature_preview_count() == 1);
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty()) {
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath("construction-offset.png")));
  }
  click(apply_button);
  REQUIRE_FALSE(window.construction_tools().active());
  const auto& plane = window.session().part_document()->construction_planes().back();
  CHECK(checked(geometry::WorkplaneResolver{}.resolve(*window.session().part_document(),
                                                      plane.workplane_id()))
            .origin.z == Approx(25));
  REQUIRE(window.viewport().select_semantic("construction-plane/" + plane.id().value()));
  REQUIRE(window.create_sketch_on_selection());
  CHECK_FALSE(action(window, "create_construction").isEnabled());
  CHECK(window.construction_tools().begin(Kind::Point).has_error());
}
