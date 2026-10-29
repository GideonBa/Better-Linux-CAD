#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/step_import_source.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/circular_cut_adapter.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/step_reference_commands.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include "gui_step_structured_import_fixture.hpp"

#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QCryptographicHash>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
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
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
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

void write_inspection_geometry(const QString& path, double width = 20) {
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(width)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  shape = checked(geometry::BodyTransformAdapter{}.translate(shape, {1000, 2000, 3000}));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.toStdString()));
}
void write_topology_geometry(const QString& path, double width = 20) {
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(width)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  shape = checked(
      geometry::CircularCutAdapter{}.cut_circular_hole(shape, checked(Quantity::length_mm(4))));
  shape = checked(geometry::BodyTransformAdapter{}.translate(shape, {1000, 2000, 3000}));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.toStdString()));
}

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
    // Keys are dispatched directly below, so establish the focus that a real
    // click gives the editor before Tab commits pending spin-box text.
    control.setFocus(Qt::MouseFocusReason);
    flush();
    key(control, Qt::Key_A, {}, Qt::ControlModifier);
    if (value.isEmpty())
      key(control, Qt::Key_Backspace);
    else
      key(control, 0, value);
    key(control, Qt::Key_Tab);
  }
  void choose(QComboBox& combo, int index) {
    INFO(combo.objectName().toStdString());
    INFO("Requested combo row: " << index);
    REQUIRE(index >= 0);
    REQUIRE(index < combo.count());
    REQUIRE(combo.model());
    const auto requested = combo.model()->index(index, combo.modelColumn(), combo.rootModelIndex());
    REQUIRE(requested.isValid());
    REQUIRE(requested.flags().testFlag(Qt::ItemIsEnabled));
    reveal(combo);
    REQUIRE(combo.isEnabled());
    combo.setFocus();
    key(combo, Qt::Key_Home);
    for (int i = 0; i < combo.count() && combo.currentIndex() != index; ++i)
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
    const int tab =
        id.startsWith("sketch.") || id == "finish_sketch"                                       ? 1
        : id.startsWith("view_") || id == "fit"                                                 ? 2
        : id.startsWith("assembly_") || id.startsWith("export_assembly") || id == "new_project" ? 3
                                                                                                : 0;
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
  void rectangle_in_active(Point2 first, Point2 second) {
    REQUIRE(window.active_sketch());
    for (auto* button : window.ribbon().findChildren<QToolButton*>())
      if (auto* action = button->defaultAction();
          action && action->text() == "Snap" && action->isChecked())
        click(*button);
    command("sketch.create.corner_rectangle");
    const auto coordinates = [&](Point2 point) {
      const auto entry =
          QString::number(point.x, 'g', 17) + ";" + QString::number(point.y, 'g', 17);
      for (const auto character : entry)
        key(window, character.unicode(), QString(character));
      key(window, Qt::Key_Return);
    };
    coordinates(first);
    const auto hover =
        checked(window.viewport().sketch_plane_to_screen({first.x + 20, first.y + 20}));
    QMouseEvent move(QEvent::MouseMove, {hover.x, hover.y},
                     window.viewport().mapToGlobal(QPoint(qRound(hover.x), qRound(hover.y))),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &move);
    flush();
    coordinates({second.x - first.x, second.y - first.y});
    command("finish_sketch");
    REQUIRE_FALSE(window.active_sketch());
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

  using SceneVisibility = std::vector<std::pair<std::string, bool>>;
  SceneVisibility scene_visibility() {
    SceneVisibility result;
    for (const auto& id : window.viewport().scene_semantic_ids())
      result.emplace_back(id, window.viewport().scene_presentation_visible(id));
    return result;
  }
  void check_scene_visibility(const SceneVisibility& expected) {
    CHECK(scene_visibility() == expected);
    for (const auto& [id, visible] : expected) {
      INFO(id);
      CHECK_FALSE(window.viewport().feature_preview_replaces(id));
      CHECK(window.viewport().scene_presentation_visible(id) == visible);
    }
  }
  void check_isolated_preview(const SceneVisibility& scene) {
    REQUIRE(window.viewport().feature_preview_active());
    CHECK(window.viewport().presentation_count() == scene.size());
    for (const auto& item : scene) {
      const auto& id = item.first;
      INFO(id);
      CHECK(window.viewport().feature_preview_replaces(id));
      CHECK_FALSE(window.viewport().scene_presentation_visible(id));
    }
  }
  void check_camera(const GuiViewportCameraBookmark& expected) {
    const auto actual = window.viewport().camera_bookmark();
    const std::array before{expected.eye.x,          expected.eye.y,
                            expected.eye.z,          expected.target.x,
                            expected.target.y,       expected.target.z,
                            expected.up_direction.x, expected.up_direction.y,
                            expected.up_direction.z, expected.scale};
    const std::array after{actual.eye.x,          actual.eye.y,          actual.eye.z,
                           actual.target.x,       actual.target.y,       actual.target.z,
                           actual.up_direction.x, actual.up_direction.y, actual.up_direction.z,
                           actual.scale};
    for (std::size_t index = 0; index < before.size(); ++index)
      CHECK(after[index] == Approx(before[index]).margin(1e-7));
    CHECK(actual.projection == expected.projection);
    CHECK(actual.plane_camera == expected.plane_camera);
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

TEST_CASE("Native STEP geometry inspection previews an empty document without accepting intent",
          "[gui][integration][block138-step-inspection][native-step-inspection]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native STEP preview rendering and task layout");
  const bool project = GENERATE(false, true);
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto document =
      directory.filePath(project ? "empty.blcad.project.json" : "empty.blcad.json");
  const auto asset = directory.filePath("supplier.step");
  write_inspection_geometry(asset);
  const auto original_asset = bytes(asset);
  REQUIRE((project ? ui.window.new_project("Empty inspection")
                   : ui.window.new_part("Empty inspection")));
  ui.file("save_as", document);
  const auto before = ui.snapshot();
  const auto undo = ui.window.session().can_undo();
  const auto redo = ui.window.session().can_redo();
  const auto undo_label = std::string(ui.window.session().undo_label());
  const auto redo_label = std::string(ui.window.session().redo_label());
  const auto normal_presentations = ui.window.viewport().presentation_count();
  ui.begin();
  ui.draft("Supplier geometry", "supplier.step", StepImportMode::Reference);
  const auto camera = ui.window.viewport().camera_bookmark();
  ui.button("inspect_geometry");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.window.viewport().feature_preview_count() == 1);
  CHECK(ui.window.viewport().presentation_count() == normal_presentations);
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.field<QLabel>("geometry_summary").text().contains("1 body definitions"));
  CHECK(ui.field<QLabel>("geometry_units").text().contains(QString::fromUtf8("→ mm")));
  CHECK_FALSE(ui.field<QPlainTextEdit>("geometry_diagnostics").toPlainText().isEmpty());
  CHECK(ui.field<QPlainTextEdit>("geometry_diagnostics").isReadOnly());
  auto& products = ui.field<QTreeWidget>("geometry_products");
  REQUIRE(products.topLevelItemCount() == 2);
  REQUIRE(products.topLevelItem(0)->childCount() == 1);
  CHECK(products.topLevelItem(0)->child(0)->text(1) == "Part / Solid");
  REQUIRE(products.topLevelItem(0)->child(0)->childCount() == 1);
  CHECK(products.topLevelItem(0)->child(0)->child(0)->text(1) == "Solid");
  CHECK_FALSE(products.topLevelItem(0)->child(0)->child(0)->flags() & Qt::ItemIsEditable);
  const auto center = checked(ui.window.viewport().model_to_screen({1000, 2000, 3002.5}));
  CHECK(center.x > 0);
  CHECK(center.x < ui.window.viewport().width());
  CHECK(center.y > 0);
  CHECK(center.y < ui.window.viewport().height());
  const auto left = checked(ui.window.viewport().model_to_screen({990, 2000, 3002.5}));
  const auto right = checked(ui.window.viewport().model_to_screen({1010, 2000, 3002.5}));
  CHECK(std::hypot(right.x - left.x, right.y - left.y) >= 80);
  ui.click_at(ui.window.viewport(), {center.x, center.y});
  CHECK_FALSE(ui.window.viewport().selected_semantic());
  CHECK(ui.window.session().selection().empty());
  CHECK(ui.snapshot() == before);
  CHECK(ui.sources().empty());
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.action("undo").isEnabled() == undo);
  CHECK(ui.action("redo").isEnabled() == redo);
  CHECK(ui.window.step_import_tools().can_undo_after_cancel() == undo);
  CHECK(ui.window.step_import_tools().can_redo_after_cancel() == redo);
  ui.reveal(products);
  CHECK(ui.field<QScrollArea>("scroll").horizontalScrollBar()->maximum() == 0);
  CHECK(products.horizontalScrollBar()->maximum() == 0);
  ui.capture(project ? "step-inspection-empty-project" : "step-inspection-empty-part");
  ui.command("save");
  REQUIRE(ui.window.step_import_tools().active());
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == before);
  ui.text(ui.field<QLineEdit>("name"), "Edited draft");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.field<QTreeWidget>("geometry_products").topLevelItemCount() == 0);
  CHECK(ui.window.viewport().camera_bookmark().scale == Approx(camera.scale).margin(1e-7));
  CHECK(ui.snapshot() == before);
  for (int route = 0; route < 3; ++route) {
    CAPTURE(route);
    if (!ui.window.step_import_tools().active()) {
      ui.begin();
      ui.draft("Supplier geometry", "supplier.step", StepImportMode::Reference);
    }
    ui.button("inspect_geometry");
    REQUIRE(ui.window.viewport().feature_preview_active());
    if (route == 0)
      ui.button("cancel");
    else if (route == 1)
      ui.key(ui.window.viewport(), Qt::Key_Escape);
    else
      ui.field<QDockWidget>("task").close();
    ui.flush();
    CHECK_FALSE(ui.window.step_import_tools().active());
    CHECK_FALSE(ui.window.viewport().feature_preview_active());
    CHECK(ui.window.viewport().presentation_count() == normal_presentations);
    CHECK(ui.snapshot() == before);
    CHECK_FALSE(ui.window.session().dirty());
    CHECK(ui.window.session().can_undo() == undo);
    CHECK(ui.window.session().can_redo() == redo);
    CHECK(ui.window.session().undo_label() == undo_label);
    CHECK(ui.window.session().redo_label() == redo_label);
  }
  CHECK(bytes(asset) == original_asset);
}

TEST_CASE(
    "Native STEP geometry inspection invalidates changed assets and respects explicit refresh",
    "[gui][integration][block138-step-inspection][native-step-inspection]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native STEP preview and filesystem notifications");
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto document = directory.filePath("inspection.blcad.json");
  const auto asset = directory.filePath("supplier.step");
  write_inspection_geometry(asset);
  REQUIRE(ui.window.new_part("Source freshness inspection"));
  QTemporaryDir changed_geometry_directory;
  REQUIRE(changed_geometry_directory.isValid());
  const auto changed_geometry = changed_geometry_directory.filePath("changed.step");
  write_inspection_geometry(changed_geometry, 40);
  const auto changed_bytes = bytes(changed_geometry);
  ui.file("save_as", document);
  ui.begin();
  ui.draft("Supplier", "supplier.step", StepImportMode::EditableBody);
  ui.preview();
  ui.apply();
  const auto source_id = QString::fromStdString(ui.sources().front().id().value());
  ui.command("save");
  const auto registered = ui.snapshot();
  const auto undo_label = std::string(ui.window.session().undo_label());
  ui.begin();
  ui.source(source_id);
  const auto camera = ui.window.viewport().camera_bookmark();
  ui.button("inspect_geometry");
  REQUIRE(ui.window.viewport().feature_preview_active());
  const auto accepted_bytes = bytes(asset);
  {
    QSaveFile replacement(asset);
    REQUIRE(replacement.open(QIODevice::WriteOnly));
    REQUIRE(replacement.write(accepted_bytes) == accepted_bytes.size());
    REQUIRE(replacement.commit());
  }
  // Finish the atomic-replacement notifications before modifying the replacement inode.
  // A pending directoryChanged event must not mask a lost file watch in the next step.
  ui.flush();
  QEventLoop replacement_events;
  QTimer::singleShot(25, &replacement_events, &QEventLoop::quit);
  replacement_events.exec();
  ui.flush();
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.field<QLabel>("status").property("freshness").toInt() ==
        static_cast<int>(StepImportFreshness::Available));
  CHECK(bytes(asset) == accepted_bytes);
  CHECK(ui.snapshot() == registered);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.action("undo").isEnabled());
  CHECK(ui.window.step_import_tools().can_undo_after_cancel());
  write(asset, changed_bytes);
  ui.flush();
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.field<QLabel>("status").property("freshness").toInt() ==
        static_cast<int>(StepImportFreshness::Changed));
  CHECK(ui.field<QTreeWidget>("geometry_products").topLevelItemCount() == 0);
  CHECK(ui.window.viewport().camera_bookmark().scale == Approx(camera.scale).margin(1e-7));
  CHECK(ui.snapshot() == registered);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.action("undo").isEnabled());
  CHECK(ui.window.step_import_tools().can_undo_after_cancel());
  ui.button("inspect_geometry");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.field<QLabel>("summary").text().contains("digest"));
  CHECK(ui.snapshot() == registered);
  ui.capture("step-inspection-changed-source-rejected");
  ui.button("cancel");
  CHECK(ui.window.session().undo_label() == undo_label);
  ui.begin();
  ui.source(source_id);
  ui.preview();
  ui.apply();
  const auto refreshed = ui.snapshot();
  REQUIRE(refreshed != registered);
  ui.command("save");
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.reveal(ui.field<QTreeWidget>("geometry_products"));
  ui.capture("step-inspection-explicit-refresh");
  const auto current_asset = bytes(asset);
  ui.command("undo");
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == registered);
  ui.command("redo");
  CHECK(ui.snapshot() == refreshed);
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  REQUIRE(ui.window.viewport().feature_preview_active());
  const auto moved = directory.filePath("renamed.blcad.json");
  ui.file("save_as", moved);
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == refreshed);
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.file("open", moved);
  CHECK_FALSE(ui.window.step_import_tools().active());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == refreshed);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(bytes(asset) == current_asset);
}

TEST_CASE(
    "Native Reference Parts insert shared nested occurrences, constrain, drive, refresh and "
    "convert",
    "[gui][integration][gui-step-import][block140-reference-parts][native-step-reference-parts]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT Reference Part presentation and Assembly interaction");
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto asset = directory.filePath("supplier.step");
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  shape = checked(
      geometry::CircularCutAdapter{}.cut_circular_hole(shape, checked(Quantity::length_mm(4))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, asset.toStdString()));
  const auto source_bytes = bytes(asset);
  ui.dialog<QInputDialog>([&] { ui.command("new_project"); },
                          [&](QInputDialog& dialog) {
                            dialog.setTextValue("Reference Assembly");
                            dialog.accept();
                          });
  const auto document = directory.filePath("reference.blcad.project.json");
  ui.file("save_as", document);
  ui.begin();
  ui.draft("Supplier", "supplier.step", StepImportMode::Reference);
  ui.preview();
  ui.apply();
  const auto source_id = QString::fromStdString(ui.sources().front().id().value());
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  ui.button("topology_prepare");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  ui.apply();
  const auto captured = ui.snapshot();
  const auto registered_project = *ui.window.session().project();
  ui.begin();
  ui.source(source_id);
  ui.text(ui.field<QLineEdit>("reference_name"), "Supplier Part");
  ui.button("reference_prepare");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.window.viewport().feature_preview_count() == 1);
  CHECK(ui.snapshot() == captured);
  ui.capture("step-reference-first-preview");
  ui.apply();
  REQUIRE(ui.window.session().project()->part_document_count() == 1);
  const auto reference_id = ui.window.session().project()->part_documents().front().id();
  REQUIRE(ui.window.session().project()->part_documents().front().is_reference_part());
  const auto first = ui.window.session().project()->assembly().component_instances().front().id();
  const auto inserted = ui.snapshot();
  const auto& imported_part = ui.window.session().project()->part_documents().front();
  const auto& first_occurrence =
      ui.window.session().project()->assembly().component_instances().front();
  const auto direct_reference = checked(geometry::StepReferenceCommands{}.insert(
      registered_project, imported_part.reference_part_definition()->catalog(), imported_part.id(),
      imported_part.name(), ui.window.session().project()->assembly().id(), first,
      first_occurrence.transform(), directory.path().toStdString()));
  CHECK(inserted == checked(serialize_project_to_json(direct_reference.project)));
  ui.command("undo");
  CHECK(ui.snapshot() == captured);
  ui.command("redo");
  CHECK(ui.snapshot() == inserted);
  // Ground the first occurrence through the existing actual placement controls.
  ui.command("assembly_placement");
  auto& assembly_tools = ui.window.assembly_tools();
  ui.choose(assembly_tools.occurrence_editor(),
            assembly_tools.occurrence_editor().findData(QStringLiteral("component:") +
                                                        QString::fromStdString(first.value())));
  auto& grounding = assembly_tools.grounding_editor();
  ui.reveal(grounding);
  grounding.setFocus();
  ui.key(grounding, Qt::Key_Space);
  REQUIRE(grounding.isChecked());
  ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
  REQUIRE(
      ui.window.session().project()->assembly().component_instances().front().grounding_state() ==
      ComponentGroundingState::Grounded);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 1);
  ui.text(ui.field<QLineEdit>("reference_x"), "40");
  const auto root_scene = ui.scene_visibility();
  REQUIRE_FALSE(root_scene.empty());
  REQUIRE(std::any_of(root_scene.begin(), root_scene.end(),
                      [](const auto& item) { return item.second; }));
  ui.button("reference_prepare");
  // Root-local placement previews retain the existing Assembly context.
  ui.check_scene_visibility(root_scene);
  ui.apply();
  REQUIRE(ui.window.session().project()->part_document_count() == 1);
  REQUIRE(ui.window.session().project()->assembly().component_instance_count() == 2);
  const auto second = ui.window.session().project()->assembly().component_instances().back().id();
  // A real child definition is loaded through the existing UI, then receives
  // another occurrence of the same immutable owned Part.
  auto child = checked(Project::create(
      DocumentId("project.child.fixture"), "Child Fixture",
      checked(AssemblyDocument::create(DocumentId("assembly.child.fixture"), "Child Assembly"))));
  const auto child_path = directory.filePath("child.blcad.project.json");
  REQUIRE(write_project_json_file(child, child_path.toStdString()));
  ui.file("assembly_import", child_path);
  REQUIRE(ui.window.session().project()->child_assembly_document_count() == 1);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 1);
  ui.choose(ui.field<QComboBox>("reference_assembly"), 1);
  ui.text(ui.field<QLineEdit>("reference_x"), "0");
  ui.text(ui.field<QLineEdit>("reference_y"), "30");
  const auto parent_scene = ui.scene_visibility();
  ui.button("reference_prepare");
  // A child definition has no unique root occurrence pose yet.
  ui.check_isolated_preview(parent_scene);
  ui.apply();
  ui.check_scene_visibility(parent_scene);
  REQUIRE(ui.window.session()
              .project()
              ->child_assembly_documents()
              .front()
              .component_instance_count() == 1);
  ui.command("assembly_insert_subassembly");
  ui.choose(assembly_tools.definition_editor(), 1);
  ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
  REQUIRE(ui.window.session().project()->assembly().subassembly_instance_count() == 1);
  CHECK(ui.window.session().project()->part_document_count() == 1);
  ui.command("fit");
  ui.capture("step-reference-repeated-nested");
  const auto repeated = ui.snapshot();
  const auto select_target = [&](QComboBox& combo, ComponentInstanceId component,
                                 std::optional<ImportedTopologyCapability> capability,
                                 const std::string& exact = {}) {
    for (std::size_t i = 0; i < assembly_tools.targets().size(); ++i) {
      const auto& target = assembly_tools.targets()[i];
      if (target.reference.component_instance() != component)
        continue;
      const auto& semantic = target.reference.semantic_reference();
      bool matches = semantic == exact;
      if (capability && is_imported_topology_target_spelling(semantic)) {
        const auto reference = checked(parse_imported_topology_target_spelling(semantic));
        matches = reference.capability() == *capability;
      }
      if (!matches)
        continue;
      const auto index = combo.findData(static_cast<int>(i));
      if (index < 0)
        continue;
      ui.choose(combo, index);
      return;
    }
    FAIL("Missing compatible Reference Part target");
  };
  // Imported analytic capability filters remain authoritative. Both are
  // ordinary relationship transactions, followed by real Undo to isolate the joint proof.
  for (const auto& [type, capability] : {std::pair{6, ImportedTopologyCapability::Plane},
                                         std::pair{1, ImportedTopologyCapability::Axis}}) {
    ui.command("assembly_relationship");
    ui.choose(assembly_tools.relationship_editor(), type);
    select_target(assembly_tools.first_editor(), first, capability);
    select_target(assembly_tools.second_editor(), second, capability);
    INFO(ui.widget<QLabel>("blcad.assembly.summary").text().toStdString());
    REQUIRE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
    CHECK(ui.snapshot() == repeated);
    ui.capture(type == 6 ? "step-reference-plane-relationship"
                         : "step-reference-axis-relationship");
    ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
    CHECK(ui.window.session().project()->assembly().constraint_count() == 1);
    ui.command("undo");
    CHECK(ui.snapshot() == repeated);
  }
  ui.command("assembly_joint");
  ui.choose(assembly_tools.joint_type_editor(), 4);
  select_target(assembly_tools.first_editor(), first, ImportedTopologyCapability::Point);
  ui.choose(assembly_tools.pick_editor(), 1);
  REQUIRE(assembly_tools.second_editor().count() > 1);
  ui.command("view_isometric");
  ui.command("fit");
  auto& viewport = ui.window.viewport();
  ui.window.raise();
  ui.window.activateWindow();
  QEventLoop painted;
  QTimer::singleShot(50, &painted, &QEventLoop::quit);
  painted.exec();
  const auto eye = viewport.camera_bookmark().eye;
  std::vector<std::size_t> point_targets;
  for (int row = 1; row < assembly_tools.second_editor().count(); ++row) {
    const auto index =
        static_cast<std::size_t>(assembly_tools.second_editor().itemData(row).toInt());
    const auto& candidate = assembly_tools.targets().at(index);
    if (!is_imported_topology_target_spelling(candidate.reference.semantic_reference()))
      continue;
    const auto reference =
        checked(parse_imported_topology_target_spelling(candidate.reference.semantic_reference()));
    // Spherical also accepts derived center Points from circular targets, whose
    // exact presentation is a rim. This proof clicks an actual imported vertex.
    if (reference.capability() != ImportedTopologyCapability::Point ||
        candidate.resolved.source_kind !=
            geometry::AssemblyGeometricTargetSourceKind::ImportedVertex)
      continue;
    REQUIRE(candidate.reference.component_instance() == second);
    REQUIRE(reference.capability() == ImportedTopologyCapability::Point);
    point_targets.push_back(index);
  }
  REQUIRE_FALSE(point_targets.empty());
  // Native depth picking cannot select an arbitrary back vertex through the
  // body. Try actual visible Point candidates, nearest to the camera first.
  const auto distance_to_eye = [&](std::size_t index) {
    const auto point = assembly_tools.targets().at(index).anchor;
    return std::pow(point.x - eye.x, 2) + std::pow(point.y - eye.y, 2) +
           std::pow(point.z - eye.z, 2);
  };
  std::stable_sort(point_targets.begin(), point_targets.end(), [&](auto left, auto right) {
    return distance_to_eye(left) < distance_to_eye(right);
  });
  std::string attempts;
  std::string mouse_selected_reference;
  for (const auto index : point_targets) {
    const auto candidate = assembly_tools.targets().at(index);
    const auto screen = checked(viewport.model_to_screen(candidate.anchor));
    REQUIRE(screen.x >= 0);
    REQUIRE(screen.x < viewport.width());
    REQUIRE(screen.y >= 0);
    REQUIRE(screen.y < viewport.height());
    attempts += candidate.reference.semantic_reference() + " at model(" +
                std::to_string(candidate.anchor.x) + "," + std::to_string(candidate.anchor.y) +
                "," + std::to_string(candidate.anchor.z) + ") screen(" + std::to_string(screen.x) +
                "," + std::to_string(screen.y) + ")\n";
    ui.capture("step-reference-point-pick-before");
    const QPoint point(qRound(screen.x), qRound(screen.y));
    QCursor::setPos(viewport.mapToGlobal(point));
    QMouseEvent move(QEvent::MouseMove, {screen.x, screen.y}, viewport.mapToGlobal(point),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &move);
    ui.click_at(viewport, {screen.x, screen.y});
    if (assembly_tools.second_editor().currentIndex() <= 0)
      continue;
    mouse_selected_reference = candidate.reference.semantic_reference();
    break;
  }
  INFO("Native Point attempts:\n" << attempts);
  INFO("Pick mask: " << viewport.selection_filter_mask()
                     << ", presentations: " << viewport.presentation_count() << ", scope: "
                     << (viewport.selection_semantic_scope()
                             ? viewport.selection_semantic_scope()->size()
                             : 0));
  REQUIRE(assembly_tools.second_editor().currentIndex() > 0);
  const auto& picked =
      assembly_tools.targets().at(assembly_tools.second_editor().currentData().toInt());
  CHECK(picked.reference.component_instance() == second);
  CHECK(picked.reference.semantic_reference() == mouse_selected_reference);
  CHECK(checked(parse_imported_topology_target_spelling(picked.reference.semantic_reference()))
            .capability() == ImportedTopologyCapability::Point);
  ui.capture("step-reference-point-picked");
  REQUIRE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
  REQUIRE(ui.window.session().project()->assembly().joints().size() == 1);
  CHECK(ui.window.session().project()->assembly().joints().front().coordinate_slots().empty());
  ui.command("undo");
  CHECK(ui.snapshot() == repeated);
  // Revolute motion uses an explicit immutable BLCAD origin frame. Imported
  // circles/axes retain their Block-139 capability set without a guessed Frame.
  ui.command("assembly_joint");
  ui.choose(assembly_tools.joint_type_editor(), 0);
  const auto origin_frame = checked(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
  select_target(assembly_tools.first_editor(), first, std::nullopt, origin_frame);
  select_target(assembly_tools.second_editor(), second, std::nullopt, origin_frame);
  INFO(ui.widget<QLabel>("blcad.assembly.summary").text().toStdString());
  REQUIRE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
  REQUIRE(ui.window.session().project()->assembly().joints().size() == 1);
  const auto joint = ui.window.session().project()->assembly().joints().front().id();
  const auto connected = ui.snapshot();
  ui.command("assembly_motion");
  ui.choose(assembly_tools.joint_editor(),
            assembly_tools.joint_editor().findData(QString::fromStdString(joint.value())));
  auto* motion_input = assembly_tools.value_editor().findChild<QLineEdit*>();
  REQUIRE(motion_input);
  ui.text(*motion_input, "25");
  REQUIRE(assembly_tools.value_editor().value() == Approx(25));
  INFO(ui.widget<QLabel>("blcad.assembly.summary").text().toStdString());
  REQUIRE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  CHECK(ui.snapshot() == connected);
  ui.capture("step-reference-revolute-motion");
  ui.click(ui.widget<QPushButton>("blcad.assembly.apply"));
  const auto moved = ui.snapshot();
  CHECK(moved != connected);
  const auto* driven_joint = ui.window.session().project()->assembly().find_joint(joint);
  REQUIRE(driven_joint);
  CHECK(driven_joint->type() == AssemblyJointType::Revolute);
  CHECK(driven_joint->coordinate_deg() == Approx(25));
  ui.command("undo");
  CHECK(ui.snapshot() == connected);
  ui.command("redo");
  CHECK(ui.snapshot() == moved);
  ui.command("save");
  // A changed accepted source invalidates the disposable preview; explicit
  // shared refresh accepts only the bytes shown in its newly prepared candidate.
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 2);
  const auto motion_scene = ui.scene_visibility();
  REQUIRE_FALSE(motion_scene.empty());
  const auto motion_camera = ui.window.viewport().camera_bookmark();
  ui.button("reference_prepare");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  ui.check_isolated_preview(motion_scene);
  ui.button("cancel");
  ui.check_scene_visibility(motion_scene);
  ui.check_camera(motion_camera);
  CHECK(ui.snapshot() == moved);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 2);
  ui.button("reference_prepare");
  auto revised = source_bytes;
  revised.insert(revised.indexOf("ENDSEC;"), "/* revision B */\n");
  write(asset, revised);
  ui.flush();
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == moved);
  ui.button("reference_prepare");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.field<QPlainTextEdit>("geometry_diagnostics").toPlainText().contains("Affected owner:"));
  ui.capture("step-reference-shared-refresh");
  ui.apply();
  const auto refreshed = ui.snapshot();
  CHECK(refreshed != moved);
  CHECK(bytes(asset) == revised);
  ui.command("undo");
  CHECK(ui.snapshot() == moved);
  ui.command("redo");
  CHECK(ui.snapshot() == refreshed);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 3);
  ui.text(ui.field<QLineEdit>("reference_name"), "Editable Copy");
  const auto refreshed_scene = ui.scene_visibility();
  REQUIRE(std::any_of(refreshed_scene.begin(), refreshed_scene.end(),
                      [](const auto& item) { return item.second; }));
  const auto refreshed_camera = ui.window.viewport().camera_bookmark();
  ui.button("reference_prepare");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  ui.check_isolated_preview(refreshed_scene);
  CHECK(ui.snapshot() == refreshed);
  ui.capture("step-reference-convert-new-editable");
  ui.button("cancel");
  ui.check_scene_visibility(refreshed_scene);
  ui.check_camera(refreshed_camera);
  CHECK(ui.snapshot() == refreshed);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 3);
  ui.text(ui.field<QLineEdit>("reference_name"), "Editable Copy");
  const auto conversion_camera = ui.window.viewport().camera_bookmark();
  ui.button("reference_prepare");
  ui.check_isolated_preview(refreshed_scene);
  ui.apply();
  // Conversion adds an uninstantiated definition; all original occurrences
  // and their presentation/camera return unchanged after the preview ends.
  ui.check_scene_visibility(refreshed_scene);
  ui.check_camera(conversion_camera);
  REQUIRE(ui.window.session().project()->part_document_count() == 2);
  CHECK(ui.window.session().project()->find_part_document(reference_id)->is_reference_part());
  const auto& editable = ui.window.session().project()->part_documents().back();
  CHECK_FALSE(editable.is_reference_part());
  CHECK(editable.imported_body_features().size() == 1);
  for (const auto& component : ui.window.session().project()->assembly().component_instances())
    CHECK(component.referenced_part_document() == reference_id);
  CHECK(ui.window.session()
            .project()
            ->child_assembly_documents()
            .front()
            .component_instances()
            .front()
            .referenced_part_document() == reference_id);
  ui.roundtrip(document);
  structured_test::check_portable_json(ui.snapshot());
  const auto accepted_project = ui.snapshot();
  const auto accepted_history = std::string(ui.window.session().undo_label());
  const auto expected_world = structured_test::project_shapes(*ui.window.session().project(),
                                                              directory.path().toStdString());
  REQUIRE(expected_world.size() == 3);
  const GuiAnalysisExportWorkbench analysis;
  const auto interference = checked(analysis.analyze_interference(ui.window.session()));
  const auto clearance = checked(analysis.analyze_clearance(ui.window.session()));
  const auto contact = checked(analysis.analyze_contact(ui.window.session()));
  ui.command("assembly_analysis");
  for (int kind = 0; kind < 3; ++kind) {
    ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), kind);
    ui.click(ui.widget<QPushButton>("blcad.inspect.run"));
    const std::array expected_rows{interference.interferences.size(),
                                   clearance.interferences.size() +
                                       clearance.clearance_violations.size(),
                                   contact.records.size()};
    INFO(ui.widget<QLabel>("blcad.inspect.summary").text().toStdString());
    CHECK(ui.widget<QLabel>("blcad.inspect.summary").text().contains("3 occurrences"));
    CHECK(ui.widget<QTableWidget>("blcad.inspect.results").rowCount() ==
          static_cast<int>(expected_rows[kind]));
    CHECK(ui.snapshot() == accepted_project);
  }
  ui.capture("step-reference-integrated-analysis");
  ui.click(ui.widget<QPushButton>("blcad.inspect.close"));
  CHECK(ui.window.session().undo_label() == accepted_history);
  const auto flat = directory.filePath("reference-refreshed-flat.step");
  const auto structured = directory.filePath("reference-refreshed-structured.step");
  ui.file("export_assembly_flat_step", flat);
  ui.file("export_assembly_structured_step", structured);
  structured_test::check_export(flat.toStdString(), expected_world);
  structured_test::check_export(structured.toStdString(), expected_world);
  CHECK(ui.snapshot() == accepted_project);
  CHECK(bytes(asset) == revised);
}

TEST_CASE(
    "Native standalone Reference and Editable copies preserve the source document and asset root",
    "[gui][integration][block140-reference-parts][native-step-reference-parts]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT standalone Reference presentation");
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.dialog<QInputDialog>([&] { ui.command("new_part"); },
                          [&](QInputDialog& dialog) {
                            dialog.setTextValue("Reference Source");
                            dialog.accept();
                          });
  const auto original_path = directory.filePath("original.blcad.json");
  ui.file("save_as", original_path);
  const auto asset = directory.filePath("supplier.step");
  write_inspection_geometry(asset);
  const auto asset_bytes = bytes(asset);
  ui.begin();
  ui.draft("Supplier", "supplier.step", StepImportMode::Reference);
  ui.preview();
  ui.apply();
  const auto source_id = QString::fromStdString(ui.sources().front().id().value());
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  ui.button("topology_prepare");
  ui.apply();
  ui.command("save");
  const auto original = ui.snapshot();
  const auto original_file = bytes(original_path);
  ui.begin();
  ui.source(source_id);
  ui.text(ui.field<QLineEdit>("reference_name"), "Standalone Reference");
  const auto original_scene = ui.scene_visibility();
  REQUIRE(std::any_of(original_scene.begin(), original_scene.end(),
                      [](const auto& item) { return item.second; }));
  const auto original_camera = ui.window.viewport().camera_bookmark();
  ui.button("reference_prepare");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  ui.check_isolated_preview(original_scene);
  CHECK(ui.snapshot() == original);
  ui.capture("step-reference-standalone-create");
  const auto reference_path = directory.filePath("reference.blcad.json");
  ui.dialog<QFileDialog>([&] { ui.apply(); },
                         [&](QFileDialog& dialog) {
                           dialog.selectFile(reference_path);
                           QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
                         });
  ui.check_scene_visibility(original_scene);
  ui.check_camera(original_camera);
  CHECK(ui.snapshot() == original);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(bytes(original_path) == original_file);
  ui.file("open", reference_path);
  REQUIRE(ui.window.session().part_document()->is_reference_part());
  REQUIRE(ui.window.session().part_shape_cache());
  CHECK(ui.window.session().part_shape_cache()->reference_result());
  CHECK(ui.window.session().part_shape_cache()->feature_shapes().empty());
  const auto reference = ui.snapshot();
  ui.tree("datum.xy");
  CHECK_FALSE(ui.action("create_sketch").isEnabled());
  CHECK_FALSE(ui.action("extrude").isEnabled());
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 3);
  ui.text(ui.field<QLineEdit>("reference_name"), "Standalone Editable Copy");
  const auto camera = ui.window.viewport().camera_bookmark();
  const auto reference_scene = ui.scene_visibility();
  REQUIRE(std::any_of(reference_scene.begin(), reference_scene.end(),
                      [](const auto& item) { return item.second; }));
  ui.button("reference_prepare");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  REQUIRE(ui.window.viewport().feature_preview_count() == 1);
  ui.check_isolated_preview(reference_scene);
  const auto a = checked(ui.window.viewport().model_to_screen({990, 1995, 3005}));
  const auto b = checked(ui.window.viewport().model_to_screen({1010, 1995, 3005}));
  CHECK(std::hypot(a.x - b.x, a.y - b.y) >= 80);
  ui.capture("step-reference-standalone-convert");
  ui.button("cancel");
  ui.check_scene_visibility(reference_scene);
  ui.check_camera(camera);
  CHECK(ui.snapshot() == reference);
  CHECK_FALSE(ui.window.session().dirty());
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 3);
  ui.button("reference_prepare");
  ui.check_isolated_preview(reference_scene);
  const auto editable_path = directory.filePath("editable.blcad.json");
  ui.dialog<QFileDialog>([&] { ui.apply(); },
                         [&](QFileDialog& dialog) {
                           dialog.selectFile(editable_path);
                           QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
                         });
  ui.check_scene_visibility(reference_scene);
  CHECK(ui.snapshot() == reference);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK_FALSE(ui.window.session().can_undo());
  CHECK(bytes(original_path) == original_file);
  CHECK(bytes(asset) == asset_bytes);
  ui.file("open", editable_path);
  CHECK_FALSE(ui.window.session().part_document()->is_reference_part());
  REQUIRE(ui.window.session().part_document()->imported_body_features().size() == 1);
  REQUIRE(ui.window.session().part_shape_cache());
  CHECK(ui.window.session().part_shape_cache()->feature_shape_count() == 1);
  CHECK(ui.window.session().part_shape_cache()->body_shape_count() == 1);
  CHECK_FALSE(ui.window.session().part_shape_cache()->reference_result());
  ui.roundtrip(editable_path);
}

TEST_CASE(
    "Native direct EditableBody imports support face sketches, modeling, refresh and "
    "protected STEP export",
    "[gui][integration][gui-step-import][block141-editable-parts][native-step-editable-parts]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT imported-base picking and Part authoring");
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto asset = directory.filePath("editable.step");
  auto supplier = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  REQUIRE(geometry::StepExporter{}.write_step(supplier, asset.toStdString()));
  const auto supplier_bytes = bytes(asset);
  ui.dialog<QInputDialog>([&] { ui.command("new_part"); },
                          [&](QInputDialog& dialog) {
                            dialog.setTextValue("Editable supplier registry");
                            dialog.accept();
                          });
  const auto registry = directory.filePath("registry.blcad.json");
  ui.file("save_as", registry);
  ui.begin();
  ui.draft("Editable supplier", "editable.step", StepImportMode::EditableBody);
  ui.preview();
  ui.apply();
  const auto source_id = QString::fromStdString(ui.sources().front().id().value());
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  ui.button("topology_prepare");
  ui.apply();
  ui.command("save");
  const auto registry_json = ui.snapshot();
  ui.begin();
  ui.source(source_id);
  ui.text(ui.field<QLineEdit>("reference_name"), "Editable imported plate");
  ui.button("reference_prepare");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == registry_json);
  ui.capture("step-editable-direct-preview");
  const auto document = directory.filePath("editable.blcad.json");
  ui.dialog<QFileDialog>([&] { ui.apply(); },
                         [&](QFileDialog& dialog) {
                           dialog.selectFile(document);
                           QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
                         });
  CHECK(ui.snapshot() == registry_json);
  ui.file("open", document);
  REQUIRE_FALSE(ui.window.session().part_document()->is_reference_part());
  REQUIRE(ui.window.session().part_document()->imported_body_features().size() == 1);
  const auto base_intent = ui.window.session().part_document()->imported_body_features().front();
  const auto base = base_intent.id();
  const auto body = ui.window.session()
                        .part_document()
                        ->imported_body_features()
                        .front()
                        .definition()
                        .outputs()
                        .front()
                        .body;
  const auto imported_intent =
      checked(serialize_part_document_to_json(*ui.window.session().part_document()));
  const auto& initial_part = *ui.window.session().part_document();
  const auto direct_editable = checked(geometry::StepEditableCommands{}.create_definition(
      *initial_part.find_step_import_source(base_intent.definition().source()),
      *initial_part.find_imported_topology_catalog(base_intent.definition().catalog()),
      initial_part.id(), initial_part.name(), base, directory.path().toStdString()));
  CHECK(imported_intent == checked(serialize_part_document_to_json(direct_editable)));
  structured_test::check_portable_json(imported_intent);
  const auto body_volume = [&] {
    const auto* shape = ui.window.session().part_shape_cache()->find_body_shape(body);
    REQUIRE(shape);
    return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
  };
  CHECK(body_volume() == Approx(1000).margin(1e-5));
  // Select the real imported top face with the mouse, then create an ordinary
  // face-supported Sketch carrying the true imported feature's provenance.
  ui.choose(ui.widget<QComboBox>("blcad.selection.filter"), 3);
  ui.command("view_top");
  geometry::StepImportTopologyContext face_context(directory.path().toStdString());
  const auto faces = checked(geometry::ResultFaceSupportGeometry::selectable_faces(
      *ui.window.session().part_document(), *ui.window.session().part_shape_cache(), body,
      &face_context));
  const auto top = std::find_if(faces.begin(), faces.end(), [](const auto& target) {
    return std::abs(target.anchor.z - 5) < 1e-7;
  });
  REQUIRE(top != faces.end());
  const auto screen = checked(ui.window.viewport().model_to_screen(top->anchor));
  REQUIRE(screen.x > 0);
  REQUIRE(screen.x < ui.window.viewport().width());
  REQUIRE(screen.y > 0);
  REQUIRE(screen.y < ui.window.viewport().height());
  QCursor::setPos(ui.window.viewport().mapToGlobal(QPoint(qRound(screen.x), qRound(screen.y))));
  QMouseEvent face_move(
      QEvent::MouseMove, {screen.x, screen.y},
      ui.window.viewport().mapToGlobal(QPoint(qRound(screen.x), qRound(screen.y))), Qt::NoButton,
      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&ui.window.viewport(), &face_move);
  ui.click_at(ui.window.viewport(), {screen.x, screen.y});
  REQUIRE(ui.window.session().selection().size() == 1);
  CHECK(ui.window.session().selection().entries().front().kind == GuiSelectionKind::Face);
  ui.command("create_sketch");
  REQUIRE(ui.window.active_sketch());
  REQUIRE(ui.window.session().part_document()->result_face_workplanes().size() == 1);
  const auto& support = ui.window.session().part_document()->result_face_workplanes().front();
  CHECK(support.reference().producer() == base);
  REQUIRE_FALSE(support.reference().origins().empty());
  CHECK(support.reference().origins().front().feature == base);
  CHECK(support.reference().origins().front().role.starts_with("imported-face/"));
  ui.rectangle_in_active({-2, -2}, {2, 2});
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 1);
  ui.text(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud"), "3 mm");
  ui.key(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud"), Qt::Key_Return);
  const auto boss_parameter =
      ui.window.feature_tools().coordinator().extrude().distance_parameter_id();
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("step-editable-imported-face-boss");
  ui.click(ui.widget<QPushButton>("blcad.feature.apply"));
  CHECK(body_volume() == Approx(1048).margin(1e-5));
  const auto boss_json = ui.snapshot();
  ui.command("undo");
  CHECK(body_volume() == Approx(1000).margin(1e-5));
  ui.command("redo");
  CHECK(ui.snapshot() == boss_json);
  ui.choose(ui.widget<QComboBox>("blcad.selection.filter"), 0);
  ui.tree("datum.xy");
  ui.command("create_sketch");
  ui.rectangle_in_active({5, -2}, {7, 2});
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  auto& extent = ui.widget<QComboBox>("blcad.extrude.extent");
  ui.choose(extent, extent.findData(static_cast<int>(ExtrudeExtentMode::ThroughAll)));
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.click(ui.widget<QPushButton>("blcad.feature.apply"));
  CHECK(body_volume() == Approx(1008).margin(1e-5));
  ui.tree(body.value());
  ui.command("hole");
  ui.text(ui.widget<QLineEdit>("blcad.hole.diameter"), "2");
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "-5");
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_y"), "0");
  ui.click(ui.widget<QPushButton>("blcad.hole.preview"));
  REQUIRE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  ui.click(ui.widget<QPushButton>("blcad.hole.apply"));
  const auto drilled = body_volume();
  CHECK(drilled == Approx(1008 - 5 * std::acos(-1.0)).margin(1e-5));
  ui.command("fillet");
  auto& edges = ui.window.feature_tools().finishing_target_editor();
  REQUIRE(edges.count() > 0);
  REQUIRE(edges.item(0)->text().contains("Imported edge"));
  ui.reveal(edges);
  edges.scrollToItem(edges.item(0));
  ui.flush();
  ui.click_at(*edges.viewport(), edges.visualItemRect(edges.item(0)).center());
  ui.text(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud"), "0.3 mm");
  ui.key(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud"), Qt::Key_Return);
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("step-editable-downstream-fillet");
  ui.click(ui.widget<QPushButton>("blcad.feature.apply"));
  REQUIRE(ui.window.session().part_document()->fillet_features().size() == 1);
  CHECK(ui.window.session()
            .part_document()
            ->fillet_features()
            .front()
            .edges()
            .front()
            .source_kind() == PartFeatureInputSourceKind::ImportedTopology);
  const auto rounded = body_volume();
  CHECK(rounded < drilled);
  ui.command("parameters");
  auto& parameters = ui.widget<QComboBox>("blcad.parameters.parameter");
  ui.choose(parameters, parameters.findData(QString::fromStdString(boss_parameter.value())));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "4");
  ui.click(ui.widget<QPushButton>("blcad.parameters.stage"));
  ui.click(ui.widget<QPushButton>("blcad.parameters.preview"));
  {
    INFO(ui.widget<QLabel>("blcad.parameters.summary").text().toStdString());
    REQUIRE(ui.widget<QPushButton>("blcad.parameters.apply").isEnabled());
  }
  ui.click(ui.widget<QPushButton>("blcad.parameters.apply"));
  CHECK(body_volume() == Approx(rounded + 16).margin(1e-5));
  CHECK(ui.window.session().part_document()->imported_body_features().front().id() == base);
  CHECK(ui.window.session().part_document()->imported_body_features().front() == base_intent);
  const auto* base_shape =
      ui.window.session().part_shape_cache()->find_imported_body_shape(base, body);
  REQUIRE(base_shape);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*base_shape).volume_mm3 == Approx(1000));
  const auto modeled = ui.snapshot();
  ui.command("undo");
  CHECK(body_volume() == Approx(rounded).margin(1e-5));
  ui.command("redo");
  CHECK(ui.snapshot() == modeled);
  ui.roundtrip(document);
  const auto modeled_volume = body_volume();
  const auto exported = directory.filePath("modeled.step");
  ui.file("export_part_step", exported);
  CHECK(bytes(exported).contains("ISO-10303-21"));
  const auto* modeled_shape = ui.window.session().part_shape_cache()->find_body_shape(body);
  REQUIRE(modeled_shape);
  structured_test::check_export(exported.toStdString(), {*modeled_shape});
  const auto readback =
      structured_test::physical_bodies(structured_test::read_export(exported.toStdString()));
  REQUIRE(readback.size() == 1);
  CHECK(readback.front().volume == Approx(modeled_volume).margin(1e-4));
  const std::array expected_bounds{-10.0, -5.0, 0.0, 10.0, 5.0, 9.0};
  for (std::size_t axis = 0; axis < expected_bounds.size(); ++axis)
    CHECK(readback.front().vertex_bounds[axis] == Approx(expected_bounds[axis]).margin(1e-5));
  structured_test::check_portable_json(modeled);
  CHECK(ui.snapshot() == modeled);
  CHECK(bytes(asset) == supplier_bytes);
  // Explicit refresh previews the complete modeled result, accepts only its
  // inspected bytes and preserves authored base/consumer identities.
  auto revised = supplier_bytes;
  revised.insert(revised.indexOf("ENDSEC;"), "/* Editable supplier revision B */\n");
  write(asset, revised);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 2);
  ui.button("reference_prepare");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == modeled);
  ui.capture("step-editable-modeled-refresh");
  ui.apply();
  CHECK(body_volume() == Approx(modeled_volume).margin(1e-5));
  const auto refreshed = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == modeled);
  ui.command("redo");
  CHECK(ui.snapshot() == refreshed);
  ui.roundtrip(document);
  const auto refreshed_export = directory.filePath("modeled-refreshed.step");
  ui.file("export_part_step", refreshed_export);
  const auto* refreshed_shape = ui.window.session().part_shape_cache()->find_body_shape(body);
  REQUIRE(refreshed_shape);
  structured_test::check_export(refreshed_export.toStdString(), {*refreshed_shape});
  CHECK(bytes(asset) == revised);
  // A changed top face breaks the real face-supported sketch; accepting the
  // new digest alone cannot publish a partially recomputed document.
  supplier = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(8))));
  REQUIRE(geometry::StepExporter{}.write_step(supplier, asset.toStdString()));
  const auto incompatible_bytes = bytes(asset);
  ui.begin();
  ui.choose(ui.field<QComboBox>("reference_operation"), 2);
  ui.button("reference_prepare");
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == refreshed);
  CHECK(bytes(asset) == incompatible_bytes);
  ui.capture("step-editable-downstream-refresh-rejected");
  ui.button("cancel");
  CHECK(imported_intent != refreshed);
}

TEST_CASE(
    "Native imported topology catalogs persist stable references and pick exact analytic targets",
    "[gui][integration][block139-step-topology][native-step-topology]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires real OCCT imported topology picking");
  const bool project = GENERATE(false, true);
  CAPTURE(project);
  SourceUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto document =
      directory.filePath(project ? "topology.blcad.project.json" : "topology.blcad.json");
  const auto asset = directory.filePath("supplier.step");
  write_topology_geometry(asset);
  const auto original_asset = bytes(asset);
  REQUIRE((project ? ui.window.new_project("Topology inspection")
                   : ui.window.new_part("Topology inspection")));
  ui.file("save_as", document);
  ui.begin();
  ui.draft("Supplier plate", "supplier.step", StepImportMode::Reference);
  ui.preview();
  ui.apply();
  ui.command("save");
  const auto source_id = QString::fromStdString(ui.sources().front().id().value());
  const auto registered = ui.snapshot();
  auto& idle_filter = ui.widget<QComboBox>("blcad.selection.filter");
  ui.choose(idle_filter, idle_filter.findData(selection_kind_bit(GuiSelectionKind::Face)));
  const auto original_mask = ui.window.viewport().selection_filter_mask();
  const auto original_scope = ui.window.viewport().selection_semantic_scope();
  ui.begin();
  ui.source(source_id);
  ui.button("inspect_geometry");
  REQUIRE(ui.field<QComboBox>("topology_product").count() == 1);
  ui.button("topology_prepare");
  INFO(ui.field<QLabel>("summary").text().toStdString());
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  REQUIRE(ui.window.viewport().preview_selection_target_count() > 20);
  CHECK(ui.snapshot() == registered);
  CHECK_FALSE(ui.window.session().dirty());
  auto& entries = ui.field<QTreeWidget>("topology_entries");
  REQUIRE(entries.topLevelItemCount() > 20);
  for (int i = 0; i < entries.topLevelItemCount(); ++i)
    CHECK(entries.topLevelItem(i)->text(2) == "resolved");
  const auto pick = [&](ImportedTopologyCapability capability, Point3 point,
                        GuiSelectionKind kind) {
    INFO("Pick capability: " << to_string(capability)
                             << ", expected kind: " << static_cast<int>(kind));
    auto& filter = ui.field<QComboBox>("topology_filter");
    ui.choose(filter, filter.findData(static_cast<int>(capability)));
    const auto left = checked(ui.window.viewport().model_to_screen({990, 2000, 3002.5}));
    const auto right = checked(ui.window.viewport().model_to_screen({1010, 2000, 3002.5}));
    REQUIRE(std::hypot(right.x - left.x, right.y - left.y) >= 80);
    const auto screen = checked(ui.window.viewport().model_to_screen(point));
    REQUIRE(screen.x > 3);
    REQUIRE(screen.x < ui.window.viewport().width() - 3);
    REQUIRE(screen.y > 3);
    REQUIRE(screen.y < ui.window.viewport().height() - 3);
    ui.window.raise();
    ui.window.activateWindow();
    QCursor::setPos(ui.window.viewport().mapToGlobal(QPoint{qRound(screen.x), qRound(screen.y)}));
    QMouseEvent move(QEvent::MouseMove, {screen.x, screen.y},
                     ui.window.viewport().mapToGlobal(QPoint{qRound(screen.x), qRound(screen.y)}),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&ui.window.viewport(), &move);
    ui.click_at(ui.window.viewport(), {screen.x, screen.y});
    const auto selected = ui.window.viewport().selected_semantic();
    REQUIRE(selected);
    CHECK(selected->kind == kind);
    const auto spelling =
        ui.field<QPlainTextEdit>("topology_reference").toPlainText().toStdString();
    REQUIRE_FALSE(spelling.empty());
    CHECK(spelling == selected->semantic_id);
    const auto reference = checked(parse_imported_topology_target_spelling(spelling));
    CHECK(reference.capability() == capability);
    CHECK(ui.snapshot() == registered);
    return reference;
  };
  ui.command("view_top");
  const auto face =
      pick(ImportedTopologyCapability::Plane, {1006, 2002, 3005}, GuiSelectionKind::Face);
  const auto edge =
      pick(ImportedTopologyCapability::Line, {1005, 1995, 3005}, GuiSelectionKind::Edge);
  const auto vertex =
      pick(ImportedTopologyCapability::Point, {990, 1995, 3005}, GuiSelectionKind::Vertex);
  const auto circle =
      pick(ImportedTopologyCapability::Circle, {1002, 2000, 3005}, GuiSelectionKind::Edge);
  ui.command("view_isometric");
  const auto cylinder =
      pick(ImportedTopologyCapability::Cylinder, {1000, 2002, 3003.5}, GuiSelectionKind::Face);
  const auto axis =
      pick(ImportedTopologyCapability::Axis, {1000, 2002, 3003.5}, GuiSelectionKind::Face);
  ui.reveal(entries);
  CHECK(ui.field<QScrollArea>("scroll").horizontalScrollBar()->maximum() == 0);
  INFO("Topology table viewport " << entries.viewport()->width() << ", header length "
                                  << entries.header()->length() << ", section widths "
                                  << entries.columnWidth(0) << ',' << entries.columnWidth(1) << ','
                                  << entries.columnWidth(2) << ',' << entries.columnWidth(3));
  CHECK(entries.horizontalScrollBar()->maximum() == 0);
  ui.capture(project ? "step-topology-project-picking" : "step-topology-part-picking");
  ui.apply();
  const auto accepted = ui.snapshot();
  CHECK(accepted != registered);
  CHECK(ui.window.session().undo_label() == "Capture imported topology catalog");
  CHECK(ui.window.viewport().preview_selection_target_count() == 0);
  CHECK(ui.window.viewport().selection_filter_mask() == original_mask);
  CHECK(ui.window.viewport().selection_semantic_scope() == original_scope);
  const auto catalog =
      project ? ui.window.session().project()->imported_topology_catalogs().front()
              : ui.window.session().part_document()->imported_topology_catalogs().front();
  for (const auto& reference : {face, edge, vertex, circle, cylinder, axis}) {
    CHECK(reference.catalog() == catalog.id());
    CHECK(catalog.find(reference.entry()));
  }
  ui.command("undo");
  CHECK(ui.snapshot() == registered);
  ui.command("redo");
  CHECK(ui.snapshot() == accepted);
  ui.roundtrip(document);
  // Opening a document resets the idle filter to All; each later task must
  // restore that current choice rather than the pre-Open Faces choice.
  const auto reopened_mask = ui.window.viewport().selection_filter_mask();
  const auto reopened_scope = ui.window.viewport().selection_semantic_scope();
  CHECK(reopened_mask == 0xFFFFFFFFU);
  ui.begin();
  ui.source(source_id);
  REQUIRE_FALSE(ui.field<QComboBox>("topology_catalog").currentData().toString().isEmpty());
  ui.button("topology_prepare");
  REQUIRE(ui.window.viewport().preview_selection_target_count() > 0);
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  ui.command("view_top");
  auto& filter = ui.field<QComboBox>("topology_filter");
  ui.choose(filter, filter.findData(static_cast<int>(ImportedTopologyCapability::Plane)));
  const auto screen = checked(ui.window.viewport().model_to_screen({1006, 2002, 3005}));
  ui.click_at(ui.window.viewport(), {screen.x, screen.y});
  REQUIRE(ui.window.viewport().selected_semantic());
  CHECK(checked(parse_imported_topology_target_spelling(
            ui.window.viewport().selected_semantic()->semantic_id)) == face);
  CHECK(ui.snapshot() == accepted);
  write_topology_geometry(asset, 40);
  const auto changed_asset = bytes(asset);
  ui.flush();
  CHECK(ui.window.viewport().preview_selection_target_count() == 0);
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == accepted);
  ui.preview();
  ui.apply();
  const auto refreshed = ui.snapshot();
  CHECK(refreshed != accepted);
  ui.begin();
  ui.source(source_id);
  ui.button("topology_prepare");
  auto& recovered = ui.field<QTreeWidget>("topology_entries");
  ui.choose(filter, filter.findData(-1));
  bool lost = false;
  for (int i = 0; i < recovered.topLevelItemCount(); ++i)
    if (recovered.topLevelItem(i)->text(2) != "resolved") {
      lost = true;
      const auto id = recovered.topLevelItem(i)->data(0, Qt::UserRole).toString();
      const auto* entry = catalog.find(ImportedTopologyId(id.toStdString()));
      REQUIRE(entry);
      const auto capability =
          entry->descriptor.kind == ImportedTopologyKind::Body   ? ImportedTopologyCapability::Body
          : entry->descriptor.kind == ImportedTopologyKind::Face ? ImportedTopologyCapability::Face
          : entry->descriptor.kind == ImportedTopologyKind::Edge
              ? ImportedTopologyCapability::Edge
              : ImportedTopologyCapability::Point;
      const auto reference =
          checked(ImportedTopologyReference::create(catalog.id(), entry->id, capability));
      CHECK_FALSE(ui.window.viewport().select_semantic(
          checked(make_imported_topology_target_spelling(reference))));
    }
  CHECK(lost);
  const auto* unchanged =
      project ? ui.window.session().project()->find_imported_topology_catalog(catalog.id())
              : ui.window.session().part_document()->find_imported_topology_catalog(catalog.id());
  REQUIRE(unchanged);
  CHECK(*unchanged == catalog);
  ui.reveal(recovered);
  ui.capture(project ? "step-topology-project-recovery" : "step-topology-part-recovery");
  ui.key(ui.window.viewport(), Qt::Key_Escape);
  CHECK(ui.window.viewport().preview_selection_target_count() == 0);
  CHECK(ui.window.viewport().selection_filter_mask() == reopened_mask);
  CHECK(ui.window.viewport().selection_semantic_scope() == reopened_scope);
  CHECK(ui.snapshot() == refreshed);
  CHECK(changed_asset != original_asset);
  CHECK(bytes(asset) == changed_asset);
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
