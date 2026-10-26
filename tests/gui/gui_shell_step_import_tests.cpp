#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/step_import_source.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}

QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

void write(const QString& path, const QByteArray& value) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(file.write(value) == value.size());
}

std::string digest(const QByteArray& value) {
  return QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex().toStdString();
}

// Block 137 deliberately registers opaque external bytes. These small source
// fixtures do not claim to be transferable STEP geometry before Block 138.
const QByteArray first_asset("ISO-10303-21;\n/* supplier revision A */\nEND-ISO-10303-21;\n");
const QByteArray second_asset("ISO-10303-21;\n/* supplier revision B */\nEND-ISO-10303-21;\n");
const QByteArray third_asset("ISO-10303-21;\n/* supplier revision C */\nEND-ISO-10303-21;\n");

// User-facing workflows stop at Qt input. Core/session reads below are oracles;
// external file writes simulate supplier changes, not authoring shortcuts.
class SourceUi {
public:
  SourceUi() : original_dialog_setting_(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs)) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    window.resize(1280, 800);
    window.show();
    flush();
    if (QApplication::platformName() == "xcb")
      REQUIRE(window.viewport().native_viewer_available());
  }
  ~SourceUi() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, original_dialog_setting_);
  }

  template <typename T> T& widget(const QString& id) {
    auto* result = window.findChild<T*>(id);
    INFO(id.toStdString());
    REQUIRE(result);
    return *result;
  }
  template <typename T> T& field(const char* id) {
    return widget<T>(QStringLiteral("blcad.step_import.") + id);
  }
  QAction& action(const QString& id) {
    return widget<QAction>(id.startsWith("sketch.") ? id : "blcad.action." + id);
  }
  void flush() {
    qApp->processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    qApp->processEvents();
  }
  void reveal(QWidget& control) {
    flush();
    for (auto* parent = control.parentWidget(); parent; parent = parent->parentWidget())
      if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
        const auto center = control.mapTo(scroll->widget(), control.rect().center());
        scroll->ensureVisible(center.x(), center.y(),
                              std::min(control.width() / 2 + 8, scroll->viewport()->width() / 2),
                              std::min(control.height() / 2 + 8, scroll->viewport()->height() / 2));
      }
    flush();
    INFO(control.objectName().toStdString());
    REQUIRE(control.isVisible());
    const auto* hit = window.childAt(control.mapTo(&window, control.rect().center()));
    INFO("Hit: " << (hit ? hit->objectName().toStdString() : "none"));
    REQUIRE((hit == &control || (hit && control.isAncestorOf(hit))));
  }
  void click_at(QWidget& target, QPointF point) {
    QMouseEvent press(QEvent::MouseButtonPress, point, target.mapToGlobal(point.toPoint()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, target.mapToGlobal(point.toPoint()),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&target, &press);
    QApplication::sendEvent(&target, &release);
    flush();
  }
  void click(QWidget& control) {
    REQUIRE(control.isEnabled());
    reveal(control);
    click_at(control, control.rect().center());
  }
  void key(QWidget& control, int code, const QString& text = {},
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers, text);
    QApplication::sendEvent(&control, &press);
    QApplication::sendEvent(&control, &release);
    flush();
  }
  void text(QLineEdit& control, const QString& value) {
    click(control);
    key(control, Qt::Key_A, {}, Qt::ControlModifier);
    if (value.isEmpty())
      key(control, Qt::Key_Backspace);
    else
      key(control, 0, value);
    key(control, Qt::Key_Tab);
  }
  void choose(QComboBox& combo, int index) {
    REQUIRE(index >= 0);
    REQUIRE(index < combo.count());
    reveal(combo);
    REQUIRE(combo.isEnabled());
    combo.setFocus();
    key(combo, Qt::Key_Home);
    for (int i = 0; i < index; ++i)
      key(combo, Qt::Key_Down);
    REQUIRE(combo.currentIndex() == index);
  }
  void source(const QString& id) {
    auto& combo = field<QComboBox>("source");
    choose(combo, combo.findData(id));
  }
  void button(const char* id) {
    click(field<QPushButton>(id));
  }
  void command(const QString& id) {
    INFO("Ribbon command: " << id.toStdString());
    auto& selected = action(id);
    REQUIRE(selected.isEnabled());
    const int tab = id.startsWith("sketch.") || id == "finish_sketch" ? 1 : 0;
    window.ribbon().set_current_tab(tab);
    flush();
    for (auto* button : window.ribbon().findChildren<QToolButton*>()) {
      // STEP sources intentionally shares one action across the model and
      // Assembly pages. QObject child order changes when a page is raised.
      if (!button->isVisible())
        continue;
      if (button->defaultAction() == &selected) {
        click(*button);
        return;
      }
      if (auto* menu = button->menu(); menu && menu->actions().contains(&selected)) {
        reveal(*button);
        bool chosen = false;
        bool timed_out = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&selected, &QAction::triggered, &watchdog, [&] { chosen = true; });
        QObject::connect(&watchdog, &QTimer::timeout, menu, [&] {
          timed_out = true;
          menu->close();
        });
        QTimer::singleShot(0, menu, [&, menu] {
          menu->setActiveAction(&selected);
          key(*menu, Qt::Key_Return);
        });
        watchdog.start(5000);
        // InstantPopup enters a nested event loop on the actual mouse press.
        // Avoid draining events between scheduling menu input and that press.
        click_at(*button, button->rect().center());
        watchdog.stop();
        REQUIRE_FALSE(timed_out);
        REQUIRE(chosen);
        return;
      }
    }
    FAIL("Missing visible ribbon entry for " + id.toStdString());
  }
  template <typename Dialog, typename Respond>
  void dialog(const std::function<void()>& invoke, Respond respond) {
    bool handled = false;
    bool timeout = false;
    QTimer poll;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
      if (auto* active = qobject_cast<Dialog*>(QApplication::activeModalWidget())) {
        poll.stop();
        handled = true;
        respond(*active);
      }
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
      timeout = true;
      if (auto* active = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        active->reject();
    });
    poll.start();
    watchdog.start(5000);
    invoke();
    poll.stop();
    watchdog.stop();
    REQUIRE_FALSE(timeout);
    REQUIRE(handled);
    flush();
  }
  void file(const QString& action_id, const QString& path) {
    dialog<QFileDialog>([&] { command(action_id); },
                        [&](QFileDialog& chooser) {
                          chooser.selectFile(path);
                          QMetaObject::invokeMethod(&chooser, "accept", Qt::DirectConnection);
                        });
  }
  void browse(const QString& path, bool accept = true) {
    dialog<QFileDialog>([&] { button("browse"); },
                        [&](QFileDialog& chooser) {
                          if (!accept) {
                            key(chooser, Qt::Key_Escape);
                            return;
                          }
                          chooser.selectFile(path);
                          QMetaObject::invokeMethod(&chooser, "accept", Qt::DirectConnection);
                        });
  }
  void tree(const std::string& id) {
    auto& browser = window.model_browser();
    const auto find = [&](auto&& self, QTreeWidgetItem* item) -> QTreeWidgetItem* {
      if (item->data(0, Qt::UserRole).toString().toStdString() == id)
        return item;
      for (int i = 0; i < item->childCount(); ++i)
        if (auto* result = self(self, item->child(i)))
          return result;
      return nullptr;
    };
    QTreeWidgetItem* item{};
    for (int i = 0; i < browser.topLevelItemCount() && !item; ++i)
      item = find(find, browser.topLevelItem(i));
    INFO(id);
    REQUIRE(item);
    for (auto* parent = item->parent(); parent; parent = parent->parent())
      parent->setExpanded(true);
    browser.scrollToItem(item);
    flush();
    click_at(*browser.viewport(), browser.visualItemRect(item).center());
  }
  void authored_part() {
    dialog<QInputDialog>([&] { command("new_part"); },
                         [&](QInputDialog& input) {
                           input.setTextValue("Supplier source workflow");
                           input.accept();
                         });
    tree("datum.xy");
    command("create_sketch");
    REQUIRE(window.active_sketch());
    command("sketch.create.corner_rectangle");
    const auto coordinates = [&](const QString& input) {
      for (auto character : input)
        key(window, character.unicode(), QString(character));
      key(window, Qt::Key_Return);
    };
    coordinates("10;10");
    const auto point = checked(window.viewport().sketch_plane_to_screen({30, 20}));
    QMouseEvent move(QEvent::MouseMove, {point.x, point.y},
                     window.viewport().mapToGlobal(QPoint(qRound(point.x), qRound(point.y))),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &move);
    flush();
    coordinates("20;10");
    command("finish_sketch");
    command("extrude");
    text(widget<QLineEdit>("blcad.manipulator_numeric_hud"), "6 mm");
    key(widget<QLineEdit>("blcad.manipulator_numeric_hud"), Qt::Key_Return);
    click(widget<QPushButton>("blcad.feature.apply"));
    REQUIRE(window.session().part_document()->bodies().size() == 1);
    REQUIRE(window.session().part_shape_cache()->body_shape_count() == 1);
  }
  std::string snapshot() {
    return window.session().project()
               ? checked(serialize_project_to_json(*window.session().project()))
               : checked(serialize_part_document_to_json(*window.session().part_document()));
  }
  const std::vector<StepImportSource>& sources() {
    return window.session().project() ? window.session().project()->step_import_sources()
                                      : window.session().part_document()->step_import_sources();
  }
  void begin() {
    command("step_sources");
    REQUIRE(window.step_import_tools().active());
  }
  void draft(const QString& name, const QString& path, StepImportMode mode) {
    source({});
    text(field<QLineEdit>("name"), name);
    text(field<QLineEdit>("path"), path);
    auto& modes = field<QComboBox>("mode");
    choose(modes, modes.findData(static_cast<int>(mode)));
  }
  void preview() {
    button("preview");
    INFO(field<QLabel>("summary").text().toStdString());
    REQUIRE(field<QPushButton>("apply").isEnabled());
  }
  void apply() {
    button("apply");
    REQUIRE_FALSE(window.step_import_tools().active());
    REQUIRE_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
  }
  void inspect(const QString& id, StepImportFreshness expected) {
    source(id);
    button("inspect");
    INFO(field<QLabel>("summary").text().toStdString());
    REQUIRE(field<QLabel>("status").property("freshness").isValid());
    CHECK(field<QLabel>("status").property("freshness").toInt() == static_cast<int>(expected));
  }
  void roundtrip(const QString& path) {
    const auto accepted = snapshot();
    command("save");
    const auto saved = bytes(path);
    file("open", path);
    CHECK(snapshot() == accepted);
    command("save");
    CHECK(bytes(path) == saved);
    CHECK_FALSE(window.session().dirty());
  }
  void capture(const char* name) {
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (directory.isEmpty())
      return;
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath(QString::fromUtf8(name) + ".png")));
  }

  ShellWindow window;

private:
  bool original_dialog_setting_;
};
} // namespace

TEST_CASE("STEP source UI workflow registers explicit modes preserves modeling and refreshes "
          "changed bytes",
          "[gui][integration][block137-step-import][shell-step-import-workflow]") {
  SourceUi ui;
  QTemporaryDir directory;
  QTemporaryDir external;
  REQUIRE(directory.isValid());
  REQUIRE(external.isValid());
  REQUIRE(QDir(directory.path()).mkdir("assets"));
  const auto path = directory.filePath("supplier.blcad.json");
  const auto asset = directory.filePath("assets/reference.step");
  const auto editable_asset = directory.filePath("assets/editable.step");
  write(asset, first_asset);
  write(editable_asset, second_asset);
  const auto external_asset = external.filePath("outside.step");
  write(external_asset, first_asset);
  ui.authored_part();
  CHECK_FALSE(ui.action("step_sources").isEnabled());
  ui.file("save_as", path);
  const auto base = ui.snapshot();
  const auto original = bytes(path);
  const auto body = ui.window.session().part_document()->bodies().front().id();
  const auto original_volume =
      geometry::RectangleExtrusionAdapter{}
          .summarize(*ui.window.session().part_shape_cache()->find_body_shape(body))
          .volume_mm3;
  CHECK(original_volume == Approx(1200).margin(1e-5));

  ui.begin();
  CHECK(ui.field<QComboBox>("mode").currentIndex() == -1);
  ui.text(ui.field<QLineEdit>("name"), "Supplier reference");
  ui.text(ui.field<QLineEdit>("path"), "assets/reference.step");
  if (ui.field<QPushButton>("preview").isEnabled())
    ui.button("preview");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == base);
  ui.draft("Supplier reference", "assets/reference.step", StepImportMode::Reference);
  ui.preview();
  ui.text(ui.field<QLineEdit>("name"), "Reviewed supplier reference");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  ui.preview();
  CHECK(ui.snapshot() == base);
  CHECK_FALSE(ui.window.session().dirty());
  ui.command("save");
  REQUIRE(ui.window.step_import_tools().active());
  CHECK(bytes(path) == original);
  ui.apply();
  REQUIRE(ui.sources().size() == 1);
  const auto reference_id = QString::fromStdString(ui.sources().front().id().value());
  CHECK(ui.sources().front().mode() == StepImportMode::Reference);
  CHECK(ui.sources().front().sha256_digest() == digest(first_asset));
  CHECK(ui.sources().front().project_relative_asset_path() == "assets/reference.step");
  const auto one_source = ui.snapshot();

  ui.begin();
  ui.draft("Duplicate under another mode", "assets/./reference.step", StepImportMode::EditableBody);
  ui.button("preview");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == one_source);
  const auto outside_path = QDir(directory.path()).relativeFilePath(external_asset);
  REQUIRE(outside_path.startsWith("../"));
  ui.text(ui.field<QLineEdit>("path"), outside_path);
  ui.button("preview");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == one_source);
  ui.draft("Editable supplier base", "assets/editable.step", StepImportMode::EditableBody);
  ui.preview();
  ui.apply();
  REQUIRE(ui.sources().size() == 2);
  CHECK(ui.sources().back().mode() == StepImportMode::EditableBody);
  const auto two_sources = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == one_source);
  ui.command("undo");
  CHECK(ui.snapshot() == base);
  ui.command("redo");
  CHECK(ui.snapshot() == one_source);
  ui.command("redo");
  CHECK(ui.snapshot() == two_sources);
  ui.command("save");

  write(asset, second_asset);
  ui.begin();
  const auto undo_label = std::string(ui.window.session().undo_label());
  ui.inspect(reference_id, StepImportFreshness::Changed);
  CHECK(ui.field<QPlainTextEdit>("expected_digest")
            .toPlainText()
            .contains(QString::fromStdString(digest(first_asset))));
  CHECK(ui.field<QPlainTextEdit>("observed_digest")
            .toPlainText()
            .contains(QString::fromStdString(digest(second_asset))));
  CHECK(ui.snapshot() == two_sources);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.window.session().undo_label() == undo_label);
  ui.preview();
  write(asset, third_asset);
  ui.button("apply");
  REQUIRE(ui.window.step_import_tools().active());
  CHECK(ui.snapshot() == two_sources);
  CHECK(ui.window.session().undo_label() == undo_label);
  ui.button("inspect");
  ui.preview();
  ui.capture("step-source-changed-refresh-preview");
  ui.apply();
  CHECK(ui.sources().front().sha256_digest() == digest(third_asset));
  const auto refreshed = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == two_sources);
  ui.command("redo");
  CHECK(ui.snapshot() == refreshed);
  ui.roundtrip(path);
  ui.begin();
  ui.inspect(reference_id, StepImportFreshness::Available);
  CHECK(ui.field<QTableWidget>("catalog").rowCount() == 2);
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  CHECK(ui.snapshot() == refreshed);
  ui.apply();
  REQUIRE(ui.sources().size() == 1);
  const auto removed = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == refreshed);
  ui.command("redo");
  CHECK(ui.snapshot() == removed);
  ui.roundtrip(path);
  CHECK(bytes(asset) == third_asset);
  CHECK(bytes(editable_asset) == second_asset);
  CHECK(bytes(external_asset) == first_asset);
  CHECK(ui.window.session().part_document()->bodies().front().id() == body);
  CHECK(ui.window.session().part_shape_cache()->body_shape_count() == 1);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*ui.window.session().part_shape_cache()->find_body_shape(body))
            .volume_mm3 == Approx(original_volume).margin(1e-8));
}

TEST_CASE(
    "STEP Project source workflow relocates assets and recovers missing and unreadable sources",
    "[gui][integration][block137-step-import][shell-step-import-workflow]") {
  SourceUi ui;
  QTemporaryDir original;
  QTemporaryDir relocated;
  REQUIRE(original.isValid());
  REQUIRE(relocated.isValid());
  REQUIRE(QDir(original.path()).mkdir("assets"));
  REQUIRE(QDir(relocated.path()).mkdir("assets"));
  const auto path = original.filePath("nested.blcad.project.json");
  const auto moved_path = relocated.filePath("nested.blcad.project.json");
  const auto asset = original.filePath("assets/assembly.step");
  const auto moved_asset = relocated.filePath("assets/assembly.step");
  write(asset, first_asset);
  ui.file("open",
          QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/nested_subassembly.blcad.project.json");
  ui.file("save_as", path);
  const auto before = ui.snapshot();
  const auto original_leaves =
      checked(AssemblyLeafOccurrenceResolver{}.resolve(*ui.window.session().project()));
  REQUIRE(original_leaves.size() == 2);
  ui.begin();
  ui.draft("Shared product source", "assets/assembly.step", StepImportMode::Reference);
  ui.preview();
  ui.apply();
  const auto accepted = ui.snapshot();
  const auto id = QString::fromStdString(ui.sources().front().id().value());
  ui.command("save");
  write(moved_asset, first_asset);
  ui.file("save_as", moved_path);
  CHECK(ui.snapshot() == accepted);
  CHECK(bytes(path) == bytes(moved_path));
  ui.roundtrip(moved_path);
  ui.begin();
  ui.inspect(id, StepImportFreshness::Available);
  const auto clean_undo = std::string(ui.window.session().undo_label());
  REQUIRE(QFile::remove(moved_asset));
  ui.button("inspect");
  CHECK(ui.field<QLabel>("status").property("freshness").toInt() ==
        static_cast<int>(StepImportFreshness::Missing));
  ui.button("preview");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == accepted);
  ui.button("cancel");
  ui.command("save");
  ui.file("open", moved_path);
  CHECK(ui.snapshot() == accepted);
  CHECK(ui.window.session().derived_results_fresh());
  ui.begin();
  ui.inspect(id, StepImportFreshness::Missing);
  REQUIRE(QDir().mkdir(moved_asset));
  ui.button("inspect");
  CHECK(ui.field<QLabel>("status").property("freshness").toInt() ==
        static_cast<int>(StepImportFreshness::Unreadable));
  ui.button("preview");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == accepted);
  REQUIRE(QDir().rmdir(moved_asset));
  write(moved_asset, first_asset);
  ui.inspect(id, StepImportFreshness::Available);
  CHECK(ui.window.session().undo_label() == clean_undo);
  CHECK_FALSE(ui.window.session().dirty());
  ui.capture("step-source-relocated-project-recovery");
  ui.button("cancel");
  ui.roundtrip(moved_path);
  const auto leaves =
      checked(AssemblyLeafOccurrenceResolver{}.resolve(*ui.window.session().project()));
  REQUIRE(leaves.size() == original_leaves.size());
  for (std::size_t i = 0; i < leaves.size(); ++i) {
    CHECK(leaves[i].subassembly_path == original_leaves[i].subassembly_path);
    CHECK(leaves[i].component_instance == original_leaves[i].component_instance);
    CHECK(leaves[i].referenced_part_document == original_leaves[i].referenced_part_document);
    CHECK(leaves[i].transforms_inner_to_outer == original_leaves[i].transforms_inner_to_outer);
  }
  ui.begin();
  ui.source(id);
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  ui.apply();
  CHECK(ui.snapshot() == before);
  ui.command("undo");
  CHECK(ui.snapshot() == accepted);
  ui.command("save");
  CHECK(bytes(asset) == first_asset);
  CHECK(bytes(moved_asset) == first_asset);
}

TEST_CASE("STEP source task guards and lifecycle discard previews without changing accepted intent",
          "[gui][block137-step-import][shell-step-import-lifecycle]") {
  SourceUi ui;
  REQUIRE_FALSE(ui.action("step_sources").isEnabled());
  REQUIRE(ui.window.new_part("Lifecycle source"));
  CHECK_FALSE(ui.action("step_sources").isEnabled());
  CHECK_FALSE(ui.window.step_import_tools().begin());
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("lifecycle.blcad.json");
  const auto copy = directory.filePath("renamed.blcad.json");
  write(directory.filePath("source.step"), first_asset);
  REQUIRE(ui.window.save_to(path));
  const auto before = ui.snapshot();
  const auto initial_can_undo = ui.window.session().can_undo();
  const auto initial_undo_label = std::string(ui.window.session().undo_label());
  const auto initial_can_redo = ui.window.session().can_redo();
  for (int route = 0; route < 3; ++route) {
    CAPTURE(route);
    ui.begin();
    ui.draft("Disposable", "source.step", StepImportMode::Reference);
    ui.preview();
    if (route == 0)
      ui.button("cancel");
    else if (route == 1)
      ui.key(ui.window.viewport(), Qt::Key_Escape);
    else
      ui.field<QDockWidget>("task").close();
    ui.flush();
    CHECK_FALSE(ui.window.step_import_tools().active());
    CHECK_FALSE(ui.window.session().task().active());
    CHECK(ui.snapshot() == before);
    CHECK_FALSE(ui.window.session().dirty());
    CHECK(ui.window.session().can_undo() == initial_can_undo);
    CHECK(ui.window.session().undo_label() == initial_undo_label);
    CHECK(ui.window.session().can_redo() == initial_can_redo);
  }
  REQUIRE(ui.window.session().task().begin("other.task"));
  ui.window.refresh();
  CHECK_FALSE(ui.action("step_sources").isEnabled());
  CHECK_FALSE(ui.window.step_import_tools().begin());
  CHECK(ui.window.session().task().command_id() == "other.task");
  REQUIRE(ui.window.session().task().cancel());
  ui.window.refresh();
  ui.begin();
  ui.draft("Relocated draft", "source.step", StepImportMode::EditableBody);
  ui.preview();
  REQUIRE(ui.window.save_to(copy));
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK(ui.snapshot() == before);
  ui.begin();
  ui.draft("Accepted", "source.step", StepImportMode::EditableBody);
  ui.preview();
  ui.apply();
  const auto accepted = ui.snapshot();
  const auto id = QString::fromStdString(ui.sources().front().id().value());
  ui.begin();
  ui.source(id);
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  ui.command("undo");
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK(ui.snapshot() == before);
  ui.begin();
  ui.draft("Discarded before Redo", "source.step", StepImportMode::Reference);
  ui.preview();
  CHECK(ui.snapshot() == before);
  ui.command("redo");
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK_FALSE(ui.window.session().task().active());
  CHECK(ui.snapshot() == accepted);
  ui.command("save");
  ui.begin();
  ui.source(id);
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  ui.file("open", copy);
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK(ui.snapshot() == accepted);
  ui.tree("datum.xy");
  ui.command("create_sketch");
  CHECK_FALSE(ui.action("step_sources").isEnabled());
  CHECK_FALSE(ui.window.step_import_tools().begin());
  REQUIRE(ui.window.active_sketch());
  ui.command("finish_sketch");
  ui.command("save");
  ui.begin();
  ui.source(id);
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  ui.dialog<QInputDialog>([&] { ui.command("new_part"); },
                          [](QInputDialog& input) {
                            input.setTextValue("New source document");
                            input.accept();
                          });
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK_FALSE(ui.window.session().task().active());
  CHECK(ui.window.session().file_path().empty());
  CHECK(ui.sources().empty());
  CHECK(ui.window.session().part_document()->name() == "New source document");
  CHECK(bytes(directory.filePath("source.step")) == first_asset);
}

TEST_CASE("Native STEP source dialog and explicit mode controls remain reachable at 1280 by 800",
          "[gui][block137-step-import][native-shell-step-import]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native shell layout and source file dialogs");
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(QDir(directory.path()).mkdir("assets"));
  const auto path = directory.filePath("native.blcad.json");
  const QString relative_asset =
      "assets/native-supplier-bracket-revision-A-for-assembly-source-registration.step";
  const auto asset = directory.filePath(relative_asset);
  write(asset, first_asset);
  REQUIRE(ui.window.new_part("Native source task"));
  ui.file("save_as", path);
  const auto before = ui.snapshot();
  ui.begin();
  const auto no_horizontal_clipping = [&] {
    ui.flush();
    CHECK(ui.window.width() == 1280);
    CHECK(ui.window.height() == 800);
    CHECK(ui.field<QScrollArea>("scroll").horizontalScrollBar()->maximum() == 0);
    CHECK(ui.field<QTableWidget>("catalog").horizontalScrollBar()->maximum() == 0);
    for (const auto* name : {"expected_digest", "observed_digest"}) {
      auto& fingerprint = ui.field<QPlainTextEdit>(name);
      const auto margins = fingerprint.contentsMargins();
      INFO("Fingerprint " << name << ": widget height " << fingerprint.height()
                          << ", viewport height " << fingerprint.viewport()->height()
                          << ", contents margins " << margins.left() << ',' << margins.top() << ','
                          << margins.right() << ',' << margins.bottom() << ", line spacing "
                          << fingerprint.fontMetrics().lineSpacing());
      CHECK(fingerprint.isReadOnly());
      CHECK(fingerprint.horizontalScrollBar()->maximum() == 0);
      CHECK(fingerprint.verticalScrollBar()->maximum() == 0);
    }
  };
  ui.text(ui.field<QLineEdit>("name"), "Native supplier source");
  ui.browse(asset, false);
  REQUIRE(ui.window.step_import_tools().active());
  CHECK(ui.field<QLineEdit>("name").text() == "Native supplier source");
  CHECK(ui.field<QLineEdit>("path").text().isEmpty());
  CHECK(ui.snapshot() == before);
  ui.browse(asset);
  CHECK(ui.field<QLineEdit>("path").text() == relative_asset);
  CHECK(ui.field<QLineEdit>("path").toolTip() == relative_asset);
  CHECK(ui.field<QComboBox>("mode").currentIndex() == -1);
  ui.choose(ui.field<QComboBox>("mode"), 1);
  ui.preview();
  ui.reveal(ui.field<QPushButton>("apply"));
  ui.reveal(ui.field<QPushButton>("cancel"));
  no_horizontal_clipping();
  ui.capture("step-source-native-register-preview");
  ui.reveal(ui.field<QPlainTextEdit>("expected_digest"));
  ui.reveal(ui.field<QPlainTextEdit>("observed_digest"));
  CHECK(ui.field<QPlainTextEdit>("expected_digest").toPlainText() ==
        QString::fromStdString(digest(first_asset)));
  CHECK(ui.field<QPlainTextEdit>("observed_digest").toPlainText() ==
        QString::fromStdString(digest(first_asset)));
  CHECK(ui.field<QPushButton>("apply").isEnabled());
  no_horizontal_clipping();
  ui.capture("step-source-native-digest-preview");
  ui.apply();
  REQUIRE(ui.sources().size() == 1);
  CHECK(ui.sources().front().mode() == StepImportMode::EditableBody);
  const auto id = QString::fromStdString(ui.sources().front().id().value());
  ui.command("save");
  ui.begin();
  ui.inspect(id, StepImportFreshness::Available);
  REQUIRE(ui.field<QTableWidget>("catalog").rowCount() == 1);
  CHECK(ui.field<QTableWidget>("catalog").item(0, 0)->data(Qt::UserRole).toString() == id);
  CHECK(ui.field<QTableWidget>("catalog").item(0, 2)->toolTip().contains(relative_asset));
  CHECK(ui.field<QLineEdit>("path").toolTip() == relative_asset);
  CHECK(ui.field<QLineEdit>("name").isReadOnly());
  CHECK(ui.field<QLineEdit>("path").isReadOnly());
  CHECK_FALSE(ui.field<QComboBox>("mode").isEnabled());
  no_horizontal_clipping();
  ui.capture("step-source-native-registry-inspection");
  ui.key(ui.window.viewport(), Qt::Key_Escape);
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK(bytes(asset) == first_asset);
}

TEST_CASE("Embedded STEP source catalog rows cannot apply a prepared Project source operation",
          "[gui][block137-step-import][shell-step-import-catalog]") {
  const bool identical_owner_ids = GENERATE(false, true);
  CAPTURE(identical_owner_ids);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("owned-sources.blcad.project.json");
  const auto root_asset = directory.filePath("root.step");
  const auto child_asset = directory.filePath("child.step");
  write(root_asset, first_asset);
  write(child_asset, second_asset);
  // Separate source registries may contain the same ID. This fixture makes an
  // owner mismatch observable without manufacturing any imported geometry.
  auto project = checked(read_project_json_file(std::string(BLCAD_SOURCE_DIR) +
                                                "/examples/nested_subassembly.blcad.project.json"));
  REQUIRE_FALSE(project.part_documents().empty());
  if (identical_owner_ids) {
    auto rebuilt = checked(
        Project::create(project.part_documents().front().id(), project.name(), project.assembly()));
    for (const auto& assembly : project.child_assembly_documents())
      REQUIRE(rebuilt.add_child_assembly_document(assembly));
    for (const auto& part : project.part_documents())
      REQUIRE(rebuilt.add_part_document(part));
    project = std::move(rebuilt);
    REQUIRE(project.validate_assembly_structure());
  }
  auto& child = project.part_documents().front();
  const auto child_id = child.id();
  CHECK((project.id() == child_id) == identical_owner_ids);
  const auto child_name = QString::fromStdString(child.name());
  const StepImportSourceId shared_id("shared.source");
  REQUIRE(project.add_step_import_source(
      checked(StepImportSource::create(shared_id, "Project supplier", "root.step",
                                       digest(first_asset), StepImportMode::Reference))));
  REQUIRE(child.add_step_import_source(
      checked(StepImportSource::create(shared_id, "Part supplier", "child.step",
                                       digest(second_asset), StepImportMode::EditableBody))));
  REQUIRE(write_project_json_file(project, path.toStdString()));

  SourceUi ui;
  ui.file("open", path);
  const auto before = ui.snapshot();
  ui.begin();
  ui.source(QString::fromStdString(shared_id.value()));
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  CHECK_FALSE(ui.action("undo").isEnabled());
  CHECK_FALSE(ui.action("redo").isEnabled());
  auto& catalog = ui.field<QTableWidget>("catalog");
  REQUIRE(catalog.rowCount() == 2);
  QTableWidgetItem* embedded{};
  QTableWidgetItem* root{};
  for (int row = 0; row < catalog.rowCount(); ++row) {
    auto* item = catalog.item(row, 0);
    REQUIRE(item);
    CHECK(item->data(Qt::UserRole).toString() == QString::fromStdString(shared_id.value()));
    REQUIRE(item->data(Qt::UserRole + 2).isValid());
    if (item->data(Qt::UserRole + 2).toBool()) {
      CHECK(item->data(Qt::UserRole + 1).toString() == QString::fromStdString(child_id.value()));
      embedded = item;
    } else {
      CHECK(item->data(Qt::UserRole + 1).toString() ==
            QString::fromStdString(project.id().value()));
      root = item;
    }
  }
  REQUIRE(embedded);
  REQUIRE(root);
  CHECK_FALSE(embedded->flags().testFlag(Qt::ItemIsSelectable));
  CHECK(root->flags().testFlag(Qt::ItemIsSelectable));
  CHECK(embedded->text().contains(child_name));
  CHECK(embedded->text().contains("Display Only"));
  ui.reveal(catalog);
  catalog.scrollToItem(embedded);
  ui.flush();
  const auto embedded_center = catalog.visualItemRect(embedded).center();
  REQUIRE(catalog.viewport()->rect().contains(embedded_center));
  ui.click_at(*catalog.viewport(), embedded_center);
  CHECK_FALSE(embedded->isSelected());
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.field<QLabel>("summary").text().contains("here only"));
  CHECK(ui.snapshot() == before);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK_FALSE(ui.window.session().can_undo());

  catalog.scrollToItem(root);
  ui.flush();
  ui.click_at(*catalog.viewport(), catalog.visualItemRect(root).center());
  ui.choose(ui.field<QComboBox>("operation"), 2);
  ui.preview();
  ui.apply();
  CHECK(ui.sources().empty());
  REQUIRE(ui.window.session().project()->find_part_document(child_id));
  CHECK(ui.window.session().project()->find_part_document(child_id)->step_import_source_count() ==
        1);
  ui.command("undo");
  CHECK(ui.snapshot() == before);
  CHECK(bytes(root_asset) == first_asset);
  CHECK(bytes(child_asset) == second_asset);
}
