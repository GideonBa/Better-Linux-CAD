#include "../support/hole_manufacturing_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/step_reference_commands.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/gui/gui_hole_conversion.hpp"
#include "blcad/gui/gui_hole_thread_preset.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_history_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
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
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
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
void activate(ShellWindow& w) {
  w.raise();
  w.activateWindow();
  QGuiApplication::sync();
  settle();
  if (!w.isActiveWindow()) {
    QEventLoop activation;
    QTimer poll, deadline;
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &activation, [&] {
      if (w.isActiveWindow())
        activation.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &activation, &QEventLoop::quit);
    poll.start(5);
    deadline.start(2000);
    activation.exec();
  }
  REQUIRE(w.isActiveWindow());
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* found = w.findChild<T*>(QString::fromUtf8(name));
  INFO(name);
  REQUIRE(found);
  return *found;
}
template <class T> T& field(ShellWindow& w, const char* path) {
  auto* found = qobject_cast<T*>(w.feature_edit_tools().field(QString::fromLatin1(path)));
  INFO(path);
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
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  settle();
  if (QApplication::platformName() == "xcb")
    CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
              .horizontalScrollBar()
              ->maximum() == 0);
}
void history_edit(ShellWindow& w, const char* id, bool convert = false) {
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
  auto& action =
      convert ? w.history_tools().convert_hole_action() : w.history_tools().edit_action();
  REQUIRE(action.isEnabled());
  action.trigger();
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
PartDocument imported_shape(QTemporaryDir& dir, const geometry::GeometryShape& shape, bool posed) {
  REQUIRE(dir.isValid());
  const auto root = std::filesystem::path(dir.path().toStdString());
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
PartDocument imported_plate(QTemporaryDir& dir, bool posed = false) {
  const auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(80)), checked(Quantity::length_mm(60)),
      checked(Quantity::length_mm(8))));
  return imported_shape(dir, shape, posed);
}
PartDocument imported_shaft(QTemporaryDir& dir, bool posed = false) {
  const auto part = threads::pair_part();
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("shaft.supplier")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto* shape = cache.find_body_shape(BodyId("body.shaft"));
  REQUIRE(shape);
  return imported_shape(dir, *shape, posed);
}
PartDocument conical_part(double first_radius, double last_radius, double height = 2) {
  auto part = checked(PartDocument::create(DocumentId("cone"), "Conical shaft"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  REQUIRE(part.add_datum_axis(
      checked(DatumAxis::create_explicit(DatumAxisId("cone.axis"), "Cone axis", {}, {0, 1, 0}))));
  auto sketch =
      checked(Sketch::create(SketchId("cone.profile"), "Cone profile", DatumPlaneId("datum.xy")));
  std::vector<Point2> points{{0, 0}};
  if (first_radius > 0)
    points.push_back({first_radius, 0});
  points.push_back({last_radius, height});
  if (last_radius > 0)
    points.push_back({0, height});
  std::vector<SketchEntityId> edges;
  for (std::size_t i = 0; i < points.size(); ++i) {
    edges.emplace_back("edge." + std::to_string(i));
    REQUIRE(sketch.add_entity(
        checked(LineSegment::create(edges.back(), points[i], points[(i + 1) % points.size()]))));
  }
  REQUIRE(sketch.add_profile(checked(ClosedProfile::create(ProfileId("cone.region"), edges))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  const auto body = blcad::test::history_fixture::new_body(part, "cone");
  REQUIRE(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
      FeatureId("cone"), "Conical shaft",
      checked(ProfileRegionReference::create(SketchId("cone.profile"), ProfileId("cone.region"),
                                             PartFeatureInputRole::RevolveProfile)),
      checked(AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                               DatumAxisId("cone.axis"))),
      RevolveAngleExtent::full(), body))));
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
std::string imported_bottom(const PartDocument& part) {
  const auto& catalog = part.imported_topology_catalogs().front();
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        std::abs(entry.descriptor.bounds_mm[2]) < 1e-5 &&
        std::abs(entry.descriptor.bounds_mm[5]) < 1e-5) {
      const auto ref = checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                                 ImportedTopologyCapability::Face));
      return "finishing/" + checked(make_imported_topology_target_spelling(ref));
    }
  FAIL("No imported bottom face");
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
void create_thread(ShellWindow& w) {
  auto& action = control<QAction>(w, "blcad.action.semantic_thread");
  REQUIRE(action.isEnabled());
  CHECK_FALSE(action.icon().isNull());
  action.trigger();
  settle();
  INFO(w.statusBar()->currentMessage().toStdString());
  REQUIRE(w.feature_edit_tools().active());
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
}
void choose_hole_mode(ShellWindow& w, const char* name, int index) {
  control<QComboBox>(w, name).setCurrentIndex(index);
  settle();
}
void thread_choice(ShellWindow& w, const char* name, const QVariant& value, bool native = false) {
  INFO(name);
  auto& initial = control<QComboBox>(w, name);
  const int target = initial.findData(value);
  REQUIRE(target >= 0);
  if (!native) {
    initial.setCurrentIndex(target);
    settle();
    return;
  }
  const int attempts_limit = initial.count() + 1;
  for (int attempts = 0; attempts < attempts_limit; ++attempts) {
    auto& current = control<QComboBox>(w, name);
    if (current.currentIndex() == target)
      return;
    control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&current);
    current.setFocus();
    REQUIRE(current.hasFocus());
    key(current, current.currentIndex() < target ? Qt::Key_Down : Qt::Key_Up);
    REQUIRE(control<QComboBox>(w, name).hasFocus());
  }
  FAIL("Thread selector did not reach its explicit choice");
}
void thread_mapping(ShellWindow& w, const char* revision = "r1", bool native = false,
                    bool invalid_major = false) {
  thread_choice(w, "blcad.thread.source", "standard", native);
  REQUIRE_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_choice(w, "blcad.thread.standard.table",
                QStringList{"synthetic.threads", QString::fromLatin1(revision)}, native);
  thread_choice(w, "blcad.thread.standard.row", "synthetic.row", native);
  CHECK(control<QComboBox>(w, "blcad.thread.standard.major_diameter").findData("angle") == -1);
  CHECK(control<QComboBox>(w, "blcad.thread.standard.included_angle").findData("major") == -1);
  CHECK(control<QComboBox>(w, "blcad.thread.standard.system").findData("pitch") == -1);
  for (const auto& [field, column] : {std::pair{"system", "system"},
                                      {"designation", "designation"},
                                      {"tolerance", "tolerance"},
                                      {"major_diameter", invalid_major ? "minor" : "major"},
                                      {"minor_diameter", "minor"},
                                      {"pitch", "pitch"},
                                      {"included_angle", "angle"}}) {
    const auto name = "blcad.thread.standard." + std::string(field);
    REQUIRE(control<QComboBox>(w, name.c_str()).currentIndex() == 0);
    thread_choice(w, name.c_str(), QString::fromLatin1(column), native);
  }
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

TEST_CASE("External Thread creation publishes only its used custom inputs in one transaction",
          "[gui][parity-b157][thread-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = threads::pair_part();
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  CHECK(field<QComboBox>(w, "/target_body").currentData() == "body.shaft");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/major_diameter").value() == 10);
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter").value() == 8);
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/pitch").value() == 1.5);
  CHECK(snapshot(w) == before);
  const auto during = dir.filePath("thread-preview.blcad.json");
  REQUIRE(w.save_to(during));
  GuiDocumentSession saved;
  REQUIRE(saved.open_file(during.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved.part_document())) == before);
  // Rebinding to an existing parameter must prune the unused owned length.
  auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/thread/length");
  binding.setCurrentIndex(binding.findData("thread.length"));
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  REQUIRE(result.thread_features().size() == 1);
  const auto& thread = result.thread_features().front();
  CHECK(thread.target_body() == BodyId("body.shaft"));
  CHECK(thread.support().role() == PartFeatureInputRole::ThreadSupportFace);
  CHECK(thread.thread().length() == ParameterId("thread.length"));
  CHECK_FALSE(thread.thread().start());
  CHECK(result.parameters().size() == part.parameters().size() + 4);
  CHECK(result.sketches().size() == part.sketches().size());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w) == Catch::Approx(2560).margin(1e-6));
  REQUIRE(checked(geometry::ThreadAdapter::cosmetic_presentations(
                      *w.session().part_shape_cache()->find_body_shape(BodyId("body.shaft"))))
              .size() == 1);
  CHECK(w.session().undo_label() == "Create Thread");
  replay(w, dir, before);
}

TEST_CASE("A new external Thread switches to modeled geometry and exports its physical cut",
          "[gui][parity-b157][thread-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  const auto before = snapshot(w);
  create_thread(w);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  mode.setCurrentIndex(mode.findData("modeled"));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const double removed =
      4 * std::numbers::pi * std::tan(std::numbers::pi / 6) * 4 / 1.5 * (4. / 2 + 1. / 3);
  const double expected = 200 * std::numbers::pi - removed;
  CHECK(volume(w, "body.shaft") == Catch::Approx(expected).margin(2e-4));
  const auto root = std::filesystem::path(dir.path().toStdString());
  REQUIRE(geometry::StepExporter{}.write_part_step(
      *w.session().part_document(), *w.session().part_shape_cache(), root / "modeled.step"));
  const auto asset = checked(inspect_step_import_asset(root, "modeled.step"));
  const auto source = checked(
      StepImportSource::create(StepImportSourceId("roundtrip"), "Thread roundtrip", "modeled.step",
                               *asset.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  REQUIRE(imported.body_count() == 2);
  double total = 0;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      total += body.volume_mm3;
  CHECK(total == Catch::Approx(2560 + expected).margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE("Invalid external Thread creation restores the consumed context without leaking inputs",
          "[gui][parity-b157][thread-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.shaft"}));
  REQUIRE(w.viewport().select_semantic("body/body.shaft"));
  const auto before = snapshot(w);
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  create_thread(w);
  field<QDoubleSpinBox>(w, "/thread/profile/major_diameter").setValue(20);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().apply());
  CHECK(w.viewport().feature_preview_count() == 0);
  field<QDoubleSpinBox>(w, "/thread/profile/major_diameter").setValue(10);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().dirty());
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  REQUIRE(w.session().selection().entries().size() == 1);
  CHECK(w.session().selection().entries().front().semantic_id == "body/body.shaft");
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/missing"}));
  CHECK_FALSE(w.feature_edit_tools().begin_new_thread());
  CHECK_FALSE(w.feature_edit_tools().active());
  CHECK(snapshot(w) == before);
}

TEST_CASE("External Thread target changes require a current exterior support before Apply",
          "[gui][parity-b157][thread-create-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  create_thread(w);
  auto& target = field<QComboBox>(w, "/target_body");
  target.setCurrentIndex(target.findData("body.base"));
  settle();
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(field<QComboBox>(w, "/support").count() == 0);
  auto& restored = field<QComboBox>(w, "/target_body");
  restored.setCurrentIndex(restored.findData("body.shaft"));
  settle();
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& support = field<QComboBox>(w, "/support");
  REQUIRE(support.count() > 0);
  for (int i = 0; i < support.count(); ++i) {
    support.setCurrentIndex(i);
    REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  }
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
}

TEST_CASE("External Thread creation retains imported capability placement and source freshness",
          "[gui][parity-b157][thread-create-shell]") {
  const bool changed = GENERATE(false, true);
  QTemporaryDir dir;
  const auto part = imported_shaft(dir, true);
  const auto body = part.bodies().front().id();
  ShellWindow w;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  if (changed) {
    QFile supplier(dir.filePath("supplier.step"));
    REQUIRE(supplier.open(QIODevice::Append));
    REQUIRE(supplier.write("\n") == 1);
    supplier.close();
    CHECK_FALSE(w.feature_edit_tools().apply());
    CHECK(snapshot(w) == before);
    CHECK(w.viewport().feature_preview_count() == 0);
    return;
  }
  REQUIRE(w.feature_edit_tools().apply());
  const auto& thread = w.session().part_document()->thread_features().front();
  const auto& ref = std::get<ImportedTopologyReference>(thread.support().source());
  CHECK(ref.capability() == ImportedTopologyCapability::Face);
  CHECK(volume(w, body.value().c_str()) == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  const auto& profile = std::get<ThreadParameterProfile>(thread.thread().profile());
  CHECK(
      w.session().part_document()->find_parameter(profile.major_diameter)->value().millimeters() ==
      Catch::Approx(10));
  replay(w, dir, before);
}

TEST_CASE("Native external Thread creation picks its support and creates a modeled cut",
          "[gui][parity-b157][thread-create-shell][native-thread-create]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native external Thread creation and OCCT rendering");
  const bool imported = GENERATE(false, true);
  QTemporaryDir dir;
  const auto part = imported ? imported_shaft(dir, true) : threads::pair_part();
  const auto body = imported ? part.bodies().front().id().value() : "body.shaft";
  ShellWindow w;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  w.viewport().set_standard_view(GuiStandardView::Front);
  settle();
  const Point3 surface = imported ? Point3{42, -10, 14} : Point3{30, -5, 4};
  click(control<QToolButton>(w, "blcad.thread.target.pick"));
  auto projected = checked(w.viewport().model_to_screen(surface));
  click_at(w.viewport(), {projected.x, projected.y});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  click(control<QToolButton>(w, "blcad.thread.support.pick"));
  projected = checked(w.viewport().model_to_screen(surface));
  click_at(w.viewport(), {projected.x, projected.y});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& length = field<QDoubleSpinBox>(w, "/thread/length");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&length);
  length.setFocus();
  length.selectAll();
  key(length, Qt::Key_3, "3");
  key(length, Qt::Key_Return);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&mode);
  mode.setFocus();
  key(mode, Qt::Key_Down);
  REQUIRE(mode.currentData() == "modeled");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
            .horizontalScrollBar()
            ->maximum() == 0);
  CHECK(snapshot(w) == before);
  capture(w, imported ? "thread-imported-creation-preview" : "thread-creation-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const double removed =
      4 * std::numbers::pi * std::tan(std::numbers::pi / 6) * 3 / 1.5 * (4. / 2 + 1. / 3);
  CHECK(volume(w, body.c_str()) == Catch::Approx(200 * std::numbers::pi - removed).margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE("Native new internal Thread template switches to valid modeled geometry",
          "[gui][parity-b157][thread-create-shell][native-thread-create]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native internal Thread creation and OCCT rendering");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  choose_hole_mode(w, "blcad.hole.internal_thread", 1);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&mode);
  mode.setFocus();
  key(mode, Qt::Key_Down);
  REQUIRE(mode.currentData() == "modeled");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "thread-internal-creation-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const double removed =
      4 * std::numbers::pi * std::tan(std::numbers::pi / 6) * 4 / 1.5 * (3. / 2 - 1. / 3);
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi - removed).margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE(
    "A new external Thread pins an explicitly mapped revision without publishing unused inputs",
    "[gui][parity-b157][thread-source-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = threads::pair_part();
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  field<QDoubleSpinBox>(w, "/thread/profile/major_diameter").setValue(0);
  auto& system = field<QLineEdit>(w, "/thread/profile/system");
  system.setText("");
  REQUIRE(QMetaObject::invokeMethod(&system, "editingFinished"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_mapping(w, "r2");
  CHECK(control<QComboBox>(w, "blcad.thread.standard.major_diameter")
            .toolTip()
            .contains("Synthetic r2/major"));
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& definition = result.thread_features().front().thread();
  const auto& pin = std::get<ThreadStandardProfile>(definition.profile());
  CHECK(pin.table == threads::selected("r2").table);
  CHECK(pin.row == "synthetic.row");
  CHECK(pin.columns.major_diameter == "major");
  const auto value = checked(resolve_thread_definition(result, definition));
  CHECK(value.pitch_mm == 2.2);
  CHECK(value.system == "Synthetic table system");
  CHECK(value.provenance.size() == 7);
  CHECK(result.engineering_resources() == part.engineering_resources());
  CHECK(result.parameters().size() == part.parameters().size() + 1);
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Incomplete and invalid Thread mappings retain committed intent and remain correctable",
          "[gui][parity-b157][thread-source-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  const auto before = snapshot(w);
  create_thread(w);
  field<QDoubleSpinBox>(w, "/thread/profile/pitch").setValue(1.9);
  thread_choice(w, "blcad.thread.source", "standard");
  CHECK_FALSE(w.feature_edit_tools().apply());
  const auto saved = dir.filePath("incomplete-thread.blcad.json");
  REQUIRE(w.save_to(saved));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(saved.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == before);
  thread_choice(w, "blcad.thread.source", "custom");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/pitch").value() == 1.9);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_mapping(w, "r1", false, true);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().apply());
  CHECK(snapshot(w) == before);
  thread_choice(w, "blcad.thread.standard.major_diameter", "major");
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_choice(w, "blcad.thread.standard.table", QStringList{"synthetic.threads", "r2"});
  CHECK(control<QComboBox>(w, "blcad.thread.standard.row").currentIndex() == 0);
  CHECK(control<QComboBox>(w, "blcad.thread.standard.major_diameter").currentIndex() == 0);
  CHECK_FALSE(w.feature_edit_tools().apply());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
}

namespace {
PartDocument legacy_cut(bool circular = false, bool authored = false, bool shifted_origin = false,
                        bool formula = false) {
  auto part = plate();
  if (formula)
    REQUIRE(part.add_expression_parameter(ParameterId("legacy.diameter"), "legacy_diameter",
                                          ParameterType::Length, "diameter"));
  const ParameterId diameter(formula ? "legacy.diameter" : "diameter");
  auto sketch =
      checked(Sketch::create(SketchId("legacy.sketch"), "Legacy source", DatumPlaneId("datum.xy")));
  if (circular) {
    REQUIRE(part.set_parameter_value(ParameterId("radius"), checked(Quantity::length_mm(10))));
    REQUIRE(sketch.add_profile(checked(CircularHolePattern::create(
        ProfileId("legacy.circle"), ParameterId("radius"), ParameterId("count"), diameter))));
  } else
    REQUIRE(sketch.add_profile(checked(CircleProfile::create(
        ProfileId("legacy.circle"), diameter, shifted_origin ? Point2{-17, 3} : Point2{-15, 0}))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  if (shifted_origin)
    REQUIRE(part.add_sketch_origin_override(
        checked(SketchOriginOverrideRecord::create(SketchId("legacy.sketch"), {2, -3}))));
  auto cut = checked(Feature::create_subtractive_extrude(
      FeatureId("legacy"), "Legacy circle cut", SketchId("legacy.sketch"), FeatureId("base")));
  REQUIRE(part.add_feature(checked(cut.with_body_result_context(holes::context()))));
  if (authored)
    REQUIRE(part.apply_history_intent({{"base", "legacy"}, {}}));
  return part;
}
void legacy_pattern(PartDocument& part, bool posed, bool feature_source = false,
                    bool explicit_placements = false) {
  auto source = holes::source("legacy");
  auto context = holes::context();
  if (!feature_source) {
    source = checked(PatternSourceReference::body(BodyId("body.base")));
    context = blcad::test::history_fixture::new_body(part, "pattern");
    REQUIRE(part.set_parameter_value(ParameterId("spacing.x"), checked(Quantity::length_mm(100))));
  }
  REQUIRE(part.add_linear_pattern_feature(checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Legacy cut pattern", {source}, holes::axis("axis.x"),
      ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
      PatternDirectionSign::Positive, context,
      explicit_placements ? std::optional(PatternPlacementOptions{}) : std::nullopt))));
  if (posed)
    REQUIRE(part.add_body_transform(checked(BodyTransform::create_rotate(
        BodyTransformId("pose"), BodyId(feature_source ? "body.base" : "body.pattern"),
        BodyTransformCoordinateSpace::World, std::nullopt,
        checked(BodyTransformRotationAxis::create_explicit({0, 0, 0}, {0, 0, 1})), 35, false,
        false))));
}
std::vector<std::string> order(const PartDocument& part) {
  std::vector<std::string> ids;
  for (const auto& row : checked(part.executable_history()))
    ids.push_back(row.node_id);
  return ids;
}
} // namespace

TEST_CASE(
    "Explicit legacy conversion preserves canonical order bindings and a retained source Sketch",
    "[gui][parity-b157][hole-conversion]") {
  const bool authored = GENERATE(false, true);
  const bool origin = GENERATE(false, true);
  const bool formula = GENERATE(false, true);
  const auto part = legacy_cut(false, authored, origin, formula);
  const auto before = checked(serialize_part_document_to_json(part));
  const auto converted = checked(GuiHoleConversionCommands::candidate(part, FeatureId("legacy")));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK_FALSE(converted.find_feature(FeatureId("legacy")));
  const auto* hole = converted.find_hole_feature(FeatureId("legacy"));
  REQUIRE(hole);
  CHECK(hole->name() == "Legacy circle cut");
  const auto diameter = std::get<ParameterId>(hole->diameter());
  CHECK(diameter == ParameterId(formula ? "legacy.diameter" : "diameter"));
  CHECK(converted.find_parameter(diameter)->formula() == part.find_parameter(diameter)->formula());
  REQUIRE(hole->placements().size() == 1);
  const auto position = std::get<Point2>(hole->placements().front().source);
  CHECK(position.x == -15);
  CHECK(position.y == 0);
  CHECK(std::holds_alternative<HoleThroughAllExtent>(hole->extent()));
  CHECK(converted.find_sketch(SketchId("legacy.sketch")));
  CHECK(converted.parameters().size() == part.parameters().size());
  CHECK(order(converted) == order(part));
  CHECK(bool(converted.authored_history_intent()) == authored);
  CHECK(checked(serialize_part_document_to_json(checked(deserialize_part_document_from_json(
            checked(serialize_part_document_to_json(converted)))))) ==
        checked(serialize_part_document_to_json(converted)));
}

TEST_CASE(
    "Legacy conversion Save and Cancel retain committed intent and Apply publishes one exact step",
    "[gui][parity-b157][hole-conversion-shell]") {
  const bool origin = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, legacy_cut(false, true, origin));
  const auto before = snapshot(w);
  const double material = 38400 - 18 * std::numbers::pi;
  CHECK(volume(w) == Catch::Approx(material).margin(1e-6));
  (void)checked(w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy")));
  REQUIRE(w.session().part_workflow_active());
  CHECK(w.session().part_document()->find_hole_feature(FeatureId("legacy")));
  CHECK(volume(w) == Catch::Approx(material).margin(1e-6));
  const auto path = dir.filePath("conversion-save.blcad.json");
  REQUIRE(w.save_to(path));
  GuiDocumentSession committed;
  REQUIRE(committed.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*committed.part_document())) == before);
  w.feature_edit_tools().cancel();
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().dirty());
  (void)checked(w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy")));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK_FALSE(w.session().part_document()->find_feature(FeatureId("legacy")));
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  CHECK(w.session().part_document()->find_sketch(SketchId("legacy.sketch")));
  replay(w, dir, before);
}

TEST_CASE(
    "Converted legacy sources retain downstream Body Pattern mapping and subsequent placement",
    "[gui][parity-b157][hole-conversion-shell]") {
  const bool posed = GENERATE(false, true);
  auto part = legacy_cut(false, true, true);
  legacy_pattern(part, posed);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  CHECK(volume(w) == Catch::Approx(38400 - 18 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.pattern") ==
        Catch::Approx(3 * (38400 - 18 * std::numbers::pi)).margin(1e-6));
  (void)checked(w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy")));
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(order(*w.session().part_document()) == order(part));
  CHECK(volume(w) == Catch::Approx(38400 - 18 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.pattern") ==
        Catch::Approx(3 * (38400 - 18 * std::numbers::pi)).margin(1e-6));
  const auto root = nlohmann::json::parse(snapshot(w));
  CHECK(root.at("part_patterns") == nlohmann::json::parse(before).at("part_patterns"));
  replay(w, dir, before);
  const auto converted = snapshot(w);
  begin(w, "legacy");
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.pattern") ==
        Catch::Approx(3 * (38400 - 32 * std::numbers::pi)).margin(1e-6));
  replay(w, dir, converted);
}

TEST_CASE(
    "Unsupported associative legacy profiles report a diagnostic and remain ordinarily editable",
    "[gui][parity-b157][hole-conversion-shell]") {
  auto part = legacy_cut(true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  const auto revision = w.session().model_revision();
  const auto result = w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy"));
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("radius/count") != std::string::npos);
  CHECK(snapshot(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK_FALSE(w.feature_edit_tools().active());
  history_edit(w, "legacy");
  rename(w);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->find_feature(FeatureId("legacy")));
  CHECK(w.session().part_document()->hole_features().empty());
  replay(w, dir, before);
}

TEST_CASE(
    "Unsupported legacy target-feature consumers reject conversion without losing their edit path",
    "[gui][parity-b157][hole-conversion-shell]") {
  auto part = legacy_cut();
  auto sketch =
      checked(Sketch::create(SketchId("later.circle"), "Later cut", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("later.circle"), ParameterId("diameter"), {15, 0}))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  auto later = checked(Feature::create_subtractive_extrude(
      FeatureId("later.cut"), "Later cut", SketchId("later.circle"), FeatureId("legacy")));
  REQUIRE(part.add_feature(checked(later.with_body_result_context(holes::context()))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  const auto revision = w.session().model_revision();
  const auto result = w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy"));
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("downstream mapping") != std::string::npos);
  CHECK(snapshot(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(volume(w) == Catch::Approx(38400 - 36 * std::numbers::pi).margin(1e-6));
  history_edit(w, "legacy");
  rename(w);
  REQUIRE(w.feature_edit_tools().apply());
  replay(w, dir, before);
}

TEST_CASE("Legacy full-result feature Patterns reject a conversion that changes material semantics",
          "[gui][parity-b157][hole-conversion-shell]") {
  const bool explicit_placements = GENERATE(false, true);
  auto part = legacy_cut(false, true, true);
  legacy_pattern(part, false, true, explicit_placements);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  const auto revision = w.session().model_revision();
  CHECK(volume(w) == Catch::Approx(12 * 60 * 8).margin(1e-6));
  const auto result = w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy"));
  REQUIRE_FALSE(result);
  INFO(result.error().message());
  CHECK(result.error().message().find(explicit_placements
                                          ? "changes the current material"
                                          : "downstream mapping") != std::string::npos);
  CHECK(snapshot(w) == before);
  CHECK(w.session().model_revision() == revision);
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK_FALSE(w.feature_edit_tools().active());
}

TEST_CASE("Cancelling a conversion restores its original selection and scoped viewport context",
          "[gui][parity-b157][hole-conversion-shell][native-hole-conversion-context]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, legacy_cut());
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.base"}));
  REQUIRE(w.viewport().select_semantic("body/body.base"));
  w.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/body.base"});
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  const auto before = snapshot(w);
  const auto undo = std::string(w.session().undo_label());
  (void)checked(w.feature_edit_tools().begin_legacy_hole_conversion(FeatureId("legacy")));
  if (QApplication::platformName() == "xcb") {
    activate(w);
    click(control<QToolButton>(w, "blcad.hole.support.pick"));
  } else {
    control<QToolButton>(w, "blcad.hole.support.pick").click();
    settle();
  }
  CHECK(w.viewport().selection_semantic_scope() != scope);
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK(w.session().undo_label() == undo);
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  REQUIRE(w.session().selection().entries().size() == 1);
  CHECK(w.session().selection().entries().front().semantic_id == "body/body.base");
  REQUIRE(w.viewport().selected_semantic());
  CHECK(w.viewport().selected_semantic()->semantic_id == "body/body.base");
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.session().part_workflow_active());
}

TEST_CASE("Native Convert to Hole exposes explicit semantic preview and one corrected publication",
          "[gui][parity-b157][hole-conversion-shell][native-hole-conversion]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native conversion action and keyboard input");
  auto part = legacy_cut(false, true, true);
  legacy_pattern(part, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  activate(w);
  history_edit(w, "legacy", true);
  REQUIRE(w.session().part_workflow_active());
  auto& diameter = field<QDoubleSpinBox>(w, "/diameter/parameter");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&diameter);
  diameter.setFocus();
  diameter.selectAll();
  REQUIRE(diameter.hasFocus());
  key(diameter, Qt::Key_4, "4");
  key(diameter, Qt::Key_Return);
  CHECK(diameter.value() == 4);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-conversion-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.pattern") ==
        Catch::Approx(3 * (38400 - 32 * std::numbers::pi)).margin(1e-6));
  const auto* hole = w.session().part_document()->find_hole_feature(FeatureId("legacy"));
  REQUIRE(hole);
  CHECK(hole->placements().front().id == HolePlacementId("legacy.position"));
  CHECK(w.session().part_document()->find_sketch(SketchId("legacy.sketch")));
  replay(w, dir, before);
}

TEST_CASE(
    "Converting a pinned Thread owns fresh profile dimensions and retains shared interval bindings",
    "[gui][parity-b157][thread-source-shell]") {
  auto part = threads::pair_part();
  const auto definition =
      checked(ThreadDefinition::create(threads::selected(), ParameterId("thread.minor")));
  REQUIRE(part.add_thread_feature(threads::external(definition)));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  begin(w, "external");
  thread_choice(w, "blcad.thread.source", "custom");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter").value() == 8);
  CHECK(field<QDoubleSpinBox>(w, "/thread/length").value() == 8);
  field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter").setValue(7);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& thread = result.thread_features().front();
  const auto& profile = std::get<ThreadParameterProfile>(thread.thread().profile());
  CHECK(thread.id() == FeatureId("external"));
  CHECK(thread.thread().length() == ParameterId("thread.minor"));
  CHECK(profile.minor_diameter != ParameterId("thread.minor"));
  CHECK(profile.major_diameter != ParameterId("thread.major"));
  CHECK(result.find_parameter(ParameterId("thread.minor"))->value().millimeters() == 8);
  CHECK(result.find_parameter(profile.minor_diameter)->value().millimeters() == 7);
  CHECK(result.parameters().size() == part.parameters().size() + 4);
  CHECK(result.engineering_resources() == part.engineering_resources());
  replay(w, dir, before);
}

TEST_CASE(
    "Internal Thread standard mapping retains its pilot and publishes only used dimensional inputs",
    "[gui][parity-b157][thread-source-shell]") {
  auto part = plate();
  REQUIRE(part.add_engineering_resource(threads::table()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(8);
  choose_hole_mode(w, "blcad.hole.internal_thread", 1);
  thread_mapping(w);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& hole = result.hole_features().front();
  REQUIRE(hole.thread());
  CHECK(std::holds_alternative<ThreadStandardProfile>(hole.thread()->profile()));
  CHECK(result.parameters().size() == part.parameters().size() + 2);
  CHECK(volume(w) == Catch::Approx(38400 - 128 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Optional Thread offset and taper can discard invalid unused values without publishing inputs",
    "[gui][parity-b157][thread-source-shell]") {
  const bool internal = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = internal ? plate() : threads::pair_part();
  open(w, dir, part);
  const auto before = snapshot(w);
  if (internal) {
    create_hole(w);
    choose_hole_mode(w, "blcad.hole.internal_thread", 1);
  } else
    create_thread(w);
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(true);
  settle();
  CHECK(field<QDoubleSpinBox>(w, "/thread/start").value() == 1);
  field<QDoubleSpinBox>(w, "/thread/start").setValue(0);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(false);
  settle();
  CHECK(w.feature_edit_tools().field("/thread/start") == nullptr);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QCheckBox>(w, "blcad.thread.taper.enabled").setChecked(true);
  settle();
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").value() == 1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").setValue(0);
  control<QCheckBox>(w, "blcad.thread.taper.enabled").setChecked(false);
  settle();
  CHECK(w.feature_edit_tools().field("/thread/profile/taper_angle") == nullptr);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->parameters().size() ==
        part.parameters().size() + (internal ? 6 : 5));
  CHECK(snapshot(w) != before);
  replay(w, dir, before);
}

TEST_CASE("Native standard Thread mapping retains focus through explicit typed selections and "
          "modeled Apply",
          "[gui][parity-b157][thread-source-shell][native-thread-source]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native standard Thread authoring");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  const auto before = snapshot(w);
  create_thread(w);
  activate(w);
  auto& invalid_length = field<QDoubleSpinBox>(w, "/thread/length");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&invalid_length);
  invalid_length.setFocus();
  REQUIRE(invalid_length.hasFocus());
  invalid_length.selectAll();
  key(invalid_length, Qt::Key_0, "0");
  key(invalid_length, Qt::Key_Return);
  thread_mapping(w, "r1", true);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& corrected_length = field<QDoubleSpinBox>(w, "/thread/length");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
      .ensureWidgetVisible(&corrected_length);
  corrected_length.setFocus();
  REQUIRE(corrected_length.hasFocus());
  corrected_length.selectAll();
  key(corrected_length, Qt::Key_4, "4");
  key(corrected_length, Qt::Key_Return);
  REQUIRE(field<QDoubleSpinBox>(w, "/thread/length").hasFocus());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  click(control<QCheckBox>(w, "blcad.thread.start.enabled"));
  auto& length = field<QDoubleSpinBox>(w, "/thread/length");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&length);
  length.setFocus();
  length.selectAll();
  key(length, Qt::Key_3, "3");
  key(length, Qt::Key_Return);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&mode);
  mode.setFocus();
  key(mode, Qt::Key_Down);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
            .horizontalScrollBar()
            ->maximum() == 0);
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
      .ensureWidgetVisible(&control<QComboBox>(w, "blcad.thread.standard.table"));
  settle();
  capture(w, "thread-standard-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const double removed =
      4 * std::numbers::pi * std::tan(55 * std::numbers::pi / 360) * 3 / 1.7 * (4. / 2 + 1. / 3);
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi - removed).margin(2e-4));
  const auto& thread = w.session().part_document()->thread_features().front();
  REQUIRE(thread.thread().start());
  CHECK(w.session()
            .part_document()
            ->find_parameter(*thread.thread().start())
            ->value()
            .millimeters() == 1);
  replay(w, dir, before);
}

TEST_CASE("A mapped taper retains its signed Angle and explicit custom removal restores a "
          "cylindrical Thread",
          "[gui][parity-b157][thread-source-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = threads::pair_part();
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  thread_mapping(w);
  control<QCheckBox>(w, "blcad.thread.standard.taper.enabled").setChecked(true);
  settle();
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& columns = control<QComboBox>(w, "blcad.thread.standard.taper_angle");
  CHECK(columns.findData("major") == -1);
  thread_choice(w, "blcad.thread.standard.taper_angle", "taper");
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  thread_choice(w, "blcad.thread.source", "custom");
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").value() == -2);
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").suffix().contains("deg"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QCheckBox>(w, "blcad.thread.taper.enabled").setChecked(false);
  settle();
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& profile =
      std::get<ThreadParameterProfile>(result.thread_features().front().thread().profile());
  CHECK_FALSE(profile.taper_angle);
  CHECK(result.parameters().size() == part.parameters().size() + 5);
  CHECK(result.engineering_resources() == part.engineering_resources());
  replay(w, dir, before);
}

TEST_CASE("Thread source rebuilds preserve an active support selector and its scoped targets",
          "[gui][parity-b157][thread-source-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  const auto before = snapshot(w);
  const auto entry_scope = w.viewport().selection_semantic_scope();
  create_thread(w);
  control<QToolButton>(w, "blcad.thread.support.pick").click();
  const auto scoped = w.viewport().selection_semantic_scope();
  REQUIRE(scoped);
  REQUIRE_FALSE(scoped->empty());
  thread_choice(w, "blcad.thread.source", "standard");
  CHECK(control<QToolButton>(w, "blcad.thread.support.pick").isChecked());
  CHECK(w.viewport().selection_semantic_scope() == scoped);
  REQUIRE(w.feature_edit_tools().accept_selection({GuiSelectionKind::Face, scoped->front()}));
  CHECK_FALSE(control<QToolButton>(w, "blcad.thread.support.pick").isChecked());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_choice(w, "blcad.thread.source", "custom");
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.feature_edit_tools().cancel();
  CHECK(w.viewport().selection_semantic_scope() == entry_scope);
  CHECK(snapshot(w) == before);
}

TEST_CASE("Editing a Hole support uses its predecessor and preserves its owned placement identity",
          "[gui][parity-b157][hole-support-edit-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = holes::part(false, false);
  open(w, dir, part);
  const auto before = snapshot(w);
  begin(w, "holes");
  auto& supports = field<QComboBox>(w, "/support");
  for (int i = 0; i < supports.count(); ++i)
    CHECK_FALSE(supports.itemData(i).toString().contains("hole/5:first/bottom"));
  const auto bottom = supports.findData("finishing/face/4:base/bottom");
  REQUIRE(bottom >= 0);
  supports.setCurrentIndex(bottom);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& hole = result.hole_features().front();
  CHECK(hole.id() == FeatureId("holes"));
  CHECK(hole.placements().front().id == HolePlacementId("first"));
  CHECK(std::get<SemanticFaceReference>(hole.support().source()).face() == SemanticFace::Bottom);
  CHECK(result.parameters().size() == part.parameters().size());
  CHECK(result.result_face_workplane_count() == 0);
  CHECK(volume(w) == Catch::Approx(38400 - 13.5 * std::numbers::pi).margin(1e-6));
  const auto axis = checked(
      HoleInstanceReference::create(hole.id(), HolePlacementId("first"), HoleReferenceRole::Axis));
  const auto frame = checked(
      geometry::HoleInstanceGeometry::resolve(result, axis, *w.session().part_shape_cache()));
  CHECK(frame.frame.origin.x == Catch::Approx(-20).margin(1e-7));
  CHECK(frame.frame.origin.y == Catch::Approx(-15).margin(1e-7));
  CHECK(frame.frame.origin.z == Catch::Approx(0).margin(1e-7));
  CHECK(frame.frame.normal.z == Catch::Approx(1).margin(1e-7));
  replay(w, dir, before);
}

TEST_CASE("Existing Thread support editing retains its original reference until an explicit rebind",
          "[gui][parity-b157][hole-support-edit-shell]") {
  const bool rebind = GENERATE(false, true);
  auto part = threads::pair_part();
  REQUIRE(part.add_thread_feature(threads::external()));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  begin(w, "external");
  if (rebind) {
    auto& supports = field<QComboBox>(w, "/support");
    int choice = -1;
    for (int i = 0; i < supports.count(); ++i)
      if (supports.itemData(i).toString() != "feature-input/current-thread-support") {
        choice = i;
        break;
      }
    REQUIRE(choice >= 0);
    supports.setCurrentIndex(choice);
  } else
    rename(w);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& thread = result.thread_features().front();
  CHECK(thread.id() == FeatureId("external"));
  if (rebind) {
    REQUIRE(std::holds_alternative<ResultFaceReference>(thread.support().source()));
    CHECK(std::get<ResultFaceReference>(thread.support().source()).producer() ==
          FeatureId("shaft"));
  } else
    CHECK(nlohmann::json::parse(snapshot(w))["thread_features"][0]["support"] ==
          nlohmann::json::parse(before)["thread_features"][0]["support"]);
  CHECK(result.parameters().size() == part.parameters().size());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Existing support selectors exclude Bodies introduced after the edited feature",
          "[gui][parity-b157][hole-support-edit-shell]") {
  auto part = holes::part(false, false);
  const auto sketch = part.features().front().input_sketch();
  const auto context = blcad::test::history_fixture::new_body(part, "later");
  auto feature = checked(Feature::create_additive_extrude(FeatureId("later"), "Later Body", sketch,
                                                          ParameterId("depth")));
  REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
  REQUIRE(part.apply_history_intent({{"base", "holes", "later"}, {}}));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  begin(w, "holes");
  const auto& targets = field<QComboBox>(w, "/target_body");
  CHECK(targets.findData("body.base") >= 0);
  CHECK(targets.findData("body.later") == -1);
  w.feature_edit_tools().cancel();
}

TEST_CASE("Editing a Hole onto an earlier result support owns only its used pending workplane",
          "[gui][parity-b157][hole-support-edit-shell]") {
  auto part = holes::part(false, false);
  REQUIRE(part.add_parameter(checked(Parameter::create_length(
      ParameterId("second.diameter"), "Second diameter", checked(Quantity::length_mm(1))))));
  const auto& original = part.hole_features().front();
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      FeatureId("second"), "Second Hole", HoleKind::Plain, original.target_body(),
      original.support(), {{HolePlacementId("second.point"), Point2{20, 15}}},
      ParameterId("second.diameter")))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  begin(w, "second");
  const auto placement = checked(HoleInstanceReference::create(
      FeatureId("holes"), HolePlacementId("first"), HoleReferenceRole::Axis));
  const auto reference = checked(ResultFaceReference::create(
      FeatureId("holes"), BodyId("body.base"), {placement.origin("bottom")}));
  auto& supports = field<QComboBox>(w, "/support");
  const auto choice =
      supports.findData(QString::fromStdString("finishing/" + reference.identity()));
  REQUIRE(choice >= 0);
  supports.setCurrentIndex(choice);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(0);
  field<QDoubleSpinBox>(w, "/placements/0/position/y").setValue(0);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  CHECK(w.session().part_document()->result_face_workplane_count() == 0);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  CHECK(result.result_face_workplane_count() == 1);
  const auto* second = result.find_hole_feature(FeatureId("second"));
  REQUIRE(second);
  CHECK(second->placements().front().id == HolePlacementId("second.point"));
  const auto& plane = std::get<DatumPlaneId>(second->support().source());
  REQUIRE(result.find_result_face_workplane(plane));
  CHECK(result.find_result_face_workplane(plane)->reference().producer() == FeatureId("holes"));
  CHECK(volume(w) == Catch::Approx(38400 - 14 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Native existing Hole support picks use predecessor geometry and restore context on Cancel",
    "[gui][parity-b157][hole-support-edit-shell][native-hole-support-edit]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native pre-feature input picking");
  const bool apply = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, holes::part(false, false));
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.base"}));
  REQUIRE(w.viewport().select_semantic("body/body.base"));
  const auto before = snapshot(w);
  const auto camera = w.viewport().camera_bookmark();
  const auto scope = w.viewport().selection_semantic_scope();
  begin(w, "holes");
  w.viewport().set_standard_view(GuiStandardView::Bottom);
  settle();
  click(control<QToolButton>(w, "blcad.hole.support.pick"));
  const auto point = checked(w.viewport().model_to_screen({-20, -15, 0}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK(field<QComboBox>(w, "/support").currentData() == "finishing/face/4:base/bottom");
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-support-edit-preview");
  if (apply) {
    click(control<QPushButton>(w, "blcad.feature_edit.apply"));
    CHECK(std::get<SemanticFaceReference>(
              w.session().part_document()->hole_features().front().support().source())
              .face() == SemanticFace::Bottom);
    replay(w, dir, before);
  } else {
    click(control<QPushButton>(w, "blcad.feature_edit.cancel"));
    CHECK(snapshot(w) == before);
    CHECK(w.viewport().selection_semantic_scope() == scope);
    REQUIRE(w.session().selection().entries().size() == 1);
    CHECK(w.session().selection().entries().front().semantic_id == "body/body.base");
    const auto restored = w.viewport().camera_bookmark();
    CHECK(restored.eye.x == Catch::Approx(camera.eye.x).margin(1e-7));
    CHECK(restored.eye.y == Catch::Approx(camera.eye.y).margin(1e-7));
    CHECK(restored.eye.z == Catch::Approx(camera.eye.z).margin(1e-7));
  }
  CHECK(w.viewport().preview_selection_target_count() == 0);
}

TEST_CASE("Hole limit faces remain explicitly selectable through invalid and cleared candidates",
          "[gui][parity-b157][hole-limit-shell]") {
  const bool editing = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, editing ? holes::part(false, false) : plate());
  const auto before = snapshot(w);
  if (editing)
    begin(w, "holes");
  else
    create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.extent", 3);
  auto& limits = field<QComboBox>(w, "/extent/face");
  const int top = limits.findData("finishing/face/4:base/top");
  const int bottom = limits.findData("finishing/face/4:base/bottom");
  REQUIRE(top >= 0);
  REQUIRE(bottom >= 0);
  limits.setCurrentIndex(top);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  limits.setCurrentIndex(-1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QLabel>(w, "blcad.feature_edit.status").text().contains("Choose a current"));
  limits.setCurrentIndex(bottom);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& part = *w.session().part_document();
  const auto& hole = part.hole_features().front();
  const auto& face = std::get<HoleToFaceExtent>(hole.extent()).face;
  CHECK(face.role() == PartFeatureInputRole::HoleLimitFace);
  CHECK(std::get<SemanticFaceReference>(face.source()).face() == SemanticFace::Bottom);
  CHECK(part.result_face_workplane_count() == 0);
  CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Imported Hole limits retain typed face identity placement and supplier guards",
          "[gui][parity-b157][hole-limit-shell]") {
  const bool posed = GENERATE(false, true);
  const bool stale = GENERATE(false, true);
  QTemporaryDir dir;
  auto part = imported_plate(dir, posed);
  const auto body = part.bodies().front().id().value();
  const auto bottom = imported_bottom(part);
  ShellWindow w;
  open(w, dir, part);
  const auto before = snapshot(w);
  REQUIRE(w.feature_edit_tools().begin_new_hole());
  choose_support(w, imported_top(part));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  choose_hole_mode(w, "blcad.hole.extent", 3);
  auto& limits = field<QComboBox>(w, "/extent/face");
  const int index = limits.findData(QString::fromStdString(bottom));
  REQUIRE(index >= 0);
  limits.setCurrentIndex(index);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  if (stale) {
    QFile file(dir.filePath("supplier.step"));
    REQUIRE(file.open(QIODevice::Append));
    REQUIRE(file.write("\n") == 1);
    file.close();
    CHECK_FALSE(w.feature_edit_tools().apply());
    CHECK(snapshot(w) == before);
    w.feature_edit_tools().cancel();
    return;
  }
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& face = std::get<HoleToFaceExtent>(result.hole_features().front().extent()).face;
  CHECK(face.role() == PartFeatureInputRole::HoleLimitFace);
  CHECK(std::get<ImportedTopologyReference>(face.source()).capability() ==
        ImportedTopologyCapability::Face);
  CHECK(result.result_face_workplane_count() == 0);
  CHECK(volume(w, body.c_str()) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A Hole can terminate at an earlier result face without creating a support workplane",
          "[gui][parity-b157][hole-limit-shell]") {
  auto part = holes::part(false, false);
  const auto context = blcad::test::history_fixture::new_body(part, "cover");
  auto cover = checked(Feature::create_additive_extrude(
      FeatureId("cover"), "Cover", part.features().front().input_sketch(), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(cover.with_body_result_context(context))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_hole(w);
  auto& targets = field<QComboBox>(w, "/target_body");
  targets.setCurrentIndex(targets.findData("body.cover"));
  settle();
  choose_support(w, "finishing/face/5:cover/top");
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(1);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-20);
  field<QDoubleSpinBox>(w, "/placements/0/position/y").setValue(-15);
  choose_hole_mode(w, "blcad.hole.extent", 3);
  const auto placement = checked(HoleInstanceReference::create(
      FeatureId("holes"), HolePlacementId("first"), HoleReferenceRole::Axis));
  const auto reference = checked(ResultFaceReference::create(
      FeatureId("holes"), BodyId("body.base"), {placement.origin("bottom")}));
  auto& limits = field<QComboBox>(w, "/extent/face");
  const int index = limits.findData(QString::fromStdString("finishing/" + reference.identity()));
  REQUIRE(index >= 0);
  limits.setCurrentIndex(index);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& face = std::get<HoleToFaceExtent>(result.hole_features().back().extent()).face;
  CHECK(std::get<ResultFaceReference>(face.source()).identity() == reference.identity());
  CHECK(result.result_face_workplane_count() == 0);
  CHECK(volume(w, "body.cover") == Catch::Approx(38400 - 1.5 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w) == Catch::Approx(38400 - 13.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Up to Body selectors exclude the target and replace an invalid same-body limit",
          "[gui][parity-b157][hole-limit-shell]") {
  const bool retarget = GENERATE(false, true);
  auto part = plate();
  for (const auto& [id, z] : {std::pair{"limit1", -4.0}, std::pair{"limit2", -2.0}}) {
    const std::string name(id);
    REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId(name + ".plane"), name, {0, 0, z}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
    blcad::test::history_fixture::rectangle(part, (name + ".profile").c_str(),
                                            (name + ".plane").c_str());
    auto feature = checked(Feature::create_additive_extrude(
        FeatureId(name), name, SketchId(name + ".profile"), ParameterId("depth")));
    REQUIRE(part.add_feature(checked(
        feature.with_body_result_context(blcad::test::history_fixture::new_body(part, id)))));
  }
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.extent", 4);
  auto& limits = field<QComboBox>(w, "/extent/body");
  CHECK(limits.findData("body.base") == -1);
  REQUIRE(limits.findData("body.limit1") >= 0);
  REQUIRE(limits.findData("body.limit2") >= 0);
  limits.setCurrentIndex(-1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QToolButton>(w, "blcad.hole.limit_body.pick").click();
  CHECK_FALSE(
      w.feature_edit_tools().accept_selection({GuiSelectionKind::Face, "body/body.limit2"}));
  REQUIRE(w.feature_edit_tools().accept_selection({GuiSelectionKind::Body, "body/body.limit2"}));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  if (retarget) {
    auto& target = field<QComboBox>(w, "/target_body");
    target.setCurrentIndex(target.findData("body.limit2"));
    settle();
    choose_support(w, "finishing/face/6:limit2/top");
    CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
    auto& replacement = field<QComboBox>(w, "/extent/body");
    CHECK(replacement.findData("body.limit2") == -1);
    replacement.setCurrentIndex(replacement.findData("body.limit1"));
    INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
    REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  }
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(std::get<HoleUpToBodyExtent>(w.session().part_document()->hole_features().front().extent())
            .body == BodyId(retarget ? "body.limit1" : "body.limit2"));
  CHECK(volume(w) == Catch::Approx(38400 - (retarget ? 0 : 2 * std::numbers::pi)).margin(1e-6));
  CHECK(volume(w, "body.limit1") == Catch::Approx(38400).margin(1e-6));
  CHECK(volume(w, "body.limit2") ==
        Catch::Approx(38400 - (retarget ? 2 * std::numbers::pi : 0)).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native Hole limit picks bind the selected planar face and recover invalid geometry",
          "[gui][parity-b157][hole-limit-shell][native-hole-limit]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native limit-face picks");
  const bool imported = GENERATE(false, true);
  QTemporaryDir dir;
  auto part = imported ? imported_plate(dir, true) : plate();
  const auto body = part.bodies().front().id().value();
  ShellWindow w;
  open(w, dir, part);
  const auto before = snapshot(w);
  if (imported) {
    REQUIRE(w.feature_edit_tools().begin_new_hole());
    choose_support(w, imported_top(part));
  } else
    create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  choose_hole_mode(w, "blcad.hole.extent", 3);
  w.viewport().set_standard_view(GuiStandardView::Top);
  settle();
  click(control<QToolButton>(w, "blcad.hole.limit_face.pick"));
  auto top =
      checked(w.viewport().model_to_screen(imported ? Point3{20, 1, 18} : Point3{10, 10, 8}));
  click_at(w.viewport(), {top.x, top.y});
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.viewport().set_standard_view(GuiStandardView::Bottom);
  settle();
  click(control<QToolButton>(w, "blcad.hole.limit_face.pick"));
  auto bottom =
      checked(w.viewport().model_to_screen(imported ? Point3{20, 1, 10} : Point3{10, 10, 0}));
  click_at(w.viewport(), {bottom.x, bottom.y});
  CHECK(field<QComboBox>(w, "/extent/face").currentData() ==
        QString::fromStdString(imported ? imported_bottom(part) : "finishing/face/4:base/bottom"));
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, imported ? "hole-imported-limit-preview" : "hole-native-limit-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(volume(w, body.c_str()) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  CHECK(w.viewport().preview_selection_target_count() == 0);
  replay(w, dir, before);
}

TEST_CASE("Conical Thread creation seeds a finite positive interval from either apex orientation",
          "[gui][parity-b157][thread-cone-shell]") {
  const bool apex_first = GENERATE(false, true);
  const bool imported = GENERATE(false, true);
  auto part = conical_part(apex_first ? 0 : 5, apex_first ? 5 : 0);
  QTemporaryDir dir;
  if (imported) {
    auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("cone.supplier")));
    REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
    part = imported_shape(dir, *cache.find_body_shape(BodyId("body.cone")), true);
  }
  const auto body = part.bodies().front().id().value();
  ShellWindow w;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/major_diameter").value() == Catch::Approx(10));
  CHECK(field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter").value() == Catch::Approx(9.5));
  CHECK(field<QDoubleSpinBox>(w, "/thread/length").value() == Catch::Approx(.95).margin(1e-8));
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& definition = result.thread_features().front().thread();
  const auto value = checked(resolve_thread_definition(result, definition));
  CHECK(value.minor_diameter_mm +
            2 * std::tan(value.taper_angle_deg * std::numbers::pi / 180) * value.length_mm >
        0);
  CHECK(value.major_diameter_mm == Catch::Approx(10).margin(1e-7));
  CHECK(result.parameters().size() == part.parameters().size() + 6);
  CHECK(volume(w, body.c_str()) == Catch::Approx(50 * std::numbers::pi / 3).margin(1e-6));
  REQUIRE(checked(geometry::ThreadAdapter::cosmetic_presentations(
                      *w.session().part_shape_cache()->find_body_shape(BodyId(body))))
              .size() == 1);
  replay(w, dir, before);
}

TEST_CASE("A conical standard profile converts to owned start and opposite-direction inputs",
          "[gui][parity-b157][thread-cone-shell]") {
  const double slope = std::tan(-2 * std::numbers::pi / 180);
  auto part = conical_part(5, 5 + 8 * slope, 8);
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("cone.profile.source")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto faces = checked(geometry::FinishingSelectionBuilder::build(
      part, cache, BodyId("body.cone"), PartFeatureInputRole::ShellRemovalFace));
  std::optional<geometry::ThreadSupportInspection> support;
  for (const auto& face : faces)
    if (const auto* ref = std::get_if<FaceReference>(&face.reference))
      if (const auto* result = std::get_if<ResultFaceReference>(&ref->source())) {
        auto selected =
            checked(FaceReference::create_result(PartFeatureInputRole::ThreadSupportFace, *result));
        auto inspected =
            geometry::ThreadAdapter::inspect_support(part, BodyId("body.cone"), selected, cache);
        if (inspected) {
          support = inspected.value();
          break;
        }
      }
  REQUIRE(support);
  const auto original = threads::table();
  auto rows = original.rows();
  const double signed_slope = std::tan(support->taper_angle_deg * std::numbers::pi / 180);
  for (auto& cell : rows.front().cells) {
    if (cell.column == "major")
      cell.value = checked(Quantity::length_mm(2 * support->radius_mm));
    if (cell.column == "minor")
      cell.value = checked(Quantity::length_mm(2 * support->radius_mm - 1));
    if (cell.column == "taper")
      cell.value = checked(Quantity::angle_deg(support->taper_angle_deg));
  }
  REQUIRE(part.add_engineering_resource(checked(EngineeringStandardTable::create(
      original.key(), original.name(), original.provenance(), original.columns(), rows))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  thread_mapping(w);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QCheckBox>(w, "blcad.thread.standard.taper.enabled").setChecked(true);
  settle();
  thread_choice(w, "blcad.thread.standard.taper_angle", "taper");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  thread_choice(w, "blcad.thread.source", "custom");
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(true);
  settle();
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  field<QDoubleSpinBox>(w, "/thread/profile/major_diameter")
      .setValue(2 * (support->radius_mm + signed_slope));
  field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter")
      .setValue(2 * (support->radius_mm + signed_slope) - 1);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& direction = field<QComboBox>(w, "/direction");
  direction.setCurrentIndex(1 - direction.currentIndex());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").setValue(-support->taper_angle_deg);
  field<QDoubleSpinBox>(w, "/thread/profile/major_diameter")
      .setValue(2 * (support->radius_mm + 7 * signed_slope));
  field<QDoubleSpinBox>(w, "/thread/profile/minor_diameter")
      .setValue(2 * (support->radius_mm + 7 * signed_slope) - 1);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& definition = result.thread_features().front().thread();
  REQUIRE(std::holds_alternative<ThreadParameterProfile>(definition.profile()));
  const auto value = checked(resolve_thread_definition(result, definition));
  CHECK(value.start_mm == 1);
  CHECK(value.taper_angle_deg == Catch::Approx(-support->taper_angle_deg).margin(1e-8));
  CHECK(result.engineering_resources() == part.engineering_resources());
  CHECK(result.parameters().size() == 7);
  const double end_radius = 5 + 8 * slope;
  CHECK(volume(w, "body.cone") ==
        Catch::Approx(std::numbers::pi * 8 / 3 * (25 + 5 * end_radius + end_radius * end_radius))
            .margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native conical Thread controls correct offset direction and unsupported modeled intent",
          "[gui][parity-b157][thread-cone-shell][native-thread-cone]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native conical Thread authoring");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, conical_part(2, 5));
  const auto before = snapshot(w);
  create_thread(w);
  activate(w);
  const auto type_number = [&](const char* path, const QString& text) {
    auto& input = field<QDoubleSpinBox>(w, path);
    control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&input);
    input.setFocus();
    settle();
    REQUIRE(input.hasFocus());
    CHECK(input.decimals() == 9);
    input.selectAll();
    key(input, Qt::Key_0, text);
    key(input, Qt::Key_Return);
  };
  type_number("/thread/length", "0.5");
  click(control<QCheckBox>(w, "blcad.thread.start.enabled"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  type_number("/thread/profile/major_diameter", "7");
  type_number("/thread/profile/minor_diameter", "6.5");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  const double angle = field<QDoubleSpinBox>(w, "/thread/profile/taper_angle").value();
  auto& direction = field<QComboBox>(w, "/direction");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&direction);
  direction.setFocus();
  REQUIRE(direction.hasFocus());
  key(direction, direction.currentIndex() ? Qt::Key_Up : Qt::Key_Down);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  type_number("/thread/profile/taper_angle", QString::number(-angle, 'f', 9));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  auto& mode = field<QComboBox>(w, "/thread/representation");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&mode);
  mode.setFocus();
  REQUIRE(mode.hasFocus());
  key(mode, Qt::Key_Down);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QLabel>(w, "blcad.feature_edit.status").text().contains("modeled tapered"));
  key(mode, Qt::Key_Up);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "thread-conical-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const auto& part = *w.session().part_document();
  const auto value =
      checked(resolve_thread_definition(part, part.thread_features().front().thread()));
  CHECK(value.start_mm == 1);
  CHECK(value.length_mm == .5);
  CHECK(value.major_diameter_mm == 7);
  CHECK(value.minor_diameter_mm == 6.5);
  CHECK(volume(w, "body.cone") == Catch::Approx(26 * std::numbers::pi).margin(1e-6));
  const auto root = std::filesystem::path(dir.path().toStdString());
  REQUIRE(geometry::StepExporter{}.write_part_step(part, *w.session().part_shape_cache(),
                                                   root / "cone.step"));
  const auto asset = checked(inspect_step_import_asset(root, "cone.step"));
  const auto source = checked(
      StepImportSource::create(StepImportSourceId("cone.roundtrip"), "Cone roundtrip", "cone.step",
                               *asset.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  REQUIRE(imported.body_count() == 1);
  double total = 0;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      total += body.volume_mm3;
  CHECK(total == Catch::Approx(26 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Existing imported Hole and Thread inputs retain their placed predecessor scope",
          "[gui][parity-b157][hole-support-edit-shell][thread-cone-shell]") {
  const bool thread = GENERATE(false, true);
  QTemporaryDir dir;
  const auto part = thread ? imported_shaft(dir, true) : imported_plate(dir, true);
  const auto body = part.bodies().front().id().value();
  ShellWindow w;
  open(w, dir, part);
  if (thread)
    create_thread(w);
  else {
    REQUIRE(w.feature_edit_tools().begin_new_hole());
    choose_support(w, imported_top(part));
    field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  }
  REQUIRE(w.feature_edit_tools().apply());
  const auto before = snapshot(w);
  const auto& current = *w.session().part_document();
  const auto parameter_count = current.parameters().size();
  const auto id =
      thread ? current.thread_features().front().id() : current.hole_features().front().id();
  begin(w, id.value().c_str());
  auto& supports = field<QComboBox>(w, "/support");
  if (thread) {
    REQUIRE(supports.count() >= 2);
    supports.setCurrentIndex((supports.currentIndex() + 1) % supports.count());
  } else {
    const auto bottom = supports.findData(QString::fromStdString(imported_bottom(part)));
    REQUIRE(bottom >= 0);
    supports.setCurrentIndex(bottom);
  }
  rename(w);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  CHECK(result.parameters().size() == parameter_count);
  if (thread) {
    CHECK(result.thread_features().front().id() == id);
    CHECK(std::holds_alternative<ImportedTopologyReference>(
        result.thread_features().front().support().source()));
    CHECK(volume(w, body.c_str()) == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  } else {
    CHECK(result.hole_features().front().id() == id);
    CHECK(std::holds_alternative<ImportedTopologyReference>(
        result.hole_features().front().support().source()));
    CHECK(volume(w, body.c_str()) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  }
  CHECK(result.result_face_workplane_count() == 0);
  replay(w, dir, before);
}

TEST_CASE("Correcting a used Thread length retries a complete rejected profile mapping",
          "[gui][parity-b157][thread-source-shell][thread-cone-shell]") {
  const bool shared = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = threads::pair_part();
  open(w, dir, part);
  const auto before = snapshot(w);
  create_thread(w);
  if (shared) {
    auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/thread/length");
    binding.setCurrentIndex(binding.findData("thread.length"));
  }
  field<QDoubleSpinBox>(w, "/thread/length").setValue(0);
  thread_mapping(w);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QComboBox>(w, "blcad.thread.standard.pitch").currentData() == "pitch");
  CHECK(snapshot(w) == before);
  field<QDoubleSpinBox>(w, "/thread/length").setValue(3);
  settle();
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QComboBox>(w, "blcad.thread.standard.pitch").currentData() == "pitch");
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& definition = result.thread_features().front().thread();
  CHECK(std::holds_alternative<ThreadStandardProfile>(definition.profile()));
  CHECK(checked(resolve_thread_definition(result, definition)).length_mm == 3);
  if (shared)
    CHECK(definition.length() == ParameterId("thread.length"));
  CHECK(result.parameters().size() == part.parameters().size() + (shared ? 0 : 1));
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native Up to Body picks exclude the target and bind the actual separate limit solid",
          "[gui][parity-b157][hole-limit-shell][native-hole-body-limit]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native limit-Body picking");
  auto part = plate();
  REQUIRE(part.add_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("limit.plane"), "Limit plane",
                                                 {20, 0, -4}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  blcad::test::history_fixture::rectangle(part, "limit.profile", "limit.plane");
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("limit"), "Limit", SketchId("limit.profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(
      feature.with_body_result_context(blcad::test::history_fixture::new_body(part, "limit")))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.extent", 4);
  field<QComboBox>(w, "/extent/body").setCurrentIndex(-1);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.viewport().set_standard_view(GuiStandardView::Top);
  settle();
  click(control<QToolButton>(w, "blcad.hole.limit_body.pick"));
  const auto scope = w.viewport().selection_semantic_scope();
  REQUIRE(scope);
  CHECK(std::find(scope->begin(), scope->end(), "body/body.base") == scope->end());
  CHECK(std::find(scope->begin(), scope->end(), "body/body.limit") != scope->end());
  const auto point = checked(w.viewport().model_to_screen({50, 0, 4}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK(field<QComboBox>(w, "/extent/body").currentData() == "body.limit");
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-body-limit-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(volume(w) == Catch::Approx(38400 - 4 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.limit") == Catch::Approx(38400).margin(1e-6));
  CHECK(std::get<HoleUpToBodyExtent>(w.session().part_document()->hole_features().front().extent())
            .body == BodyId("body.limit"));
  CHECK(w.viewport().preview_selection_target_count() == 0);
  replay(w, dir, before);
}

TEST_CASE(
    "Create Another retains every Hole recipe with fresh owned inputs and placement identities",
    "[gui][parity-b157][hole-repeat-shell]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  CAPTURE(mode);
  auto part = manufacture::seed();
  REQUIRE(part.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(80))));
  REQUIRE(part.set_parameter_value(ParameterId("height"), checked(Quantity::length_mm(60))));
  if (mode == 8) {
    REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId("slot.second.plane"), "Second slot plane", {20, 0, 8}, {1, 0, 0},
        {0, 1, 0}, {0, 0, 1}))));
    auto second = checked(
        Sketch::create(SketchId("slot.second"), "Second slot", DatumPlaneId("slot.second.plane")));
    const auto original = *part.find_sketch(SketchId("slot"));
    for (const auto& line : original.line_segments())
      REQUIRE(second.add_entity(line));
    for (const auto& arc : original.arc_segments())
      REQUIRE(second.add_entity(arc));
    REQUIRE(second.add_profile(original.arc_closed_profiles().front()));
    REQUIRE(part.add_sketch(std::move(second)));
  }
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  if (mode != 8)
    field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  choose_hole_mode(w, "blcad.hole.recipe", mode);
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  const auto first = w.session().part_document()->hole_features().front();
  CHECK(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == mode);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  if (mode == 8) {
    auto& sketch = field<QComboBox>(w, "/manufacturing/sketch");
    const int index = sketch.findData("slot.second");
    REQUIRE(index >= 0);
    sketch.setCurrentIndex(index);
  } else
    field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(15);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  REQUIRE(result.hole_features().size() == 2);
  const auto& next = result.hole_features().back();
  CHECK(next.id() != first.id());
  CHECK(next.placements().front().id != first.placements().front().id);
  CHECK(next.kind() == first.kind());
  CHECK(next.manufacturing().index() == first.manufacturing().index());
  CHECK(next.extent().index() == first.extent().index());
  const auto previous_inputs = first.parameter_dependencies();
  const auto next_inputs = next.parameter_dependencies();
  CHECK(previous_inputs.size() == next_inputs.size());
  for (const auto& id : next_inputs)
    CHECK(std::find(previous_inputs.begin(), previous_inputs.end(), id) == previous_inputs.end());
  CHECK(result.parameters().size() == part.parameters().size() + 2 * previous_inputs.size());
  if (mode == 4) {
    const auto& before_steps = std::get<HoleStepped>(first.manufacturing()).steps;
    const auto& after_steps = std::get<HoleStepped>(next.manufacturing()).steps;
    REQUIRE(before_steps.size() == after_steps.size());
    for (std::size_t i = 0; i < before_steps.size(); ++i)
      CHECK(before_steps[i].id != after_steps[i].id);
  }
  const double removed[] = {8, 24, 44. / 3, 16, 31, 8, 8, 12};
  const double cut = mode == 8 ? (8 + std::numbers::pi) * 8 : removed[mode] * std::numbers::pi;
  CHECK(volume(w) == Catch::Approx(38400 - 2 * cut).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Create Another retains custom or pinned Thread intent and explicit shared length bindings",
    "[gui][parity-b157][hole-repeat-shell]") {
  const bool pinned = GENERATE(false, true);
  const bool shared = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = threads::pair_part();
  open(w, dir, part);
  create_thread(w);
  if (pinned)
    thread_mapping(w);
  if (shared) {
    auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/thread/length");
    binding.setCurrentIndex(binding.findData("thread.length"));
  }
  field<QDoubleSpinBox>(w, "/thread/length").setValue(2);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.create_another").isVisible());
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  const auto first = w.session().part_document()->thread_features().front();
  CHECK(control<QComboBox>(w, "blcad.thread.source").currentData() ==
        (pinned ? "standard" : "custom"));
  CHECK(field<QDoubleSpinBox>(w, "/thread/length").value() == 2);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(true);
  settle();
  field<QDoubleSpinBox>(w, "/thread/start").setValue(4);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  REQUIRE(result.thread_features().size() == 2);
  const auto& next = result.thread_features().back();
  CHECK(next.id() != first.id());
  CHECK(next.direction() == first.direction());
  CHECK(next.thread().profile().index() == first.thread().profile().index());
  CHECK((next.thread().length() == first.thread().length()) == shared);
  CHECK(result.parameters().size() ==
        part.parameters().size() + (pinned ? 0 : 8) + (shared ? 0 : 2) + 1);
  CHECK(result.engineering_resources() == part.engineering_resources());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  REQUIRE(checked(geometry::ThreadAdapter::cosmetic_presentations(
                      *w.session().part_shape_cache()->find_body_shape(BodyId("body.shaft"))))
              .size() == 2);
  replay(w, dir, before);
}

TEST_CASE(
    "Repeated internal Threads preserve bore aliases while Cancel discards fresh candidate inputs",
    "[gui][parity-b157][hole-repeat-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  choose_hole_mode(w, "blcad.hole.internal_thread", 1);
  const auto minor =
      control<QComboBox>(w, "blcad.feature_edit.binding/thread/profile/minor_diameter")
          .currentData();
  auto& bore = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  bore.setCurrentIndex(bore.findData(minor));
  REQUIRE(w.feature_edit_tools().apply(true));
  const auto before = snapshot(w);
  const auto first = w.session().part_document()->hole_features().front();
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(15);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply(true));
  const auto second_state = snapshot(w);
  REQUIRE(w.feature_edit_tools().active());
  const auto second = w.session().part_document()->hole_features().back();
  REQUIRE(second.thread());
  const auto& profile = std::get<ThreadParameterProfile>(second.thread()->profile());
  CHECK(std::get<ParameterId>(second.diameter()) == profile.minor_diameter);
  CHECK(std::get<ParameterId>(second.diameter()) != std::get<ParameterId>(first.diameter()));
  CHECK(volume(w) == Catch::Approx(38400 - 64 * std::numbers::pi).margin(1e-6));
  field<QDoubleSpinBox>(w, "/thread/length").setValue(0);
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == second_state);
  CHECK_FALSE(w.session().task().active());
  CHECK(w.viewport().preview_selection_target_count() == 0);
  replay(w, dir, before);
}

TEST_CASE("Native Create Another retains a Counterbore recipe and separate transactional ownership",
          "[gui][parity-b157][hole-repeat-shell][native-hole-repeat]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native repeated Hole authoring");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  click(control<QPushButton>(w, "blcad.feature_edit.create_another"));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  CHECK(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 1);
  activate(w);
  auto& x = field<QDoubleSpinBox>(w, "/placements/0/position/x");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&x);
  x.setFocus();
  REQUIRE(x.hasFocus());
  x.selectAll();
  key(x, Qt::Key_1, "15");
  key(x, Qt::Key_Return);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-repeat-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(w.session().part_document()->hole_features().size() == 2);
  CHECK(volume(w) == Catch::Approx(38400 - 48 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Create Another retains every termination and its external limit inputs",
          "[gui][parity-b157][hole-repeat-shell]") {
  const int extent = GENERATE(0, 1, 2, 3, 4, 5);
  CAPTURE(extent);
  auto part = plate();
  REQUIRE(part.add_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("limit.plane"), "Limit plane",
                                                 {0, 0, -4}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  blcad::test::history_fixture::rectangle(part, "limit.profile", "limit.plane");
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("limit"), "Limit", SketchId("limit.profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(
      feature.with_body_result_context(blcad::test::history_fixture::new_body(part, "limit")))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  choose_hole_mode(w, "blcad.hole.extent", extent);
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  CHECK(control<QComboBox>(w, "blcad.hole.extent").currentIndex() == extent);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(15);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  CHECK(result.hole_features().front().extent().index() ==
        result.hole_features().back().extent().index());
  const double depths[] = {5, 8, 8, 8, 4, 4};
  CHECK(volume(w) == Catch::Approx(38400 - 2 * depths[extent] * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.limit") == Catch::Approx(38400).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Repeated Holes preserve formula and pinned seat bindings while copying only owned depth",
          "[gui][parity-b157][hole-repeat-shell]") {
  auto part = plate();
  REQUIRE(part.add_engineering_resource(manufacture::table()));
  REQUIRE(part.add_parameter(checked(Parameter::create_expression(
      ParameterId("shared.diameter"), "shared_diameter", ParameterType::Length, "diameter",
      checked(Quantity::length_mm(3))))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  create_hole(w);
  auto& bore = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  bore.setCurrentIndex(bore.findData("shared.diameter"));
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  auto& source = control<QComboBox>(w, "blcad.hole.source/manufacturing/diameter");
  const int pin = source.findData(QStringList{"synthetic.holes", "r1", "small", "seat.diameter"});
  REQUIRE(pin >= 0);
  source.setCurrentIndex(pin);
  settle();
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  CHECK_FALSE(field<QDoubleSpinBox>(w, "/diameter/parameter").isEnabled());
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(15);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  const auto& first = result.hole_features().front();
  const auto& second = result.hole_features().back();
  CHECK(std::get<ParameterId>(first.diameter()) == ParameterId("shared.diameter"));
  CHECK(std::get<ParameterId>(second.diameter()) == ParameterId("shared.diameter"));
  const auto& first_seat = std::get<HoleCounterbore>(first.manufacturing());
  const auto& second_seat = std::get<HoleCounterbore>(second.manufacturing());
  CHECK(std::get<HoleStandardSelection>(first_seat.diameter) ==
        std::get<HoleStandardSelection>(second_seat.diameter));
  CHECK(std::get<ParameterId>(first_seat.depth) != std::get<ParameterId>(second_seat.depth));
  CHECK(result.parameters().size() == part.parameters().size() + 2);
  CHECK(result.find_parameter(ParameterId("shared.diameter"))->formula() ==
        part.find_parameter(ParameterId("shared.diameter"))->formula());
  CHECK(result.engineering_resources() == part.engineering_resources());
  CHECK(volume(w) == Catch::Approx(38400 - 43 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Native Thread Create Another retains its profile and commits a separate corrected interval",
    "[gui][parity-b157][hole-repeat-shell][native-hole-repeat]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native repeated Thread authoring");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  create_thread(w);
  activate(w);
  auto& length = field<QDoubleSpinBox>(w, "/thread/length");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&length);
  length.setFocus();
  REQUIRE(length.hasFocus());
  length.selectAll();
  key(length, Qt::Key_2, "2");
  key(length, Qt::Key_Return);
  click(control<QPushButton>(w, "blcad.feature_edit.create_another"));
  REQUIRE(w.feature_edit_tools().active());
  const auto before = snapshot(w);
  CHECK(field<QDoubleSpinBox>(w, "/thread/length").value() == 2);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  click(control<QCheckBox>(w, "blcad.thread.start.enabled"));
  auto& start = field<QDoubleSpinBox>(w, "/thread/start");
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll").ensureWidgetVisible(&start);
  start.setFocus();
  REQUIRE(start.hasFocus());
  start.selectAll();
  key(start, Qt::Key_4, "4");
  key(start, Qt::Key_Return);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "thread-repeat-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const auto& result = *w.session().part_document();
  REQUIRE(result.thread_features().size() == 2);
  CHECK(result.thread_features().front().id() != result.thread_features().back().id());
  CHECK(result.thread_features().front().thread().length() !=
        result.thread_features().back().thread().length());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A repeated or existing modeled Thread can explicitly move to another shaft",
          "[gui][parity-b157][hole-repeat-shell]") {
  const bool repeated = GENERATE(false, true);
  CAPTURE(repeated);
  auto part = threads::pair_part();
  auto other = checked(Feature::create_additive_extrude(
      FeatureId("other.shaft"), "Other shaft", SketchId("shaft.profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(other.with_body_result_context(
      blcad::test::history_fixture::new_body(part, "other.shaft")))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.shaft"}));
  create_thread(w);
  CHECK(field<QComboBox>(w, "/target_body").currentData() == "body.shaft");
  field<QDoubleSpinBox>(w, "/thread/length").setValue(2);
  auto& mode = field<QComboBox>(w, "/thread/representation");
  mode.setCurrentIndex(mode.findData("modeled"));
  REQUIRE(w.feature_edit_tools().apply(repeated));
  const auto before = snapshot(w);
  if (!repeated) {
    const auto id = w.session().part_document()->thread_features().front().id();
    begin(w, id.value().c_str());
  }
  REQUIRE(w.feature_edit_tools().active());
  CHECK(field<QComboBox>(w, "/thread/representation").currentData() == "modeled");
  auto& target = field<QComboBox>(w, "/target_body");
  const int body = target.findData("body.other.shaft");
  REQUIRE(body >= 0);
  target.setCurrentIndex(body);
  settle();
  auto& support = field<QComboBox>(w, "/support");
  REQUIRE(support.count() > 0);
  support.setCurrentIndex(0);
  INFO(support.currentData().toString().toStdString());
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  const auto applied = w.feature_edit_tools().apply();
  INFO((applied ? "Applied" : applied.error().object_id() + ": " + applied.error().message()));
  REQUIRE(applied);
  const auto& result = *w.session().part_document();
  REQUIRE(result.thread_features().size() == (repeated ? 2 : 1));
  CHECK(result.thread_features().back().target_body() == BodyId("body.other.shaft"));
  CHECK(result.thread_features().back().thread().representation() == ThreadRepresentation::Modeled);
  const auto& face =
      std::get<ResultFaceReference>(result.thread_features().back().support().source());
  CHECK(face.body() == BodyId("body.other.shaft"));
  const double removed =
      4 * std::numbers::pi * std::tan(std::numbers::pi / 6) * 2 / 1.5 * (4. / 2 + 1. / 3);
  CHECK(volume(w, "body.shaft") ==
        Catch::Approx(200 * std::numbers::pi - (repeated ? removed : 0)).margin(2e-4));
  CHECK(volume(w, "body.other.shaft") ==
        Catch::Approx(200 * std::numbers::pi - removed).margin(2e-4));
  replay(w, dir, before);
}

namespace {
PartDocument point_plate() {
  auto part = plate();
  REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("positions.plane"), "Rotated position frame", {-15, 0, 8}, {0, 1, 0},
      {-1, 0, 0}, {0, 0, 1}))));
  auto sketch = checked(
      Sketch::create(SketchId("positions"), "Hole positions", DatumPlaneId("positions.plane")));
  REQUIRE(sketch.add_entity(checked(SketchPoint::create(SketchEntityId("a"), {0, 0}))));
  REQUIRE(sketch.add_entity(checked(SketchPoint::create(SketchEntityId("b"), {0, -30}))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  auto wrong = checked(
      Sketch::create(SketchId("wrong.positions"), "Wrong support", DatumPlaneId("datum.xy")));
  REQUIRE(wrong.add_entity(checked(SketchPoint::create(SketchEntityId("a"), {0, 0}))));
  REQUIRE(part.add_sketch(std::move(wrong)));
  return part;
}
void reference_position(ShellWindow& w, const char* path, const QStringList& key) {
  const auto kind_path = QString::fromLatin1(path) + "/kind";
  auto* kind = qobject_cast<QComboBox*>(w.feature_edit_tools().field(kind_path));
  REQUIRE(kind);
  kind->setCurrentIndex(1);
  settle();
  auto* point =
      qobject_cast<QComboBox*>(w.feature_edit_tools().field(QString::fromLatin1(path) + "/point"));
  REQUIRE(point);
  const int selected = point->findData(key);
  REQUIRE(selected >= 0);
  point->setCurrentIndex(selected);
  settle();
}
void move_position_source(ShellWindow& w, double local_x) {
  REQUIRE(w.session().commit_part_transaction("Move source point", [local_x](PartDocument& part) {
    auto sketch =
        Sketch::create(SketchId("positions"), "Hole positions", DatumPlaneId("positions.plane"));
    if (!sketch)
      return Result<std::size_t>::failure(sketch.error());
    auto point = SketchPoint::create(SketchEntityId("a"), {local_x, 0});
    auto added = sketch.value().add_entity(point.value());
    if (!added)
      return added;
    added = sketch.value().add_entity(SketchPoint::create(SketchEntityId("b"), {0, -30}).value());
    if (!added)
      return added;
    return part.update_sketch(std::move(sketch.value()));
  }));
  w.refresh();
  settle();
}
} // namespace

TEST_CASE("Hole point selectors preserve scoped references and reject incompatible frames",
          "[gui][parity-b157][hole-position-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, point_plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  reference_position(w, "/placements/0", {"positions", "a"});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  control<QPushButton>(w, "blcad.feature_edit.add/placements").click();
  settle();
  reference_position(w, "/placements/1", {"positions", "b"});
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  reference_position(w, "/placements/1", {"wrong.positions", "a"});
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().apply());
  CHECK(snapshot(w) == before);
  CHECK(control<QLabel>(w, "blcad.feature_edit.status").text().contains("support plane"));
  reference_position(w, "/placements/1", {"positions", "b"});
  REQUIRE(w.feature_edit_tools().apply());
  const auto hole = w.session().part_document()->hole_features().front();
  REQUIRE(hole.placements().size() == 2);
  CHECK(hole.placements()[0].id != hole.placements()[1].id);
  CHECK(std::get<HoleSketchPointReference>(hole.placements()[1].source).point ==
        SketchEntityId("b"));
  CHECK(volume(w) == Catch::Approx(38400 - 16 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
  const auto after = snapshot(w);
  begin(w, hole.id().value().c_str());
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(0);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  field<QComboBox>(w, "/placements/0/kind").setCurrentIndex(0);
  settle();
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/x").value() == Catch::Approx(-15));
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/y").value() == Catch::Approx(0));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& independent = w.session().part_document()->hole_features().front().placements()[0];
  CHECK(std::get<Point2>(independent.source).x == Catch::Approx(-15));
  CHECK(independent.id == hole.placements()[0].id);
  CHECK(volume(w) == Catch::Approx(38400 - 16 * std::numbers::pi).margin(1e-6));
  replay(w, dir, after);
}

TEST_CASE("Hole source breadcrumb keeps edits local through Save Cancel and one combined Apply",
          "[gui][parity-b157][hole-position-shell][source-sketch-workflow]") {
  const bool existing = GENERATE(false, true);
  CAPTURE(existing);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, point_plate());
  REQUIRE(
      w.session().selection().add({GuiSelectionKind::SketchEntity, "sketch/positions/entity/a"}));
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  REQUIRE(field<QComboBox>(w, "/placements/0/point").currentData() ==
          QStringList{"positions", "a"});
  if (existing) {
    REQUIRE(w.feature_edit_tools().apply());
    begin(w, w.session().part_document()->hole_features().front().id().value().c_str());
  }
  const auto before = snapshot(w);
  const auto constraints = w.session().sketch_constraint_catalogs();
  const auto dimensions = w.session().sketch_dimension_catalogs();
  auto idle_entry = w.session();
  (void)idle_entry.task().cancel();
  const auto undo = std::string(idle_entry.undo_label());
  const auto selection = w.session().selection().entries();
  const auto selection_mask = w.session().selection().filter_mask();
  control<QPushButton>(w, "blcad.hole.edit_source/placements/0").click();
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  REQUIRE(w.active_sketch() == SketchId("positions"));
  CHECK_FALSE(w.feature_edit_tools().apply());
  move_position_source(w, 10);
  const auto edited = snapshot(w);
  REQUIRE(edited != before);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == edited);
  const auto saved = dir.filePath("source-draft.blcad.json");
  REQUIRE(w.save_to(saved));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(saved.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == before);
  control<QPushButton>(w, "blcad.hole.breadcrumb").click();
  REQUIRE_FALSE(w.active_sketch());
  REQUIRE_FALSE(w.feature_edit_tools().editing_source_sketch());
  CHECK(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
  CHECK(w.session().sketch_constraint_catalogs() == constraints);
  CHECK(w.session().sketch_dimension_catalogs() == dimensions);
  CHECK(w.session().undo_label() == undo);
  CHECK(w.session().selection().entries() == selection);
  CHECK(w.session().selection().filter_mask() == selection_mask);
  CHECK_FALSE(w.session().part_workflow_active());
  if (existing)
    begin(w, w.session().part_document()->hole_features().front().id().value().c_str());
  else {
    w.session().selection().clear();
    REQUIRE(
        w.session().selection().add({GuiSelectionKind::SketchEntity, "sketch/positions/entity/a"}));
    create_hole(w);
    field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  }
  control<QPushButton>(w, "blcad.hole.edit_source/placements/0").click();
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  move_position_source(w, 10);
  REQUIRE(w.finish_active_sketch());
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(w.feature_edit_tools().apply());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(w.session()
            .part_document()
            ->find_sketch(SketchId("positions"))
            ->find_point(SketchEntityId("a"))
            ->position()
            .x == 10);
  CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
  const auto& result = *w.session().part_document();
  const auto hole = result.hole_features().front();
  const auto axis = checked(HoleInstanceReference::create(hole.id(), hole.placements().front().id,
                                                          HoleReferenceRole::Axis));
  const auto physical = checked(
      geometry::HoleInstanceGeometry::resolve(result, axis, *w.session().part_shape_cache()));
  CHECK(physical.frame.origin.x == Catch::Approx(-15));
  CHECK(physical.frame.origin.y == Catch::Approx(10));
  CHECK(physical.frame.origin.z == Catch::Approx(8));
  replay(w, dir, before);
  const auto accepted_state = snapshot(w);
  begin(w, hole.id().value().c_str());
  control<QPushButton>(w, "blcad.hole.edit_source/placements/0").click();
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  move_position_source(w, 5);
  w.feature_edit_tools().cancel();
  CHECK_FALSE(w.active_sketch());
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(snapshot(w) == accepted_state);
}

TEST_CASE("Native Hole point picking and source breadcrumb publish one scoped workflow",
          "[gui][parity-b157][hole-position-shell][native-hole-position]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native position picking and Sketch return");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, point_plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  reference_position(w, "/placements/0", {"positions", "a"});
  click(control<QToolButton>(w, "blcad.hole.position/placements/0.pick"));
  REQUIRE(w.viewport().preview_selection_target_count() >= 2);
  const auto screen = checked(w.viewport().model_to_screen({15, 0, 8}));
  click_at(w.viewport(), {screen.x, screen.y});
  REQUIRE(field<QComboBox>(w, "/placements/0/point").currentData() ==
          QStringList{"positions", "b"});
  click(control<QPushButton>(w, "blcad.hole.edit_source/placements/0"));
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  activate(w);
  auto& point = control<QAction>(w, "sketch.create.point");
  REQUIRE(point.isEnabled());
  point.trigger();
  settle();
  const auto source_screen = checked(w.viewport().sketch_plane_to_screen({10, -15}));
  click_at(w.viewport(), {source_screen.x, source_screen.y});
  const auto added =
      w.session().part_document()->find_sketch(SketchId("positions"))->points().size();
  REQUIRE(added == 3);
  capture(w, "hole-source-sketch");
  click(control<QPushButton>(w, "blcad.hole.breadcrumb"));
  REQUIRE_FALSE(w.active_sketch());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-position-return");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  const auto& ref = std::get<HoleSketchPointReference>(
      w.session().part_document()->hole_features().front().placements()[0].source);
  CHECK(ref.sketch == SketchId("positions"));
  CHECK(ref.point == SketchEntityId("b"));
  CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Hole creation preselects multiple explicit and projected points without copying their "
          "coordinates",
          "[gui][parity-b157][hole-position-shell]") {
  auto part = point_plate();
  REQUIRE(part.add_construction_point(checked(ConstructionPoint::create_explicit(
      ConstructionPointId("position.projector"), "Projected position", {0, 15, 8}))));
  auto sketch = *part.find_sketch(SketchId("positions"));
  REQUIRE(sketch.add_reference(checked(ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("projected"), ConstructionPointId("position.projector")))));
  REQUIRE(part.update_sketch(std::move(sketch)));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  for (const auto* id : {"sketch/positions/entity/a", "sketch/positions/entity/projected"})
    REQUIRE(w.session().selection().add({GuiSelectionKind::SketchEntity, id}));
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& positions = w.session().part_document()->hole_features().front().placements();
  REQUIRE(positions.size() == 2);
  CHECK(std::get<HoleSketchPointReference>(positions[0].source).point == SketchEntityId("a"));
  CHECK(std::get<HoleSketchPointReference>(positions[1].source).point ==
        SketchEntityId("projected"));
  CHECK(volume(w) == Catch::Approx(38400 - 16 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A Slot placement can recover from a referenced source without abandoning the recipe",
          "[gui][parity-b157][hole-position-shell]") {
  auto part = manufacture::seed();
  auto sketch = *part.find_sketch(SketchId("slot"));
  REQUIRE(sketch.add_entity(checked(SketchPoint::create(SketchEntityId("anchor"), {0, 0}))));
  REQUIRE(part.update_sketch(std::move(sketch)));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  const auto original_volume = volume(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 8);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  reference_position(w, "/placements/0", {"slot", "anchor"});
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().apply());
  field<QComboBox>(w, "/placements/0/kind").setCurrentIndex(0);
  settle();
  CHECK(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 8);
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/x").value() == 0);
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/y").value() == 0);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(original_volume - (8 + std::numbers::pi) * 8).margin(1e-6));
  replay(w, dir, before);
}

namespace {
GuiViewportManipulatorHandle canvas(ShellWindow& w, const char* path) {
  if (QApplication::platformName() != "xcb") {
    GuiViewportCameraBookmark camera;
    camera.eye = {200, 200, 200};
    camera.scale = 200;
    camera.projection = GuiViewportProjection::Orthographic;
    REQUIRE(w.viewport().restore_camera_bookmark(camera));
    w.feature_edit_tools().refresh_presentation();
  }
  auto& controls = control<QComboBox>(w, "blcad.feature_edit.dynamic");
  const int index = controls.findData(QString::fromLatin1(path));
  REQUIRE(index >= 0);
  controls.setCurrentIndex(index);
  settle();
  REQUIRE(w.feature_edit_tools().manipulators().handles().size() == 1);
  return w.feature_edit_tools().manipulators().handles().front();
}
void canvas_mouse(ShellWindow& w, QEvent::Type type, GuiSketchScreenPoint point) {
  const QPointF local(point.x, point.y);
  QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                    type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &event);
  settle();
}
GuiSketchScreenPoint start_canvas(ShellWindow& w) {
  const auto drawings = w.feature_edit_tools().manipulators().presentations();
  REQUIRE(drawings.size() == 1);
  const auto pointer = drawings.front().endpoint;
  REQUIRE(w.viewport().rect().contains(QPointF(pointer.x, pointer.y).toPoint()));
  canvas_mouse(w, QEvent::MouseButtonPress, pointer);
  REQUIRE_FALSE(w.feature_edit_tools().manipulators().active_handle_id().empty());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.create_another").isEnabled());
  return pointer;
}
} // namespace

TEST_CASE("Hole canvas diameter candidates correct invalid input cancel exactly and publish once",
          "[gui][parity-b157][hole-canvas-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  const auto descriptor = canvas(w, "/diameter/parameter");
  CHECK(descriptor.kind == GuiViewportManipulatorKind::Radial);
  CHECK(descriptor.model_units_per_value == Catch::Approx(.5));
  CHECK(descriptor.origin.z == Catch::Approx(8));
  auto pointer = start_canvas(w);
  CHECK_FALSE(w.feature_edit_tools().set_drag_text("0 mm"));
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  CHECK(field<QDoubleSpinBox>(w, "/diameter/parameter").value() == 4);
  key(w.viewport(), Qt::Key_Escape);
  CHECK(field<QDoubleSpinBox>(w, "/diameter/parameter").value() == 2);
  CHECK(snapshot(w) == before);
  CHECK(control<QPushButton>(w, "blcad.feature_edit.create_another").isEnabled());
  pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QPushButton>(w, "blcad.feature_edit.create_another").isEnabled());
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("A selected Hole placement owns its canvas frame while shared depth remains explicit",
          "[gui][parity-b157][hole-canvas-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  control<QPushButton>(w, "blcad.feature_edit.add/placements").click();
  settle();
  field<QDoubleSpinBox>(w, "/placements/1/position/x").setValue(15);
  const auto selected = canvas(w, "/placements/1/position/y");
  const auto placement_id = control<QGroupBox>(w, "blcad.feature_edit.entry/placements/1")
                                .property("semanticId")
                                .toString()
                                .toStdString();
  CHECK(selected.origin.x == Catch::Approx(15));
  CHECK(selected.origin.y == Catch::Approx(0));
  auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("5 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/y").value() == 0);
  CHECK(field<QDoubleSpinBox>(w, "/placements/1/position/y").value() == 5);
  control<QPushButton>(w, "blcad.feature_edit.↑/placements/1").click();
  settle();
  CHECK(control<QComboBox>(w, "blcad.feature_edit.dynamic").currentData() ==
        "/placements/0/position/y");
  REQUIRE(w.feature_edit_tools().manipulators().handles().size() == 1);
  CHECK(w.feature_edit_tools().manipulators().handles().front().id == selected.id);
  choose_hole_mode(w, "blcad.hole.extent", 0);
  (void)canvas(w, "/extent/depth");
  pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("3 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& part = *w.session().part_document();
  const auto& hole = part.hole_features().front();
  const auto ref = checked(HoleInstanceReference::create(hole.id(), HolePlacementId(placement_id),
                                                         HoleReferenceRole::Axis));
  const auto frame =
      checked(geometry::HoleInstanceGeometry::resolve(part, ref, *w.session().part_shape_cache()));
  CHECK(frame.frame.origin.x == Catch::Approx(15));
  CHECK(frame.frame.origin.y == Catch::Approx(5));
  CHECK(volume(w) == Catch::Approx(38400 - 6 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Hole canvas formula override creates an owned dimension without changing its source",
          "[gui][parity-b157][hole-canvas-shell]") {
  auto part = plate();
  REQUIRE(part.add_parameter(checked(
      Parameter::create_expression(ParameterId("shared.bore"), "shared_bore", ParameterType::Length,
                                   "diameter", checked(Quantity::length_mm(3))))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  create_hole(w);
  auto& binding = control<QComboBox>(w, "blcad.feature_edit.binding/diameter/parameter");
  binding.setCurrentIndex(binding.findData("shared.bore"));
  CHECK_FALSE(field<QDoubleSpinBox>(w, "/diameter/parameter").isEnabled());
  auto& controls = control<QComboBox>(w, "blcad.feature_edit.dynamic");
  controls.setCurrentIndex(controls.findData("/diameter/parameter"));
  CHECK(w.feature_edit_tools().manipulators().handles().empty());
  control<QPushButton>(w, "blcad.feature_edit.independent/diameter/parameter").click();
  settle();
  REQUIRE(field<QDoubleSpinBox>(w, "/diameter/parameter").isEnabled());
  (void)canvas(w, "/diameter/parameter");
  const auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  REQUIRE(w.feature_edit_tools().apply());
  const auto& result = *w.session().part_document();
  CHECK(result.find_parameter(ParameterId("shared.bore"))->formula() == "diameter");
  CHECK(result.find_parameter(ParameterId("shared.bore"))->value().millimeters() == 3);
  CHECK(result.parameters().size() == part.parameters().size() + 1);
  CHECK(volume(w) == Catch::Approx(38400 - 32 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Hole canvas frames follow rotations and uniform scale after the edited feature",
          "[gui][parity-b157][hole-canvas-shell][native-hole-canvas]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  if (QApplication::platformName() == "xcb")
    activate(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  REQUIRE(w.feature_edit_tools().apply());
  const auto id = w.session().part_document()->hole_features().front().id();
  REQUIRE(w.session().commit_part_transaction("Pose the drilled plate", [](PartDocument& part) {
    auto added = part.add_body_transform(
        BodyTransform::create_uniform_scale(BodyTransformId("after.scale"), BodyId("body.base"),
                                            BodyTransformCoordinateSpace::World, std::nullopt, 2,
                                            {}, false, false)
            .value());
    if (!added)
      return added;
    added = part.add_body_transform(
        BodyTransform::create_rotate(
            BodyTransformId("after.rotate"), BodyId("body.base"),
            BodyTransformCoordinateSpace::World, std::nullopt,
            BodyTransformRotationAxis::create_explicit({}, {0, 1, 0}).value(), 90, false, false)
            .value());
    if (!added)
      return added;
    return part.add_body_transform(
        BodyTransform::create_translate(BodyTransformId("after.translate"), BodyId("body.base"),
                                        BodyTransformCoordinateSpace::World, std::nullopt,
                                        {40, 10, 0}, false, false)
            .value());
  }));
  w.refresh();
  w.viewport().fit_all();
  settle();
  const auto before = snapshot(w);
  begin(w, id.value().c_str());
  const auto posed = canvas(w, "/diameter/parameter");
  CHECK(posed.origin.x == Catch::Approx(56));
  CHECK(posed.origin.y == Catch::Approx(10));
  CHECK(posed.origin.z == Catch::Approx(0).margin(1e-9));
  CHECK(posed.model_units_per_value == Catch::Approx(1));
  const auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  capture(w, "hole-canvas-scaled-preview");
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx((38400 - 32 * std::numbers::pi) * 8).margin(1e-5));
  replay(w, dir, before);
}

TEST_CASE("Thread interval canvas anchors the length after the explicit start offset",
          "[gui][parity-b157][hole-canvas-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  const auto before = snapshot(w);
  create_thread(w);
  auto descriptor = canvas(w, "/thread/length");
  CHECK(std::abs(descriptor.axis.z) == Catch::Approx(1));
  auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("2 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(true);
  settle();
  (void)canvas(w, "/thread/start");
  pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  const auto length_frame = canvas(w, "/thread/length");
  CHECK(length_frame.origin.z == Catch::Approx(descriptor.origin.z + 4 * descriptor.axis.z));
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  REQUIRE(checked(geometry::ThreadAdapter::cosmetic_presentations(
                      *w.session().part_shape_cache()->find_body_shape(BodyId("body.shaft"))))
              .size() == 1);
  replay(w, dir, before);
}

TEST_CASE("Native Hole canvas release samples the final pointer on a posed support and cancels "
          "numeric drag",
          "[gui][parity-b157][hole-canvas-shell][native-hole-canvas]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native support-anchored Hole interaction");
  auto part = plate();
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("before.rotate"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {0, 0, 1})), 37, false,
      false))));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("before.translate"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {50, 20, 10}, false, false))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  activate(w);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  const auto descriptor = canvas(w, "/placements/0/position/x");
  CHECK(descriptor.origin.z == Catch::Approx(18));
  const auto drawings = w.feature_edit_tools().manipulators().presentations();
  REQUIRE(drawings.size() == 1);
  const auto pointer = start_canvas(w);
  const GuiSketchScreenPoint final{pointer.x + .2 * (pointer.x - drawings.front().anchor.x),
                                   pointer.y + .2 * (pointer.y - drawings.front().anchor.y)};
  const auto measure = [&](GuiSketchScreenPoint pixel) {
    const auto ray = checked(w.viewport().screen_to_view_ray(pixel));
    const auto d = normalized(ray.direction);
    const auto axis = normalized(descriptor.axis);
    const Vector3 delta{ray.origin.x - descriptor.origin.x, ray.origin.y - descriptor.origin.y,
                        ray.origin.z - descriptor.origin.z};
    const double projection = dot(axis, d);
    return (dot(delta, axis) - projection * dot(delta, d)) / (1 - projection * projection);
  };
  const double expected = descriptor.reference_value +
                          (measure(final) - measure(pointer)) / descriptor.model_units_per_value;
  // No intermediate move: the actual mouse release must be authoritative.
  canvas_mouse(w, QEvent::MouseButtonRelease, final);
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/x").value() ==
        Catch::Approx(expected).margin(1e-5));
  CHECK(snapshot(w) == before);
  (void)canvas(w, "/diameter/parameter");
  auto grip = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  key(w.viewport(), Qt::Key_Escape);
  CHECK(field<QDoubleSpinBox>(w, "/diameter/parameter").value() == 2);
  choose_hole_mode(w, "blcad.hole.extent", 0);
  (void)canvas(w, "/extent/depth");
  grip = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("3 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, grip);
  capture(w, "hole-canvas-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(volume(w) == Catch::Approx(38400 - 3 * std::numbers::pi).margin(1e-6));
  const auto& result = *w.session().part_document();
  const auto& hole = result.hole_features().front();
  const auto reference = checked(
      HoleInstanceReference::create(hole.id(), hole.placements()[0].id, HoleReferenceRole::Axis));
  const auto actual = checked(
      geometry::HoleInstanceGeometry::resolve(result, reference, *w.session().part_shape_cache()));
  const double angle = 37 * std::numbers::pi / 180;
  CHECK(actual.frame.origin.x == Catch::Approx(50 + expected * std::cos(angle)).margin(1e-5));
  CHECK(actual.frame.origin.y == Catch::Approx(20 + expected * std::sin(angle)).margin(1e-5));
  CHECK(actual.frame.origin.z == Catch::Approx(18));
  replay(w, dir, before);
}

TEST_CASE("Native Thread interval grips drive a modeled cut with one Apply and exact history",
          "[gui][parity-b157][hole-canvas-shell][native-hole-canvas]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native support-anchored Thread interaction");
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part());
  activate(w);
  const auto before = snapshot(w);
  const auto original_volume = volume(w, "body.shaft");
  create_thread(w);
  (void)canvas(w, "/thread/length");
  auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("2 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  click(control<QCheckBox>(w, "blcad.thread.start.enabled"));
  (void)canvas(w, "/thread/start");
  pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("4 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  auto& representation = field<QComboBox>(w, "/thread/representation");
  representation.setCurrentIndex(representation.findData("modeled"));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  (void)canvas(w, "/thread/length");
  pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("3 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  capture(w, "thread-canvas-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const auto& result = *w.session().part_document();
  const auto definition =
      checked(resolve_thread_definition(result, result.thread_features().front().thread()));
  CHECK(definition.start_mm == 4);
  CHECK(definition.length_mm == 3);
  const double depth = (definition.major_diameter_mm - definition.minor_diameter_mm) / 2;
  // Integrate 2*pi*r times the V-groove's axial fraction: the radial
  // integral is a*depth^2/2 + depth^3/3, with a = minor diameter / 2.
  const double removed = 4 * std::numbers::pi * depth * depth *
                         std::tan(definition.included_angle_deg * std::numbers::pi / 360) *
                         definition.length_mm / definition.pitch_mm *
                         (definition.minor_diameter_mm / 4 + depth / 3);
  CHECK(volume(w, "body.shaft") == Catch::Approx(original_volume - removed).margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE("Profile-driven Slot placement and width do not expose ineffective canvas controls",
          "[gui][parity-b157][hole-canvas-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, manufacture::seed());
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 8);
  choose_hole_mode(w, "blcad.hole.extent", 0);
  const auto& controls = control<QComboBox>(w, "blcad.feature_edit.dynamic");
  CHECK(controls.findData("/placements/0/position/x") == -1);
  CHECK(controls.findData("/placements/0/position/y") == -1);
  CHECK(controls.findData("/diameter/parameter") == -1);
  (void)canvas(w, "/extent/depth");
  const auto pointer = start_canvas(w);
  REQUIRE(w.feature_edit_tools().set_drag_text("3 mm"));
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(volume(w) == Catch::Approx(1600 - (8 + std::numbers::pi) * 3).margin(1e-6));
}

namespace {
struct PresetFile {
  QTemporaryDir directory;
  QByteArray previous = qgetenv("BLCAD_PRESETS_PATH");
  PresetFile() {
    REQUIRE(directory.isValid());
    qputenv("BLCAD_PRESETS_PATH", path().toUtf8());
  }
  ~PresetFile() {
    qputenv("BLCAD_PRESETS_PATH", previous);
  }
  QString path() const {
    return directory.filePath("presets.json");
  }
};
void preset_name(ShellWindow& w, const char* name) {
  control<QComboBox>(w, "blcad.feature_preset.name").setCurrentText(QString::fromUtf8(name));
}
} // namespace

TEST_CASE("Native Hole presets snapshot values and retain a new document's placement and target",
          "[gui][parity-b157][hole-preset-shell][native-hole-preset]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native preset controls and compact layout");
  PresetFile presets;
  QTemporaryDir dir;
  ShellWindow w;
  open(w, dir, plate());
  const auto initial = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  choose_hole_mode(w, "blcad.hole.recipe", 1);
  field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").setValue(4);
  field<QDoubleSpinBox>(w, "/manufacturing/depth/parameter").setValue(2);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(-15);
  preset_name(w, "Ø2 counterbore");
  activate(w);
  click(control<QPushButton>(w, "blcad.feature_preset.save"));
  REQUIRE(checked(GuiHoleThreadPresetCommands::load(presets.path())).size() == 1);
  CHECK(snapshot(w) == initial);
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == initial);
  open(w, dir, threads::pair_part("preset.destination", false));
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/placements/0/position/x").setValue(3);
  preset_name(w, "Ø2 counterbore");
  click(control<QPushButton>(w, "blcad.feature_preset.load"));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(field<QDoubleSpinBox>(w, "/placements/0/position/x").value() == 3);
  CHECK(field<QDoubleSpinBox>(w, "/diameter/parameter").value() == 2);
  CHECK(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 1);
  CHECK(control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
            .horizontalScrollBar()
            ->maximum() == 0);
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
      .ensureWidgetVisible(&control<QComboBox>(w, "blcad.feature_preset.name"));
  settle();
  capture(w, "hole-preset-preview");
  const auto original_volume = volume(w);
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const auto& hole = w.session().part_document()->hole_features().front();
  CHECK(hole.target_body() == BodyId("body.base"));
  CHECK(volume(w) == Catch::Approx(original_volume - 14 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native Thread presets bring their exact revision into the current draft with one Apply",
          "[gui][parity-b157][hole-preset-shell][native-hole-preset]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Thread preset controls");
  PresetFile presets;
  auto source = threads::pair_part();
  REQUIRE(source.add_thread_feature(
      threads::external(threads::definition(threads::selected(), true, ThreadHandedness::Left))));
  REQUIRE(GuiHoleThreadPresetCommands::save(
      presets.path(),
      checked(GuiHoleThreadPresetCommands::capture(source, FeatureId("external"), "Pinned left"))));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, threads::pair_part("current.shaft", false));
  const auto before = snapshot(w);
  create_thread(w);
  preset_name(w, "Pinned left");
  activate(w);
  click(control<QPushButton>(w, "blcad.feature_preset.load"));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(control<QComboBox>(w, "blcad.thread.standard.table").currentIndex() > 0);
  CHECK(control<QComboBox>(w, "blcad.thread.standard.row").currentData().toString() ==
        "synthetic.row");
  CHECK(w.session().part_document()->engineering_resources().entries().empty());
  CHECK(snapshot(w) == before);
  control<QScrollArea>(w, "blcad.feature_edit.inputs_scroll")
      .ensureWidgetVisible(&control<QComboBox>(w, "blcad.feature_preset.name"));
  settle();
  capture(w, "thread-preset-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  const auto& part = *w.session().part_document();
  REQUIRE(part.engineering_resources().entries().size() == 1);
  CHECK(part.engineering_resources().find_table(threads::selected().table));
  CHECK_FALSE(part.engineering_resources().find_table(threads::selected("r2").table));
  const auto& thread = part.thread_features().front();
  CHECK(nlohmann::json::parse(snapshot(w)).at("thread_features").at(0).at("support") ==
        nlohmann::json::parse(checked(serialize_part_document_to_json(source)))
            .at("thread_features")
            .at(0)
            .at("support"));
  CHECK(thread.thread().handedness() == ThreadHandedness::Left);
  CHECK(part.find_parameter(thread.thread().length())->value().millimeters() == 4);
  CHECK(part.find_parameter(*thread.thread().start())->value().millimeters() == 1);
  CHECK(volume(w, "body.shaft") == Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Preset load failures preserve valid settings and unused imported revisions are pruned",
          "[gui][parity-b157][hole-preset-shell]") {
  const bool conflict = GENERATE(false, true);
  PresetFile presets;
  auto source = manufacture::seed();
  REQUIRE(source.add_engineering_resource(manufacture::table()));
  REQUIRE(source.add_hole_feature(manufacture::feature(1, HoleThroughAllExtent{}, true)));
  REQUIRE(GuiHoleThreadPresetCommands::save(
      presets.path(),
      checked(GuiHoleThreadPresetCommands::capture(source, FeatureId("hole"), "Pinned seat"))));
  auto destination = plate();
  if (conflict)
    REQUIRE(destination.add_engineering_resource(manufacture::table("r1", false, 6)));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, destination);
  const auto before = snapshot(w);
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  preset_name(w, "Pinned seat");
  control<QPushButton>(w, "blcad.feature_preset.load").click();
  settle();
  CHECK(snapshot(w) == before);
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  if (conflict) {
    CHECK(
        control<QLabel>(w, "blcad.feature_preset.status").text().startsWith("Preset not loaded:"));
    CHECK(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 0);
  } else {
    CHECK(field<QLabel>(w, "/manufacturing/diameter/table/revision").text() == "r1");
    choose_hole_mode(w, "blcad.hole.recipe", 0);
  }
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->engineering_resources() ==
        destination.engineering_resources());
  CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Preset name replacement and removal stay outside model history and lock during canvas input",
    "[gui][parity-b157][hole-preset-shell]") {
  PresetFile presets;
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, plate());
  const auto before = snapshot(w);
  create_hole(w);
  preset_name(w, "Everyday hole");
  control<QPushButton>(w, "blcad.feature_preset.save").click();
  settle();
  REQUIRE(checked(GuiHoleThreadPresetCommands::load(presets.path())).size() == 1);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
  control<QPushButton>(w, "blcad.feature_preset.save").click();
  settle();
  const auto saved = checked(GuiHoleThreadPresetCommands::load(presets.path()));
  REQUIRE(saved.size() == 1);
  CHECK(checked(resolve_hole_diameter(saved.front().values,
                                      saved.front().values.hole_features().front().diameter()))
            .diameter.millimeters() == 4);
  (void)canvas(w, "/diameter/parameter");
  const auto pointer = start_canvas(w);
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_preset.load").isEnabled());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_preset.save").isEnabled());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_preset.remove").isEnabled());
  canvas_mouse(w, QEvent::MouseButtonRelease, pointer);
  REQUIRE(control<QPushButton>(w, "blcad.feature_preset.remove").isEnabled());
  control<QPushButton>(w, "blcad.feature_preset.remove").click();
  settle();
  CHECK(checked(GuiHoleThreadPresetCommands::load(presets.path())).empty());
  CHECK_FALSE(control<QPushButton>(w, "blcad.feature_preset.load").isEnabled());
  CHECK(snapshot(w) == before);
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == before);
}

namespace {
PartDocument hidden_hole_input(int mode) {
  auto part = holes::part(false, false);
  if (mode == 0)
    REQUIRE(part.set_body_visibility(BodyId("body.base"), BodyVisibility::Hidden));
  if (mode == 2) {
    auto sketch =
        checked(Sketch::create(SketchId("later.stock"), "Later stock", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId("later.stock"), ParameterId("width"), ParameterId("height"), {60, 0}))));
    REQUIRE(part.add_sketch(std::move(sketch)));
    auto feature = checked(Feature::create_additive_extrude(
        FeatureId("later.base"), "Later stock", SketchId("later.stock"), ParameterId("depth")));
    REQUIRE(part.add_feature(checked(
        feature.with_body_result_context(blcad::test::history_fixture::new_body(part, "other")))));
    REQUIRE(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
        FeatureId("consume"), BodyBooleanOperation::Add, BodyId("body.other"),
        {BodyId("body.base")}, BodyBooleanResultMode::ModifyTarget, std::nullopt, false))));
    REQUIRE(part.apply_history_intent({{"base", "holes", "later.base", "consume"}, {}}));
  }
  return part;
}
PartDocument hidden_thread_input(int mode) {
  auto part = threads::pair_part();
  REQUIRE(part.add_thread_feature(threads::external()));
  if (mode == 0)
    REQUIRE(part.set_body_visibility(BodyId("body.shaft"), BodyVisibility::Hidden));
  if (mode == 2)
    REQUIRE(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
        FeatureId("consume"), BodyBooleanOperation::Add, BodyId("body.base"),
        {BodyId("body.shaft")}, BodyBooleanResultMode::ModifyTarget, std::nullopt, false))));
  return part;
}
void hide_input_presentation(ShellWindow& w, int mode, const char* id) {
  if (mode == 1) {
    REQUIRE(w.session().set_presentation_visible(std::string("body/") + id, false));
    w.refresh();
    settle();
  }
}
double modeled_external_removal() {
  const double depth = 1;
  return 4 * std::numbers::pi * depth * depth * std::tan(55 * std::numbers::pi / 360) * 4 / 1.7 *
         (8. / 4 + depth / 3);
}
PartDocument prefix_position_plate(bool authored = true) {
  auto part = point_plate();
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("later"), "Later body", SketchId("profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(
      feature.with_body_result_context(blcad::test::history_fixture::new_body(part, "later")))));
  if (authored)
    REQUIRE(part.apply_history_intent({{"base", "later"}, {}}));
  return part;
}
void prefix_after(ShellWindow& w, const char* id) {
  const auto history = checked(w.session().part_document()->executable_history());
  const auto found = std::find_if(history.begin(), history.end(),
                                  [&](const auto& row) { return row.node_id == id; });
  REQUIRE(found != history.end());
  REQUIRE(w.history_tools().begin_prefix(std::size_t(found - history.begin()) + 1));
  settle();
  REQUIRE(w.session().history_prefix_ready());
}
} // namespace

TEST_CASE("Hole history selectors retain hidden and consumed predecessor targets without changing "
          "visibility",
          "[gui][parity-b157][hole-input-history-shell]") {
  const int mode = GENERATE(0, 1, 2);
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = hidden_hole_input(mode);
  open(w, dir, part);
  hide_input_presentation(w, mode, "body.base");
  const auto before = snapshot(w);
  if (mode == 2)
    CHECK_FALSE(w.session().part_shape_cache()->find_body_shape(BodyId("body.base")));
  begin(w, "holes");
  auto& target = field<QComboBox>(w, "/target_body");
  CHECK(target.currentData().toString() == "body.base");
  CHECK(target.currentText().contains(mode == 2 ? "input history" : "hidden input"));
  CHECK(target.findData("body.other") == -1);
  auto& supports = field<QComboBox>(w, "/support");
  const auto bottom = supports.findData("finishing/face/4:base/bottom");
  REQUIRE(bottom >= 0);
  for (int i = 0; i < supports.count(); ++i)
    CHECK_FALSE(supports.itemData(i).toString().contains("later.stock"));
  supports.setCurrentIndex(bottom);
  settle();
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(std::get<SemanticFaceReference>(
            w.session().part_document()->hole_features().front().support().source())
            .face() == SemanticFace::Bottom);
  CHECK(w.session().part_document()->find_body(BodyId("body.base"))->visibility() ==
        part.find_body(BodyId("body.base"))->visibility());
  CHECK(w.session().presentation_visible("body/body.base") == (mode != 1));
  CHECK(volume(w, mode == 2 ? "body.other" : "body.base") ==
        Catch::Approx((mode == 2 ? 67200 : 38400) - 13.5 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE(
    "Thread history selectors use an available predecessor after its result is hidden or consumed",
    "[gui][parity-b157][hole-input-history-shell]") {
  const int mode = GENERATE(0, 1, 2);
  ShellWindow w;
  QTemporaryDir dir;
  const auto part = hidden_thread_input(mode);
  open(w, dir, part);
  hide_input_presentation(w, mode, "body.shaft");
  const auto before = snapshot(w);
  if (mode == 2)
    CHECK_FALSE(w.session().part_shape_cache()->find_body_shape(BodyId("body.shaft")));
  begin(w, "external");
  CHECK(field<QComboBox>(w, "/target_body").currentData().toString() == "body.shaft");
  CHECK(field<QComboBox>(w, "/target_body")
            .currentText()
            .contains(mode == 2 ? "input history" : "hidden input"));
  REQUIRE(field<QComboBox>(w, "/support").count() > 0);
  auto& representation = field<QComboBox>(w, "/thread/representation");
  representation.setCurrentIndex(representation.findData("modeled"));
  CHECK(snapshot(w) == before);
  REQUIRE(w.feature_edit_tools().apply());
  CHECK(w.session().part_document()->find_body(BodyId("body.shaft"))->visibility() ==
        part.find_body(BodyId("body.shaft"))->visibility());
  CHECK(w.session().presentation_visible("body/body.shaft") == (mode != 1));
  CHECK(volume(w, mode == 2 ? "body.base" : "body.shaft") ==
        Catch::Approx((mode == 2 ? 2560 : 0) + 200 * std::numbers::pi - modeled_external_removal())
            .margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE("Native Hole input picks temporarily expose hidden and consumed geometry and restore it "
          "on Cancel",
          "[gui][parity-b157][hole-input-history-shell][native-hole-input-history]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native hidden/predecessor Hole picks");
  const int mode = GENERATE(0, 2);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, hidden_hole_input(mode));
  const auto before = snapshot(w);
  const auto scope = w.viewport().selection_semantic_scope();
  const auto camera = w.viewport().camera_bookmark();
  begin(w, "holes");
  activate(w);
  w.viewport().set_standard_view(GuiStandardView::Bottom);
  click(control<QToolButton>(w, "blcad.hole.support.pick"));
  REQUIRE(w.viewport().feature_preview_active());
  w.viewport().fit_all();
  settle();
  const auto point = checked(w.viewport().model_to_screen({-20, -15, 0}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK(field<QComboBox>(w, "/support").currentData() == "finishing/face/4:base/bottom");
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "hole-hidden-input-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.cancel"));
  CHECK(snapshot(w) == before);
  CHECK(w.viewport().selection_semantic_scope() == scope);
  const auto restored_camera = w.viewport().camera_bookmark();
  CHECK(restored_camera.eye.x == Catch::Approx(camera.eye.x).margin(1e-6));
  CHECK(restored_camera.eye.y == Catch::Approx(camera.eye.y).margin(1e-6));
  CHECK(restored_camera.eye.z == Catch::Approx(camera.eye.z).margin(1e-6));
  CHECK(restored_camera.target.x == Catch::Approx(camera.target.x).margin(1e-6));
  CHECK(restored_camera.target.y == Catch::Approx(camera.target.y).margin(1e-6));
  CHECK(restored_camera.target.z == Catch::Approx(camera.target.z).margin(1e-6));
  CHECK(restored_camera.scale == Catch::Approx(camera.scale).margin(1e-6));
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK_FALSE(w.session().task().active());
  CHECK(w.session().part_document()->find_body(BodyId("body.base"))->visibility() ==
        (mode == 0 ? BodyVisibility::Hidden : BodyVisibility::Visible));
}

TEST_CASE("Native Thread input picks use the cylinder before later consumption and publish its "
          "modeled interval once",
          "[gui][parity-b157][hole-input-history-shell][native-hole-input-history]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native hidden/predecessor Thread picks");
  const int mode = GENERATE(0, 2);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, hidden_thread_input(mode));
  const auto before = snapshot(w);
  begin(w, "external");
  activate(w);
  w.viewport().set_standard_view(GuiStandardView::Right);
  click(control<QToolButton>(w, "blcad.thread.support.pick"));
  REQUIRE(w.viewport().feature_preview_active());
  w.viewport().fit_all();
  settle();
  const auto point = checked(w.viewport().model_to_screen({35, 0, 4}));
  click_at(w.viewport(), {point.x, point.y});
  CHECK_FALSE(control<QToolButton>(w, "blcad.thread.support.pick").isChecked());
  auto& representation = field<QComboBox>(w, "/thread/representation");
  representation.setCurrentIndex(representation.findData("modeled"));
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  capture(w, "thread-hidden-input-preview");
  click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  CHECK(volume(w, mode == 2 ? "body.base" : "body.shaft") ==
        Catch::Approx((mode == 2 ? 2560 : 0) + 200 * std::numbers::pi - modeled_external_removal())
            .margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE("Prefix Hole source edits and Create Another remain local until the outer Apply",
          "[gui][parity-b157][hole-prefix-shell]") {
  const bool authored = GENERATE(false, true);
  CAPTURE(authored);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, prefix_position_plate(authored));
  const auto before = snapshot(w);
  prefix_after(w, "base");
  REQUIRE(
      w.session().selection().add({GuiSelectionKind::SketchEntity, "sketch/positions/entity/a"}));
  create_hole(w);
  REQUIRE(w.session().part_history_command_active());
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().begin(FeatureId("later")));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  control<QPushButton>(w, "blcad.hole.edit_source/placements/0").click();
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  move_position_source(w, 10);
  const auto edited = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.redo());
  CHECK(snapshot(w) == edited);
  const auto saved = dir.filePath("prefix-source.blcad.json");
  REQUIRE(w.save_to(saved));
  GuiDocumentSession saved_session;
  REQUIRE(saved_session.open_file(saved.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*saved_session.part_document())) == before);
  control<QPushButton>(w, "blcad.hole.breadcrumb").click();
  REQUIRE_FALSE(w.active_sketch());
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(w.feature_edit_tools().apply(true));
  REQUIRE(w.feature_edit_tools().active());
  CHECK(w.session().history_prefix_count() == 2);
  REQUIRE(w.session().part_document()->hole_features().size() == 1);
  const auto created = w.session().part_document()->hole_features().front().id();
  const auto staged = snapshot(w);
  CHECK_FALSE(w.session().part_shape_cache()->find_body_shape(BodyId("body.later")));
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == staged);
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK(w.session().part_workflow_active());
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  CHECK(w.session().history_prefix_count() == 1);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == staged);
  const auto history = checked(w.session().part_document()->executable_history());
  REQUIRE(history.size() == 3);
  CHECK(history[1].node_id == created.value());
  CHECK(history[2].node_id == "later");
  control<QPushButton>(w, "blcad.history.prefix_apply").click();
  REQUIRE_FALSE(w.session().part_workflow_active());
  REQUIRE(w.session().derived_results_fresh());
  CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
  CHECK(volume(w, "body.later") == Catch::Approx(38400).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Prefix Hole and Thread edits cancel their child and retain the consumed-body suffix",
          "[gui][parity-b157][hole-prefix-shell]") {
  const bool thread = GENERATE(false, true);
  CAPTURE(thread);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, thread ? hidden_thread_input(2) : hidden_hole_input(2));
  const auto before = snapshot(w);
  prefix_after(w, thread ? "external" : "holes");
  const auto prefix = w.session().history_prefix_count();
  const auto constraints = w.session().sketch_constraint_catalogs();
  const auto dimensions = w.session().sketch_dimension_catalogs();
  for (const bool cancel : {true, false}) {
    begin(w, thread ? "external" : "holes");
    REQUIRE(w.session().part_history_command_active());
    CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
    if (thread) {
      auto& representation = field<QComboBox>(w, "/thread/representation");
      representation.setCurrentIndex(representation.findData("modeled"));
    } else {
      auto& support = field<QComboBox>(w, "/support");
      const int bottom = support.findData("finishing/face/4:base/bottom");
      REQUIRE(bottom >= 0);
      support.setCurrentIndex(bottom);
      field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(4);
    }
    if (cancel) {
      w.feature_edit_tools().cancel();
      CHECK(snapshot(w) == before);
      CHECK(w.session().history_prefix_count() == prefix);
      CHECK_FALSE(w.session().part_history_command_active());
      CHECK(w.session().part_workflow_active());
      CHECK(w.session().sketch_constraint_catalogs() == constraints);
      CHECK(w.session().sketch_dimension_catalogs() == dimensions);
    } else
      REQUIRE(w.feature_edit_tools().apply());
  }
  REQUIRE_FALSE(w.session().part_history_command_active());
  CHECK(w.session().history_prefix_count() == prefix);
  const auto staged = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.session().can_undo());
  REQUIRE(w.redo());
  CHECK(snapshot(w) == staged);
  control<QPushButton>(w, "blcad.history.prefix_apply").click();
  REQUIRE(w.session().derived_results_fresh());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(volume(w, thread ? "body.base" : "body.other") ==
        Catch::Approx(thread ? 2560 + 200 * std::numbers::pi - modeled_external_removal()
                             : 67200 - 24 * std::numbers::pi)
            .margin(2e-4));
  replay(w, dir, before);
}

TEST_CASE(
    "Prefix Thread Create Another inserts before the existing suffix and cancels only its draft",
    "[gui][parity-b157][hole-prefix-shell]") {
  ShellWindow w;
  QTemporaryDir dir;
  auto part = hidden_thread_input(2);
  REQUIRE(part.apply_history_intent({{"base", "shaft", "external", "consume"}, {}}));
  open(w, dir, part);
  const auto before = snapshot(w);
  prefix_after(w, "shaft");
  REQUIRE(w.session().selection().add({GuiSelectionKind::Body, "body/body.shaft"}));
  create_thread(w);
  CHECK(w.session().part_history_command_active());
  REQUIRE(w.feature_edit_tools().apply(true));
  CHECK(w.session().history_prefix_count() == 3);
  REQUIRE(w.feature_edit_tools().active());
  REQUIRE(w.session().part_document()->thread_features().size() == 2);
  const auto staged = snapshot(w);
  w.feature_edit_tools().cancel();
  CHECK_FALSE(w.session().part_history_command_active());
  CHECK(snapshot(w) == staged);
  const auto history = checked(w.session().part_document()->executable_history());
  CHECK(history[3].node_id == "external");
  CHECK(history[4].node_id == "consume");
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == staged);
  // Prefix publication deliberately defers the later Thread's physical
  // overlap check. A failed root Apply must retain the entire correctable draft.
  control<QPushButton>(w, "blcad.history.prefix_apply").click();
  CHECK_FALSE(w.session().derived_results_fresh());
  CHECK(w.session().history_prefix_ready());
  CHECK(snapshot(w) == staged);
  CHECK(control<QLabel>(w, "blcad.history.prefix_status").text().contains("overlapping thread"));
  begin(w, history[2].node_id.c_str());
  control<QCheckBox>(w, "blcad.thread.start.enabled").setChecked(true);
  settle();
  field<QDoubleSpinBox>(w, "/thread/start").setValue(4);
  field<QDoubleSpinBox>(w, "/thread/length").setValue(2);
  REQUIRE(w.feature_edit_tools().apply());
  control<QPushButton>(w, "blcad.history.prefix_apply").click();
  REQUIRE(w.session().derived_results_fresh());
  CHECK(volume(w) == Catch::Approx(2560 + 200 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}

TEST_CASE("Native prefix Hole source return and cancellation preserve the parent command boundary",
          "[gui][parity-b157][native-hole-prefix]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native prefix picking and Source Sketch return");
  const bool cancel = GENERATE(true, false);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, prefix_position_plate());
  const auto before = snapshot(w);
  const auto constraints = w.session().sketch_constraint_catalogs();
  const auto dimensions = w.session().sketch_dimension_catalogs();
  prefix_after(w, "base");
  create_hole(w);
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  reference_position(w, "/placements/0", {"positions", "a"});
  click(control<QToolButton>(w, "blcad.hole.position/placements/0.pick"));
  const auto screen = checked(w.viewport().model_to_screen({15, 0, 8}));
  click_at(w.viewport(), {screen.x, screen.y});
  REQUIRE(field<QComboBox>(w, "/placements/0/point").currentData() ==
          QStringList{"positions", "b"});
  click(control<QPushButton>(w, "blcad.hole.edit_source/placements/0"));
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  activate(w);
  auto& point = control<QAction>(w, "sketch.create.point");
  REQUIRE(point.isEnabled());
  point.trigger();
  settle();
  const auto source_screen = checked(w.viewport().sketch_plane_to_screen({10, -15}));
  click_at(w.viewport(), {source_screen.x, source_screen.y});
  REQUIRE(w.session().part_document()->find_sketch(SketchId("positions"))->points().size() == 3);
  CHECK_FALSE(control<QPushButton>(w, "blcad.history.prefix_apply").isEnabled());
  if (cancel) {
    w.feature_edit_tools().cancel();
    CHECK_FALSE(w.active_sketch());
    CHECK_FALSE(w.session().task().active());
    CHECK_FALSE(w.session().part_history_command_active());
    CHECK(w.session().history_prefix_count() == 1);
    CHECK(snapshot(w) == before);
    CHECK(w.session().sketch_constraint_catalogs() == constraints);
    CHECK(w.session().sketch_dimension_catalogs() == dimensions);
    CHECK_FALSE(w.session().can_undo());
    w.history_tools().cancel();
    CHECK_FALSE(w.session().part_workflow_active());
  } else {
    click(control<QPushButton>(w, "blcad.hole.breadcrumb"));
    REQUIRE_FALSE(w.active_sketch());
    capture(w, "hole-prefix-source-return");
    click(control<QPushButton>(w, "blcad.feature_edit.apply"));
    REQUIRE_FALSE(w.feature_edit_tools().active());
    CHECK(w.session().history_prefix_count() == 2);
    CHECK(w.session().part_workflow_active());
    click(control<QPushButton>(w, "blcad.history.prefix_apply"));
    REQUIRE(w.session().derived_results_fresh());
    CHECK(w.session().part_document()->find_sketch(SketchId("positions"))->points().size() == 3);
    CHECK(volume(w) == Catch::Approx(38400 - 8 * std::numbers::pi).margin(1e-6));
    replay(w, dir, before);
  }
}

TEST_CASE("Hole and Thread guards preserve unrelated workspace workflow and stale document edits",
          "[gui][parity-b157][hole-context-shell]") {
  const bool thread = GENERATE(false, true);
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, thread ? hidden_thread_input(2) : hidden_hole_input(2));
  const auto before = snapshot(w);
  for (const auto workspace : {GuiWorkspace::Assembly, GuiWorkspace::Inspect,
                               GuiWorkspace::Exchange, GuiWorkspace::Sketch}) {
    REQUIRE(w.session().set_workspace(workspace));
    w.refresh();
    CHECK_FALSE(control<QAction>(w, "blcad.action.semantic_hole").isEnabled());
    CHECK_FALSE(control<QAction>(w, "blcad.action.semantic_thread").isEnabled());
    CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
    CHECK_FALSE(w.feature_edit_tools().begin_new_thread());
    CHECK_FALSE(w.feature_edit_tools().begin(FeatureId(thread ? "external" : "holes")));
    CHECK_FALSE(w.feature_edit_tools().active());
    CHECK_FALSE(w.session().task().active());
    CHECK(snapshot(w) == before);
  }
  REQUIRE(w.session().set_workspace(GuiWorkspace::Part));
  REQUIRE(w.session().begin_part_workflow());
  CHECK_FALSE(w.feature_edit_tools().begin(FeatureId(thread ? "external" : "holes")));
  CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
  CHECK_FALSE(w.feature_edit_tools().begin_new_thread());
  CHECK(w.session().part_workflow_active());
  CHECK_FALSE(w.session().task().active());
  w.session().cancel_part_workflow();
  w.refresh();
  begin(w, thread ? "external" : "holes");
  REQUIRE(w.session().commit_part_transaction("Independent source change", [](PartDocument& part) {
    auto changed = part.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(81)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto changed = snapshot(w);
  REQUIRE(changed != before);
  CHECK_FALSE(w.feature_edit_tools().apply());
  w.feature_edit_tools().cancel();
  CHECK(snapshot(w) == changed);
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.session().part_workflow_active());
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
}

TEST_CASE("Immutable STEP Reference Parts reject Hole and Thread authoring without taking a task",
          "[gui][parity-b157][hole-context-shell]") {
  QTemporaryDir dir;
  const auto editable = imported_plate(dir);
  const auto& old = editable.step_import_sources().front();
  auto source =
      checked(StepImportSource::create(old.id(), old.name(), old.project_relative_asset_path(),
                                       old.sha256_digest(), StepImportMode::Reference));
  const auto reference = checked(geometry::StepReferenceCommands{}.create_definition(
      source, editable.imported_topology_catalogs().front(), DocumentId("reference.hole"),
      "Immutable plate", std::filesystem::path(dir.path().toStdString())));
  ShellWindow w;
  open(w, dir, reference);
  REQUIRE(w.session().part_document()->is_reference_part());
  const auto before = snapshot(w);
  CHECK_FALSE(control<QAction>(w, "blcad.action.semantic_hole").isEnabled());
  CHECK_FALSE(control<QAction>(w, "blcad.action.semantic_thread").isEnabled());
  CHECK_FALSE(w.feature_edit_tools().begin_new_hole());
  CHECK_FALSE(w.feature_edit_tools().begin_new_thread());
  CHECK_FALSE(w.feature_edit_tools().begin(FeatureId("import.base")));
  CHECK_FALSE(w.feature_edit_tools().active());
  CHECK_FALSE(w.session().task().active());
  CHECK_FALSE(w.session().part_workflow_active());
  CHECK(snapshot(w) == before);
}

TEST_CASE("Source return rebases the pending Hole recipe and limit by identity before another Hole",
          "[gui][parity-b157][hole-canonical-rebase-shell]") {
  const bool to_face = GENERATE(false, true);
  CAPTURE(to_face);
  auto part = point_plate();
  REQUIRE(part.add_parameter(checked(Parameter::create_length(
      ParameterId("suffix.diameter"), "Suffix diameter", checked(Quantity::length_mm(2))))));
  const auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      FeatureId("suffix.hole"), "Existing suffix Hole", HoleKind::Plain, BodyId("body.base"),
      support, {{HolePlacementId("suffix.position"), Point2{15, 0}}},
      ParameterId("suffix.diameter")))));
  REQUIRE(part.apply_history_intent({{"base", "suffix.hole"}, {}}));
  ShellWindow w;
  QTemporaryDir dir;
  open(w, dir, part);
  const auto before = snapshot(w);
  const auto suffix_record = [&](const std::string& text) {
    const auto root = nlohmann::json::parse(text);
    for (const auto& row : root.at("hole_features"))
      if (row.at("id") == "suffix.hole")
        return row;
    FAIL("Missing suffix Hole");
    return nlohmann::json{};
  };
  const auto original_suffix = suffix_record(before);
  prefix_after(w, "base");
  REQUIRE(
      w.session().selection().add({GuiSelectionKind::SketchEntity, "sketch/positions/entity/a"}));
  create_hole(w);
  auto& name = field<QLineEdit>(w, "/name");
  name.setText("Pending prefix seat");
  REQUIRE(QMetaObject::invokeMethod(&name, "editingFinished"));
  field<QDoubleSpinBox>(w, "/diameter/parameter").setValue(2);
  if (to_face) {
    choose_hole_mode(w, "blcad.hole.extent", 3);
    auto& limits = field<QComboBox>(w, "/extent/face");
    const int bottom = limits.findData("finishing/face/4:base/bottom");
    REQUIRE(bottom >= 0);
    limits.setCurrentIndex(bottom);
    settle();
  }
  if (QApplication::platformName() == "xcb")
    click(control<QPushButton>(w, "blcad.hole.edit_source/placements/0"));
  else
    control<QPushButton>(w, "blcad.hole.edit_source/placements/0").click();
  REQUIRE(w.feature_edit_tools().editing_source_sketch());
  if (QApplication::platformName() == "xcb") {
    activate(w);
    auto& point = control<QAction>(w, "sketch.create.point");
    REQUIRE(point.isEnabled());
    point.trigger();
    settle();
    const auto screen = checked(w.viewport().sketch_plane_to_screen({10, -15}));
    click_at(w.viewport(), {screen.x, screen.y});
    REQUIRE(w.session().part_document()->find_sketch(SketchId("positions"))->points().size() == 3);
    click(control<QPushButton>(w, "blcad.hole.breadcrumb"));
  } else {
    move_position_source(w, 10);
    control<QPushButton>(w, "blcad.hole.breadcrumb").click();
  }
  REQUIRE_FALSE(w.active_sketch());
  const auto source_state = snapshot(w);
  CHECK(suffix_record(source_state) == original_suffix);
  if (QApplication::platformName() == "xcb") {
    auto& recipe = control<QComboBox>(w, "blcad.hole.recipe");
    recipe.setFocus();
    key(recipe, Qt::Key_Home);
    key(recipe, Qt::Key_Down);
    REQUIRE(control<QComboBox>(w, "blcad.hole.recipe").currentIndex() == 1);
  } else
    choose_hole_mode(w, "blcad.hole.recipe", 1);
  REQUIRE(field<QLineEdit>(w, "/name").text() == "Pending prefix seat");
  field<QDoubleSpinBox>(w, "/manufacturing/diameter/parameter").setValue(4);
  field<QDoubleSpinBox>(w, "/manufacturing/depth/parameter").setValue(2);
  INFO(control<QLabel>(w, "blcad.feature_edit.status").text().toStdString());
  REQUIRE(control<QPushButton>(w, "blcad.feature_edit.apply").isEnabled());
  CHECK(snapshot(w) == source_state);
  if (QApplication::platformName() == "xcb") {
    capture(w, to_face ? "hole-prefix-rebased-face-limit" : "hole-prefix-rebased-recipe");
    click(control<QPushButton>(w, "blcad.feature_edit.apply"));
  } else
    REQUIRE(w.feature_edit_tools().apply());
  REQUIRE_FALSE(w.feature_edit_tools().active());
  CHECK(w.session().history_prefix_count() == 2);
  CHECK(suffix_record(snapshot(w)) == original_suffix);
  if (QApplication::platformName() == "xcb")
    click(control<QPushButton>(w, "blcad.history.prefix_apply"));
  else
    control<QPushButton>(w, "blcad.history.prefix_apply").click();
  REQUIRE(w.session().derived_results_fresh());
  CHECK(suffix_record(snapshot(w)) == original_suffix);
  const auto& accepted = *w.session().part_document();
  REQUIRE(accepted.hole_features().size() == 2);
  const auto found =
      std::find_if(accepted.hole_features().begin(), accepted.hole_features().end(),
                   [](const auto& hole) { return hole.id() != FeatureId("suffix.hole"); });
  REQUIRE(found != accepted.hole_features().end());
  CHECK(found->name() == "Pending prefix seat");
  CHECK(std::holds_alternative<HoleCounterbore>(found->manufacturing()));
  CHECK(std::holds_alternative<HoleToFaceExtent>(found->extent()) == to_face);
  CHECK(volume(w) == Catch::Approx(38400 - 22 * std::numbers::pi).margin(1e-6));
  replay(w, dir, before);
}
