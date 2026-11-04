#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_parameter_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T& control(QWidget& widget, const char* id) {
  auto* result = widget.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(result);
  return *result;
}

void click(QWidget& widget) {
  bool scrolled = false;
  for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
      scroll->ensureWidgetVisible(&widget);
      scrolled = true;
    }
  if (scrolled)
    QApplication::processEvents();
  const QPoint point = widget.rect().center();
  REQUIRE(widget.window()->childAt(widget.mapTo(widget.window(), point)) == &widget);
  const QPoint global = widget.mapToGlobal(point);
  QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QApplication::sendEvent(&widget, &release);
  QApplication::processEvents();
}

void show(ShellWindow& window, QSize size = QSize(1280, 800)) {
  size = size.boundedTo(QGuiApplication::primaryScreen()->availableGeometry().size());
  window.resize(size);
  window.show();
  QApplication::processEvents();
  if (QApplication::platformName() == QStringLiteral("xcb"))
    REQUIRE(window.viewport().native_viewer_available());
}

void capture(ShellWindow& window, const QString& name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!dir.isEmpty())
    REQUIRE(window.grab().save(dir + '/' + name + QStringLiteral(".png")));
}
} // namespace

TEST_CASE("Workspace keeps ribbon and quick access above docks and restores hidden panels",
          "[gui][shell][workflow-layout]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Layout review")));
  show(window);
  auto& browser = control<QDockWidget>(window, "blcad.shell.model_dock");
  auto& properties = control<QDockWidget>(window, "blcad.shell.property_dock");
  auto& diagnostics = control<QDockWidget>(window, "blcad.shell.diagnostics_dock");
  CHECK(window.ribbon().width() == window.width());
  CHECK(browser.mapTo(&window, QPoint()).y() >= window.ribbon().height());
  CHECK_FALSE(diagnostics.isVisible());
  const auto revision = window.session().presentation_revision();
  properties.close();
  REQUIRE_FALSE(properties.isVisible());
  window.ribbon().set_current_tab(6);
  QToolButton* toggle = nullptr;
  for (auto* button : window.ribbon().findChildren<QToolButton*>())
    if (button->defaultAction() == properties.toggleViewAction())
      toggle = button;
  REQUIRE(toggle);
  REQUIRE(toggle->isVisible());
  click(*toggle);
  CHECK(properties.isVisible());
  window.addDockWidget(Qt::RightDockWidgetArea, &browser);
  control<QAction>(window, "blcad.action.reset_layout").trigger();
  CHECK(window.dockWidgetArea(&browser) == Qt::LeftDockWidgetArea);
  CHECK(window.dockWidgetArea(&properties) == Qt::LeftDockWidgetArea);
  CHECK(window.session().presentation_revision() == revision);
  browser.close();
  CHECK_FALSE(browser.isVisible());
  control<QAction>(window, "blcad.action.reset_layout").trigger();
  CHECK(browser.isVisible());
  window.ribbon().set_current_tab(0);
  capture(window, QStringLiteral("workflow-model"));

  auto& file = control<QToolButton>(window, "blcad.ribbon.file");
  REQUIRE(file.menu());
  CHECK(file.menu()->actions().contains(&control<QAction>(window, "blcad.action.save_as")));
  CHECK(file.menu()->actions().contains(&control<QAction>(window, "blcad.action.new_project")));
  auto& save = control<QToolButton>(window, "blcad.quick.blcad.action.save");
  for (int tab = 0; tab < window.ribbon().tab_count(); ++tab) {
    if (!window.ribbon().tab_enabled(tab))
      continue;
    window.ribbon().set_current_tab(tab);
    QApplication::processEvents();
    CHECK(save.isVisible());
    CHECK(save.defaultAction() == &control<QAction>(window, "blcad.action.save"));
    CHECK(file.isVisible());
  }
}

TEST_CASE("Command search runs existing actions and keeps unavailable commands inert",
          "[gui][shell][workflow-layout]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Command discovery")));
  show(window);
  auto& search = control<QToolButton>(window, "blcad.ribbon.search");
  bool entered = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    REQUIRE(dialog);
    entered = true;
    auto& query = control<QLineEdit>(*dialog, "blcad.command_search.query");
    auto& results = control<QTreeWidget>(*dialog, "blcad.command_search.results");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(buttons);
    CHECK(buttons->button(QDialogButtonBox::Ok)->icon().name() == QStringLiteral("check"));
    CHECK(buttons->button(QDialogButtonBox::Cancel)->icon().name() == QStringLiteral("close"));
    query.setText(QStringLiteral("extrude"));
    REQUIRE(results.topLevelItemCount() == 1);
    CHECK(results.topLevelItem(0)->text(1) == QStringLiteral("3D Model / Create"));
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK_FALSE(window.session().task().active());
    query.setText(QStringLiteral("no such command"));
    CHECK(results.topLevelItemCount() == 0);
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    query.setText(QStringLiteral("manage parameter"));
    REQUIRE(results.topLevelItemCount() == 1);
    REQUIRE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    click(*buttons->button(QDialogButtonBox::Ok));
  });
  click(search);
  REQUIRE(entered);
  CHECK(window.ribbon().current_tab() == 5);
  CHECK(window.parameter_tools().active());
  CHECK(window.ribbon().width() == window.width());
  CHECK(window.session().task().active());
  // Panel recovery must not dismiss or apply the active transaction.
  control<QAction>(window, "blcad.action.reset_layout").trigger();
  CHECK(window.parameter_tools().active());
  capture(window, QStringLiteral("workflow-parameters"));
}

TEST_CASE("Command search includes menu variants once and follows owner availability",
          "[gui][shell][workflow-layout]") {
  ShellRibbon ribbon;
  const auto tab = ribbon.add_tab(QStringLiteral("Sketch"));
  auto* group = ribbon.add_group(tab, QStringLiteral("Draw"));
  QAction variant(QStringLiteral("Rectangle from Center"), &ribbon);
  variant.setShortcut(QKeySequence(QStringLiteral("Ctrl+9")));
  ribbon.add_menu_button(group, QStringLiteral("Rectangle"), {&variant});
  ribbon.add_small_button(group, &variant);
  int triggered = 0;
  QObject::connect(&variant, &QAction::triggered, [&] { ++triggered; });
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    REQUIRE(dialog);
    auto& query = control<QLineEdit>(*dialog, "blcad.command_search.query");
    auto& results = control<QTreeWidget>(*dialog, "blcad.command_search.results");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(buttons);
    query.setText(QStringLiteral("rectangle CENTER"));
    REQUIRE(results.topLevelItemCount() == 1);
    CHECK(results.topLevelItem(0)->text(2) == QStringLiteral("Ctrl+9"));
    variant.setEnabled(false);
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    variant.setEnabled(true);
    REQUIRE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    click(*buttons->button(QDialogButtonBox::Ok));
  });
  ribbon.show_command_search();
  CHECK(triggered == 1);
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    REQUIRE(dialog);
    dialog->reject();
  });
  ribbon.show_command_search();
  CHECK(triggered == 1);
}

TEST_CASE("Compact workspace retains reachable task actions and full width command surface",
          "[gui][shell][workflow-layout]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Compact workspace")));
  show(window, QSize(1024, 640));
  REQUIRE(window.parameter_tools().begin());
  QApplication::processEvents();
  CHECK(window.ribbon().width() == window.width());
  CHECK(window.viewport().width() >= 360);
  auto& search = control<QToolButton>(window, "blcad.ribbon.search");
  const QRect bounds(search.mapTo(&window, QPoint()), search.size());
  CHECK(window.rect().contains(bounds));
  CHECK(window.childAt(search.mapTo(&window, search.rect().center())) == &search);
  capture(window, QStringLiteral("workflow-compact"));
}

TEST_CASE("Ctrl K opens command discovery and arrow keys navigate without editing the model",
          "[gui][shell][workflow-layout]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Keyboard discovery")));
  show(window);
  window.raise();
  window.activateWindow();
  window.viewport().setFocus();
  QGuiApplication::sync();
  QApplication::processEvents();
  const auto revision = window.session().presentation_revision();
  bool handled = false;
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, [&] {
    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
      dialog->reject();
  });
  const auto key = [](QWidget& widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, code, modifiers);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers);
    QApplication::sendEvent(&widget, &press);
    QApplication::sendEvent(&widget, &release);
  };
  QTimer::singleShot(50, &window, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog)
      return;
    handled = true;
    dialog->activateWindow();
    auto& query = control<QLineEdit>(*dialog, "blcad.command_search.query");
    query.setFocus();
    QGuiApplication::sync();
    QApplication::processEvents();
    auto& results = control<QTreeWidget>(*dialog, "blcad.command_search.results");
    query.setText(QStringLiteral("view"));
    REQUIRE(results.topLevelItemCount() > 1);
    const auto row = results.indexOfTopLevelItem(results.currentItem());
    key(query, Qt::Key_Down);
    CHECK(results.indexOfTopLevelItem(results.currentItem()) == row + 1);
    key(query, Qt::Key_Up);
    CHECK(results.indexOfTopLevelItem(results.currentItem()) == row);
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (!directory.isEmpty())
      REQUIRE(dialog->grab().save(directory + QStringLiteral("/workflow-command-search.png")));
    key(query, Qt::Key_Escape);
  });
  watchdog.start(3000);
  key(window.viewport(), Qt::Key_K, Qt::ControlModifier);
  watchdog.stop();
  REQUIRE(handled);
  CHECK(window.session().presentation_revision() == revision);
  CHECK_FALSE(window.session().task().active());
}
