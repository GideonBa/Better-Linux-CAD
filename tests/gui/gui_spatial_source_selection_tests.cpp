#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QToolButton>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
template <class T> T& control(ShellWindow& w, const char* id) {
  auto* p = w.findChild<T*>(QStringLiteral("blcad.sketch3d.") + id);
  REQUIRE(p);
  return *p;
}
void click(ShellWindow& w, const char* id) {
  control<QPushButton>(w, id).click();
}
void pick(ShellWindow& w, int slot) {
  control<QToolButton>(w, ("constraint_target_" + std::to_string(slot) + ".selector.pick").c_str())
      .click();
}
void target(ShellWindow& w, int slot, const char* label) {
  auto& c = control<QComboBox>(w, ("constraint_target_" + std::to_string(slot)).c_str());
  auto row = c.findText(label);
  REQUIRE(row >= 0);
  c.setCurrentIndex(row);
}
std::string snapshot(const GuiDocumentSession& s) {
  return f::checked(serialize_part_document_to_json(*s.part_document()));
}
void seed(ShellWindow& w) {
  REQUIRE(w.new_part("Spatial source selection"));
  REQUIRE(w.session().commit_part_transaction("Source", [](PartDocument& p) {
    p = sf::straight_sweep();
    auto s = *p.find_sketch_3d(Sketch3DId("driven"));
    (void)f::checked(s.add_point(f::point("free", {1.5, 1.25, 8})));
    return p.update_sketch_3d(s);
  }));
  w.refresh();
}
void escape(ShellWindow& w) {
  QKeyEvent e(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &e);
}
void pointer(ShellWindow& w, QEvent::Type type, Point3 p) {
  const auto screen = f::checked(w.viewport().model_to_screen(p));
  const QPointF local(screen.x, screen.y);
  QMouseEvent e(type, local, w.viewport().mapToGlobal(local.toPoint()),
                type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &e);
  qApp->processEvents();
}
} // namespace
TEST_CASE("Spatial typed input selection restores scope and rejects a cleared target",
          "[gui][parity-b165][spatial-source-selection]") {
  ShellWindow w;
  seed(w);
  const auto original = snapshot(w.session());
  w.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::SketchEntity));
  const std::optional<std::vector<std::string>> scope{{"sketch3d/driven/entity/line"}};
  w.viewport().set_selection_semantic_scope(scope);
  REQUIRE(w.viewport().select_semantic("sketch3d/driven/entity/line"));
  const auto preselection = w.viewport().selected_semantic();
  const auto session_selection = w.session().selection().entries();
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("driven")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(2);
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Fixed));
  pick(w, 0);
  REQUIRE(w.viewport().preview_selection_target_count() >= 3);
  const auto aliases = w.viewport().selection_semantic_scope();
  REQUIRE(aliases);
  REQUIRE(w.viewport().select_semantic(aliases->at(2)));
  CHECK(control<QComboBox>(w, "constraint_target_0").currentText() == "Local point / free");
  CHECK(w.viewport().preview_selection_target_count() == 0);
  CHECK(w.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::SketchEntity));
  CHECK(w.viewport().selection_semantic_scope() == scope);
  CHECK(w.viewport().selected_semantic() == preselection);
  CHECK(w.session().selection().entries() == session_selection);
  control<QToolButton>(w, "constraint_target_0.selector.clear").click();
  click(w, "constraint_accept");
  CHECK(control<QLabel>(w, "constraint_feedback").text().contains("Select each compatible target"));
  CHECK(snapshot(w.session()) == original);
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Tangent));
  pick(w, 0);
  REQUIRE(w.viewport().preview_selection_target_count() >= 1);
  REQUIRE(w.viewport().select_semantic(w.viewport().selection_semantic_scope()->front()));
  CHECK(control<QLabel>(w, "constraint_feedback").text().contains("ambiguous"));
  CHECK_FALSE(control<QComboBox>(w, "constraint_target_0").currentData().isValid());
  escape(w);
  pick(w, 0);
  escape(w);
  CHECK(w.sketch_3d_tools().active());
  CHECK(w.viewport().preview_selection_target_count() == 0);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  escape(w);
  CHECK_FALSE(w.sketch_3d_tools().active());
  CHECK(snapshot(w.session()) == original);
}
TEST_CASE("Native spatial selectors pick hidden physical support and source curve through preview",
          "[gui][parity-b165][native-spatial-sources]") {
  if (QApplication::platformName() != "xcb") {
    SKIP("Requires native Qt/OCCT picking");
  }
  ShellWindow w;
  seed(w);
  REQUIRE(w.session().set_presentation_visible("body/base.body", false));
  w.refresh();
  const auto area =
      QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
  w.move(area.topLeft());
  w.resize(std::min(1280, area.width()), std::min(800, area.height()));
  w.show();
  qApp->processEvents();
  REQUIRE(w.viewport().native_viewer_available());
  auto camera = w.viewport().camera_bookmark();
  camera.eye = {40, 35, 50};
  camera.target = {3, 2, 5};
  camera.scale = 22;
  REQUIRE(w.viewport().restore_camera_bookmark(camera));
  const auto before = snapshot(w.session());
  REQUIRE(w.sketch_3d_tools().begin_edit(Sketch3DId("driven")));
  control<QTabWidget>(w, "tabs").setCurrentIndex(2);
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::PointOnFace));
  target(w, 0, "Local point / free");
  pick(w, 1);
  CHECK(w.viewport().preview_selection_target_count() == 0);
  control<QCheckBox>(w, "constraint_hidden_inputs").setChecked(true);
  REQUIRE(w.viewport().preview_selection_target_count() >= 6);
  qApp->processEvents();
  for (const auto type : {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease})
    pointer(w, type, {1.5, 1.25, 5});
  INFO(control<QComboBox>(w, "constraint_target_1").currentText().toStdString());
  REQUIRE(control<QComboBox>(w, "constraint_target_1").currentText().contains("top"));
  CHECK(w.viewport().preview_selection_target_count() == 0);
  click(w, "constraint_accept");
  INFO(control<QLabel>(w, "constraint_feedback").text().toStdString());
  REQUIRE(w.sketch_3d_tools().preview());
  control<QTabWidget>(w, "tabs").setCurrentIndex(1);
  click(w, "curve_pick");
  REQUIRE(w.viewport().preview_selection_target_count() == 1);
  for (const auto type : {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease})
    pointer(w, type, {0, 0, 7});
  REQUIRE(control<QComboBox>(w, "curve").currentData().toString() == "line");
  CHECK(w.viewport().preview_selection_target_count() == 0);
  CHECK(control<QPushButton>(w, "apply").isEnabled());
  CHECK_FALSE(w.session().presentation_visible("body/base.body"));
  if (const auto directory = qEnvironmentVariable("BLCAD_CAPTURE_DIR"); !directory.isEmpty()) {
    QDir().mkpath(directory);
    REQUIRE(w.grab().save(directory + "/spatial-source-selection.png"));
  }
  REQUIRE(w.sketch_3d_tools().apply());
  const auto after = snapshot(w.session());
  CHECK(after != before);
  const auto& s = *w.session().part_document()->find_sketch_3d(Sketch3DId("driven"));
  CHECK(s.find_point(SketchEntityId("free"))->z().explicit_coordinate()->millimeters() ==
        Catch::Approx(5).margin(1e-7));
  REQUIRE(s.find_constraint(SketchConstraintId("constraint.1")));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w.session()) == before);
  REQUIRE(w.session().redo());
  CHECK(snapshot(w.session()) == after);
  const auto reopened = f::checked(deserialize_part_document_from_json(after));
  CHECK(f::checked(serialize_part_document_to_json(reopened)) == after);
  w.refresh();
  qApp->processEvents();
  REQUIRE(w.session().part_modeling_ready());
  REQUIRE(w.viewport().select_semantic("sketch3d/driven/entity/line"));
  REQUIRE(w.viewport().selected_semantic());
  REQUIRE(w.viewport().selected_semantic()->semantic_id == "sketch3d/driven/entity/line");
  pointer(w, QEvent::MouseButtonDblClick, {0, 0, 7});
  INFO("Task after double-click: " << w.session().task().command_id());
  REQUIRE(w.sketch_3d_tools().active());
  CHECK(control<QTabWidget>(w, "tabs").currentIndex() == 1);
  CHECK(control<QComboBox>(w, "curve").currentData().toString() == "line");
  control<QTabWidget>(w, "tabs").setCurrentIndex(2);
  control<QComboBox>(w, "constraint_kind")
      .setCurrentIndex(static_cast<int>(SpatialConstraintKind::Tangent));
  pick(w, 0);
  for (const auto type : {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease})
    pointer(w, type, {0, 0, 9.75});
  CHECK(control<QComboBox>(w, "constraint_target_0").currentText() == "Local curve / line / end");
  CHECK(w.viewport().preview_selection_target_count() == 0);
  w.sketch_3d_tools().cancel();
  CHECK(snapshot(w.session()) == after);
}
