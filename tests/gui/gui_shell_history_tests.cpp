#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
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
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <memory>

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
  const QSize desired_size(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 1000 : 1440,
                           qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 720 : 920);
  const QSize practical_size =
      QApplication::platformName() == "offscreen"
          ? desired_size
          : desired_size.boundedTo(w.screen()->availableGeometry().size() - QSize(48, 48));
  w.resize(practical_size);
  w.show();
  settle_paint();
  w.resize(practical_size);
  settle_paint();
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
  if (qEnvironmentVariableIsSet("BLCAD_UI_COMPACT"))
    CHECK(w.width() <= 1000);
  REQUIRE(w.grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png")));
}
} // namespace

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
}
