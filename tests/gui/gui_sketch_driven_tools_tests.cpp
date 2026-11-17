#include "../support/engineering_resource_fixture.hpp"
#include "../support/rib_web_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_reference_repair_tools.hpp"
#include "blcad/gui/shell/shell_sketch_driven_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>
#include <numbers>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace r = blcad::test::rib_fixture;
namespace {
using Tools = ShellSketchDrivenTools;
template <class T> T* field(Tools& tools, const char* key) {
  auto* result = qobject_cast<T*>(tools.field(key));
  REQUIRE(result);
  return result;
}
void select(Tools& t, const char* key, const char* id) {
  auto* box = field<QComboBox>(t, key);
  const int at = box->findData(QString::fromUtf8(id));
  REQUIRE(at >= 0);
  box->setCurrentIndex(at);
}
std::string source(const ShellWindow& w) {
  return f::checked(serialize_part_document_to_json(*w.session().part_document()));
}
void open(ShellWindow& w, const PartDocument& p, QTemporaryDir& dir) {
  auto path = dir.filePath("part.blcad.json");
  {
    std::ofstream out(path.toStdString());
    out << f::checked(serialize_part_document_to_json(p));
  }
  REQUIRE(w.open_path(path));
}
double volume(const ShellWindow& w) {
  const auto* shape = w.session().part_shape_cache()->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void rib_inputs(Tools& tools) {
  select(tools, "sketch", "rib.sketch");
  select(tools, "curve", "line");
  field<QPushButton>(tools, "curve_add")->click();
  select(tools, "size_binding", "rib.t");
  select(tools, "distance_binding", "rib.l");
}
void mouse(ShellWindow& w, QEvent::Type type, QPoint at, Qt::MouseButtons buttons) {
  QMouseEvent event(type, QPointF(at), w.viewport().mapToGlobal(at), Qt::LeftButton, buttons,
                    Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &event);
  qApp->processEvents();
}
void preview(Tools& t) {
  auto result = t.preview();
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
} // namespace
TEST_CASE("Owned Rib preview and Apply preserve complete history and exact cancellation",
          "[gui][sketch-driven-tools][parity-b184]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, r::fixture(), dir);
  const auto before = source(w);
  auto& tools = w.sketch_driven_tools();
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  preview(tools);
  CHECK(source(w) == before);
  CHECK(w.viewport().feature_preview_count() == 1);
  REQUIRE(tools.apply());
  CHECK_FALSE(tools.active());
  CHECK(volume(w) == Catch::Approx(1664).margin(1e-5));
  const auto applied = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == applied);
  QTreeWidgetItem* selected = nullptr;
  for (QTreeWidgetItemIterator it(&w.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString() == "sketch_driven.1")
      selected = *it;
  REQUIRE(selected);
  w.model_browser().setCurrentItem(selected);
  qApp->processEvents();
  REQUIRE(w.history_tools().edit_action().isEnabled());
  w.history_tools().edit_action().trigger();
  CHECK(tools.active());
  preview(tools);
  REQUIRE(tools.apply());
  CHECK(source(w) == applied);
  REQUIRE(tools.begin_edit(FeatureId("sketch_driven.1")));
  field<QLineEdit>(tools, "size")->setText("-1");
  CHECK_FALSE(tools.preview());
  CHECK(source(w) == applied);
  CHECK_FALSE(tools.manipulators().handles().empty());
  tools.cancel();
  CHECK(source(w) == applied);
}
TEST_CASE("Native Rib grip uses exact bound thickness and Escape restores its draft",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QGuiApplication::platformName() == "offscreen")
    SKIP("Requires native Qt/OCCT viewport");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, r::fixture(), dir);
  w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380,
           std::min(850, w.screen()->availableGeometry().height() - 40));
  w.show();
  QCoreApplication::processEvents();
  w.viewport().fit_all();
  auto& tools = w.sketch_driven_tools();
  const auto before = source(w);
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  preview(tools);
  QCoreApplication::processEvents();
  auto presentations = tools.manipulators().presentations();
  auto grip = std::find_if(presentations.begin(), presentations.end(),
                           [](const auto& p) { return p.handle_id == "sketch_driven/thickness"; });
  REQUIRE(grip != presentations.end());
  QPoint at(qRound(grip->endpoint.x), qRound(grip->endpoint.y));
  mouse(w, QEvent::MouseButtonPress, at, Qt::LeftButton);
  REQUIRE(tools.manipulators().active_handle_id() == "sketch_driven/thickness");
  auto* numeric = field<QLineEdit>(tools, "grip_numeric");
  numeric->selectAll();
  QKeyEvent typed(QEvent::KeyPress, 0, Qt::NoModifier, "1.2 mm");
  QApplication::sendEvent(numeric, &typed);
  mouse(w, QEvent::MouseButtonRelease, at, Qt::NoButton);
  CHECK(field<QLineEdit>(tools, "size")->text().toDouble() == 1.2);
  CHECK(source(w) == before);
  QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier);
  QApplication::sendEvent(field<QLineEdit>(tools, "size"), &accept);
  CHECK_FALSE(tools.active());
  CHECK(volume(w) == Catch::Approx(1676.8).margin(1e-5));
  auto* repeat = w.findChild<QAction*>("blcad.action.repeat_last_command");
  REQUIRE(repeat);
  REQUIRE(repeat->isEnabled());
  repeat->trigger();
  CHECK(tools.active());
  tools.cancel();
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  const auto applied = source(w);
  REQUIRE(tools.begin_edit(FeatureId("sketch_driven.1")));
  preview(tools);
  QCoreApplication::processEvents();
  presentations = tools.manipulators().presentations();
  grip = std::find_if(presentations.begin(), presentations.end(),
                      [](const auto& p) { return p.handle_id == "sketch_driven/thickness"; });
  REQUIRE(grip != presentations.end());
  at = {qRound(grip->endpoint.x), qRound(grip->endpoint.y)};
  mouse(w, QEvent::MouseButtonPress, at, Qt::LeftButton);
  REQUIRE(tools.set_drag_text("1.4 mm"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &escape);
  CHECK(field<QLineEdit>(tools, "size")->text().toDouble() == 1.2);
  CHECK(tools.manipulators().active_handle_id().empty());
  presentations = tools.manipulators().presentations();
  grip = std::find_if(presentations.begin(), presentations.end(),
                      [](const auto& p) { return p.handle_id == "sketch_driven/thickness"; });
  REQUIRE(grip != presentations.end());
  at = {qRound(grip->endpoint.x), qRound(grip->endpoint.y)};
  mouse(w, QEvent::MouseButtonPress, at, Qt::LeftButton);
  REQUIRE(tools.set_drag_text("1.6 mm"));
  QEvent lost_capture(QEvent::UngrabMouse);
  QApplication::sendEvent(&w.viewport(), &lost_capture);
  CHECK(tools.manipulators().active_handle_id().empty());
  CHECK(field<QLineEdit>(tools, "size")->text().toDouble() == 1.2);
  CHECK(source(w) == applied);
  tools.cancel();
  CHECK(source(w) == applied);
  w.hide();
}
namespace {
void show(ShellWindow& w) {
  w.resize(qEnvironmentVariableIsSet("BLCAD_COMPACT_UI") ? 1050 : 1380,
           std::min(850, w.screen()->availableGeometry().height() - 40));
  w.show();
  w.viewport().fit_all();
  qApp->processEvents();
  REQUIRE(w.viewport().native_viewer_available());
}
void reveal(QWidget* field) {
  auto* parent = field->parentWidget();
  while (parent && !qobject_cast<QScrollArea*>(parent))
    parent = parent->parentWidget();
  REQUIRE(parent);
  auto* scroll = qobject_cast<QScrollArea*>(parent);
  QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
  qApp->processEvents();
  scroll->ensureWidgetVisible(field, 8, 32);
  qApp->processEvents();
  CHECK(field->isVisible());
  CHECK(scroll->viewport()->rect().contains(
      QRect(field->mapTo(scroll->viewport(), QPoint{}), field->size())));
}
void capture(ShellWindow& w, const char* name) {
  auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (root.isEmpty())
    return;
  REQUIRE(QDir().mkpath(root));
  qApp->processEvents();
  REQUIRE(w.grab().save(QDir(root).filePath(QString::fromUtf8(name) + ".png")));
}
void support(ShellWindow& w, EmbossMapping mapping) {
  auto& t = w.sketch_driven_tools();
  auto choices = f::checked(geometry::SketchDrivenGuideQuery::faces(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base")));
  std::size_t count = 0;
  for (const auto& chosen : choices)
    if (chosen.mapping == mapping && (mapping != EmbossMapping::Planar || chosen.normal.z > .99)) {
      const auto key =
          interactive_face_identity(std::get<FaceReference>(chosen.selection.reference));
      select(t, "face", key.c_str());
      field<QPushButton>(t, "face_add")->click();
      ++count;
      if (mapping == EmbossMapping::Planar)
        break;
    }
  REQUIRE(count > 0);
}
PartDocument cylinder() {
  auto p = f::checked(PartDocument::create(DocumentId("native.wrap"), "Wrapped label"));
  f::accept(p.add_datum_plane(f::checked(DatumPlane::xy())));
  for (const auto& [id, value] :
       {std::pair{"width", 4.0}, {"height", 3.0}, {"depth", 20.0}, {"diameter", 20.0}})
    f::accept(p.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  auto circle =
      f::checked(Sketch::create(SketchId("cylinder"), "Cylinder", DatumPlaneId("datum.xy")));
  f::accept(circle.add_profile(
      f::checked(CircleProfile::create(ProfileId("circle"), ParameterId("diameter")))));
  f::accept(p.add_sketch(circle));
  auto context = f::new_body(p, "base");
  auto feature = f::checked(Feature::create_additive_extrude(FeatureId("base"), "Cylinder",
                                                             circle.id(), ParameterId("depth")));
  f::accept(p.add_feature(f::checked(feature.with_body_result_context(context))));
  f::rectangle(p, "label.sketch");
  return p;
}
} // namespace
TEST_CASE("Native source Sketch return preserves Rib settings and one parent undo boundary",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Sketch breadcrumb");
  const bool cancel = GENERATE(true, false);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, r::fixture(), dir);
  show(w);
  auto& tools = w.sketch_driven_tools();
  const auto before = source(w);
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  preview(tools);
  REQUIRE(tools.edit_source_sketch());
  REQUIRE(w.active_sketch() == SketchId("rib.sketch"));
  CHECK(tools.editing_source_sketch());
  REQUIRE(w.session().commit_part_transaction("Edit skeleton", [](PartDocument& p) {
    auto sketch =
        f::checked(Sketch::create(SketchId("rib.sketch"), "Skeleton", DatumPlaneId("datum.xy")));
    f::accept(sketch.add_entity(
        f::checked(LineSegment::create(SketchEntityId("line"), {-6, 0}, {6, 0}))));
    return p.update_sketch(sketch);
  }));
  w.refresh();
  REQUIRE(w.finish_active_sketch());
  CHECK_FALSE(tools.editing_source_sketch());
  CHECK(tools.active());
  preview(tools);
  capture(w, "rib-source-return");
  if (cancel) {
    tools.cancel();
    CHECK(source(w) == before);
  } else {
    REQUIRE(tools.apply());
    CHECK(volume(w) == Catch::Approx(1648).margin(1e-5));
    REQUIRE(w.undo());
    CHECK(source(w) == before);
    REQUIRE(w.redo());
    CHECK(volume(w) == Catch::Approx(1648).margin(1e-5));
  }
  w.hide();
}
TEST_CASE(
    "Native bound text height and tokens survive Apply history reopen export and mass updates",
    "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native text property panel");
  auto p = r::fixture();
  blcad::test::engineering_fixture::annotate(p);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, p, dir);
  show(w);
  auto& tools = w.sketch_driven_tools();
  const auto before = source(w);
  REQUIRE(tools.begin(Tools::Mode::Emboss));
  field<QComboBox>(tools, "source_kind")->setCurrentIndex(2);
  select(tools, "sketch", "profile");
  field<QLineEdit>(tools, "text")->setText("O${n}");
  field<QLineEdit>(tools, "size")->setText("0.5");
  field<QLineEdit>(tools, "height")->setText("4");
  field<QLineEdit>(tools, "anchor_x")->setText("-4");
  field<QLineEdit>(tools, "anchor_y")->setText("-2");
  support(w, EmbossMapping::Planar);
  field<QPushButton>(tools, "token_add")->click();
  auto* table = field<QTableWidget>(tools, "tokens");
  REQUIRE(table->rowCount() == 1);
  table->item(0, 0)->setText("n");
  auto* parameter = qobject_cast<QComboBox*>(table->cellWidget(0, 1));
  REQUIRE(parameter);
  parameter->setCurrentIndex(parameter->findData("count"));
  preview(tools);
  CHECK(source(w) == before);
  reveal(field<QLineEdit>(tools, "text"));
  capture(w, "emboss-text-input");
  REQUIRE(tools.apply());
  CHECK(volume(w) > 1600);
  const auto& e = std::get<EmbossIntent>(
      w.session().part_document()->sketch_driven_features().back().definition().intent);
  const auto& text = std::get<EmbossTextSource>(e.source);
  CHECK(text.text.template_text() == "O${n}");
  CHECK(f::checked(text.text.resolve(*w.session().part_document())).text == "O3");
  const double first = volume(w);
  REQUIRE(w.session().commit_part_transaction("Bound label value", [](PartDocument& part) {
    auto changed = part.set_parameter_value(ParameterId("count"), f::checked(Quantity::count(5)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  w.refresh();
  CHECK(std::abs(volume(w) - first) > 1e-5);
  auto mass = f::checked(
      geometry::BodyResultInspector{}.inspect_physical_properties(*w.session().part_document()));
  REQUIRE(mass.data().known_mass);
  CHECK(mass.data().known_mass->mass_kg == Catch::Approx(volume(w) * 4e-6).margin(1e-9));
  const auto token_state = source(w);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("sketch_driven.1")));
  field<QLineEdit>(tools, "height")->setText("5");
  preview(tools);
  reveal(field<QLineEdit>(tools, "height"));
  capture(w, "emboss-text-height");
  REQUIRE(tools.apply());
  const auto after = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == token_state);
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  const auto path = dir.filePath("label.blcad.json");
  REQUIRE(w.save_to(path));
  REQUIRE(w.open_path(path));
  CHECK(source(w) == after);
  REQUIRE(geometry::StepExporter{}.write_part_step(*w.session().part_document(),
                                                   *w.session().part_shape_cache(),
                                                   dir.filePath("label.step").toStdString()));
  w.hide();
}
TEST_CASE("Native cylindrical Engrave preserves explicit seam and normal depth through history",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native wrapped engraving");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, cylinder(), dir);
  show(w);
  auto& tools = w.sketch_driven_tools();
  const auto before = source(w);
  const auto faces = f::checked(geometry::SketchDrivenGuideQuery::faces(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base")));
  const auto selected = std::find_if(faces.begin(), faces.end(), [](const auto& target) {
    return target.mapping == EmbossMapping::Cylindrical;
  });
  REQUIRE(selected != faces.end());
  w.viewport().set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Face));
  w.refresh_feature_selection_scene();
  REQUIRE(w.viewport().select_semantic(selected->selection.semantic_id));
  REQUIRE(tools.begin(Tools::Mode::Engrave));
  CHECK(field<QComboBox>(tools, "mapping")->currentIndex() == 1);
  CHECK(field<QListWidget>(tools, "supports")->count() == 1);
  select(tools, "sketch", "label.sketch");
  field<QComboBox>(tools, "source_kind")->setCurrentIndex(1);
  field<QComboBox>(tools, "mapping")->setCurrentIndex(1);
  field<QLineEdit>(tools, "anchor_y")->setText("10");
  field<QLineEdit>(tools, "seam")->setText("360");
  field<QLineEdit>(tools, "size")->setText("0.5");
  support(w, EmbossMapping::Cylindrical);
  field<QPushButton>(tools, "center_axis")->click();
  preview(tools);
  auto seam =
      std::find_if(tools.manipulators().handles().begin(), tools.manipulators().handles().end(),
                   [](const auto& handle) { return handle.id == "sketch_driven/seam"; });
  REQUIRE(seam != tools.manipulators().handles().end());
  CHECK(seam->reference_value == 360);
  CHECK(std::abs(seam->axis.z) == 1);
  auto depth =
      std::find_if(tools.manipulators().handles().begin(), tools.manipulators().handles().end(),
                   [](const auto& handle) { return handle.id == "sketch_driven/depth"; });
  REQUIRE(depth != tools.manipulators().handles().end());
  CHECK(depth->origin.z == Catch::Approx(10).margin(1e-6));
  CHECK((depth->origin.x * depth->axis.x + depth->origin.y * depth->axis.y) / 10 ==
        Catch::Approx(-1).margin(1e-8));
  reveal(field<QLineEdit>(tools, "seam"));
  capture(w, "engrave-wrap-seam");
  REQUIRE(tools.apply());
  CHECK(volume(w) == Catch::Approx(2000 * std::numbers::pi - 12 * (.5 - .25 / 20)).margin(4e-4));
  const auto after = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("sketch_driven.1")));
  field<QComboBox>(tools, "mapping")->setCurrentIndex(0);
  field<QLineEdit>(tools, "seam")->setText("0");
  CHECK_FALSE(tools.preview());
  CHECK(source(w) == after);
  CHECK_FALSE(tools.manipulators().handles().empty());
  tools.cancel();
  CHECK(source(w) == after);
  const auto path = dir.filePath("wrapped.blcad.json");
  REQUIRE(w.save_to(path));
  REQUIRE(w.open_path(path));
  CHECK(source(w) == after);
  REQUIRE(geometry::StepExporter{}.write_part_step(*w.session().part_document(),
                                                   *w.session().part_shape_cache(),
                                                   dir.filePath("wrapped.step").toStdString()));
  w.hide();
}

namespace {
PartDocument with_limit() {
  auto p = r::fixture();
  auto plane = f::checked(ConstructionPlane::create_explicit(ConstructionPlaneId("limit.plane"),
                                                             "Limit plane", {0, 0, 16}, {1, 0, 0},
                                                             {0, 1, 0}, {0, 0, 1}));
  f::accept(p.add_construction_plane(plane));
  auto sketch = f::checked(
      Sketch::create(SketchId("limit.sketch"), "Limit", DatumPlaneId(plane.id().value())));
  f::accept(sketch.add_profile(f::checked(RectangleProfile::create(
      ProfileId("limit.profile"), ParameterId("width"), ParameterId("height")))));
  f::accept(p.add_sketch(sketch));
  auto context = f::new_body(p, "limit");
  auto feature = f::checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit",
                                                             sketch.id(), ParameterId("depth")));
  f::accept(p.add_feature(f::checked(feature.with_body_result_context(context))));
  return p;
}
void choose_limit(Tools& t) {
  auto* box = field<QComboBox>(t, "limit_face");
  int chosen = -1;
  for (int i = 0; i < box->count(); ++i)
    if (box->itemText(i).contains("limit / ") && box->itemText(i).endsWith(", 16)")) {
      REQUIRE(chosen < 0);
      chosen = i;
    }
  REQUIRE(chosen >= 0);
  box->setCurrentIndex(chosen);
}
} // namespace
TEST_CASE("Native Rib termination repair retains exact invalid intent through Undo and reopen",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native termination selection and repair");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, with_limit(), dir);
  show(w);
  auto& tools = w.sketch_driven_tools();
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  select(tools, "body", "body.base");
  field<QComboBox>(tools, "extent")->setCurrentIndex(1);
  choose_limit(tools);
  preview(tools);
  reveal(field<QComboBox>(tools, "limit_face"));
  capture(w, "rib-termination");
  REQUIRE(tools.apply());
  CHECK(volume(w) == Catch::Approx(1728).margin(1e-5));
  auto broken = *w.session().part_document();
  auto old = *broken.find_sketch_driven_feature(FeatureId("sketch_driven.1"));
  auto intent = std::get<RibWebIntent>(old.definition().intent);
  auto lost = f::checked(ResultFaceReference::create(FeatureId("limit"), BodyId("body.limit"),
                                                     {{FeatureId("limit"), "lost/support"}}));
  intent.extent = RibFaceExtent{
      f::checked(FaceReference::create_result(PartFeatureInputRole::ExtrudeLimitFace, lost))};
  f::accept(broken.update_sketch_driven_feature(f::checked(
      SketchDrivenFeature::create_rib_web(old.id(), old.name(), old.target_body(), intent))));
  const auto invalid = f::checked(serialize_part_document_to_json(broken));
  REQUIRE(w.save_to(dir.filePath("valid.blcad.json")));
  const auto bad = dir.filePath("broken.blcad.json");
  {
    std::ofstream out(bad.toStdString());
    out << invalid;
  }
  REQUIRE(w.open_path(bad, GuiDocumentOpenMode::ReferenceRepair));
  REQUIRE(w.session().reference_repair_mode());
  w.reference_repair_tools().cancel();
  REQUIRE(w.feature_edit_tools().begin(old.id()));
  CHECK_FALSE(tools.preview());
  CHECK(source(w) == invalid);
  choose_limit(tools);
  preview(tools);
  reveal(field<QComboBox>(tools, "limit_face"));
  capture(w, "rib-termination-repair");
  REQUIRE(tools.apply());
  CHECK_FALSE(w.session().reference_repair_mode());
  CHECK(volume(w) == Catch::Approx(1728).margin(1e-5));
  const auto repaired = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == invalid);
  CHECK(w.session().reference_repair_mode());
  REQUIRE(w.redo());
  CHECK(source(w) == repaired);
  CHECK_FALSE(w.session().reference_repair_mode());
  const auto path = dir.filePath("repaired.blcad.json");
  REQUIRE(w.save_to(path));
  REQUIRE(w.open_path(path));
  CHECK(source(w) == repaired);
  REQUIRE(geometry::StepExporter{}.write_part_step(*w.session().part_document(),
                                                   *w.session().part_shape_cache(),
                                                   dir.filePath("repaired.step").toStdString()));
  w.hide();
}
TEST_CASE("Rib insertion validates a complete suffix before publishing its End of Part command",
          "[gui][sketch-driven-tools][parity-b184]") {
  auto p = r::fixture();
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("suffix"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {30, 0, 0}, false, false))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, p, dir);
  const auto before = source(w);
  REQUIRE(w.session().begin_part_history_context(1));
  auto& tools = w.sketch_driven_tools();
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  preview(tools);
  REQUIRE(tools.apply());
  CHECK(w.session().history_prefix_ready());
  auto history = f::checked(w.session().part_document()->executable_history());
  REQUIRE(history.size() == 3);
  CHECK(history[1].node_id == "sketch_driven.1");
  CHECK(history[2].node_id == "suffix");
  REQUIRE(w.session().publish_part_workflow("Insert Rib"));
  w.refresh();
  CHECK(w.session().derived_results_fresh());
  CHECK(volume(w) == Catch::Approx(1664).margin(1e-5));
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(volume(w) == Catch::Approx(1664).margin(1e-5));
}
TEST_CASE("Native generated Rib edge remains selectable by the ordinary Fillet tool",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native generated-edge selection");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, r::fixture(), dir);
  show(w);
  auto& rib = w.sketch_driven_tools();
  REQUIRE(rib.begin(Tools::Mode::Rib));
  rib_inputs(rib);
  preview(rib);
  REQUIRE(rib.apply());
  const auto before = source(w);
  auto targets = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base"),
      PartFeatureInputRole::FilletEdge));
  auto edge = std::find_if(targets.begin(), targets.end(), [](const auto& t) {
    return std::abs(t.anchor.x + 8) < 1e-6 && std::abs(t.anchor.y) < 1e-6 &&
           std::abs(t.anchor.z - 12) < 1e-6;
  });
  REQUIRE(edge != targets.end());
  auto& finishing = w.feature_tools();
  REQUIRE(finishing.begin_finishing(ShellFeatureTools::Command::Fillet));
  (void)finishing.set_numeric_value(.1);
  REQUIRE(w.viewport().select_semantic(edge->semantic_id));
  qApp->processEvents();
  REQUIRE(finishing.set_numeric_value(.1));
  REQUIRE(w.viewport().feature_preview_active());
  capture(w, "rib-generated-fillet");
  REQUIRE(finishing.apply());
  CHECK(volume(w) == Catch::Approx(1664 - (1 - std::numbers::pi / 4) * .01).margin(3e-5));
  const auto after = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  w.hide();
}
namespace {
template <class T> T* hole_field(ShellWindow& w, const char* path) {
  auto* field = qobject_cast<T*>(w.feature_edit_tools().field(path));
  INFO(path);
  REQUIRE(field);
  return field;
}
std::string web_face(ShellWindow& w, int role) {
  auto faces = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base"),
      PartFeatureInputRole::FilletSupportFace));
  std::optional<std::string> selected;
  for (const auto& face : faces) {
    const auto p = face.anchor;
    bool matches = p.x < -1 && (role == 0   ? std::abs(p.y - .5) < 1e-6 && p.z > 9 && p.z < 11
                                : role == 1 ? std::abs(p.y + .5) < 1e-6 && p.z > 9 && p.z < 11
                                            : std::abs(p.y) < 1e-6 && std::abs(p.z - 12) < 1e-6);
    if (matches) {
      REQUIRE_FALSE(selected);
      selected = face.semantic_id;
    }
  }
  REQUIRE(selected);
  return *selected;
}
} // namespace
TEST_CASE("Native cross Web supports ordinary Hole full round and upstream thickness history",
          "[gui][native-sketch-driven][parity-b184]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native F03 consumer workflow");
  auto p = r::fixture({f::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0})),
                       f::checked(LineSegment::create(SketchEntityId("cross"), {0, -4}, {0, 4}))});
  ShellWindow w;
  QTemporaryDir dir;
  open(w, p, dir);
  show(w);
  auto& web = w.sketch_driven_tools();
  REQUIRE(web.begin(Tools::Mode::Web));
  rib_inputs(web);
  select(web, "curve", "cross");
  field<QPushButton>(web, "curve_add")->click();
  REQUIRE(field<QListWidget>(web, "curves")->count() == 2);
  preview(web);
  REQUIRE(web.apply());
  CHECK(volume(w) == Catch::Approx(1692).margin(1e-5));
  auto choices = f::checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base"),
      PartFeatureInputRole::ShellRemovalFace));
  const auto side_id = web_face(w, 0);
  auto side = std::find_if(choices.begin(), choices.end(),
                           [&](const auto& f) { return f.semantic_id == side_id; });
  REQUIRE(side != choices.end());
  const auto& ref = std::get<FaceReference>(side->reference);
  const auto* captured = std::get_if<ResultFaceReference>(&ref.source());
  REQUIRE(captured);
  const auto frame = f::checked(geometry::ResultFaceSupportGeometry::resolve_current(
                                    *w.session().part_document(), *w.session().part_shape_cache(),
                                    BodyId("body.base"), captured->origins()))
                         .plane;
  w.session().selection().clear();
  REQUIRE(w.session().selection().add({GuiSelectionKind::Face, side_id}));
  REQUIRE(w.feature_edit_tools().begin_new_hole());
  hole_field<QDoubleSpinBox>(w, "/diameter/parameter")->setValue(.2);
  const Vector3 delta{-4 - frame.origin.x, .5 - frame.origin.y, 10 - frame.origin.z};
  hole_field<QDoubleSpinBox>(w, "/placements/0/position/x")->setValue(dot(delta, frame.x_axis));
  hole_field<QDoubleSpinBox>(w, "/placements/0/position/y")->setValue(dot(delta, frame.y_axis));
  auto* direction = hole_field<QComboBox>(w, "/direction");
  int inward = direction->findData("opposite_support_normal");
  REQUIRE(inward >= 0);
  direction->setCurrentIndex(inward);
  qApp->processEvents();
  auto hole = w.feature_edit_tools().apply();
  INFO((hole ? "ok" : hole.error().message()));
  REQUIRE(hole);
  CHECK(volume(w) == Catch::Approx(1692 - std::numbers::pi * .01).margin(3e-5));
  capture(w, "web-generated-hole");
  const auto first = web_face(w, 0), second = web_face(w, 1), middle = web_face(w, 2);
  auto& finishing = w.feature_tools();
  REQUIRE(finishing.begin_finishing(ShellFeatureTools::Command::Fillet));
  finishing.finishing_mode_editor().setCurrentIndex(3);
  auto* group = w.findChild<QComboBox*>("blcad.fillet.group");
  REQUIRE(group);
  REQUIRE(w.viewport().select_semantic(first));
  qApp->processEvents();
  group->setCurrentIndex(1);
  REQUIRE(w.viewport().select_semantic(second));
  qApp->processEvents();
  group->setCurrentIndex(2);
  REQUIRE(w.viewport().select_semantic(middle));
  qApp->processEvents();
  auto* continuity = w.findChild<QComboBox*>("blcad.fillet.continuity");
  REQUIRE(continuity);
  continuity->setCurrentIndex(1);
  REQUIRE(w.viewport().feature_preview_active());
  capture(w, "web-full-round");
  REQUIRE(finishing.apply());
  CHECK(volume(w) ==
        Catch::Approx(1692 - std::numbers::pi * .01 - (2. / 7) * .25 * 7.5).margin(4e-5));
  const auto before = source(w);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("sketch_driven.1")));
  field<QLineEdit>(web, "size")->setText("1.2");
  preview(web);
  REQUIRE(web.apply());
  const double expected =
      1600 + (24 - 1.2) * 1.2 * 4 - std::numbers::pi * .01 * 1.2 - (2. / 7) * .36 * 7.4;
  CHECK(volume(w) == Catch::Approx(expected).margin(5e-5));
  const auto after = source(w);
  REQUIRE(w.undo());
  CHECK(source(w) == before);
  REQUIRE(w.redo());
  CHECK(source(w) == after);
  const auto path = dir.filePath("web-consumers.blcad.json");
  REQUIRE(w.save_to(path));
  REQUIRE(w.open_path(path));
  CHECK(source(w) == after);
  REQUIRE(geometry::StepExporter{}.write_part_step(
      *w.session().part_document(), *w.session().part_shape_cache(),
      dir.filePath("web-consumers.step").toStdString()));
  w.hide();
}
TEST_CASE("Formula thickness stays bound until explicit New value rebinding",
          "[gui][sketch-driven-tools][parity-b184]") {
  auto p = r::fixture();
  f::accept(p.add_expression_parameter(ParameterId("computed"), "computed", ParameterType::Length,
                                       "radius / 2"));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, p, dir);
  auto& tools = w.sketch_driven_tools();
  REQUIRE(tools.begin(Tools::Mode::Rib));
  rib_inputs(tools);
  select(tools, "size_binding", "computed");
  preview(tools);
  CHECK(field<QLineEdit>(tools, "size")->isReadOnly());
  auto handles = tools.manipulators().handles();
  auto thickness = std::find_if(handles.begin(), handles.end(),
                                [](const auto& h) { return h.id == "sketch_driven/thickness"; });
  REQUIRE(thickness != handles.end());
  CHECK_FALSE(thickness->interactive);
  field<QComboBox>(tools, "size_binding")->setCurrentIndex(0);
  field<QLineEdit>(tools, "size")->setText(".8");
  preview(tools);
  REQUIRE(tools.apply());
  CHECK(volume(w) == Catch::Approx(1651.2).margin(1e-5));
  CHECK(w.session().part_document()->find_parameter(ParameterId("computed"))->is_expression());
  CHECK(
      w.session().part_document()->find_parameter(ParameterId("computed"))->value().millimeters() ==
      .5);
}
