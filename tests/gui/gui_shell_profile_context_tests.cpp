#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
namespace {
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* result = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(result);
  return *result;
}
QAction& action(ShellWindow& window, const char* id) {
  return control<QAction>(window, (std::string("blcad.action.") + id).c_str());
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
std::string seed(ShellWindow& window) {
  REQUIRE(window.new_part("Profile context"));
  const auto empty = snapshot(window);
  REQUIRE(window.session().commit_part_transaction(
      "Create profile context fixture", [](PartDocument& part) {
        for (const auto& [id, value] :
             {std::pair{"width", 30.0}, {"height", 20.0}, {"depth", 10.0}})
          REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
              ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
        auto sketch = reference_checked(
            Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")));
        REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
            ProfileId("profile.base"), ParameterId("width"), ParameterId("height")))));
        REQUIRE(part.add_sketch(sketch));
        return Result<std::size_t>::success(1);
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
QFrame& panel(ShellWindow& window) {
  return control<QFrame>(window, "blcad.context.toolbar");
}
QAction& context_action(ShellWindow& window, const char* id) {
  return action(window, (std::string("context_") + id).c_str());
}
void context(ShellWindow& window, const char* id) {
  REQUIRE(panel(window).isVisible());
  auto& button = control<QToolButton>(window, (std::string("blcad.context.") + id).c_str());
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  button.click();
  qApp->processEvents();
}
void profile(ShellWindow& window) {
  tree(window, "sketch.base");
  REQUIRE(panel(window).isVisible());
  REQUIRE(context_action(window, "extrude").isEnabled());
}
} // namespace

TEST_CASE("Profile context commands require verified Core intent and respect the model filter",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  CHECK_FALSE(panel(window).isVisible());
  seed(window);
  CHECK_FALSE(panel(window).isVisible());
  profile(window);
  CHECK(context_action(window, "revolve").isEnabled());
  const auto before = snapshot(window);
  control<QComboBox>(window, "blcad.selection.filter").setCurrentIndex(5);
  CHECK_FALSE(panel(window).isVisible());
  CHECK_FALSE(context_action(window, "extrude").isEnabled());
  tree(window, "sketch.base");
  CHECK_FALSE(panel(window).isVisible());
  control<QComboBox>(window, "blcad.selection.filter").setCurrentIndex(0);
  tree(window, "datum.xy");
  CHECK_FALSE(panel(window).isVisible());
  REQUIRE_FALSE(window.feature_tools().select_profile(SketchId("missing"), ProfileId("missing")));
  CHECK_FALSE(panel(window).isVisible());
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.base"), ProfileId("profile.base")));
  CHECK(panel(window).isVisible()); // Late Finish Sketch-style handoff.
  CHECK(snapshot(window) == before);
}

TEST_CASE("Profile context consumes selection and every cancellation restores the complete source",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  seed(window);
  profile(window);
  const auto scene = reference_checked(geometry::ViewportSceneBuilder{}.build_part(
      *window.session().part_document(), *window.session().part_shape_cache()));
  const auto target = std::find_if(scene.begin(), scene.end(), [](const auto& item) {
    return item.kind == geometry::ViewportSceneKind::SketchEntity;
  });
  REQUIRE(target != scene.end());
  REQUIRE(window.viewport().select_semantic(target->semantic_id));
  REQUIRE_FALSE(window.session().selection().empty());
  const auto source = window.feature_tools().profile_context();
  REQUIRE(source);
  const auto selected = window.session().selection().entries();
  const auto viewport = window.viewport().selected_semantic();
  const auto before = snapshot(window);
  context(window, "extrude");
  CHECK(window.session().selection().empty());
  CHECK_FALSE(window.viewport().selected_semantic());
  CHECK_FALSE(context_action(window, "extrude").isVisible());
  CHECK(context_action(window, "cancel").isEnabled());
  SECTION("Context cancel") {
    context(window, "cancel");
  }
  SECTION("Task cancel") {
    control<QPushButton>(window, "blcad.feature.cancel").click();
  }
  SECTION("Escape") {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_tools().profile_context());
  CHECK(window.feature_tools().profile_context()->sketch == source->sketch);
  CHECK(window.feature_tools().profile_context()->profile == source->profile);
  CHECK(window.session().selection().entries().size() == selected.size());
  for (std::size_t i = 0; i < selected.size(); ++i) {
    CHECK(window.session().selection().entries()[i].kind == selected[i].kind);
    CHECK(window.session().selection().entries()[i].semantic_id == selected[i].semantic_id);
    CHECK(window.session().selection().entries()[i].candidate_id == selected[i].candidate_id);
  }
  REQUIRE(window.viewport().selected_semantic().has_value() == viewport.has_value());
  if (viewport)
    CHECK(window.viewport().selected_semantic()->semantic_id == viewport->semantic_id);
  CHECK(context_action(window, "extrude").isEnabled());
  CHECK_FALSE(context_action(window, "cancel").isVisible());
}

TEST_CASE("Context Extrude Apply is atomic and Revolve cancellation restores its profile handoff",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  seed(window);
  profile(window);
  const auto before = snapshot(window);
  context(window, "extrude");
  control<QLineEdit>(window, "blcad.extrude.taper").setText("invalid");
  CHECK_FALSE(window.feature_tools().apply());
  CHECK(snapshot(window) == before);
  control<QLineEdit>(window, "blcad.extrude.taper").clear();
  REQUIRE(window.feature_tools().set_numeric_text("12 mm"));
  REQUIRE(window.feature_tools().apply());
  CHECK(window.session().part_document()->feature_count() == 1);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  profile(window);
  context(window, "revolve");
  CHECK(window.feature_tools().command() == ShellFeatureTools::Command::Revolve);
  CHECK(window.session().selection().empty());
  context(window, "cancel");
  REQUIRE(window.feature_tools().profile_context());
  CHECK(context_action(window, "revolve").isEnabled());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Profile context verifies a single open line and never invents a closed profile",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  seed(window);
  REQUIRE(window.session().commit_part_transaction("Open sources", [](PartDocument& part) {
    auto line = reference_checked(
        Sketch::create(SketchId("sketch.line"), "Line", DatumPlaneId("datum.xz")));
    REQUIRE(line.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("line.one"), {-10, 0}, {10, 0}))));
    REQUIRE(part.add_sketch(line));
    auto chain = reference_checked(
        Sketch::create(SketchId("sketch.chain"), "Chain", DatumPlaneId("datum.xy")));
    REQUIRE(chain.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("chain.one"), {0, 0}, {10, 0}))));
    REQUIRE(chain.add_entity(
        reference_checked(LineSegment::create(SketchEntityId("chain.two"), {10, 0}, {10, 10}))));
    return part.add_sketch(chain);
  }));
  window.refresh();
  tree(window, "sketch.chain");
  CHECK_FALSE(panel(window).isVisible());
  tree(window, "sketch.line");
  REQUIRE(panel(window).isVisible());
  CHECK(context_action(window, "extrude").isVisible());
  CHECK_FALSE(context_action(window, "revolve").isVisible());
  const auto before = snapshot(window);
  context(window, "extrude");
  CHECK(window.session().part_document()->find_sketch(SketchId("sketch.line"))->profile_count() ==
        0);
  context(window, "cancel");
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_tools().profile_context());
  CHECK_FALSE(window.feature_tools().profile_context()->profile);
}

TEST_CASE("Profile context stays out of Sketch foreign tasks and interaction capture",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  seed(window);
  profile(window);
  tree(window, "sketch.base", true);
  REQUIRE(window.active_sketch());
  CHECK_FALSE(panel(window).isVisible());
  REQUIRE(window.finish_active_sketch());
  REQUIRE(panel(window).isVisible());
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(panel(window).isVisible());
  context_action(window, "extrude").trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  CHECK(panel(window).isVisible());
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  CHECK_FALSE(panel(window).isVisible());
  context_action(window, "cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  window.refresh();
  REQUIRE(window.undo()); // Remove the fixture Sketch; stale handoffs must disappear.
  CHECK_FALSE(panel(window).isVisible());
}

TEST_CASE("Context preview Save and document replacement preserve only committed intent",
          "[gui][block136-profile-context]") {
  ShellWindow window;
  seed(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("context.blcad");
  REQUIRE(window.save_to(path));
  profile(window);
  const auto before = snapshot(window);
  context(window, "extrude");
  REQUIRE(window.feature_tools().set_numeric_text("17 mm"));
  REQUIRE(window.save_to(path));
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().task().active());
  SECTION("Cancelled replacement") {
    // Make the clean saved document dirty before testing replacement refusal.
    context(window, "cancel");
    REQUIRE(window.session().commit_part_transaction("Unsaved", [](PartDocument& part) {
      return part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId("unsaved"), "Unsaved", reference_checked(Quantity::length_mm(7)))));
    }));
    window.refresh();
    profile(window);
    context(window, "extrude");
    bool answered = false;
    QTimer::singleShot(0, [&] {
      if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        answered = true;
        dialog->button(QMessageBox::Cancel)->click();
      }
    });
    CHECK_FALSE(window.new_part("Cancelled"));
    REQUIRE(answered);
    CHECK(window.session().task().active());
    CHECK(context_action(window, "cancel").isEnabled());
  }
  SECTION("Open") {
    REQUIRE(window.open_path(path));
    CHECK_FALSE(panel(window).isVisible());
    CHECK(snapshot(window) == before);
  }
  SECTION("New Part") {
    REQUIRE(window.new_part("Next"));
    CHECK_FALSE(panel(window).isVisible());
  }
  SECTION("Failed Open") {
    CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(panel(window).isVisible());
    CHECK(snapshot(window) == before);
  }
}
