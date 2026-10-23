#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_construction_tools.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
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

// Interaction helpers deliberately stop at Qt widgets and input events.
// The scenarios never create model intent or drive controllers/workbenches directly.
class WorkflowUi {
public:
  WorkflowUi()
      : native_dialogs_disabled_(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs)) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    window.resize(1280, 800);
    window.show();
    flush();
    if (QApplication::platformName() == "xcb")
      REQUIRE(window.viewport().native_viewer_available());
  }
  ~WorkflowUi() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, native_dialogs_disabled_);
  }

  template <typename T> T& widget(const QString& id) {
    auto* found = window.findChild<T*>(id);
    INFO(id.toStdString());
    REQUIRE(found);
    return *found;
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
    for (auto* ancestor = control.parentWidget(); ancestor; ancestor = ancestor->parentWidget())
      if (auto* scroll = qobject_cast<QScrollArea*>(ancestor))
        scroll->ensureWidgetVisible(&control, 8, 8);
    flush();
    INFO(control.objectName().toStdString());
    REQUIRE(control.isVisible());
    const QPoint center = control.mapTo(&window, control.rect().center());
    const auto* hit = window.childAt(center);
    REQUIRE((hit == &control || (hit && control.isAncestorOf(hit))));
  }
  void mouse(QWidget& target, QPointF point, QEvent::Type type,
             Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, point, target.mapToGlobal(point.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      modifiers);
    QApplication::sendEvent(&target, &event);
  }
  void click_at(QWidget& target, QPointF point, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    mouse(target, point, QEvent::MouseButtonPress, modifiers);
    mouse(target, point, QEvent::MouseButtonRelease, modifiers);
    flush();
  }
  void click(QWidget& control) {
    REQUIRE(control.isEnabled());
    reveal(control);
    if (auto* checkbox = qobject_cast<QCheckBox*>(&control)) {
      const bool before = checkbox->isChecked();
      click_at(control, {8, control.height() / 2.0});
      REQUIRE(checkbox->isChecked() != before);
    } else
      click_at(control, control.rect().center());
  }
  void button(const QString& id) {
    click(widget<QPushButton>(id));
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
    key(control, 0, value);
    key(control, Qt::Key_Return);
  }
  void scalar(const QString& value) {
    text(widget<QLineEdit>("blcad.manipulator_numeric_hud"), value);
  }
  void choose(QComboBox& control, int row) {
    REQUIRE(row >= 0);
    REQUIRE(row < control.count());
    reveal(control);
    control.showPopup();
    flush();
    auto* view = control.view();
    const auto index = control.model()->index(row, control.modelColumn());
    view->scrollTo(index);
    flush();
    const auto rectangle = view->visualRect(index);
    REQUIRE(view->viewport()->rect().contains(rectangle.center()));
    click_at(*view->viewport(), rectangle.center());
    REQUIRE(control.currentIndex() == row);
  }
  void data(QComboBox& control, const QVariant& value) {
    choose(control, control.findData(value));
  }
  void list(QListWidget& control, int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(row >= 0);
    REQUIRE(row < control.count());
    reveal(control);
    auto* item = control.item(row);
    control.scrollToItem(item);
    flush();
    click_at(*control.viewport(), control.visualItemRect(item).center(), modifiers);
  }
  void list_data(QListWidget& control, const QString& id) {
    for (int row = 0; row < control.count(); ++row)
      if (control.item(row)->data(Qt::UserRole).toString() == id) {
        list(control, row);
        return;
      }
    FAIL("Missing list input: " + id.toStdString());
  }
  void command(const QString& id, int tab = -1) {
    INFO("Ribbon command: " << id.toStdString());
    auto& selected = action(id);
    REQUIRE(selected.isEnabled());
    if (tab < 0)
      tab = id.startsWith("sketch.") || id == "finish_sketch" ? 1
            : id.startsWith("assembly_") || id.startsWith("export_assembly_") || id == "new_project"
                ? 3
                : 0;
    window.ribbon().set_current_tab(tab);
    flush();
    for (auto* button : window.ribbon().findChildren<QToolButton*>()) {
      if (button->defaultAction() == &selected) {
        click(*button);
        return;
      }
      if (auto* menu = button->menu(); menu && menu->actions().contains(&selected)) {
        reveal(*button);
        // InstantPopup enters a nested Qt loop on mouse press. Activate its
        // menu row with Enter; a watchdog prevents a broken menu from hanging CI.
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
        // reveal() already ran before scheduling the menu input. Do not
        // drain events again until the press has actually opened the popup.
        click_at(*button, button->rect().center());
        watchdog.stop();
        REQUIRE_FALSE(timed_out);
        REQUIRE(chosen);
        return;
      }
    }
    FAIL("No ribbon button for " + id.toStdString());
  }
  template <typename Dialog, typename Respond>
  void dialog(const std::function<void()>& invoke, Respond respond) {
    bool handled = false;
    bool timed_out = false;
    QTimer poll;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
      if (auto* dialog = qobject_cast<Dialog*>(QApplication::activeModalWidget())) {
        poll.stop();
        handled = true;
        respond(*dialog);
      }
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
      timed_out = true;
      if (auto* active = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        active->reject();
    });
    poll.start();
    watchdog.start(5000);
    invoke();
    poll.stop();
    watchdog.stop();
    REQUIRE_FALSE(timed_out);
    REQUIRE(handled);
    flush();
  }
  void new_document(const QString& name, bool project = false) {
    dialog<QInputDialog>([&] { command(project ? "new_project" : "new_part"); },
                         [&](QInputDialog& input) {
                           input.setTextValue(name);
                           input.accept();
                         });
    REQUIRE(window.session().has_document());
  }
  void file(const QString& command_id, const QString& path, bool accept = true) {
    dialog<QFileDialog>([&] { command(command_id); },
                        [&](QFileDialog& input) {
                          if (!accept) {
                            input.reject();
                            return;
                          }
                          input.selectFile(path);
                          QMetaObject::invokeMethod(&input, "accept", Qt::DirectConnection);
                        });
  }
  void tree(const std::string& id, bool activate = false) {
    const auto find = [&](auto&& self, QTreeWidgetItem* item) -> QTreeWidgetItem* {
      if (item->data(0, Qt::UserRole).toString().toStdString() == id)
        return item;
      for (int row = 0; row < item->childCount(); ++row)
        if (auto* found = self(self, item->child(row)))
          return found;
      return nullptr;
    };
    auto& browser = window.model_browser();
    QTreeWidgetItem* item{};
    for (int row = 0; row < browser.topLevelItemCount() && !item; ++row)
      item = find(find, browser.topLevelItem(row));
    INFO(id);
    REQUIRE(item);
    for (auto* parent = item->parent(); parent; parent = parent->parent())
      parent->setExpanded(true);
    browser.scrollToItem(item);
    flush();
    const auto point = browser.visualItemRect(item).center();
    click_at(*browser.viewport(), point);
    if (activate) {
      mouse(*browser.viewport(), point, QEvent::MouseButtonDblClick);
      mouse(*browser.viewport(), point, QEvent::MouseButtonRelease);
      flush();
    }
  }
  void plane_pick(Point2 point) {
    const auto screen = checked(window.viewport().sketch_plane_to_screen(point));
    const QPointF local(screen.x, screen.y);
    REQUIRE(window.viewport().rect().contains(local.toPoint()));
    click_at(window.viewport(), local);
  }
  SketchId rectangle(const std::string& plane, Point2 first, Point2 second) {
    tree(plane);
    command("create_sketch");
    REQUIRE(window.active_sketch());
    const auto id = *window.active_sketch();
    // Numeric geometry assertions use unsnapped coordinates. Toggle the real
    // grid-snap command instead of changing viewport/controller configuration.
    for (auto* button : window.ribbon().findChildren<QToolButton*>())
      if (auto* selected = button->defaultAction();
          selected && selected->text() == "Fang" && selected->isChecked())
        click(*button);
    command("sketch.create.corner_rectangle");
    REQUIRE(window.sketch_tools().tool_active());
    plane_pick(first);
    REQUIRE(window.viewport().sketch_snap_result());
    const auto first_pick = *window.viewport().sketch_snap_result();
    // Native screen coordinates are pixel-quantized. Exercise pick backout
    // and absolute/relative keyboard input for exact authored dimensions.
    key(window, Qt::Key_Escape);
    REQUIRE(window.sketch_tools().tool_active());
    const auto coordinates = [&](Point2 value) {
      const QString entry = QString::number(value.x) + ";" + QString::number(value.y);
      for (const auto character : entry)
        key(window, character.unicode(), QString(character));
      key(window, Qt::Key_Return);
    };
    coordinates(first);
    const auto hover = checked(window.viewport().sketch_plane_to_screen(second));
    QMouseEvent move(QEvent::MouseMove, {hover.x, hover.y},
                     window.viewport().mapToGlobal(QPoint(qRound(hover.x), qRound(hover.y))),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &move);
    flush();
    REQUIRE(window.viewport().sketch_preview_point_count() >= 4);
    coordinates({second.x - first.x, second.y - first.y});
    INFO("first raw=" << first_pick.raw_point.x << "," << first_pick.raw_point.y << " snapped="
                      << first_pick.snapped_point.x << "," << first_pick.snapped_point.y);
    INFO("tool active=" << window.sketch_tools().tool_active());
    REQUIRE(window.session().part_document()->find_sketch(id)->line_segments().size() == 4);
    command("finish_sketch");
    REQUIRE_FALSE(window.active_sketch());
    const auto* sketch = window.session().part_document()->find_sketch(id);
    REQUIRE(sketch);
    INFO("lines=" << sketch->line_segments().size() << ", arcs=" << sketch->arc_segments().size()
                  << ", closed=" << sketch->closed_profiles().size()
                  << ", rectangles=" << sketch->rectangle_profiles().size()
                  << ", arc profiles=" << sketch->arc_closed_profiles().size()
                  << ", composite profiles=" << sketch->composite_closed_profiles().size());
    REQUIRE(sketch->closed_profiles().size() + sketch->rectangle_profiles().size() +
                sketch->arc_closed_profiles().size() + sketch->composite_closed_profiles().size() ==
            1);
    return id;
  }
  BodyId extrude(double depth) {
    command("extrude");
    scalar(QString::number(depth) + " mm");
    button("blcad.feature.apply");
    REQUIRE_FALSE(window.session().task().active());
    return window.session().part_document()->bodies().back().id();
  }
  DatumPlaneId offset(double value) {
    command("create_workplane");
    widget<QDoubleSpinBox>("blcad.construction.offset").setValue(value);
    button("blcad.construction.apply");
    return window.session().part_document()->construction_planes().back().workplane_id();
  }
  std::string snapshot() {
    return window.session().project()
               ? checked(serialize_project_to_json(*window.session().project()))
               : checked(serialize_part_document_to_json(*window.session().part_document()));
  }
  geometry::ShapeSummary summary(const BodyId& id) {
    const auto* shape = window.session().part_shape_cache()->find_body_shape(id);
    REQUIRE(shape);
    return geometry::RectangleExtrusionAdapter{}.summarize(*shape);
  }
  geometry::GeometryShapeBounds bounds(const BodyId& id) {
    const auto* shape = window.session().part_shape_cache()->find_body_shape(id);
    REQUIRE(shape);
    return checked(geometry::BodyTransformAdapter{}.bounds(*shape));
  }
  QByteArray bytes(const QString& path) {
    QFile input(path);
    REQUIRE(input.open(QIODevice::ReadOnly));
    return input.readAll();
  }
  void roundtrip(const QString& path) {
    const auto before = snapshot();
    file("save_as", path);
    const auto saved = bytes(path);
    CHECK_FALSE(window.session().dirty());
    file("open", path);
    CHECK(snapshot() == before);
    CHECK_FALSE(window.session().dirty());
    command("save");
    CHECK(bytes(path) == saved); // Includes the GUI constraint/dimension sidecars.
    CHECK_FALSE(window.session().can_undo());
  }
  void idle() {
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(window.session().derived_results_fresh());
    CHECK(window.viewport().selection_filter_mask() == 0xFFFFFFFFU);
  }
  void capture(const QString& name) {
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (directory.isEmpty())
      return;
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath(name + ".png")));
  }
  ShellWindow window;

private:
  bool native_dialogs_disabled_;
};
} // namespace

TEST_CASE("Combined UI machining workflow edits upstream geometry then patterns transforms "
          "measures and exports",
          "[gui][integration][shell-combined-workflow][shell-combined-part]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Machining workflow");
  const auto plane = ui.offset(10);
  ui.rectangle(plane.value(), {20, 20}, {50, 40});
  const auto base = ui.extrude(6);
  const auto feature = ui.window.session().part_document()->features().front().id();
  CHECK(ui.summary(base).volume_mm3 == Approx(3600).margin(1e-5));
  std::vector<std::string> checkpoints{ui.snapshot()};

  ui.command("fillet");
  auto& targets = ui.widget<QListWidget>("blcad.finishing.targets");
  REQUIRE(targets.count() > 0);
  ui.list(targets, 0);
  ui.scalar("1 mm");
  CHECK(ui.snapshot() == checkpoints.back());
  ui.button("blcad.feature.apply");
  CHECK(ui.window.session().part_document()->fillet_features().size() == 1);
  CHECK(ui.summary(base).volume_mm3 < 3600);
  checkpoints.push_back(ui.snapshot());

  ui.tree(feature.value(), true);
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/length_parameter").setValue(8);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Machined blank");
  CHECK(ui.snapshot() == checkpoints.back());
  ui.button("blcad.feature_edit.apply");
  const auto base_volume = ui.summary(base).volume_mm3;
  CHECK(base_volume > 3600);
  CHECK(base_volume < 4800);
  CHECK(ui.window.session().part_document()->features().front().id() == feature);
  checkpoints.push_back(ui.snapshot());

  ui.command("linear_pattern");
  ui.list_data(ui.widget<QListWidget>("blcad.pattern.sources"),
               QString::fromStdString(base.value()));
  ui.scalar("40 mm");
  ui.widget<QSpinBox>("blcad.pattern.count").setValue(3);
  CHECK(ui.snapshot() == checkpoints.back());
  ui.button("blcad.feature.apply");
  const auto pattern = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(pattern).solid_count == 3);
  CHECK(ui.summary(pattern).volume_mm3 == Approx(3 * base_volume).margin(1e-4));
  const auto bounds = ui.bounds(pattern);
  checkpoints.push_back(ui.snapshot());

  ui.command("body_translate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"),
               QString::fromStdString(pattern.value()));
  ui.scalar("12 mm");
  ui.text(ui.widget<QLineEdit>("blcad.body.translation.1"), "-4 mm");
  ui.text(ui.widget<QLineEdit>("blcad.body.translation.2"), "3 mm");
  ui.button("blcad.feature.apply");
  CHECK(ui.bounds(pattern).minimum.x == Approx(bounds.minimum.x + 12).margin(1e-5));
  CHECK(ui.bounds(pattern).minimum.z == Approx(bounds.minimum.z + 3).margin(1e-5));
  CHECK(ui.summary(pattern).volume_mm3 == Approx(3 * base_volume).margin(1e-4));
  checkpoints.push_back(ui.snapshot());
  ui.idle();

  ui.command("measure_body");
  const auto& measure = ui.window.measure_tools();
  const auto found =
      std::find_if(measure.targets().begin(), measure.targets().end(),
                   [&](const auto& target) { return target.body && *target.body == pattern; });
  REQUIRE(found != measure.targets().end());
  ui.data(ui.widget<QComboBox>("blcad.measure.first"),
          static_cast<int>(found - measure.targets().begin()));
  REQUIRE(measure.result());
  CHECK(measure.result()->volume_mm3 == Approx(3 * base_volume).margin(1e-4));
  CHECK(measure.result()->solid_count == 3);
  ui.button("blcad.measure.copy");
  CHECK_FALSE(QApplication::clipboard()->text().isEmpty());
  ui.button("blcad.measure.close");
  CHECK(ui.snapshot() == checkpoints.back());
  for (std::size_t i = checkpoints.size() - 1; i > 0; --i) {
    ui.command("undo");
    CHECK(ui.snapshot() == checkpoints[i - 1]);
    ui.idle();
  }
  for (std::size_t i = 1; i < checkpoints.size(); ++i) {
    ui.command("redo");
    CHECK(ui.snapshot() == checkpoints[i]);
  }
  ui.roundtrip(directory.filePath("machined.blcad.json"));
  ui.command("fit", 2);
  ui.capture("combined-machining");
  const auto saved = ui.bytes(directory.filePath("machined.blcad.json"));
  ui.file("export_part_step", directory.filePath("machined.step"));
  CHECK(ui.bytes(directory.filePath("machined.step")).startsWith("ISO-10303-21;"));
  CHECK(ui.bytes(directory.filePath("machined.blcad.json")) == saved);
  CHECK(ui.snapshot() == checkpoints.back());
  ui.idle();
}

TEST_CASE("Combined UI Boolean transform and Mirror workflow preserves branching history and "
          "source roles",
          "[gui][integration][shell-combined-workflow][shell-combined-bodies]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  ui.new_document("Boolean workflow");
  ui.rectangle("datum.xy", {20, 20}, {40, 40});
  const auto target = ui.extrude(10);
  ui.rectangle("datum.xy", {30, 20}, {50, 40});
  const auto tool = ui.extrude(10);
  const auto original = ui.snapshot();

  ui.command("body_boolean");
  ui.choose(ui.widget<QComboBox>("blcad.boolean.operation"), 1);
  ui.data(ui.widget<QComboBox>("blcad.boolean.target"), QString::fromStdString(target.value()));
  ui.click(ui.widget<QCheckBox>("blcad.boolean.keep_tools"));
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(tool.value()));
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == original);
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == original);
  ui.idle();

  ui.command("body_boolean");
  ui.choose(ui.widget<QComboBox>("blcad.boolean.operation"), 1);
  ui.data(ui.widget<QComboBox>("blcad.boolean.target"), QString::fromStdString(target.value()));
  auto& keep = ui.widget<QCheckBox>("blcad.boolean.keep_tools");
  if (!keep.isChecked())
    ui.click(keep);
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(tool.value()));
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(target).volume_mm3 == Approx(2000));
  CHECK(ui.summary(tool).volume_mm3 == Approx(4000));
  const auto cut = ui.snapshot();

  ui.command("body_scale");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"),
               QString::fromStdString(target.value()));
  ui.scalar("2");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(target).volume_mm3 == Approx(16000));
  const auto scaled = ui.snapshot();
  ui.command("body_mirror");
  ui.list_data(ui.widget<QListWidget>("blcad.pattern.sources"),
               QString::fromStdString(target.value()));
  ui.button("blcad.feature.apply");
  const auto mirror = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(mirror).volume_mm3 == Approx(16000));
  const auto mirrored = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == scaled);
  ui.command("undo");
  CHECK(ui.snapshot() == cut);
  ui.command("redo");
  CHECK(ui.snapshot() == scaled);
  ui.command("redo");
  CHECK(ui.snapshot() == mirrored);
  ui.command("undo");
  CHECK(ui.window.session().can_redo());

  ui.command("body_translate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(tool.value()));
  ui.scalar("40 mm");
  ui.button("blcad.feature.apply");
  CHECK_FALSE(ui.window.session().can_redo());
  CHECK(ui.window.session().part_document()->mirror_features().empty());
  CHECK(ui.summary(tool).volume_mm3 == Approx(4000));
  CHECK(ui.bounds(tool).minimum.x == Approx(70).margin(1e-5));
  ui.roundtrip(directory.filePath("branched.blcad.json"));
  ui.idle();
}

TEST_CASE(
    "Combined UI Loft workflow authors multiple workplanes reorders sections and edits the result",
    "[gui][integration][shell-combined-workflow][shell-combined-loft]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  ui.new_document("Loft workflow");
  const auto lower = ui.rectangle("datum.xy", {20, 20}, {50, 50});
  const auto middle_plane = ui.offset(10);
  const auto middle = ui.rectangle(middle_plane.value(), {22, 22}, {48, 48});
  const auto upper_plane = ui.offset(20);
  const auto upper = ui.rectangle(upper_plane.value(), {25, 25}, {45, 45});
  const auto before = ui.snapshot();
  ui.command("loft");
  auto& source = ui.widget<QComboBox>("blcad.spatial.source");
  const auto add = [&](const SketchId& sketch) {
    int row = -1;
    for (int i = 0; i < source.count(); ++i)
      if (source.itemText(i).startsWith(
              QString::fromStdString(
                  ui.window.session().part_document()->find_sketch(sketch)->name()) +
              " / "))
        row = i;
    ui.choose(source, row);
    ui.button("blcad.spatial.add");
  };
  add(lower);
  add(upper);
  add(middle);
  auto& order = ui.widget<QListWidget>("blcad.spatial.order");
  REQUIRE(order.count() == 3);
  ui.list(order, 2);
  for (auto* button : ui.window.findChildren<QPushButton*>())
    if (button->isVisible() && button->text() == "Hoch") {
      ui.click(*button);
      break;
    }
  CHECK(order.item(1)->text().startsWith(
      QString::fromStdString(ui.window.session().part_document()->find_sketch(middle)->name())));
  CHECK(ui.snapshot() == before);
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  const auto loft = ui.window.session().part_document()->loft_features().back().id();
  CHECK(ui.summary(body).solid_count == 1);
  CHECK(ui.summary(body).volume_mm3 > 8000);
  CHECK(ui.summary(body).volume_mm3 < 18000);
  // OCCT bounds conservatively include display triangulation deflection.
  // They must enclose both authored end planes on either display backend.
  CHECK(ui.bounds(body).minimum.z <= 1e-5);
  CHECK(ui.bounds(body).maximum.z >= 20 - 1e-5);
  const auto created = ui.snapshot();
  ui.tree(loft.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Three section taper");
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.window.session().part_document()->loft_features().back().id() == loft);
  CHECK(ui.window.session().part_document()->loft_features().back().name() ==
        "Three section taper");
  const auto edited = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == created);
  ui.command("undo");
  CHECK(ui.snapshot() == before);
  ui.command("redo");
  ui.command("redo");
  CHECK(ui.snapshot() == edited);
  ui.roundtrip(directory.filePath("loft.blcad.json"));
  ui.command("fit", 2);
  ui.capture("combined-loft");
  ui.idle();
}

TEST_CASE("Combined UI Assembly workflow imports an authored Part places relates solves analyzes "
          "and exports",
          "[gui][integration][shell-combined-workflow][shell-combined-assembly]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  ui.new_document("Reusable plate");
  ui.rectangle("datum.xy", {20, 20}, {50, 40});
  ui.extrude(4);
  const auto definition = ui.window.session().part_document()->id();
  const auto feature = ui.window.session().part_document()->features().front().id();
  const auto part_path = directory.filePath("plate.blcad.json");
  ui.file("save_as", part_path);
  ui.new_document("Assembly workflow", true);
  ui.file("assembly_import", part_path);
  REQUIRE(ui.window.session().project()->part_documents().size() == 1);
  const auto imported = ui.snapshot();
  std::vector<ComponentInstanceId> instances;
  for (int i = 0; i < 2; ++i) {
    ui.command("assembly_insert_component");
    ui.data(ui.widget<QComboBox>("blcad.assembly.definition"),
            QString::fromStdString(definition.value()));
    ui.widget<QDoubleSpinBox>("blcad.assembly.placement.0").setValue(i * 40);
    ui.widget<QDoubleSpinBox>("blcad.assembly.placement.2").setValue(i * 12);
    if (i == 0)
      ui.click(ui.window.assembly_tools().grounding_editor());
    ui.button("blcad.assembly.apply");
    instances.push_back(
        ui.window.session().project()->assembly().component_instances().back().id());
  }
  const auto placed = ui.snapshot();
  ui.command("assembly_relationship");
  const auto reference = [&](QComboBox& choice, int component, SemanticFace face) {
    const auto& targets = ui.window.assembly_tools().targets();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& item) {
      return item.reference.component_instance() == instances[component] &&
             item.reference.semantic_reference() ==
                 feature.value() + "." + std::string(to_string(face));
    });
    REQUIRE(found != targets.end());
    ui.data(choice, static_cast<int>(found - targets.begin()));
  };
  reference(ui.widget<QComboBox>("blcad.assembly.first"), 0, SemanticFace::Top);
  reference(ui.widget<QComboBox>("blcad.assembly.second"), 1, SemanticFace::Bottom);
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == placed);
  ui.button("blcad.assembly.apply");
  CHECK(ui.window.session().project()->assembly().constraint_count() == 1);
  CHECK(ui.window.session()
            .project()
            ->assembly()
            .find_component_instance(instances[1])
            ->transform()
            .translation_mm.z == Approx(4).margin(1e-5));
  const auto related = ui.snapshot();
  ui.command("assembly_solve");
  ui.button("blcad.assembly.apply");
  CHECK(ui.window.session().project()->assembly().constraint_count() == 1);
  const auto solved = ui.snapshot();

  ui.command("assembly_analysis");
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 1);
  ui.widget<QDoubleSpinBox>("blcad.inspect.clearance").setValue(15);
  ui.button("blcad.inspect.run");
  auto& rows = ui.widget<QTableWidget>("blcad.inspect.results");
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text().startsWith("Abstand: 10"));
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 2);
  CHECK(rows.rowCount() == 0);
  ui.button("blcad.inspect.run");
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text().startsWith("Getrennt:"));
  ui.button("blcad.inspect.close");
  CHECK(ui.snapshot() == solved);
  ui.command("undo");
  CHECK(ui.snapshot() == related);
  ui.command("undo");
  CHECK(ui.snapshot() == placed);
  ui.command("undo");
  ui.command("undo");
  CHECK(ui.snapshot() == imported);
  for (int i = 0; i < 4; ++i)
    ui.command("redo");
  CHECK(ui.snapshot() == solved);
  ui.roundtrip(directory.filePath("assembly.blcad.project.json"));
  for (const auto& [command, name] : std::vector<std::pair<QString, QString>>{
           {"export_assembly_flat_step", "flat.step"},
           {"export_assembly_structured_step", "structured.step"}}) {
    ui.file(command, directory.filePath(name));
    CHECK(ui.bytes(directory.filePath(name)).startsWith("ISO-10303-21;"));
    CHECK(ui.snapshot() == solved);
    CHECK_FALSE(ui.window.session().dirty());
  }
  ui.command("fit", 2);
  ui.capture("combined-assembly");
  ui.idle();
}

TEST_CASE("Combined UI cancellation workflow preserves saved state through invalid tasks dialogs "
          "and document switches",
          "[gui][integration][shell-combined-workflow][shell-combined-cancel]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  ui.new_document("Recovery workflow");
  const auto sketch = ui.rectangle("datum.xy", {20, 20}, {40, 40});
  const auto before = ui.snapshot();
  ui.file("save_as", directory.filePath("sketch.blcad.json"));
  ui.tree("datum.xy");
  ui.tree(sketch.value());
  ui.command("extrude");
  ui.scalar("8 mm");
  CHECK(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.action("create_construction").isEnabled());
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == before);
  CHECK_FALSE(ui.window.session().dirty());
  ui.idle();
  ui.command("create_workplane");
  ui.widget<QDoubleSpinBox>("blcad.construction.offset").setValue(25);
  ui.text(ui.widget<QLineEdit>("blcad.construction.name"), "   ");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.construction.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  ui.text(ui.widget<QLineEdit>("blcad.construction.name"), "Recovered offset");
  CHECK(ui.window.viewport().feature_preview_active());
  ui.key(ui.widget<QLineEdit>("blcad.construction.name"), Qt::Key_Escape);
  CHECK(ui.snapshot() == before);
  CHECK_FALSE(ui.window.session().dirty());
  ui.tree("datum.xy");
  ui.tree(sketch.value());
  ui.extrude(8);
  const auto changed = ui.snapshot();
  ui.file("open", directory.filePath("sketch.blcad.json"), false);
  CHECK(ui.snapshot() == changed);
  CHECK(ui.window.session().dirty());
  ui.dialog<QMessageBox>(
      [&] { ui.file("open", directory.filePath("sketch.blcad.json")); },
      [](QMessageBox& question) { question.button(QMessageBox::Cancel)->click(); });
  CHECK(ui.snapshot() == changed);
  ui.command("undo");
  CHECK(ui.snapshot() == before);
  CHECK_FALSE(ui.window.session().dirty());
  ui.command("redo");
  CHECK(ui.snapshot() == changed);
  ui.roundtrip(directory.filePath("recovered.blcad.json"));
  ui.new_document("Another document");
  CHECK(ui.window.session().part_document()->feature_count() == 0);
  CHECK_FALSE(ui.window.session().task().active());
  ui.file("save_as", directory.filePath("empty.blcad.json"));
  ui.file("open", directory.filePath("recovered.blcad.json"));
  CHECK(ui.snapshot() == changed);
  ui.tree(sketch.value(), true);
  REQUIRE(ui.window.active_sketch());
  ui.command("sketch.create.line");
  ui.plane_pick({45, 10});
  ui.key(ui.window.viewport(), Qt::Key_Escape);
  ui.command("finish_sketch");
  CHECK(ui.snapshot() == changed);
  ui.idle();
}
