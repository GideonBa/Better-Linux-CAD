#include "../support/hole_manufacturing_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
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
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
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
void click_at(QWidget& widget, QPointF point) {
  REQUIRE(widget.isVisible());
  REQUIRE(widget.isEnabled());
  REQUIRE(widget.rect().contains(point.toPoint()));
  REQUIRE(widget.screen()->availableGeometry().contains(widget.mapToGlobal(point.toPoint())));
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, widget.mapToGlobal(point.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
    settle();
  }
}
void click(QWidget& widget) {
  for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(parent))
      scroll->ensureWidgetVisible(&widget);
  settle();
  const auto point = widget.rect().center();
  auto* window = widget.window();
  const auto local = widget.mapTo(window, point);
  REQUIRE(window->rect().contains(local));
  auto* hit = window->childAt(local);
  INFO(widget.objectName().toStdString());
  REQUIRE((hit == &widget || (hit && widget.isAncestorOf(hit))));
  click_at(widget, widget.rect().center());
}
void key(QWidget& widget, int code, const QString& text = {}) {
  for (auto type : {QEvent::KeyPress, QEvent::KeyRelease}) {
    QKeyEvent event(type, code, Qt::NoModifier, text);
    QApplication::sendEvent(&widget, &event);
  }
  settle();
}
void capture(ShellWindow& w, const char* name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  REQUIRE(w.grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png")));
}
PartDocument plate() {
  auto p = holes::part(false, false);
  REQUIRE(p.remove_hole_feature(FeatureId("holes")));
  return p;
}
PartDocument imported_plate(QTemporaryDir& dir, bool posed = false) {
  REQUIRE(dir.isValid());
  const auto root = std::filesystem::path(dir.path().toStdString());
  const auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(80)), checked(Quantity::length_mm(60)),
      checked(Quantity::length_mm(8))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, (root / "supplier.step").string()));
  const auto asset = checked(inspect_step_import_asset(root, "supplier.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source = checked(
      StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                               *asset.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog"), imported, 0));
  auto part = checked(geometry::StepEditableCommands{}.create_definition(
      source, catalog, DocumentId("import.hole"), "Imported plate", FeatureId("import.base"),
      root));
  if (posed)
    REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
        BodyTransformId("pose"), part.bodies().front().id(), BodyTransformCoordinateSpace::World,
        {}, {12, -5, 10}, false, false))));
  return part;
}
std::string imported_top(const PartDocument& part) {
  const auto& catalog = part.imported_topology_catalogs().front();
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        std::abs(entry.descriptor.bounds_mm[2] - 8) < 1e-5 &&
        std::abs(entry.descriptor.bounds_mm[5] - 8) < 1e-5) {
      const auto ref = checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                                 ImportedTopologyCapability::Face));
      return "finishing/" + checked(make_imported_topology_target_spelling(ref));
    }
  FAIL("No imported top face");
  return {};
}
void choose_support(ShellWindow& w, const std::string& semantic) {
  auto& support = field<QComboBox>(w, "/support");
  const int index = support.findData(QString::fromStdString(semantic));
  REQUIRE(index >= 0);
  support.setCurrentIndex(index);
  settle();
}
std::string hole_bottom(ShellWindow& w) {
  const auto faces = checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base"),
      PartFeatureInputRole::ShellRemovalFace));
  for (const auto& face : faces)
    if (const auto* ref = std::get_if<FaceReference>(&face.reference))
      if (const auto* result = std::get_if<ResultFaceReference>(&ref->source());
          result && result->origins().front().role.ends_with("/bottom"))
        return face.semantic_id;
  FAIL("No Hole bottom");
  return {};
}
void create_hole(ShellWindow& w) {
  auto& action = control<QAction>(w, "blcad.action.semantic_hole");
  REQUIRE(action.isEnabled());
  CHECK_FALSE(action.icon().isNull());
  action.trigger();
  settle();
  REQUIRE(w.feature_edit_tools().active());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
}
void choose_hole_mode(ShellWindow& w, const char* name, int index) {
  control<QComboBox>(w, name).setCurrentIndex(index);
  settle();
}
} // namespace

TEST_CASE("Hole recipe creation matches independent seat step cone fit and slot volumes",
          "[gui][parity-b157][hole-recipe-shell]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  CAPTURE(mode);
  auto p = manufacture::seed();
  REQUIRE(p.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(80))));
  REQUIRE(p.set_parameter_value(ParameterId("height"), checked(Quantity::length_mm(60))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", mode);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const double removed[] = {8, 24, 44. / 3, 16, 31, 8, 8, 12};
  const double cut = mode == 8 ? (8 + std::numbers::pi) * 8 : removed[mode] * std::numbers::pi;
  CHECK(volume(w) == Catch::Approx(38400 - cut).margin(1e-6));
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  CHECK(w.session().part_document()->sketches().size() == p.sketches().size());
  replay(w, dir, before);
}

TEST_CASE("An existing Hole acquires only the required recipe dimensions in its original slot",
          "[gui][parity-b157][hole-recipe-shell]") {
  auto p = holes::part(false, false);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  begin(w, "holes");
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  auto& diameter = field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter");
  diameter.setValue(2);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  diameter.setValue(5);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->hole_features().front().id() == FeatureId("holes"));
  CHECK(w.session().part_document()->parameters().size() == p.parameters().size() + 2);
  CHECK(volume(w) == Catch::Approx(38400 - 21.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A sourced Hole seat discloses its pin and converts explicitly to a custom dimension",
          "[gui][parity-b157][hole-recipe-shell]") {
  const bool custom = GENERATE(false, true);
  auto p = plate();
  REQUIRE(p.add_engineering_resource(manufacture::table()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  auto& source = control<QComboBox>(w, "blcad.hole.source/manufacturing/diameter");
  const QStringList pin{"synthetic.holes", "r1", "small", "seat.diameter"};
  const int selected = source.findData(pin);
  REQUIRE(selected > 0);
  CHECK_FALSE(source.itemData(selected, Qt::ToolTipRole).toString().isEmpty());
  source.setCurrentIndex(selected);
  settle();
  CHECK(control<QLabel>(w, "blcad.hole.resolved/manufacturing/diameter").text() == "4 mm");
  CHECK(field<QLabel>(w, "/manufacturing/diameter/table/revision").text() == "r1");
  if (custom) {
    choose_hole_mode(w, "blcad.hole.source/manufacturing/diameter", 0);
    CHECK(field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").value() == 4);
    field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").setValue(5);
  }
  REQUIRE(w.feature_edit_tools().apply());
  const auto& seat = std::get<HoleCounterbore>(
      w.session().part_document()->hole_features().front().manufacturing());
  CHECK(std::holds_alternative<ParameterId>(seat.diameter) == custom);
  if (!custom)
    CHECK(std::get<HoleStandardSelection>(seat.diameter).table == manufacture::table().key());
  CHECK(w.session().part_document()->parameters().size() ==
        p.parameters().size() + (custom ? 3 : 2));
  CHECK(volume(w) == Catch::Approx(38400 - (custom ? 18.5 : 14) * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Recipe transitions retire step identities and invalid tip changes remain correctable",
          "[gui][parity-b157][hole-recipe-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 4);
  const auto old_id = control<QGroupBox>(w, "blcad.feature_edit.entry/manufacturing/steps/0")
                          .property("semanticId");
  choose_hole_mode(w, "blcad.hole.recipe", 0);
  choose_hole_mode(w, "blcad.hole.recipe", 4);
  CHECK(control<QGroupBox>(w, "blcad.feature_edit.entry/manufacturing/steps/0")
            .property("semanticId") != old_id);
  choose_hole_mode(w, "blcad.hole.recipe", 0);
  choose_hole_mode(w, "blcad.hole.tip", 1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QComboBox>(w, "blcad.hole.tip").currentIndex() == 0);
  choose_hole_mode(w, "blcad.hole.extent", 0);
  choose_hole_mode(w, "blcad.hole.tip", 1);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const double cone = 1 / std::tan(59 * std::numbers::pi / 180) / 3;
  CHECK(volume(w) == Catch::Approx(38400 - (5 + cone) * std::numbers::pi).margin(1e-6));
  CHECK(w.session().part_document()->parameters().size() == plate().parameters().size() + 3);
  replay(w, dir, before);
}

TEST_CASE(
    "Internal Thread creation distinguishes drill and thread dimensions and cancels unused inputs",
    "[gui][parity-b157][hole-recipe-shell]") {
  const bool enabled = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  auto p = plate();
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  choose_hole_mode(w, "blcad.hole.internal_thread", 1);
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter").value() == 4);
  CHECK(field<QDoubleSpinBox>(w, "/thread/length").value() == 4);
  field<QDoubleSpinBox>(w, "/thread/length").setValue(20);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  field<QDoubleSpinBox>(w, "/thread/length").setValue(3);
  if (!enabled)
    choose_hole_mode(w, "blcad.hole.internal_thread", 0);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->hole_features().front().thread().has_value() == enabled);
  CHECK(w.session().part_document()->parameters().size() ==
        p.parameters().size() + (enabled ? 6 : 1));
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Native Hole recipe fields support keyboard transitions numeric correction and mouse Apply",
    "[gui][parity-b157][hole-recipe-shell][native-hole-recipe]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native recipe controls and OCCT rendering");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  auto& recipe = control<QComboBox>(w, "blcad.hole.recipe");
  auto& scroll = control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll");
  scroll.ensureWidgetVisible(&recipe);
  w.raise();
  w.activateWindow();
  settle();
  recipe.setFocus();
  settle();
  REQUIRE(QApplication::focusWidget() == &recipe);
  key(recipe, Qt::Key_Down);
  REQUIRE(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 1);
  REQUIRE(QApplication::focusWidget() == &control<QComboBox>(w, "blcad.hole.recipe"));
  key(control<QComboBox>(w, "blcad.hole.recipe"), Qt::Key_Down);
  REQUIRE(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 2);
  REQUIRE(QApplication::focusWidget() == &control<QComboBox>(w, "blcad.hole.recipe"));
  key(control<QComboBox>(w, "blcad.hole.recipe"), Qt::Key_Up);
  REQUIRE(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 1);
  auto& diameter = field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter");
  scroll.ensureWidgetVisible(&diameter);
  diameter.setFocus();
  diameter.selectAll();
  key(diameter, Qt::Key_5, "5");
  key(diameter, Qt::Key_Return);
  CHECK(diameter.value() == 5);
  CHECK(scroll.horizontalScrollBar()->maximum() == 0);
  CHECK(snapshot(w) == before);
  capture(w, "hole-recipe-preview");
  auto& apply = control<QPushButton>(w, "blcad.feature_edit.apply");
  REQUIRE(w.rect().contains(apply.mapTo(&w, apply.rect().center())));
  click(apply);
  CHECK(volume(w) == Catch::Approx(38400 - 18.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Semantic Hole creation previews disposable inputs and publishes one typed transaction",
          "[gui][parity-b157][hole-create-shell]") {
  const auto p = plate();
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(10);
  CHECK(snapshot(w) == before);
  const auto during = dir.filePath("creation-preview.blcad.json");
  REQUIRE(w.save_to(during));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(during.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  REQUIRE(result.hole_features().size() == 1);
  CHECK(result.features().size() == p.features().size());
  CHECK(result.sketches().size() == p.sketches().size());
  CHECK(result.parameters().size() == p.parameters().size() + 1);
  const auto& hole = result.hole_features().front();
  CHECK(hole.kind() == HoleKind::Plain);
  CHECK(std::holds_alternative<HoleThroughAllExtent>(hole.extent()));
  CHECK(std::get<Point2>(hole.placements().front().source).x == 10);
  CHECK_FALSE(hole.placements().front().id.empty());
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  CHECK(w.session().undo_label() == "Create Hole");
  replay(w, dir, before);
  auto& repeat = control<QAction>(w, "blcad.action.repeat_last_command");
  REQUIRE(repeat.isEnabled());
  repeat.trigger();
  REQUIRE(w.feature_edit_tools().active());
  w.feature_edit_tools().cancel();
}

TEST_CASE("Semantic Hole creation only publishes used pending parameters",
          "[gui][parity-b157][hole-create-shell]") {
  const auto p = plate();
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  binding.setCurrentIndex(binding.findData("diameter"));
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->parameters().size() == p.parameters().size());
  CHECK(std::get<ParameterId>(w.session().part_document()->hole_features().front().diameter()) ==
        ParameterId("diameter"));
  CHECK(volume(w) == Catch::Approx(38400 - 18 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Invalid Hole creation remains correctable and Cancel restores the consumed context",
          "[gui][parity-b157][hole-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.base"}));
  REQUIRE(w.viewport().select_semantic("body/body.base"));
  const auto before = snapshot(w);
  const auto undo = std::string(w.session().undo_label());
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(800);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK_FALSE(w.feature_edit_tools().apply());
  CHECK(snapshot(w) == before);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK(w.session().undo_label() == undo);
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  REQUIRE(w.session().selection().entries().size() == 1);
  CHECK(w.session().selection().entries().front().semantic_id == "body/body.base");
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK_FALSE(w.session().task().active());
}

TEST_CASE("Create Another owns a fresh identity and one undo step per published Hole",
          "[gui][parity-b157][hole-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-10);
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  const auto first = snapshot(w);
  const auto first_hole = w.session().part_document()->hole_features().front();
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  CHECK(field<QDoubleSpinBox>(w, "/diameter/parameter").value() == 4);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(10);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& last = w.session().part_document()->hole_features().back();
  CHECK(last.id() != first_hole.id());
  CHECK(last.placements().front().id != first_hole.placements().front().id);
  CHECK(volume(w) == Catch::Approx(38400 - 64 * std::numbers::pi).margin(1e-6));
  replay(w, dir, first);
  REQUIRE(w.undo());
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
}

TEST_CASE("Semantic Hole action rejects unavailable targets and an existing command",
          "[gui][parity-b157][hole-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  REQUIRE(w.session().set_presentation_visible("body/body.base", false));
  w.refresh();
  CHECK_FALSE(control<QAction>(w, "blcad.action.semantic_hole").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
  REQUIRE(w.session().set_presentation_visible("body/body.base", true));
  w.refresh();
  REQUIRE(w.session().task().begin("other.command"));
  CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
  REQUIRE(w.session().task().cancel());
  CHECK(snapshot(w) == before);
}

TEST_CASE("Changing the Hole target requires a new support and cuts only the chosen body",
          "[gui][parity-b157][hole-create-shell]") {
  auto p = plate();
  const auto context = blcad::test::history_fixture::new_body(p, "other");
  const auto extrude = checked(Feature::create_additive_extrude(
      FeatureId("other"), "Other", SketchId("profile"), ParameterId("depth")));
  REQUIRE(p.add_feature(checked(extrude.with_body_result_context(context))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  auto& target = field<QComboBox>(w, "/target_body");
  target.setCurrentIndex(target.findData("body.other"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  settle();
  auto& support = field<QComboBox>(w, "/support");
  CHECK(support.currentIndex() == -1);
  CHECK_FALSE(w.feature_edit_tools().apply());
  int top = -1;
  for (int i = 0; i < support.count(); ++i)
    if (support.itemData(i).toString().contains("other") &&
        support.itemData(i).toString().contains("top"))
      top = i;
  REQUIRE(top >= 0);
  support.setCurrentIndex(top);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->hole_features().front().target_body() == BodyId("body.other"));
  CHECK(std::get<SemanticFaceReference>(
            w.session().part_document()->hole_features().front().support().source())
            .source_feature() == FeatureId("other"));
  CHECK(volume(w) == Catch::Approx(38400).margin(1e-6));
  CHECK(volume(w, "body.other") == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Hole face preselection resolves the same typed support and rejects a nonplanar datum",
          "[gui][parity-b157][hole-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  const auto faces = checked(geometry::FinishingSelectionBuilder::build(
      *w.session().part_document(), *w.session().part_shape_cache(), BodyId("body.base"),
      PartFeatureInputRole::ShellRemovalFace));
  auto top = std::find_if(faces.begin(), faces.end(), [](const auto& face) {
    const auto* ref = std::get_if<FaceReference>(&face.reference);
    const auto* semantic = ref ? std::get_if<SemanticFaceReference>(&ref->source()) : nullptr;
    return semantic && semantic->face() == SemanticFace::Top;
  });
  REQUIRE(top != faces.end());
  REQUIRE(w.session().selection().add({GuiSelectionKind::Face, top->semantic_id}));
  create_hole(w);
  CHECK(field<QComboBox>(w, "/support").currentData().toString().toStdString() == top->semantic_id);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& support = std::get<SemanticFaceReference>(
      w.session().part_document()->hole_features().front().support().source());
  CHECK(support.source_feature() == FeatureId("base"));
  CHECK(support.face() == SemanticFace::Top);
  replay(w, dir, before);
  w.session().selection().clear();
  REQUIRE(w.session().selection().add({GuiSelectionKind::Datum, "datum-axis/axis"}));
  CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
  CHECK_FALSE(w.feature_edit_tools().active());
  CHECK_FALSE(w.session().task().active());
}

TEST_CASE("Native semantic Hole creation supports scoped picking keyboard input and mouse Apply",
          "[gui][parity-b157][hole-create-shell][native-hole-create]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Hole creation and OCCT rendering");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  click(control<QToolButton>(w, "blcad.hole.target.pick"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  const auto screen = checked(w.viewport().model_to_screen({8, 6, 8}));
  click_at(w.viewport(), {screen.x, screen.y});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  click(control<QToolButton>(w, "blcad.hole.support.pick"));
  const auto face_screen = checked(w.viewport().model_to_screen({8, 6, 8}));
  click_at(w.viewport(), {face_screen.x, face_screen.y});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(field<QComboBox>(w, "/support").currentData() == "finishing/face/4:base/top");
  auto& diameter = field<QDoubleSpinBox>(w, "/diameter/parameter");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&diameter);
  diameter.setFocus();
  diameter.selectAll();
  key(diameter, Qt::Key_4, "4");
  key(diameter, Qt::Key_Return);
  CHECK(diameter.value() == 4);
  CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
            .horizontalScrollBar()
            ->maximum() == 0);
  CHECK(snapshot(w) == before);
  capture(w, "hole-creation-preview");
  auto& apply = control<QPushButton>(w, "blcad.feature_edit.apply");
  CHECK(w.rect().contains(apply.mapTo(&w, apply.rect().center())));
  click(apply);
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

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

TEST_CASE(
    "Hole termination choices execute the selected limit and preserve the separate limit body",
    "[gui][parity-b157][hole-recipe-shell]") {
  const int extent = GENERATE(0, 1, 2, 3, 4, 5);
  CAPTURE(extent);
  auto p = plate();
  REQUIRE(p.add_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("limit.plane"), "Limit plane",
                                                 {0, 0, -4}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  blcad::test::history_fixture::rectangle(p, "limit.profile", "limit.plane");
  auto body = blcad::test::history_fixture::new_body(p, "limit");
  auto extrude = checked(Feature::create_additive_extrude(
      FeatureId("limit"), "Limit", SketchId("limit.profile"), ParameterId("depth")));
  REQUIRE(p.add_feature(checked(extrude.with_body_result_context(body))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.extent", extent);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const double depths[] = {5, 8, 8, 8, 4, 4};
  if (extent == 3)
    CHECK(std::get<HoleToFaceExtent>(w.session().part_document()->hole_features().front().extent())
              .face.role() == PartFeatureInputRole::HoleLimitFace);
  CHECK(volume(w) == Catch::Approx(38400 - depths[extent] * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.limit") == Catch::Approx(38400).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Recipe adoption retains explicitly edited shared parameters and prunes the unused seed",
          "[gui][parity-b157][hole-recipe-shell]") {
  auto p = plate();
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  binding.setCurrentIndex(binding.findData("diameter"));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").setValue(4);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(
      w.session().part_document()->find_parameter(ParameterId("diameter"))->value().millimeters() ==
      2);
  CHECK(w.session().part_document()->parameters().size() == p.parameters().size() + 2);
  CHECK(volume(w) == Catch::Approx(38400 - 14 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Seat angle sources offer angle cells and preserve their dimensional meaning",
          "[gui][parity-b157][hole-recipe-shell]") {
  auto p = plate();
  REQUIRE(p.add_engineering_resource(manufacture::table()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 2);
  CHECK(field<QDoubleSpinBox>(w, "/manufacturing/included_angle/parameter").suffix() == " deg");
  auto& source = control<QComboBox>(w, "blcad.hole.source/manufacturing/included_angle");
  REQUIRE(source.count() == 2);
  source.setCurrentIndex(1);
  settle();
  CHECK(control<QLabel>(w, "blcad.hole.resolved/manufacturing/included_angle").text() == "90 deg");
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(38400 - 44. / 3 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Returning to a custom seat preserves a dimension deliberately shared by another field",
          "[gui][parity-b157][hole-recipe-shell]") {
  auto p = plate();
  REQUIRE(p.add_engineering_resource(manufacture::table()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, p);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  const QStringList pin{"synthetic.holes", "r1", "small", "seat.diameter"};
  auto select_standard = [&] {
    auto& source = control<QComboBox>(w, "blcad.hole.source/manufacturing/diameter");
    const auto index = source.findData(pin);
    REQUIRE(index > 0);
    source.setCurrentIndex(index);
    settle();
  };
  select_standard();
  choose_hole_mode(w, "blcad.hole.source/manufacturing/diameter", 0);
  field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").setValue(5);
  const auto shared =
      control<QComboBox>(w, "blcad.feature_edit.binding/manufacturing/diameter/parameter")
          .currentData();
  auto& depth = control<QComboBox>(w, "blcad.feature_edit.binding/manufacturing/depth/parameter");
  REQUIRE(depth.findData(shared) >= 0);
  depth.setCurrentIndex(depth.findData(shared));
  REQUIRE(field<QDoubleSpinBox>(w, "/manufacturing/depth/parameter").value() == 5);
  select_standard();
  choose_hole_mode(w, "blcad.hole.source/manufacturing/diameter", 0);
  CHECK(field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").value() == 4);
  CHECK(field<QDoubleSpinBox>(w, "/manufacturing/depth/parameter").value() == 5);
  CHECK(control<QComboBox>(w, "blcad.feature_edit.binding/manufacturing/diameter/parameter")
            .currentData() != shared);
  CHECK(control<QComboBox>(w, "blcad.feature_edit.binding/manufacturing/depth/parameter")
            .currentData() == shared);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->parameters().size() == p.parameters().size() + 3);
  CHECK(volume(w) == Catch::Approx(38400 - 23 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Hole creation binds a current imported plane through authored Body placement",
          "[gui][parity-b157][hole-support-shell]") {
  const bool posed = GENERATE(false, true);
  QTemporaryDir dir;
  auto p = imported_plate(dir, posed);
  const auto top = imported_top(p);
  const auto body = p.bodies().front().id().value();
  ShellWindow w;
  open(w, dir, p);
  const auto before = snapshot(w);
  REQUIRE(w.feature_edit_tools().begin_new_hole());
  choose_support(w, top);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& part = *w.session().part_document();
  const auto& hole = part.hole_features().front();
  REQUIRE(std::holds_alternative<ImportedTopologyReference>(hole.support().source()));
  CHECK(std::get<ImportedTopologyReference>(hole.support().source()).capability() ==
        ImportedTopologyCapability::Plane);
  CHECK(hole.support().role() == PartFeatureInputRole::HoleSupportPlane);
  CHECK(part.result_face_workplane_count() == 0);
  CHECK(volume(w, body.c_str()) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  auto axis = checked(HoleInstanceReference::create(hole.id(), hole.placements().front().id,
                                                    HoleReferenceRole::Axis));
  auto frame =
      checked(geometry::HoleInstanceGeometry::resolve(part, axis, *w.session().part_shape_cache()));
  CHECK(frame.frame.origin.x == Catch::Approx(posed ? 12 : 0).margin(1e-6));
  CHECK(frame.frame.origin.y == Catch::Approx(posed ? -5 : 0).margin(1e-6));
  CHECK(frame.frame.origin.z == Catch::Approx(posed ? 18 : 8).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A changed supplier asset cannot publish a pending Hole or its dimensions",
          "[gui][parity-b157][hole-support-shell]") {
  QTemporaryDir dir;
  auto p = imported_plate(dir);
  ShellWindow w;
  open(w, dir, p);
  const auto before = snapshot(w);
  const auto undo = std::string(w.session().undo_label());
  REQUIRE(w.feature_edit_tools().begin_new_hole());
  choose_support(w, imported_top(p));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  QFile asset(dir.filePath("supplier.step"));
  REQUIRE(asset.open(QIODevice::ReadOnly));
  const auto bytes = asset.readAll();
  asset.close();
  REQUIRE(asset.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(asset.write("changed supplier bytes") > 0);
  asset.close();
  CHECK_FALSE(w.feature_edit_tools().apply());
  CHECK(snapshot(w) == before);
  CHECK(w.session().undo_label() == undo);
  CHECK_FALSE(w.session().dirty());
  CHECK_FALSE(w.viewport().feature_preview_active());
  w.feature_edit_tools().cancel();
  REQUIRE(asset.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(asset.write(bytes) == bytes.size());
  asset.close();
}

TEST_CASE("A derived Hole support workplane is pending until its single atomic publication",
          "[gui][parity-b157][hole-support-shell]") {
  const bool commit = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part(false, false));
  const auto before = snapshot(w);
  const auto bottom = hole_bottom(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(1);
  choose_support(w, bottom);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(w.session().part_document()->result_face_workplane_count() == 0);
  CHECK(snapshot(w) == before);
  REQUIRE(w.save_to(dir.filePath("support-preview.blcad.json")));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(w.session().file_path()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  if (!commit) {
    w.feature_edit_tools().cancel();
    CHECK(snapshot(w) == before);
    CHECK_FALSE(w.session().can_undo());
    return;
  }
  REQUIRE(w.feature_edit_tools().apply());
  REQUIRE(w.session().part_document()->result_face_workplane_count() == 1);
  const auto& plane = w.session().part_document()->result_face_workplanes().front();
  const auto& hole = w.session().part_document()->hole_features().back();
  CHECK(std::get<DatumPlaneId>(hole.support().source()) == plane.id());
  CHECK(plane.reference().origins().front().role.ends_with("/bottom"));
  CHECK(volume(w) == Catch::Approx(38400 - 14 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Changing a pending result support prunes its workplane even after a recipe transition",
          "[gui][parity-b157][hole-support-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part(false, false));
  const auto before = snapshot(w);
  const auto bottom = hole_bottom(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(1);
  choose_support(w, bottom);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  choose_support(w, "finishing/face/4:base/top");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->result_face_workplane_count() == 0);
  CHECK(volume(w) == Catch::Approx(38400 - 27.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Native Hole creation picks a bounded derived bottom and publishes only its used workplane",
    "[gui][parity-b157][hole-support-shell][native-hole-support]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires actual derived support picking");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part(false, false));
  const auto before = snapshot(w);
  const auto bottom = hole_bottom(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(1);
  w.viewport().set_standard_view(GuiStandardView::Top);
  settle();
  click(control<QToolButton>(w, "blcad.hole.support.pick"));
  const auto point = checked(w.viewport().model_to_screen({-20, -15, 2}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK(field<QComboBox>(w, "/support").currentData().toString().toStdString() == bottom);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
            .horizontalScrollBar()
            ->maximum() == 0);
  capture(w, "hole-derived-support-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(w.session().part_document()->result_face_workplane_count() == 1);
  CHECK(volume(w) == Catch::Approx(38400 - 14 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native Hole creation picks the actual placed imported face with a typed plane binding",
          "[gui][parity-b157][hole-support-shell][native-hole-support]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires actual imported support picking");
  QTemporaryDir dir;
  auto p = imported_plate(dir, true);
  const auto body = p.bodies().front().id().value();
  const auto top = imported_top(p);
  ShellWindow w;
  open(w, dir, p);
  const auto before = snapshot(w);
  REQUIRE(w.feature_edit_tools().begin_new_hole());
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  w.viewport().set_standard_view(GuiStandardView::Top);
  settle();
  click(control<QToolButton>(w, "blcad.hole.support.pick"));
  const auto point = checked(w.viewport().model_to_screen({20, 1, 18}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK(field<QComboBox>(w, "/support").currentData().toString().toStdString() == top);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-imported-support-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  const auto& reference = std::get<ImportedTopologyReference>(
      w.session().part_document()->hole_features().front().support().source());
  CHECK(reference.capability() == ImportedTopologyCapability::Plane);
  CHECK(w.session().part_document()->result_face_workplane_count() == 0);
  CHECK(volume(w, body.c_str()) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}
