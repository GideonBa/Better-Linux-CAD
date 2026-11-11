#include "../support/hole_pattern_fixture.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::hole_pattern_fixture;
namespace {
using Command = ShellFeatureTools::Command;
template <class T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* found = w.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(found);
  return *found;
}
void settle() {
  for (int i = 0; i < 5; ++i)
    qApp->processEvents();
}
void click(QWidget& w, QPoint point) {
  REQUIRE(w.isVisible());
  REQUIRE(w.isEnabled());
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent e(type, point, w.mapToGlobal(point), Qt::LeftButton,
                  type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&w, &e);
    settle();
  }
}
std::string snapshot(ShellWindow& w) {
  return checked(serialize_part_document_to_json(*w.session().part_document()));
}
void open(ShellWindow& w, QTemporaryDir& dir, bool multiple = true,
          PartDocument p = f::part(true)) {
  if (!multiple)
    p = f::part();
  REQUIRE(dir.isValid());
  auto path = dir.filePath("hole-pattern.blcad.json");
  REQUIRE(write_part_document_json_file(p, path.toStdString()));
  REQUIRE(w.open_path(path));
  QSize size(qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 1024 : 1280,
             qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? 640 : 800);
  if (QApplication::platformName() == "xcb")
    size = size.boundedTo(w.screen()->availableGeometry().size() - QSize(48, 48));
  w.resize(size);
  w.show();
  settle();
  w.viewport().fit_all();
  settle();
}
void axis(ShellWindow& w, QComboBox& editor, const char* id) {
  const auto* value = w.session().part_document()->find_datum_axis(DatumAxisId(id));
  REQUIRE(value);
  auto index = editor.findText(QString::fromStdString(value->name()));
  REQUIRE(index >= 0);
  editor.setCurrentIndex(index);
}
void begin(ShellWindow& w, Command c = Command::LinearPattern, bool select = true) {
  auto& t = w.feature_tools();
  REQUIRE(t.begin_pattern(c));
  control<QComboBox>(w, "blcad.pattern.source_mode").setCurrentIndex(1);
  axis(w, t.pattern_reference_editor(), c == Command::LinearPattern ? "axis.x" : "axis");
  REQUIRE(t.pattern_source_editor().count() == 1);
  if (select) {
    t.pattern_source_editor().item(0)->setSelected(true);
    REQUIRE(t.set_numeric_value(c == Command::LinearPattern ? 12 : 120));
    REQUIRE(w.viewport().feature_preview_active());
  }
}
void second(ShellWindow& w) {
  axis(w, control<QComboBox>(w, "blcad.pattern.second.reference"), "axis.y");
  control<QGroupBox>(w, "blcad.pattern.second.enabled").setChecked(true);
  INFO(control<QLabel>(w, "blcad.feature.summary").text().toStdString());
  REQUIRE(w.viewport().feature_preview_active());
}
QListWidgetItem& occurrence(ShellWindow& w, unsigned i, unsigned j) {
  auto& list = control<QListWidget>(w, "blcad.pattern.skipped");
  for (int row = 0; row < list.count(); ++row) {
    auto* item = list.item(row);
    if (item->data(Qt::UserRole).toUInt() == i && item->data(Qt::UserRole + 1).toUInt() == j)
      return *item;
  }
  FAIL("Missing occurrence");
  return *list.item(0);
}
double volume(ShellWindow& w) {
  const auto* s = w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(s);
  return geometry::RectangleExtrusionAdapter{}.summarize(*s).volume_mm3;
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  settle();
  REQUIRE(w.grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png")));
}
void replay(ShellWindow& w, QTemporaryDir& dir, const std::string& before) {
  const auto after = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == after);
  const auto path = dir.filePath("reopened.blcad.json");
  REQUIRE(w.save_to(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
}
} // namespace
TEST_CASE("Hole Pattern preview retains the active exclusion row throughout checkbox dispatch",
          "[gui][parity-b156][hole-pattern-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = snapshot(w);
  begin(w);
  second(w);
  auto& list = control<QListWidget>(w, "blcad.pattern.skipped");
  auto& item = occurrence(w, 2, 1);
  const QPersistentModelIndex index(list.model()->index(list.row(&item), 0));
  for (const auto state : {Qt::Checked, Qt::Unchecked, Qt::Checked}) {
    item.setCheckState(state);
    REQUIRE(index.isValid());
    REQUIRE(&occurrence(w, 2, 1) == &item);
    CHECK(item.checkState() == state);
    REQUIRE(w.viewport().feature_preview_active());
  }
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_tools().apply());
  CHECK((w.session().part_document()->linear_pattern_features().front().placements()->skipped ==
         std::vector<PatternOccurrenceIndex>{{2, 1}}));
  replay(w, dir, before);
}
TEST_CASE("Pattern task authors Hole directions dormant exclusions and one atomic in-place result",
          "[gui][parity-b156][hole-pattern-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = snapshot(w);
  const auto parameters = w.session().part_document()->parameters().size();
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  begin(w);
  second(w);
  control<QPushButton>(w, "blcad.pattern.second.pick_reference").setChecked(true);
  CHECK(w.viewport().preview_selection_target_count() == 0);
  CHECK(w.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Datum));
  REQUIRE(w.viewport().select_semantic("datum-axis/axis.y"));
  CHECK(w.feature_tools().pattern_source_editor().selectedItems().size() == 1);
  CHECK(w.viewport().feature_preview_replaces("body/body.base"));
  CHECK(w.viewport().preview_selection_target_count() == 2);
  CHECK(w.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Face));
  occurrence(w, 2, 1).setCheckState(Qt::Checked);
  REQUIRE(w.feature_tools().set_pattern_count(1));
  CHECK(occurrence(w, 2, 1).text().contains("Outside"));
  CHECK(occurrence(w, 2, 1).checkState() == Qt::Checked);
  REQUIRE(w.feature_tools().set_pattern_count(3));
  CHECK(occurrence(w, 2, 1).checkState() == Qt::Checked);
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_tools().apply());
  const auto& p = *w.session().part_document();
  REQUIRE(p.linear_pattern_features().size() == 1);
  const auto& pattern = p.linear_pattern_features().front();
  REQUIRE(pattern.placements());
  REQUIRE(pattern.placements()->second_direction);
  CHECK((pattern.placements()->skipped == std::vector<PatternOccurrenceIndex>{{2, 1}}));
  CHECK(pattern.sources().front().kind() == PatternSourceKind::Feature);
  CHECK(pattern.body_result_context().operation_mode() == FeatureBodyOperationMode::Cut);
  CHECK(p.bodies().size() == 1);
  CHECK(p.parameters().size() == parameters + 4);
  CHECK(volume(w) == Catch::Approx(38400 - 10 * 17.5 * std::numbers::pi).margin(1e-6));
  auto ref = checked(HoleInstanceReference::create(FeatureId("holes"), HolePlacementId("first"),
                                                   HoleReferenceRole::Seat,
                                                   HolePatternOccurrence{pattern.id(), {1, 1}}));
  const auto frame =
      checked(geometry::HoleInstanceGeometry::resolve(p, ref, *w.session().part_shape_cache()));
  CHECK(frame.frame.origin.x == Catch::Approx(-8));
  CHECK(frame.frame.origin.y == Catch::Approx(0).margin(1e-7));
  CHECK(frame.frame.origin.z == Catch::Approx(7));
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  CHECK(w.viewport().preview_selection_target_count() == 0);
  replay(w, dir, before);
}
TEST_CASE(
    "Pattern task diagnoses invalid directions and cost without losing intent or draft isolation",
    "[gui][parity-b156][hole-pattern-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = snapshot(w);
  begin(w);
  second(w);
  occurrence(w, 2, 1).setCheckState(Qt::Checked);
  axis(w, control<QComboBox>(w, "blcad.pattern.second.reference"), "axis.x");
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK(control<QLabel>(w, "blcad.feature.summary").text().contains("independent"));
  CHECK_FALSE(w.statusBar()->currentMessage().contains("Preview valid"));
  axis(w, control<QComboBox>(w, "blcad.pattern.second.reference"), "axis.y");
  REQUIRE(w.viewport().feature_preview_active());
  control<QSpinBox>(w, "blcad.pattern.second.count").setValue(1000);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature.apply").isEnabled());
  CHECK(control<QListWidget>(w, "blcad.pattern.skipped").count() < 10);
  control<QSpinBox>(w, "blcad.pattern.second.count").setValue(2);
  REQUIRE(w.viewport().feature_preview_active());
  CHECK(occurrence(w, 2, 1).checkState() == Qt::Checked);
  REQUIRE(w.save_to(dir.filePath("draft.blcad.json")));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(dir.filePath("draft.blcad.json").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  w.feature_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK(w.viewport().preview_selection_target_count() == 0);
  REQUIRE(w.feature_tools().begin_pattern(Command::LinearPattern));
  CHECK(control<QComboBox>(w, "blcad.pattern.source_mode").currentIndex() == 0);
  CHECK_FALSE(control<QGroupBox>(w, "blcad.pattern.second.enabled").isChecked());
  w.feature_tools().cancel();
  REQUIRE(w.feature_tools().begin_body_operation(Command::BodyTranslate));
  CHECK(w.feature_tools().numeric_editor().isVisibleTo(&w));
  CHECK(w.feature_tools().numeric_editor().parentWidget() !=
        &control<QGroupBox>(w, "blcad.pattern.second.enabled"));
  w.feature_tools().cancel();
}
TEST_CASE("Linear and Circular Hole tasks accept count one without an extra Body or cut",
          "[gui][parity-b156][hole-pattern-shell]") {
  for (auto command : {Command::LinearPattern, Command::CircularPattern}) {
    ShellWindow w;
    QTemporaryDir dir;
    open(w, dir, false);
    const auto before = snapshot(w);
    begin(w, command);
    REQUIRE(w.feature_tools().set_pattern_count(1));
    REQUIRE(w.feature_tools().apply());
    CHECK(w.session().part_document()->bodies().size() == 1);
    CHECK(volume(w) == Catch::Approx(38400 - 17.5 * std::numbers::pi).margin(1e-6));
    replay(w, dir, before);
  }
}
TEST_CASE("Existing feature editing preserves V7 coordinates as integer intent and dormant skips",
          "[gui][parity-b156][hole-pattern-shell]") {
  auto p = f::part();
  auto options = f::options();
  options.skipped = {{2, 1}};
  REQUIRE(p.add_linear_pattern_feature(f::linear(options)));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, true, p);
  const auto before = snapshot(w);
  auto& edit = w.feature_edit_tools();
  REQUIRE(edit.begin(FeatureId("pattern")));
  auto* column = qobject_cast<QSpinBox*>(edit.field("/placements/skipped/0/first"));
  REQUIRE(column);
  CHECK(column->value() == 3);
  column->setValue(2);
  auto* count = qobject_cast<QDoubleSpinBox*>(edit.field("/count_parameter"));
  REQUIRE(count);
  count->setValue(1);
  REQUIRE(edit.apply());
  const auto* pattern =
      w.session().part_document()->find_linear_pattern_feature(FeatureId("pattern"));
  REQUIRE(pattern);
  CHECK((pattern->placements()->skipped == std::vector<PatternOccurrenceIndex>{{1, 1}}));
  CHECK(w.session().part_document()->find_parameter(ParameterId("count"))->value().count_value() ==
        1);
  CHECK(volume(w) == Catch::Approx(38400 - 2 * 17.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}
TEST_CASE(
    "Native Hole Pattern picking and compact grouped controls produce the visible accepted cut",
    "[gui][parity-b156][hole-pattern-shell][native-hole-pattern-shell]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT Hole source picking");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  begin(w, Command::LinearPattern, false);
  REQUIRE(w.viewport().native_viewer_available());
  w.viewport().set_standard_view(GuiStandardView::Isometric);
  w.viewport().fit_all();
  settle();
  bool picked = false;
  for (double theta : {0., .8, 1.6, 2.4, 3.2, 4., 4.8, 5.6}) {
    const auto screen = checked(w.viewport().model_to_screen(
        {-20 + 1.5 * std::cos(theta), -15 + 1.5 * std::sin(theta), 6.8}));
    click(w.viewport(), QPoint(qRound(screen.x), qRound(screen.y)));
    if (w.feature_tools().pattern_source_editor().selectedItems().size() == 1) {
      picked = true;
      break;
    }
  }
  REQUIRE(picked);
  REQUIRE(w.feature_tools().set_numeric_value(12));
  REQUIRE(w.viewport().feature_preview_active());
  auto& scroll = control<QScrollArea>(w, "blcad.feature.inputs_scroll");
  scroll.verticalScrollBar()->setValue(0);
  settle();
  capture(w, "hole-pattern-sources");
  second(w);
  occurrence(w, 2, 1).setCheckState(Qt::Checked);
  auto& group = control<QGroupBox>(w, "blcad.pattern.second.enabled");
  scroll.ensureWidgetVisible(&group);
  settle();
  group.setChecked(false);
  group.setFocus();
  for (auto type : {QEvent::KeyPress, QEvent::KeyRelease}) {
    QKeyEvent event(type, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(&group, &event);
    settle();
  }
  REQUIRE(group.isChecked());
  capture(w, "hole-pattern-direction2");
  auto& skip = control<QListWidget>(w, "blcad.pattern.skipped");
  scroll.ensureWidgetVisible(&skip);
  settle();
  auto& skipped = occurrence(w, 2, 1);
  skipped.setCheckState(Qt::Unchecked);
  skip.scrollToItem(&skipped);
  click(*skip.viewport(), {10, skip.visualItemRect(&skipped).center().y()});
  REQUIRE(skipped.checkState() == Qt::Checked);
  capture(w, "hole-pattern-skips");
  auto& apply = control<QPushButton>(w, "blcad.feature.apply");
  auto& cancel = control<QPushButton>(w, "blcad.feature.cancel");
  for (auto* button : {&apply, &cancel}) {
    CHECK(button->isVisibleTo(&w));
    CHECK(w.rect().contains(button->mapTo(&w, button->rect().center())));
  }
  click(apply, apply.rect().center());
  REQUIRE_FALSE(w.feature_tools().active());
  CHECK(volume(w) == Catch::Approx(38400 - 10 * 17.5 * std::numbers::pi).margin(1e-6));
  CHECK(w.viewport().preview_selection_target_count() == 0);
  capture(w, "hole-pattern-applied");
}

TEST_CASE("A preselected Hole opens its existing Pattern task with the typed Feature source",
          "[gui][parity-b156][hole-pattern-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, false);
  QTreeWidgetItem* selected = nullptr;
  const auto find = [&](auto&& self, QTreeWidgetItem* item) -> void {
    const auto* node = w.browser_model().find(item->data(0, Qt::UserRole).toString().toStdString());
    if (node && node->kind == GuiBrowserNodeKind::Feature && node->semantic_id == "holes")
      selected = item;
    for (int i = 0; i < item->childCount(); ++i)
      self(self, item->child(i));
  };
  for (int i = 0; i < w.model_browser().topLevelItemCount(); ++i)
    find(find, w.model_browser().topLevelItem(i));
  REQUIRE(selected);
  w.model_browser().setCurrentItem(selected);
  REQUIRE(w.feature_tools().begin_pattern(Command::LinearPattern));
  CHECK(control<QComboBox>(w, "blcad.pattern.source_mode").currentIndex() == 1);
  REQUIRE(w.feature_tools().pattern_source_editor().selectedItems().size() == 1);
  axis(w, w.feature_tools().pattern_reference_editor(), "axis.x");
  REQUIRE(w.feature_tools().set_numeric_value(12));
  CHECK(w.viewport().feature_preview_replaces("body/body.base"));
  w.feature_tools().cancel();
  CHECK(w.viewport().preview_selection_target_count() == 0);
}
