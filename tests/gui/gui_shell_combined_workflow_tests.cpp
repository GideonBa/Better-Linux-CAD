#include "../support/loft_alignment_fixture.hpp"
#include "../support/loft_section_refinement_fixture.hpp"
#include "../support/mixed_path_loft_fixture.hpp"
#include "../support/rotor_performance_fixture.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_construction_tools.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_parameter_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <optional>
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
// Combined workflows author their inputs through the UI. The Planar
// motion case imports the repository's seated Part definition through a file
// dialog; the separately tagged mixed-path case opens preauthored Sketch/planes.
// Neither path drives authoring controllers/workbenches directly.
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
  void fit_first_screen() {
    if (QApplication::platformName() != "xcb")
      return;
    auto* screen = QGuiApplication::screenAt(QPoint(0, 0));
    REQUIRE(screen);
    const auto available = screen->availableGeometry().adjusted(12, 12, -12, -12);
    window.move(available.topLeft());
    window.resize(std::min(1280, available.width()), std::min(800, available.height()));
    window.raise();
    window.activateWindow();
    flush();
  }
  bool wait_for_ui(const std::function<bool()>& ready) {
    if (!ready()) {
      QEventLoop transition;
      QTimer poll;
      QTimer deadline;
      deadline.setSingleShot(true);
      QObject::connect(&poll, &QTimer::timeout, &transition, [&] {
        if (ready())
          transition.quit();
      });
      QObject::connect(&deadline, &QTimer::timeout, &transition, &QEventLoop::quit);
      poll.start(5);
      deadline.start(2000);
      transition.exec();
    }
    return ready();
  }
  void reveal(QWidget& control) {
    flush();
    for (auto* ancestor = control.parentWidget(); ancestor; ancestor = ancestor->parentWidget())
      if (auto* scroll = qobject_cast<QScrollArea*>(ancestor)) {
        // An editable combo can remain partly clipped after a task narrows
        // the ribbon, even after ensureWidgetVisible.
        // Scroll the actual control bounds before enforcing the hit check.
        const auto center = control.mapTo(scroll->widget(), control.rect().center());
        scroll->ensureVisible(center.x(), center.y(),
                              std::min(control.width() / 2 + 8, scroll->viewport()->width() / 2),
                              std::min(control.height() / 2 + 8, scroll->viewport()->height() / 2));
      }
    flush();
    INFO(control.objectName().toStdString());
    REQUIRE(control.isVisible());
    const QPoint center = control.mapTo(&window, control.rect().center());
    const auto* hit = window.childAt(center);
    INFO("Control center: " << center.x() << "," << center.y());
    INFO("Hit widget: " << (hit ? hit->metaObject()->className() : "none"));
    INFO("Hit identity: " << (hit ? hit->objectName().toStdString() : "none"));
    INFO("Visible region: " << control.visibleRegion().boundingRect().width() << "x"
                            << control.visibleRegion().boundingRect().height());
    if (!(hit == &control || (hit && control.isAncestorOf(hit))) &&
        qEnvironmentVariableIsSet("BLCAD_UI_CAPTURE_DIR"))
      capture("control-not-visible");
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
    INFO(control.objectName().toStdString());
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
    if (value.isEmpty())
      key(control, Qt::Key_Backspace);
    else
      key(control, 0, value);
    key(control, Qt::Key_Return);
  }
  void scalar(const QString& value) {
    text(widget<QLineEdit>("blcad.manipulator_numeric_hud"), value);
  }
  void number(QAbstractSpinBox& control, const QString& value) {
    auto* editor = control.findChild<QLineEdit*>();
    REQUIRE(editor);
    text(*editor, value);
  }
  void cell(QTableWidget& control, int row, int column, const QString& value) {
    auto* item = control.item(row, column);
    REQUIRE(item);
    reveal(control);
    control.scrollToItem(item);
    flush();
    const auto rectangle = control.visualItemRect(item);
    REQUIRE(control.viewport()->rect().contains(rectangle.center()));
    click_at(*control.viewport(), rectangle.center());
    key(control, Qt::Key_F2);
    auto* editor =
        qobject_cast<QLineEdit*>(control.indexWidget(control.model()->index(row, column)));
    REQUIRE(editor);
    REQUIRE(control.isAncestorOf(editor));
    text(*editor, value);
    CHECK(control.item(row, column)->text() == value);
  }
  void choose(QComboBox& control, int row) {
    INFO(control.objectName().toStdString());
    INFO("Popup row: " << row);
    REQUIRE(row >= 0);
    REQUIRE(row < control.count());
    reveal(control);
    REQUIRE(control.isEnabled());
    const bool native = QApplication::platformName() == "xcb";
    if (native) {
      // Synthetic Qt clicks do not activate the desktop window like a real
      // mouse click. Establish native focus before asking it to grab a popup.
      window.raise();
      window.activateWindow();
      QGuiApplication::sync();
      flush();
      REQUIRE(wait_for_ui([&] { return window.isActiveWindow(); }));
      QCursor::setPos(control.mapToGlobal(control.rect().center()));
      QGuiApplication::sync();
      flush();
    }
    control.showPopup();
    if (native)
      QGuiApplication::sync();
    flush();
    auto* view = control.view();
    INFO("Main window active: " << window.isActiveWindow());
    INFO("Combo visible: " << control.isVisible());
    INFO("Combo animation: " << QApplication::isEffectEnabled(Qt::UI_AnimateCombo));
    REQUIRE(wait_for_ui([&] { return view->isVisible(); }));
    const auto index = control.model()->index(row, control.modelColumn());
    view->scrollTo(index, QAbstractItemView::PositionAtCenter);
    flush();
    const auto rectangle = view->visualRect(index);
    REQUIRE(view->viewport()->rect().contains(rectangle.center()));
    const auto global = view->viewport()->mapToGlobal(rectangle.center());
    bool pointer_ready = !native;
    int pointer_warps = 0;
    int stable_ticks = 0;
    if (native) {
      // Native popup tracking also consults the desktop pointer. Keep it at
      // the same row as the synthetic events. Popup activation can restore an
      // earlier pointer position, and its queued MotionNotify can replace a
      // synthetic hover. Require both to agree across separate event-loop ticks.
      QEventLoop transition;
      QTimer tick;
      QTimer deadline;
      tick.setSingleShot(true); // Event draining below must not reenter this tick.
      deadline.setSingleShot(true);
      bool timed_out = false;
      QObject::connect(&deadline, &QTimer::timeout, &transition, [&] {
        timed_out = true;
        transition.quit();
      });
      QObject::connect(&tick, &QTimer::timeout, &transition, [&] {
        const auto before = QCursor::pos();
        if (before != global && pointer_warps < 4) {
          ++pointer_warps;
          QCursor::setPos(global);
        }
        QGuiApplication::sync();
        flush();
        if (timed_out)
          return;
        QMouseEvent move(QEvent::MouseMove, rectangle.center(), global, Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QGuiApplication::sync();
        flush();
        if (timed_out)
          return;
        const auto actual = QCursor::pos();
        const bool settled = actual == global && view->isVisible() &&
                             view->viewport()->rect().contains(rectangle.center()) &&
                             view->indexAt(rectangle.center()) == index &&
                             view->currentIndex() == index;
        stable_ticks = settled ? stable_ticks + 1 : 0;
        if (stable_ticks >= 2) {
          pointer_ready = true;
          transition.quit();
        } else {
          tick.start(5);
        }
      });
      deadline.start(2000);
      tick.start(0);
      transition.exec();
    }
    const auto actual = QCursor::pos();
    const auto popup_geometry = view->window()->geometry();
    const auto screen_geometry = view->screen()->geometry();
    const auto available_geometry = view->screen()->availableGeometry();
    INFO("Requested cursor: " << global.x() << ',' << global.y());
    INFO("Actual cursor: " << actual.x() << ',' << actual.y());
    INFO("Pointer warp requests: " << pointer_warps);
    INFO("Stable cursor/hover ticks: " << stable_ticks);
    const auto current_global = view->viewport()->mapToGlobal(rectangle.center());
    INFO("Current row global position: " << current_global.x() << ',' << current_global.y());
    INFO("Current hover row: " << view->currentIndex().row());
    INFO("Screen DPR: " << view->screen()->devicePixelRatio());
    INFO("Popup geometry: " << popup_geometry.x() << ',' << popup_geometry.y() << ' '
                            << popup_geometry.width() << 'x' << popup_geometry.height());
    INFO("Screen geometry: " << screen_geometry.x() << ',' << screen_geometry.y() << ' '
                             << screen_geometry.width() << 'x' << screen_geometry.height());
    INFO("Available geometry: " << available_geometry.x() << ',' << available_geometry.y() << ' '
                                << available_geometry.width() << 'x'
                                << available_geometry.height());
    if (!pointer_ready && qEnvironmentVariableIsSet("BLCAD_UI_CAPTURE_DIR")) {
      capture("assembly-popup-pointer-mismatch");
      const auto path = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR") +
                        QStringLiteral("/assembly-popup-pointer-mismatch-popup.png");
      REQUIRE(view->window()->grab().save(path));
    }
    if (native) {
      REQUIRE(pointer_ready);
      REQUIRE(actual == global);
    }
    REQUIRE(view->isVisible());
    REQUIRE(view->indexAt(rectangle.center()) == index);
    // A real popup selection first moves over the scrolled row. Publish the
    // hover before clicking so native menu selection follows the visible item.
    if (!native) {
      QMouseEvent move(QEvent::MouseMove, rectangle.center(),
                       view->viewport()->mapToGlobal(rectangle.center()), Qt::NoButton,
                       Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(view->viewport(), &move);
      flush();
    }
    REQUIRE(view->currentIndex() == index);
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
  SketchId rectangle(const std::string& plane, Point2 first, Point2 second,
                     std::optional<Point2> preview_point = std::nullopt) {
    INFO("Rectangle on " << plane << ": " << first.x << "," << first.y << " to " << second.x << ","
                         << second.y);
    tree(plane);
    command("create_sketch");
    REQUIRE(window.active_sketch());
    const auto id = *window.active_sketch();
    // Numeric geometry assertions use unsnapped coordinates. Toggle the real
    // grid-snap command instead of changing viewport/controller configuration.
    for (auto* button : window.ribbon().findChildren<QToolButton*>())
      if (auto* selected = button->defaultAction();
          selected && selected->text() == "Snap" && selected->isChecked())
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
    // Small final dimensions can fall inside axis-inference tolerance. Allow
    // a wider pointer preview before the exact numeric refinement below.
    const auto hover =
        checked(window.viewport().sketch_plane_to_screen(preview_point.value_or(second)));
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
  SketchId open_line(const std::string& plane, Point2 first, Point2 second) {
    tree(plane);
    command("create_sketch");
    REQUIRE(window.active_sketch());
    const auto id = *window.active_sketch();
    command("sketch.create.line");
    const auto coordinates = [&](Point2 point) {
      const auto entry = QString::number(point.x) + ";" + QString::number(point.y);
      for (const auto character : entry)
        key(window, character.unicode(), QString(character));
      key(window, Qt::Key_Return);
    };
    coordinates(first);
    coordinates({second.x - first.x, second.y - first.y});
    command("finish_sketch");
    REQUIRE_FALSE(window.active_sketch());
    const auto* sketch = window.session().part_document()->find_sketch(id);
    REQUIRE(sketch);
    REQUIRE(sketch->line_segments().size() == 1);
    REQUIRE(sketch->profile_count() == 0);
    REQUIRE(action("extrude").isEnabled());
    CHECK_FALSE(action("revolve").isEnabled());
    return id;
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
  void history(const std::vector<std::string>& states) {
    REQUIRE(states.size() > 1);
    for (std::size_t i = states.size() - 1; i > 0; --i) {
      command("undo");
      CHECK(snapshot() == states[i - 1]);
      idle();
    }
    for (std::size_t i = 1; i < states.size(); ++i) {
      command("redo");
      CHECK(snapshot() == states[i]);
      idle();
    }
  }
  void standard_view(const QString& label) {
    window.ribbon().set_current_tab(2);
    flush();
    for (auto* button : window.ribbon().findChildren<QToolButton*>())
      if (button->defaultAction() && button->defaultAction()->text() == label) {
        click(*button);
        return;
      }
    FAIL("No standard view button: " + label.toStdString());
  }
  void limit_face(const FeatureId& source, const char* face, Point3 point) {
    INFO("Limit source: " << source.value() << "/" << face);
    const auto expected = "construction-source/" + std::to_string(source.value().size()) + ":" +
                          source.value() + "/face/" + face;
    if (QApplication::platformName() == "xcb") {
      flush();
      const auto screen = checked(window.viewport().model_to_screen(point));
      const QPoint local(qRound(screen.x), qRound(screen.y));
      REQUIRE(window.viewport().rect().contains(local));
      click_at(window.viewport(), local);
    } else {
      const auto role = widget<QComboBox>("blcad.extrude.pick_role").currentIndex();
      REQUIRE(role > 0);
      data(widget<QComboBox>(role == 1 ? "blcad.extrude.first_face" : "blcad.extrude.second_face"),
           QString::fromStdString(source.value() + "/" + face));
    }
    REQUIRE(window.viewport().selected_semantic());
    CHECK(window.viewport().selected_semantic()->semantic_id == expected);
    CHECK(window.feature_tools().manipulators().active_handle_id().empty());
  }
  void idle() {
    CHECK_FALSE(window.session().task().active());
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK(window.session().derived_results_fresh());
    CHECK(window.viewport().selection_filter_mask() == 0xFFFFFFFFU);
    CHECK_FALSE(window.viewport().selection_semantic_scope());
    CHECK(window.feature_tools().extrude_selection_items().empty());
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

TEST_CASE("Hermes desktop authors dimensions previews and edits a solid through history and reopen",
          "[gui][integration][shell-combined-workflow][hermes-design-workflow]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Instrument bracket");
  // Use available desktop space when proving the same workflow at higher DPI.
  const QSize available = QApplication::platformName() == QStringLiteral("xcb")
                              ? ui.window.screen()->availableGeometry().size() - QSize(32, 48)
                              : QSize(1920, 1080);
  const QSize ordinary = QSize(1280, 800).boundedTo(available);
  ui.window.resize(ordinary);
  ui.flush();
  const auto sketch = ui.rectangle("datum.xy", {20, 20}, {50, 40});
  ui.tree(sketch.value(), true);
  REQUIRE(ui.window.active_sketch() == sketch);

  // Reach the dimension group through the actual horizontally scrolling ribbon.
  ui.command("sketch.dimension.rapid");
  REQUIRE(ui.window.sketch_tools().rapid_dimension_active());
  const auto hover = [&](Point2 point) {
    const auto screen = checked(ui.window.viewport().sketch_plane_to_screen(point));
    const QPointF local(screen.x, screen.y);
    REQUIRE(ui.window.viewport().rect().contains(local.toPoint()));
    QMouseEvent move(QEvent::MouseMove, local, ui.window.viewport().mapToGlobal(local.toPoint()),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&ui.window.viewport(), &move);
    ui.flush();
  };
  hover({35, 20});
  REQUIRE(ui.window.sketch_tools().rapid_dimension_candidate());
  ui.plane_pick({35, 20});
  hover({35, 8});
  REQUIRE(ui.window.sketch_tools().rapid_dimension_candidate());
  ui.plane_pick({35, 8});
  const auto dimensions = checked(ui.window.session().sketch_dimension_catalog(sketch));
  REQUIRE(dimensions.dimensions().size() == 1U);
  CHECK(dimensions.dimensions().front().kind() == SketchDimensionKind::Length);
  REQUIRE(dimensions.dimensions().front().parameter());
  const auto* width = ui.window.session().part_document()->find_parameter(
      *dimensions.dimensions().front().parameter());
  REQUIRE(width);
  CHECK(width->value().millimeters() == Approx(30));
  ui.key(ui.window, Qt::Key_Escape);
  CHECK_FALSE(ui.window.sketch_tools().rapid_dimension_active());
  ui.capture("hermes-sketch");
  ui.command("finish_sketch");
  REQUIRE_FALSE(ui.window.active_sketch());
  std::vector<std::string> states{ui.snapshot()};
  const auto undo_before_preview = std::string(ui.window.session().undo_label());

  ui.command("extrude");
  ui.scalar("8 mm");
  REQUIRE(ui.window.viewport().feature_preview_active());
  auto& apply = ui.widget<QPushButton>("blcad.feature.apply");
  auto& cancel = ui.widget<QPushButton>("blcad.feature.cancel");
  REQUIRE(apply.isEnabled());
  REQUIRE(cancel.isEnabled());
  ui.reveal(apply);
  ui.reveal(cancel);
  CHECK(ui.snapshot() == states.back());
  ui.capture("hermes-feature-preview");
  const QSize compact = QSize(1024, 640).boundedTo(available);
  ui.window.resize(compact);
  ui.flush();
  CHECK(ui.window.width() <= compact.width());
  CHECK(ui.window.height() <= compact.height());
  ui.reveal(apply);
  ui.reveal(cancel);
  ui.capture("hermes-feature-compact");
  ui.window.resize(ordinary);
  ui.flush();
  ui.button("blcad.feature.cancel");
  ui.idle();
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.window.session().undo_label() == undo_before_preview);

  const auto body = ui.extrude(8);
  REQUIRE(ui.window.session().part_document()->features().size() == 1U);
  const auto feature = ui.window.session().part_document()->features().front().id();
  CHECK(ui.summary(body).solid_count == 1U);
  CHECK(ui.summary(body).volume_mm3 == Approx(4800).margin(1e-5));
  states.push_back(ui.snapshot());
  const auto select_body = [&] {
    ui.tree(body.value());
    auto& browser = ui.window.model_browser();
    const auto* selected = browser.currentItem();
    REQUIRE(selected);
    CHECK(selected->data(0, Qt::UserRole).toString() == QString::fromStdString(body.value()));
    CHECK(selected->isSelected());
    CHECK(browser.viewport()->rect().contains(browser.visualItemRect(selected).center()));
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->semantic_id == "body/" + body.value());
  };
  select_body();

  ui.tree(feature.value(), true);
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/length_parameter"), "12");
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Bracket blank");
  REQUIRE(ui.widget<QPushButton>("blcad.feature_edit.apply").isEnabled());
  ui.reveal(ui.widget<QPushButton>("blcad.feature_edit.cancel"));
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(7200).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  CHECK(ui.summary(body).volume_mm3 == Approx(7200).margin(1e-5));
  CHECK(checked(ui.window.session().sketch_dimension_catalog(sketch)).dimensions().size() == 1U);
  ui.roundtrip(directory.filePath("instrument-bracket.blcad"));
  CHECK(ui.summary(body).volume_mm3 == Approx(7200).margin(1e-5));
  CHECK(checked(ui.window.session().sketch_dimension_catalog(sketch)).dimensions().size() == 1U);
  ui.command("fit", 2);
  select_body();
  ui.capture("hermes-model");
  ui.window.resize(QSize(1600, 1000).boundedTo(available));
  ui.flush();
  select_body();
  ui.capture("hermes-model-large");
  ui.window.resize(ordinary);
  ui.flush();
  // STEP export is in the far-end inspection group. command() scrolls the
  // ribbon and verifies the actual button center before opening the dialog.
  const auto step = directory.filePath("instrument-bracket.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).startsWith("ISO-10303-21;"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Coincident repair restores a dragged junction and supports dimensions history and extrusion",
    "[gui][integration][coincident-closure][coincident-closure-native]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Repaired contour");
  ui.tree("datum.xy");
  ui.command("create_sketch");
  REQUIRE(ui.window.active_sketch());
  const auto sketch_id = *ui.window.active_sketch();
  ui.command("sketch.create.line");
  const auto coordinates = [&](Point2 point) {
    const auto text = QString::number(point.x) + ";" + QString::number(point.y);
    for (auto character : text)
      ui.key(ui.window, character.unicode(), QString(character));
    ui.key(ui.window, Qt::Key_Return);
  };
  // Exact numeric input deliberately leaves the closing junction independent:
  // no endpoint snap was accepted there. Ordinary chain junctions are automatic.
  coordinates({10, 10});
  for (Point2 offset : {Point2{40, 0}, {0, 30}, {-40, 0}, {0, -30}})
    coordinates(offset);
  for (int backout = 0; backout < 3 && ui.window.sketch_tools().tool_active(); ++backout)
    ui.key(ui.window, Qt::Key_Escape);
  REQUIRE_FALSE(ui.window.sketch_tools().tool_active());
  const auto sketch = [&]() -> const Sketch& {
    return *ui.window.session().part_document()->find_sketch(sketch_id);
  };
  REQUIRE(sketch().line_segments().size() == 4U);
  const auto mouse = [&](QEvent::Type type, Point2 point) {
    const auto screen = checked(ui.window.viewport().sketch_plane_to_screen(point));
    ui.mouse(ui.window.viewport(), {screen.x, screen.y}, type);
    ui.flush();
  };
  const auto drag_start = checked(ui.window.viewport().sketch_plane_to_screen({10, 10}));
  const auto drag_end = checked(ui.window.viewport().sketch_plane_to_screen({-5, -10}));
  REQUIRE(std::hypot(drag_end.x - drag_start.x, drag_end.y - drag_start.y) >
          QApplication::startDragDistance());
  mouse(QEvent::MouseButtonPress, {10, 10});
  mouse(QEvent::MouseMove, {-5, -10});
  mouse(QEvent::MouseButtonRelease, {-5, -10});
  const auto first = sketch().line_segments().front().start();
  const auto last = sketch().line_segments().back().end();
  REQUIRE(std::hypot(first.x - last.x, first.y - last.y) > 1.0);
  CHECK_FALSE(
      geometry::SketchFinishService::finish(*ui.window.session().part_document(), sketch_id));
  std::vector<std::string> states{ui.snapshot()};
  const auto constraints_before = checked(ui.window.session().sketch_constraint_catalog(sketch_id));
  ui.plane_pick(first);
  const auto last_screen = checked(ui.window.viewport().sketch_plane_to_screen(last));
  ui.click_at(ui.window.viewport(), {last_screen.x, last_screen.y}, Qt::ControlModifier);
  REQUIRE(ui.window.session().selection().entries().size() == 2U);
  bool applied = false;
  for (auto* button : ui.window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() && button->defaultAction()->text() == "Coincident") {
      ui.click(*button);
      applied = true;
      break;
    }
  REQUIRE(applied);
  CHECK(checked(ui.window.session().sketch_constraint_catalog(sketch_id)).constraints().size() ==
        constraints_before.constraints().size() + 1U);
  CHECK(sketch().line_segments().front().start() == sketch().line_segments().back().end());
  REQUIRE(geometry::SketchFinishService::finish(*ui.window.session().part_document(), sketch_id));
  states.push_back(ui.snapshot());
  ui.capture("coincident-repaired-junction");

  // Author two dimensions through the actual Rapid Dimension pointer workflow.
  for (std::size_t line_index : {0U, 1U}) {
    const auto line = sketch().line_segments()[line_index];
    const Point2 center{(line.start().x + line.end().x) / 2, (line.start().y + line.end().y) / 2};
    ui.command("sketch.dimension.rapid");
    const auto hover = [&](Point2 point) {
      const auto screen = checked(ui.window.viewport().sketch_plane_to_screen(point));
      QMouseEvent move(QEvent::MouseMove, {screen.x, screen.y},
                       ui.window.viewport().mapToGlobal(QPoint(qRound(screen.x), qRound(screen.y))),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(&ui.window.viewport(), &move);
      ui.flush();
      REQUIRE(ui.window.sketch_tools().rapid_dimension_candidate());
    };
    hover(center);
    ui.plane_pick(center);
    const Point2 label =
        line_index == 0 ? Point2{center.x, center.y - 15} : Point2{center.x + 15, center.y};
    hover(label);
    ui.plane_pick(label);
    ui.key(ui.window, Qt::Key_Escape);
    REQUIRE(checked(ui.window.session().sketch_dimension_catalog(sketch_id)).dimensions().size() ==
            line_index + 1U);
    states.push_back(ui.snapshot());
  }
  const auto area = [&] {
    double twice_area = 0;
    for (const auto& line : sketch().line_segments())
      twice_area += line.start().x * line.end().y - line.start().y * line.end().x;
    return std::abs(twice_area) / 2;
  };
  const double repaired_area = area();
  REQUIRE(repaired_area > 100);
  ui.command("finish_sketch");
  states.push_back(ui.snapshot());
  const auto body = ui.extrude(8);
  CHECK(ui.summary(body).volume_mm3 == Approx(repaired_area * 8).margin(1e-5));
  states.push_back(ui.snapshot());
  const auto dimension =
      checked(ui.window.session().sketch_dimension_catalog(sketch_id)).dimensions().front();
  const auto current =
      ui.window.session().part_document()->find_parameter(*dimension.parameter())->value();
  checked(GuiSketchDimensionController::edit_literal(
      ui.window.session(), sketch_id, dimension.id(),
      checked(Quantity::length_mm(current.millimeters() * 1.1))));
  ui.window.refresh();
  CHECK(sketch().line_segments().front().start() == sketch().line_segments().back().end());
  CHECK(ui.summary(body).volume_mm3 == Approx(area() * 8).margin(1e-5));
  CHECK(ui.summary(body).volume_mm3 != Approx(repaired_area * 8).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("repaired-contour.blcad.json"));
  CHECK(ui.summary(body).volume_mm3 == Approx(area() * 8).margin(1e-5));
  ui.capture("coincident-repaired-solid");
  const auto exported = directory.filePath("repaired-contour.step");
  ui.file("export_part_step", exported);
  CHECK(ui.bytes(exported).startsWith("ISO-10303-21;"));
}

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

TEST_CASE("Combined UI extrude extents machine an offset profile through corrections and history",
          "[gui][integration][shell-combined-workflow][shell-combined-extrude-extents]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Extent machining");
  ui.rectangle("datum.xy", {-40, -30}, {40, 30});
  const auto base = ui.extrude(20);
  const auto feature = ui.window.session().part_document()->features().back().id();
  const auto plane = ui.offset(10);
  const auto sketch = ui.rectangle(plane.value(), {-12, -8}, {12, 8});
  const auto initial_parameters = ui.window.session().part_document()->parameter_count();
  std::vector<std::string> states{ui.snapshot()};
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(base.value()));
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 2);
  ui.scalar("6 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_distance"), "4 deg");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.widget<QLineEdit>("blcad.extrude.second_distance").text() == "4 deg");
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_distance"), "4 mm");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("extrude-two-sided-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(92160).margin(1e-4));
  CHECK(ui.window.session().part_document()->parameter_count() == initial_parameters + 2);
  states.push_back(ui.snapshot());
  ui.tree(sketch.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 5);
  const auto first = QString::fromStdString(feature.value());
  ui.data(ui.widget<QComboBox>("blcad.extrude.first_face"), first + "/right");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.data(ui.widget<QComboBox>("blcad.extrude.first_face"), first + "/top");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("extrude-to-face-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(90624).margin(1e-4));
  CHECK(ui.window.session().part_document()->parameter_count() == initial_parameters + 2);
  states.push_back(ui.snapshot());
  ui.tree(sketch.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 6);
  ui.data(ui.widget<QComboBox>("blcad.extrude.first_face"), first + "/bottom");
  ui.data(ui.widget<QComboBox>("blcad.extrude.second_face"), first + "/bottom");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.data(ui.widget<QComboBox>("blcad.extrude.second_face"), first + "/top");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("extrude-between-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(88320).margin(1e-4));
  CHECK(ui.window.session().part_document()->parameter_count() == initial_parameters + 2);
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(sketch.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 1);
  ui.scalar("8 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "90 deg");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "-2 deg");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 1);
  ui.capture("extrude-symmetric-taper-preview");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("extent-machining.blcad"));
  CHECK(ui.summary(base).volume_mm3 == Approx(88320).margin(1e-4));
  const auto step = directory.filePath("extent-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Combined UI revolves directed and symmetric grooves through downstream edits and history",
    "[gui][integration][shell-combined-workflow][shell-combined-revolve-controls]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Rotational machining");
  ui.rectangle("datum.xy", {-30, -30}, {30, 30});
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 1);
  ui.scalar("24 mm");
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(body).volume_mm3 == Approx(86400).margin(1e-4));
  const auto sketch = ui.rectangle("datum.xy", {-12, 6}, {12, 10});
  const auto parameters = ui.window.session().part_document()->parameter_count();
  std::vector<std::string> states{ui.snapshot()};
  ui.command("revolve");
  ui.choose(ui.widget<QComboBox>("blcad.revolve.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.revolve.target"), QString::fromStdString(body.value()));
  ui.data(ui.widget<QComboBox>("blcad.revolve.axis"), "datum-axis/datum.axis.y");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.data(ui.widget<QComboBox>("blcad.revolve.axis"), "datum-axis/datum.axis.x");
  ui.scalar("90 mm");
  CHECK(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud").text() == "180");
  ui.scalar("90 deg");
  ui.choose(ui.widget<QComboBox>("blcad.revolve.side"), 1);
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("revolve-negative-cut-preview");
  const auto first = ui.window.feature_tools().coordinator().revolve().feature_id();
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(86400 - 384 * std::numbers::pi).margin(1e-4));
  states.push_back(ui.snapshot());
  ui.tree(sketch.value());
  ui.command("revolve");
  ui.choose(ui.widget<QComboBox>("blcad.revolve.operation"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.revolve.extent"), 2);
  ui.scalar("90 deg");
  ui.capture("revolve-symmetric-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(86400 - 576 * std::numbers::pi).margin(1e-4));
  states.push_back(ui.snapshot());
  ui.tree(sketch.value());
  ui.command("revolve");
  ui.choose(ui.widget<QComboBox>("blcad.revolve.operation"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.revolve.extent"), 0);
  REQUIRE(ui.window.feature_tools().manipulators().handles().empty());
  ui.capture("revolve-full-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(86400 - 1536 * std::numbers::pi).margin(1e-4));
  CHECK(ui.window.session().part_document()->body_count() == 1);
  CHECK(ui.window.session().part_document()->parameter_count() == parameters);
  states.push_back(ui.snapshot());
  ui.tree(first.value(), true);
  INFO((ui.window.session().diagnostics().empty()
            ? std::string{}
            : ui.window.session().diagnostics().back().message));
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/extent/angle_deg"), "60");
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Directed groove");
  ui.button("blcad.feature_edit.apply");
  const auto* intermediate = ui.window.session().part_shape_cache()->find_feature_shape(first);
  REQUIRE(intermediate);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*intermediate).volume_mm3 ==
        Approx(86400 - 256 * std::numbers::pi).margin(1e-4));
  CHECK(ui.summary(body).volume_mm3 == Approx(86400 - 1536 * std::numbers::pi).margin(1e-4));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(sketch.value());
  ui.command("revolve");
  ui.choose(ui.widget<QComboBox>("blcad.revolve.extent"), 0);
  ui.choose(ui.widget<QComboBox>("blcad.revolve.extent"), 2);
  ui.scalar("120 deg");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("rotational-machining.blcad"));
  CHECK(ui.summary(body).volume_mm3 == Approx(86400 - 1536 * std::numbers::pi).margin(1e-4));
  const auto step = directory.filePath("rotational-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI path extrusion machines a prior cut and follows corrected spatial edits",
          "[gui][integration][shell-combined-workflow][shell-combined-path-extrude]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Path machining");
  ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto body = ui.extrude(20);
  const auto pocket = ui.rectangle("datum.xy", {16, -3}, {20, 3});
  const auto profile = ui.rectangle("datum.xy", {-4, -3}, {4, 3});
  ui.command("sketch_3d");
  for (int index = 0; index < 4; ++index) {
    ui.choose(ui.widget<QComboBox>("blcad.sketch3d.point"), 0);
    for (const auto* axis : {"x", "y", "z"}) {
      auto& spin = ui.widget<QDoubleSpinBox>(QStringLiteral("blcad.sketch3d.") + axis);
      auto* editor = spin.findChild<QLineEdit*>();
      REQUIRE(editor);
      ui.text(*editor, QString::number(QString::fromLatin1(axis) == "z" ? index * 5 : 0));
    }
    ui.button("blcad.sketch3d.stage_point");
  }
  for (int index = 1; index <= 3; ++index) {
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.start"), "point." + QString::number(index));
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.end"), "point." + QString::number(index + 1));
    ui.button("blcad.sketch3d.stage_line");
  }
  ui.button("blcad.sketch3d.preview");
  ui.button("blcad.sketch3d.apply");
  const auto spatial = ui.window.session().part_document()->sketches_3d().back().id();
  ui.command("path_curve");
  for (int index = 1; index <= 3; ++index) {
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    ui.choose(source,
              source.findText("3D Sketch 1 / line." + QString::number(index), Qt::MatchStartsWith));
    ui.button("blcad.spatial.add");
  }
  ui.button("blcad.feature.apply");
  const auto path = ui.window.session().part_document()->path_curves().back().id();
  std::vector<std::string> states{ui.snapshot()};
  ui.tree(pocket.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.scalar("6 mm");
  const auto prior_cut = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(47856).margin(1e-5));
  states.push_back(ui.snapshot());
  const auto parameters = ui.window.session().part_document()->parameter_count();
  ui.tree(profile.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 2);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_distance"), "4 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "90 deg");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.path"), QString::fromStdString(path.value()));
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(body.value()));
  // Re-open the path popup after changing target-row visibility. This catches
  // form rows that remain logically visible but become occluded by another row.
  ui.data(ui.widget<QComboBox>("blcad.extrude.path"), QString::fromStdString(path.value()));
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.window.feature_tools().manipulators().handles().empty());
  CHECK(ui.window.viewport().feature_preview_replaces("body/" + body.value()));
  CHECK(ui.snapshot() == states.back());
  ui.command("fit", 2);
  ui.capture("path-extrude-cut-preview");
  const auto routed_cut = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(47136).margin(1e-5));
  CHECK(ui.window.session().part_document()->parameter_count() == parameters);
  CHECK(ui.window.session().part_document()->body_count() == 1);
  states.push_back(ui.snapshot());

  ui.tree(prior_cut.value(), true);
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/length_parameter"), "8");
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Deeper pocket before route");
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(47088).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(spatial.value(), true);
  ui.data(ui.widget<QComboBox>("blcad.sketch3d.point"), "point.4");
  auto& z = ui.widget<QDoubleSpinBox>("blcad.sketch3d.z");
  auto* editor = z.findChild<QLineEdit*>();
  REQUIRE(editor);
  ui.text(*editor, "10");
  ui.button("blcad.sketch3d.stage_point");
  ui.button("blcad.sketch3d.preview");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.sketch3d.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.text(*editor, "25");
  ui.button("blcad.sketch3d.stage_point");
  ui.button("blcad.sketch3d.preview");
  REQUIRE(ui.widget<QPushButton>("blcad.sketch3d.apply").isEnabled());
  ui.capture("path-extrude-downstream-edit-preview");
  ui.button("blcad.sketch3d.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(46848).margin(1e-5));
  CHECK(ui.window.session().part_document()->find_feature(routed_cut)->path_curve() == path);
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(profile.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 1);
  ui.scalar("7 mm");
  ui.capture("path-extrude-return-to-normal-preview");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("path-machining.blcad"));
  CHECK(ui.summary(body).volume_mm3 == Approx(46848).margin(1e-5));
  const auto step = directory.filePath("path-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Combined UI thin walls machine a plate and retain independent wall parameters through history",
    "[gui][integration][shell-combined-workflow][shell-combined-thin-extrude]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Thin wall machining");
  ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto base = ui.extrude(20);
  const auto groove = ui.open_line("datum.xy", {-20, 0}, {20, 0});
  const auto slot = ui.open_line("datum.xy", {-20, 10}, {20, 10});
  const auto parameters = ui.window.session().part_document()->parameter_count();
  std::vector<std::string> states{ui.snapshot()};
  ui.tree(groove.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.thin_mode"), 1);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "2 mm");
  ui.scalar("6 mm");
  REQUIRE(ui.window.viewport().feature_preview_replaces("body/" + base.value()));
  CHECK(ui.snapshot() == states.back());
  const auto first = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.capture("thin-midplane-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(47520).margin(1e-5));
  const auto thickness = ui.window.session()
                             .part_document()
                             ->find_feature(first)
                             ->extrude_intent()
                             .thin()
                             ->first_thickness_parameter();
  states.push_back(ui.snapshot());

  ui.tree(groove.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 1);
  ui.scalar("12 mm");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.thin_mode"), 2);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "2 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_thickness"), "3 deg");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_thickness"), "3 mm");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  const auto wall = ui.window.feature_tools().coordinator().extrude().result_body_id();
  ui.capture("thin-two-sided-new-body-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(wall).volume_mm3 == Approx(2400).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(47520).margin(1e-5));
  states.push_back(ui.snapshot());

  ui.tree(slot.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(base.value()));
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 3);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "3 mm");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.window.feature_tools().manipulators().handles().size() == 1);
  ui.command("fit", 2);
  ui.capture("thin-through-all-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(45120).margin(1e-5));
  CHECK(ui.window.session().part_document()->parameter_count() == parameters + 6);
  CHECK(ui.window.session().part_document()->body_count() == 2);
  states.push_back(ui.snapshot());

  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(thickness.value()));
  CHECK(ui.widget<QLabel>("blcad.parameters.usage")
            .text()
            .contains(QString::fromStdString(first.value())));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "4");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(44640).margin(1e-5));
  CHECK(ui.summary(wall).volume_mm3 == Approx(2400).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(first.value(), true);
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Parameterized thin groove");
  ui.button("blcad.feature_edit.apply");
  const auto* edited = ui.window.session().part_document()->find_feature(first);
  REQUIRE(edited);
  REQUIRE(edited->extrude_intent().thin());
  CHECK_FALSE(edited->profile());
  CHECK(edited->extrude_intent().thin()->first_thickness_parameter() == thickness);
  CHECK(ui.summary(base).volume_mm3 == Approx(44640).margin(1e-5));
  CHECK(ui.summary(wall).volume_mm3 == Approx(2400).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(groove.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.thin_mode"), 2);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_thickness"), "7 mm");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.thin_mode"), 1);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 1);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "2.5 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "-1 deg");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("thin-machining.blcad"));
  CHECK(ui.summary(base).volume_mm3 == Approx(44640).margin(1e-5));
  CHECK(ui.summary(wall).volume_mm3 == Approx(2400).margin(1e-5));
  const auto step = directory.filePath("thin-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI view navigation preserves Sketch and machining state through history and "
          "reopening",
          "[gui][integration][shell-combined-workflow][shell-combined-navigation]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Navigation machining");
  ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto body = ui.extrude(20);
  const auto base_feature = ui.window.session().part_document()->features().back().id();
  const auto depth =
      ui.window.session().part_document()->find_feature(base_feature)->length_parameter();
  const auto pocket = ui.rectangle("datum.xy", {-10, -5}, {10, 5});
  const auto before = ui.snapshot();
  ui.command("view_top", 2);
  ui.command("perspective", 2); // Capture an orthographic model view.
  REQUIRE(ui.window.viewport().projection() == GuiViewportProjection::Orthographic);
  const auto home = ui.window.viewport().camera_bookmark();
  ui.command("capture_home", 2);
  auto& marks = ui.widget<QComboBox>("blcad.navigation.bookmarks");
  REQUIRE(marks.lineEdit());
  ui.text(*marks.lineEdit(), "Top");
  ui.command("bookmark_save", 2);
  ui.command("view_front", 2);
  ui.text(*marks.lineEdit(), "Front");
  ui.command("bookmark_save", 2);
  CHECK(marks.count() == 2);
  ui.command("view_back", 2); // Actual ViewCube popup input.
  ui.command("view_home", 2);
  const auto home_restored = ui.window.viewport().camera_bookmark();
  CHECK(home_restored.eye.x == Approx(home.eye.x).margin(1e-7));
  CHECK(home_restored.eye.y == Approx(home.eye.y).margin(1e-7));
  CHECK(home_restored.eye.z == Approx(home.eye.z).margin(1e-7));
  CHECK(home_restored.scale == Approx(home.scale).margin(1e-7));
  CHECK_FALSE(ui.action("perspective").isChecked());
  CHECK(ui.snapshot() == before);
  ui.tree(pocket.value(), true);
  REQUIRE(ui.window.active_sketch());
  CHECK_FALSE(ui.action("view_home").isEnabled());
  CHECK_FALSE(ui.action("view_front").isEnabled());
  CHECK_FALSE(ui.action("bookmark_restore").isEnabled());
  CHECK_FALSE(ui.action("perspective").isEnabled());
  ui.command("finish_sketch");
  CHECK(ui.snapshot() == before);
  CHECK(marks.count() == 2);
  std::vector<std::string> states{ui.snapshot()};

  ui.tree(pocket.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.scalar("6 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "invalid");
  ui.window.ribbon().set_current_tab(2);
  ui.flush();
  ui.choose(marks, marks.findText("Front"));
  ui.command("bookmark_restore", 2);
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.widget<QLineEdit>("blcad.extrude.taper").text() == "invalid");
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.flush();
  const auto presentations = ui.window.feature_tools().manipulators().presentations();
  const auto handle = std::find_if(presentations.begin(), presentations.end(), [](const auto& p) {
    return p.handle_id == kExtrudeExtentHandleId;
  });
  REQUIRE(handle != presentations.end());
  const QPointF point(handle->endpoint.x, handle->endpoint.y);
  REQUIRE(ui.window.viewport().rect().contains(point.toPoint()));
  ui.mouse(ui.window.viewport(), point, QEvent::MouseButtonPress);
  REQUIRE(ui.window.feature_tools().manipulators().active_handle_id() == kExtrudeExtentHandleId);
  ui.flush();
  CHECK_FALSE(ui.action("view_home").isEnabled());
  CHECK_FALSE(ui.action("bookmark_restore").isEnabled());
  ui.mouse(ui.window.viewport(), point + QPointF(12, -14), QEvent::MouseMove);
  ui.key(ui.window, Qt::Key_Escape);
  REQUIRE(ui.window.feature_tools().active());
  CHECK(ui.window.feature_tools().manipulators().active_handle_id().empty());
  CHECK(ui.window.feature_tools().coordinator().extrude().distance() == Approx(6));
  CHECK(ui.snapshot() == states.back());
  ui.flush();
  ui.command("view_home", 2);
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.capture("navigation-machining-preview");
  const auto cut = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(46800).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(depth.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "24");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(56400).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(cut.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Navigation checked pocket");
  ui.button("blcad.feature_edit.apply");
  states.push_back(ui.snapshot());
  ui.history(states);
  CHECK(marks.count() == 2);
  ui.window.ribbon().set_current_tab(2);
  ui.flush();
  ui.choose(marks, marks.findText("Top"));
  ui.command("bookmark_restore", 2);
  ui.command("bookmark_remove", 2);
  CHECK(marks.count() == 1);
  ui.capture("navigation-bookmarks-after-history");
  ui.roundtrip(directory.filePath("navigation.blcad"));
  CHECK(marks.count() == 0);
  CHECK_FALSE(ui.action("view_home").isEnabled());
  CHECK(ui.summary(body).volume_mm3 == Approx(56400).margin(1e-5));
  const auto step = directory.filePath("navigation.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Combined UI face collection preserves earlier machining and propagates changed source limits",
    "[gui][integration][shell-combined-workflow][shell-combined-extrude-limit-picking]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Semantic limit machining");
  ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto base = ui.extrude(20);
  const auto source = ui.window.session().part_document()->features().back().id();
  const auto depth = ui.window.session().part_document()->find_feature(source)->length_parameter();
  ui.rectangle("datum.xy", {45, -5}, {55, 5});
  const auto stop = ui.extrude(5);
  const auto stop_source = ui.window.session().part_document()->features().back().id();
  const auto pocket = ui.rectangle("datum.xy", {-10, -5}, {10, 5});
  const auto through = ui.rectangle("datum.xy", {15, -4}, {25, 4});
  const auto wall = ui.open_line("datum.xy", {-20, 10}, {20, 10});
  const auto parameters = ui.window.session().part_document()->parameter_count();
  std::vector<std::string> states{ui.snapshot()};

  ui.tree(pocket.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(base.value()));
  ui.scalar("4 mm");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(47200).margin(1e-5));
  states.push_back(ui.snapshot());

  ui.tree(through.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(base.value()));
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 5);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.pick_role"), 1);
  ui.standard_view("Right");
  ui.command("fit", 2);
  ui.limit_face(source, "right", {30, -10, 10});
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.standard_view("Top");
  ui.limit_face(source, "top", {-20, -10, 20});
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.window.viewport().feature_preview_replaces("body/" + base.value()));
  const auto cut = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.capture("extrude-picked-to-face-cut");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(45600).margin(1e-5));
  ui.idle();
  states.push_back(ui.snapshot());

  ui.tree(wall.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.thin_mode"), 1);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "2 mm");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 6);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.pick_role"), 1);
  ui.limit_face(source, "top", {-20, -10, 20});
  ui.choose(ui.widget<QComboBox>("blcad.extrude.pick_role"), 2);
  ui.limit_face(source, "top", {-20, -10, 20});
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.limit_face(stop_source, "top", {50, 0, 5});
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  const auto wall_body = ui.window.feature_tools().coordinator().extrude().result_body_id();
  ui.capture("extrude-picked-between-thin-wall");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(wall_body).volume_mm3 == Approx(1200).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(45600).margin(1e-5));
  CHECK(ui.summary(stop).volume_mm3 == Approx(500).margin(1e-5));
  CHECK(ui.window.session().part_document()->parameter_count() == parameters + 2);
  ui.idle();
  states.push_back(ui.snapshot());

  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(depth.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "26");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(59520).margin(1e-5));
  CHECK(ui.summary(wall_body).volume_mm3 == Approx(1680).margin(1e-5));
  CHECK(ui.summary(stop).volume_mm3 == Approx(500).margin(1e-5));
  CHECK(ui.bounds(wall_body).minimum.z == Approx(5).margin(1e-5));
  CHECK(ui.bounds(wall_body).maximum.z == Approx(26).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(cut.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Picked through cut");
  ui.button("blcad.feature_edit.apply");
  REQUIRE(ui.window.session()
              .part_document()
              ->find_feature(cut)
              ->extrude_intent()
              .extent()
              .first_face());
  CHECK(ui.summary(base).volume_mm3 == Approx(59520).margin(1e-5));
  CHECK(ui.summary(wall_body).volume_mm3 == Approx(1680).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);

  ui.tree(wall.value());
  ui.command("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 6);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.pick_role"), 2);
  ui.limit_face(stop_source, "top", {50, 0, 5});
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 0);
  CHECK_FALSE(ui.window.viewport().selection_semantic_scope());
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 5);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.pick_role"), 1);
  ui.limit_face(source, "top", {-20, -10, 26});
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("picked-limits.blcad"));
  CHECK(ui.summary(base).volume_mm3 == Approx(59520).margin(1e-5));
  CHECK(ui.summary(wall_body).volume_mm3 == Approx(1680).margin(1e-5));
  const auto step = directory.filePath("picked-limits.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
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
  const auto move_history = std::string(ui.window.session().undo_label());
  const bool move_can_redo = ui.window.session().can_redo();
  ui.scalar("40 mm");
  CHECK(ui.window.statusBar()->currentMessage().contains("no measurable material"));
  CHECK(ui.snapshot() == scaled);
  CHECK(ui.window.session().undo_label() == move_history);
  CHECK(ui.window.session().can_redo() == move_can_redo);
  CHECK(ui.bounds(tool).minimum.x == Approx(30).margin(1e-5));
  // Retained Boolean tools are associative: moving the tool entirely out of
  // the target invalidates the existing Cut. Recover with a valid overlap.
  ui.scalar("5 mm");
  ui.button("blcad.feature.apply");
  CHECK_FALSE(ui.window.session().can_redo());
  CHECK(ui.window.session().part_document()->mirror_features().empty());
  CHECK(ui.summary(tool).volume_mm3 == Approx(4000));
  CHECK(ui.bounds(tool).minimum.x == Approx(35).margin(1e-5));
  CHECK(ui.summary(target).volume_mm3 == Approx(24000));
  ui.roundtrip(directory.filePath("branched.blcad.json"));
  CHECK(ui.summary(target).volume_mm3 == Approx(24000));
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
    if (button->isVisible() && button->text() == "Up") {
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

TEST_CASE(
    "Mixed path UI Loft reverses an arc reorders five oriented sections edits and restores history",
    "[gui][integration][shell-mixed-path-loft]") {
  namespace fixture = blcad::test::mixed_loft;
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto input = directory.filePath("oriented-inputs.blcad");
  // This scenario tests consuming existing oriented Sketches, not their authoring.
  // The imported document contains no PathCurve, Loft or body.
  REQUIRE(write_part_document_json_file(fixture::inputs(5, false), input.toStdString()));
  ui.file("open", input);
  const auto imported = ui.snapshot();
  REQUIRE(ui.window.session().part_document()->path_curves().empty());
  REQUIRE(ui.window.session().part_document()->bodies().empty());
  ui.command("path_curve");
  const auto add = [&](const QString& prefix) {
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    ui.choose(source, source.findText(prefix, Qt::MatchStartsWith));
    ui.button("blcad.spatial.add");
  };
  add("Mixed trajectory / lead");
  add("Mixed trajectory / spline");
  add("Mixed trajectory / arc");
  auto& path_order = ui.widget<QListWidget>("blcad.spatial.order");
  REQUIRE(path_order.count() == 3);
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == imported);
  ui.list(path_order, 2);
  ui.key(path_order, Qt::Key_Space);
  REQUIRE(path_order.item(2)->checkState() == Qt::Checked);
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == imported);
  ui.button("blcad.feature.apply");
  REQUIRE(ui.window.session().part_document()->path_curves().size() == 1);
  const auto path = ui.window.session().part_document()->path_curves().back().id();
  const auto with_path = ui.snapshot();

  ui.command("loft");
  for (const int i : {0, 2, 1, 3, 4})
    add(QString("Section %1 / ").arg(i));
  ui.data(ui.widget<QComboBox>("blcad.spatial.path"), QString::fromStdString(path.value()));
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  auto& sections = ui.widget<QListWidget>("blcad.spatial.order");
  REQUIRE(sections.count() == 5);
  ui.list(sections, 2);
  bool moved = false;
  for (auto* button : ui.window.findChildren<QPushButton*>())
    if (button->isVisible() && button->text() == "Up") {
      ui.click(*button);
      moved = true;
      break;
    }
  REQUIRE(moved);
  CHECK(sections.item(1)->text().startsWith("Section 1 / "));
  REQUIRE(ui.window.viewport().feature_preview_active());
  QComboBox* continuity{};
  for (auto* combo : ui.window.findChildren<QComboBox*>())
    if (combo->isVisible() && combo->findText("G2") >= 0)
      continuity = combo;
  REQUIRE(continuity);
  ui.choose(*continuity, continuity->findText("G2"));
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == with_path);
  ui.choose(*continuity, continuity->findText("C0"));
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.feature.apply");
  const auto* part = ui.window.session().part_document();
  REQUIRE(part->loft_features().size() == 1);
  const auto& committed_sections = part->loft_features().back().sections();
  REQUIRE(committed_sections.size() == 5);
  for (std::size_t i = 0; i < committed_sections.size(); ++i) {
    CHECK(committed_sections[i].source_node_id() == "section." + std::to_string(i));
    CHECK_FALSE(committed_sections[i].flip_normal());
  }
  CHECK(part->loft_features().back().path_curve() == path);
  const auto body = part->bodies().back().id();
  CHECK(ui.summary(body).solid_count == 1);
  const auto initial_volume = ui.summary(body).volume_mm3;
  CHECK(initial_volume > 100);
  const auto created = ui.snapshot();
  ui.idle();

  // Enter the middle profile's new width through the visible property delegate.
  ui.tree("section.2.width");
  auto& properties = ui.widget<QTableWidget>("blcad.shell.properties");
  QTableWidgetItem* value{};
  for (int row = 0; row < properties.rowCount(); ++row)
    if (properties.item(row, 1)->data(Qt::UserRole).toString() == "value")
      value = properties.item(row, 1);
  REQUIRE(value);
  properties.scrollToItem(value);
  ui.flush();
  ui.click_at(*properties.viewport(), properties.visualItemRect(value).center());
  ui.key(properties, Qt::Key_F2);
  auto* editor = properties.findChild<QLineEdit*>();
  REQUIRE(editor);
  ui.text(*editor, "4.5");
  REQUIRE(ui.window.session()
              .part_document()
              ->find_parameter(ParameterId("section.2.width"))
              ->value()
              .millimeters() == 4.5);
  const auto edited = ui.snapshot();
  const auto edited_volume = ui.summary(body).volume_mm3;
  CHECK(edited_volume > initial_volume + 5);
  ui.command("undo");
  CHECK(ui.snapshot() == created);
  CHECK(ui.summary(body).volume_mm3 == Approx(initial_volume).epsilon(1e-8));
  ui.command("undo");
  CHECK(ui.snapshot() == with_path);
  CHECK(ui.window.session().part_shape_cache()->body_shape_count() == 0);
  ui.command("undo");
  CHECK(ui.snapshot() == imported);
  for (int i = 0; i < 3; ++i)
    ui.command("redo");
  CHECK(ui.snapshot() == edited);
  CHECK(ui.summary(body).volume_mm3 == Approx(edited_volume).epsilon(1e-8));
  ui.idle();
  ui.capture("mixed-path-five-oriented-sections");
  const auto saved = directory.filePath("mixed-loft.blcad");
  ui.roundtrip(saved);
  CHECK(ui.summary(body).volume_mm3 == Approx(edited_volume).epsilon(1e-8));
  const auto source_bytes = ui.bytes(saved);
  const auto step = directory.filePath("mixed-loft.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.bytes(saved) == source_bytes);
  CHECK(ui.snapshot() == edited);
  ui.idle();
}

TEST_CASE(
    "UI Loft automatically reconciles unequal spline profiles through preview history and export",
    "[gui][integration][shell-combined-workflow][shell-loft-auto-segments]") {
  namespace fixture = blcad::test::loft_refinement;
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto input = directory.filePath("unequal-sections.blcad.json");
  REQUIRE(write_part_document_json_file(fixture::inputs(), input.toStdString()));
  ui.file("open", input);
  const auto original = ui.snapshot();
  const auto source_sketches =
      QJsonDocument::fromJson(QByteArray::fromStdString(original)).object().value("sketches");
  const auto unchanged = [&] {
    CHECK(QJsonDocument::fromJson(QByteArray::fromStdString(ui.snapshot()))
              .object()
              .value("sketches") == source_sketches);
    const auto& sketches = ui.window.session().part_document()->sketches();
    REQUIRE(sketches.size() == 3);
    CHECK(sketches[0].spline_segments().size() == 8);
    CHECK(sketches[1].spline_segments().size() == 9);
    CHECK(sketches[2].spline_segments().size() == 6);
  };
  const auto prepare = [&] {
    ui.command("loft");
    CHECK(ui.widget<QLabel>("blcad.spatial.order_hint").text().contains("automatically"));
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    for (const auto prefix : {"Root profile /", "Tip profile /", "Mid profile /"}) {
      ui.choose(source, source.findText(prefix, Qt::MatchStartsWith));
      ui.button("blcad.spatial.add");
    }
    auto& order = ui.widget<QListWidget>("blcad.spatial.order");
    REQUIRE(order.count() == 3);
    ui.list(order, 2);
    ui.button("blcad.spatial.order.up");
    CHECK(order.item(1)->text().startsWith("Mid profile /"));
    REQUIRE(ui.window.viewport().feature_preview_active());
    REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
    CHECK(ui.snapshot() == original);
    unchanged();
  };
  prepare();
  ui.key(ui.window.viewport(), Qt::Key_Escape);
  ui.idle();
  CHECK(ui.snapshot() == original);
  CHECK_FALSE(ui.window.session().can_undo());
  prepare();
  auto& continuity = ui.widget<QComboBox>("blcad.spatial.continuity");
  ui.choose(continuity, 2);
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  unchanged();
  ui.choose(continuity, 0);
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.capture("loft-automatic-unequal-spline-preview");
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  const auto loft = ui.window.session().part_document()->loft_features().back().id();
  const auto summary = ui.summary(body);
  CHECK(summary.solid_count == 1);
  CHECK(summary.volume_mm3 > 0);
  unchanged();
  std::vector<std::string> states{original, ui.snapshot()};
  ui.tree(loft.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Automatically matched blade");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.window.session().part_document()->loft_features().back().id() == loft);
  unchanged();
  states.push_back(ui.snapshot());
  ui.command("body_translate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(body.value()));
  ui.scalar("8 mm");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).solid_count == 1);
  CHECK(ui.summary(body).volume_mm3 == Approx(summary.volume_mm3).epsilon(1e-8));
  unchanged();
  states.push_back(ui.snapshot());
  ui.history(states);
  unchanged();
  ui.roundtrip(directory.filePath("automatic-loft.blcad.json"));
  unchanged();
  const auto saved = ui.snapshot();
  ui.file("export_part_step", directory.filePath("automatic-loft.step"));
  CHECK(ui.bytes(directory.filePath("automatic-loft.step")).startsWith("ISO-10303-21;"));
  CHECK(ui.snapshot() == saved);
  CHECK_FALSE(ui.window.session().dirty());
  ui.command("fit", 2);
  ui.capture("loft-automatic-unequal-spline-result");
  ui.idle();
}

TEST_CASE("UI Loft aligns independently drawn sections through opt-out recovery history and export",
          "[gui][integration][shell-combined-workflow][shell-loft-section-alignment]") {
  namespace fixture = blcad::test::loft_alignment;
  auto middle = fixture::MiddleAuthoring::Reversed;
  SECTION("the middle contour was drawn backwards") {
    middle = fixture::MiddleAuthoring::Reversed;
  }
  SECTION("the middle contour starts at another corner") {
    middle = fixture::MiddleAuthoring::ShiftedStart;
  }
  CAPTURE(middle);
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto input = directory.filePath("independent-loft-sections.blcad.json");
  REQUIRE(write_part_document_json_file(fixture::inputs(middle, false), input.toStdString()));
  ui.file("open", input);
  const auto original = ui.snapshot();
  const auto sketches =
      QJsonDocument::fromJson(QByteArray::fromStdString(original)).object().value("sketches");
  const auto unchanged = [&] {
    CHECK(QJsonDocument::fromJson(QByteArray::fromStdString(ui.snapshot()))
              .object()
              .value("sketches") == sketches);
  };
  const auto prepare = [&] {
    ui.command("loft");
    auto& automatic = ui.widget<QCheckBox>("blcad.spatial.auto_align");
    REQUIRE(automatic.isChecked());
    REQUIRE(automatic.isEnabled());
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    // Select the tip before the middle, then correct the list with its real
    // ordering control, as when assembling a Loft from a crowded browser.
    for (const auto* prefix : {"Section 0 /", "Section 2 /", "Section 1 /"}) {
      ui.choose(source, source.findText(prefix, Qt::MatchStartsWith));
      ui.button("blcad.spatial.add");
    }
    auto& order = ui.widget<QListWidget>("blcad.spatial.order");
    REQUIRE(order.count() == 3);
    ui.list(order, 2);
    ui.button("blcad.spatial.order.up");
    CHECK(order.item(1)->text().startsWith("Section 1 /"));
    ui.button("blcad.spatial.add"); // Duplicate current source is rejected.
    CHECK(order.count() == 3);
    REQUIRE(ui.window.viewport().feature_preview_active());
    REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
    CHECK(ui.snapshot() == original);
    unchanged();
  };
  prepare();
  auto& automatic = ui.widget<QCheckBox>("blcad.spatial.auto_align");
  ui.click(automatic);
  CHECK_FALSE(automatic.isChecked());
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.click(automatic);
  REQUIRE(automatic.isChecked());
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.capture("loft-section-alignment-preview");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == original);
  CHECK_FALSE(ui.window.session().can_undo());
  ui.idle();

  // Opting out preserves the manually authored correspondence, including an
  // intentional twist. Undo/Redo must reproduce that choice exactly.
  prepare();
  ui.click(ui.widget<QCheckBox>("blcad.spatial.auto_align"));
  ui.button("blcad.feature.apply");
  const auto manual_body = ui.window.session().part_document()->bodies().back().id();
  const auto manual = ui.snapshot();
  CHECK(ui.summary(manual_body).solid_count == 1);
  CHECK(ui.summary(manual_body).volume_mm3 < 2200.0);
  unchanged();
  ui.command("undo");
  CHECK(ui.snapshot() == original);
  ui.command("redo");
  CHECK(ui.snapshot() == manual);
  ui.command("undo");
  CHECK(ui.snapshot() == original);
  ui.idle();

  prepare(); // A fresh task starts with automatic correspondence enabled.
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  REQUIRE(ui.window.session().part_document()->loft_features().size() == 1);
  REQUIRE(ui.window.session().part_document()->bodies().size() == 1);
  const auto verify_body = [&] {
    const auto* aligned = ui.window.session().part_shape_cache()->find_body_shape(body);
    REQUIRE(aligned);
    fixture::verify_prism(*aligned);
  };
  verify_body();
  unchanged();
  const auto corrected = ui.snapshot();
  CHECK_FALSE(ui.window.session().can_redo());
  ui.history({original, corrected});
  verify_body();
  unchanged();
  ui.roundtrip(directory.filePath("aligned-loft.blcad.json"));
  CHECK(ui.snapshot() == corrected);
  verify_body();
  unchanged();
  const auto saved = ui.bytes(directory.filePath("aligned-loft.blcad.json"));
  const auto step = directory.filePath("aligned-loft.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).startsWith("ISO-10303-21;"));
  CHECK(ui.bytes(directory.filePath("aligned-loft.blcad.json")) == saved);
  CHECK(ui.snapshot() == corrected);
  CHECK_FALSE(ui.window.session().dirty());
  ui.command("fit", 2);
  ui.capture("loft-section-alignment-result");
  ui.idle();
}

TEST_CASE("Combined UI sews authored faces rejects an open shell and branches into a solid Boolean",
          "[gui][integration][shell-combined-workflow][shell-combined-surface-solid]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Sewn solid workflow");
  ui.rectangle("datum.xy", {20, 20}, {40, 40});
  const auto base = ui.extrude(10);
  const auto source_name = ui.window.session().part_document()->features().front().name();
  CHECK(ui.summary(base).volume_mm3 == Approx(4000).margin(1e-6));
  const auto original = ui.snapshot();
  const auto stitch = [&](int faces) {
    ui.command("surface_stitch");
    for (int index = 0; index < faces; ++index) {
      auto& target = ui.widget<QComboBox>("blcad.surface.target");
      ui.choose(target, target.findText(QString::fromStdString(
                            source_name + " / " +
                            std::string(to_string(static_cast<SemanticFace>(index))))));
      ui.button("blcad.surface.add");
    }
    CHECK(ui.widget<QListWidget>("blcad.surface.order").count() == faces);
    REQUIRE(ui.window.viewport().feature_preview_active());
    CHECK(ui.snapshot() == original);
    ui.scalar("0.001 mm");
    ui.button("blcad.feature.apply");
    return ui.window.session().part_document()->bodies().back().id();
  };
  const auto open_shell = stitch(5);
  CHECK(ui.summary(open_shell).solid_count == 0);
  const auto incomplete = ui.snapshot();
  ui.command("surface_to_solid");
  auto& target = ui.widget<QComboBox>("blcad.surface.target");
  ui.choose(target, target.findText(QString::fromStdString(
                        ui.window.session().part_document()->find_body(open_shell)->name())));
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == incomplete);
  ui.button("blcad.feature.cancel");
  ui.idle();
  ui.command("undo");
  CHECK(ui.snapshot() == original);
  CHECK(ui.window.session().can_redo());

  const auto shell = stitch(6);
  CHECK_FALSE(ui.window.session().can_redo());
  CHECK(ui.summary(shell).solid_count == 0);
  std::vector<std::string> states{original, ui.snapshot()};
  ui.command("surface_to_solid");
  ui.choose(target, target.findText(QString::fromStdString(
                        ui.window.session().part_document()->find_body(shell)->name())));
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.capture("combined-sewn-solid-preview");
  ui.button("blcad.feature.apply");
  const auto solid = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(solid).solid_count == 1);
  CHECK(ui.summary(solid).volume_mm3 == Approx(4000).margin(1e-6));
  CHECK(ui.window.session().part_document()->find_body(solid)->kind() == BodyKind::Solid);
  states.push_back(ui.snapshot());

  ui.command("body_translate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(solid.value()));
  ui.scalar("10 mm");
  ui.button("blcad.feature.apply");
  CHECK(ui.bounds(solid).minimum.x == Approx(30).margin(1e-5));
  CHECK(ui.bounds(solid).maximum.x == Approx(50).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(4000).margin(1e-6));
  states.push_back(ui.snapshot());

  ui.command("body_boolean");
  ui.choose(ui.widget<QComboBox>("blcad.boolean.operation"), 0);
  ui.data(ui.widget<QComboBox>("blcad.boolean.target"), QString::fromStdString(base.value()));
  ui.capture("combined-sewn-boolean-controls");
  ui.choose(ui.widget<QComboBox>("blcad.boolean.result"), 1);
  auto& keep = ui.widget<QCheckBox>("blcad.boolean.keep_tools");
  if (!keep.isChecked())
    ui.click(keep);
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), QString::fromStdString(solid.value()));
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto& operation = ui.window.session().part_document()->body_boolean_features().back();
  REQUIRE(operation.produced_body());
  const auto united = *operation.produced_body();
  CHECK(operation.result_mode() == BodyBooleanResultMode::NewBody);
  CHECK(operation.target_body() == base);
  CHECK(operation.keep_tool_bodies());
  CHECK(united != base);
  CHECK(united != solid);
  CHECK(ui.summary(united).solid_count == 1);
  CHECK(ui.summary(united).volume_mm3 == Approx(6000).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(4000).margin(1e-6));
  CHECK(ui.summary(solid).volume_mm3 == Approx(4000).margin(1e-6));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("sewn-solid.blcad.json"));
  const auto saved = ui.bytes(directory.filePath("sewn-solid.blcad.json"));
  ui.file("export_part_step", directory.filePath("sewn-solid.step"));
  CHECK(ui.bytes(directory.filePath("sewn-solid.step")).startsWith("ISO-10303-21;"));
  CHECK(ui.bytes(directory.filePath("sewn-solid.blcad.json")) == saved);
  CHECK(ui.snapshot() == states.back());
  CHECK_FALSE(ui.window.session().dirty());
  ui.idle();
}

TEST_CASE("Combined UI edits a revolved source through a rotated scaled circular pattern",
          "[gui][integration][shell-combined-workflow][shell-combined-revolve-pattern]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Revolved pattern workflow");
  ui.rectangle("datum.xy", {20, 2}, {24, 4});
  ui.command("revolve");
  ui.scalar("180 deg");
  ui.button("blcad.feature.apply");
  const auto source = ui.window.session().part_document()->bodies().back().id();
  const auto revolve = ui.window.session().part_document()->revolve_features().back().id();
  const double initial_volume = 24 * std::numbers::pi;
  CHECK(ui.summary(source).solid_count == 1);
  CHECK(ui.summary(source).volume_mm3 == Approx(initial_volume).margin(1e-5));
  std::vector<std::string> states{ui.snapshot()};

  ui.command("circular_pattern");
  ui.list_data(ui.widget<QListWidget>("blcad.pattern.sources"),
               QString::fromStdString(source.value()));
  ui.number(ui.widget<QSpinBox>("blcad.pattern.count"), "4");
  ui.scalar("360 deg");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto result = ui.window.session().part_document()->bodies().back().id();
  const auto pattern = ui.window.session().part_document()->circular_pattern_features().back().id();
  CHECK(ui.summary(result).solid_count == 4);
  CHECK(ui.summary(result).volume_mm3 == Approx(4 * initial_volume).margin(1e-5));
  const auto patterned_bounds = ui.bounds(result);
  states.push_back(ui.snapshot());
  ui.command("body_rotate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"),
               QString::fromStdString(result.value()));
  ui.scalar("45 deg");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(result).volume_mm3 == Approx(4 * initial_volume).margin(1e-5));
  const auto rotated_bounds = ui.bounds(result);
  CHECK(rotated_bounds.maximum.x < patterned_bounds.maximum.x - 1);
  CHECK(rotated_bounds.minimum.x > patterned_bounds.minimum.x + 1);
  states.push_back(ui.snapshot());
  ui.command("body_scale");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"),
               QString::fromStdString(result.value()));
  for (const auto& [axis, value] :
       std::vector<std::pair<int, QString>>{{0, "5 mm"}, {1, "-3 mm"}, {2, "2 mm"}})
    ui.text(ui.widget<QLineEdit>(QString("blcad.body.center.%1").arg(axis)), value);
  ui.scalar("1.5");
  ui.button("blcad.feature.apply");
  const double scale_volume = 1.5 * 1.5 * 1.5;
  CHECK(ui.summary(result).volume_mm3 == Approx(4 * initial_volume * scale_volume).margin(1e-4));
  const auto scaled_bounds = ui.bounds(result);
  CHECK(scaled_bounds.minimum.x == Approx(1.5 * rotated_bounds.minimum.x - 2.5).margin(0.05));
  CHECK(scaled_bounds.minimum.y == Approx(1.5 * rotated_bounds.minimum.y + 1.5).margin(0.05));
  CHECK(scaled_bounds.minimum.z == Approx(1.5 * rotated_bounds.minimum.z - 1).margin(0.05));
  states.push_back(ui.snapshot());

  ui.tree(revolve.value(), true);
  REQUIRE(ui.window.feature_edit_tools().active());
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/extent/angle_deg"), "90");
  CHECK(ui.snapshot() == states.back());
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.capture("combined-revolve-pattern-edit-preview");
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.window.session().part_document()->revolve_features().back().id() == revolve);
  CHECK(ui.summary(source).volume_mm3 == Approx(initial_volume / 2).margin(1e-5));
  CHECK(ui.summary(result).solid_count == 4);
  CHECK(ui.summary(result).volume_mm3 == Approx(2 * initial_volume * scale_volume).margin(1e-4));
  states.push_back(ui.snapshot());

  ui.tree(pattern.value(), true);
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/count_parameter"), "1");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature_edit.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.number(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/count_parameter"), "6");
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Six transformed quarter sleeves");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.window.session().part_document()->circular_pattern_features().back().id() == pattern);
  CHECK(ui.summary(result).solid_count == 6);
  CHECK(ui.summary(result).volume_mm3 == Approx(3 * initial_volume * scale_volume).margin(1e-4));
  states.push_back(ui.snapshot());
  ui.history(states);

  ui.command("measure_body");
  const auto& targets = ui.window.measure_tools().targets();
  const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
    return target.body && *target.body == result;
  });
  REQUIRE(found != targets.end());
  ui.data(ui.widget<QComboBox>("blcad.measure.first"), static_cast<int>(found - targets.begin()));
  REQUIRE(ui.window.measure_tools().result());
  CHECK(ui.window.measure_tools().result()->solid_count == 6);
  CHECK(ui.window.measure_tools().result()->volume_mm3 ==
        Approx(3 * initial_volume * scale_volume).margin(1e-4));
  ui.button("blcad.measure.copy");
  CHECK_FALSE(QApplication::clipboard()->text().isEmpty());
  ui.button("blcad.measure.close");
  CHECK(ui.snapshot() == states.back());
  ui.roundtrip(directory.filePath("revolved-pattern.blcad.json"));
  ui.command("fit", 2);
  ui.capture("combined-revolve-pattern");
  ui.idle();
}

TEST_CASE("Combined UI authors a bounded Planar joint drives all coordinates and branches motion",
          "[gui][integration][shell-combined-workflow][shell-combined-planar-motion]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  // Frame-capable seats currently come from the retained circular-hole contract.
  // Use the public definition import; do not synthesize missing Hole UI coverage.
  const auto definition_path =
      QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/revolute_joint.blcad.project.json";
  const auto definition_bytes = ui.bytes(definition_path);
  ui.new_document("Planar motion workflow", true);
  ui.file("assembly_import", definition_path);
  REQUIRE(ui.window.session().project()->part_documents().size() == 1);
  const auto definition = ui.window.session().project()->part_documents().front().id();
  std::vector<ComponentInstanceId> instances;
  for (int index = 0; index < 2; ++index) {
    ui.command("assembly_insert_component");
    ui.data(ui.widget<QComboBox>("blcad.assembly.definition"),
            QString::fromStdString(definition.value()));
    ui.text(ui.widget<QLineEdit>("blcad.assembly.name"),
            index == 0 ? "Fixed plate" : "Moving plate");
    ui.number(ui.widget<QDoubleSpinBox>("blcad.assembly.placement.0"), QString::number(index * 35));
    ui.number(ui.widget<QDoubleSpinBox>("blcad.assembly.placement.2"), QString::number(index * 12));
    if (index == 0)
      ui.click(ui.window.assembly_tools().grounding_editor());
    ui.button("blcad.assembly.apply");
    instances.push_back(
        ui.window.session().project()->assembly().component_instances().back().id());
  }
  std::vector<std::string> states{ui.snapshot()};
  ui.command("assembly_joint");
  ui.choose(ui.window.assembly_tools().joint_type_editor(),
            static_cast<int>(AssemblyJointType::Planar));
  const auto reference = [&](QComboBox& choice, const ComponentInstanceId& instance) {
    const auto& targets = ui.window.assembly_tools().targets();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
      return target.reference.component_instance() == instance &&
             target.reference.semantic_reference() == "feature.hole.seat";
    });
    REQUIRE(found != targets.end());
    ui.data(choice, static_cast<int>(found - targets.begin()));
  };
  reference(ui.widget<QComboBox>("blcad.assembly.first"), instances[0]);
  reference(ui.widget<QComboBox>("blcad.assembly.second"), instances[1]);
  auto& coordinates = ui.widget<QTableWidget>("blcad.assembly.slots");
  REQUIRE(coordinates.rowCount() == 3);
  ui.capture("combined-planar-limits-input");
  ui.cell(coordinates, 0, 2, "-50");
  ui.cell(coordinates, 0, 3, "50");
  ui.cell(coordinates, 1, 2, "-40");
  ui.cell(coordinates, 1, 3, "40");
  ui.cell(coordinates, 2, 2, "-90");
  ui.cell(coordinates, 2, 3, "90");
  ui.cell(coordinates, 0, 2, "60");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.cell(coordinates, 0, 2, "-50");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.assembly.apply");
  const auto joint = ui.window.session().project()->assembly().joints().back().id();
  CHECK(ui.window.session().project()->assembly().joints().back().type() ==
        AssemblyJointType::Planar);
  const auto& joint_coordinates =
      ui.window.session().project()->assembly().joints().back().coordinate_slots();
  REQUIRE(joint_coordinates.size() == 3);
  for (int index = 0; index < 3; ++index) {
    const double limit = index == 0 ? 50 : index == 1 ? 40 : 90;
    REQUIRE(joint_coordinates[index].lower_limit());
    REQUIRE(joint_coordinates[index].upper_limit());
    const auto scalar = [index](const Quantity& value) {
      return index == 2 ? value.degrees() : value.millimeters();
    };
    CHECK(scalar(*joint_coordinates[index].lower_limit()) == -limit);
    CHECK(scalar(*joint_coordinates[index].upper_limit()) == limit);
  }
  states.push_back(ui.snapshot());
  const auto motion = [&](AssemblyJointCoordinateRole role, const QString& value) {
    ui.command("assembly_motion");
    ui.data(ui.widget<QComboBox>("blcad.assembly.joint"), QString::fromStdString(joint.value()));
    ui.data(ui.window.assembly_tools().coordinate_editor(), static_cast<int>(role));
    ui.number(ui.widget<QDoubleSpinBox>("blcad.assembly.value"), value);
    REQUIRE(ui.window.viewport().feature_preview_active());
    CHECK(ui.snapshot() == states.back());
  };
  motion(AssemblyJointCoordinateRole::TranslationU, "35");
  auto& slider = ui.window.assembly_tools().slider();
  ui.click(slider);
  ui.key(slider, Qt::Key_End);
  CHECK(ui.widget<QDoubleSpinBox>("blcad.assembly.value").value() == 50);
  ui.key(slider, Qt::Key_Home);
  CHECK(ui.widget<QDoubleSpinBox>("blcad.assembly.value").value() == -50);
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.assembly.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  motion(AssemblyJointCoordinateRole::TranslationU, "30");
  ui.button("blcad.assembly.apply");
  states.push_back(ui.snapshot());
  motion(AssemblyJointCoordinateRole::TranslationV, "20");
  ui.button("blcad.assembly.apply");
  states.push_back(ui.snapshot());
  motion(AssemblyJointCoordinateRole::RotationNormal, "90");
  ui.capture("combined-planar-motion-preview");
  ui.button("blcad.assembly.apply");
  const auto& assembly = ui.window.session().project()->assembly();
  const auto* driven = assembly.find_joint(joint);
  REQUIRE(driven);
  CHECK(driven->find_coordinate_slot(AssemblyJointCoordinateRole::TranslationU)
            ->value()
            .millimeters() == 30);
  CHECK(driven->find_coordinate_slot(AssemblyJointCoordinateRole::TranslationV)
            ->value()
            .millimeters() == 20);
  CHECK(driven->find_coordinate_slot(AssemblyJointCoordinateRole::RotationNormal)
            ->value()
            .degrees() == 90);
  CHECK(assembly.find_component_instance(instances[0])->transform() == identity_rigid_transform());
  const auto posed = assembly.find_component_instance(instances[1])->transform();
  CHECK(posed.translation_mm.x == Approx(30).margin(1e-5));
  CHECK(posed.translation_mm.y == Approx(20).margin(1e-5));
  CHECK(posed.translation_mm.z == Approx(0).margin(1e-5));
  CHECK(posed.rotation_deg.z == Approx(90).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.command("undo");
  CHECK(ui.snapshot() == states[states.size() - 2]);
  states.pop_back();
  motion(AssemblyJointCoordinateRole::RotationNormal, "-45");
  ui.button("blcad.assembly.apply");
  CHECK_FALSE(ui.window.session().can_redo());
  const auto branched = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == states.back());
  ui.command("redo");
  CHECK(ui.snapshot() == branched);
  ui.command("assembly_analysis");
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 1);
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.clearance"), "100");
  ui.button("blcad.inspect.run");
  auto& rows = ui.widget<QTableWidget>("blcad.inspect.results");
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text().startsWith("Distance:"));
  bool parsed = false;
  const double clearance = rows.item(0, 2)->text().section(' ', 1, 1).toDouble(&parsed);
  REQUIRE(parsed);
  // The imported 10x20 plate is centered at its seat. At U=30, V=20 and -45°,
  // its nearest corner lies diagonally beyond the fixed plate's (5,10) corner.
  const double expected_clearance = std::hypot(25 - 15 / std::sqrt(2), 10 - 5 / std::sqrt(2));
  CHECK(clearance == Approx(expected_clearance).margin(1e-5));
  ui.button("blcad.inspect.close");
  CHECK(ui.snapshot() == branched);
  ui.roundtrip(directory.filePath("planar-motion.blcad.project.json"));
  for (const auto& [command, name] : std::vector<std::pair<QString, QString>>{
           {"export_assembly_flat_step", "planar-flat.step"},
           {"export_assembly_structured_step", "planar-structured.step"}}) {
    ui.file(command, directory.filePath(name));
    CHECK(ui.bytes(directory.filePath(name)).startsWith("ISO-10303-21;"));
    CHECK(ui.snapshot() == branched);
    CHECK_FALSE(ui.window.session().dirty());
  }
  CHECK(ui.bytes(definition_path) == definition_bytes);
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
  CHECK(rows.item(0, 2)->text().startsWith("Distance: 10"));
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 2);
  CHECK(rows.rowCount() == 0);
  ui.button("blcad.inspect.run");
  REQUIRE(rows.rowCount() == 1);
  CHECK(rows.item(0, 2)->text().startsWith("Separated:"));
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

TEST_CASE(
    "Combined UI workplane editing rejects cyclic references and preserves downstream history",
    "[gui][integration][shell-combined-workflow][block136-construction-edit][shell-combined-"
    "workplane-edit]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Workplane editing workflow");
  const auto plane = ui.offset(10);
  const auto sketch = ui.rectangle(plane.value(), {20, 20}, {50, 40});
  const auto body = ui.extrude(6);
  const auto initial = ui.snapshot();
  CHECK(ui.bounds(body).minimum.z == Approx(10).margin(1e-5));
  ui.tree(plane.value(), true);
  REQUIRE(ui.window.construction_tools().active());
  auto& binding = ui.widget<QComboBox>("blcad.construction.parameter");
  ui.choose(binding, 0); // Provision another parameter; preserve the existing shared binding.
  auto& offset = ui.widget<QDoubleSpinBox>("blcad.construction.offset");
  ui.click(offset);
  ui.key(offset, Qt::Key_A, {}, Qt::ControlModifier);
  ui.key(offset, Qt::Key_2, "2");
  ui.key(offset, Qt::Key_3, "3");
  // Deliberately leave numeric text pending until the Apply mouse event.
  CHECK(ui.snapshot() == initial);
  ui.button("blcad.construction.apply");
  ui.idle();
  const auto edited = ui.snapshot();
  CHECK(edited != initial);
  CHECK(ui.bounds(body).minimum.z == Approx(23).margin(1e-5));
  CHECK(ui.summary(body).volume_mm3 == Approx(3600).margin(1e-5));
  CHECK(ui.window.session().part_document()->find_sketch(sketch)->workplane() == plane);

  ui.command("create_construction");
  ui.choose(ui.widget<QComboBox>("blcad.construction.kind"),
            static_cast<int>(ShellConstructionTools::Kind::FacePlane));
  ui.button("blcad.construction.apply");
  const auto face = ui.window.session().part_document()->derived_workplanes().back().id();
  const auto with_face = ui.snapshot();
  ui.tree(plane.value(), true);
  ui.data(ui.widget<QComboBox>("blcad.construction.plane"), QString::fromStdString(face.value()));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.construction.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == with_face);
  // Recover in the same task, then cancel using Escape from an actual task input.
  ui.data(ui.widget<QComboBox>("blcad.construction.plane"), "datum.xy");
  CHECK(ui.widget<QPushButton>("blcad.construction.apply").isEnabled());
  ui.key(ui.widget<QComboBox>("blcad.construction.plane"), Qt::Key_Escape);
  ui.idle();
  CHECK(ui.snapshot() == with_face);
  ui.tree(plane.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.construction.name"), "Machining datum");
  ui.capture("combined-workplane-edit");
  ui.button("blcad.construction.apply");
  const auto renamed = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == with_face);
  ui.command("undo");
  CHECK(ui.snapshot() == edited);
  ui.command("undo");
  CHECK(ui.snapshot() == initial);
  CHECK(ui.bounds(body).minimum.z == Approx(10).margin(1e-5));
  for (int i = 0; i < 3; ++i)
    ui.command("redo");
  CHECK(ui.snapshot() == renamed);
  CHECK(ui.bounds(body).minimum.z == Approx(23).margin(1e-5));
  ui.roundtrip(directory.filePath("workplane-edit.blcad.json"));
  CHECK(ui.bounds(body).minimum.z == Approx(23).margin(1e-5));
  const auto exported = ui.snapshot();
  const auto step = directory.filePath("workplane-edit.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == exported);
  ui.idle();
}

TEST_CASE("Combined UI semantic construction chain updates downstream bodies and preserves history",
          "[gui][integration][shell-combined-workflow][block136-construction-relations][shell-"
          "combined-construction]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Semantic construction workflow");
  ui.rectangle("datum.xy", {20, 20}, {50, 40});
  const auto base = ui.extrude(6);
  const auto feature = ui.window.session().part_document()->features().front().id();
  const auto mode = [&](ShellConstructionTools::Kind kind) {
    ui.command("create_construction");
    ui.choose(ui.widget<QComboBox>("blcad.construction.kind"), static_cast<int>(kind));
  };
  const auto element = [&](const QString& role, const FeatureId& source, int value) {
    auto& combo = ui.widget<QComboBox>("blcad.construction." + role);
    int row = -1;
    for (int i = 0; i < combo.count(); ++i)
      if (combo.itemData(i).toString().toStdString() == source.value() &&
          combo.itemData(i, Qt::UserRole + 1).toInt() == value)
        row = i;
    ui.choose(combo, row);
  };
  mode(ShellConstructionTools::Kind::PointOnVertex);
  element("vertex", feature, static_cast<int>(SemanticVertex::TopFrontRight));
  ui.button("blcad.construction.apply");
  const auto vertex = ui.window.session().part_document()->construction_points().back().id();
  mode(ShellConstructionTools::Kind::PointOnEdge);
  element("edge", feature, static_cast<int>(SemanticEdge::TopFront));
  ui.button("blcad.construction.apply");
  const auto midpoint = ui.window.session().part_document()->construction_points().back().id();
  mode(ShellConstructionTools::Kind::ParallelEdgeThroughPoint);
  element("edge", feature, static_cast<int>(SemanticEdge::FrontRight));
  ui.data(ui.widget<QComboBox>("blcad.construction.point1"),
          QString::fromStdString(midpoint.value()));
  ui.button("blcad.construction.apply");
  const auto line = ui.window.session().part_document()->construction_lines().back().id();
  mode(ShellConstructionTools::Kind::AxisFromLine);
  ui.data(ui.widget<QComboBox>("blcad.construction.line"), QString::fromStdString(line.value()));
  ui.button("blcad.construction.apply");
  const auto axis = ui.window.session().part_document()->datum_axes().back().id();
  mode(ShellConstructionTools::Kind::ParallelPlaneThroughPoint);
  ui.data(ui.widget<QComboBox>("blcad.construction.plane"), "datum.xy");
  ui.data(ui.widget<QComboBox>("blcad.construction.point1"),
          QString::fromStdString(vertex.value()));
  ui.button("blcad.construction.apply");
  const auto plane =
      ui.window.session().part_document()->construction_planes().back().workplane_id();
  const auto sketch = ui.rectangle(plane.value(), {20, 20}, {40, 40});
  const auto child = ui.extrude(4);
  const auto child_feature = ui.window.session().part_document()->features().back().id();
  CHECK(ui.summary(base).volume_mm3 == Approx(3600).margin(1e-5));
  CHECK(ui.summary(child).volume_mm3 == Approx(1600).margin(1e-5));
  CHECK(ui.bounds(child).minimum.z == Approx(6).margin(1e-5));
  std::vector<std::string> states{ui.snapshot()};

  ui.tree(vertex.value(), true);
  element("vertex", feature, static_cast<int>(SemanticVertex::BottomBackLeft));
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.window.viewport().feature_preview_replaces("construction-plane/" + plane.value()));
  ui.button("blcad.construction.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.bounds(child).minimum.x == Approx(40).margin(1e-5));
  CHECK(ui.bounds(child).minimum.y == Approx(40).margin(1e-5));
  CHECK(ui.bounds(child).minimum.z == Approx(0).margin(1e-5));
  CHECK(ui.bounds(child).maximum.z == Approx(4).margin(1e-5));
  CHECK(ui.summary(child).volume_mm3 == Approx(1600).margin(1e-5));
  CHECK(ui.window.session().part_document()->find_sketch(sketch)->workplane() == plane);
  ui.tree(vertex.value(), true);
  element("vertex", child_feature, static_cast<int>(SemanticVertex::TopFrontRight));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.construction.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  element("vertex", feature, static_cast<int>(SemanticVertex::BottomBackLeft));
  CHECK(ui.widget<QPushButton>("blcad.construction.apply").isEnabled());
  ui.key(ui.widget<QComboBox>("blcad.construction.vertex"), Qt::Key_Escape);
  ui.idle();
  ui.tree(midpoint.value(), true);
  element("edge", feature, static_cast<int>(SemanticEdge::BottomBack));
  CHECK(ui.window.viewport().feature_preview_replaces("datum-axis/" + axis.value()));
  ui.button("blcad.construction.apply");
  states.push_back(ui.snapshot());
  ui.tree(line.value(), true);
  element("edge", feature, static_cast<int>(SemanticEdge::TopRight));
  // Long definition labels must not force the task inputs off the left edge.
  auto* panel = ui.widget<QComboBox>("blcad.construction.kind").parentWidget();
  while (panel && !qobject_cast<QScrollArea*>(panel))
    panel = panel->parentWidget();
  REQUIRE(panel);
  CHECK(static_cast<QScrollArea*>(panel)->horizontalScrollBar()->maximum() == 0);
  ui.capture("combined-construction-relations");
  ui.button("blcad.construction.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.window.session().part_document()->find_datum_axis(axis)->source_construction_line() ==
        line);
  for (std::size_t i = states.size() - 1; i > 0; --i) {
    ui.command("undo");
    CHECK(ui.snapshot() == states[i - 1]);
  }
  CHECK(ui.bounds(child).minimum.z == Approx(6).margin(1e-5));
  for (std::size_t i = 1; i < states.size(); ++i) {
    ui.command("redo");
    CHECK(ui.snapshot() == states[i]);
  }
  CHECK(ui.bounds(child).minimum.z == Approx(0).margin(1e-5));
  ui.roundtrip(directory.filePath("semantic-chain.blcad.json"));
  const auto before_export = ui.snapshot();
  const auto step = directory.filePath("semantic-chain.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == before_export);
  ui.idle();
}

TEST_CASE("Combined UI construction grips rotate a workplane and move its downstream solid",
          "[gui][integration][shell-combined-workflow][shell-combined-manipulators]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Construction grips workflow");
  const auto drag = [&](const std::string& id, const QString& value) {
    INFO(id);
    ui.window.construction_tools().refresh_presentation();
    const auto handles = ui.window.construction_tools().manipulators().presentations();
    const auto found = std::find_if(handles.begin(), handles.end(),
                                    [&](const auto& h) { return h.handle_id == id; });
    REQUIRE(found != handles.end());
    const QPointF p(found->endpoint.x, found->endpoint.y);
    REQUIRE(ui.window.viewport().rect().contains(p.toPoint()));
    ui.mouse(ui.window.viewport(), p, QEvent::MouseButtonPress);
    ui.flush();
    REQUIRE(ui.window.construction_tools().manipulators().active_handle_id() == id);
    auto& numeric = ui.widget<QLineEdit>("blcad.construction.grip_numeric");
    ui.key(numeric, Qt::Key_A, {}, Qt::ControlModifier);
    ui.key(numeric, 0, value);
    REQUIRE(ui.window.construction_tools().manipulators().numeric_override_active());
    // Numeric input stays authoritative even when release has a different position.
    ui.mouse(ui.window.viewport(), p + QPointF(17, 9), QEvent::MouseButtonRelease);
    ui.flush();
    REQUIRE(ui.window.construction_tools().manipulators().active_handle_id().empty());
  };
  // Headless mapping uses a model span; retain the fitted native OCCT zoom factor.
  auto camera = ui.window.viewport().camera_bookmark();
  camera.eye = {150, 150, 150};
  camera.target = {0, 0, 0};
  if (QApplication::platformName() != "xcb")
    camera.scale = 200;
  REQUIRE(ui.window.viewport().restore_camera_bookmark(camera));
  ui.command("create_construction");
  ui.choose(ui.widget<QComboBox>("blcad.construction.kind"),
            static_cast<int>(ShellConstructionTools::Kind::Plane));
  const auto before = ui.snapshot();
  drag("construction.position.z", "15 mm");
  ui.choose(ui.widget<QComboBox>("blcad.construction.manipulation"), 1);
  drag("construction.rotation.x", "30 deg");
  CHECK(ui.snapshot() == before);
  ui.button("blcad.construction.apply");
  const auto plane = ui.window.session().part_document()->construction_planes().back().id();
  const auto sketch = ui.rectangle(plane.value(), {20, 20}, {50, 40});
  const auto body = ui.extrude(6);
  const auto volume = ui.summary(body).volume_mm3;
  CHECK(volume == Approx(3600).epsilon(1e-8));
  const auto lower = ui.bounds(body).minimum.z;
  const auto modeled = ui.snapshot();
  ui.tree(plane.value(), true);
  ui.choose(ui.widget<QComboBox>("blcad.construction.manipulation"), 0);
  drag("construction.position.z", "45 mm");
  CHECK(ui.snapshot() == modeled);
  CHECK(ui.window.viewport().feature_preview_replaces("body/" + body.value()));
  ui.button("blcad.construction.apply");
  CHECK(ui.window.session().part_document()->find_sketch(sketch)->workplane() ==
        DatumPlaneId(plane.value()));
  CHECK(ui.summary(body).volume_mm3 == Approx(volume).epsilon(1e-8));
  CHECK(ui.bounds(body).minimum.z == Approx(lower + 30).margin(1e-5));
  const auto edited = ui.snapshot();
  ui.command("undo");
  CHECK(ui.snapshot() == modeled);
  CHECK(ui.bounds(body).minimum.z == Approx(lower).margin(1e-5));
  ui.command("redo");
  CHECK(ui.snapshot() == edited);
  ui.idle();
  ui.capture("construction-grips-downstream-solid");
  const auto saved = directory.filePath("grips.blcad");
  ui.roundtrip(saved);
  const auto bytes = ui.bytes(saved);
  const auto step = directory.filePath("grips.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.bytes(saved) == bytes);
  CHECK(ui.snapshot() == edited);
  ui.idle();
}

TEST_CASE("Combined UI creates a spatial chain sweeps it edits endpoints rejects collapse and "
          "restores history",
          "[gui][integration][shell-combined-workflow][shell-combined-sketch3d][shell-sketch3d-"
          "grips-workflow][shell-sketch3d-placement-workflow][shell-sketch3d-direction-workflow]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Spatial chain workflow");
  const auto profile = ui.rectangle("datum.xy", {-2, -2}, {2, 2});
  const auto initial = ui.snapshot();
  ui.command("sketch_3d");
  ui.command("fit", 2);
  auto placement_camera = ui.window.viewport().camera_bookmark();
  placement_camera.eye = {150, 150, 165};
  placement_camera.target = {0, 0, 15};
  placement_camera.projection = GuiViewportProjection::Orthographic;
  if (QApplication::platformName() != "xcb")
    placement_camera.scale = 120;
  REQUIRE(ui.window.viewport().restore_camera_bookmark(placement_camera));
  const auto coordinate = [&](const char* axis, double value) {
    auto& spin = ui.widget<QDoubleSpinBox>(QStringLiteral("blcad.sketch3d.") + axis);
    auto* editor = spin.findChild<QLineEdit*>();
    REQUIRE(editor);
    ui.text(*editor, QString::number(value));
  };
  auto& points = ui.widget<QComboBox>("blcad.sketch3d.point");
  for (int index = 0; index < 4; ++index) {
    if (index == 0)
      ui.choose(points, 0);
    else if (index == 3) {
      ui.data(ui.widget<QComboBox>("blcad.sketch3d.placement_anchor"), "point.3");
      const auto count = points.count();
      ui.text(ui.widget<QLineEdit>("blcad.sketch3d.distance"), "10");
      ui.button("blcad.sketch3d.stage_direction");
      CHECK(points.count() == count);
      CHECK(ui.snapshot() == initial);
      ui.button("blcad.sketch3d.preview");
      CHECK_FALSE(ui.window.viewport().feature_preview_active());
      ui.text(ui.widget<QLineEdit>("blcad.sketch3d.azimuth"), "0");
      ui.text(ui.widget<QLineEdit>("blcad.sketch3d.elevation"), "90");
      ui.reveal(ui.widget<QPushButton>("blcad.sketch3d.stage_direction"));
      ui.capture("sketch3d-distance-direction-input");
      ui.button("blcad.sketch3d.stage_direction");
      CHECK(points.count() == count + 1);
      CHECK(points.currentData().toString() == "point.4");
      CHECK(ui.snapshot() == initial);
    } else {
      ui.data(ui.widget<QComboBox>("blcad.sketch3d.placement_anchor"), "point.1");
      ui.choose(ui.widget<QComboBox>("blcad.sketch3d.lock"), 3);
      ui.choose(ui.widget<QComboBox>("blcad.sketch3d.pick"), 5);
      const auto p = checked(ui.window.viewport().model_to_screen({0, 0, index * 10.0}));
      REQUIRE(ui.window.viewport().rect().contains(QPoint(qRound(p.x), qRound(p.y))));
      QMouseEvent hover(QEvent::MouseMove, {p.x, p.y},
                        ui.window.viewport().mapToGlobal(QPoint(qRound(p.x), qRound(p.y))),
                        Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(&ui.window.viewport(), &hover);
      ui.flush();
      REQUIRE(ui.widget<QWidget>("blcad.sketch3d.placement_overlay").isVisible());
      if (index == 2)
        ui.capture("sketch3d-point-placement-hover");
      const auto count = points.count();
      ui.click_at(ui.window.viewport(), {p.x, p.y});
      CHECK(points.count() == count + 1);
      CHECK(points.currentData().toString() == "point." + QString::number(index + 1));
      CHECK(ui.snapshot() == initial);
      ui.choose(ui.widget<QComboBox>("blcad.sketch3d.pick"), 0);
    }
    // Refine placed coordinates through the existing typed stage so the same
    // Sweep dimensions remain exact across native pixel quantization.
    coordinate("x", 0);
    coordinate("y", 0);
    coordinate("z", index * 10);
    ui.button("blcad.sketch3d.stage_point");
  }
  for (int index = 1; index <= 3; ++index) {
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.start"), "point." + QString::number(index));
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.end"), "point." + QString::number(index + 1));
    ui.button("blcad.sketch3d.stage_line");
  }
  ui.button("blcad.sketch3d.preview");
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == initial);
  ui.command("fit", 2);
  ui.capture("sketch3d-chain-preview");
  ui.button("blcad.sketch3d.apply");
  const auto spatial = ui.window.session().part_document()->sketches_3d().back().id();
  const auto created = ui.snapshot();
  ui.command("path_curve");
  for (int index = 1; index <= 3; ++index) {
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    ui.choose(source,
              source.findText("3D Sketch 1 / line." + QString::number(index), Qt::MatchStartsWith));
    ui.button("blcad.spatial.add");
  }
  ui.button("blcad.feature.apply");
  const auto path = ui.window.session().part_document()->path_curves().back().id();
  const auto with_path = ui.snapshot();
  ui.command("sweep");
  auto& source = ui.widget<QComboBox>("blcad.spatial.source");
  ui.choose(source,
            source.findText(QString::fromStdString(
                                ui.window.session().part_document()->find_sketch(profile)->name()) +
                                " /",
                            Qt::MatchStartsWith));
  ui.data(ui.widget<QComboBox>("blcad.spatial.path"), QString::fromStdString(path.value()));
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(body).solid_count == 1);
  CHECK(ui.summary(body).volume_mm3 == Approx(480).margin(1e-6));
  const auto swept = ui.snapshot();
  ui.tree(spatial.value(), true);
  REQUIRE(ui.window.sketch_3d_tools().active());
  auto camera = ui.window.viewport().camera_bookmark();
  camera.eye = {150, 150, 165};
  camera.target = {0, 0, 15};
  camera.projection = GuiViewportProjection::Orthographic;
  if (QApplication::platformName() != "xcb")
    camera.scale = 120;
  REQUIRE(ui.window.viewport().restore_camera_bookmark(camera));
  auto& pick = ui.widget<QComboBox>("blcad.sketch3d.pick");
  ui.choose(pick, 1);
  const auto endpoint = checked(ui.window.viewport().model_to_screen({0, 0, 30}));
  ui.click_at(ui.window.viewport(), {endpoint.x, endpoint.y});
  REQUIRE(points.currentData().toString() == "point.4");
  ui.choose(pick, 0);
  ui.click(ui.widget<QCheckBox>("blcad.sketch3d.grips"));
  ui.choose(ui.widget<QComboBox>("blcad.sketch3d.lock"), 3);
  const auto drag_point = [&](const QString& value) {
    ui.window.sketch_3d_tools().refresh_presentation();
    const auto handles = ui.window.sketch_3d_tools().manipulators().presentations();
    REQUIRE(handles.size() == 1);
    const QPointF point(handles.front().endpoint.x, handles.front().endpoint.y);
    REQUIRE(ui.window.viewport().rect().contains(point.toPoint()));
    ui.mouse(ui.window.viewport(), point, QEvent::MouseButtonPress);
    ui.flush();
    REQUIRE(ui.window.sketch_3d_tools().manipulators().active_handle_id() == "sketch3d.position.z");
    auto& numeric = ui.widget<QLineEdit>("blcad.sketch3d.grip_numeric");
    ui.key(numeric, Qt::Key_A, {}, Qt::ControlModifier);
    ui.key(numeric, 0, value);
    REQUIRE(ui.window.sketch_3d_tools().manipulators().numeric_override_active());
    ui.mouse(ui.window.viewport(), point + QPointF(17, 9), QEvent::MouseButtonRelease);
    ui.flush();
    CHECK(ui.window.sketch_3d_tools().manipulators().active_handle_id().empty());
  };
  drag_point("20 mm");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.sketch3d.apply").isEnabled());
  CHECK(ui.snapshot() == swept);
  drag_point("40 mm");
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.window.viewport().feature_preview_replaces("body/" + body.value()));
  CHECK(ui.snapshot() == swept);
  ui.command("fit", 2);
  ui.capture("sketch3d-downstream-sweep-preview");
  ui.button("blcad.sketch3d.apply");
  const auto edited = ui.snapshot();
  CHECK(ui.summary(body).volume_mm3 == Approx(640).margin(1e-6));
  CHECK(ui.bounds(body).maximum.z == Approx(40).margin(1e-5));
  CHECK(ui.window.session().part_document()->path_curves().back().segments().back().entity() ==
        SketchEntityId("line.3"));
  for (const auto& expected : {swept, with_path, created, initial}) {
    ui.command("undo");
    CHECK(ui.snapshot() == expected);
  }
  for (int index = 0; index < 4; ++index)
    ui.command("redo");
  CHECK(ui.snapshot() == edited);
  CHECK(ui.summary(body).volume_mm3 == Approx(640).margin(1e-6));
  ui.tree(spatial.value(), true);
  ui.data(points, "point.2");
  coordinate("x", 3);
  ui.button("blcad.sketch3d.stage_point");
  ui.key(ui.widget<QDoubleSpinBox>("blcad.sketch3d.x"), Qt::Key_Escape);
  CHECK(ui.snapshot() == edited);
  ui.idle();
  const auto saved = directory.filePath("spatial.blcad");
  ui.roundtrip(saved);
  const auto source_bytes = ui.bytes(saved);
  const auto step = directory.filePath("spatial.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.bytes(saved) == source_bytes);
  CHECK(ui.snapshot() == edited);
  ui.idle();
}

TEST_CASE(
    "Combined UI copies planar endpoints into a spatial path and recovers a downstream Sweep edit",
    "[gui][integration][shell-combined-workflow][shell-combined-projected-sweep]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Planar point copy workflow");
  // Keep the hover beyond the origin-axis snap tolerance in the initial view.
  const auto source = ui.rectangle("datum.xz", {0, 0}, {20, 30});
  const auto profile = ui.rectangle("datum.xy", {-2, -2}, {2, 2});
  const auto edge =
      ui.window.session().part_document()->find_sketch(source)->line_segments().back().id();
  const auto initial = ui.snapshot();
  ui.command("sketch_3d");
  auto& points = ui.widget<QComboBox>("blcad.sketch3d.point");
  auto& sources = ui.widget<QComboBox>("blcad.sketch3d.projection_source");
  for (auto kind :
       {SketchReferenceTargetKind::LineSegmentEnd, SketchReferenceTargetKind::LineSegmentStart}) {
    int row = -1;
    for (int i = 1; i < sources.count(); ++i)
      if (sources.itemData(i).toString() == QString::fromStdString(source.value()) &&
          sources.itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(kind) &&
          sources.itemData(i, Qt::UserRole + 2).toString() == QString::fromStdString(edge.value()))
        row = i;
    REQUIRE(row > 0);
    ui.choose(sources, row);
    auto& consent = ui.widget<QCheckBox>("blcad.sketch3d.projection_copy");
    CHECK_FALSE(consent.isChecked());
    const int count = points.count();
    ui.button("blcad.sketch3d.stage_projection");
    CHECK(points.count() == count);
    CHECK(ui.snapshot() == initial);
    ui.click(consent);
    if (kind == SketchReferenceTargetKind::LineSegmentStart) {
      ui.reveal(ui.widget<QPushButton>("blcad.sketch3d.stage_projection"));
      ui.capture("sketch3d-planar-copy-controls");
    }
    ui.button("blcad.sketch3d.stage_projection");
    CHECK(points.count() == count + 1);
    CHECK_FALSE(consent.isChecked());
    CHECK(ui.snapshot() == initial);
  }
  auto& receipt = ui.widget<QLabel>("blcad.sketch3d.projection_receipts");
  CHECK(receipt.text().contains("point.1"));
  CHECK(receipt.text().contains("point.2"));
  CHECK(receipt.text().contains(QString::fromStdString(source.value())));
  ui.reveal(receipt);
  ui.capture("sketch3d-planar-copy-provenance");
  ui.data(ui.widget<QComboBox>("blcad.sketch3d.placement_anchor"), "point.2");
  ui.text(ui.widget<QLineEdit>("blcad.sketch3d.distance"), "10");
  ui.text(ui.widget<QLineEdit>("blcad.sketch3d.azimuth"), "0");
  ui.text(ui.widget<QLineEdit>("blcad.sketch3d.elevation"), "90");
  ui.button("blcad.sketch3d.stage_direction");
  for (int index = 1; index <= 2; ++index) {
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.start"), "point." + QString::number(index));
    ui.data(ui.widget<QComboBox>("blcad.sketch3d.end"), "point." + QString::number(index + 1));
    ui.button("blcad.sketch3d.stage_line");
  }
  ui.button("blcad.sketch3d.preview");
  CHECK(ui.snapshot() == initial);
  ui.button("blcad.sketch3d.apply");
  const auto spatial = ui.window.session().part_document()->sketches_3d().back().id();
  const auto created = ui.snapshot();
  ui.command("path_curve");
  for (int index = 1; index <= 2; ++index) {
    auto& input = ui.widget<QComboBox>("blcad.spatial.source");
    ui.choose(input,
              input.findText("3D Sketch 1 / line." + QString::number(index), Qt::MatchStartsWith));
    ui.button("blcad.spatial.add");
  }
  ui.button("blcad.feature.apply");
  const auto path = ui.window.session().part_document()->path_curves().back().id();
  const auto with_path = ui.snapshot();
  ui.command("sweep");
  auto& input = ui.widget<QComboBox>("blcad.spatial.source");
  ui.choose(input,
            input.findText(QString::fromStdString(
                               ui.window.session().part_document()->find_sketch(profile)->name()) +
                               " /",
                           Qt::MatchStartsWith));
  ui.data(ui.widget<QComboBox>("blcad.spatial.path"), QString::fromStdString(path.value()));
  ui.button("blcad.feature.apply");
  const auto body = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(body).volume_mm3 == Approx(640).margin(1e-6));
  const auto swept = ui.snapshot();
  ui.tree(spatial.value(), true);
  ui.data(points, "point.3");
  auto& z = ui.widget<QDoubleSpinBox>("blcad.sketch3d.z");
  ui.number(z, "30");
  ui.button("blcad.sketch3d.stage_point");
  ui.button("blcad.sketch3d.preview");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.sketch3d.apply").isEnabled());
  CHECK(ui.snapshot() == swept);
  ui.number(z, "50");
  ui.button("blcad.sketch3d.stage_point");
  ui.button("blcad.sketch3d.preview");
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.sketch3d.apply");
  const auto edited = ui.snapshot();
  CHECK(ui.summary(body).volume_mm3 == Approx(800).margin(1e-6));
  CHECK(ui.bounds(body).maximum.z == Approx(50).margin(1e-6));
  for (const auto& expected : {swept, with_path, created, initial}) {
    ui.command("undo");
    CHECK(ui.snapshot() == expected);
  }
  for (int index = 0; index < 4; ++index)
    ui.command("redo");
  CHECK(ui.snapshot() == edited);
  ui.roundtrip(directory.filePath("planar-copy.blcad"));
  const auto step = directory.filePath("planar-copy.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == edited);
  ui.idle();
}

TEST_CASE("Combined UI parameter batches drive a rebound extrusion through formula recovery "
          "history and export",
          "[gui][integration][shell-combined-workflow][shell-combined-parameters]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Parameterized solid");
  ui.rectangle("datum.xy", {20, 20}, {50, 40});
  const auto body = ui.extrude(4);
  const auto feature = ui.window.session().part_document()->features().front().id();
  CHECK(ui.summary(body).volume_mm3 == Approx(2400).margin(1e-5));
  std::vector<std::string> states{ui.snapshot()};
  ui.command("parameters");
  const auto add_parameter = [&](const QString& name, const QString& value, int type,
                                 bool formula) {
    ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), QString());
    ui.text(ui.widget<QLineEdit>("blcad.parameters.name"), name);
    ui.choose(ui.widget<QComboBox>("blcad.parameters.type"), type);
    ui.choose(ui.widget<QComboBox>("blcad.parameters.mode"), formula ? 1 : 0);
    ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), value);
    ui.button("blcad.parameters.stage");
    const auto id = ui.widget<QComboBox>("blcad.parameters.parameter").currentData().toString();
    REQUIRE_FALSE(id.isEmpty());
    return id;
  };
  const auto depth = add_parameter("base_depth", "2", 0, false);
  const auto copies = add_parameter("copies", "3", 1, false);
  const auto angle = add_parameter("inclination", "-22.123456789012345", 2, false);
  const auto computed = add_parameter("driven_depth", "base_depth * copies", 0, true);
  add_parameter("depth_twice", "driven_depth * 2", 0, true);
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.window.viewport().feature_preview_active());
  ui.capture("parameter-manager-preview");
  ui.button("blcad.parameters.apply");
  ui.idle();
  states.push_back(ui.snapshot());
  CHECK(ui.window.session()
            .part_document()
            ->find_parameter(ParameterId(angle.toStdString()))
            ->value()
            .degrees() == -22.123456789012345);

  ui.tree(feature.value(), true);
  ui.data(ui.widget<QComboBox>("blcad.feature_edit.binding/length_parameter"), computed);
  CHECK_FALSE(ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/length_parameter").isEnabled());
  ui.button("blcad.feature_edit.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(3600).margin(1e-5));
  states.push_back(ui.snapshot());

  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), depth);
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "4");
  ui.button("blcad.parameters.stage");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), computed);
  CHECK(ui.widget<QLabel>("blcad.parameters.usage")
            .text()
            .contains(QString::fromStdString(feature.value())));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "depth_twice / 2");
  ui.button("blcad.parameters.stage");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.parameters.preview").isEnabled());
  CHECK(ui.widget<QLineEdit>("blcad.parameters.input").text() == "depth_twice / 2");
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "base_depth * (copies + 1)");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  REQUIRE(ui.widget<QPushButton>("blcad.parameters.apply").isEnabled());
  // New pending invalid input must revoke an earlier successful whole-model preview.
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "driven_depth + 1 mm");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.parameters.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.parameters.stage");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.parameters.preview").isEnabled());
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), copies);
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "1.5");
  ui.button("blcad.parameters.stage");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.parameters.preview").isEnabled());
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), computed);
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.summary(body).volume_mm3 == Approx(3600).margin(1e-5));
  ui.capture("parameter-manager-usage");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(9600).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  CHECK(ui.summary(body).volume_mm3 == Approx(9600).margin(1e-5));
  ui.command("parameters");
  add_parameter("discarded", "7", 0, false);
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("parameters.blcad"));
  CHECK(ui.summary(body).volume_mm3 == Approx(9600).margin(1e-5));
  const auto step = directory.filePath("parameters.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI hole workflow cuts a finished body then edits pattern parameters and adds a "
          "single hole",
          "[gui][integration][shell-combined-workflow][shell-combined-holes]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Machined hole plate");
  ui.rectangle("datum.xy", {-40, -30}, {40, 30});
  const auto body = ui.extrude(12);
  CHECK(ui.summary(body).volume_mm3 == Approx(57600).margin(1e-5));
  std::vector<std::string> states{ui.snapshot()};
  ui.command("fillet");
  auto& edges = ui.widget<QListWidget>("blcad.finishing.targets");
  REQUIRE(edges.count() > 0);
  ui.list(edges, 0);
  ui.scalar("1 mm");
  ui.button("blcad.feature.apply");
  const auto rounded_volume = ui.summary(body).volume_mm3;
  REQUIRE(rounded_volume < 57600);
  states.push_back(ui.snapshot());
  ui.tree(body.value());
  ui.command("hole");
  REQUIRE(ui.window.hole_tools().active());
  CHECK(ui.widget<QComboBox>("blcad.hole.target").currentData().toString() ==
        QString::fromStdString(body.value()));
  ui.choose(ui.widget<QComboBox>("blcad.hole.placement"), 1);
  ui.tree("datum.xz");
  CHECK(ui.widget<QComboBox>("blcad.hole.plane").currentData() == "datum.xz");
  ui.tree("datum.xy");
  CHECK(ui.widget<QComboBox>("blcad.hole.plane").currentData() == "datum.xy");
  ui.data(ui.widget<QComboBox>("blcad.hole.plane"), "datum.xy");
  ui.text(ui.widget<QLineEdit>("blcad.hole.diameter"), "6");
  ui.text(ui.widget<QLineEdit>("blcad.hole.radius"), "20");
  ui.text(ui.widget<QLineEdit>("blcad.hole.count"), "3.5");
  ui.button("blcad.hole.preview");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  CHECK(ui.widget<QLineEdit>("blcad.hole.count").text() == "3.5");
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.hole.count"), "4");
  ui.text(ui.widget<QLineEdit>("blcad.hole.angle"), "30.123456789012345");
  ui.button("blcad.hole.preview");
  REQUIRE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.capture("hole-circle-preview");
  ui.button("blcad.hole.apply");
  ui.idle();
  CHECK(ui.summary(body).volume_mm3 ==
        Approx(rounded_volume - 4 * std::numbers::pi * 9 * 12).margin(1e-5));
  states.push_back(ui.snapshot());
  const auto cut = ui.window.session().part_document()->features().back();
  const auto pattern = ui.window.session()
                           .part_document()
                           ->find_sketch(cut.input_sketch())
                           ->circular_hole_patterns()
                           .front();
  const auto pattern_parameters = ui.window.session().part_document()->parameter_count();
  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(pattern.count_parameter().value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "6");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(body).volume_mm3 ==
        Approx(rounded_volume - 6 * std::numbers::pi * 9 * 12).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.command("hole");
  ui.choose(ui.widget<QComboBox>("blcad.hole.placement"), 0);
  // Reuse the circle's diameter instead of creating or overwriting a parameter.
  ui.data(ui.widget<QComboBox>("blcad.hole.diameter_binding"),
          QString::fromStdString(pattern.hole_diameter_parameter().value()));
  CHECK(ui.widget<QLineEdit>("blcad.hole.diameter").isReadOnly());
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "1000");
  ui.button("blcad.hole.preview");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "0");
  ui.button("blcad.hole.preview");
  ui.capture("hole-single-bound-preview");
  ui.button("blcad.hole.apply");
  CHECK(ui.window.session().part_document()->parameter_count() == pattern_parameters);
  CHECK(ui.summary(body).volume_mm3 ==
        Approx(rounded_volume - 7 * std::numbers::pi * 9 * 12).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.command("hole");
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "10");
  ui.button("blcad.hole.preview");
  REQUIRE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  ui.button("blcad.hole.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.idle();
  ui.roundtrip(directory.filePath("machined-holes.blcad"));
  CHECK(ui.summary(body).volume_mm3 ==
        Approx(rounded_volume - 7 * std::numbers::pi * 9 * 12).margin(1e-5));
  const auto step = directory.filePath("machined-holes.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI filters constrain model selection while finishing and hole tasks collect "
          "references",
          "[gui][integration][shell-combined-workflow][shell-combined-selection-filters]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Filtered machining");
  const auto sketch = ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto body = ui.extrude(12);
  const auto depth = ui.window.session().part_document()->features().back().length_parameter();
  auto& filter = ui.widget<QComboBox>("blcad.selection.filter");
  ui.choose(filter, 1); // Profiles
  ui.tree(sketch.value());
  REQUIRE(ui.action("extrude").isEnabled());
  ui.choose(filter, 5); // Bodies evicts the browser-only profile fallback.
  CHECK_FALSE(ui.action("extrude").isEnabled());
  ui.tree("datum.xy");
  CHECK_FALSE(ui.action("create_sketch").isEnabled());
  CHECK(ui.window.session().selection().empty());
  ui.tree(body.value());
  REQUIRE_FALSE(ui.window.session().selection().empty());
  CHECK(ui.window.session().selection().entries().front().kind == GuiSelectionKind::Body);
  ui.command("view_top", 2);
  if (QApplication::platformName() == "xcb") {
    const auto point = checked(ui.window.viewport().model_to_screen({15, 10, 12}));
    ui.click_at(ui.window.viewport(), {point.x, point.y});
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->semantic_id == "body/" + body.value());
  }
  std::vector<std::string> states{ui.snapshot()};
  ui.command("fillet");
  CHECK_FALSE(filter.isEnabled());
  CHECK(ui.window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Edge));
  auto& edges = ui.widget<QListWidget>("blcad.finishing.targets");
  REQUIRE(edges.count() > 0);
  ui.list(edges, 0);
  const auto accepted_radius = ui.widget<QLineEdit>("blcad.manipulator_numeric_hud").text();
  ui.scalar("invalid");
  // The primary manipulator editor rejects bad text by restoring its last
  // accepted value. No candidate or model change is accepted from that text.
  CHECK(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud").text() == accepted_radius);
  CHECK(ui.snapshot() == states.back());
  ui.scalar("1 mm");
  ui.capture("selection-filter-finishing-preview");
  ui.button("blcad.feature.apply");
  REQUIRE(filter.isEnabled());
  CHECK(filter.currentIndex() == 5);
  CHECK(ui.window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));
  const auto rounded_volume = ui.summary(body).volume_mm3;
  REQUIRE(rounded_volume < 28800);
  states.push_back(ui.snapshot());
  ui.command("hole");
  CHECK_FALSE(filter.isEnabled());
  ui.tree("datum.xz");
  CHECK(ui.widget<QComboBox>("blcad.hole.plane").currentData() == "datum.xz");
  REQUIRE_FALSE(ui.window.session().selection().empty());
  CHECK(ui.window.session().selection().entries().front().kind == GuiSelectionKind::Datum);
  ui.tree("datum.xy");
  ui.text(ui.widget<QLineEdit>("blcad.hole.diameter"), "6");
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "1000");
  ui.button("blcad.hole.preview");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "0");
  ui.button("blcad.hole.preview");
  ui.capture("selection-filter-hole-preview");
  ui.button("blcad.hole.apply");
  CHECK(filter.currentIndex() == 5);
  CHECK(ui.window.session().selection().filter_mask() ==
        selection_kind_bit(GuiSelectionKind::Body));
  CHECK(ui.summary(body).volume_mm3 ==
        Approx(rounded_volume - std::numbers::pi * 9 * 12).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.choose(filter, 0);
  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(depth.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "16");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.choose(filter, 5);
  ui.tree(sketch.value(), true);
  REQUIRE(ui.window.active_sketch());
  CHECK_FALSE(filter.isEnabled());
  ui.command("finish_sketch");
  CHECK(filter.currentIndex() == 5);
  CHECK_FALSE(ui.action("extrude").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.capture("selection-filter-after-history");
  ui.roundtrip(directory.filePath("filtered-machining.blcad"));
  CHECK(filter.currentIndex() == 0);
  const auto step = directory.filePath("filtered-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI repeats fresh hole tasks then updates finishing and source parameters "
          "through history",
          "[gui][integration][shell-combined-workflow][shell-combined-repeat]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Repeated machining");
  ui.rectangle("datum.xy", {-40, -30}, {40, 30});
  const auto body = ui.extrude(12);
  const auto depth = ui.window.session().part_document()->features().back().length_parameter();
  std::vector<std::string> states{ui.snapshot()};
  ui.command("hole");
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "-15");
  ui.text(ui.widget<QLineEdit>("blcad.hole.diameter"), "8");
  ui.button("blcad.hole.preview");
  ui.button("blcad.hole.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.action("repeat_last_command").text() == "Repeat: Holes");
  ui.command("repeat_last_command");
  REQUIRE(ui.window.hole_tools().active());
  CHECK(ui.widget<QLineEdit>("blcad.hole.center_x").text() == "0");
  CHECK(ui.widget<QLineEdit>("blcad.hole.diameter").text() == "6");
  CHECK_FALSE(ui.action("repeat_last_command").isEnabled());
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "1000");
  ui.button("blcad.hole.preview");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.hole.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.hole.center_x"), "15");
  ui.button("blcad.hole.preview");
  ui.reveal(ui.widget<QToolButton>("blcad.repeat.button"));
  ui.capture("repeat-hole-preview");
  ui.button("blcad.hole.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(57600 - std::numbers::pi * 25 * 12).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.command("fillet");
  auto& edges = ui.widget<QListWidget>("blcad.finishing.targets");
  REQUIRE(edges.count() > 0);
  ui.list(edges, 0);
  ui.scalar("1 mm");
  ui.button("blcad.feature.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.action("repeat_last_command").text() == "Repeat: Fillet");
  // Exercise the actual window shortcut, including focus outside the ribbon.
  ui.window.activateWindow();
  ui.window.setFocus();
  ui.flush();
  ui.key(ui.window, Qt::Key_R, {}, Qt::ControlModifier);
  REQUIRE(ui.window.feature_tools().command() == ShellFeatureTools::Command::Fillet);
  CHECK(ui.snapshot() == states.back());
  ui.key(ui.window, Qt::Key_Escape);
  CHECK_FALSE(ui.window.session().task().active());
  ui.command("parameters");
  CHECK_FALSE(ui.action("repeat_last_command").isEnabled());
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(depth.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "16");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.action("repeat_last_command").text() == "Repeat: Fillet");
  ui.history(states);
  REQUIRE(ui.action("repeat_last_command").isEnabled());
  auto& repeat_button = ui.widget<QToolButton>("blcad.repeat.button");
  ui.reveal(repeat_button);
  CHECK(repeat_button.width() >= repeat_button.sizeHint().width());
  ui.capture("repeat-after-history");
  ui.roundtrip(directory.filePath("repeated-machining.blcad"));
  CHECK_FALSE(ui.action("repeat_last_command").isEnabled());
  CHECK(ui.action("repeat_last_command").text() == "Repeat Command");
  const auto step = directory.filePath("repeated-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI profile contexts restore cancelled cuts then author Thin walls and "
          "propagate upstream edits",
          "[gui][integration][shell-combined-workflow][shell-combined-profile-context]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Context machining");
  const auto plate = ui.rectangle("datum.xy", {-30, -20}, {30, 20});
  const auto pocket = ui.rectangle("datum.xy", {-10, -5}, {10, 5});
  const auto wall = ui.open_line("datum.xz", {40, 0}, {60, 0});
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  ui.tree(plate.value());
  ui.command("view_top", 2);
  if (QApplication::platformName() == "xcb") {
    ui.tree("datum.xy");
    REQUIRE_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
    const auto point = checked(ui.window.viewport().model_to_screen({22, 12, 0}));
    ui.click_at(ui.window.viewport(), {point.x, point.y});
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->kind == GuiSelectionKind::SketchEntity);
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
    CHECK(toolbar.geometry()
              .adjusted(-16, -16, 16, 16)
              .contains(QPoint(qRound(point.x), qRound(point.y))));
  }
  std::vector<std::string> states{ui.snapshot()};
  context("extrude");
  CHECK(ui.window.session().selection().empty());
  ui.scalar("20 mm");
  const auto base = ui.window.feature_tools().coordinator().extrude().result_body_id();
  const auto base_feature = ui.window.feature_tools().coordinator().extrude().feature_id();
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(48000).margin(1e-5));
  const auto depth =
      ui.window.session().part_document()->find_feature(base_feature)->length_parameter();
  states.push_back(ui.snapshot());
  ui.tree(pocket.value());
  context("extrude");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.scalar("6 mm");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.taper"), "invalid");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  context("cancel");
  CHECK_FALSE(ui.window.session().task().active());
  REQUIRE(ui.window.feature_tools().profile_context());
  CHECK(ui.window.feature_tools().profile_context()->sketch == pocket);
  context("extrude");
  CHECK(ui.widget<QComboBox>("blcad.extrude.operation").currentIndex() == 0);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
  ui.scalar("6 mm");
  ui.capture("context-cut-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(46800).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(wall.value());
  CHECK_FALSE(ui.action("context_revolve").isVisible());
  context("extrude");
  ui.text(ui.widget<QLineEdit>("blcad.extrude.thickness"), "2");
  ui.scalar("10 mm");
  const auto wall_body = ui.window.feature_tools().coordinator().extrude().result_body_id();
  ui.capture("context-thin-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(wall_body).volume_mm3 == Approx(400).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.command("parameters");
  CHECK_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(depth.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "24");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(base).volume_mm3 == Approx(56400).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(base_feature.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Context base plate");
  ui.button("blcad.feature_edit.apply");
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(pocket.value());
  REQUIRE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  ui.capture("context-after-history");
  ui.roundtrip(directory.filePath("context-machining.blcad"));
  CHECK_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  const auto step = directory.filePath("context-machining.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI spatial contexts seed chosen profiles recover Loft order and update a Sweep",
          "[gui][integration][shell-combined-workflow][shell-combined-spatial-context]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Spatial context workflow");
  // Author the lower profile last so it cannot be the default first source.
  const auto middle_plane = ui.offset(10);
  const auto middle = ui.rectangle(middle_plane.value(), {-8, -8}, {8, 8});
  const auto upper_plane = ui.offset(20);
  const auto upper = ui.rectangle(upper_plane.value(), {-6, -6}, {6, 6});
  const auto lower = ui.rectangle("datum.xy", {-10, -10}, {10, 10});
  const auto path_sketch = ui.open_line("datum.xz", {0, 0}, {0, 20});
  const auto label = [&](const SketchId& sketch) {
    return QString::fromStdString(
               ui.window.session().part_document()->find_sketch(sketch)->name()) +
           " / ";
  };
  const auto add = [&](const SketchId& sketch) {
    auto& source = ui.widget<QComboBox>("blcad.spatial.source");
    ui.choose(source, source.findText(label(sketch), Qt::MatchStartsWith));
    ui.button("blcad.spatial.add");
  };
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  ui.command("path_curve");
  add(path_sketch);
  ui.button("blcad.feature.apply");
  const auto path = ui.window.session().part_document()->path_curves().back().id();
  std::vector<std::string> states{ui.snapshot()};
  ui.tree(lower.value());
  ui.command("view_top", 2);
  if (QApplication::platformName() == "xcb") {
    ui.tree("datum.xy");
    REQUIRE_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
    const auto point = checked(ui.window.viewport().model_to_screen({-9, -9, 0}));
    ui.click_at(ui.window.viewport(), {point.x, point.y});
    REQUIRE(ui.window.feature_tools().profile_context());
    CHECK(ui.window.feature_tools().profile_context()->sketch == lower);
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
  }
  context("sweep");
  CHECK(ui.widget<QComboBox>("blcad.spatial.source").currentText().startsWith(label(lower)));
  ui.scalar("15 deg");
  ui.data(ui.widget<QComboBox>("blcad.spatial.guide"), QString::fromStdString(path.value()));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  context("cancel");
  REQUIRE(ui.window.feature_tools().profile_context());
  CHECK(ui.window.feature_tools().profile_context()->sketch == lower);
  context("loft");
  auto& sections = ui.widget<QListWidget>("blcad.spatial.order");
  REQUIRE(sections.count() == 1);
  CHECK(sections.item(0)->text().startsWith(label(lower)));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  add(lower);
  CHECK(sections.count() == 1);
  add(upper);
  add(middle);
  ui.list(sections, 2);
  ui.button("blcad.spatial.order.up");
  CHECK(sections.item(1)->text().startsWith(label(middle)));
  ui.data(ui.widget<QComboBox>("blcad.spatial.path"), QString::fromStdString(path.value()));
  ui.choose(ui.window.feature_tools().spatial_continuity_editor(), 2);
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.choose(ui.window.feature_tools().spatial_continuity_editor(), 0);
  auto& up = ui.widget<QPushButton>("blcad.spatial.order.up");
  auto& hint = ui.widget<QLabel>("blcad.spatial.order_hint");
  auto& trajectory = ui.widget<QComboBox>("blcad.spatial.path");
  CHECK(up.height() >= up.sizeHint().height());
  CHECK(hint.mapTo(&ui.window, hint.rect().bottomLeft()).y() <
        trajectory.mapTo(&ui.window, trajectory.rect().topLeft()).y());
  ui.capture("spatial-context-loft-preview");
  ui.button("blcad.feature.apply");
  const auto loft = ui.window.session().part_document()->loft_features().back().id();
  const auto loft_body = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(loft_body).solid_count == 1);
  CHECK(ui.summary(loft_body).volume_mm3 > 3000);
  CHECK(ui.summary(loft_body).volume_mm3 < 8000);
  states.push_back(ui.snapshot());
  ui.tree(lower.value());
  context("sweep");
  CHECK(ui.widget<QComboBox>("blcad.spatial.source").currentText().startsWith(label(lower)));
  CHECK(ui.widget<QComboBox>("blcad.spatial.mode").currentIndex() == 0);
  ui.scalar("0 deg");
  ui.capture("spatial-context-sweep-preview");
  ui.button("blcad.feature.apply");
  const auto& sweep = ui.window.session().part_document()->sweep_features().back();
  CHECK(std::get<ProfileRegionReference>(sweep.profile().source()).sketch() == lower);
  REQUIRE(sweep.twist_parameter());
  const auto twist = *sweep.twist_parameter();
  const auto swept_body = ui.window.session().part_document()->bodies().back().id();
  CHECK(ui.summary(swept_body).volume_mm3 == Approx(8000).margin(1e-4));
  states.push_back(ui.snapshot());
  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(twist.value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "15");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(swept_body).solid_count == 1);
  CHECK(ui.summary(swept_body).volume_mm3 > 0);
  states.push_back(ui.snapshot());
  ui.tree(loft.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Context three-section loft");
  ui.button("blcad.feature_edit.apply");
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.tree(lower.value());
  REQUIRE(ui.widget<QToolButton>("blcad.context.sweep").isVisible());
  REQUIRE(ui.widget<QToolButton>("blcad.context.loft").isVisible());
  ui.capture("spatial-context-after-history");
  ui.roundtrip(directory.filePath("spatial-context.blcad"));
  CHECK_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  const auto step = directory.filePath("spatial-context.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Combined native UI finishing contexts machine separate Bodies and propagate a parameter batch",
    "[gui][integration][shell-combined-workflow][shell-combined-finishing-context]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Idle finishing subshape context workflow requires native Qt/OCCT picking");
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Context vessels and machining");
  ui.rectangle("datum.xy", {-60, 40}, {-30, 60});
  const auto machined = ui.extrude(10);
  ui.rectangle("datum.xy", {0, 0}, {30, 20});
  const auto vessel = ui.extrude(10);
  const auto source = ui.window.session().part_document()->features().back().id();
  const auto depth = ui.window.session().part_document()->find_feature(source)->length_parameter();
  std::vector<std::string> states{ui.snapshot()};
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  const auto pick = [&](const BodyId& body, const std::string& role, bool edge,
                        const QString& view) {
    ui.command(view, 2);
    auto& filter = ui.widget<QComboBox>("blcad.selection.filter");
    ui.data(filter, selection_kind_bit(edge ? GuiSelectionKind::Edge : GuiSelectionKind::Face));
    ui.tree("datum.xy");
    REQUIRE_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
    const auto targets = checked(geometry::FinishingSelectionBuilder::build(
        *ui.window.session().part_document(), *ui.window.session().part_shape_cache(), body,
        edge ? PartFeatureInputRole::FilletEdge : PartFeatureInputRole::ShellRemovalFace));
    const auto target = std::find_if(targets.begin(), targets.end(), [&](const auto& item) {
      return item.label.ends_with(" / " + role);
    });
    REQUIRE(target != targets.end());
    const auto point = checked(ui.window.viewport().model_to_screen(target->anchor));
    ui.click_at(ui.window.viewport(), {point.x, point.y});
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->semantic_id == target->semantic_id);
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
  };
  pick(vessel, "right", false, "view_right");
  context("draft");
  CHECK(ui.widget<QComboBox>("blcad.finishing.body").currentData().toString() ==
        QString::fromStdString(vessel.value()));
  CHECK(ui.widget<QListWidget>("blcad.finishing.targets").selectedItems().size() == 1);
  ui.scalar("4 deg");
  CHECK(ui.snapshot() == states.back());
  context("cancel");
  CHECK(ui.window.viewport().selected_semantic());
  pick(vessel, "top", false, "view_top");
  context("shell");
  auto& faces = ui.widget<QListWidget>("blcad.finishing.targets");
  REQUIRE(faces.selectedItems().size() == 1);
  const int removal = faces.row(faces.selectedItems().front());
  ui.list(faces, removal);
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.list(faces, removal);
  ui.scalar("1 mm");
  ui.capture("finishing-context-shell-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(vessel).volume_mm3 == Approx(1464).margin(1e-4));
  CHECK(ui.summary(machined).volume_mm3 == Approx(6000).margin(1e-4));
  const auto thickness =
      ui.window.session().part_document()->shell_features().back().thickness_parameter();
  states.push_back(ui.snapshot());
  pick(machined, "top_front", true, "view_front");
  context("fillet");
  ui.scalar("1.5 mm");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(machined).volume_mm3 < 6000);
  CHECK(ui.summary(machined).volume_mm3 > 5800);
  CHECK(ui.summary(vessel).volume_mm3 == Approx(1464).margin(1e-4));
  states.push_back(ui.snapshot());
  pick(machined, "bottom_back", true, "view_isometric");
  context("chamfer");
  auto& mode = ui.widget<QComboBox>("blcad.finishing.mode");
  CHECK(mode.height() >= mode.minimumSizeHint().height());
  ui.choose(mode, 1);
  auto& secondary = ui.widget<QLineEdit>("blcad.finishing.secondary");
  CHECK(secondary.height() >= secondary.minimumSizeHint().height());
  ui.text(ui.widget<QLineEdit>("blcad.finishing.secondary"), "0.8 mm");
  ui.scalar("1 mm");
  CHECK(ui.snapshot() == states.back());
  ui.key(ui.window, Qt::Key_Escape);
  REQUIRE(ui.window.viewport().selected_semantic());
  context("chamfer");
  CHECK(ui.widget<QComboBox>("blcad.finishing.mode").currentIndex() == 0);
  ui.choose(ui.widget<QComboBox>("blcad.finishing.mode"), 2);
  ui.text(ui.widget<QLineEdit>("blcad.finishing.secondary"), "30 deg");
  ui.scalar("1 mm");
  ui.capture("finishing-context-chamfer-preview");
  ui.button("blcad.feature.apply");
  const auto machined_volume = ui.summary(machined).volume_mm3;
  states.push_back(ui.snapshot());
  ui.command("parameters");
  for (const auto& [parameter, value] : {std::pair{thickness, "1.2"}, std::pair{depth, "12"}}) {
    ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
            QString::fromStdString(parameter.value()));
    ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), value);
    ui.button("blcad.parameters.stage");
  }
  ui.button("blcad.parameters.preview");
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(vessel).volume_mm3 == Approx(1953.792).margin(1e-3));
  CHECK(ui.summary(machined).volume_mm3 == Approx(machined_volume).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(source.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Context vessel base");
  ui.button("blcad.feature_edit.apply");
  states.push_back(ui.snapshot());
  CHECK(ui.window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Edge));
  ui.data(ui.widget<QComboBox>("blcad.selection.filter"), 0xFFFFFFFFU);
  ui.history(states);
  pick(vessel, "right", false, "view_right");
  ui.capture("finishing-context-after-history");
  ui.roundtrip(directory.filePath("finishing-context.blcad"));
  CHECK_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  const auto step = directory.filePath("finishing-context.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI pattern contexts chain reflected sources through upstream parameter batches",
          "[gui][integration][shell-combined-workflow][shell-combined-pattern-context]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Context pattern chain");
  ui.rectangle("datum.xy", {0, 0}, {10, 10}, Point2{20, 20});
  const auto unchanged = ui.extrude(10);
  ui.rectangle("datum.xy", {20, 20}, {28, 28}, Point2{40, 40});
  const auto source = ui.extrude(8);
  const auto source_depth =
      ui.window.session().part_document()->features().back().length_parameter();
  std::vector<std::string> states{ui.snapshot()};
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  const auto pick = [&](const BodyId& body) {
    ui.command("view_isometric", 2);
    ui.data(ui.widget<QComboBox>("blcad.selection.filter"),
            selection_kind_bit(GuiSelectionKind::Body));
    ui.tree("datum.xy");
    REQUIRE_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
    if (QApplication::platformName() == "xcb") {
      const auto bounds = ui.bounds(body);
      const auto point = checked(ui.window.viewport().model_to_screen(
          {(bounds.minimum.x + bounds.maximum.x) / 2, (bounds.minimum.y + bounds.maximum.y) / 2,
           (bounds.minimum.z + bounds.maximum.z) / 2}));
      ui.click_at(ui.window.viewport(), {point.x, point.y});
      REQUIRE(ui.window.viewport().selected_semantic());
      CHECK(ui.window.viewport().selected_semantic()->semantic_id == "body/" + body.value());
    } else
      ui.tree(body.value());
    REQUIRE(ui.window.session().selection().size() == 1);
    CHECK(ui.window.session().selection().entries().front().semantic_id == "body/" + body.value());
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
  };
  const auto exact_source = [&](const BodyId& body) {
    const auto& selected = ui.widget<QListWidget>("blcad.pattern.sources").selectedItems();
    REQUIRE(selected.size() == 1);
    CHECK(selected.front()->data(Qt::UserRole).toString() == QString::fromStdString(body.value()));
  };
  pick(source);
  context("body_mirror");
  exact_source(source);
  ui.button("blcad.pattern.pick_reference");
  ui.tree("datum.xz");
  CHECK(ui.snapshot() == states.back());
  context("cancel");
  REQUIRE(ui.window.session().selection().size() == 1);
  CHECK(ui.window.session().selection().entries().front().semantic_id == "body/" + source.value());
  context("body_mirror");
  ui.button("blcad.pattern.pick_reference");
  ui.tree("datum.xz");
  ui.capture("pattern-context-mirror-preview");
  ui.button("blcad.feature.apply");
  const auto reflected = ui.window.session().part_document()->bodies().back().id();
  const auto mirror = ui.window.session().part_document()->mirror_features().back().id();
  CHECK(ui.summary(reflected).volume_mm3 == Approx(512).margin(1e-5));
  CHECK(ui.bounds(reflected).minimum.y == Approx(-28).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(source);
  context("circular_pattern");
  exact_source(source);
  auto& sources = ui.widget<QListWidget>("blcad.pattern.sources");
  ui.list_data(sources, QString::fromStdString(source.value()));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.list_data(sources, QString::fromStdString(source.value()));
  auto& count = ui.widget<QSpinBox>("blcad.pattern.count");
  CHECK(count.height() >= count.minimumSizeHint().height());
  ui.number(count, "4");
  ui.scalar("240 deg");
  ui.capture("pattern-context-circular-preview");
  ui.button("blcad.feature.apply");
  const auto radial = ui.window.session().part_document()->bodies().back().id();
  const auto count_parameter =
      ui.window.session().part_document()->circular_pattern_features().back().count_parameter();
  CHECK(ui.summary(radial).solid_count == 4);
  CHECK(ui.summary(radial).volume_mm3 == Approx(2048).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(reflected);
  context("linear_pattern");
  exact_source(reflected);
  ui.number(count, "2");
  ui.choose(ui.widget<QComboBox>("blcad.pattern.extent"), 1);
  ui.choose(ui.widget<QComboBox>("blcad.pattern.direction"), 1);
  ui.scalar("5 deg");
  CHECK(ui.snapshot() == states.back());
  ui.scalar("45 mm");
  ui.capture("pattern-context-linear-preview");
  ui.button("blcad.feature.apply");
  const auto linear = ui.window.session().part_document()->bodies().back().id();
  const auto extent_parameter =
      ui.window.session().part_document()->linear_pattern_features().back().extent_parameter();
  CHECK(ui.summary(linear).solid_count == 2);
  CHECK(ui.summary(linear).volume_mm3 == Approx(1024).margin(1e-5));
  const auto& linear_intent = ui.window.session().part_document()->linear_pattern_features().back();
  CHECK(linear_intent.extent_mode() == LinearPatternExtentMode::TotalExtent);
  CHECK(linear_intent.direction_sign() == PatternDirectionSign::Negative);
  CHECK(ui.bounds(linear).minimum.x == Approx(ui.bounds(reflected).minimum.x - 45).margin(1e-5));
  states.push_back(ui.snapshot());

  ui.command("parameters");
  for (const auto& [parameter, value] :
       {std::pair{source_depth, "12"}, std::pair{count_parameter, "5"},
        std::pair{extent_parameter, "55"}}) {
    ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
            QString::fromStdString(parameter.value()));
    ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), value);
    ui.button("blcad.parameters.stage");
  }
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(unchanged).volume_mm3 == Approx(1000).margin(1e-5));
  CHECK(ui.summary(source).volume_mm3 == Approx(768).margin(1e-5));
  CHECK(ui.summary(reflected).volume_mm3 == Approx(768).margin(1e-5));
  CHECK(ui.summary(radial).volume_mm3 == Approx(3840).margin(1e-5));
  CHECK(ui.summary(linear).volume_mm3 == Approx(1536).margin(1e-5));
  CHECK(ui.bounds(linear).minimum.x == Approx(ui.bounds(reflected).minimum.x - 55).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.tree(mirror.value(), true);
  ui.text(ui.widget<QLineEdit>("blcad.feature_edit.field/name"), "Context reflected source");
  ui.button("blcad.feature_edit.apply");
  states.push_back(ui.snapshot());
  ui.data(ui.widget<QComboBox>("blcad.selection.filter"), 0xFFFFFFFFU);
  ui.history(states);
  pick(unchanged);
  ui.capture("pattern-context-after-history");
  ui.roundtrip(directory.filePath("pattern-context.blcad"));
  CHECK_FALSE(ui.widget<QWidget>("blcad.context.toolbar").isVisible());
  const auto step = directory.filePath("pattern-context.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI Body contexts collect roles and preserve a Boolean Transform chain",
          "[gui][integration][shell-combined-workflow][shell-combined-body-context]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Body context machining");
  ui.rectangle("datum.xy", {0, 0}, {10, 10}, Point2{20, 20});
  const auto tool = ui.extrude(10);
  ui.rectangle("datum.xy", {20, 0}, {30, 10}, Point2{40, 20});
  const auto target = ui.extrude(10);
  std::vector<std::string> states{ui.snapshot()};
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  const auto pick = [&](const BodyId& body) {
    ui.command("view_isometric", 2);
    ui.data(ui.widget<QComboBox>("blcad.selection.filter"),
            selection_kind_bit(GuiSelectionKind::Body));
    ui.tree("datum.xy");
    if (QApplication::platformName() == "xcb") {
      const auto bounds = ui.bounds(body);
      const auto point = checked(ui.window.viewport().model_to_screen(
          {(bounds.minimum.x + bounds.maximum.x) / 2, (bounds.minimum.y + bounds.maximum.y) / 2,
           bounds.maximum.z}));
      ui.click_at(ui.window.viewport(), {point.x, point.y});
    } else
      ui.tree(body.value());
    REQUIRE(ui.window.session().selection().size() == 1);
    CHECK(ui.window.session().selection().entries().front().semantic_id == "body/" + body.value());
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
  };
  auto& sources = ui.widget<QListWidget>("blcad.body.sources");
  const auto exact = [&](const BodyId& body) {
    REQUIRE(sources.selectedItems().size() == 1);
    CHECK(sources.selectedItems().front()->data(Qt::UserRole).toString() ==
          QString::fromStdString(body.value()));
  };
  pick(target);
  context("body_translate");
  exact(target);
  ui.tree(tool.value());
  REQUIRE(sources.selectedItems().size() == 2);
  ui.scalar("4 mm");
  context("cancel");
  CHECK(ui.snapshot() == states.back());
  REQUIRE(ui.window.session().selection().size() == 1);
  CHECK(ui.window.session().selection().entries().front().semantic_id == "body/" + target.value());
  context("body_translate");
  exact(target);
  ui.scalar("-15 mm");
  ui.text(ui.widget<QLineEdit>("blcad.body.translation.1"), "0 mm");
  ui.capture("body-context-translate-preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  CHECK(ui.bounds(target).minimum.x == Approx(5).margin(1e-5));
  CHECK(ui.bounds(tool).minimum.x == Approx(0).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(target);
  context("body_boolean");
  auto& target_editor = ui.widget<QComboBox>("blcad.boolean.target");
  CHECK(target_editor.currentData().toString() == QString::fromStdString(target.value()));
  CHECK(sources.selectedItems().empty());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.button("blcad.body.pick_reference");
  ui.tree(tool.value());
  CHECK(target_editor.currentData().toString() == QString::fromStdString(tool.value()));
  ui.button("blcad.body.pick_reference");
  ui.tree(target.value());
  ui.choose(ui.widget<QComboBox>("blcad.boolean.operation"), 1);
  ui.click(ui.widget<QCheckBox>("blcad.boolean.keep_tools"));
  ui.tree(tool.value());
  exact(tool);
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture("body-context-boolean-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(target).volume_mm3 == Approx(500).margin(1e-5));
  CHECK(ui.summary(tool).volume_mm3 == Approx(1000).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(target);
  context("body_rotate");
  exact(target);
  ui.button("blcad.body.pick_reference");
  ui.tree("datum.axis.z");
  ui.scalar("90 deg");
  CHECK(ui.snapshot() == states.back());
  ui.capture("body-context-rotate-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.bounds(target).minimum.x == Approx(-10).margin(1e-5));
  CHECK(ui.bounds(target).minimum.y == Approx(10).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(target);
  context("body_scale");
  exact(target);
  ui.scalar("-1");
  CHECK(ui.snapshot() == states.back());
  ui.text(ui.widget<QLineEdit>("blcad.body.center.0"), "-10 mm");
  ui.text(ui.widget<QLineEdit>("blcad.body.center.1"), "10 mm");
  ui.scalar("2");
  ui.capture("body-context-scale-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(target).volume_mm3 == Approx(4000).margin(1e-5));
  CHECK(ui.summary(tool).volume_mm3 == Approx(1000).margin(1e-5));
  CHECK(ui.bounds(target).maximum.y == Approx(20).margin(1e-5));
  REQUIRE(ui.window.session().part_document()->body_transforms().size() == 3);
  for (const auto& transform : ui.window.session().part_document()->body_transforms())
    CHECK(transform.body() == target);
  states.push_back(ui.snapshot());
  ui.data(ui.widget<QComboBox>("blcad.selection.filter"), 0xFFFFFFFFU);
  ui.history(states);
  ui.roundtrip(directory.filePath("body-context.blcad"));
  const auto step = directory.filePath("body-context.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE("Combined UI Surface contexts fill trim extend sew and convert a shell through history",
          "[gui][integration][shell-combined-workflow][shell-combined-surface-context]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Surface context workflow");
  const auto boundary_sketch = ui.rectangle("datum.xy", {20, 20}, {40, 40});
  const auto base = ui.extrude(10);
  const auto base_feature = ui.window.session().part_document()->features().front();
  const auto trim_sketch = ui.rectangle("datum.xy", {25, 25}, {35, 35}, Point2{50, 50});
  const auto make_path = [&](const SketchId& id) {
    const auto sketch = *ui.window.session().part_document()->find_sketch(id);
    ui.command("path_curve");
    for (const auto& line : sketch.line_segments()) {
      auto& source = ui.widget<QComboBox>("blcad.spatial.source");
      ui.choose(source,
                source.findText(QString::fromStdString(sketch.name() + " / " + line.id().value())));
      ui.button("blcad.spatial.add");
    }
    ui.click(ui.window.feature_tools().spatial_closed_editor());
    ui.button("blcad.feature.apply");
    return ui.window.session().part_document()->path_curves().back().id();
  };
  const auto path = make_path(boundary_sketch);
  const auto trim_path = make_path(trim_sketch);
  std::vector<std::string> states{ui.snapshot()};
  const auto context = [&](const QString& id) {
    ui.click(ui.widget<QToolButton>("blcad.context." + id));
  };
  const auto pick = [&](const std::string& id) {
    ui.tree(id);
    REQUIRE(ui.window.session().selection().size() == 1);
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
  };
  auto& order = ui.widget<QListWidget>("blcad.surface.order");
  auto& source = ui.widget<QComboBox>("blcad.surface.source");
  auto& target = ui.widget<QComboBox>("blcad.surface.target");
  pick(path.value());
  context("surface_boundary");
  REQUIRE(order.count() == 1);
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  context("cancel");
  CHECK(ui.snapshot() == states.back());
  REQUIRE(ui.window.session().selection().size() == 1);
  CHECK(ui.window.session().selection().entries().front().semantic_id == "path/" + path.value());
  context("surface_fill");
  REQUIRE(order.count() == 1);
  ui.button("blcad.surface.add");
  CHECK(order.count() == 1);
  ui.list(order, 0);
  ui.button("blcad.surface.remove");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.button("blcad.surface.add");
  ui.capture("surface-context-fill-preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto filled =
      surface_feature_result_body(ui.window.session().part_document()->surface_features().back());
  CHECK(ui.bounds(filled).maximum.x == Approx(40).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(filled.value());
  context("surface_trim");
  CHECK(ui.widget<QComboBox>("blcad.surface.pick_role").currentIndex() == 1);
  CHECK(target.currentText() ==
        QString::fromStdString(ui.window.session().part_document()->find_body(filled)->name()));
  ui.tree(trim_path.value());
  CHECK(source.currentText() ==
        QString::fromStdString(
            ui.window.session().part_document()->find_path_curve(trim_path)->name()));
  ui.capture("surface-context-trim-preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto trimmed =
      surface_feature_result_body(ui.window.session().part_document()->surface_features().back());
  CHECK(ui.bounds(trimmed).minimum.x == Approx(25).margin(1e-5));
  CHECK(ui.bounds(trimmed).maximum.y == Approx(35).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(filled.value());
  context("surface_extend");
  ui.choose(source,
            source.findText(QString::fromStdString(base_feature.name() + " / bottom_back")));
  ui.scalar("-3 mm");
  CHECK(ui.snapshot() == states.back());
  ui.scalar("5 mm");
  ui.capture("surface-context-extend-preview");
  ui.button("blcad.feature.apply");
  const auto extended =
      surface_feature_result_body(ui.window.session().part_document()->surface_features().back());
  CHECK(ui.bounds(extended).minimum.y == Approx(15).margin(1e-5));
  CHECK(ui.bounds(extended).maximum.y == Approx(40).margin(1e-5));
  states.push_back(ui.snapshot());

  pick(filled.value());
  context("surface_to_solid");
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  context("cancel");
  CHECK(ui.snapshot() == states.back());
  context("surface_stitch");
  REQUIRE(order.count() == 1);
  ui.tree(extended.value());
  REQUIRE(order.count() == 2);
  ui.list(order, 1);
  ui.button("blcad.surface.remove");
  REQUIRE(order.count() == 1);
  for (const auto face : {"top", "front", "back", "left", "right"}) {
    ui.choose(target, target.findText(QString::fromStdString(base_feature.name() + " / " + face)));
    ui.button("blcad.surface.add");
  }
  REQUIRE(order.count() == 6);
  ui.button("blcad.surface.add");
  CHECK(order.count() == 6);
  ui.list(order, 5);
  ui.button("blcad.surface.move_up");
  ui.button("blcad.surface.move_down");
  ui.scalar("0.001 mm");
  ui.capture("surface-context-stitch-preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto shell =
      surface_feature_result_body(ui.window.session().part_document()->surface_features().back());
  CHECK(ui.summary(shell).solid_count == 0);
  states.push_back(ui.snapshot());

  pick(shell.value());
  context("surface_to_solid");
  ui.capture("surface-context-solid-preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  const auto solid =
      surface_feature_result_body(ui.window.session().part_document()->surface_features().back());
  CHECK(ui.summary(solid).solid_count == 1);
  CHECK(ui.summary(solid).volume_mm3 == Approx(4000).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(4000).margin(1e-5));
  states.push_back(ui.snapshot());

  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"),
          QString::fromStdString(base_feature.length_parameter().value()));
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "12");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.parameters.apply");
  CHECK(ui.summary(solid).volume_mm3 == Approx(4800).margin(1e-5));
  CHECK(ui.summary(base).volume_mm3 == Approx(4800).margin(1e-5));
  CHECK(ui.bounds(trimmed).maximum.x == Approx(35).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("surface-context.blcad"));
  const auto step = directory.filePath("surface-context.step");
  ui.file("export_part_step", step);
  CHECK(ui.bytes(step).contains("ISO-10303-21"));
  CHECK(ui.snapshot() == states.back());
  ui.idle();
}

TEST_CASE(
    "Combined UI Assembly contexts place and drive a plate through sampled analysis and history",
    "[gui][integration][shell-combined-workflow][shell-combined-assembly-context]"
    "[shell-combined-assembly-analysis]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto source =
      QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/revolute_joint.blcad.project.json";
  const auto original_source = ui.bytes(source);
  ui.new_document("Plate motion and inspection", true);
  ui.file("assembly_import", source);
  const auto part = ui.window.session().project()->part_documents().front().id();
  std::vector<ComponentInstanceId> components;
  for (int index = 0; index < 2; ++index) {
    ui.command("assembly_insert_component");
    ui.data(ui.window.assembly_tools().definition_editor(), QString::fromStdString(part.value()));
    ui.text(ui.widget<QLineEdit>("blcad.assembly.name"), index ? "Moving leaf" : "Fixed leaf");
    ui.number(ui.window.assembly_tools().placement_editor(0), QString::number(index * 30));
    ui.number(ui.window.assembly_tools().placement_editor(2), QString::number(index * 12));
    if (!index)
      ui.click(ui.window.assembly_tools().grounding_editor());
    ui.button("blcad.assembly.apply");
    components.push_back(
        ui.window.session().project()->assembly().component_instances().back().id());
  }
  const auto context = [&](const QString& command) {
    auto& toolbar = ui.widget<QWidget>("blcad.context.toolbar");
    REQUIRE(toolbar.isVisible());
    CHECK(ui.window.viewport().rect().contains(toolbar.geometry()));
    ui.click(ui.widget<QToolButton>("blcad.context." + command));
  };
  std::vector<std::string> states{ui.snapshot()};
  ui.tree(components[1].value());
  const auto selection = ui.window.session().selection().entries();
  context("assembly_placement");
  ui.number(ui.window.assembly_tools().placement_editor(0), "55");
  CHECK(ui.snapshot() == states.back());
  REQUIRE(ui.window.viewport().feature_preview_active());
  context("cancel");
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.window.session().selection().entries() == selection);
  context("assembly_placement");
  ui.number(ui.window.assembly_tools().placement_editor(0), "0");
  ui.number(ui.window.assembly_tools().placement_editor(2), "0");
  ui.button("blcad.assembly.apply");
  CHECK(ui.window.session()
            .project()
            ->assembly()
            .find_component_instance(components[1])
            ->transform() == identity_rigid_transform());
  states.push_back(ui.snapshot());

  ui.command("assembly_joint");
  ui.choose(ui.window.assembly_tools().joint_type_editor(),
            static_cast<int>(AssemblyJointType::Revolute));
  const auto seat = [&](QComboBox& input, const ComponentInstanceId& component) {
    const auto& targets = ui.window.assembly_tools().targets();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
      return target.reference.component_instance() == component &&
             target.reference.semantic_reference() == "feature.hole.seat";
    });
    REQUIRE(found != targets.end());
    ui.data(input, static_cast<int>(found - targets.begin()));
  };
  seat(ui.window.assembly_tools().first_editor(), components[0]);
  seat(ui.window.assembly_tools().second_editor(), components[1]);
  auto& joint_slots = ui.window.assembly_tools().slots_editor();
  REQUIRE(joint_slots.rowCount() == 1);
  ui.cell(joint_slots, 0, 2, "-60");
  ui.cell(joint_slots, 0, 3, "60");
  ui.button("blcad.assembly.apply");
  const auto joint = ui.window.session().project()->assembly().joints().back().id();
  states.push_back(ui.snapshot());
  const auto motion = [&](const QString& angle) {
    ui.tree(components[1].value());
    context("assembly_motion");
    CHECK(ui.window.assembly_tools().joint_editor().currentData().toString() ==
          QString::fromStdString(joint.value()));
    ui.number(ui.window.assembly_tools().value_editor(), angle);
    REQUIRE(ui.window.viewport().feature_preview_active());
    CHECK(ui.snapshot() == states.back());
  };
  motion("30");
  ui.capture("combined-assembly-context-motion");
  ui.button("blcad.assembly.apply");
  CHECK(ui.window.session().project()->assembly().find_joint(joint)->coordinate_deg() == 30);
  CHECK(ui.window.session()
            .project()
            ->assembly()
            .find_component_instance(components[1])
            ->transform()
            .rotation_deg.z == Approx(30).margin(1e-5));
  states.push_back(ui.snapshot());
  motion("-25");
  ui.key(ui.window, Qt::Key_Escape);
  CHECK(ui.snapshot() == states.back());
  ui.history(states);
  ui.command("undo");
  states.pop_back();
  motion("-45");
  ui.button("blcad.assembly.apply");
  CHECK_FALSE(ui.window.session().can_redo());
  states.push_back(ui.snapshot());
  ui.roundtrip(directory.filePath("plate-motion.blcad.project.json"));
  const auto persisted = ui.snapshot();
  const auto saved = ui.bytes(directory.filePath("plate-motion.blcad.project.json"));
  ui.command("assembly_analysis");
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 2);
  ui.button("blcad.inspect.run");
  auto& results = ui.widget<QTableWidget>("blcad.inspect.results");
  REQUIRE(results.rowCount() == 1);
  CHECK(results.item(0, 2)->text().startsWith("Interference:"));
  const auto highlight = [&](int row, int column) {
    auto* item = results.item(row, column);
    REQUIRE(item);
    const auto semantic = item->data(Qt::UserRole + 1).toString().toStdString();
    REQUIRE_FALSE(semantic.empty());
    ui.reveal(results);
    results.scrollToItem(item);
    ui.flush();
    ui.click_at(*results.viewport(), results.visualItemRect(item).center());
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->semantic_id == semantic);
    REQUIRE(ui.window.session().selection().size() == 1);
    CHECK(ui.window.session().selection().entries().front().semantic_id == semantic);
  };
  highlight(0, 0);
  highlight(0, 1);
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 4);
  CHECK(results.rowCount() == 0);
  CHECK_FALSE(ui.window.viewport().selected_semantic());
  auto& sweep_joint = ui.widget<QComboBox>("blcad.inspect.sweep_joint");
  ui.choose(sweep_joint,
            sweep_joint.findData(QString::fromStdString(joint.value()), Qt::UserRole + 1));
  CHECK(sweep_joint.currentData(Qt::UserRole + 2).toInt() == 0);
  ui.number(ui.widget<QSpinBox>("blcad.inspect.sweep_samples"), "3");
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_start"), "-90");
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_end"), "45");
  ui.button("blcad.inspect.run");
  CHECK(results.rowCount() == 0);
  CHECK_FALSE(ui.widget<QLabel>("blcad.inspect.summary").text().isEmpty());
  CHECK(ui.snapshot() == persisted);
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_start"), "-45");
  REQUIRE(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_start").value() == -45);
  REQUIRE(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_end").value() == 45);
  REQUIRE(ui.widget<QSpinBox>("blcad.inspect.sweep_samples").value() == 3);
  ui.button("blcad.inspect.run");
  INFO(ui.widget<QLabel>("blcad.inspect.summary").text().toStdString());
  REQUIRE(results.rowCount() == 3);
  for (int row = 0; row < 3; ++row)
    CHECK(results.item(row, 2)->data(Qt::UserRole).toDouble() == -45 + row * 45);
  highlight(1, 1);
  ui.capture("combined-assembly-context-sweep");
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_end"), "30");
  CHECK(results.rowCount() == 0);
  CHECK_FALSE(ui.window.viewport().selected_semantic());
  ui.button("blcad.inspect.close");
  CHECK(ui.snapshot() == persisted);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK_FALSE(ui.window.session().can_undo());
  CHECK_FALSE(ui.window.session().can_redo());
  for (const auto& [command, filename] : std::vector<std::pair<QString, QString>>{
           {"export_assembly_flat_step", "plate-motion-flat.step"},
           {"export_assembly_structured_step", "plate-motion-structured.step"}}) {
    ui.file(command, directory.filePath(filename));
    CHECK(ui.bytes(directory.filePath(filename)).startsWith("ISO-10303-21;"));
    CHECK(ui.snapshot() == persisted);
  }
  CHECK(ui.bytes(directory.filePath("plate-motion.blcad.project.json")) == saved);
  CHECK(ui.bytes(source) == original_source);
  ui.idle();
}

TEST_CASE("Combined UI repeated subassemblies share solved motion through qualified references and "
          "exports",
          "[gui][integration][shell-combined-workflow][shell-combined-assembly-hierarchy]") {
  WorkflowUi ui;
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Reusable moving cartridge", true);
  ui.file("assembly_import",
          QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/revolute_joint.blcad.project.json");
  const auto part = ui.window.session().project()->part_documents().front().id();
  const auto child = ui.window.session().project()->assembly().id();
  ui.command("assembly_insert_component");
  ui.data(ui.window.assembly_tools().definition_editor(), QString::fromStdString(part.value()));
  ui.text(ui.widget<QLineEdit>("blcad.assembly.name"), "Shared moving plate");
  ui.number(ui.window.assembly_tools().placement_editor(0), "30");
  ui.number(ui.window.assembly_tools().placement_editor(2), "12");
  ui.button("blcad.assembly.apply");
  const auto internal =
      ui.window.session().project()->assembly().component_instances().front().id();
  const auto child_file = directory.filePath("cartridge.blcad.project.json");
  ui.file("save_as", child_file);
  const auto saved_child = ui.bytes(child_file);

  ui.new_document("Repeated cartridges", true);
  ui.file("assembly_import", child_file);
  ui.command("assembly_insert_component");
  ui.data(ui.window.assembly_tools().definition_editor(), QString::fromStdString(part.value()));
  ui.text(ui.widget<QLineEdit>("blcad.assembly.name"), "Grounded reference plate");
  ui.click(ui.window.assembly_tools().grounding_editor());
  ui.button("blcad.assembly.apply");
  const auto fixed = ui.window.session().project()->assembly().component_instances().front().id();
  std::vector<SubassemblyInstanceId> occurrences;
  std::vector<RigidTransform> boundaries;
  for (int index = 0; index < 2; ++index) {
    ui.command("assembly_insert_subassembly");
    ui.data(ui.window.assembly_tools().definition_editor(), QString::fromStdString(child.value()));
    ui.text(ui.widget<QLineEdit>("blcad.assembly.name"),
            index ? "Rear cartridge" : "Front cartridge");
    ui.number(ui.window.assembly_tools().placement_editor(1), QString::number(index * 60));
    ui.button("blcad.assembly.apply");
    const auto& occurrence =
        ui.window.session().project()->assembly().subassembly_instances().back();
    occurrences.push_back(occurrence.id());
    boundaries.push_back(occurrence.transform());
  }
  std::vector<std::string> states{ui.snapshot()};
  ui.command("assembly_validate_hierarchy");
  CHECK(ui.widget<QLabel>("blcad.assembly.summary").text().contains("3 visible active"));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  ui.capture("combined-assembly-hierarchy-validation");
  ui.button("blcad.assembly.cancel");
  CHECK(ui.snapshot() == states.back());
  const auto endpoint = [&](QComboBox& input, const std::vector<SubassemblyInstanceId>& path,
                            const ComponentInstanceId& component, const char* semantic) {
    const auto& targets = ui.window.assembly_tools().hierarchy_targets();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
      return target.endpoint.occurrence_path() == path &&
             target.endpoint.component_instance() == component &&
             target.endpoint.semantic_reference() == semantic;
    });
    REQUIRE(found != targets.end());
    ui.data(input, static_cast<int>(found - targets.begin()));
  };
  const auto shared_transform = [&] {
    const auto* assembly = ui.window.session().project()->find_assembly_document(child);
    REQUIRE(assembly);
    const auto* component = assembly->find_component_instance(internal);
    REQUIRE(component);
    return component->transform();
  };
  ui.command("assembly_cross_relationship");
  ui.choose(ui.window.assembly_tools().relationship_editor(),
            static_cast<int>(AssemblyConstraintType::Mate));
  ui.text(ui.widget<QLineEdit>("blcad.assembly.cross_name"), "Assembly seating check");
  endpoint(ui.window.assembly_tools().first_editor(), {}, fixed, "feature.base_extrude.top");
  endpoint(ui.window.assembly_tools().second_editor(), {occurrences[0]}, internal,
           "feature.base_extrude.bottom");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.assembly.apply");
  const auto relationship =
      ui.window.session().project()->cross_hierarchy_constraints().back().id();
  CHECK(shared_transform().translation_mm.z == 12);
  states.push_back(ui.snapshot());
  ui.command("assembly_cross_solve");
  auto& groups = ui.widget<QListWidget>("blcad.assembly.solve_group");
  REQUIRE(groups.count() == 1);
  ui.list(groups, 0);
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.capture("combined-assembly-hierarchy-solve");
  ui.button("blcad.assembly.apply");
  CHECK(shared_transform().translation_mm.z == Approx(2).margin(1e-5));
  CHECK(shared_transform().translation_mm.x == Approx(30).margin(1e-5));
  states.push_back(ui.snapshot());
  // Release the seating check before installing a frame-based rotary joint.
  // Editing preserves the relationship identity and its qualified endpoints.
  const auto seated = *ui.window.session().project()->find_cross_hierarchy_constraint(relationship);
  ui.command("assembly_cross_relationship");
  ui.data(ui.widget<QComboBox>("blcad.assembly.cross_record"),
          QString::fromStdString(relationship.value()));
  ui.click(ui.widget<QCheckBox>("blcad.assembly.cross_active"));
  ui.text(ui.widget<QLineEdit>("blcad.assembly.cross_name"), "Seating check retained inactive");
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.assembly.apply");
  const auto* inactive =
      ui.window.session().project()->find_cross_hierarchy_constraint(relationship);
  REQUIRE(inactive);
  CHECK(inactive->state() == AssemblyConstraintState::Inactive);
  CHECK(inactive->target_a() == seated.target_a());
  CHECK(inactive->target_b() == seated.target_b());
  CHECK(ui.window.session().project()->cross_hierarchy_constraint_count() == 1);
  states.push_back(ui.snapshot());

  ui.command("assembly_cross_joint");
  ui.choose(ui.window.assembly_tools().joint_type_editor(),
            static_cast<int>(AssemblyJointType::Revolute));
  ui.text(ui.widget<QLineEdit>("blcad.assembly.cross_name"), "Shared cartridge hinge");
  endpoint(ui.window.assembly_tools().first_editor(), {}, fixed, "feature.hole.seat");
  endpoint(ui.window.assembly_tools().second_editor(), {occurrences[0]}, internal,
           "feature.hole.seat");
  auto& joint_slots = ui.window.assembly_tools().slots_editor();
  REQUIRE(joint_slots.rowCount() == 1);
  ui.cell(joint_slots, 0, 2, "-60");
  ui.cell(joint_slots, 0, 3, "60");
  ui.button("blcad.assembly.apply");
  const auto joint = ui.window.session().project()->cross_hierarchy_joints().back().id();
  CHECK(shared_transform().translation_mm.z == Approx(2).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.command("assembly_cross_motion");
  ui.data(ui.window.assembly_tools().joint_editor(), QString::fromStdString(joint.value()));
  ui.data(ui.window.assembly_tools().coordinate_editor(),
          static_cast<int>(AssemblyJointCoordinateRole::Rotation));
  ui.number(ui.window.assembly_tools().value_editor(), "30");
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.capture("combined-assembly-hierarchy-motion");
  ui.button("blcad.assembly.apply");
  const auto moved = shared_transform();
  CHECK(moved.translation_mm.x == Approx(0).margin(1e-5));
  CHECK(moved.translation_mm.z == Approx(0).margin(1e-5));
  CHECK(moved.rotation_deg.z == Approx(30).margin(1e-5));
  CHECK(ui.window.session().project()->find_cross_hierarchy_joint(joint)->coordinate_deg() == 30);
  for (std::size_t index = 0; index < occurrences.size(); ++index)
    CHECK(ui.window.session()
              .project()
              ->assembly()
              .find_subassembly_instance(occurrences[index])
              ->transform() == boundaries[index]);
  const auto leaves =
      checked(AssemblyLeafOccurrenceResolver{}.resolve(*ui.window.session().project()));
  REQUIRE(leaves.size() == 3);
  for (const auto& path : occurrences) {
    const auto found = std::find_if(leaves.begin(), leaves.end(), [&](const auto& leaf) {
      return leaf.subassembly_path == std::vector<SubassemblyInstanceId>{path} &&
             leaf.component_instance == internal;
    });
    REQUIRE(found != leaves.end());
    REQUIRE_FALSE(found->transforms_inner_to_outer.empty());
    CHECK(found->transforms_inner_to_outer.front() == moved);
  }
  states.push_back(ui.snapshot());
  ui.command("assembly_cross_joint");
  ui.data(ui.widget<QComboBox>("blcad.assembly.cross_record"),
          QString::fromStdString(joint.value()));
  ui.cell(joint_slots, 0, 3, "10");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.assembly.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.cell(joint_slots, 0, 3, "60");
  ui.text(ui.widget<QLineEdit>("blcad.assembly.cross_name"), "Shared cartridge hinge reviewed");
  ui.button("blcad.assembly.apply");
  CHECK(ui.window.session().project()->cross_hierarchy_joint_count() == 1);
  CHECK(ui.window.session().project()->find_cross_hierarchy_joint(joint)->coordinate_deg() == 30);
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("repeated-cartridges.blcad.project.json"));
  const auto persisted = ui.snapshot();

  ui.command("assembly_analysis");
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 2);
  ui.button("blcad.inspect.run");
  auto& results = ui.widget<QTableWidget>("blcad.inspect.results");
  REQUIRE(results.rowCount() == 3);
  const auto nested_semantic = [&](std::size_t index) {
    return "component/" + child.value() + "/" + occurrences[index].value() + "/" + internal.value();
  };
  int repeated_pair = -1;
  for (int row = 0; row < results.rowCount(); ++row) {
    const auto first = results.item(row, 0)->data(Qt::UserRole + 1).toString().toStdString();
    const auto second = results.item(row, 1)->data(Qt::UserRole + 1).toString().toStdString();
    if ((first == nested_semantic(0) && second == nested_semantic(1)) ||
        (first == nested_semantic(1) && second == nested_semantic(0)))
      repeated_pair = row;
  }
  REQUIRE(repeated_pair >= 0);
  CHECK(results.item(repeated_pair, 2)->text().startsWith("Separated:"));
  std::vector<std::string> highlighted;
  for (int column = 0; column < 2; ++column) {
    auto* item = results.item(repeated_pair, column);
    const auto semantic = item->data(Qt::UserRole + 1).toString().toStdString();
    ui.reveal(results);
    results.scrollToItem(item);
    ui.flush();
    ui.click_at(*results.viewport(), results.visualItemRect(item).center());
    REQUIRE(ui.window.viewport().selected_semantic());
    CHECK(ui.window.viewport().selected_semantic()->semantic_id == semantic);
    highlighted.push_back(semantic);
  }
  CHECK(highlighted[0] != highlighted[1]);
  ui.capture("combined-assembly-repeated-occurrence-highlight");
  ui.choose(ui.widget<QComboBox>("blcad.inspect.kind"), 4);
  auto& sweep = ui.widget<QComboBox>("blcad.inspect.sweep_joint");
  ui.choose(sweep, sweep.findData(QString::fromStdString(joint.value()), Qt::UserRole + 1));
  CHECK(sweep.currentData(Qt::UserRole + 2).toInt() == 1);
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_start"), "0");
  ui.number(ui.widget<QDoubleSpinBox>("blcad.inspect.sweep_end"), "30");
  ui.number(ui.widget<QSpinBox>("blcad.inspect.sweep_samples"), "2");
  ui.button("blcad.inspect.run");
  REQUIRE(results.rowCount() == 6);
  for (int row = 0; row < results.rowCount(); ++row)
    CHECK(results.item(row, 2)->data(Qt::UserRole).toDouble() == (row < 3 ? 0 : 30));
  ui.capture("combined-assembly-cross-sweep");
  ui.button("blcad.inspect.close");
  CHECK(ui.snapshot() == persisted);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK_FALSE(ui.window.session().can_undo());
  CHECK_FALSE(ui.window.session().can_redo());
  for (const auto& [command, filename] : std::vector<std::pair<QString, QString>>{
           {"export_assembly_flat_step", "cartridges-flat.step"},
           {"export_assembly_structured_step", "cartridges-structured.step"}}) {
    ui.file(command, directory.filePath(filename));
    CHECK(ui.bytes(directory.filePath(filename)).startsWith("ISO-10303-21;"));
    CHECK(ui.snapshot() == persisted);
  }
  CHECK(ui.bytes(child_file) == saved_child);
  ui.idle();
}

TEST_CASE("Combined UI complex rotor keeps preview recovery history and persistence consistent",
          "[gui][integration][shell-combined-workflow][complex-part-workflow]") {
  WorkflowUi ui;
  ui.fit_first_screen();
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto fixture = test::rotor_performance_part(20);
  const auto source = directory.filePath("rotor.blcad");
  {
    QFile file(source);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(
                QByteArray::fromStdString(checked(serialize_part_document_to_json(fixture)))) > 0);
  }
  ui.file("open", source);
  const BodyId body("body.rotor");
  const double original_volume = ui.summary(body).volume_mm3;
  REQUIRE(ui.summary(body).solid_count == 1);
  std::vector<std::string> states{ui.snapshot()};
  const auto targets = checked(geometry::FinishingSelectionBuilder::build(
      *ui.window.session().part_document(), *ui.window.session().part_shape_cache(), body,
      PartFeatureInputRole::FilletEdge));
  const auto tip = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    return target.anchor.x > 85 && std::abs(target.anchor.y) < 20 && target.anchor.z > 9;
  });
  REQUIRE(tip != targets.end());
  const int tip_row = static_cast<int>(std::distance(targets.begin(), tip));
  const auto begin = [&] {
    ui.command("view_top", 2);
    ui.command("fillet");
    if (QApplication::platformName() == "xcb") {
      const auto point = checked(ui.window.viewport().model_to_screen(tip->anchor));
      REQUIRE(ui.window.viewport().rect().contains(QPoint(qRound(point.x), qRound(point.y))));
      ui.click_at(ui.window.viewport(), {point.x, point.y});
    } else {
      ui.list(ui.widget<QListWidget>("blcad.finishing.targets"), tip_row);
    }
    auto& edges = ui.widget<QListWidget>("blcad.finishing.targets");
    REQUIRE(edges.selectedItems().size() == 1);
    CHECK(edges.row(edges.selectedItems().front()) == tip_row);
    ui.scalar("0.2 mm");
    REQUIRE(ui.window.viewport().feature_preview_active());
  };
  begin();
  const auto accepted = ui.widget<QLineEdit>("blcad.manipulator_numeric_hud").text();
  ui.scalar("100 mm");
  CHECK(ui.widget<QLineEdit>("blcad.manipulator_numeric_hud").text() == accepted);
  CHECK(ui.snapshot() == states.back());
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.capture("rotor-fillet-recovered-preview");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  begin();
  ui.button("blcad.feature.apply");
  const double rounded = ui.summary(body).volume_mm3;
  REQUIRE(rounded < original_volume);
  states.push_back(ui.snapshot());
  ui.command("body_translate");
  ui.list_data(ui.widget<QListWidget>("blcad.body.sources"), "body.rotor");
  ui.scalar("2 mm");
  REQUIRE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(body).volume_mm3 == Approx(rounded).epsilon(1e-10));
  states.push_back(ui.snapshot());
  ui.command("parameters");
  ui.data(ui.widget<QComboBox>("blcad.parameters.parameter"), "hub.height");
  ui.text(ui.widget<QLineEdit>("blcad.parameters.input"), "5");
  ui.button("blcad.parameters.stage");
  ui.button("blcad.parameters.preview");
  REQUIRE(ui.widget<QPushButton>("blcad.parameters.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.parameters.apply");
  // Each spline blade has exact area 4*(90-40)=200 mm² and height 10.
  // The hub/blade overlap is linear in hub height. The tip fillet lies
  // entirely outside the hub, and Translate preserves volume.
  const double expected = rounded - (original_volume - 20 * 200 * 10) / 6;
  // ShapeSummary uses ordinary OCCT mass integration for the spline surfaces.
  CHECK(ui.summary(body).volume_mm3 == Approx(expected).epsilon(1e-8).margin(1e-5));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("edited-rotor.blcad"));
  CHECK(ui.summary(body).volume_mm3 == Approx(expected).epsilon(1e-8).margin(1e-5));
  ui.file("export_part_step", directory.filePath("rotor.step"));
  CHECK(ui.bytes(directory.filePath("rotor.step")).contains("ISO-10303-21"));
  ui.capture("rotor-edited-reopened");
}

TEST_CASE("Combined UI Subtract distinguishes wrong target full removal and an existing void",
          "[gui][integration][shell-combined-workflow][cut-material-workflow]") {
  WorkflowUi ui;
  ui.fit_first_screen();
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Cut outcome recovery");
  ui.rectangle("datum.xy", {0, 0}, {10, 10}, Point2{30, 30});
  const auto target = ui.extrude(10);
  ui.rectangle("datum.xy", {40, 0}, {50, 10}, Point2{70, 30});
  const auto wrong_target = ui.extrude(10);
  ui.rectangle("datum.xy", {4, 4}, {6, 6}, Point2{20, 20});
  const auto tool = ui.extrude(20);
  ui.rectangle("datum.xy", {-1, -1}, {11, 11}, Point2{30, 30});
  const auto covering_tool = ui.extrude(20);
  std::vector<std::string> states{ui.snapshot()};
  const auto begin = [&](const BodyId& body, const BodyId& cutter) {
    ui.command("body_boolean");
    ui.choose(ui.widget<QComboBox>("blcad.boolean.operation"), 1);
    ui.data(ui.widget<QComboBox>("blcad.boolean.target"), QString::fromStdString(body.value()));
    auto& keep = ui.widget<QCheckBox>("blcad.boolean.keep_tools");
    if (!keep.isChecked())
      ui.click(keep);
    ui.list_data(ui.widget<QListWidget>("blcad.body.sources"),
                 QString::fromStdString(cutter.value()));
  };
  const auto invalid = [&](const QString& reason) {
    CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
    CHECK_FALSE(ui.window.viewport().feature_preview_active());
    CHECK(ui.widget<QLabel>("blcad.feature.summary").text().contains(reason));
    const auto before = ui.snapshot();
    const auto history = std::string(ui.window.session().undo_label());
    // The button prevents a user Apply; also prove the command's transaction
    // guard rejects a forced call without publishing a candidate or history.
    REQUIRE_FALSE(ui.window.feature_tools().apply());
    CHECK(ui.snapshot() == before);
    CHECK(ui.window.session().undo_label() == history);
    CHECK(ui.summary(target).volume_mm3 == Approx(1000));
  };
  begin(wrong_target, tool);
  invalid("no measurable material");
  ui.capture("cut-wrong-target-diagnostic");
  ui.data(ui.widget<QComboBox>("blcad.boolean.target"), QString::fromStdString(target.value()));
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  REQUIRE(ui.window.viewport().feature_preview_active());
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());

  begin(target, covering_tool);
  invalid("entire target");
  ui.capture("cut-full-removal-diagnostic");
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  begin(target, tool);
  ui.capture("cut-material-preview");
  ui.button("blcad.feature.apply");
  CHECK(ui.summary(target).volume_mm3 == Approx(960));
  CHECK(ui.summary(wrong_target).volume_mm3 == Approx(1000));
  CHECK(ui.summary(tool).volume_mm3 == Approx(80));
  states.push_back(ui.snapshot());
  // The same tool now lies in an existing void, so a second Subtract must
  // explain its lack of material rather than silently adding an unchanged feature.
  begin(target, tool);
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.widget<QLabel>("blcad.feature.summary").text().contains("no measurable material"));
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  ui.history(states);
  ui.roundtrip(directory.filePath("subtract-recovery.blcad.json"));
  CHECK(ui.summary(target).volume_mm3 == Approx(960));
  ui.file("export_part_step", directory.filePath("subtract-recovery.step"));
  CHECK(ui.bytes(directory.filePath("subtract-recovery.step")).startsWith("ISO-10303-21;"));
  ui.idle();
}

TEST_CASE("Combined UI curved Cut recovers direction and extent on offset and oblique supports",
          "[gui][integration][shell-combined-workflow][cut-material-workflow]") {
  const bool oblique = GENERATE(false, true);
  CAPTURE(oblique);
  WorkflowUi ui;
  ui.fit_first_screen();
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  ui.new_document("Curved Cut diagnostics");
  ui.rectangle("datum.xy", {0, 0}, {20, 20});
  const auto target = ui.extrude(10);
  const auto producer = ui.window.session().part_document()->features().front().id();
  ui.command("fillet");
  ui.list(ui.widget<QListWidget>("blcad.finishing.targets"), 0);
  ui.scalar("0.5 mm");
  ui.button("blcad.feature.apply");
  const auto fillet = ui.window.session().part_document()->fillet_features().back().id();
  const double original = ui.summary(target).volume_mm3;
  CHECK(original < 4000);
  ui.rectangle("datum.xy", {40, 0}, {50, 10}, Point2{70, 30});
  const auto wrong_target = ui.extrude(10);

  ui.command("create_construction");
  ui.choose(ui.widget<QComboBox>("blcad.construction.kind"),
            static_cast<int>(ShellConstructionTools::Kind::Plane));
  for (const auto& [field, value] :
       std::vector<std::pair<QString, QString>>{{"origin.x", oblique ? "20" : "10"},
                                                {"origin.y", "10"},
                                                {"origin.z", "15"},
                                                {"direction.x", "1"},
                                                {"direction.z", oblique ? "-1" : "0"}})
    ui.number(ui.widget<QDoubleSpinBox>("blcad.construction." + field), value);
  ui.button("blcad.construction.apply");
  const auto support =
      ui.window.session().part_document()->construction_planes().back().workplane_id();
  ui.tree(support.value());
  ui.command("create_sketch");
  const auto sketch = *ui.window.active_sketch();
  ui.command("sketch.create.center_radius_circle");
  const auto coordinates = [&](const QString& text) {
    for (const auto character : text)
      ui.key(ui.window, character.unicode(), QString(character));
    ui.key(ui.window, Qt::Key_Return);
  };
  coordinates("0;0");
  coordinates("1;0");
  REQUIRE(ui.window.session().part_document()->find_sketch(sketch)->circle_profiles().size() == 1);
  ui.command("finish_sketch");
  std::vector<std::string> states{ui.snapshot()};
  const auto cut = [&] {
    ui.tree(sketch.value());
    ui.command("extrude");
    ui.choose(ui.widget<QComboBox>("blcad.extrude.operation"), 2);
    ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(target.value()));
  };
  cut();
  ui.scalar("100 mm");
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.widget<QLabel>("blcad.feature.summary").text().contains("no measurable material"));
  CHECK(ui.widget<QLabel>("blcad.extrude.hint").text().contains("direction"));
  const auto history = std::string(ui.window.session().undo_label());
  REQUIRE_FALSE(ui.window.feature_tools().apply());
  CHECK(ui.window.session().undo_label() == history);
  CHECK(ui.snapshot() == states.back());
  ui.capture(oblique ? "cut-oblique-outward-diagnostic" : "cut-offset-outward-diagnostic");
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 1);
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"),
          QString::fromStdString(wrong_target.value()));
  CHECK_FALSE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.widget<QLabel>("blcad.feature.summary").text().contains("no measurable material"));
  ui.data(ui.widget<QComboBox>("blcad.extrude.target"), QString::fromStdString(target.value()));
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.choose(ui.widget<QComboBox>("blcad.extrude.direction"), 0);
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 2);
  ui.text(ui.widget<QLineEdit>("blcad.extrude.second_distance"), "100 mm");
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  CHECK(ui.snapshot() == states.back());
  ui.button("blcad.feature.cancel");
  CHECK(ui.snapshot() == states.back());
  CHECK(ui.summary(target).volume_mm3 == Approx(original));

  cut();
  ui.choose(ui.widget<QComboBox>("blcad.extrude.extent"), 3);
  REQUIRE(ui.widget<QPushButton>("blcad.feature.apply").isEnabled());
  ui.capture(oblique ? "cut-oblique-through-all-preview" : "cut-offset-through-all-preview");
  ui.button("blcad.feature.apply");
  const double normal_z = oblique ? std::sqrt(0.5) : 1;
  CHECK(ui.summary(target).volume_mm3 ==
        Approx(original - std::numbers::pi * 10 / normal_z).margin(1e-5));
  if (ui.window.viewport().native_viewer_available()) {
    ui.data(ui.widget<QComboBox>("blcad.selection.filter"),
            selection_kind_bit(GuiSelectionKind::Body));
    REQUIRE(ui.window.viewport().set_plane_camera(
        {10, 10, 5}, {oblique ? normal_z : 0, 0, normal_z}, {0, 1, 0}));
    auto camera = ui.window.viewport().camera_bookmark();
    camera.target = {10, 10, 5};
    camera.eye = {10 + (oblique ? 100 * normal_z : 0), 10, 5 + 100 * normal_z};
    camera.projection = GuiViewportProjection::Orthographic;
    REQUIRE(ui.window.viewport().restore_camera_bookmark(camera));
    ui.flush();
    const auto initial_center = checked(ui.window.viewport().model_to_screen({10, 10, 5}));
    const auto initial_rim = checked(ui.window.viewport().model_to_screen({10, 11, 5}));
    const double radius_dip =
        std::hypot(initial_rim.x - initial_center.x, initial_rim.y - initial_center.y);
    REQUIRE(std::isfinite(radius_dip));
    REQUIRE(radius_dip > 0);
    // Native V3d bookmarks expose a zoom coefficient. Calibrate from the
    // authoritative projection instead of treating it as a model-space span.
    camera = ui.window.viewport().camera_bookmark();
    camera.scale *= 20 / radius_dip;
    REQUIRE(ui.window.viewport().restore_camera_bookmark(camera));
    ui.flush();
    const auto center_screen = checked(ui.window.viewport().model_to_screen({10, 10, 5}));
    const auto rim_screen = checked(ui.window.viewport().model_to_screen({10, 11, 5}));
    // Keep the hole larger than OCCT's screen-space selection tolerance.
    REQUIRE(std::hypot(rim_screen.x - center_screen.x, rim_screen.y - center_screen.y) > 12);
    const auto pick = [&](Point3 point) {
      const auto screen = checked(ui.window.viewport().model_to_screen(point));
      REQUIRE(ui.window.viewport().rect().contains(QPoint(qRound(screen.x), qRound(screen.y))));
      ui.click_at(ui.window.viewport(), {screen.x, screen.y});
    };
    // Look along the actual cutting axis: the center ray passes through the
    // hole while a neighbouring ray must still pick the retained target body.
    pick({10, 10, 5});
    INFO((ui.window.session().selection().empty()
              ? std::string("clear hole ray")
              : ui.window.session().selection().entries().front().semantic_id));
    CHECK(ui.window.session().selection().empty());
    pick({10, 14, 5});
    REQUIRE(ui.window.session().selection().size() == 1);
    CHECK(ui.window.session().selection().entries().front().semantic_id ==
          "body/" + target.value());
    ui.capture(oblique ? "cut-oblique-native-hole-proof" : "cut-offset-native-hole-proof");
    ui.data(ui.widget<QComboBox>("blcad.selection.filter"), 0xFFFFFFFFU);
  }
  states.push_back(ui.snapshot());
  ui.tree(producer.value(), true);
  ui.widget<QDoubleSpinBox>("blcad.feature_edit.field/length_parameter").setValue(12);
  ui.button("blcad.feature_edit.apply");
  const auto* before_cut = ui.window.session().part_shape_cache()->find_feature_shape(fillet);
  REQUIRE(before_cut);
  const double updated = geometry::RectangleExtrusionAdapter{}.summarize(*before_cut).volume_mm3;
  CHECK(ui.summary(target).volume_mm3 ==
        Approx(updated - std::numbers::pi * 12 / normal_z).margin(1e-5));
  CHECK(ui.summary(wrong_target).volume_mm3 == Approx(1000));
  states.push_back(ui.snapshot());
  ui.history(states);
  ui.roundtrip(directory.filePath("curved-cut.blcad.json"));
  CHECK(ui.summary(target).volume_mm3 ==
        Approx(updated - std::numbers::pi * 12 / normal_z).margin(1e-5));
  ui.file("export_part_step", directory.filePath("curved-cut.step"));
  CHECK(ui.bytes(directory.filePath("curved-cut.step")).startsWith("ISO-10303-21;"));
  ui.idle();
}
