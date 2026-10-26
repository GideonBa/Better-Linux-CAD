#include "../support/reference_repair_fixture.hpp"
#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_diagnostics_tools.hpp"
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
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
namespace {
template <typename T> T& control(ShellWindow& window, const char* name) {
  auto* result = window.findChild<T*>(QStringLiteral("blcad.sketch_diagnostics.") + name);
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
QAction& action(ShellWindow& window, const char* name = "sketch_diagnostics") {
  auto* result = window.findChild<QAction*>(QStringLiteral("blcad.action.") + name);
  REQUIRE(result);
  return *result;
}
QString seed(ShellWindow& window, const QTemporaryDir& directory) {
  auto part =
      reference_checked(PartDocument::create(DocumentId("part.diagnostics"), "Diagnostics"));
  for (const auto& [id, value] : {std::pair{"width", 20.0}, {"height", 10.0}, {"depth", 8.0}})
    REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
        ParameterId(id), id, reference_checked(Quantity::length_mm(value, id))))));
  REQUIRE(part.add_datum_plane(reference_checked(DatumPlane::xy())));
  auto sketch =
      reference_checked(Sketch::create(SketchId("review"), "Review", DatumPlaneId("datum.xy")));
  for (const std::string id : {"line", "free", "conflict"})
    REQUIRE(sketch.add_entity(
        reference_checked(LineSegment::create(SketchEntityId(id), {0, 0}, {20, 0}))));
  REQUIRE(sketch.add_entity(reference_checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline"), {0, 20}, {5, 25}, {10, 25}, {20, 20}))));
  const auto start =
      reference_checked(SketchReferenceTarget::create_line_segment_start(SketchEntityId("line")));
  const auto end =
      reference_checked(SketchReferenceTarget::create_line_segment_end(SketchEntityId("line")));
  for (const std::string id : {"a", "z"}) {
    REQUIRE(sketch.add_constraint(reference_checked(
        SketchGeometricConstraint::create_fixed(SketchConstraintId("fixed." + id), start))));
    REQUIRE(
        sketch.add_dimension(reference_checked(SketchDrivingDimension::create_horizontal_distance(
            SketchDimensionId("dimension." + id), start, end, ParameterId("width")))));
  }
  const auto line =
      reference_checked(SketchReferenceTarget::create_line_segment(SketchEntityId("conflict")));
  REQUIRE(sketch.add_constraint(reference_checked(
      SketchGeometricConstraint::create_horizontal(SketchConstraintId("horizontal"), line))));
  REQUIRE(sketch.add_constraint(reference_checked(SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("horizontal.duplicate"), line))));
  REQUIRE(sketch.add_constraint(reference_checked(
      SketchGeometricConstraint::create_vertical(SketchConstraintId("vertical"), line))));
  REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_feature(reference_checked(Feature::create_additive_extrude(
      FeatureId("solid"), "Solid", sketch.id(), ParameterId("depth")))));
  auto other =
      reference_checked(Sketch::create(SketchId("other"), "Other", DatumPlaneId("datum.xy")));
  REQUIRE(other.add_profile(reference_checked(RectangleProfile::create(
      ProfileId("other.profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_sketch(other));
  REQUIRE(part.add_sketch(
      reference_checked(Sketch::create(SketchId("empty"), "Empty", DatumPlaneId("datum.xy")))));
  const auto path = directory.filePath("diagnostics.blcad.json");
  REQUIRE(write_part_document_json_file(part, path.toStdString()));
  REQUIRE(window.open_path(path));
  REQUIRE(window.session().commit_part_sketch_intent_transaction(
      "Seed accepted catalogs",
      [](PartDocument& document, std::vector<SketchConstraintCatalog>& constraints,
         std::vector<SketchDimensionCatalog>& dimensions) {
        auto target = reference_checked(SketchConstraintIntentTarget::entity("entity/free"));
        auto constraint_catalog =
            reference_checked(SketchConstraintCatalog::create(document.id(), SketchId("review")));
        REQUIRE(constraint_catalog.add(reference_checked(
            SketchConstraintIntent::create(SketchConstraintId("accepted.horizontal"),
                                           SketchSolverConstraintKind::Horizontal, {target}))));
        auto dimension_catalog =
            reference_checked(SketchDimensionCatalog::create(document.id(), SketchId("review")));
        REQUIRE(dimension_catalog.add(reference_checked(SketchDimensionIntent::create(
            SketchDimensionId("accepted.length"), SketchDimensionKind::Length,
            SketchDimensionMode::Reference, {target}))));
        constraints.push_back(std::move(constraint_catalog));
        dimensions.push_back(std::move(dimension_catalog));
        return Result<std::size_t>::success(2);
      }));
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  return path;
}
void select(ShellWindow& window, const QString& id) {
  auto& list = control<QListWidget>(window, "diagnostics");
  for (int row = 0; row < list.count(); ++row)
    if (list.item(row)->data(Qt::UserRole).toString() == id) {
      list.setCurrentRow(row);
      return;
    }
  FAIL("Diagnostic missing: " << id.toStdString());
}
void begin(ShellWindow& window) {
  REQUIRE(action(window).isEnabled());
  action(window).trigger();
  REQUIRE(window.sketch_diagnostics_tools().active());
}
void choose_sketch(ShellWindow& window, const QString& id) {
  auto& combo = control<QComboBox>(window, "sketch");
  const int index = combo.findData(id);
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void preview(ShellWindow& window) {
  REQUIRE(window.sketch_diagnostics_tools().preview());
  REQUIRE(control<QPushButton>(window, "apply").isEnabled());
  REQUIRE(window.viewport().feature_preview_active());
}
QTreeWidgetItem* sketch_item(QTreeWidget& tree, QTreeWidgetItem* parent = nullptr) {
  const int count = parent ? parent->childCount() : tree.topLevelItemCount();
  for (int index = 0; index < count; ++index) {
    auto* item = parent ? parent->child(index) : tree.topLevelItem(index);
    if (item->data(0, Qt::UserRole).toString() == "review")
      return item;
    if (auto* result = sketch_item(tree, item))
      return result;
  }
  return nullptr;
}
void enter(ShellWindow& window) {
  auto* item = sketch_item(window.model_browser());
  REQUIRE(item);
  window.model_browser().itemDoubleClicked(item, 0);
  REQUIRE(window.active_sketch() == SketchId("review"));
}
void click(QWidget& widget, QPoint point = {}) {
  REQUIRE(widget.isVisible());
  if (point.isNull())
    point = widget.rect().center();
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, QPointF(point), widget.mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
  }
  qApp->processEvents();
}
void ribbon(ShellWindow& window, const char* name, int tab) {
  window.ribbon().set_current_tab(tab);
  qApp->processEvents();
  for (auto* button : window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() == &action(window, name) && button->isVisible()) {
      for (auto* parent = button->parentWidget(); parent; parent = parent->parentWidget())
        if (auto* scroll = qobject_cast<QScrollArea*>(parent))
          scroll->ensureWidgetVisible(button);
      qApp->processEvents();
      REQUIRE(!button->visibleRegion().isEmpty());
      click(*button);
      return;
    }
  FAIL("Ribbon button missing");
}
void mouse_select(ShellWindow& window, const QString& id) {
  auto& list = control<QListWidget>(window, "diagnostics");
  for (int row = 0; row < list.count(); ++row) {
    auto* item = list.item(row);
    if (item->data(Qt::UserRole).toString() == id) {
      list.scrollToItem(item);
      qApp->processEvents();
      click(*list.viewport(), list.visualItemRect(item).center());
      REQUIRE(list.currentItem() == item);
      return;
    }
  }
  FAIL("Diagnostic missing: " << id.toStdString());
}

} // namespace

TEST_CASE(
    "Sketch diagnostics expose every Core message and explicitly block unsupported suggestions",
    "[gui][block136-sketch-diagnostics]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  const auto before = snapshot(window);
  begin(window);
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  select(window, "conflicting_horizontal_vertical_constraints/conflict");
  CHECK(control<QComboBox>(window, "suggestion").currentData() ==
        "remove_conflicting_orientation_constraint");
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  CHECK(control<QLabel>(window, "summary").text().contains("Manual"));
  CHECK_FALSE(window.sketch_diagnostics_tools().preview());
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  select(window, "free_spline_control_point/spline:control1");
  CHECK(control<QComboBox>(window, "suggestion").count() == 0);
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  select(window, "redundant_orientation_constraint/conflict");
  CHECK(control<QComboBox>(window, "suggestion").count() == 0);
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  choose_sketch(window, "other");
  CHECK(control<QComboBox>(window, "suggestion").currentData() == "add_driving_dimension");
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  choose_sketch(window, "empty");
  CHECK(control<QListWidget>(window, "diagnostics").count() == 0);
  CHECK(control<QLabel>(window, "summary").text().contains("No diagnostics"));
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
}

TEST_CASE(
    "Diagnostic repair previews are disposable and each supported action owns one history step",
    "[gui][block136-sketch-diagnostics]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto constraints = bytes(path + ".sketch-constraints.json");
  const auto dimensions = bytes(path + ".sketch-dimensions.json");
  for (const QString diagnostic :
       {"duplicate_fixed_endpoint/line:start",
        "duplicate_driving_dimension_target/line_segment_end:line|line_segment_start:line",
        "unconstrained_line_endpoint/free:start"}) {
    CAPTURE(diagnostic.toStdString());
    const auto before = snapshot(window);
    begin(window);
    select(window, diagnostic);
    preview(window);
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().dirty());
    CHECK_FALSE(window.session().can_undo());
    CHECK(bytes(path).toStdString() == before);
    CHECK(bytes(path + ".sketch-constraints.json") == constraints);
    CHECK(bytes(path + ".sketch-dimensions.json") == dimensions);
    REQUIRE(window.sketch_diagnostics_tools().apply());
    const auto after = snapshot(window);
    CHECK(after != before);
    CHECK_FALSE(window.sketch_diagnostics_tools().active());
    REQUIRE(window.undo());
    CHECK(snapshot(window) == before);
    CHECK_FALSE(window.session().can_undo());
    REQUIRE(window.redo());
    CHECK(snapshot(window) == after);
    REQUIRE(window.save_to({}));
    REQUIRE(window.open_path(path));
    CHECK(snapshot(window) == after);
    CHECK(bytes(path + ".sketch-constraints.json") == constraints);
    CHECK(bytes(path + ".sketch-dimensions.json") == dimensions);
  }
  const auto* sketch = window.session().part_document()->find_sketch(SketchId("review"));
  CHECK(sketch->find_geometric_constraint(SketchConstraintId("fixed.a")));
  CHECK_FALSE(sketch->find_geometric_constraint(SketchConstraintId("fixed.z")));
  CHECK(sketch->find_driving_dimension(SketchDimensionId("dimension.a")));
  CHECK_FALSE(sketch->find_driving_dimension(SketchDimensionId("dimension.z")));
  CHECK(sketch->find_geometric_constraint(SketchConstraintId("repair.fixed.free.start")));
}

TEST_CASE("Diagnostic selection cancellation saves and revision changes revoke previews",
          "[gui][block136-sketch-diagnostics]") {
  ShellWindow window;
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto before = snapshot(window);
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  select(window, "unconstrained_line_endpoint/free:start");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  preview(window);
  choose_sketch(window, "other");
  CHECK_FALSE(window.viewport().feature_preview_active());
  window.sketch_diagnostics_tools().cancel();
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(window.save_to({}));
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK(bytes(path).toStdString() == before);
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  window.session().set_derived_results_fresh(false);
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().recompute());
  window.refresh();
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  auto* dock = window.findChild<QDockWidget*>("blcad.shell.sketch_diagnostics_task");
  REQUIRE(dock);
  dock->close();
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(window.new_part("Replacement"));
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.sketch_diagnostics_tools().preview());
  REQUIRE(window.save_to(directory.filePath("replacement.blcad.json")));
  REQUIRE(window.new_project("Project"));
  CHECK_FALSE(action(window).isEnabled());
}

TEST_CASE("Sketch diagnostics suspend active authoring and restore committed interactions on exit",
          "[gui][block136-sketch-diagnostics]") {
  ShellWindow window;
  QTemporaryDir directory;
  seed(window, directory);
  enter(window);
  const auto before = snapshot(window);
  REQUIRE(window.viewport().sketch_interaction_active());
  begin(window);
  CHECK_FALSE(window.viewport().sketch_interaction_active());
  CHECK_FALSE(control<QComboBox>(window, "sketch").isEnabled());
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&control<QListWidget>(window, "diagnostics"), &escape);
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(snapshot(window) == before);
  begin(window);
  choose_sketch(window, "other"); // A disabled selector cannot redirect an editing task.
  CHECK_FALSE(window.sketch_diagnostics_tools().preview());
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(snapshot(window) == before);
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(window.sketch_diagnostics_tools().apply());
  CHECK(window.viewport().sketch_interaction_active());
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(window.active_sketch() == SketchId("review"));
  begin(window);
  select(window, "unconstrained_line_endpoint/free:start");
  preview(window);
  CHECK_FALSE(window.finish_active_sketch()); // Remaining orientation conflict is explicit.
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK(window.active_sketch() == SketchId("review"));
  CHECK(window.viewport().sketch_interaction_active());
}

TEST_CASE(
    "Diagnostic context guards protect foreign tasks revisions and existing constraint identities",
    "[gui][block136-sketch-diagnostics]") {
  ShellWindow window;
  CHECK_FALSE(action(window).isEnabled());
  CHECK_FALSE(window.sketch_diagnostics_tools().begin());
  QTemporaryDir directory;
  seed(window, directory);
  REQUIRE(window.session().task().begin("foreign.command"));
  window.refresh();
  CHECK_FALSE(action(window).isEnabled());
  CHECK_FALSE(window.sketch_diagnostics_tools().begin());
  CHECK(window.session().task().command_id() == "foreign.command");
  REQUIRE(window.session().task().cancel());
  REQUIRE(
      window.session().commit_part_transaction("Seed available history", [](PartDocument& part) {
        return part.add_parameter(reference_checked(
            Parameter::create_length(ParameterId("history"), "History",
                                     reference_checked(Quantity::length_mm(2, "history")))));
      }));
  window.refresh();
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(window.undo());
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(window.redo());
  CHECK_FALSE(window.sketch_diagnostics_tools().active());
  begin(window);
  select(window, "duplicate_fixed_endpoint/line:start");
  preview(window);
  REQUIRE(
      window.session().commit_part_transaction("External parameter edit", [](PartDocument& part) {
        return part.add_parameter(reference_checked(
            Parameter::create_length(ParameterId("external"), "External",
                                     reference_checked(Quantity::length_mm(1, "external")))));
      }));
  const auto edited = snapshot(window);
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  CHECK(snapshot(window) == edited);
  CHECK(window.session()
            .part_document()
            ->find_sketch(SketchId("review"))
            ->find_geometric_constraint(SketchConstraintId("fixed.z")));
  REQUIRE(
      window.session().commit_part_transaction("Reserve repair identity", [](PartDocument& part) {
        auto sketch = *part.find_sketch(SketchId("review"));
        REQUIRE(sketch.add_constraint(reference_checked(SketchGeometricConstraint::create_fixed(
            SketchConstraintId("repair.fixed.free.start"),
            reference_checked(
                SketchReferenceTarget::create_line_segment_end(SketchEntityId("line")))))));
        return part.update_sketch(sketch);
      }));
  REQUIRE(window.save_to({}));
  const auto reserved = snapshot(window);
  begin(window);
  select(window, "unconstrained_line_endpoint/free:start");
  CHECK_FALSE(control<QPushButton>(window, "preview").isEnabled());
  CHECK_FALSE(window.sketch_diagnostics_tools().preview());
  CHECK_FALSE(window.sketch_diagnostics_tools().apply());
  CHECK(snapshot(window) == reserved);
  window.sketch_diagnostics_tools().cancel();
  const auto failed = directory.filePath("failed.blcad.json");
  REQUIRE(write_part_document_json_file(test::repair_open_fixture(), failed.toStdString()));
  REQUIRE(window.open_path(failed, GuiDocumentOpenMode::ReferenceRepair));
  CHECK_FALSE(action(window).isEnabled());
  CHECK_FALSE(window.sketch_diagnostics_tools().begin());
  CHECK(window.session().task().command_id() == "reference.repair");
}

TEST_CASE("Native ribbon list preview cancel apply history and save combine diagnostic repairs",
          "[gui][block136-sketch-diagnostics][shell-sketch-diagnostics-input]") {
  ShellWindow window;
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  QTemporaryDir directory;
  const auto path = seed(window, directory);
  const auto before = snapshot(window);
  ribbon(window, "sketch_diagnostics", 0);
  mouse_select(window, "duplicate_fixed_endpoint/line:start");
  click(control<QPushButton>(window, "preview"));
  REQUIRE(window.viewport().feature_preview_active());
  ribbon(window, "fit", 2);
  if (QApplication::platformName() == "xcb") {
    const auto captures = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) {
      REQUIRE(QDir().mkpath(captures));
      CHECK(window.grab().save(QDir(captures).filePath("sketch-diagnostics-preview.png")));
    }
  }
  click(control<QPushButton>(window, "cancel"));
  CHECK(snapshot(window) == before);
  ribbon(window, "sketch_diagnostics", 0);
  mouse_select(window, "duplicate_fixed_endpoint/line:start");
  click(control<QPushButton>(window, "preview"));
  click(control<QPushButton>(window, "apply"));
  const auto after = snapshot(window);
  CHECK(after != before);
  ribbon(window, "undo", 0);
  CHECK(snapshot(window) == before);
  ribbon(window, "redo", 0);
  CHECK(snapshot(window) == after);
  ribbon(window, "save", 0);
  CHECK_FALSE(window.session().dirty());
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == after);
  enter(window);
  ribbon(window, "sketch_diagnostics", 1);
  mouse_select(window, "unconstrained_line_endpoint/free:start");
  click(control<QPushButton>(window, "preview"));
  click(control<QPushButton>(window, "apply"));
  CHECK(window.viewport().sketch_interaction_active());
  CHECK(window.session()
            .part_document()
            ->find_sketch(SketchId("review"))
            ->find_geometric_constraint(SketchConstraintId("repair.fixed.free.start")));
}
