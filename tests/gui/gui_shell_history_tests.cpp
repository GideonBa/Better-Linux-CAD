#include "../support/engineering_resource_fixture.hpp"
#include "../support/hole_feature_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
void settle() {
  for (int i = 0; i < 5; ++i)
    qApp->processEvents();
}
void settle_paint() {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 120) {
    qApp->processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(5);
  }
}
void mouse(QWidget& widget, QEvent::Type type, QPoint point, Qt::MouseButton button,
           Qt::MouseButtons buttons) {
  QMouseEvent event(type, point, widget.mapToGlobal(point), button, buttons, Qt::NoModifier);
  QApplication::sendEvent(&widget, &event);
  settle();
}
void click(QWidget& widget) {
  REQUIRE(widget.isEnabled());
  const auto point =
      qobject_cast<QCheckBox*>(&widget) ? QPoint(10, widget.height() / 2) : widget.rect().center();
  mouse(widget, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
  mouse(widget, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
}
void key(QWidget& widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent event(QEvent::KeyPress, code, modifiers);
  QApplication::sendEvent(&widget, &event);
  settle();
}
template <typename T> T& control(ShellWindow& w, const char* name) {
  auto* found = w.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(found);
  return *found;
}
std::string snapshot(const ShellWindow& w) {
  return checked(serialize_part_document_to_json(*w.session().part_document()));
}
QTreeWidgetItem* item(QTreeWidgetItem* parent, const QString& id) {
  if (parent->data(0, Qt::UserRole).toString() == id)
    return parent;
  for (int i = 0; i < parent->childCount(); ++i)
    if (auto* found = item(parent->child(i), id))
      return found;
  return nullptr;
}
QTreeWidgetItem* item(ShellWindow& w, const char* id) {
  for (int i = 0; i < w.model_browser().topLevelItemCount(); ++i)
    if (auto* found = item(w.model_browser().topLevelItem(i), QString::fromLatin1(id)))
      return found;
  FAIL("Missing browser item " << id);
  return nullptr;
}
QModelIndex index(const QAbstractItemModel& model, const QString& id,
                  const QModelIndex& parent = {}) {
  for (int row = 0; row < model.rowCount(parent); ++row) {
    const auto candidate = model.index(row, 0, parent);
    if (candidate.data(Qt::UserRole).toString() == id)
      return candidate;
    if (const auto found = index(model, id, candidate); found.isValid())
      return found;
  }
  return {};
}
PartDocument history_part() {
  auto part = fixture::native_base();
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("tool"), "Independent tool", SketchId("profile"), ParameterId("radius")));
  REQUIRE(
      part.add_feature(checked(feature.with_body_result_context(fixture::new_body(part, "tool")))));
  return part;
}
void open(ShellWindow& w, QTemporaryDir& dir, PartDocument part = history_part()) {
  REQUIRE(dir.isValid());
  const auto path = dir.filePath("history.blcad.json");
  REQUIRE(write_part_document_json_file(part, path.toStdString()));
  REQUIRE(w.open_path(path));
  const QSize desired_size(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 1024 : 1280,
                           qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 640 : 800);
  const QSize practical_size =
      QApplication::platformName() == "offscreen"
          ? desired_size
          : desired_size.boundedTo(w.screen()->availableGeometry().size() - QSize(48, 48));
  w.resize(practical_size);
  w.show();
  settle_paint();
  w.resize(practical_size);
  settle_paint();
  CHECK(w.size() == practical_size);
  if (QApplication::platformName() == "xcb")
    REQUIRE(w.viewport().native_viewer_available());
  w.viewport().fit_all();
  settle();
}
void context(ShellWindow& w, const char* id, QAction& action) {
  auto* target = item(w, id);
  w.model_browser().scrollToItem(target);
  settle();
  const auto point = w.model_browser().visualItemRect(target).center();
  bool found = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (!menu)
      return;
    found = menu->actions().contains(&action);
    if (!found) {
      menu->close();
      return;
    }
    menu->setActiveAction(&action);
    QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(menu, &accept);
  });
  QTimer failsafe;
  failsafe.setSingleShot(true);
  QObject::connect(&failsafe, &QTimer::timeout, &w, [] {
    if (auto* popup = QApplication::activePopupWidget())
      popup->close();
  });
  failsafe.start(1000);
  QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                          w.model_browser().viewport()->mapToGlobal(point));
  QApplication::sendEvent(w.model_browser().viewport(), &event);
  failsafe.stop();
  settle();
  REQUIRE(found);
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  settle_paint();
  if (qEnvironmentVariableIsSet("BLCAD_UI_COMPACT")) {
    CHECK(w.width() <= 1024);
    CHECK(w.height() <= 640);
  }
  REQUIRE(w.grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png")));
}
} // namespace

namespace {
PartDocument prefix_part() {
  auto part = history_part();
  REQUIRE(part.apply_history_intent({{"base", "fillet", "tool"}, {}}));
  return part;
}
QTreeWidgetItem* marker(ShellWindow& w) {
  const auto find = [&](auto&& self, QTreeWidgetItem* node) -> QTreeWidgetItem* {
    if (node->data(0, ShellHistoryBrowser::marker_role).toBool())
      return node;
    for (int i = 0; i < node->childCount(); ++i)
      if (auto* found = self(self, node->child(i)))
        return found;
    return nullptr;
  };
  for (int i = 0; i < w.model_browser().topLevelItemCount(); ++i)
    if (auto* found = find(find, w.model_browser().topLevelItem(i)))
      return found;
  FAIL("Missing End of Part marker");
  return nullptr;
}
void quick(ShellWindow& w, const char* id) {
  auto* action = w.findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  for (auto* button : w.ribbon().findChildren<QToolButton*>())
    if (button->isVisible() && button->defaultAction() == action) {
      click(*button);
      return;
    }
  FAIL("No visible quick/ribbon button for " << id);
}
void number(QDoubleSpinBox& spin, const QString& value) {
  auto* line = spin.findChild<QLineEdit*>();
  REQUIRE(line);
  line->setFocus();
  key(*line, Qt::Key_A, Qt::ControlModifier);
  for (const auto character : value) {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(character));
    QApplication::sendEvent(line, &event);
  }
  key(*line, Qt::Key_Return);
  CHECK(spin.value() == Catch::Approx(value.toDouble()));
}
void camera_matches(ShellWindow& w, const GuiViewportCameraBookmark& expected) {
  const auto actual = w.viewport().camera_bookmark();
  CHECK(actual.projection == expected.projection);
  const std::array a{actual.scale,         actual.eye.x,          actual.eye.y,
                     actual.eye.z,         actual.target.x,       actual.target.y,
                     actual.target.z,      actual.up_direction.x, actual.up_direction.y,
                     actual.up_direction.z};
  const std::array b{expected.scale,         expected.eye.x,          expected.eye.y,
                     expected.eye.z,         expected.target.x,       expected.target.y,
                     expected.target.z,      expected.up_direction.x, expected.up_direction.y,
                     expected.up_direction.z};
  for (std::size_t i = 0; i < a.size(); ++i) {
    CAPTURE(i);
    CHECK(a[i] == Catch::Approx(b[i]).margin(1e-6));
  }
}
void edit_prefix_depth(ShellWindow& w, double depth) {
  context(w, "base", w.history_tools().edit_action());
  REQUIRE(w.feature_edit_tools().active());
  auto* field = qobject_cast<QDoubleSpinBox*>(w.feature_edit_tools().field("/length_parameter"));
  REQUIRE(field);
  number(*field, QString::number(depth));
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  REQUIRE_FALSE(w.feature_edit_tools().active());
  REQUIRE(w.history_tools().prefix_active());
  REQUIRE(w.session().history_prefix_ready());
}
} // namespace

TEST_CASE("End of Part context input retains complete intent and Cancel restores the full view",
          "[gui][parity-b148][history-marker-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto before = snapshot(w);
  const auto camera = w.viewport().camera_bookmark();
  const auto size = w.size();
  REQUIRE(marker(w)->data(0, ShellHistoryBrowser::marker_position_role).toULongLong() == 3);
  context(w, "fillet", w.history_tools().marker_before_action());
  REQUIRE(w.history_tools().prefix_active());
  REQUIRE(w.session().history_prefix_count() == 1);
  CHECK_FALSE(w.session().derived_results_fresh());
  CHECK(w.session().history_prefix_ready());
  CHECK(w.session().part_shape_cache()->body_shape_count() == 1);
  CHECK(item(w, "fillet")->text(0).contains("after End of Part"));
  CHECK_FALSE(item(w, "fillet")->font(0).italic());
  CHECK_FALSE(w.history_tools().edit_action().isEnabled());
  CHECK_FALSE(w.findChild<QAction*>("blcad.action.export_part_step")->isEnabled());
  CHECK_FALSE(w.findChild<QAction*>("blcad.action.measure_body")->isEnabled());
  CHECK(w.findChild<QAction*>("blcad.action.export_part_step")->toolTip().contains("End of Part"));
  CHECK(w.findChild<QAction*>("blcad.action.measure_body")->toolTip().contains("End of Part"));
  CHECK(snapshot(w) == before);
  CHECK(w.size() == size);
  auto* end = marker(w);
  CHECK(end->parent()->indexOfChild(end) == 1);
  REQUIRE(control<QPushButton>(w, "blcad.history.prefix_apply").isVisible());
  capture(w, "history-end-of-part-prefix");
  click(control<QPushButton>(w, "blcad.history.prefix_cancel"));
  settle_paint();
  CHECK(w.statusBar()->currentMessage() == "End of Part cancelled.");
  CHECK_FALSE(w.history_tools().active());
  CHECK_FALSE(w.session().history_prefix_count());
  CHECK(snapshot(w) == before);
  CHECK(w.session().derived_results_fresh());
  CHECK(w.session().part_shape_cache()->body_shape_count() == 2);
  CHECK_FALSE(w.session().can_undo());
  CHECK(w.size() == size);
  camera_matches(w, camera);
  auto* last = marker(w);
  w.model_browser().scrollToItem(last);
  settle();
  const auto point = w.model_browser().visualItemRect(last).center();
  mouse(*w.model_browser().viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton,
        Qt::LeftButton);
  mouse(*w.model_browser().viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton,
        Qt::NoButton);
  mouse(*w.model_browser().viewport(), QEvent::MouseButtonDblClick, point, Qt::LeftButton,
        Qt::LeftButton);
  mouse(*w.model_browser().viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton,
        Qt::NoButton);
  REQUIRE(w.history_tools().prefix_active());
  CHECK(w.session().history_prefix_count() == 3);
  CHECK_FALSE(w.session().derived_results_fresh());
  key(w, Qt::Key_Escape);
  CHECK_FALSE(w.history_tools().active());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  camera_matches(w, camera);
}

TEST_CASE(
    "Native prefix edits keep failed suffixes correctable and Save writes the complete checkpoint",
    "[gui][parity-b148][history-marker-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto before = snapshot(w);
  context(w, "fillet", w.history_tools().marker_before_action());
  const auto initial_size = w.size();
  edit_prefix_depth(w, 0.25);
  const auto invalid = snapshot(w);
  CHECK(invalid != before);
  quick(w, "save");
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(w.session().file_path().string())))) == before);
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  CHECK(w.history_tools().prefix_active());
  CHECK(w.session().history_prefix_count() == 1);
  CHECK(snapshot(w) == invalid);
  INFO("Window " << w.width() << "x" << w.height() << "; before " << initial_size.width() << "x"
                 << initial_size.height());
  CHECK(w.size() == initial_size);
  CHECK(control<QLabel>(w, "blcad.history.prefix_status")
            .text()
            .contains("fillet", Qt::CaseInsensitive));
  settle_paint();
  CHECK(w.model_browser().viewport()->rect().contains(
      w.model_browser().visualItemRect(marker(w)).center()));
  capture(w, "history-end-of-part-invalid-suffix");
  edit_prefix_depth(w, 4);
  const auto after = snapshot(w);
  key(control<QPushButton>(w, "blcad.history.prefix_apply"), Qt::Key_Return, Qt::ControlModifier);
  REQUIRE_FALSE(w.history_tools().active());
  REQUIRE(w.session().derived_results_fresh());
  REQUIRE(w.session().part_shape_cache()->validate_complete_history_result(
      *w.session().part_document()));
  CHECK(w.session().undo_label() == "Edit at End of Part");
  CHECK(w.statusBar()->currentMessage() == "End of Part applied.");
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  CHECK_FALSE(w.session().history_prefix_count());
  quick(w, "redo");
  CHECK(snapshot(w) == after);
  CHECK_FALSE(w.session().history_prefix_count());
  quick(w, "save");
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
  CHECK(reopened.derived_results_fresh());
  CHECK_FALSE(reopened.history_prefix_count());
}

TEST_CASE("Marker drag rejects foreign and stale payloads and evaluates only its authored prefix",
          "[gui][parity-b148][history-marker-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto before = snapshot(w);
  auto& tree = w.model_browser();
  const auto mime_for = [&](ShellWindow& source) {
    auto& browser = source.model_browser();
    auto* row = marker(source);
    browser.scrollToItem(row);
    settle();
    const auto point = browser.visualItemRect(row).center();
    mouse(*browser.viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(*browser.viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    const auto find = [&](auto&& self, const QModelIndex& parent) -> QModelIndex {
      for (int i = 0; i < browser.model()->rowCount(parent); ++i) {
        const auto candidate = browser.model()->index(i, 0, parent);
        if (candidate.data(ShellHistoryBrowser::marker_role).toBool())
          return candidate;
        if (const auto nested = self(self, candidate); nested.isValid())
          return nested;
      }
      return {};
    };
    const auto source_index = find(find, {});
    REQUIRE(source_index.isValid());
    return std::unique_ptr<QMimeData>(browser.model()->mimeData({source_index}));
  };
  const auto drop = [&](const QMimeData& mime, const char* target_id) {
    auto* target = target_id ? item(w, target_id) : marker(w)->parent();
    tree.scrollToItem(target);
    settle();
    const auto position = tree.visualItemRect(target).topLeft() + QPoint(30, 2);
    QDragEnterEvent enter(position, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &enter);
    QDragMoveEvent move(position, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &move);
    QDropEvent event(position, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &event);
    settle();
    return event.isAccepted();
  };
  {
    QTemporaryDir foreign_directory;
    ShellWindow foreign;
    open(foreign, foreign_directory, prefix_part());
    const auto foreign_mime = mime_for(foreign);
    REQUIRE(foreign_mime);
    CHECK_FALSE(drop(*foreign_mime, "fillet"));
    CHECK_FALSE(w.history_tools().active());
  }
  const auto original_mime = mime_for(w);
  REQUIRE(original_mime);
  REQUIRE(drop(*original_mime, "fillet"));
  REQUIRE(w.session().history_prefix_count() == 1);
  CHECK(snapshot(w) == before);
  CHECK(marker(w)->parent()->indexOfChild(marker(w)) == 1);
  CHECK_FALSE(drop(*original_mime, "base"));
  REQUIRE(drop(*mime_for(w), "base"));
  CHECK(w.session().history_prefix_count() == 0);
  CHECK(w.session().part_shape_cache()->body_shape_count() == 0);
  CHECK_FALSE(w.viewport().select_semantic("body/body.base"));
  REQUIRE(drop(*mime_for(w), nullptr));
  CHECK(w.session().history_prefix_count() == 3);
  CHECK_FALSE(w.session().derived_results_fresh());
  CHECK_FALSE(w.session().part_shape_cache()->validate_complete_history_result(
      *w.session().part_document()));
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  CHECK_FALSE(w.history_tools().active());
  CHECK(w.session().derived_results_fresh());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE("Prefix Extrude source Sketch cancellation and Create Another retain the parent context",
          "[gui][parity-b148][history-marker-shell][history-marker-child][history-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT source Sketch pointer input");
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto before = snapshot(w);
  const auto constraints = w.session().sketch_constraint_catalogs();
  const auto dimensions = w.session().sketch_dimension_catalogs();
  context(w, "fillet", w.history_tools().marker_before_action());
  const auto choose_profile = [&] {
    auto& profile = control<QComboBox>(w, "blcad.extrude.profile");
    REQUIRE(profile.count() > 0);
    profile.setFocus();
    key(profile, Qt::Key_Home);
    CHECK(profile.currentIndex() == 0);
    REQUIRE(w.feature_tools().coordinator().extrude().active());
  };
  quick(w, "extrude");
  choose_profile();
  REQUIRE(w.session().part_history_command_active());
  click(control<QToolButton>(w, "blcad.feature.source_breadcrumb"));
  REQUIRE(w.feature_tools().editing_source_sketch());
  REQUIRE(w.active_sketch());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  const auto sketch = *w.active_sketch();
  const auto line_count = w.session().part_document()->find_sketch(sketch)->line_segments().size();
  auto* line = w.findChild<QAction*>("sketch.create.line");
  REQUIRE(line);
  REQUIRE(line->isEnabled());
  line->trigger();
  settle();
  for (const Point2 point : {Point2{-5, 0}, Point2{5, 0}}) {
    const auto screen = checked(w.viewport().sketch_plane_to_screen(point));
    const QPoint position(qRound(screen.x), qRound(screen.y));
    REQUIRE(w.viewport().rect().contains(position));
    mouse(w.viewport(), QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    mouse(w.viewport(), QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
  }
  CHECK(w.session().part_document()->find_sketch(sketch)->line_segments().size() > line_count);
  CHECK(snapshot(w) != before);
  for (int backout = 0; backout < 4 && w.sketch_tools().tool_active(); ++backout) {
    key(w, Qt::Key_Escape);
    CHECK(w.feature_tools().editing_source_sketch());
  }
  REQUIRE_FALSE(w.sketch_tools().tool_active());
  key(w, Qt::Key_Escape);
  REQUIRE_FALSE(w.feature_tools().active());
  REQUIRE_FALSE(w.active_sketch());
  REQUIRE(w.history_tools().prefix_active());
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK(w.session().history_prefix_count() == 1);
  CHECK(snapshot(w) == before);
  CHECK(w.session().sketch_constraint_catalogs() == constraints);
  CHECK(w.session().sketch_dimension_catalogs() == dimensions);
  quick(w, "extrude");
  choose_profile();
  const auto created = w.feature_tools().coordinator().extrude().feature_id();
  capture(w, "history-end-of-part-child");
  click(control<QPushButton>(w, "blcad.feature.create_another"));
  REQUIRE(w.feature_tools().active());
  REQUIRE(w.session().part_history_command_active());
  CHECK(w.session().history_prefix_count() == 2);
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  click(control<QPushButton>(w, "blcad.feature.cancel"));
  REQUIRE(w.history_tools().prefix_active());
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK(w.session().history_prefix_count() == 2);
  REQUIRE(w.session().part_document()->find_feature(created));
  const auto staged = snapshot(w);
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  CHECK(w.history_tools().prefix_active());
  CHECK(w.session().history_prefix_count() == 1);
  CHECK_FALSE(w.session().can_undo());
  quick(w, "redo");
  CHECK(snapshot(w) == staged);
  CHECK(w.session().history_prefix_count() == 2);
  quick(w, "save");
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(w.session().file_path().string())))) == before);
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  REQUIRE_FALSE(w.history_tools().active());
  REQUIRE(w.session().derived_results_fresh());
  CHECK(w.session().part_shape_cache()->body_shape_count() == 3);
  CHECK(w.session().undo_label() == "Edit at End of Part");
  CHECK(w.statusBar()->currentMessage() == "End of Part applied.");
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  CHECK_FALSE(w.session().history_prefix_count());
}

TEST_CASE("A finishing child selects only prefix geometry and replays the original suffix once",
          "[gui][parity-b148][history-marker-shell][history-marker-finishing][history-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT finishing preview and selection");
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto before = snapshot(w);
  const auto original =
      geometry::RectangleExtrusionAdapter{}
          .summarize(*w.session().part_shape_cache()->find_body_shape(BodyId("body.base")))
          .volume_mm3;
  context(w, "fillet", w.history_tools().marker_before_action());
  quick(w, "fillet");
  auto& targets = control<QListWidget>(w, "blcad.finishing.targets");
  REQUIRE(targets.selectedItems().empty());
  QListWidgetItem* selected = nullptr;
  for (int i = 0; i < targets.count(); ++i)
    if (targets.item(i)->text().contains("Top Back"))
      selected = targets.item(i);
  REQUIRE(selected);
  targets.scrollToItem(selected);
  settle();
  const auto point = targets.visualItemRect(selected).center();
  mouse(*targets.viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
  mouse(*targets.viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  click(control<QPushButton>(w, "blcad.feature.apply"));
  REQUIRE(w.history_tools().prefix_active());
  REQUIRE(w.session().history_prefix_count() == 2);
  CHECK(w.session().part_document()->fillet_features().size() == 2);
  CHECK(w.session().part_shape_cache()->body_shape_count() == 1);
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  REQUIRE_FALSE(w.history_tools().active());
  REQUIRE(w.session().derived_results_fresh());
  REQUIRE(w.session().part_shape_cache()->validate_complete_history_result(
      *w.session().part_document()));
  const auto final_volume =
      geometry::RectangleExtrusionAdapter{}
          .summarize(*w.session().part_shape_cache()->find_body_shape(BodyId("body.base")))
          .volume_mm3;
  CHECK(final_volume < original - 0.1);
  CHECK(w.session().part_shape_cache()->body_shape_count() == 2);
  auto independent =
      checked(geometry::ShapeCache::create(ShapeCacheId("native.prefix.full.oracle")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(*w.session().part_document(),
                                                                 independent));
  CHECK(final_volume ==
        Catch::Approx(geometry::RectangleExtrusionAdapter{}
                          .summarize(*independent.find_body_shape(BodyId("body.base")))
                          .volume_mm3)
            .margin(1e-6));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*w.session().part_shape_cache()->find_body_shape(BodyId("body.tool")))
            .volume_mm3 == Catch::Approx(200));
  capture(w, "history-end-of-part-complete");
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  CHECK_FALSE(w.session().history_prefix_count());
}

TEST_CASE("History context commands preview cancel save apply undo and edit through real Qt input",
          "[gui][parity-b148][history-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory);
  const auto before = snapshot(w);
  const auto camera = w.viewport().camera_bookmark();
  const auto window_size = w.size();
  context(w, "fillet", w.history_tools().suppress_action());
  QString sizes = QString("window=%1x%2, before=%3x%4, min=%5x%6, hint=%7x%8")
                      .arg(w.width())
                      .arg(w.height())
                      .arg(window_size.width())
                      .arg(window_size.height())
                      .arg(w.minimumWidth())
                      .arg(w.minimumHeight())
                      .arg(w.minimumSizeHint().width())
                      .arg(w.minimumSizeHint().height());
  INFO(sizes.toStdString());
  CHECK(w.size() == window_size);
  REQUIRE(w.history_tools().active());
  REQUIRE(w.model_browser().currentItem());
  CHECK(w.model_browser().currentItem()->data(0, Qt::UserRole).toString() == "fillet");
  CHECK(w.viewport().selection_filter_mask() == 0);
  CHECK_FALSE(w.viewport().select_semantic("body/body.base"));
  auto& apply = control<QPushButton>(w, "blcad.history.apply");
  REQUIRE(apply.isEnabled());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.save_to({}));
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(w.session().file_path().string())))) == before);
  capture(w, "history-suppression-preview");
  key(control<QListWidget>(w, "blcad.history.order"), Qt::Key_Escape);
  CHECK(w.statusBar()->currentMessage() == "History edit cancelled.");
  CHECK_FALSE(w.history_tools().active());
  CHECK(snapshot(w) == before);
  CHECK(w.viewport().feature_preview_count() == 0);
  const auto restored_camera = w.viewport().camera_bookmark();
  CHECK(restored_camera.projection == camera.projection);
  CHECK(restored_camera.scale == Catch::Approx(camera.scale).margin(1e-6));
  CHECK(restored_camera.eye.x == Catch::Approx(camera.eye.x).margin(1e-6));
  CHECK(restored_camera.eye.y == Catch::Approx(camera.eye.y).margin(1e-6));
  CHECK(restored_camera.eye.z == Catch::Approx(camera.eye.z).margin(1e-6));
  CHECK(restored_camera.target.x == Catch::Approx(camera.target.x).margin(1e-6));
  CHECK(restored_camera.target.y == Catch::Approx(camera.target.y).margin(1e-6));
  CHECK(restored_camera.target.z == Catch::Approx(camera.target.z).margin(1e-6));
  CHECK(restored_camera.up_direction.x == Catch::Approx(camera.up_direction.x).margin(1e-6));
  CHECK(restored_camera.up_direction.y == Catch::Approx(camera.up_direction.y).margin(1e-6));
  CHECK(restored_camera.up_direction.z == Catch::Approx(camera.up_direction.z).margin(1e-6));
  REQUIRE(w.model_browser().currentItem());
  CHECK(w.model_browser().currentItem()->data(0, Qt::UserRole).toString() == "fillet");
  context(w, "fillet", w.history_tools().suppress_action());
  key(apply, Qt::Key_Return, Qt::ControlModifier);
  CHECK_FALSE(w.history_tools().active());
  REQUIRE(w.session().part_document()->authored_history_intent());
  CHECK((w.session().part_document()->authored_history_intent()->suppressed ==
         std::vector<std::string>{"fillet"}));
  CHECK(w.session().undo_label() == "Suppress Fillet");
  CHECK(w.statusBar()->currentMessage() == "History change applied.");
  CHECK(item(w, "fillet")->text(0).contains("suppressed"));
  CHECK(item(w, "fillet")->font(0).italic());
  auto* group = item(w, "fillet")->parent();
  group->setExpanded(false);
  REQUIRE(w.session().set_presentation_visible("body/body.tool", false));
  w.refresh();
  CHECK_FALSE(item(w, "fillet")->parent()->isExpanded());
  item(w, "fillet")->parent()->setExpanded(true);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.redo());
  context(w, "fillet", w.history_tools().restore_action());
  click(apply);
  CHECK(w.session().part_document()->authored_history_intent()->suppressed.empty());
  context(w, "fillet", w.history_tools().edit_action());
  CHECK(w.feature_edit_tools().active());
  CHECK_FALSE(w.history_tools().active());
  w.feature_edit_tools().cancel();
}

TEST_CASE("History dependency closure is explicit and unchecking it invalidates Apply again",
          "[gui][parity-b148][history-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory);
  const auto before = snapshot(w);
  context(w, "base", w.history_tools().suppress_action());
  auto& apply = control<QPushButton>(w, "blcad.history.apply");
  auto& closure = control<QCheckBox>(w, "blcad.history.include_dependents");
  auto& status = control<QLabel>(w, "blcad.history.status");
  auto& scroll = control<QScrollArea>(w, "blcad.history.inputs_scroll");
  CHECK_FALSE(scroll.isAncestorOf(&status));
  CHECK_FALSE(closure.visibleRegion().isEmpty());
  CHECK_FALSE(apply.isEnabled());
  REQUIRE(control<QListWidget>(w, "blcad.history.dependents").count() == 1);
  CHECK(control<QListWidget>(w, "blcad.history.dependents").item(0)->text().contains("fillet"));
  key(apply, Qt::Key_Return, Qt::ControlModifier);
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.model_browser().currentItem());
  CHECK(w.model_browser().currentItem()->data(0, Qt::UserRole).toString() == "base");
  click(closure);
  CHECK(apply.isEnabled());
  click(closure);
  CHECK_FALSE(apply.isEnabled());
  CHECK(w.session().task().stage() == GuiTaskStage::EditingParameters);
  click(closure);
  capture(w, "history-dependent-closure");
  click(apply);
  const auto* intent = &*w.session().part_document()->authored_history_intent();
  CHECK((intent->suppressed == std::vector<std::string>{"base", "fillet"}));
  CHECK(w.session().part_shape_cache()->find_body_shape(BodyId("body.base")) == nullptr);
  REQUIRE(w.session().part_shape_cache()->find_body_shape(BodyId("body.tool")));
  CHECK(item(w, "body.base")->text(0).contains("unavailable"));
  context(w, "base", w.history_tools().restore_action());
  click(apply);
  CHECK((w.session().part_document()->authored_history_intent()->suppressed ==
         std::vector<std::string>{"fillet"}));
}

TEST_CASE("Browser drag drop proposes validated order without moving the committed tree",
          "[gui][parity-b148][history-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory);
  const auto before = snapshot(w);
  auto& tree = w.model_browser();
  auto* target = item(w, "base");
  tree.scrollToItem(target);
  settle();
  const auto mime_for = [&](const char* id) {
    tree.scrollToItem(item(w, id));
    const auto point = tree.visualItemRect(item(w, id)).center();
    mouse(*tree.viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(*tree.viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    const auto source = index(*tree.model(), QString::fromLatin1(id));
    REQUIRE(source.isValid());
    return std::unique_ptr<QMimeData>(tree.model()->mimeData({source}));
  };
  const auto drop = [&](const QMimeData& mime) {
    tree.scrollToItem(item(w, "base"));
    settle();
    const auto pos = tree.visualItemRect(item(w, "base")).topLeft() + QPoint(30, 2);
    QDragEnterEvent enter(pos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &enter);
    QDragMoveEvent move(pos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &move);
    QDropEvent event(pos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &event);
    settle();
    return event.isAccepted();
  };
  auto illegal = mime_for("fillet");
  {
    QTemporaryDir foreign_directory;
    ShellWindow foreign;
    open(foreign, foreign_directory);
    REQUIRE(foreign.session().presentation_revision() == w.session().presentation_revision());
    const auto foreign_index = index(*foreign.model_browser().model(), "tool");
    REQUIRE(foreign_index.isValid());
    const std::unique_ptr<QMimeData> foreign_mime(
        foreign.model_browser().model()->mimeData({foreign_index}));
    REQUIRE(foreign_mime);
    CHECK_FALSE(drop(*foreign_mime));
    CHECK_FALSE(w.history_tools().active());
    CHECK_FALSE(foreign.history_tools().active());
  }
  REQUIRE(illegal);
  CHECK_FALSE(drop(*illegal));
  CHECK_FALSE(w.history_tools().active());
  CHECK(snapshot(w) == before);
  auto legal = mime_for("tool");
  REQUIRE(legal);
  REQUIRE(drop(*legal));
  REQUIRE(w.history_tools().active());
  CHECK(snapshot(w) == before);
  CHECK(item(w, "base")->parent()->child(0)->data(0, Qt::UserRole).toString() == "base");
  auto& order = control<QListWidget>(w, "blcad.history.order");
  CHECK(order.item(0)->data(Qt::UserRole).toString() == "tool");
  capture(w, "history-reorder-preview");
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK((w.session().part_document()->authored_history_intent()->order ==
         std::vector<std::string>{"tool", "base", "fillet"}));
  CHECK(item(w, "tool")->parent()->child(0)->data(0, Qt::UserRole).toString() == "tool");
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  // A drag payload from an earlier revision cannot author a new candidate.
  CHECK_FALSE(drop(*legal));
  CHECK_FALSE(w.history_tools().active());
}

TEST_CASE("Keyboard reorder rejects dependencies and keeps failure correctable until Cancel",
          "[gui][parity-b148][history-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory);
  const auto before = snapshot(w);
  REQUIRE(w.history_tools().begin_reorder());
  auto& order = control<QListWidget>(w, "blcad.history.order");
  auto& apply = control<QPushButton>(w, "blcad.history.apply");
  CHECK_FALSE(apply.isEnabled());
  int fillet = -1;
  for (int i = 0; i < order.count(); ++i)
    if (order.item(i)->data(Qt::UserRole).toString() == "fillet")
      fillet = i;
  REQUIRE(fillet > 0);
  order.setCurrentRow(fillet);
  for (int i = 0; i < fillet; ++i)
    key(order, Qt::Key_Up, Qt::ControlModifier);
  CHECK_FALSE(apply.isEnabled());
  CHECK(control<QLabel>(w, "blcad.history.status").text().contains("base"));
  CHECK(snapshot(w) == before);
  for (int i = 0; i < fillet; ++i)
    key(order, Qt::Key_Down, Qt::ControlModifier);
  CHECK_FALSE(apply.isEnabled());
  key(order, Qt::Key_Escape);
  CHECK_FALSE(w.history_tools().active());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE("History restore failures keep a valid committed model until dimensions are corrected",
          "[gui][parity-b148][history-shell][history-native]") {
  auto part = history_part();
  REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(30))));
  REQUIRE(part.apply_history_intent({{"base", "fillet", "tool"}, {"fillet"}}));
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, std::move(part));
  const auto before = snapshot(w);
  context(w, "fillet", w.history_tools().restore_action());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.apply").isEnabled());
  CHECK(control<QLabel>(w, "blcad.history.status").text().contains("fillet"));
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  key(control<QListWidget>(w, "blcad.history.order"), Qt::Key_Escape);
  REQUIRE(w.session().part_shape_cache()->validate_complete_history_result(
      *w.session().part_document()));
  context(w, "fillet", w.history_tools().edit_action());
  REQUIRE(w.feature_edit_tools().active());
  auto* radius = qobject_cast<QDoubleSpinBox*>(w.feature_edit_tools().field("/radius_parameter"));
  REQUIRE(radius);
  radius->setValue(1);
  settle();
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK_FALSE(w.feature_edit_tools().active());
  CHECK((w.session().part_document()->authored_history_intent()->suppressed ==
         std::vector<std::string>{"fillet"}));
  context(w, "fillet", w.history_tools().restore_action());
  REQUIRE(control<QPushButton>(w, "blcad.history.apply").isEnabled());
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK(w.session().part_document()->authored_history_intent()->suppressed.empty());
  REQUIRE(w.session().part_shape_cache()->find_feature_shape(FeatureId("fillet")));
}

TEST_CASE("History commands are discoverable with current availability reasons",
          "[gui][parity-b148][history-shell][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory);
  w.model_browser().setCurrentItem(item(w, "fillet"));
  settle();
  bool searched = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    REQUIRE(dialog);
    auto* query = dialog->findChild<QLineEdit*>("blcad.command_search.query");
    auto* results = dialog->findChild<QTreeWidget*>("blcad.command_search.results");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(query);
    REQUIRE(results);
    REQUIRE(buttons);
    query->setText("Unsuppress Feature");
    REQUIRE(results->topLevelItemCount() == 1);
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    bool reason = false;
    for (auto* label : dialog->findChildren<QLabel*>())
      reason = reason || label->text().contains("already active");
    CHECK(reason);
    query->setText("Suppress Feature");
    QTreeWidgetItem* selected = nullptr;
    for (int i = 0; i < results->topLevelItemCount(); ++i)
      if (results->topLevelItem(i)->text(0) == "Suppress Feature")
        selected = results->topLevelItem(i);
    REQUIRE(selected);
    results->setCurrentItem(selected);
    CHECK(selected->text(1).contains("History"));
    REQUIRE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    searched = true;
    click(*buttons->button(QDialogButtonBox::Ok));
  });
  w.ribbon().show_command_search();
  settle();
  CHECK(searched);
  CHECK(w.history_tools().active());
  CHECK_FALSE(w.history_tools().reorder_action().isEnabled());
  CHECK(w.history_tools().reorder_action().toolTip().contains("active command"));
  w.history_tools().cancel();
}

TEST_CASE("Imported bases and immutable Reference Parts explain unavailable history commands",
          "[gui][parity-b148][history-shell][history-native][history-import]") {
  const bool reference = GENERATE(false, true);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  const auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(8))));
  REQUIRE(geometry::StepExporter{}.write_step(shape,
                                              directory.filePath("supplier.step").toStdString()));
  const auto asset = checked(inspect_step_import_asset(root, "supplier.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step", *asset.observed_sha256_digest(),
      reference ? StepImportMode::Reference : StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog"), imported, 0));
  auto part =
      reference
          ? checked(geometry::StepReferenceCommands{}.create_definition(
                source, catalog, DocumentId("reference.supplier"), "Reference supplier", root))
          : checked(geometry::StepEditableCommands{}.create_definition(
                source, catalog, DocumentId("editable.supplier"), "Editable supplier",
                FeatureId("import.base"), root));
  ShellWindow w;
  open(w, directory, std::move(part));
  const auto before = snapshot(w);
  if (!reference)
    w.model_browser().setCurrentItem(item(w, "import.base"));
  settle();
  CHECK_FALSE(w.history_tools().suppress_action().isEnabled());
  CHECK_FALSE(w.history_tools().reorder_action().isEnabled());
  CHECK_FALSE(w.history_tools().edit_action().isEnabled());
  CHECK(
      w.history_tools().suppress_action().toolTip().contains(reference ? "immutable" : "required"));
  CHECK_FALSE(w.history_tools().begin_suppression("import.base", true));
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  if (reference) {
    CHECK_FALSE(w.history_tools().marker_action().isEnabled());
    CHECK_FALSE(w.history_tools().begin_prefix(0));
    CHECK_FALSE(w.session().part_workflow_active());
  } else {
    context(w, "import.base", w.history_tools().marker_before_action());
    REQUIRE(w.session().history_prefix_count() == 0);
    CHECK(w.session().part_shape_cache()->body_shape_count() == 0);
    CHECK_FALSE(w.session().derived_results_fresh());
    click(control<QPushButton>(w, "blcad.history.prefix_apply"));
    CHECK_FALSE(w.history_tools().active());
    CHECK(w.session().derived_results_fresh());
    CHECK(snapshot(w) == before);
    CHECK_FALSE(w.session().can_undo());
  }
}

TEST_CASE("H01 H02 H03 UI01 compose suppression reorder prefix edit restore and physical exchange",
          "[gui][parity-b148][history-combined-acceptance][history-native]") {
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, prefix_part());
  const auto camera = w.viewport().camera_bookmark();
  std::vector<std::string> stages{snapshot(w)};
  context(w, "fillet", w.history_tools().suppress_action());
  click(control<QPushButton>(w, "blcad.history.apply"));
  stages.push_back(snapshot(w));
  CHECK(w.session().undo_label() == "Suppress Fillet");
  CHECK(w.statusBar()->currentMessage() == "History change applied.");
  context(w, "tool", w.history_tools().reorder_action());
  auto& order = control<QListWidget>(w, "blcad.history.order");
  key(order, Qt::Key_End);
  REQUIRE(order.currentItem()->data(Qt::UserRole).toString() == "tool");
  key(order, Qt::Key_Up, Qt::ControlModifier);
  key(order, Qt::Key_Up, Qt::ControlModifier);
  key(order, Qt::Key_Return, Qt::ControlModifier);
  REQUIRE_FALSE(w.history_tools().active());
  CHECK(w.session().undo_label() == "Reorder features");
  CHECK((w.session().part_document()->authored_history_intent()->order ==
         std::vector<std::string>{"tool", "base", "fillet"}));
  stages.push_back(snapshot(w));
  context(w, "fillet", w.history_tools().marker_before_action());
  REQUIRE(w.session().history_prefix_count() == 2);
  CHECK(item(w, "fillet")->text(0).contains("suppressed; after End of Part"));
  edit_prefix_depth(w, 4);
  const auto partial = directory.filePath("partial.step").toStdString();
  CHECK_FALSE(geometry::StepExporter{}.write_part_step(*w.session().part_document(),
                                                       *w.session().part_shape_cache(), partial));
  CHECK_FALSE(std::filesystem::exists(partial));
  quick(w, "save");
  CHECK(checked(serialize_part_document_to_json(checked(
            read_part_document_json_file(w.session().file_path().string())))) == stages.back());
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  REQUIRE(w.session().derived_results_fresh());
  CHECK(w.session().undo_label() == "Edit at End of Part");
  CHECK(w.statusBar()->currentMessage() == "End of Part applied.");
  const auto volume = [&](const char* id) {
    const auto* shape = w.session().part_shape_cache()->find_body_shape(BodyId(id));
    REQUIRE(shape);
    return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
  };
  CHECK(volume("body.base") == Catch::Approx(800));
  CHECK(volume("body.tool") == Catch::Approx(200));
  stages.push_back(snapshot(w));
  context(w, "fillet", w.history_tools().restore_action());
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK(w.session().undo_label() == "Unsuppress Fillet");
  CHECK(w.session().part_document()->authored_history_intent()->suppressed.empty());
  REQUIRE(volume("body.base") < 800);
  CHECK(volume("body.base") > 790);
  CHECK(volume("body.tool") == Catch::Approx(200));
  const double final_volume = volume("body.base") + volume("body.tool");
  stages.push_back(snapshot(w));
  for (std::size_t i = stages.size() - 1; i > 0; --i) {
    quick(w, "undo");
    CHECK(snapshot(w) == stages[i - 1]);
    CHECK_FALSE(w.session().history_prefix_count());
    REQUIRE(w.session().derived_results_fresh());
  }
  CHECK_FALSE(w.session().can_undo());
  for (std::size_t i = 1; i < stages.size(); ++i) {
    quick(w, "redo");
    CHECK(snapshot(w) == stages[i]);
    CHECK_FALSE(w.session().history_prefix_count());
  }
  camera_matches(w, camera);
  quick(w, "save");
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == stages.back());
  REQUIRE(reopened.derived_results_fresh());
  const auto step = directory.filePath("combined.step").toStdString();
  const auto exported = checked(geometry::StepExporter{}.write_part_step(
      *reopened.part_document(), *reopened.part_shape_cache(), step));
  CHECK(exported.exported_solid_body_count == 2);
  const auto root = std::filesystem::path(directory.path().toStdString());
  const auto asset = checked(inspect_step_import_asset(root, "combined.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source =
      checked(StepImportSource::create(StepImportSourceId("readback"), "Readback", "combined.step",
                                       *asset.observed_sha256_digest(), StepImportMode::Reference));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  CHECK(imported.body_count() == 2);
  double readback_volume = 0;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      readback_volume += body.volume_mm3;
  CHECK(readback_volume == Catch::Approx(final_volume).margin(1e-5));
  capture(w, "history-combined-complete");
}

TEST_CASE(
    "Parent Cancel restores a long named folded Part and redo while an Extrude child is active",
    "[gui][parity-b148][history-combined-acceptance][history-parent-cancel][history-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT selection restoration");
  QTemporaryDir directory;
  auto part = prefix_part();
  const std::string long_name = "History acceptance " + std::string(180, 'L');
  auto json = nlohmann::json::parse(checked(serialize_part_document_to_json(part)));
  json["document"]["name"] = long_name;
  part = checked(deserialize_part_document_from_json(json.dump()));
  REQUIRE(part.update_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), long_name, BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  ShellWindow w;
  open(w, directory, std::move(part));
  const auto before = snapshot(w);
  context(w, "fillet", w.history_tools().suppress_action());
  click(control<QPushButton>(w, "blcad.history.apply"));
  const auto redo = snapshot(w);
  quick(w, "undo");
  REQUIRE(w.session().can_redo());
  auto* features = item(w, "fillet")->parent();
  features->setExpanded(false);
  REQUIRE(w.viewport().select_semantic("body/body.base"));
  const auto camera = w.viewport().camera_bookmark();
  const auto filter = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  REQUIRE(w.history_tools().begin_prefix(1));
  CHECK_FALSE(item(w, "fillet")->parent()->isExpanded());
  CHECK(control<QLabel>(w, "blcad.history.prefix_label")
            .toolTip()
            .contains(QString::fromStdString(long_name)));
  quick(w, "extrude");
  auto& profiles = control<QComboBox>(w, "blcad.extrude.profile");
  profiles.setFocus();
  key(profiles, Qt::Key_Home);
  REQUIRE(w.feature_tools().active());
  REQUIRE(w.session().part_history_command_active());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  capture(w, "history-parent-child-long-name");
  click(control<QPushButton>(w, "blcad.history.prefix_cancel"));
  REQUIRE_FALSE(w.history_tools().active());
  CHECK_FALSE(w.feature_tools().active());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.session().history_prefix_count());
  CHECK(snapshot(w) == before);
  CHECK(w.session().derived_results_fresh());
  CHECK_FALSE(w.session().can_undo());
  CHECK(w.session().can_redo());
  CHECK_FALSE(item(w, "fillet")->parent()->isExpanded());
  CHECK(w.viewport().selection_filter_mask() == filter);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  REQUIRE(w.viewport().selected_semantic());
  CHECK(w.viewport().selected_semantic()->semantic_id == "body/body.base");
  camera_matches(w, camera);
  quick(w, "redo");
  CHECK(snapshot(w) == redo);
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  REQUIRE(w.history_tools().begin_prefix(1));
  auto replacement = fixture::native_base();
  const auto target = directory.filePath("replacement.blcad.json");
  REQUIRE(write_part_document_json_file(replacement, target.toStdString()));
  REQUIRE(w.open_path(target));
  CHECK_FALSE(w.history_tools().active());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK_FALSE(w.session().history_prefix_count());
  CHECK(w.session().derived_results_fresh());
  CHECK(snapshot(w) == checked(serialize_part_document_to_json(replacement)));
  CHECK_FALSE(w.session().can_undo());
}

TEST_CASE(
    "H01 mixed native chain preserves independent Bodies across modifier and creator suppression",
    "[gui][parity-b148][history-combined-acceptance][history-mixed-chain][history-native]") {
  auto part = fixture::native_base();
  for (const auto& [id, value] :
       {std::pair{"boss.depth", 14.0}, {"hole.diameter", 2.0}, {"chamfer.distance", 0.5}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  auto boss = checked(Feature::create_additive_extrude(
      FeatureId("boss"), "Boss", SketchId("profile"), ParameterId("boss.depth")));
  REQUIRE(part.add_feature(
      checked(boss.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::Join, BodyId("body.base"), std::nullopt))))));
  auto sketch = checked(Sketch::create(SketchId("hole.sketch"), "Hole", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(
      CircleProfile::create(ProfileId("hole.profile"), ParameterId("hole.diameter"), {0, 0}))));
  REQUIRE(part.add_sketch(sketch));
  auto hole = checked(Feature::create_subtractive_extrude(FeatureId("hole"), "Hole", sketch.id(),
                                                          FeatureId("boss")));
  REQUIRE(part.add_feature(
      checked(hole.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::Cut, BodyId("body.base"), std::nullopt))))));
  const auto edge = [&](PartFeatureInputRole role, SemanticEdge which) {
    return checked(EdgeReference::create_linear(
        role, checked(SemanticEdgeReference::create(FeatureId("boss"), which))));
  };
  REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Fillet", BodyId("body.base"),
      {edge(PartFeatureInputRole::FilletEdge, SemanticEdge::TopFront)}, ParameterId("radius")))));
  REQUIRE(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Chamfer", BodyId("body.base"),
      {edge(PartFeatureInputRole::ChamferEdge, SemanticEdge::BottomBack)},
      ParameterId("chamfer.distance")))));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {10, 0, 0}, false, false))));
  for (const auto& [id, depth] : {std::pair{"tool", "radius"}, {"unrelated", "depth"}}) {
    auto feature = checked(Feature::create_additive_extrude(FeatureId(id), id, SketchId("profile"),
                                                            ParameterId(depth)));
    REQUIRE(
        part.add_feature(checked(feature.with_body_result_context(fixture::new_body(part, id)))));
  }
  REQUIRE(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
      FeatureId("boolean"), BodyBooleanOperation::Subtract, BodyId("body.base"),
      {BodyId("body.tool")}, BodyBooleanResultMode::ModifyTarget, std::nullopt, true))));
  auto legacy = checked(geometry::ShapeCache::create(ShapeCacheId("mixed.legacy.oracle")));
  (void)checked(geometry::GeometryRecomputeExecutor{}.execute_document(part, legacy));
  const auto original_volume = geometry::RectangleExtrusionAdapter{}
                                   .summarize(*legacy.find_body_shape(BodyId("body.base")))
                                   .volume_mm3;
  REQUIRE(part.apply_history_intent(
      {{"base", "boss", "hole", "fillet", "chamfer", "pose", "tool", "boolean", "unrelated"}, {}}));
  QTemporaryDir directory;
  ShellWindow w;
  open(w, directory, std::move(part));
  const auto before = snapshot(w);
  const auto volume = [&](const char* id) {
    const auto* shape = w.session().part_shape_cache()->find_body_shape(BodyId(id));
    REQUIRE(shape);
    return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
  };
  CHECK(volume("body.base") == Catch::Approx(original_volume).margin(1e-6));
  CHECK(volume("body.tool") == Catch::Approx(200));
  CHECK(volume("body.unrelated") == Catch::Approx(1600));
  context(w, "chamfer", w.history_tools().suppress_action());
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK(volume("body.base") > original_volume);
  CHECK(volume("body.tool") == Catch::Approx(200));
  context(w, "chamfer", w.history_tools().restore_action());
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK(snapshot(w) == before);
  CHECK(volume("body.base") == Catch::Approx(original_volume).margin(1e-6));
  context(w, "base", w.history_tools().suppress_action());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.apply").isEnabled());
  REQUIRE(control<QListWidget>(w, "blcad.history.dependents").count() >= 5);
  click(control<QCheckBox>(w, "blcad.history.include_dependents"));
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK_FALSE(w.session().part_shape_cache()->find_body_shape(BodyId("body.base")));
  CHECK(w.session().part_shape_cache()->body_shape_count() == 2);
  CHECK(volume("body.tool") == Catch::Approx(200));
  CHECK(volume("body.unrelated") == Catch::Approx(1600));
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  context(w, "unrelated", w.history_tools().reorder_action());
  auto& order = control<QListWidget>(w, "blcad.history.order");
  key(order, Qt::Key_End);
  for (int i = order.count() - 1; i > 0; --i)
    key(order, Qt::Key_Up, Qt::ControlModifier);
  click(control<QPushButton>(w, "blcad.history.apply"));
  CHECK(w.session().part_document()->authored_history_intent()->order.front() == "unrelated");
  CHECK(volume("body.base") == Catch::Approx(original_volume).margin(1e-6));
  CHECK(volume("body.unrelated") == Catch::Approx(1600));
  capture(w, "history-mixed-chain-complete");
}

TEST_CASE("Resource-bearing Part prefix creation preserves pins Save Undo and default inheritance",
          "[gui][parity-b149][resource-history-shell][engineering-resource-integration]") {
  QTemporaryDir directory;
  auto part = prefix_part();
  blcad::test::engineering_fixture::annotate(part);
  ShellWindow w;
  open(w, directory, std::move(part));
  const auto before = snapshot(w);
  CHECK(nlohmann::json::parse(before).at("version") == 3);
  context(w, "fillet", w.history_tools().marker_before_action());
  edit_prefix_depth(w, 4);
  quick(w, "save");
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(w.session().file_path().string())))) == before);
  click(control<QPushButton>(w, "blcad.history.prefix_cancel"));
  CHECK(snapshot(w) == before);
  context(w, "fillet", w.history_tools().marker_before_action());
  quick(w, "extrude");
  auto& profiles = control<QComboBox>(w, "blcad.extrude.profile");
  profiles.setFocus();
  key(profiles, Qt::Key_Home);
  REQUIRE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  click(control<QPushButton>(w, "blcad.feature.apply"));
  REQUIRE(w.history_tools().prefix_active());
  quick(w, "save");
  CHECK(checked(serialize_part_document_to_json(
            checked(read_part_document_json_file(w.session().file_path().string())))) == before);
  click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  REQUIRE(w.session().derived_results_fresh());
  const auto& result = *w.session().part_document();
  REQUIRE(result.bodies().size() == 3);
  CHECK(result.effective_material_for_body(BodyId("body.tool"))->key() ==
        blcad::test::engineering_fixture::key("synthetic.material", "r2"));
  for (const auto& body : result.bodies())
    if (body.id() != BodyId("body.tool")) {
      REQUIRE(result.effective_material_for_body(body.id()));
      CHECK(result.effective_material_for_body(body.id())->key() ==
            blcad::test::engineering_fixture::key());
    }
  const auto after = snapshot(w);
  capture(w, "resource-prefix-inheritance");
  quick(w, "undo");
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  quick(w, "redo");
  CHECK(snapshot(w) == after);
  quick(w, "save");
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
  CHECK(reopened.derived_results_fresh());
}

TEST_CASE("Semantic Hole browser history keeps invalid extent restoration atomic",
          "[gui][parity-b153][hole-history-shell]") {
  auto part = blcad::test::hole_fixture::part("hole.shell", true);
  const auto hole = blcad::test::hole_fixture::hole("hole", blcad::test::hole_fixture::standard(),
                                                    HoleToNextExtent{});
  REQUIRE(part.update_hole_feature(checked(HoleFeature::create(
      hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(), hole.placements(),
      hole.diameter(), hole.extent(), HoleDirection::SupportNormal))));
  REQUIRE(part.apply_history_intent({{"base", "hole"}, {"hole"}}));
  ShellWindow w;
  QTemporaryDir directory;
  open(w, directory, part);
  const auto before = snapshot(w);
  auto* row = item(w, "hole");
  REQUIRE(row);
  CHECK(row->data(0, ShellHistoryBrowser::owner_role).toString() == "hole.shell");
  CHECK(row->text(0).contains("Semantic Hole"));
  REQUIRE(w.session().derived_results_fresh());
  REQUIRE(w.session().part_shape_cache()->validate_complete_geometry_result(
      *w.session().part_document()));
  context(w, "hole", w.history_tools().restore_action());
  REQUIRE(w.history_tools().active());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.apply").isEnabled());
  CHECK(control<QLabel>(w, "blcad.history.status")
            .text()
            .contains("no forward material intersection"));
  CHECK_FALSE(w.history_tools().apply());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  w.history_tools().cancel();
  REQUIRE(w.history_tools().begin_prefix(1));
  CHECK_FALSE(w.session().part_shape_cache()->validate_complete_history_result(
      *w.session().part_document()));
  CHECK(w.session().part_shape_cache()->body_shape_count() == 1);
  w.history_tools().cancel();
  CHECK(snapshot(w) == before);
  REQUIRE(w.session().derived_results_fresh());
  CHECK_FALSE(w.session().can_undo());
  capture(w, "semantic-hole-suppressed-browser");
  REQUIRE(w.save_to({}));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == before);
  CHECK(reopened.derived_results_fresh());
}

TEST_CASE("Semantic Hole restore previews a physical cut and commits one reversible history step",
          "[gui][parity-b153][hole-history-shell][hole-straight-shell]") {
  const bool to_next = GENERATE(false, true);
  auto part = blcad::test::hole_fixture::part("hole.shell.cut", true);
  if (to_next)
    REQUIRE(part.update_hole_feature(blcad::test::hole_fixture::hole(
        "hole", blcad::test::hole_fixture::standard(), HoleToNextExtent{})));
  REQUIRE(part.apply_history_intent({{"base", "hole"}, {"hole"}}));
  ShellWindow w;
  QTemporaryDir directory;
  open(w, directory, part);
  const auto before = snapshot(w);
  context(w, "hole", w.history_tools().restore_action());
  REQUIRE(control<QPushButton>(w, "blcad.history.apply").isEnabled());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  click(control<QPushButton>(w, "blcad.history.apply"));
  const auto after = snapshot(w);
  CHECK(after != before);
  REQUIRE(w.session().derived_results_fresh());
  const auto check_volume = [&](double expected) {
    const auto* cache = w.session().part_shape_cache();
    REQUIRE(cache);
    REQUIRE(cache->validate_complete_geometry_result(*w.session().part_document()));
    CHECK(geometry::RectangleExtrusionAdapter{}
              .summarize(*cache->find_body_shape(BodyId("body.base")))
              .volume_mm3 == Catch::Approx(expected).margin(1e-6));
  };
  const double cut_volume = 1600 - std::numbers::pi * 3.3 * 3.3 * 8;
  check_volume(cut_volume);
  CHECK(w.session().part_shape_cache()->find_body_result(BodyId("body.base"))->source_feature_id ==
        FeatureId("hole"));
  capture(w, to_next ? "semantic-hole-to-next-cut" : "semantic-hole-restored-cut");
  REQUIRE(w.save_to({}));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
  REQUIRE(reopened.derived_results_fresh());
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*reopened.part_shape_cache()->find_body_shape(BodyId("body.base")))
            .volume_mm3 == Catch::Approx(cut_volume).margin(1e-6));
  REQUIRE(w.session().undo());
  CHECK(snapshot(w) == before);
  check_volume(1600);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.session().redo());
  CHECK(snapshot(w) == after);
  check_volume(cut_volume);
}
