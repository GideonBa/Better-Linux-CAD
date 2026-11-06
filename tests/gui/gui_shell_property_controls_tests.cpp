#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T& widget(QWidget& root, const char* name) {
  auto* result = root.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(result);
  return *result;
}

void click(QWidget& target) {
  for (auto* parent = target.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent))
      scroll->ensureWidgetVisible(&target);
  QApplication::processEvents();
  REQUIRE(target.isVisible());
  REQUIRE(target.window()->childAt(target.mapTo(target.window(), target.rect().center())) ==
          &target);
  const QPointF point = target.rect().center();
  const QPointF global = target.mapToGlobal(point.toPoint());
  QMouseEvent down(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                   Qt::NoModifier);
  QMouseEvent up(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                 Qt::NoModifier);
  QApplication::sendEvent(&target, &down);
  QApplication::sendEvent(&target, &up);
  QApplication::processEvents();
}

void key(QWidget& target, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
         const QString& text = {}) {
  QKeyEvent down(QEvent::KeyPress, key, modifiers, text);
  QKeyEvent up(QEvent::KeyRelease, key, modifiers, text);
  QApplication::sendEvent(&target, &down);
  QApplication::sendEvent(&target, &up);
  QApplication::processEvents();
}

std::string snapshot(ShellWindow& window) {
  auto source = serialize_part_document_to_json(*window.session().part_document());
  REQUIRE(source);
  return source.value();
}

void prepare(ShellWindow& window) {
  window.resize(
      QSize(1280, 800).boundedTo(QGuiApplication::primaryScreen()->availableGeometry().size()));
  window.show();
  window.raise();
  window.activateWindow();
  QApplication::processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
  REQUIRE(window.new_part(QStringLiteral("Property workflow")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({5, 5}));
  REQUIRE(window.sketch_tools().add_pick({25, 15}));
  REQUIRE(window.finish_active_sketch());
}

void capture(ShellWindow& window, const QString& name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(window.grab().save(directory + '/' + name + ".png"));
}
} // namespace

TEST_CASE(
    "Property groups expose collapsed diagnostics and semantic selectors retain owner authority",
    "[gui][parity-b145][property-controls][property-controls-widget]") {
  QWidget root;
  auto* column = new QVBoxLayout(&root);
  auto* group = new ShellPropertyGroup("test.group", "Input Geometry", &root);
  column->addWidget(group);
  auto* choices = new QComboBox(&group->content());
  choices->addItem("First face", "face.first");
  choices->addItem("Second face", "face.second");
  int picks = 0;
  auto* owner = new QObject(&root);
  auto* selector = new ShellPropertySelector(
      "test.selector", "Face", *choices, *owner, [&](bool active) { picks += active ? 1 : -1; },
      &group->content());
  group->content_layout().addWidget(selector);
  root.resize(460, 320);
  root.show();
  QApplication::processEvents();
  CHECK(widget<QLabel>(root, "test.selector.count").text() == "1 selected");
  click(widget<QToolButton>(root, "test.selector.pick"));
  CHECK(selector->active());
  CHECK(picks == 1);
  selector->set_active(false);
  CHECK(picks == 1); // Presentation synchronization does not request another pick.
  click(widget<QToolButton>(root, "test.selector.clear"));
  CHECK(choices->currentIndex() == -1);
  CHECK(widget<QLabel>(root, "test.selector.count").text() == "0 selected");
  CHECK_FALSE(widget<QToolButton>(root, "test.selector.clear").isEnabled());
  {
    const QSignalBlocker guard(choices);
    choices->setCurrentIndex(1);
  }
  selector->set_active(false);
  CHECK(choices->currentData().toString() == "face.second");
  CHECK(widget<QLabel>(root, "test.selector.count").text() == "1 selected");
  choices->setEnabled(false);
  CHECK_FALSE(widget<QToolButton>(root, "test.selector.pick").isEnabled());
  CHECK_FALSE(widget<QToolButton>(root, "test.selector.clear").isEnabled());
  choices->setEnabled(true);
  CHECK(widget<QToolButton>(root, "test.selector.pick").isEnabled());
  group->set_diagnostic("Select <the correct face> before Apply");
  auto& heading = widget<QToolButton>(root, "test.group.toggle");
  heading.setFocus();
  key(heading, Qt::Key_Space, Qt::NoModifier, " ");
  CHECK_FALSE(group->expanded());
  CHECK_FALSE(selector->isVisible());
  CHECK(heading.text().contains("(!)"));
  CHECK(heading.accessibleDescription().contains("<the correct face>"));
  CHECK(widget<QLabel>(root, "test.group.diagnostic").textFormat() == Qt::PlainText);
  click(heading);
  CHECK(group->expanded());
  group->set_diagnostic({});
  CHECK_FALSE(heading.text().contains("(!)"));
  delete owner;
  click(widget<QToolButton>(root, "test.selector.pick"));
  CHECK(picks == 1); // A retired command owner cannot be called by retained widgets.
}

TEST_CASE("Extrude property groups preserve candidates diagnostics and compact completion controls",
          "[gui][parity-b145][property-controls][property-controls-widget]") {
  ShellWindow window;
  prepare(window);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  REQUIRE(window.feature_tools().begin_extrude());
  QApplication::processEvents();
  CHECK(widget<QLabel>(window, "blcad.extrude.profile_summary")
            .text()
            .contains(QString::fromStdString(
                window.session().part_document()->sketches().front().name())));
  auto& advanced = widget<QToolButton>(window, "blcad.extrude.advanced_group.toggle");
  auto& taper = widget<QLineEdit>(window, "blcad.extrude.taper");
  click(taper);
  taper.selectAll();
  key(taper, Qt::Key_unknown, Qt::NoModifier, "100 deg");
  key(taper, Qt::Key_Return);
  CHECK_FALSE(widget<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK(advanced.text().contains("(!)"));
  click(advanced);
  CHECK_FALSE(taper.isVisible());
  CHECK(advanced.text().contains("(!)"));
  CHECK(snapshot(window) == before);
  click(advanced);
  click(taper);
  taper.selectAll();
  key(taper, Qt::Key_Backspace);
  key(taper, Qt::Key_Return);
  REQUIRE(widget<QPushButton>(window, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(advanced.text().contains("(!)"));
  window.resize(
      QSize(1024, 640).boundedTo(QGuiApplication::primaryScreen()->availableGeometry().size()));
  QApplication::processEvents();
  auto& apply = widget<QPushButton>(window, "blcad.feature.apply");
  auto& cancel = widget<QPushButton>(window, "blcad.feature.cancel");
  CHECK(apply.isVisible());
  CHECK(cancel.isVisible());
  CHECK(window.rect().contains(apply.mapTo(&window, apply.rect().center())));
  CHECK(window.rect().contains(cancel.mapTo(&window, cancel.rect().center())));
  capture(window, "property-groups-compact");
  click(cancel);
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
}

TEST_CASE("Ctrl Enter validates the focused feature value before committing exactly one extrusion",
          "[gui][parity-b145][property-controls][property-controls-widget]") {
  ShellWindow window;
  prepare(window);
  const auto before = snapshot(window);
  REQUIRE(window.feature_tools().begin_extrude());
  QApplication::processEvents();
  auto& input = window.feature_tools().numeric_editor();
  click(input);
  input.selectAll();
  key(input, Qt::Key_unknown, Qt::NoModifier, "8 deg");
  key(input, Qt::Key_Return, Qt::ControlModifier);
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_tools().active());
  click(input);
  input.selectAll();
  key(input, Qt::Key_unknown, Qt::NoModifier, "8 mm");
  REQUIRE(input.text() == "8 mm");
  key(input, Qt::Key_Return, Qt::ControlModifier);
  INFO(widget<QLabel>(window, "blcad.feature.summary").text().toStdString());
  REQUIRE_FALSE(window.feature_tools().active());
  REQUIRE(window.session().part_document()->features().size() == 1);
  const auto* shape = window.session().part_shape_cache()->final_shape();
  REQUIRE(shape);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
        Catch::Approx(1600).margin(1e-6));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().redo());
  REQUIRE(window.session().part_document()->features().size() == 1);
}
