#include "../support/engineering_resource_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>
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
void click(QWidget& widget, QPoint point) {
  visible(widget);
  REQUIRE(widget.isEnabled());
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, widget.mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
    settle();
  }
}
void number(ShellWindow& window, const char* path, const QString& value) {
  auto& spin = field<QDoubleSpinBox>(window, path);
  auto& input = control<QLineEdit>(spin, "qt_spinbox_lineedit");
  text(input, value);
  key(input, Qt::Key_Return);
}
void choose(QComboBox& combo, int index) {
  REQUIRE(index >= 0);
  visible(combo);
  combo.setCurrentIndex(index);
  settle();
}
void search(ShellWindow& window, const QString& command) {
  bool found = false, timed_out = false;
  QTimer respond, watchdog;
  respond.setSingleShot(true);
  QObject::connect(&respond, &QTimer::timeout, &window, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog)
      return;
    auto* query = dialog->findChild<QLineEdit*>("blcad.command_search.query");
    auto* results = dialog->findChild<QTreeWidget*>("blcad.command_search.results");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (query && results && buttons) {
      query->setText(command);
      for (int i = 0; i < results->topLevelItemCount(); ++i)
        if (results->topLevelItem(i)->text(0) == command) {
          results->setCurrentItem(results->topLevelItem(i));
          if (buttons->button(QDialogButtonBox::Ok)->isEnabled()) {
            found = true;
            buttons->button(QDialogButtonBox::Ok)->click();
            return;
          }
        }
    }
    dialog->reject();
  });
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
    timed_out = true;
    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
      dialog->reject();
  });
  respond.start(20);
  watchdog.start(5000);
  window.ribbon().show_command_search();
  respond.stop();
  watchdog.stop();
  REQUIRE_FALSE(timed_out);
  REQUIRE(found);
  settle();
}
std::string project_snapshot(ShellWindow& window) {
  return checked(serialize_project_to_json(*window.session().project()));
}
void project_history(ShellWindow& window, const std::string& before) {
  const auto after = project_snapshot(window);
  REQUIRE(window.undo());
  CHECK(project_snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(project_snapshot(window) == after);
}
void assembly_reference(ShellAssemblyTools& tools, QComboBox& combo, const char* component,
                        const std::string& semantic) {
  const auto& targets = tools.targets();
  const auto target = std::find_if(targets.begin(), targets.end(), [&](const auto& value) {
    return value.reference.component_instance() == ComponentInstanceId(component) &&
           value.reference.semantic_reference() == semantic;
  });
  REQUIRE(target != targets.end());
  choose(combo, combo.findData(static_cast<int>(std::distance(targets.begin(), target))));
}
void button(ShellWindow& window, const char* id) {
  auto& value = control<QPushButton>(window, id);
  click(value, value.rect().center());
}
struct FileDialogs {
  bool original = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
  FileDialogs() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
  }
  ~FileDialogs() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, original);
  }
};
void structured_copy(ShellWindow& window, const QString& output) {
  FileDialogs dialogs;
  bool handled = false, timed_out = false;
  QTimer poll, watchdog;
  poll.setInterval(5);
  QObject::connect(&poll, &QTimer::timeout, &window, [&] {
    if (auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
      poll.stop();
      handled = true;
      chooser->selectFile(output);
      QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
    }
  });
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
    timed_out = true;
    if (auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
      chooser->reject();
  });
  poll.start();
  watchdog.start(5000);
  button(window, "blcad.step_import.apply");
  poll.stop();
  watchdog.stop();
  REQUIRE_FALSE(timed_out);
  REQUIRE(handled);
  settle();
}
void mapped_thread(ShellWindow& window, const QString& revision) {
  const auto select = [&](const char* name, const QVariant& value) {
    auto& combo = control<QComboBox>(window, name);
    choose(combo, combo.findData(value));
  };
  select("blcad.thread.source", "standard");
  select("blcad.thread.standard.table", QStringList{"synthetic.threads", revision});
  select("blcad.thread.standard.row", "synthetic.row");
  for (const auto& [field, column] : {std::pair{"system", "system"},
                                      {"designation", "designation"},
                                      {"tolerance", "tolerance"},
                                      {"major_diameter", "major"},
                                      {"minor_diameter", "minor"},
                                      {"pitch", "pitch"},
                                      {"included_angle", "angle"}})
    select(("blcad.thread.standard." + std::string(field)).c_str(), column);
}
void part_history(ShellWindow& window, const std::string& before) {
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}
void thread_samples(ShellWindow& window, bool internal, double pitch, ThreadHandedness hand) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(
      BodyId(internal ? "body.base" : "body.shaft"));
  REQUIRE(shape);
  unsigned cut = 0, solid = 0;
  for (double z : {.35, 1.05, 1.85, 2.7})
    for (double theta : {.23, 1.17, 2.39, 3.65, 5.31})
      for (double radius : {4.25, 4.75}) {
        const double phase = std::remainder(z - (hand == ThreadHandedness::Right ? 1. : -1.) *
                                                    pitch * theta / (2 * std::numbers::pi),
                                            pitch);
        const double width =
            (internal ? 5 - radius : radius - 4) * std::tan(55 * std::numbers::pi / 360);
        if (std::abs(std::abs(phase) - width) < .025)
          continue;
        const bool removed = std::abs(phase) < width;
        CAPTURE(internal, pitch, z, theta, radius, phase, width);
        const Point3 point{internal ? radius * std::cos(theta) : 30 + radius * std::cos(theta),
                           (internal ? -1 : 1) * radius * std::sin(theta),
                           internal ? 7 - z : 1 + z};
        CHECK(blcad::test::classify_solid_point(*shape, point) ==
              (removed ? blcad::test::SolidPointLocation::Outside
                       : blcad::test::SolidPointLocation::Inside));
        removed ? ++cut : ++solid;
      }
  CHECK(cut > 5);
  CHECK(solid > 5);
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

TEST_CASE("New native Hole positions continue through Pattern Assembly and structured STEP",
          "[gui][parity-b157][native-hole-thread-combined][hole-combined-b04]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Hole picking and combined OCCT interaction");
  auto stock = plate_fixture::part(false, false);
  REQUIRE(stock.remove_hole_feature(FeatureId("holes")));
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, stock);
  const auto stock_json = snapshot(window);
  search(window, "Hole");
  REQUIRE(window.feature_edit_tools().active());
  number(window, "/diameter/parameter", "3");
  number(window, "/placements/0/position/x", "-20");
  number(window, "/placements/0/position/y", "-15");
  choose(control<QComboBox>(window, "blcad.hole.recipe"), 1);
  choose(control<QComboBox>(window, "blcad.hole.extent"), 1);
  number(window, "/manufacturing/diameter/parameter", "5");
  number(window, "/manufacturing/depth/parameter", "1");
  auto& add = control<QPushButton>(window, "blcad.feature_edit.add/placements");
  click(add, add.rect().center());
  number(window, "/placements/1/position/x", "-20");
  number(window, "/placements/1/position/y", "-6");
  task_layout(window);
  CHECK(snapshot(window) == stock_json);
  capture(window, "combined-hole-preview");
  auto& apply = control<QPushButton>(window, "blcad.feature_edit.apply");
  click(apply, apply.rect().center());
  REQUIRE_FALSE(window.feature_edit_tools().active());
  CHECK(volume(window, "body.base") == Catch::Approx(38400 - 44 * std::numbers::pi).margin(1e-6));
  const auto hole_json = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == stock_json);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == hole_json);

  auto& patterns = window.feature_tools();
  REQUIRE(patterns.begin_pattern(ShellFeatureTools::Command::LinearPattern));
  choose(control<QComboBox>(window, "blcad.pattern.source_mode"), 1);
  auto& first_axis = patterns.pattern_reference_editor();
  choose(first_axis, first_axis.findText("axis.x"));
  REQUIRE(patterns.pattern_source_editor().count() == 1);
  window.viewport().set_standard_view(GuiStandardView::Isometric);
  window.viewport().fit_all();
  settle();
  for (const double theta : {0., .8, 1.6, 2.4, 3.2, 4., 4.8, 5.6}) {
    const auto screen = checked(window.viewport().model_to_screen(
        {-20 + 1.5 * std::cos(theta), -15 + 1.5 * std::sin(theta), 6.8}));
    click(window.viewport(), {qRound(screen.x), qRound(screen.y)});
    if (patterns.pattern_source_editor().selectedItems().size() == 1)
      break;
  }
  REQUIRE(patterns.pattern_source_editor().selectedItems().size() == 1);
  text(patterns.numeric_editor(), "12 mm");
  key(patterns.numeric_editor(), Qt::Key_Return);
  patterns.pattern_count_editor().setValue(3);
  auto& second_axis = control<QComboBox>(window, "blcad.pattern.second.reference");
  choose(second_axis, second_axis.findText("axis.y"));
  auto& second = control<QGroupBox>(window, "blcad.pattern.second.enabled");
  visible(second);
  key(second, Qt::Key_Space);
  REQUIRE(second.isChecked());
  control<QSpinBox>(window, "blcad.pattern.second.count").setValue(2);
  control<QDoubleSpinBox>(window, "blcad.pattern.second.value").setValue(15);
  auto& skipped = control<QListWidget>(window, "blcad.pattern.skipped");
  QListWidgetItem* excluded = nullptr;
  for (int i = 0; i < skipped.count(); ++i)
    if (skipped.item(i)->data(Qt::UserRole).toUInt() == 2 &&
        skipped.item(i)->data(Qt::UserRole + 1).toUInt() == 1)
      excluded = skipped.item(i);
  REQUIRE(excluded);
  visible(skipped);
  skipped.scrollToItem(excluded);
  click(*skipped.viewport(), {10, skipped.visualItemRect(excluded).center().y()});
  REQUIRE(excluded->checkState() == Qt::Checked);
  CHECK(snapshot(window) == hole_json);
  capture(window, "combined-hole-pattern-preview");
  auto& pattern_apply = control<QPushButton>(window, "blcad.feature.apply");
  click(pattern_apply, pattern_apply.rect().center());
  REQUIRE_FALSE(patterns.active());
  CHECK(volume(window, "body.base") == Catch::Approx(38400 - 220 * std::numbers::pi).margin(1e-6));
  const auto patterned_json = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == hole_json);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == patterned_json);
  REQUIRE(window.save_to(directory.filePath("authored.blcad.json")));
  GuiDocumentSession authored;
  REQUIRE(authored.open_file(directory.filePath("authored.blcad.json").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*authored.part_document())) == patterned_json);

  auto base = *authored.part_document();
  const auto hole = base.hole_features().front();
  const auto pattern = base.linear_pattern_features().front();
  REQUIRE(hole.placements().size() == 2);
  auto cover_json = nlohmann::json::parse(patterned_json);
  cover_json["document"]["id"] = "cover";
  auto cover = checked(deserialize_part_document_from_json(cover_json.dump()));
  REQUIRE(cover.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(4))));
  const auto& seat = std::get<HoleCounterbore>(hole.manufacturing());
  REQUIRE(std::holds_alternative<ParameterId>(seat.depth));
  REQUIRE(cover.set_parameter_value(std::get<ParameterId>(seat.depth),
                                    checked(Quantity::length_mm(.5))));
  REQUIRE(cover.add_construction_plane(checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("underside"), "Cover underside", {},
                                         {1, 0, 0}, {0, -1, 0}, {0, 0, -1}))));
  const auto underside = checked(PlaneReference::create_construction_plane(
      PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("underside")));
  REQUIRE(cover.update_hole_feature(checked(HoleFeature::create(
      hole.id(), hole.name(), hole.kind(), hole.target_body(), underside,
      {{hole.placements()[0].id, Point2{-20, 15}}, {hole.placements()[1].id, Point2{-20, 6}}},
      hole.diameter(), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal, hole.tip(),
      hole.manufacturing()))));
  namespace material = blcad::test::engineering_fixture;
  material::annotate(base);
  material::annotate(cover);
  REQUIRE(cover.set_default_material(material::key("synthetic.material", "r2")));
  auto root = checked(AssemblyDocument::create(DocumentId("combined.root"), "Plate and cover"));
  REQUIRE(root.add_parameter(
      checked(Parameter::create_count(ParameterId("driver"), "Hole count",
                                      checked(Quantity::count(3)), ParameterScope::Assembly))));
  for (const auto* part : {&base, &cover}) {
    REQUIRE(root.add_member_part(part->id()));
    REQUIRE(root.add_binding(checked(
        ParameterBinding::create(ParameterBindingId("binding." + part->id().value()), part->id(),
                                 pattern.count_parameter(), ParameterId("driver")))));
  }
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("base"), "Base", base.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("cover"), "Cover", cover.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {{0, 0, 11}, {}}))));
  auto project = checked(Project::create(DocumentId("combined.project"), "Combined B04", root));
  REQUIRE(project.add_part_document(base));
  REQUIRE(project.add_part_document(cover));
  const auto project_path = directory.filePath("assembly.blcad.project.json");
  REQUIRE(write_project_json_file(project, project_path.toStdString()));
  REQUIRE(window.open_path(project_path));
  window.viewport().fit_all();
  settle();
  auto& assembly = window.assembly_tools();
  for (const auto index : {PatternOccurrenceIndex{1, 1}, PatternOccurrenceIndex{2, 0}}) {
    const auto before = project_snapshot(window);
    const auto semantic = checked(make_hole_target_spelling(checked(
        HoleInstanceReference::create(hole.id(), hole.placements()[0].id, HoleReferenceRole::Entry,
                                      HolePatternOccurrence{pattern.id(), index}))));
    REQUIRE(assembly.begin(ShellAssemblyTools::Command::Relationship));
    choose(assembly.relationship_editor(), static_cast<int>(AssemblyConstraintType::Insert));
    assembly_reference(assembly, assembly.first_editor(), "base", semantic);
    assembly_reference(assembly, assembly.second_editor(), "cover", semantic);
    REQUIRE(window.viewport().feature_preview_active());
    CHECK(project_snapshot(window) == before);
    capture(window, "combined-hole-assembly-preview");
    auto& assembly_apply = control<QPushButton>(window, "blcad.assembly.apply");
    click(assembly_apply, assembly_apply.rect().center());
    REQUIRE_FALSE(assembly.active());
    project_history(window, before);
  }
  const auto pose = window.session()
                        .project()
                        ->assembly()
                        .find_component_instance(ComponentInstanceId("cover"))
                        ->transform();
  const auto point =
      geometry::AssemblyHierarchyTransformEvaluator{}.evaluate_point({pose}, {4, 0, 0});
  CHECK(point.x == Catch::Approx(4).margin(1e-6));
  CHECK(point.y == Catch::Approx(0).margin(1e-6));
  CHECK(point.z == Catch::Approx(8).margin(1e-6));
  const auto expected_mass = [](unsigned holes) {
    return (38400 - holes * 22 * std::numbers::pi) * 4e-6 +
           (19200 - holes * 11 * std::numbers::pi) * 6e-6;
  };
  const auto mass = [&] {
    const auto report = checked(
        geometry::BodyResultInspector{}.inspect_physical_properties(*window.session().project()));
    REQUIRE(report.data().complete_mass());
    return report.data().complete_mass()->mass_kg;
  };
  CHECK(mass() == Catch::Approx(expected_mass(10)).margin(1e-9));
  const auto before_count = project_snapshot(window);
  REQUIRE(window.session().commit_project_transaction("Set shared Hole count", [](Project& value) {
    auto result =
        value.set_assembly_parameter_value(ParameterId("driver"), checked(Quantity::count(2)));
    return result ? Result<std::size_t>::success(1) : Result<std::size_t>::failure(result.error());
  }));
  window.refresh();
  REQUIRE(window.session().project()->assembly().constraint_count() == 2);
  const auto& constraints = window.session().project()->assembly().constraints();
  REQUIRE(geometry::AssemblyInsertConstraintEquationBuilder{}.build(*window.session().project(),
                                                                    constraints[0]));
  CHECK_FALSE(geometry::AssemblyInsertConstraintEquationBuilder{}.build(*window.session().project(),
                                                                        constraints[1]));
  CHECK(mass() == Catch::Approx(expected_mass(8)).margin(1e-9));
  project_history(window, before_count);
  const auto accepted = project_snapshot(window);
  REQUIRE(window.save_to(project_path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(project_path.toStdString()));
  CHECK(checked(serialize_project_to_json(*reopened.project())) == accepted);
  const auto step_path = directory.filePath("roundtrip.step");
  REQUIRE(window.inspect_tools().export_step(
      {GuiStepExportMode::AssemblyStructured, step_path.toStdString()}));
  CHECK(project_snapshot(window) == accepted);
  QFile step(step_path);
  REQUIRE(step.open(QIODevice::ReadOnly));
  const auto step_bytes = step.readAll();
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("roundtrip"), "Roundtrip", "roundtrip.step",
      checked(step_import_sha256(std::string_view(step_bytes.constData(), step_bytes.size()))),
      StepImportMode::EditableBody));
  const auto imported =
      checked(geometry::StepImportReader{}.read(source, directory.path().toStdString()));
  REQUIRE(imported.body_count() == 2);
  std::vector<double> volumes;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      volumes.push_back(body.volume_mm3);
  std::sort(volumes.begin(), volumes.end());
  CHECK(volumes[0] == Catch::Approx(19200 - 88 * std::numbers::pi).margin(1e-6));
  CHECK(volumes[1] == Catch::Approx(38400 - 176 * std::numbers::pi).margin(1e-6));
  CHECK(
      std::any_of(imported.occurrences.begin(), imported.occurrences.end(), [](const auto& value) {
        return std::abs(value.transform.matrix[11] - 8) < 1e-6;
      }));
  capture(window, "combined-hole-step-accepted");

  REQUIRE(window.step_import_tools().begin());
  text(control<QLineEdit>(window, "blcad.step_import.name"), "Authored plate and cover");
  text(control<QLineEdit>(window, "blcad.step_import.path"), "roundtrip.step");
  auto& mode = control<QComboBox>(window, "blcad.step_import.mode");
  choose(mode, mode.findData(static_cast<int>(StepImportMode::EditableBody)));
  button(window, "blcad.step_import.preview");
  button(window, "blcad.step_import.apply");
  REQUIRE_FALSE(window.step_import_tools().active());
  REQUIRE(window.save_to(project_path));
  const auto registered = project_snapshot(window);
  const auto history = std::string(window.session().undo_label());
  const auto registered_source = window.session().project()->step_import_sources().back().id();
  REQUIRE(window.step_import_tools().begin());
  auto& sources = control<QComboBox>(window, "blcad.step_import.source");
  choose(sources, sources.findData(QString::fromStdString(registered_source.value())));
  choose(control<QComboBox>(window, "blcad.step_import.reference_operation"), 4);
  text(control<QLineEdit>(window, "blcad.step_import.reference_name"), "Roundtrip plate and cover");
  button(window, "blcad.step_import.reference_prepare");
  REQUIRE(window.viewport().feature_preview_count() == 2);
  capture(window, "combined-hole-step-import-preview");
  const auto copy = directory.filePath("roundtrip.blcad.project.json");
  structured_copy(window, copy);
  REQUIRE_FALSE(window.step_import_tools().active());
  REQUIRE(QFile::exists(copy));
  CHECK(project_snapshot(window) == registered);
  CHECK(window.session().file_path() == project_path.toStdString());
  CHECK(window.session().undo_label() == history);
  CHECK_FALSE(window.session().dirty());
  REQUIRE(window.open_path(copy));
  const auto* roundtrip = window.session().project();
  REQUIRE(roundtrip);
  REQUIRE(roundtrip->part_document_count() == 2);
  std::vector<double> reopened_volumes, heights;
  for (const auto& part : roundtrip->part_documents()) {
    REQUIRE_FALSE(part.is_reference_part());
    REQUIRE(part.hole_features().empty()); // STEP carries solids, not native feature recipes.
    const auto* cache = window.session().project_part_shape_cache(part.id());
    REQUIRE(cache);
    for (const auto& body : cache->body_shapes())
      reopened_volumes.push_back(
          geometry::RectangleExtrusionAdapter{}.summarize(body.shape).volume_mm3);
  }
  const auto leaves = checked(AssemblyLeafOccurrenceResolver{}.resolve(*roundtrip));
  REQUIRE(leaves.size() == 2);
  for (const auto& leaf : leaves)
    heights.push_back(geometry::AssemblyHierarchyTransformEvaluator{}
                          .evaluate_point(leaf.transforms_inner_to_outer, {})
                          .z);
  std::sort(reopened_volumes.begin(), reopened_volumes.end());
  std::sort(heights.begin(), heights.end());
  REQUIRE(reopened_volumes.size() == 2);
  CHECK(reopened_volumes[0] == Catch::Approx(volumes[0]).margin(1e-6));
  CHECK(reopened_volumes[1] == Catch::Approx(volumes[1]).margin(1e-6));
  CHECK(heights[0] == Catch::Approx(0).margin(1e-6));
  CHECK(heights[1] == Catch::Approx(8).margin(1e-6));
  const auto reopened_json = project_snapshot(window);
  REQUIRE(window.save_to(copy));
  GuiDocumentSession portable;
  REQUIRE(portable.open_file(copy.toStdString()));
  CHECK(checked(serialize_project_to_json(*portable.project())) == reopened_json);
  capture(window, "combined-hole-step-reopened");
}

TEST_CASE("Native internal and external Threads combine pins hands interval correction and "
          "representations",
          "[gui][parity-b157][native-hole-thread-combined][thread-combined-b03]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native combined Thread authoring and OCCT solids");
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, thread_fixture::pair_part());
  for (bool internal : {true, false}) {
    CAPTURE(internal);
    const auto before = snapshot(window);
    search(window, internal ? "Hole" : "Thread");
    REQUIRE(window.feature_edit_tools().active());
    if (internal) {
      number(window, "/diameter/parameter", "8");
      choose(control<QComboBox>(window, "blcad.hole.extent"), 0);
      number(window, "/extent/depth", "6");
      choose(control<QComboBox>(window, "blcad.hole.internal_thread"), 1);
    }
    mapped_thread(window, "r1");
    auto& offset = control<QCheckBox>(window, "blcad.thread.start.enabled");
    click(offset, offset.rect().center());
    number(window, "/thread/start", "1");
    number(window, "/thread/length", "20");
    REQUIRE_FALSE(control<QPushButton>(window, "blcad.feature_edit.apply").isEnabled());
    CHECK(snapshot(window) == before);
    number(window, "/thread/length", "3");
    choose(field<QComboBox>(window, "/thread/handedness"), 1);
    task_layout(window);
    button(window, "blcad.feature_edit.apply");
    REQUIRE_FALSE(window.feature_edit_tools().active());
    const auto id = internal ? window.session().part_document()->hole_features().front().id()
                             : window.session().part_document()->thread_features().front().id();
    const auto* body = internal ? "body.base" : "body.shaft";
    const double baseline = internal ? 2560 - 96 * std::numbers::pi : 200 * std::numbers::pi;
    CHECK(volume(window, body) == Catch::Approx(baseline).margin(1e-6));
    part_history(window, before);
    for (const auto& [revision, hand] :
         {std::pair{"r1", ThreadHandedness::Left}, {"r2", ThreadHandedness::Right}}) {
      CAPTURE(revision);
      const auto previous = snapshot(window);
      REQUIRE(window.feature_edit_tools().begin(id));
      mapped_thread(window, revision);
      choose(field<QComboBox>(window, "/thread/handedness"),
             hand == ThreadHandedness::Left ? 1 : 0);
      choose(field<QComboBox>(window, "/thread/representation"), 1);
      task_layout(window);
      capture(window,
              internal ? "combined-internal-thread-preview" : "combined-external-thread-preview");
      button(window, "blcad.feature_edit.apply");
      REQUIRE_FALSE(window.feature_edit_tools().active());
      const double pitch = std::string_view(revision) == "r1" ? 1.7 : 2.2;
      const double removed = 4 * std::numbers::pi * std::tan(55 * std::numbers::pi / 360) * 3 /
                             pitch * (internal ? (5. / 2 - 1. / 3) : (4. / 2 + 1. / 3));
      CHECK(volume(window, body) == Catch::Approx(baseline - removed).margin(2e-4));
      thread_samples(window, internal, pitch, hand);
      const auto& part = *window.session().part_document();
      const auto& definition =
          internal ? *part.find_hole_feature(id)->thread() : part.find_thread_feature(id)->thread();
      CHECK(std::get<ThreadStandardProfile>(definition.profile()).table ==
            thread_fixture::selected(revision).table);
      CHECK(definition.handedness() == hand);
      if (internal)
        CHECK(part.find_parameter(
                      std::get<HoleBlindExtent>(part.find_hole_feature(id)->extent()).depth)
                  ->value()
                  .millimeters() == 6);
      part_history(window, previous);
    }
    const auto modeled = snapshot(window);
    REQUIRE(window.feature_edit_tools().begin(id));
    choose(field<QComboBox>(window, "/thread/representation"), 0);
    button(window, "blcad.feature_edit.apply");
    CHECK(volume(window, body) == Catch::Approx(baseline).margin(1e-6));
    part_history(window, modeled);
  }
  const auto final = snapshot(window);
  const auto path = directory.filePath("combined-threads.blcad.json");
  REQUIRE(window.save_to(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) == final);
  REQUIRE(reopened.part_document()->hole_features().size() == 1);
  REQUIRE(reopened.part_document()->thread_features().size() == 1);
  capture(window, "combined-threads-cosmetic-accepted");
}
