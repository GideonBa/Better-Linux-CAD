#include "../support/hole_manufacturing_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace holes = blcad::test::hole_pattern_fixture;
namespace threads = blcad::test::thread_fixture;
namespace manufacture = blcad::test::manufacturing_fixture;
namespace {
template <class T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
void settle() {
  for (int i = 0; i < 5; ++i)
    qApp->processEvents();
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* found = w.findChild<T*>(QString::fromLatin1(name));
  REQUIRE(found);
  return *found;
}
template <class T> T& field(ShellWindow& w, const char* path) {
  auto* found = qobject_cast<T*>(w.feature_edit_tools().field(QString::fromLatin1(path)));
  REQUIRE(found);
  return *found;
}
std::string snapshot(ShellWindow& w) {
  return checked(serialize_part_document_to_json(*w.session().part_document()));
}
void open(ShellWindow& w, QTemporaryDir& dir, const PartDocument& p) {
  REQUIRE(dir.isValid());
  auto path = dir.filePath("edit.blcad.json");
  REQUIRE(write_part_document_json_file(p, path.toStdString()));
  REQUIRE(w.open_path(path));
  auto size = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT") ? QSize(1024, 640) : QSize(1280, 800);
  if (QApplication::platformName() == "xcb")
    size = size.boundedTo(w.screen()->availableGeometry().size() - QSize(48, 48));
  w.resize(size);
  w.show();
  settle();
  w.viewport().fit_all();
  settle();
}
double volume(ShellWindow& w, const char* body = "body.base") {
  const auto* shape = w.session().part_shape_cache()->find_body_shape(BodyId(body));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void replay(ShellWindow& w, QTemporaryDir& dir, const std::string& before) {
  const auto after = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == after);
  const auto path = dir.filePath("reopen.blcad.json");
  REQUIRE(w.save_to(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == after);
}
void begin(ShellWindow& w, const char* id) {
  const auto result = w.feature_edit_tools().begin(FeatureId(id));
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  settle();
  if (QApplication::platformName() == "xcb")
    CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
              .horizontalScrollBar()
              ->maximum() == 0);
}
void history_edit(ShellWindow& w, const char* id) {
  QTreeWidgetItem* selected = nullptr;
  const auto find = [&](auto&& self, QTreeWidgetItem* item) -> void {
    const auto* node = w.browser_model().find(item->data(0, Qt::UserRole).toString().toStdString());
    if (node && node->kind == GuiBrowserNodeKind::Feature && node->semantic_id == id)
      selected = item;
    for (int i = 0; i < item->childCount(); ++i)
      self(self, item->child(i));
  };
  for (int i = 0; i < w.model_browser().topLevelItemCount(); ++i)
    find(find, w.model_browser().topLevelItem(i));
  REQUIRE(selected);
  w.model_browser().setCurrentItem(selected);
  settle();
  REQUIRE(w.history_tools().edit_action().isEnabled());
  w.history_tools().edit_action().trigger();
  REQUIRE(w.feature_edit_tools().active());
  settle();
  if (QApplication::platformName() == "xcb")
    CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
              .horizontalScrollBar()
              ->maximum() == 0);
}
void rename(ShellWindow& w) {
  auto& name = field<QLineEdit>(w, "/name");
  name.setText("Edited semantic feature");
  REQUIRE(QMetaObject::invokeMethod(&name, "editingFinished"));
}
void click(QWidget& widget) {
  REQUIRE(widget.isVisible());
  REQUIRE(widget.isEnabled());
  const auto point = widget.rect().center();
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, widget.mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
    settle();
  }
}
void key(QWidget& widget, int code, const QString& text = {}) {
  for (auto type : {QEvent::KeyPress, QEvent::KeyRelease}) {
    QKeyEvent event(type, code, Qt::NoModifier, text);
    QApplication::sendEvent(&widget, &event);
    settle();
  }
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  REQUIRE(w.grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png")));
}
} // namespace

TEST_CASE("Hole property editing validates complete Pattern geometry and retains instance seats",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  auto p = holes::part();
  REQUIRE(p.add_linear_pattern_feature(holes::linear()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  const auto old_volume = volume(w);
  GuiFeatureEditController controller;
  REQUIRE(controller.begin(w.session(), FeatureId("holes")));
  CHECK(controller.kind() == GuiFeatureEditKind::Hole);
  history_edit(w, "holes");
  auto& diameter = field<QDoubleSpinBox>(w, "/diameter/parameter");
  diameter.setValue(40);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK(snapshot(w) == before);
  CHECK(volume(w) == Catch::Approx(old_volume));
  diameter.setValue(4);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  capture(w, "hole-property-pattern-preview");
  const auto during = dir.filePath("during-preview.blcad.json");
  REQUIRE(w.save_to(during));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(during.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(38400 - 6 * 26.25 * std::numbers::pi).margin(1e-6));
  const auto ref = checked(HoleInstanceReference::create(
      FeatureId("holes"), HolePlacementId("first"), HoleReferenceRole::Seat,
      HolePatternOccurrence{FeatureId("pattern"), {1, 1}}));
  const auto seat = checked(geometry::HoleInstanceGeometry::resolve(
      *w.session().part_document(), ref, *w.session().part_shape_cache()));
  CHECK(seat.frame.origin.x == Catch::Approx(-8));
  CHECK(seat.frame.origin.y == Catch::Approx(0).margin(1e-7));
  CHECK(seat.frame.origin.z == Catch::Approx(7));
  replay(w, dir, before);
}

TEST_CASE("Hole property editing keeps formula ownership until an explicit parameter rebind",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  auto p = holes::part();
  REQUIRE(p.add_expression_parameter(ParameterId("bound"), "Bound bore", ParameterType::Length,
                                     "diameter"));
  const auto h = *p.find_hole_feature(FeatureId("holes"));
  REQUIRE(p.update_hole_feature(checked(HoleFeature::create(
      h.id(), h.name(), h.kind(), h.target_body(), h.support(), h.placements(),
      ParameterId("bound"), h.extent(), h.direction(), h.tip(), h.manufacturing()))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  begin(w, "holes");
  auto& diameter = field<QDoubleSpinBox>(w, "/diameter/parameter");
  CHECK_FALSE(diameter.isEnabled());
  CHECK(diameter.toolTip().contains("diameter"));
  auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  binding.setCurrentIndex(binding.findData("diameter"));
  REQUIRE(diameter.isEnabled());
  diameter.setValue(4);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(std::get<ParameterId>(w.session().part_document()->hole_features().front().diameter()) ==
        ParameterId("diameter"));
  CHECK(w.session().part_document()->find_parameter(ParameterId("bound"))->formula() ==
        std::optional<std::string>("diameter"));
  replay(w, dir, before);
}

TEST_CASE("Pinned Hole and Thread table mappings stay read-only while feature names are edited",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  for (const bool external : {false, true}) {
    auto p = external ? threads::pair_part() : blcad::test::hole_fixture::part();
    if (external)
      REQUIRE(p.add_thread_feature(threads::external(threads::definition(threads::selected()))));
    ShellWindow w;
    QTemporaryDir dir;
    open(w, dir, p);
    const auto before = snapshot(w);
    begin(w, external ? "external" : "hole");
    const auto table = external ? "/thread/profile/table/id" : "/diameter/table/id";
    CHECK(field<QLabel>(w, table).text() == (external ? "synthetic.threads" : "synthetic.holes"));
    CHECK(field<QLabel>(w, external ? "/thread/profile/table/revision" : "/diameter/table/revision")
              .text() == "r1");
    CHECK_FALSE(
        field<QLabel>(w, external ? "/thread/profile/columns/pitch" : "/diameter/diameter_column")
            .text()
            .isEmpty());
    rename(w);
    REQUIRE(w.feature_edit_tools().apply());
    CHECK(w.session().part_document()->engineering_resources().entries().size() ==
          p.engineering_resources().entries().size());
    replay(w, dir, before);
  }
}

TEST_CASE("Hole property Add Entry never reuses a removed draft placement identity",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part(true));
  const auto before = snapshot(w);
  begin(w, "holes");
  control<QPushButton>(w, "blcad.feature_edit.add/placements").click();
  settle();
  const auto first_id = control<QGroupBox>(w, "blcad.feature_edit.entry/placements/2")
                            .property("semanticId")
                            .toString();
  field<QDoubleSpinBox>(w, "/placements/2/position/x").setValue(-5);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QPushButton>(w, "blcad.feature_edit.Entfernen/placements/2").click();
  settle();
  control<QPushButton>(w, "blcad.feature_edit.add/placements").click();
  settle();
  const auto second_id = control<QGroupBox>(w, "blcad.feature_edit.entry/placements/2")
                             .property("semanticId")
                             .toString();
  CHECK_FALSE(first_id.isEmpty());
  CHECK_FALSE(second_id.isEmpty());
  CHECK(first_id != second_id);
  field<QDoubleSpinBox>(w, "/placements/2/position/x").setValue(-5);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& placements = w.session().part_document()->hole_features().front().placements();
  REQUIRE(placements.size() == 3);
  CHECK(placements[0].id == HolePlacementId("first"));
  CHECK(placements[1].id == HolePlacementId("other"));
  CHECK(placements[2].id == HolePlacementId(second_id.toStdString()));
  CHECK(volume(w) == Catch::Approx(38400 - 3 * 17.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
  begin(w, "holes");
  control<QPushButton>(w, "blcad.feature_edit.Entfernen/placements/2").click();
  settle();
  REQUIRE(w.feature_edit_tools().apply());
  const auto removed = snapshot(w);
  begin(w, "holes");
  control<QPushButton>(w, "blcad.feature_edit.add/placements").click();
  settle();
  field<QDoubleSpinBox>(w, "/placements/2/position/x").setValue(-5);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& added = w.session().part_document()->hole_features().front().placements().back();
  CHECK(added.id != HolePlacementId(first_id.toStdString()));
  CHECK(added.id != HolePlacementId(second_id.toStdString()));
  const auto retired = checked(HoleInstanceReference::create(
      FeatureId("holes"), HolePlacementId(second_id.toStdString()), HoleReferenceRole::Axis));
  CHECK_FALSE(geometry::HoleInstanceGeometry::resolve(*w.session().part_document(), retired,
                                                      *w.session().part_shape_cache()));
  replay(w, dir, removed);
}

TEST_CASE("All existing manufacturing Hole definitions survive property editing exactly",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  for (int mode = 0; mode <= 8; ++mode) {
    CAPTURE(mode);
    auto p = manufacture::part(mode);
    ShellWindow w;
    QTemporaryDir dir;
    open(w, dir, p);
    const auto before = snapshot(w);
    const auto old_volume = volume(w);
    begin(w, "hole");
    rename(w);
    REQUIRE(w.feature_edit_tools().apply());
    CHECK(volume(w) == Catch::Approx(old_volume).margin(1e-7));
    auto expected = nlohmann::json::parse(before);
    expected["hole_features"][0]["name"] = "Edited semantic feature";
    CHECK(nlohmann::json::parse(snapshot(w)) == expected);
    CHECK(w.session().part_document()->hole_features().front().manufacturing().index() ==
          p.hole_features().front().manufacturing().index());
    replay(w, dir, before);
  }
}

TEST_CASE("Cancelling Hole property edits restores selection and the complete committed state",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part());
  const auto before = snapshot(w);
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  const auto undo = std::string(w.session().undo_label());
  begin(w, "holes");
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(w.viewport().feature_preview_active());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK(std::string(w.session().undo_label()) == undo);
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  CHECK_FALSE(w.viewport().feature_preview_active());
}

TEST_CASE("Internal Thread fields distinguish drill depth angle pitch start and thread length",
          "[gui][parity-b157][hole-thread-edit-shell]") {
  auto p = threads::pair_part();
  REQUIRE(p.add_hole_feature(threads::internal(threads::definition(threads::custom(), true))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  const auto original = volume(w);
  begin(w, "internal");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/included_angle").suffix() == " deg");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/pitch").suffix() == " mm");
  field<QDoubleSpinBox>(w, "/thread/length").setValue(20);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  field<QDoubleSpinBox>(w, "/thread/length").setValue(3);
  field<QDoubleSpinBox>(w, "/thread/start").setValue(2);
  field<QComboBox>(w, "/thread/handedness").setCurrentIndex(1);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(original));
  CHECK(w.session()
            .part_document()
            ->find_parameter(ParameterId("drill.depth"))
            ->value()
            .millimeters() == 6);
  CHECK(w.session()
            .part_document()
            ->find_hole_feature(FeatureId("internal"))
            ->thread()
            ->handedness() == ThreadHandedness::Left);
  replay(w, dir, before);
}

TEST_CASE("Native semantic Thread property input edits real geometry with one reversible Apply",
          "[gui][parity-b157][hole-thread-edit-shell][native-hole-thread-edit]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Hole/Thread property input and OCCT rendering");
  auto p = threads::pair_part();
  REQUIRE(p.add_thread_feature(threads::external(threads::definition(threads::custom(), true))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  const auto original = volume(w, "body.shaft");
  history_edit(w, "external");
  auto& scroll = control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll");
  auto& length = field<QDoubleSpinBox>(w, "/thread/length");
  scroll.ensureWidgetVisible(&length);
  length.setFocus();
  length.selectAll();
  key(length, Qt::Key_3, "3");
  key(length, Qt::Key_Return);
  CHECK(length.value() == 3);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  scroll.ensureWidgetVisible(&mode);
  mode.setFocus();
  key(mode, Qt::Key_Down);
  CHECK(scroll.horizontalScrollBar()->maximum() == 0);
  REQUIRE(mode.currentData() == "modeled");
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  capture(w, "thread-property-modeled-preview");
  auto& apply = control<QPushButton>(w, "blcad.feature_edit.apply");
  CHECK(w.rect().contains(apply.mapTo(&w, apply.rect().center())));
  click(apply);
  CHECK(volume(w, "body.shaft") < original);
  CHECK(volume(w, "body.shaft") > 0);
  CHECK(w.session().part_document()->thread_features().front().thread().representation() ==
        ThreadRepresentation::Modeled);
  CHECK(w.session()
            .part_document()
            ->find_parameter(ParameterId("thread.length"))
            ->value()
            .millimeters() == 3);
  capture(w, "thread-property-modeled-applied");
  replay(w, dir, before);
}
