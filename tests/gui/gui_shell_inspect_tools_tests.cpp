#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
template <typename T> T& control(ShellWindow& window, const char* name) {
  auto* item = window.findChild<T*>(QStringLiteral("blcad.inspect.") + name);
  REQUIRE(item);
  return *item;
}
QAction& action(ShellWindow& window, const char* id) {
  auto* item = window.inspect_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(item);
  return *item;
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
std::string snapshot(ShellWindow& window) {
  return window.session().project()
             ? checked(serialize_project_to_json(*window.session().project()))
             : checked(serialize_part_document_to_json(*window.session().part_document()));
}
void project(ShellWindow& window, const char* file = "revolute_joint.blcad.project.json") {
  REQUIRE(window.open_path(QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/" + file));
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
}
void part(ShellWindow& window) {
  REQUIRE(window.new_part("STEP acceptance"));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({0, 0}));
  REQUIRE(window.sketch_tools().add_pick({20, 10}));
  REQUIRE(window.finish_active_sketch());
  window.feature_tools().extrude_action().trigger();
  REQUIRE(window.feature_tools().active());
  REQUIRE(window.feature_tools().apply());
}
void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  CHECK(window.grab().save(QDir(directory).filePath(QString::fromUtf8(name) + ".png")));
}
} // namespace

TEST_CASE("Shell exports an authored Part without changing source history or persistence",
          "[gui][block136-inspect-export][shell-part-step]") {
  ShellWindow window;
  part(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("part.blcad.json")));
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  const auto redo = window.session().can_redo();
  const auto saved = bytes(directory.filePath("part.blcad.json"));
  REQUIRE(action(window, "export_part_step").isEnabled());
  CHECK_FALSE(action(window, "export_assembly_flat_step").isEnabled());
  const auto path = directory.filePath("part.step");
  auto result = checked(
      window.inspect_tools().export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}));
  CHECK(result.exported_item_count == 1);
  CHECK(result.written_bytes == static_cast<std::size_t>(bytes(path).size()));
  CHECK(bytes(path).startsWith("ISO-10303-21;"));
  CHECK(bytes(path).contains("blcad:body-definition:"));
  CHECK(window.inspect_tools()
            .export_step({GuiStepExportMode::PartMultiBody,
                          directory.filePath("part.blcad.json").toStdString()})
            .has_error());
  CHECK(bytes(directory.filePath("part.blcad.json")) == saved);
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK(window.session().undo_label() == undo);
  CHECK(window.session().can_redo() == redo);
  REQUIRE(window.save_to({}));
  CHECK(bytes(directory.filePath("part.blcad.json")) == saved);
  ShellWindow reopened;
  REQUIRE(reopened.open_path(directory.filePath("part.blcad.json")));
  CHECK(snapshot(reopened) == before);
}
TEST_CASE("Shell exports flat and structured nested assembly occurrences without changing poses",
          "[gui][block136-inspect-export][shell-assembly-step]") {
  ShellWindow window;
  project(window, "nested_subassembly.blcad.project.json");
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  for (auto mode : {GuiStepExportMode::AssemblyFlattened, GuiStepExportMode::AssemblyStructured}) {
    const auto path = directory.filePath(
        mode == GuiStepExportMode::AssemblyFlattened ? "flat.step" : "structured.step");
    auto result = checked(window.inspect_tools().export_step({mode, path.toStdString()}));
    CHECK(result.exported_item_count > 1);
    CHECK(bytes(path).startsWith("ISO-10303-21;"));
    CHECK(bytes(path).size() == static_cast<qint64>(result.written_bytes));
  }
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
  CHECK_FALSE(window.session().can_redo());
  CHECK_FALSE(window.session().dirty());
}
TEST_CASE("Assembly analysis distinguishes collisions clearance and contact without writing intent",
          "[gui][block136-inspect-export][shell-assembly-analysis]") {
  ShellWindow window;
  project(window);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  action(window, "assembly_analysis").trigger();
  REQUIRE(window.inspect_tools().active());
  CHECK_FALSE(action(window, "export_assembly_structured_step").isEnabled());
  auto& rows = control<QTableWidget>(window, "results");
  for (int kind = 0; kind < 3; ++kind) {
    control<QComboBox>(window, "kind").setCurrentIndex(kind);
    control<QPushButton>(window, "run").click();
    REQUIRE(rows.rowCount() == 1);
    CHECK(rows.item(0, 2)->text().startsWith("Kollision:"));
    CHECK(rows.item(0, 0)->toolTip().contains("component.plate."));
    CHECK(rows.item(0, 0)->text().contains("Plate"));
  }
  control<QComboBox>(window, "kind").setCurrentIndex(3);
  CHECK(rows.rowCount() == 0);
  CHECK(window.inspect_tools().run_analysis().has_error());
  CHECK(rows.rowCount() == 0);
  auto& group = control<QListWidget>(window, "group");
  REQUIRE(group.count() == 2);
  for (int i = 0; i < group.count(); ++i)
    if (group.item(i)->data(Qt::UserRole).toString() == "component.plate.free")
      group.item(i)->setSelected(true);
  REQUIRE(window.inspect_tools().run_analysis());
  CHECK(control<QLabel>(window, "summary").text().contains("Verbleibende Freiheitsgrade: 6"));
  window.inspect_tools().cancel();
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
  CHECK_FALSE(window.session().dirty());
}
TEST_CASE("Analysis input changes and model freshness invalidate previous results",
          "[gui][block136-inspect-export][shell-assembly-analysis]") {
  ShellWindow window;
  project(window);
  REQUIRE(window.inspect_tools().begin_analysis());
  REQUIRE(window.inspect_tools().run_analysis());
  auto& rows = control<QTableWidget>(window, "results");
  REQUIRE(rows.rowCount() == 1);
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(window.inspect_tools().active());
  CHECK(rows.rowCount() == 0);
  CHECK_FALSE(action(window, "assembly_analysis").isEnabled());
  CHECK(window.inspect_tools().run_analysis().has_error());
  window.session().set_derived_results_fresh(true);
  window.refresh();
  REQUIRE(window.inspect_tools().begin_analysis());
  REQUIRE(window.inspect_tools().run_analysis());
  REQUIRE(window.session().commit_project_transaction("Move plate", [](Project& value) {
    return value.assembly().set_component_instance_transform(
        ComponentInstanceId("component.plate.free"), {{100, 0, 0}, {}});
  }));
  window.refresh();
  CHECK_FALSE(window.inspect_tools().active());
  CHECK(rows.rowCount() == 0);
  REQUIRE(window.inspect_tools().begin_analysis());
  control<QComboBox>(window, "kind").setCurrentIndex(2);
  REQUIRE(window.inspect_tools().run_analysis());
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text().startsWith("Getrennt:"));
  REQUIRE(window.undo());
  CHECK_FALSE(window.inspect_tools().active());
  CHECK(rows.rowCount() == 0);
}
TEST_CASE("Assembly clearance threshold and touching classification use current posed geometry",
          "[gui][block136-inspect-export][shell-assembly-analysis]") {
  ShellWindow window;
  project(window);
  const auto place = [&](double x) {
    REQUIRE(window.session().commit_project_transaction("Place plate", [=](Project& value) {
      return value.assembly().set_component_instance_transform(
          ComponentInstanceId("component.plate.free"), {{x, 0, 0}, {}});
    }));
    window.refresh();
    REQUIRE(window.inspect_tools().begin_analysis());
  };
  place(10.5);
  const auto before = snapshot(window);
  control<QComboBox>(window, "kind").setCurrentIndex(1);
  auto& threshold = control<QDoubleSpinBox>(window, "clearance");
  auto& rows = control<QTableWidget>(window, "results");
  threshold.setValue(1);
  REQUIRE(window.inspect_tools().run_analysis());
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text() == "Abstand: 0.5 mm");
  threshold.setValue(0.25);
  CHECK(rows.rowCount() == 0);
  REQUIRE(window.inspect_tools().run_analysis());
  CHECK(rows.rowCount() == 0);
  CHECK(snapshot(window) == before);
  window.inspect_tools().cancel();
  place(10);
  control<QComboBox>(window, "kind").setCurrentIndex(2);
  REQUIRE(window.inspect_tools().run_analysis());
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text() == "Berührung: 0 mm");
  auto* dock = window.findChild<QDockWidget*>("blcad.shell.inspect_task");
  REQUIRE(dock);
  dock->close();
  CHECK_FALSE(window.inspect_tools().active());
  CHECK_FALSE(window.session().task().active());
  CHECK(rows.rowCount() == 0);
}
TEST_CASE("Shell analysis and export reject busy stale empty and wrong document contexts",
          "[gui][block136-inspect-export]") {
  ShellWindow window;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("existing.step");
  QFile existing(path);
  REQUIRE(existing.open(QIODevice::WriteOnly));
  REQUIRE(existing.write("preserved") == 9);
  existing.close();
  auto& tools = window.inspect_tools();
  CHECK_FALSE(action(window, "export_part_step").isEnabled());
  CHECK(tools.export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}).has_error());
  CHECK(tools.begin_analysis().has_error());
  REQUIRE(window.new_part("Empty"));
  CHECK(tools.export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}).has_error());
  CHECK(bytes(path) == "preserved");
  CHECK(
      QDir(directory.path()).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
  REQUIRE(window.save_to(directory.filePath("empty.blcad.json")));
  part(window);
  CHECK(tools.export_step({GuiStepExportMode::AssemblyFlattened, path.toStdString()}).has_error());
  CHECK(tools.export_step({GuiStepExportMode::PartMultiBody, {}}).has_error());
  REQUIRE(window.session().task().begin("other.task"));
  window.refresh();
  CHECK(tools.export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}).has_error());
  CHECK(tools.begin_analysis().has_error());
  CHECK(window.session().task().command_id() == "other.task");
  REQUIRE(window.session().task().cancel());
  window.session().set_derived_results_fresh(false);
  window.refresh();
  CHECK_FALSE(action(window, "export_part_step").isEnabled());
  CHECK(tools.export_step({GuiStepExportMode::PartMultiBody, path.toStdString()}).has_error());
  CHECK(bytes(path) == "preserved");
}
TEST_CASE("Native shell analysis controls and STEP dialogs are reachable at 1280 by 800",
          "[gui][block136-inspect-export][native-shell-inspect]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native shell layout and file dialogs");
  const bool native_dialogs_disabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  struct RestoreDialogAttribute {
    bool value;
    ~RestoreDialogAttribute() {
      QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, value);
    }
  } restore_dialog_attribute{native_dialogs_disabled};
  ShellWindow window;
  project(window);
  window.ribbon().set_current_tab(3);
  auto& analysis = action(window, "assembly_analysis");
  QToolButton* open = nullptr;
  for (auto* candidate : window.findChildren<QToolButton*>())
    if (candidate->defaultAction() == &analysis)
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
  REQUIRE(window.inspect_tools().active());
  auto& run = control<QPushButton>(window, "run");
  qApp->processEvents();
  REQUIRE(window.childAt(run.mapTo(&window, run.rect().center())) == &run);
  run.click();
  window.viewport().fit_all();
  qApp->processEvents();
  REQUIRE(control<QTableWidget>(window, "results").rowCount() == 1);
  capture(window, "native-assembly-analysis");
  auto& close = control<QPushButton>(window, "close");
  REQUIRE(window.childAt(close.mapTo(&window, close.rect().center())) == &close);
  close.click();
  CHECK_FALSE(window.inspect_tools().active());
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("native.step");
  bool dialog_seen = false;
  QTimer::singleShot(0, [&] {
    if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
      dialog_seen = true;
      dialog->selectFile(path);
      QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    }
  });
  action(window, "export_assembly_structured_step").trigger();
  REQUIRE(dialog_seen);
  CHECK(bytes(path).startsWith("ISO-10303-21;"));
  capture(window, "native-step-complete");
  QTimer::singleShot(0, [] {
    if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
      dialog->reject();
  });
  action(window, "export_assembly_flat_step").trigger();
  CHECK(QDir(directory.path()).entryList(QDir::Files).size() == 1);
  REQUIRE(window.inspect_tools().begin_analysis());
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &escape);
  CHECK_FALSE(window.inspect_tools().active());
}
