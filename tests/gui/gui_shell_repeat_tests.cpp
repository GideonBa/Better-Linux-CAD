#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* result = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(result);
  return *result;
}
QAction& action(ShellWindow& window, const char* id) {
  return control<QAction>(window, (std::string("blcad.action.") + id).c_str());
}
void invoke(ShellWindow& window, const char* id) {
  auto& command = action(window, id);
  REQUIRE(command.isEnabled());
  command.trigger();
  qApp->processEvents();
}
QAction& repeat(ShellWindow& window) {
  return action(window, "repeat_last_command");
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
std::string seed(ShellWindow& window) {
  REQUIRE(window.new_part("Selection"));
  const auto empty = snapshot(window);
  REQUIRE(
      window.session().commit_part_transaction("Create selection fixture", [](PartDocument& part) {
        for (const auto& [id, value] :
             {std::pair{"width", 30.0}, {"height", 20.0}, {"depth", 10.0}})
          REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
              ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
        auto sketch = reference_checked(
            Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")));
        REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
            ProfileId("profile.base"), ParameterId("width"), ParameterId("height")))));
        REQUIRE(part.add_sketch(sketch));
        REQUIRE(part.add_body(
            reference_checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
        auto feature = reference_checked(Feature::create_additive_extrude(
            FeatureId("feature.base"), "Base", sketch.id(), ParameterId("depth")));
        return part.add_feature(reference_checked(
            feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
                FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base"))))));
      }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  return empty;
}
QTreeWidgetItem* find_item(QTreeWidgetItem* root, const QString& id) {
  if (root->data(0, Qt::UserRole).toString() == id)
    return root;
  for (int i = 0; i < root->childCount(); ++i)
    if (auto* found = find_item(root->child(i), id))
      return found;
  return nullptr;
}
void tree(ShellWindow& window, const char* id, bool activate = false) {
  auto& browser = window.model_browser();
  QTreeWidgetItem* found = nullptr;
  for (int i = 0; i < browser.topLevelItemCount() && !found; ++i)
    found = find_item(browser.topLevelItem(i), QString::fromLatin1(id));
  REQUIRE(found);
  browser.setCurrentItem(found);
  if (activate)
    browser.itemDoubleClicked(found, 0);
  qApp->processEvents();
}
void fillet(ShellWindow& window) {
  invoke(window, "fillet");
  auto& targets = control<QListWidget>(window, "blcad.finishing.targets");
  REQUIRE(targets.count() > 0);
  targets.item(0)->setSelected(true);
  REQUIRE(window.feature_tools().set_numeric_text("1 mm"));
  REQUIRE(window.feature_tools().apply());
}
void hole(ShellWindow& window, const char* center = "8") {
  invoke(window, "hole");
  control<QLineEdit>(window, "blcad.hole.center_x").setText(center);
  control<QLineEdit>(window, "blcad.hole.center_y").setText("8");
  control<QLineEdit>(window, "blcad.hole.diameter").setText("3");
  REQUIRE(window.hole_tools().preview());
  REQUIRE(window.hole_tools().apply());
}
} // namespace

TEST_CASE("Repeat records only accepted modeling commands and keeps the record across cancellation",
          "[gui][block136-repeat]") {
  ShellWindow window;
  CHECK_FALSE(repeat(window).isEnabled());
  seed(window);
  const auto before = snapshot(window);
  invoke(window, "fillet");
  window.feature_tools().cancel();
  CHECK_FALSE(repeat(window).isEnabled());
  CHECK(snapshot(window) == before);
  fillet(window);
  CHECK(repeat(window).text() == "Erneut: Verrunden");
  qApp->processEvents();
  auto& button = control<QToolButton>(window, "blcad.repeat.button");
  CHECK(button.width() >= button.sizeHint().width());
  REQUIRE(repeat(window).isEnabled());
  const auto applied = snapshot(window);
  invoke(window, "repeat_last_command");
  CHECK(window.feature_tools().command() == ShellFeatureTools::Command::Fillet);
  CHECK_FALSE(repeat(window).isEnabled());
  CHECK(snapshot(window) == applied);
  window.feature_tools().cancel();
  CHECK(repeat(window).isEnabled());
  CHECK(repeat(window).text() == "Erneut: Verrunden");
  invoke(window, "hole");
  control<QLineEdit>(window, "blcad.hole.diameter").setText("invalid");
  CHECK_FALSE(window.hole_tools().preview());
  CHECK_FALSE(window.hole_tools().apply());
  CHECK(repeat(window).text() == "Erneut: Verrunden");
  window.hole_tools().cancel();
  CHECK(snapshot(window) == applied);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Repeat uses the current profile and ordinary defaults instead of replaying prior inputs",
          "[gui][block136-repeat]") {
  ShellWindow window;
  seed(window);
  REQUIRE(window.session().commit_part_transaction("Second profile", [](PartDocument& part) {
    auto sketch = reference_checked(
        Sketch::create(SketchId("sketch.other"), "Other", DatumPlaneId("datum.xz")));
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId("profile.other"), ParameterId("width"), ParameterId("height")))));
    return part.add_sketch(sketch);
  }));
  window.refresh();
  tree(window, "sketch.base");
  invoke(window, "extrude");
  REQUIRE(window.feature_tools().set_numeric_text("23 mm"));
  REQUIRE(window.feature_tools().apply());
  CHECK(repeat(window).text() == "Erneut: Extrusion");
  tree(window, "body.base");
  CHECK_FALSE(repeat(window).isEnabled());
  // This explicit handoff occurs after normal refresh, as Finish Sketch does.
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.other"), ProfileId("profile.other")));
  REQUIRE(repeat(window).isEnabled());
  const auto before = snapshot(window);
  invoke(window, "repeat_last_command");
  CHECK(window.feature_tools().coordinator().extrude().distance() != Approx(23));
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->features().back().input_sketch() ==
        SketchId("sketch.other"));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Repeat hole authoring starts a new local draft with fresh inputs and one atomic Apply",
          "[gui][block136-repeat]") {
  ShellWindow window;
  seed(window);
  hole(window);
  const auto before = snapshot(window);
  invoke(window, "repeat_last_command");
  REQUIRE(window.hole_tools().active());
  CHECK(control<QLineEdit>(window, "blcad.hole.center_x").text() == "0");
  CHECK(control<QLineEdit>(window, "blcad.hole.diameter").text() == "6");
  CHECK(snapshot(window) == before);
  control<QLineEdit>(window, "blcad.hole.center_x").setText("-8");
  control<QLineEdit>(window, "blcad.hole.center_y").setText("0");
  REQUIRE(window.hole_tools().preview());
  REQUIRE(window.hole_tools().apply());
  CHECK(window.session().part_document()->features().back().id() !=
        window.session().part_document()->features().front().id());
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(repeat(window).isEnabled());
}

TEST_CASE("Repeat respects task Sketch and capture guards without changing the remembered command",
          "[gui][block136-repeat]") {
  ShellWindow window;
  seed(window);
  fillet(window);
  const auto label = repeat(window).text();
  const auto before = snapshot(window);
  tree(window, "sketch.base", true);
  REQUIRE(window.active_sketch());
  CHECK_FALSE(repeat(window).isEnabled());
  REQUIRE(window.finish_active_sketch());
  CHECK(repeat(window).isEnabled());
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(repeat(window).isEnabled());
  repeat(window).trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  CHECK(repeat(window).isEnabled());
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  CHECK_FALSE(repeat(window).isEnabled());
  REQUIRE(window.session().task().cancel());
  window.refresh();
  CHECK(repeat(window).text() == label);
  CHECK(snapshot(window) == before);
}

TEST_CASE("Repeat is transient across Save history and document replacement boundaries",
          "[gui][block136-repeat]") {
  ShellWindow window;
  seed(window);
  fillet(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("repeat.blcad");
  REQUIRE(window.save_to(path));
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  file.close();
  const auto label = repeat(window).text();
  REQUIRE(window.undo());
  REQUIRE(window.redo());
  CHECK(repeat(window).text() == label);
  REQUIRE(window.save_to(path));
  REQUIRE(file.open(QIODevice::ReadOnly));
  CHECK(file.readAll() == bytes);
  file.close();
  CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
  CHECK(repeat(window).text() == label);
  bool replaced = true;
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("New Part") {
    REQUIRE(window.new_part("Next"));
  }
  SECTION("New Project") {
    REQUIRE(window.new_project("Next"));
  }
  SECTION("Cancelled replacement") {
    REQUIRE(window.session().commit_part_transaction("Unsaved", [](PartDocument& part) {
      return part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId("unsaved"), "Unsaved", reference_checked(Quantity::length_mm(7)))));
    }));
    bool answered = false;
    QTimer::singleShot(0, [&] {
      if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        answered = true;
        dialog->button(QMessageBox::Cancel)->click();
      }
    });
    CHECK_FALSE(window.new_project("Cancelled"));
    REQUIRE(answered);
    replaced = false;
  }
  CHECK(repeat(window).text() == (replaced ? "Befehl wiederholen" : label));
  CHECK(repeat(window).isEnabled() == !replaced);
}

TEST_CASE("Repeat routes Assembly insertion through its ordinary fresh definition task",
          "[gui][block136-repeat]") {
  ShellWindow window;
  seed(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("definition.blcad");
  REQUIRE(window.save_to(path));
  REQUIRE(window.new_project("Repeated assembly"));
  REQUIRE(window.assembly_tools().import_definition(path));
  REQUIRE(window.assembly_tools().begin(ShellAssemblyTools::Command::InsertComponent));
  window.assembly_tools().definition_editor().setCurrentIndex(1);
  REQUIRE(window.assembly_tools().apply());
  qApp->processEvents();
  auto& button = control<QToolButton>(window, "blcad.repeat.button");
  CHECK(button.width() >= button.sizeHint().width());
  CHECK(repeat(window).isEnabled());
  const auto count = window.session().project()->assembly().component_instance_count();
  invoke(window, "repeat_last_command");
  CHECK(window.assembly_tools().active());
  CHECK(window.assembly_tools().definition_editor().currentIndex() == 0);
  CHECK(window.session().project()->assembly().component_instance_count() == count);
  window.assembly_tools().definition_editor().setCurrentIndex(1);
  REQUIRE(window.assembly_tools().apply());
  CHECK(window.session().project()->assembly().component_instance_count() == count + 1);
  REQUIRE(window.undo());
  CHECK(window.session().project()->assembly().component_instance_count() == count);
}
