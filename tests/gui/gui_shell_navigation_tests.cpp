#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
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
QComboBox& marks(ShellWindow& window) {
  return control<QComboBox>(window, "blcad.navigation.bookmarks");
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
std::string seed(ShellWindow& window) {
  REQUIRE(window.new_part("Navigation"));
  const auto empty = snapshot(window);
  REQUIRE(
      window.session().commit_part_transaction("Create navigation fixture", [](PartDocument& part) {
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
void same_camera(const GuiViewportCameraBookmark& actual,
                 const GuiViewportCameraBookmark& expected) {
  for (const auto& [value, saved] : {std::pair{actual.eye.x, expected.eye.x},
                                     {actual.eye.y, expected.eye.y},
                                     {actual.eye.z, expected.eye.z},
                                     {actual.target.x, expected.target.x},
                                     {actual.target.y, expected.target.y},
                                     {actual.target.z, expected.target.z},
                                     {actual.up_direction.x, expected.up_direction.x},
                                     {actual.up_direction.y, expected.up_direction.y},
                                     {actual.up_direction.z, expected.up_direction.z},
                                     {actual.scale, expected.scale}})
    CHECK(value == Approx(saved).margin(1e-7));
  CHECK(actual.projection == expected.projection);
  CHECK(actual.plane_camera == expected.plane_camera);
}
void save_mark(ShellWindow& window, const QString& name) {
  marks(window).setEditText(name);
  invoke(window, "bookmark_save");
}
QTreeWidgetItem* item(QTreeWidgetItem* root, const QString& id) {
  if (root->data(0, Qt::UserRole).toString() == id)
    return root;
  for (int i = 0; i < root->childCount(); ++i)
    if (auto* found = item(root->child(i), id))
      return found;
  return nullptr;
}
} // namespace

TEST_CASE("Shell view cube exposes seven distinct camera frames without model history",
          "[gui][block136-navigation]") {
  ShellWindow window;
  CHECK_FALSE(action(window, "view_front").isEnabled());
  CHECK_FALSE(action(window, "capture_home").isEnabled());
  CHECK_FALSE(action(window, "fit").isEnabled());
  const auto empty = seed(window);
  QTemporaryDir directory;
  REQUIRE(window.save_to(directory.filePath("navigation.blcad")));
  const auto before = snapshot(window);
  const struct {
    const char* id;
    Vector3 direction;
    Vector3 up;
  } views[] = {{"view_isometric", {1, -1, 1}, {-1, 1, 2}}, {"view_front", {0, -1, 0}, {0, 0, 1}},
               {"view_back", {0, 1, 0}, {0, 0, 1}},        {"view_left", {-1, 0, 0}, {0, 0, 1}},
               {"view_right", {1, 0, 0}, {0, 0, 1}},       {"view_top", {0, 0, 1}, {0, 1, 0}},
               {"view_bottom", {0, 0, -1}, {0, -1, 0}}};
  for (bool perspective : {false, true}) {
    window.viewport().set_projection(perspective ? GuiViewportProjection::Perspective
                                                 : GuiViewportProjection::Orthographic);
    for (const auto& view : views) {
      INFO(view.id << " perspective=" << perspective);
      invoke(window, view.id);
      const auto camera = window.viewport().camera_bookmark();
      const Vector3 direction{camera.eye.x - camera.target.x, camera.eye.y - camera.target.y,
                              camera.eye.z - camera.target.z};
      const double length = std::hypot(direction.x, direction.y, direction.z);
      const double expected_length =
          std::hypot(view.direction.x, view.direction.y, view.direction.z);
      CHECK((direction.x * view.direction.x + direction.y * view.direction.y +
             direction.z * view.direction.z) /
                (length * expected_length) ==
            Approx(1));
      const double up_length = std::hypot(view.up.x, view.up.y, view.up.z);
      CHECK((camera.up_direction.x * view.up.x + camera.up_direction.y * view.up.y +
             camera.up_direction.z * view.up.z) /
                up_length ==
            Approx(1));
      CHECK_FALSE(camera.plane_camera);
      CHECK(action(window, "perspective").isChecked() == perspective);
      CHECK_FALSE(window.session().dirty());
      CHECK(snapshot(window) == before);
    }
  }
  REQUIRE(window.undo());
  CHECK(snapshot(window) == empty);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Shell Home and named views preserve complete cameras and replace trimmed names",
          "[gui][block136-navigation]") {
  ShellWindow window;
  seed(window);
  const auto before = snapshot(window);
  window.viewport().set_projection(GuiViewportProjection::Orthographic);
  REQUIRE(window.viewport().set_plane_camera({3, 4, 6}, {0, 0, 1}, {0, 1, 0}));
  const auto saved = window.viewport().camera_bookmark();
  invoke(window, "capture_home");
  save_mark(window, "  Schnitt Ä  ");
  REQUIRE(marks(window).count() == 1);
  CHECK(marks(window).itemText(0) == "Schnitt Ä");
  invoke(window, "view_back");
  window.viewport().set_projection(GuiViewportProjection::Perspective);
  window.resize(1100, 760);
  qApp->processEvents();
  invoke(window, "view_home");
  same_camera(window.viewport().camera_bookmark(), saved);
  CHECK_FALSE(action(window, "perspective").isChecked());
  invoke(window, "view_right");
  const auto replacement = window.viewport().camera_bookmark();
  save_mark(window, "Schnitt Ä");
  CHECK(marks(window).count() == 1);
  invoke(window, "view_front");
  invoke(window, "bookmark_restore");
  same_camera(window.viewport().camera_bookmark(), replacement);
  save_mark(window, "Vorne");
  CHECK(marks(window).count() == 2);
  marks(window).setEditText("unknown");
  CHECK_FALSE(action(window, "bookmark_restore").isEnabled());
  CHECK_FALSE(action(window, "bookmark_remove").isEnabled());
  marks(window).setEditText("   ");
  CHECK_FALSE(action(window, "bookmark_save").isEnabled());
  marks(window).setEditText("Schnitt Ä");
  invoke(window, "bookmark_remove");
  CHECK(marks(window).count() == 1);
  CHECK(marks(window).itemText(0) == "Vorne");
  CHECK_FALSE(action(window, "bookmark_restore").isEnabled());
  invoke(window, "view_home");
  same_camera(window.viewport().camera_bookmark(), saved);
  invoke(window, "view_isometric");
  window.viewport().set_projection(GuiViewportProjection::Perspective);
  const auto perspective = window.viewport().camera_bookmark();
  save_mark(window, "Perspective");
  invoke(window, "view_bottom");
  window.viewport().set_projection(GuiViewportProjection::Orthographic);
  invoke(window, "bookmark_restore");
  same_camera(window.viewport().camera_bookmark(), perspective);
  CHECK(action(window, "perspective").isChecked());
  CHECK(snapshot(window) == before);
}

TEST_CASE(
    "Shell view snapshots survive history and failed opening but reset on successful replacement",
    "[gui][block136-navigation]") {
  ShellWindow window;
  seed(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("navigation.blcad");
  REQUIRE(window.save_to(path));
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  file.close();
  save_mark(window, "Review");
  invoke(window, "capture_home");
  const auto saved = window.viewport().camera_bookmark();
  REQUIRE(window.undo());
  REQUIRE(window.redo());
  CHECK(marks(window).count() == 1);
  invoke(window, "view_home");
  same_camera(window.viewport().camera_bookmark(), saved);
  REQUIRE(window.save_to(path));
  REQUIRE(file.open(QIODevice::ReadOnly));
  CHECK(file.readAll() == bytes);
  file.close();
  CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
  CHECK(marks(window).count() == 1);
  REQUIRE(action(window, "view_home").isEnabled());
  SECTION("Cancelled replacement") {
    REQUIRE(window.session().commit_part_transaction("Make document dirty", [](PartDocument& part) {
      return part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId("unsaved"), "Unsaved", reference_checked(Quantity::length_mm(7)))));
    }));
    const auto dirty = snapshot(window);
    bool answered = false;
    QTimer::singleShot(0, [&] {
      if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        answered = true;
        dialog->button(QMessageBox::Cancel)->click();
      }
    });
    CHECK_FALSE(window.new_part("Cancelled"));
    REQUIRE(answered);
    CHECK(snapshot(window) == dirty);
    CHECK(marks(window).count() == 1);
    CHECK(action(window, "view_home").isEnabled());
    return;
  }
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("Part") {
    REQUIRE(window.new_part("Fresh Part"));
  }
  SECTION("Project") {
    REQUIRE(window.new_project("Fresh Assembly"));
  }
  CHECK(marks(window).count() == 0);
  CHECK_FALSE(action(window, "view_home").isEnabled());
  CHECK_FALSE(action(window, "bookmark_restore").isEnabled());
}

TEST_CASE("Sketch focus and active grips protect the camera from model navigation",
          "[gui][block136-navigation]") {
  ShellWindow window;
  seed(window);
  save_mark(window, "Model");
  invoke(window, "capture_home");
  QTreeWidgetItem* sketch = nullptr;
  for (int i = 0; i < window.model_browser().topLevelItemCount(); ++i)
    if (auto* found = item(window.model_browser().topLevelItem(i), "sketch.base"))
      sketch = found;
  REQUIRE(sketch);
  window.model_browser().itemDoubleClicked(sketch, 0);
  REQUIRE(window.active_sketch());
  const auto focused = window.viewport().camera_bookmark();
  for (const auto* id : {"view_front", "view_bottom", "view_home", "capture_home",
                         "bookmark_restore", "bookmark_save", "perspective"}) {
    CHECK_FALSE(action(window, id).isEnabled());
    action(window, id).trigger();
  }
  same_camera(window.viewport().camera_bookmark(), focused);
  REQUIRE(action(window, "fit").isEnabled());
  invoke(window, "finish_sketch");
  window.viewport().grab_interaction_mouse();
  qApp->processEvents();
  CHECK_FALSE(action(window, "view_front").isEnabled());
  CHECK_FALSE(action(window, "view_home").isEnabled());
  CHECK_FALSE(action(window, "fit").isEnabled());
  window.viewport().release_interaction_mouse();
  qApp->processEvents();
  REQUIRE(action(window, "view_home").isEnabled());
  invoke(window, "view_home");
}

TEST_CASE("Navigation preserves valid and failed feature candidates and reference selection scope",
          "[gui][block136-navigation]") {
  ShellWindow window;
  seed(window);
  invoke(window, "view_front");
  save_mark(window, "Candidate");
  REQUIRE(
      window.feature_tools().select_profile(SketchId("sketch.base"), ProfileId("profile.base")));
  REQUIRE(window.feature_tools().begin_extrude());
  const auto before = snapshot(window);
  const auto stage = window.session().task().stage();
  invoke(window, "view_bottom");
  invoke(window, "bookmark_restore");
  CHECK(window.viewport().feature_preview_active());
  CHECK(window.session().task().stage() == stage);
  auto& taper = control<QLineEdit>(window, "blcad.extrude.taper");
  taper.setText("invalid");
  REQUIRE(QMetaObject::invokeMethod(&taper, "editingFinished", Qt::DirectConnection));
  invoke(window, "view_back");
  invoke(window, "bookmark_restore");
  CHECK_FALSE(control<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(taper.text() == "invalid");
  taper.clear();
  REQUIRE(QMetaObject::invokeMethod(&taper, "editingFinished", Qt::DirectConnection));
  control<QComboBox>(window, "blcad.extrude.extent").setCurrentIndex(5);
  control<QComboBox>(window, "blcad.extrude.pick_role").setCurrentIndex(1);
  const auto scope = window.viewport().selection_semantic_scope();
  const auto selected = window.viewport().selected_semantic();
  REQUIRE(selected);
  invoke(window, "view_top");
  invoke(window, "bookmark_restore");
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == selected->semantic_id);
  CHECK(window.session().task().stage() == stage);
  CHECK(snapshot(window) == before);
  window.feature_tools().cancel();
  CHECK(snapshot(window) == before);
}
