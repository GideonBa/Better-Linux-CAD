#include "../support/hole_pattern_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace plate_fixture = blcad::test::hole_pattern_fixture;
namespace thread_fixture = blcad::test::thread_fixture;
namespace {
template <class T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
template <class T> T& control(QWidget& root, const char* name) {
  auto* found = root.findChild<T*>(QString::fromLatin1(name));
  INFO(name);
  REQUIRE(found);
  return *found;
}
template <class T> T& field(ShellWindow& window, const char* path) {
  auto* found = qobject_cast<T*>(window.feature_edit_tools().field(QString::fromLatin1(path)));
  INFO(path);
  REQUIRE(found);
  return *found;
}
void settle() {
  for (int i = 0; i < 5; ++i)
    qApp->processEvents();
}
void visible(QWidget& widget) {
  for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent))
      scroll->ensureWidgetVisible(&widget);
  widget.setFocus();
  settle();
  REQUIRE(widget.isVisible());
}
void key(QWidget& widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
         const QString& text = {}) {
  for (const auto type : {QEvent::KeyPress, QEvent::KeyRelease}) {
    QKeyEvent event(type, code, modifiers, text);
    QApplication::sendEvent(&widget, &event);
  }
  settle();
}
void text(QLineEdit& input, const QString& value) {
  visible(input);
  input.selectAll();
  key(input, Qt::Key_unknown, Qt::NoModifier, value);
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_part_document_to_json(*window.session().part_document()));
}
void open(ShellWindow& window, QTemporaryDir& directory, const PartDocument& part) {
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("acceptance.blcad.json");
  REQUIRE(write_part_document_json_file(part, path.toStdString()));
  REQUIRE(window.open_path(path));
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  if (QApplication::platformName() == "xcb")
    size = size.boundedTo(window.screen()->availableGeometry().size() - QSize(48, 48));
  window.resize(size);
  window.show();
  window.raise();
  window.activateWindow();
  settle();
  window.viewport().fit_all();
  settle();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
double volume(ShellWindow& window, const char* id) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(BodyId(id));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  settle();
  REQUIRE(window.grab().save(QDir(directory).filePath(QString::fromLatin1(name) + ".png")));
}
void task_layout(ShellWindow& window) {
  for (const auto* name : {"blcad.feature_edit.apply", "blcad.feature_edit.cancel"}) {
    auto& button = control<QPushButton>(window, name);
    CHECK(button.isVisibleTo(&window));
    CHECK(window.rect().contains(button.mapTo(&window, button.rect().center())));
  }
  auto& guidance = control<QLabel>(window, "blcad.feature_edit.guidance");
  const auto required = guidance.heightForWidth(guidance.width());
  REQUIRE(required > 0);
  CHECK(guidance.height() >= required);
  CHECK_FALSE(control<QDockWidget>(window, "blcad.shell.diagnostics_dock").isVisible());
}
} // namespace

TEST_CASE("Hole and Thread keyboard input validates focused values and publishes one operation",
          "[gui][parity-b157][hole-thread-keyboard-shell]") {
  const bool thread = GENERATE(false, true);
  CAPTURE(thread);
  auto part = thread ? thread_fixture::pair_part() : plate_fixture::part(false, false);
  if (!thread)
    REQUIRE(part.remove_hole_feature(FeatureId("holes")));
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, part);
  const auto before = snapshot(window);
  auto& action = control<QAction>(window, thread ? "blcad.action.semantic_thread"
                                                 : "blcad.action.semantic_hole");
  REQUIRE(action.isEnabled());
  action.trigger();
  settle();
  REQUIRE(window.feature_edit_tools().active());
  CHECK_FALSE(control<QAction>(window, "blcad.action.semantic_hole").isEnabled());
  CHECK_FALSE(control<QAction>(window, "blcad.action.semantic_thread").isEnabled());
  auto& number = field<QDoubleSpinBox>(window, thread ? "/thread/length" : "/diameter/parameter");
  auto& input = control<QLineEdit>(number, "qt_spinbox_lineedit");
  const auto initial = QString::number(number.value(), 'g', 12);
  text(input, ".");
  REQUIRE_FALSE(number.hasAcceptableInput());
  key(input, Qt::Key_Return, Qt::ControlModifier);
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_edit_tools().active());
  CHECK_FALSE(control<QPushButton>(window, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QLabel>(window, "blcad.feature_edit.status").text().contains("focused numeric"));
  CHECK_FALSE(number.hasAcceptableInput());
  const auto* focused = QApplication::focusWidget();
  REQUIRE(focused);
  CHECK((focused == &number || number.isAncestorOf(focused)));
  capture(window, thread ? "thread-keyboard-invalid" : "hole-keyboard-invalid");
  text(input, initial);
  key(input, Qt::Key_Return);
  CHECK(window.feature_edit_tools().active());
  REQUIRE(control<QPushButton>(window, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(window) == before);
  text(input, "800");
  key(input, Qt::Key_Return, Qt::ControlModifier);
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_edit_tools().active());
  CHECK_FALSE(control<QPushButton>(window, "blcad.feature_edit.apply").isEnabled());
  task_layout(window);
  text(input, thread ? "2" : "4");
  key(input, Qt::Key_Return);
  REQUIRE(control<QPushButton>(window, "blcad.feature_edit.apply").isEnabled());
  const auto* old_focus = QApplication::focusWidget();
  key(input, Qt::Key_Tab);
  CHECK(QApplication::focusWidget() != old_focus);
  CHECK(snapshot(window) == before);
  auto& name = field<QLineEdit>(window, "/name");
  const QString expected_name = thread ? "Keyboard Thread" : "Keyboard Hole";
  text(name, expected_name);
  const auto saved_preview = directory.filePath("keyboard-preview.blcad.json");
  REQUIRE(window.save_to(saved_preview));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(saved_preview.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  task_layout(window);
  capture(window, thread ? "thread-keyboard-preview" : "hole-keyboard-preview");
  key(name, Qt::Key_Return, Qt::ControlModifier);
  REQUIRE_FALSE(window.feature_edit_tools().active());
  CHECK_FALSE(window.session().task().active());
  const auto& accepted = *window.session().part_document();
  if (thread) {
    REQUIRE(accepted.thread_features().size() == 1);
    CHECK(accepted.thread_features().front().name() == expected_name.toStdString());
    CHECK(volume(window, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  } else {
    REQUIRE(accepted.hole_features().size() == 1);
    CHECK(accepted.hole_features().front().name() == expected_name.toStdString());
    CHECK(volume(window, "body.base") == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  }
  const auto after = snapshot(window);
  auto& repeat = control<QAction>(window, "blcad.action.repeat_last_command");
  REQUIRE(repeat.isEnabled());
  repeat.trigger();
  settle();
  REQUIRE(window.feature_edit_tools().active());
  auto& repeated = field<QDoubleSpinBox>(window, thread ? "/thread/length" : "/diameter/parameter");
  auto& repeated_input = control<QLineEdit>(repeated, "qt_spinbox_lineedit");
  text(repeated_input, "7");
  key(repeated_input, Qt::Key_Escape);
  REQUIRE_FALSE(window.feature_edit_tools().active());
  CHECK(snapshot(window) == after);
  CHECK_FALSE(window.session().task().active());
  CHECK(window.session().undo_label() == (thread ? "Create Thread" : "Create Hole"));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  const auto path = directory.filePath("keyboard-accepted.blcad.json");
  REQUIRE(window.save_to(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
  capture(window, thread ? "thread-keyboard-accepted" : "hole-keyboard-accepted");
}
