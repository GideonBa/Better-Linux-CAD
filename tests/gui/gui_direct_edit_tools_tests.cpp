#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/gui/gui_feature_edit.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/shell/shell_direct_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_driven_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace {
using Tools = ShellDirectEditTools;
template <class T> T* field(Tools& t, const char* key) {
  auto* result = qobject_cast<T*>(t.field(key));
  REQUIRE(result);
  return result;
}
std::string source(const ShellWindow& w) {
  return f::checked(serialize_part_document_to_json(*w.session().part_document()));
}
void open(ShellWindow& w, QTemporaryDir& dir) {
  const auto path = dir.filePath("part.blcad.json");
  {
    std::ofstream out(path.toStdString());
    out << f::checked(serialize_part_document_to_json(f::native_base()));
  }
  REQUIRE(w.open_path(path));
}
double volume(const ShellWindow& w) {
  const auto* shape = w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void top(Tools& t) {
  auto* combo = field<QComboBox>(t, "face");
  const auto key = interactive_face_identity(f::face(PartFeatureInputRole::SurfaceSourceFace));
  const int at = combo->findData(QString::fromStdString(key));
  REQUIRE(at >= 0);
  combo->setCurrentIndex(at);
  field<QPushButton>(t, "add_face")->click();
}
void preview(Tools& t) {
  auto r = t.preview();
  INFO((r ? "ok" : r.error().message()));
  REQUIRE(r);
}
void mouse(ShellWindow& w, QEvent::Type type, QPoint at, Qt::MouseButtons buttons) {
  QMouseEvent event(type, QPointF(at), w.viewport().mapToGlobal(at), Qt::LeftButton, buttons,
                    Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &event);
  qApp->processEvents();
}
QPoint grip(Tools& t, const char* id) {
  const auto handles = t.manipulators().presentations();
  auto at = std::find_if(handles.begin(), handles.end(),
                         [&](const auto& h) { return h.handle_id == id; });
  REQUIRE(at != handles.end());
  return {qRound(at->endpoint.x), qRound(at->endpoint.y)};
}
} // namespace
TEST_CASE("Owned Direct Edit checks preview history and publishes exact "
          "reversible intent",
          "[gui][direct-edit-tools][parity-b187]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = source(w);
  const auto mask = w.viewport().selection_filter_mask();
  auto& t = w.direct_edit_tools();
  auto* action = w.findChild<QAction*>("blcad.action.direct_edit");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  CHECK(action->icon().name() == "move");
  bool ribbon_entry = false;
  for (auto* button : w.ribbon().findChildren<QToolButton*>())
    ribbon_entry |= button->defaultAction() == action;
  CHECK(ribbon_entry);
  action->trigger();
  REQUIRE(t.active());
  field<QComboBox>(t, "mode")->setCurrentIndex(2);
  top(t);
  field<QLineEdit>(t, "size")->setText("0");
  CHECK_FALSE(t.preview());
  CHECK(source(w) == before);
  CHECK(field<QLineEdit>(t, "size")->text() == "0");
  field<QLineEdit>(t, "size")->setText("1");
  preview(t);
  CHECK(source(w) == before);
  CHECK(w.viewport().feature_preview_count() == 1);
  REQUIRE(t.apply());
  CHECK_FALSE(t.active());
  CHECK(volume(w) == Catch::Approx(1800).margin(1e-5));
  const auto applied = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == applied);
  GuiFeatureEditController controller;
  REQUIRE(controller.begin(w.session(), FeatureId("direct_edit.1")));
  CHECK(controller.kind() == GuiFeatureEditKind::DirectEdit);
  controller.cancel();
  QTreeWidgetItem* node = nullptr;
  for (QTreeWidgetItemIterator it(&w.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "direct_edit.1")
      node = *it;
  REQUIRE(node);
  w.model_browser().setCurrentItem(node);
  qApp->processEvents();
  REQUIRE(w.history_tools().edit_action().isEnabled());
  w.history_tools().edit_action().trigger();
  CHECK(t.active());
  CHECK(field<QComboBox>(t, "mode")->currentIndex() == 2);
  preview(t);
  REQUIRE(t.apply());
  CHECK(source(w) == applied);
  REQUIRE(t.begin_edit(FeatureId("direct_edit.1")));
  field<QLineEdit>(t, "size")->setText("-30");
  CHECK_FALSE(t.preview());
  CHECK(source(w) == applied);
  CHECK_FALSE(field<QPushButton>(t, "apply")->isEnabled());
  CHECK_FALSE(t.manipulators().handles().empty());
  field<QLineEdit>(t, "size")->setText("-2");
  preview(t);
  field<QComboBox>(t, "mode")->setCurrentIndex(3);
  CHECK_FALSE(t.preview());
  field<QComboBox>(t, "mode")->setCurrentIndex(2);
  CHECK(field<QLineEdit>(t, "size")->text() == "-2");
  t.cancel();
  CHECK(source(w) == applied);
  CHECK(w.viewport().selection_filter_mask() == mask);
  REQUIRE(t.begin_edit(FeatureId("direct_edit.1")));
  preview(t);
  field<QComboBox>(t, "x_parameter")
      ->setCurrentIndex(field<QComboBox>(t, "x_parameter")->findData("width"));
  field<QLineEdit>(t, "size")->setText("2");
  preview(t);
  t.cancel();
  CHECK(source(w) == applied);
  REQUIRE(w.save_to(dir.filePath("saved.blcad.json")));
  ShellWindow reopened;
  REQUIRE(reopened.open_path(dir.filePath("saved.blcad.json")));
  CHECK(source(reopened) == applied);
  CHECK(volume(reopened) == Catch::Approx(1800).margin(1e-5));
}

TEST_CASE("Direct Edit binding and frame errors preserve pending input and "
          "current model",
          "[gui][direct-edit-tools][parity-b187]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  const auto before = source(w);
  auto& t = w.direct_edit_tools();
  REQUIRE(t.begin());
  top(t);
  field<QLineEdit>(t, "frame_y_y")->setText("2");
  CHECK_FALSE(t.preview());
  CHECK(field<QLineEdit>(t, "frame_y_y")->text() == "2");
  CHECK(source(w) == before);
  field<QPushButton>(t, "world_frame")->click();
  auto* zero_binding = field<QComboBox>(t, "x_parameter");
  zero_binding->setCurrentIndex(zero_binding->findData("width"));
  field<QLineEdit>(t, "x")->setText("0");
  preview(t);
  auto* combo = field<QComboBox>(t, "z_parameter");
  combo->setCurrentIndex(combo->findData("depth"));
  field<QLineEdit>(t, "z")->setText("3.125");
  preview(t);
  CHECK(source(w) == before);
  REQUIRE(t.apply());
  CHECK(w.session().part_document()->find_parameter(ParameterId("depth"))->value().millimeters() ==
        3.125);
  CHECK(w.session().part_document()->find_parameter(ParameterId("width"))->value().millimeters() ==
        20);
  CHECK(volume(w) == Catch::Approx(1250).margin(1e-5));
  REQUIRE(w.undo());
  CHECK(source(w) == before);
}

TEST_CASE("Generic pattern sources expose owned Direct Edit and Emboss records and retain history",
          "[gui][direct-edit-tools][generic-direct-edit-source][parity-b187]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  REQUIRE(w.session().commit_part_transaction("Owned feature sources", [](PartDocument& p) {
    for (const auto& [id, value] :
         {std::pair{"direct.distance", 1.}, {"label.depth", .5}, {"source.spacing", 30.}})
      REQUIRE(p.add_parameter(f::checked(
          Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
    REQUIRE(p.add_direct_edit_feature(f::checked(DirectEditFeature::create(
        FeatureId("direct"), "Direct top", BodyId("body.base"), FeatureId("base"),
        {{f::face(PartFeatureInputRole::SurfaceSourceFace), DirectEditFaceCapability::Planar}},
        DirectFaceOffset{ParameterId("direct.distance")}))));
    auto sketch =
        f::checked(Sketch::create(SketchId("label.sketch"), "Label", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(f::checked(RectangleProfile::create(
        ProfileId("label.profile"), ParameterId("radius"), ParameterId("radius")))));
    REQUIRE(p.add_sketch(sketch));
    const auto top = f::checked(ResultFaceReference::create(
        FeatureId("direct"), BodyId("body.base"), {{FeatureId("direct"), "face/0"}}));
    EmbossIntent label{
        f::checked(ProfileRegionReference::create(sketch.id(), ProfileId("label.profile"))),
        {f::checked(FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace, top))},
        EmbossOperation::Raise,
        ParameterId("label.depth"),
        EmbossMapping::Planar,
        {},
        0,
        {}};
    REQUIRE(p.add_sketch_driven_feature(f::checked(
        SketchDrivenFeature::create(FeatureId("label"), "Embossed label", BodyId("body.base"),
                                    SketchDrivenDefinition{label}))));
    REQUIRE(
        p.add_body(f::checked(Body::create(BodyId("pattern.body"), "Pattern", BodyKind::Solid))));
    REQUIRE(p.add_linear_pattern_feature(f::checked(LinearPatternFeature::create(
        FeatureId("source.pattern"), "Feature source pattern",
        {f::checked(PatternSourceReference::feature(FeatureId("base")))},
        f::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                    DatumAxisId("axis"))),
        ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("source.spacing"),
        PatternDirectionSign::Positive,
        f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                    BodyId("pattern.body")))))));
    return Result<std::size_t>::success(1);
  }));
  w.refresh();
  const auto before = source(w);
  for (const QString& id : {QString("direct"), QString("label")}) {
    auto& generic = w.feature_edit_tools();
    REQUIRE(generic.begin(FeatureId("source.pattern")));
    auto* choice = qobject_cast<QComboBox*>(generic.field("/sources/0/feature"));
    REQUIRE(choice);
    CHECK(choice->findData("direct") >= 0);
    CHECK(choice->findData("label") >= 0);
    CHECK(choice->findData("source.pattern") < 0);
    choice->setCurrentIndex(choice->findData(id));
    qApp->processEvents();
    CHECK(source(w) == before);
    CAPTURE(id.toStdString());
    auto apply_result = generic.apply();
    INFO((apply_result ? std::string{}
                       : apply_result.error().object_id() + ": " + apply_result.error().message()));
    REQUIRE(apply_result);
    const auto* pattern =
        w.session().part_document()->find_linear_pattern_feature(FeatureId("source.pattern"));
    REQUIRE(pattern);
    CHECK(std::get<FeatureId>(pattern->sources().front().source()).value() == id.toStdString());
    const auto* result = w.session().part_shape_cache()->find_body_shape(BodyId("pattern.body"));
    REQUIRE(result);
    CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*result).volume_mm3 ==
          Catch::Approx(3 * (id == "direct" ? 1800. : 1800.5)).margin(1e-5));
    CHECK(volume(w) == Catch::Approx(1800.5).margin(1e-5));
    const auto applied = source(w);
    REQUIRE(w.undo());
    CHECK(source(w) == before);
    REQUIRE(w.redo());
    CHECK(source(w) == applied);
    REQUIRE(w.undo());
    CHECK(source(w) == before);
  }
  REQUIRE(w.feature_edit_tools().begin(FeatureId("direct")));
  REQUIRE(w.direct_edit_tools().active());
  CHECK(field<QComboBox>(w.direct_edit_tools(), "mode")->currentIndex() == 2);
  preview(w.direct_edit_tools());
  w.direct_edit_tools().cancel();
  REQUIRE(w.feature_edit_tools().begin(FeatureId("label")));
  CHECK(w.sketch_driven_tools().active());
  w.sketch_driven_tools().cancel();
  CHECK(source(w) == before);
}

TEST_CASE("Native Direct Edit triad preserves exact numeric release and "
          "lost-capture rollback",
          "[gui][direct-edit-tools][native-direct-edit][parity-b187]") {
  if (QGuiApplication::platformName() == "offscreen")
    SKIP("Requires native Qt/OCCT viewport");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir);
  w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380,
           std::min(850, w.screen()->availableGeometry().height() - 40));
  w.show();
  w.raise();
  w.activateWindow();
  REQUIRE(QTest::qWaitForWindowActive(&w));
  qApp->processEvents();
  w.viewport().fit_all();
  qApp->processEvents();
  REQUIRE(w.viewport().native_viewer_available());
  const auto before = source(w);
  auto& t = w.direct_edit_tools();
  REQUIRE(t.begin());
  top(t);
  field<QPushButton>(t, "align_frame")->click();
  preview(t);
  qApp->processEvents();
  CHECK(field<QLineEdit>(t, "origin_z")->text().toDouble() == 8);
  auto at = grip(t, "direct_edit/z");
  mouse(w, QEvent::MouseButtonPress, at, Qt::LeftButton);
  REQUIRE(t.manipulators().active_handle_id() == "direct_edit/z");
  auto* numeric = field<QLineEdit>(t, "grip_numeric");
  numeric->setFocus();
  qApp->processEvents();
  REQUIRE(numeric->hasFocus());
  numeric->selectAll();
  QKeyEvent exact(QEvent::KeyPress, 0, Qt::NoModifier, "2.25 mm");
  QApplication::sendEvent(numeric, &exact);
  mouse(w, QEvent::MouseButtonRelease, at, Qt::NoButton);
  CHECK(field<QLineEdit>(t, "z")->text().toDouble() == 2.25);
  CHECK(source(w) == before);
  at = grip(t, "direct_edit/z");
  mouse(w, QEvent::MouseButtonPress, at, Qt::LeftButton);
  REQUIRE(t.set_drag_text("3.75 mm"));
  QEvent lost(QEvent::UngrabMouse);
  QApplication::sendEvent(&w.viewport(), &lost);
  CHECK(t.manipulators().active_handle_id().empty());
  CHECK(field<QLineEdit>(t, "z")->text().toDouble() == 2.25);
  preview(t);
  REQUIRE(t.apply());
  CHECK(volume(w) == Catch::Approx(2050).margin(1e-5));
  const auto applied = source(w);
  QTreeWidgetItem* record = nullptr;
  for (QTreeWidgetItemIterator it(&w.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "direct_edit.1")
      record = *it;
  REQUIRE(record);
  for (auto* parent = record->parent(); parent; parent = parent->parent())
    parent->setExpanded(true);
  w.model_browser().scrollToItem(record);
  qApp->processEvents();
  const auto row = w.model_browser().visualItemRect(record);
  REQUIRE(!row.isEmpty());
  auto* browser = w.model_browser().viewport();
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                          QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease}) {
    const auto buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent activate(type, QPointF(row.center()), browser->mapToGlobal(row.center()),
                         Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(browser, &activate);
  }
  qApp->processEvents();
  REQUIRE(t.active());
  CHECK(field<QLineEdit>(t, "z")->text().toDouble() == 2.25);
  field<QComboBox>(t, "mode")->setCurrentIndex(1);
  preview(t);
  qApp->processEvents();
  auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!root.isEmpty()) {
    REQUIRE(QDir().mkpath(root));
    REQUIRE(w.grab().save(root + "/direct-edit-rotate.png"));
  }
  auto* scroll = t.field("origin_z")->parentWidget();
  while (scroll && !qobject_cast<QScrollArea*>(scroll))
    scroll = scroll->parentWidget();
  REQUIRE(scroll);
  qobject_cast<QScrollArea*>(scroll)->ensureWidgetVisible(t.field("origin_z"));
  qApp->processEvents();
  CHECK(t.field("origin_z")->isVisible());
  CHECK(t.field("apply")->isVisible());
  CHECK(t.field("cancel")->isVisible());
  field<QLineEdit>(t, "axis_x")->setText("0");
  CHECK_FALSE(t.preview());
  CHECK(source(w) == applied);
  field<QLineEdit>(t, "axis_x")->setText("1");
  preview(t);
  t.cancel();
  CHECK(source(w) == applied);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == applied);
  w.hide();
}
