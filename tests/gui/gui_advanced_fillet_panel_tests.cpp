#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QTableWidget>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace {
std::string saved(ShellWindow& w) {
  return f::checked(serialize_part_document_to_json(*w.session().part_document()));
}
void open(ShellWindow& w, QTemporaryDir& directory) {
  QFile file(directory.filePath("input.blcad.json"));
  REQUIRE(file.open(QIODevice::WriteOnly));
  const auto data = f::checked(serialize_part_document_to_json(f::native_base()));
  REQUIRE(file.write(QByteArray::fromStdString(data)) == qint64(data.size()));
  file.close();
  REQUIRE(w.open_path(file.fileName()));
  w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380, 850);
  w.show();
  w.viewport().fit_all();
  qApp->processEvents();
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* value = w.findChild<T*>(name);
  REQUIRE(value);
  return *value;
}
void pick(ShellWindow& w, std::string_view role) {
  const auto items = w.feature_tools().finishing_selection_items();
  const auto it = std::find_if(items.begin(), items.end(),
                               [&](const auto& item) { return item.semantic_id.ends_with(role); });
  REQUIRE(it != items.end());
  REQUIRE(w.viewport().select_semantic(it->semantic_id));
  qApp->processEvents();
}
void capture(ShellWindow& w, QString name) {
  const auto path = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (path.isEmpty())
    return;
  REQUIRE(QDir().mkpath(path));
  qApp->processEvents();
  CHECK(w.grab().save(QDir(path).filePath(name + ".png")));
}
void mouse(ShellWindow& w, QEvent::Type type, GuiSketchScreenPoint point,
           Qt::MouseButtons buttons) {
  const QPointF local(point.x, point.y);
  QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                    Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &event);
  qApp->processEvents();
}
} // namespace
TEST_CASE("Owned advanced Fillet panel creates edits reverses and reopens station laws atomically",
          "[gui][advanced-fillet-panel][parity-b179]") {
  ShellWindow window;
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  open(window, dir);
  auto& tools = window.feature_tools();
  const auto before = saved(window);
  const auto mask = window.viewport().selection_filter_mask();
  REQUIRE(tools.begin_finishing(ShellFeatureTools::Command::Fillet));
  auto& mode = control<QComboBox>(window, "blcad.finishing.mode");
  CHECK(mode.count() == 4);
  mode.setCurrentIndex(1);
  pick(window, "top_front");
  REQUIRE(window.viewport().feature_preview_active());
  auto& stations = control<QTableWidget>(window, "blcad.fillet.stations");
  REQUIRE(stations.rowCount() == 2);
  auto& end = *qobject_cast<QLineEdit*>(stations.cellWidget(1, 3));
  end.setText("1.5");
  REQUIRE(QMetaObject::invokeMethod(&end, "editingFinished", Qt::DirectConnection));
  REQUIRE(window.viewport().feature_preview_active());
  end.setText("40");
  REQUIRE(QMetaObject::invokeMethod(&end, "editingFinished", Qt::DirectConnection));
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(tools.apply());
  CHECK(std::any_of(tools.manipulators().handles().begin(), tools.manipulators().handles().end(),
                    [](const auto& handle) { return handle.id.starts_with("fillet.failure/"); }));
  CHECK(saved(window) == before);
  capture(window, "variable-failure");
  end.setText("1.5");
  REQUIRE(QMetaObject::invokeMethod(&end, "editingFinished", Qt::DirectConnection));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(saved(window) == before);
  capture(window, "variable-stations");
  REQUIRE(tools.apply());
  const auto created = saved(window);
  const auto feature = window.session().part_document()->fillet_features().front();
  const auto intent = std::get<VariableFilletIntent>(feature.advanced_intent()->definition);
  CHECK(window.session()
            .part_document()
            ->find_parameter(intent.groups[0].stations.back().radius)
            ->value()
            .millimeters() == 1.5);
  REQUIRE(window.feature_edit_tools().begin(feature.id()));
  CHECK(mode.currentIndex() == 1);
  CHECK(stations.rowCount() == 2);
  REQUIRE(tools.apply());
  CHECK(saved(window) == created);
  REQUIRE(window.undo());
  CHECK(saved(window) == created); // the no-op edit is still one normal history action
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
  REQUIRE(window.redo());
  CHECK(saved(window) == created);
  REQUIRE(tools.begin_fillet_edit(feature.id()));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &escape);
  CHECK(saved(window) == created);
  CHECK(window.viewport().selection_filter_mask() == mask);
  REQUIRE(window.save_to(dir.filePath("variable.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("variable.blcad.json").toStdString()));
  CHECK(f::checked(serialize_part_document_to_json(*reopened.part_document())) == created);
}
TEST_CASE("Owned face and full-round roles reject duplicates and keep derived size unbound",
          "[gui][advanced-fillet-panel][parity-b179]") {
  ShellWindow window;
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  open(window, dir);
  auto& tools = window.feature_tools();
  const auto before = saved(window);
  REQUIRE(tools.begin_finishing(ShellFeatureTools::Command::Fillet));
  auto& mode = control<QComboBox>(window, "blcad.finishing.mode");
  bool full = false;
  SECTION("G2 face blend") {}
  SECTION("G2 full round") {
    full = true;
  }
  mode.setCurrentIndex(full ? 3 : 2);
  auto& group = control<QComboBox>(window, "blcad.fillet.group");
  pick(window, full ? "front" : "top");
  group.setCurrentIndex(1);
  pick(window, full ? "front" : "top");
  CHECK(tools.finishing_target_editor().selectedItems().empty());
  CHECK(saved(window) == before);
  pick(window, full ? "back" : "front");
  if (full) {
    group.setCurrentIndex(2);
    pick(window, "top");
  }
  control<QComboBox>(window, "blcad.fillet.continuity").setCurrentIndex(1);
  REQUIRE(window.viewport().feature_preview_active());
  capture(window, full ? "full-round-roles" : "face-blend-roles");
  if (full) {
    CHECK(control<QLabel>(window, "blcad.fillet.derived_size").text().contains("5"));
    CHECK(tools.manipulators().handles().empty());
  }
  REQUIRE(tools.apply());
  const auto after = saved(window);
  const auto feature = window.session().part_document()->fillet_features().front();
  CHECK(feature.parameter_dependencies().size() == (full ? 0U : 1U));
  REQUIRE(tools.begin_fillet_edit(feature.id()));
  CHECK(mode.currentIndex() == (full ? 3 : 2));
  REQUIRE(tools.apply());
  CHECK(saved(window) == after);
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK(saved(window) == before);
  REQUIRE(window.redo());
  CHECK(saved(window) == after);
}
TEST_CASE(
    "Native advanced stations drag only their bound radius and show the physical law direction",
    "[gui][native-advanced-fillet][parity-b179]") {
  if (QApplication::platformName() == "offscreen")
    SKIP("Requires the native OCCT viewer");
  ShellWindow window;
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  open(window, dir);
  REQUIRE(window.viewport().native_viewer_available());
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_finishing(ShellFeatureTools::Command::Fillet));
  control<QComboBox>(window, "blcad.finishing.mode").setCurrentIndex(1);
  pick(window, "top_front");
  REQUIRE(window.viewport().feature_preview_active());
  const auto before = saved(window);
  const auto guides = tools.manipulators().presentations();
  REQUIRE(guides.size() == 3);
  const auto handle =
      std::find_if(guides.begin(), guides.end(), [](const auto& p) { return p.interactive; });
  REQUIRE(handle != guides.end());
  const auto direction =
      std::find_if(guides.begin(), guides.end(), [](const auto& p) { return !p.interactive; });
  REQUIRE(direction != guides.end());
  CHECK(direction->value_text == "Law Direction");
  mouse(window, QEvent::MouseButtonPress, handle->endpoint, Qt::LeftButton);
  REQUIRE(tools.manipulators().active_handle_id() == handle->handle_id);
  REQUIRE(tools.set_numeric_value(1.25));
  mouse(window, QEvent::MouseButtonRelease, handle->endpoint, Qt::NoButton);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(saved(window) == before);
  capture(window, "native-variable-drag");
  REQUIRE(tools.apply());
  const auto& feature = window.session().part_document()->fillet_features().front();
  const auto& group =
      std::get<VariableFilletIntent>(feature.advanced_intent()->definition).groups[0];
  CHECK(window.session()
            .part_document()
            ->find_parameter(group.stations.front().radius)
            ->value()
            .millimeters() == Catch::Approx(1.25));
  CHECK(window.session()
            .part_document()
            ->find_parameter(group.stations.back().radius)
            ->value()
            .millimeters() == Catch::Approx(2));
}
