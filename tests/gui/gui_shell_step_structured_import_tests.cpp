#include "gui_step_structured_import_fixture.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <functional>
#include <tuple>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;
using structured_test::checked;
namespace {
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
void write(const QString& path, const QByteArray& data) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(file.write(data) == data.size());
}
class StructuredUi {
public:
  StructuredUi() : native_dialogs_(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs)) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    window.resize(1280, 800);
    window.show();
    flush();
    REQUIRE(window.viewport().native_viewer_available());
  }
  ~StructuredUi() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, native_dialogs_);
  }
  void flush() {
    qApp->processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    qApp->processEvents();
  }
  template <class T> T& widget(const QString& id) {
    auto* result = window.findChild<T*>(id);
    INFO(id.toStdString());
    REQUIRE(result);
    return *result;
  }
  template <class T> T& field(const char* id) {
    return widget<T>(QStringLiteral("blcad.step_import.") + id);
  }
  void reveal(QWidget& control) {
    flush();
    for (auto* parent = control.parentWidget(); parent; parent = parent->parentWidget())
      if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
        const auto center = control.mapTo(scroll->widget(), control.rect().center());
        scroll->ensureVisible(center.x(), center.y(),
                              std::min(control.width() / 2 + 8, scroll->viewport()->width() / 2),
                              std::min(control.height() / 2 + 8, scroll->viewport()->height() / 2));
      }
    flush();
    REQUIRE(control.isVisible());
    const auto* hit = window.childAt(control.mapTo(&window, control.rect().center()));
    INFO(control.objectName().toStdString());
    REQUIRE((hit == &control || (hit && control.isAncestorOf(hit))));
  }
  void click_at(QWidget& control) {
    const QPointF point = control.rect().center();
    QMouseEvent press(QEvent::MouseButtonPress, point, control.mapToGlobal(point.toPoint()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, control.mapToGlobal(point.toPoint()),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&control, &press);
    QApplication::sendEvent(&control, &release);
    flush();
  }
  void click(QWidget& control) {
    REQUIRE(control.isEnabled());
    reveal(control);
    click_at(control);
  }
  void key(QWidget& control, int code, const QString& text = {},
           Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, code, mods, text),
        release(QEvent::KeyRelease, code, mods, text);
    QApplication::sendEvent(&control, &press);
    QApplication::sendEvent(&control, &release);
    flush();
  }
  void text(QLineEdit& control, const QString& value) {
    click(control);
    control.setFocus(Qt::MouseFocusReason);
    key(control, Qt::Key_A, {}, Qt::ControlModifier);
    key(control, 0, value);
    key(control, Qt::Key_Tab);
  }
  void choose(QComboBox& control, int index) {
    INFO(control.objectName().toStdString());
    REQUIRE(index >= 0);
    REQUIRE(index < control.count());
    REQUIRE(control.model()
                ->index(index, control.modelColumn(), control.rootModelIndex())
                .flags()
                .testFlag(Qt::ItemIsEnabled));
    reveal(control);
    control.setFocus();
    key(control, Qt::Key_Home);
    for (int i = 0; i < control.count() && control.currentIndex() != index; ++i)
      key(control, Qt::Key_Down);
    REQUIRE(control.currentIndex() == index);
  }
  void button(const char* id) {
    click(field<QPushButton>(id));
  }
  void command(const QString& id) {
    auto& action = widget<QAction>("blcad.action." + id);
    REQUIRE(action.isEnabled());
    window.ribbon().set_current_tab(id.startsWith("export_assembly") ? 3 : 0);
    flush();
    for (auto* button : window.ribbon().findChildren<QToolButton*>()) {
      if (!button->isVisible())
        continue;
      if (button->defaultAction() == &action) {
        click(*button);
        return;
      }
      if (auto* menu = button->menu(); menu && menu->actions().contains(&action)) {
        reveal(*button);
        bool chosen = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&action, &QAction::triggered, &watchdog, [&] { chosen = true; });
        QObject::connect(&watchdog, &QTimer::timeout, menu, [menu] { menu->close(); });
        QTimer::singleShot(0, menu, [&, menu] {
          menu->setActiveAction(&action);
          key(*menu, Qt::Key_Return);
        });
        watchdog.start(5000);
        click_at(*button);
        watchdog.stop();
        REQUIRE(chosen);
        return;
      }
    }
    FAIL("Missing visible ribbon action " + id.toStdString());
  }
  void dialog(const std::function<void()>& invoke,
              const std::function<void(QFileDialog&)>& respond) {
    bool handled = false, timeout = false;
    QTimer poll, watchdog;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
      if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
        poll.stop();
        handled = true;
        respond(*dialog);
      }
    });
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
      timeout = true;
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        dialog->reject();
    });
    poll.start();
    watchdog.start(5000);
    invoke();
    poll.stop();
    watchdog.stop();
    REQUIRE_FALSE(timeout);
    REQUIRE(handled);
    flush();
  }
  void file(const QString& command_id, const QString& path) {
    dialog([&] { command(command_id); },
           [&](QFileDialog& chooser) {
             chooser.selectFile(path);
             QMetaObject::invokeMethod(&chooser, "accept", Qt::DirectConnection);
           });
  }
  void save_copy(const QString& path, bool accept = true) {
    dialog([&] { button("apply"); },
           [&](QFileDialog& chooser) {
             if (!accept) {
               key(chooser, Qt::Key_Escape);
               return;
             }
             chooser.selectFile(path);
             QMetaObject::invokeMethod(&chooser, "accept", Qt::DirectConnection);
           });
  }
  std::string snapshot() const {
    const auto& session = window.session();
    return session.project() ? checked(serialize_project_to_json(*session.project()))
                             : checked(serialize_part_document_to_json(*session.part_document()));
  }
  void begin() {
    command("step_sources");
    REQUIRE(window.step_import_tools().active());
  }
  void prepare(const QString& source) {
    choose(field<QComboBox>("source"), field<QComboBox>("source").findData(source));
    choose(field<QComboBox>("reference_operation"), 4);
    text(field<QLineEdit>("reference_name"), "Imported supplier assembly");
    button("reference_prepare");
    INFO(field<QLabel>("summary").text().toStdString());
    REQUIRE(field<QPushButton>("apply").isEnabled());
    REQUIRE(window.viewport().feature_preview_count() == 5);
  }
  void capture(const QString& name) {
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (directory.isEmpty())
      return;
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath(name + ".png")));
  }
  ShellWindow window;

private:
  bool native_dialogs_;
};

std::vector<Point3> centers(const Project& project) {
  std::vector<Point3> result;
  for (const auto& leaf : checked(AssemblyLeafOccurrenceResolver{}.resolve(project)))
    result.push_back(geometry::AssemblyHierarchyTransformEvaluator{}.evaluate_point(
        leaf.transforms_inner_to_outer, {0, 0, 2.5}));
  std::sort(result.begin(), result.end(),
            [](Point3 a, Point3 b) { return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z); });
  return result;
}
} // namespace

TEST_CASE("Native structured STEP source workflow saves nested shared Reference and Editable "
          "Projects without replacing the registry",
          "[gui][integration][block142-structured-step-import][native-step-structured-import]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT STEP assembly preview");
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  CAPTURE(static_cast<int>(mode));
  const auto prefix = mode == StepImportMode::Reference
                          ? QStringLiteral("step-structured-reference")
                          : QStringLiteral("step-structured-editable");
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto asset = directory.filePath("nested.step");
  structured_test::write_supplier(asset.toStdString());
  const auto original_asset = bytes(asset);
  StructuredUi ui;
  REQUIRE(ui.window.new_part("STEP registry"));
  const auto registry = directory.filePath("registry.blcad.json");
  ui.file("save_as", registry);
  ui.begin();
  ui.text(ui.field<QLineEdit>("name"), "Nested supplier");
  ui.text(ui.field<QLineEdit>("path"), "nested.step");
  auto& modes = ui.field<QComboBox>("mode");
  ui.choose(modes, modes.findData(static_cast<int>(mode)));
  ui.button("preview");
  REQUIRE(ui.field<QPushButton>("apply").isEnabled());
  ui.button("apply");
  REQUIRE_FALSE(ui.window.step_import_tools().active());
  ui.command("save");
  const auto original = ui.snapshot();
  const auto history = ui.window.session().undo_label();
  const auto source = ui.window.session().part_document()->step_import_sources().front();
  const auto imported =
      checked(geometry::StepImportReader{}.read(source, directory.path().toStdString()));
  const auto foreign_part = std::find_if(imported.products.begin(), imported.products.end(),
                                         [](const auto& product) { return !product.is_assembly; });
  REQUIRE(foreign_part != imported.products.end());
  const auto expected = checked(geometry::StepStructuredImportCommands{}.create_project(
      source, DocumentId("project.step.gui"), "Imported supplier assembly",
      directory.path().toStdString()));
  const auto scene = ui.window.viewport().scene_semantic_ids();
  std::vector<bool> visible;
  for (const auto& id : scene)
    visible.push_back(ui.window.viewport().scene_presentation_visible(id));
  const auto camera = ui.window.viewport().camera_bookmark();
  const auto selection = ui.window.viewport().selected_semantic();
  ui.begin();
  ui.prepare(QString::fromStdString(source.id().value()));
  REQUIRE(ui.field<QComboBox>("topology_catalog").count() == 1);
  CHECK(ui.field<QComboBox>("reference_operation").count() == 5);
  CHECK_FALSE(ui.field<QComboBox>("reference_part").isEnabled());
  CHECK_FALSE(ui.field<QComboBox>("reference_assembly").isEnabled());
  CHECK_FALSE(ui.field<QLineEdit>("reference_x").isEnabled());
  CHECK(ui.snapshot() == original);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.window.step_import_tools().can_undo_after_cancel());
  for (const auto& id : scene)
    CHECK_FALSE(ui.window.viewport().scene_presentation_visible(id));
  auto& tree = ui.field<QTreeWidget>("geometry_products");
  ui.reveal(tree);
  REQUIRE(tree.topLevelItemCount() == 2);
  CHECK(tree.topLevelItem(0)->childCount() == 1);
  CHECK(tree.topLevelItem(0)->child(0)->text(0) == QString::fromStdString(foreign_part->name));
  CHECK(ui.field<QLabel>("geometry_summary").text().contains("5 posed body occurrences"));
  CHECK(tree.horizontalScrollBar()->maximum() == 0);
  CHECK(ui.field<QScrollArea>("scroll").horizontalScrollBar()->maximum() == 0);
  REQUIRE_FALSE(ui.field<QPlainTextEdit>("geometry_diagnostics").toPlainText().isEmpty());
  const auto expected_centers = centers(expected.project);
  REQUIRE(expected_centers.size() == 5);
  double left = ui.window.viewport().width(), right = 0;
  for (const auto center : expected_centers) {
    const auto point = checked(ui.window.viewport().model_to_screen(center));
    CHECK(point.x > 3);
    CHECK(point.x < ui.window.viewport().width() - 3);
    CHECK(point.y > 3);
    CHECK(point.y < ui.window.viewport().height() - 3);
    left = std::min(left, point.x);
    right = std::max(right, point.x);
  }
  CHECK(right - left >= 100);
  ui.capture(prefix + "-preview");
  const auto output = directory.filePath("imported.blcad.project.json");
  const auto modal_stale_output = directory.filePath("stale.blcad.project.json");
  ui.dialog([&] { ui.button("apply"); },
            [&](QFileDialog& chooser) {
              write(asset, original_asset +
                               QByteArray("\n/* source changed during destination choice */\n"));
              ui.flush();
              ui.window.refresh();
              chooser.selectFile(modal_stale_output);
              QMetaObject::invokeMethod(&chooser, "accept", Qt::DirectConnection);
            });
  CHECK_FALSE(QFile::exists(modal_stale_output));
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == original);
  CHECK(ui.window.viewport().selected_semantic() == selection);
  write(asset, original_asset);
  ui.flush();
  ui.prepare(QString::fromStdString(source.id().value()));
  ui.save_copy(output, false);
  CHECK_FALSE(QFile::exists(output));
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK(ui.snapshot() == original);
  CHECK(ui.window.viewport().selected_semantic() == selection);
  for (std::size_t i = 0; i < scene.size(); ++i)
    CHECK(ui.window.viewport().scene_presentation_visible(scene[i]) == visible[i]);
  const auto restored = ui.window.viewport().camera_bookmark();
  CHECK(restored.scale == Approx(camera.scale).margin(1e-7));
  CHECK(restored.eye.x == Approx(camera.eye.x).margin(1e-7));
  CHECK(restored.eye.y == Approx(camera.eye.y).margin(1e-7));
  CHECK(restored.eye.z == Approx(camera.eye.z).margin(1e-7));
  ui.prepare(QString::fromStdString(source.id().value()));
  ui.save_copy(output);
  REQUIRE(QFile::exists(output));
  REQUIRE_FALSE(ui.window.step_import_tools().active());
  CHECK(ui.snapshot() == original);
  CHECK(ui.window.session().file_path() == registry.toStdString());
  CHECK(ui.window.session().undo_label() == history);
  CHECK_FALSE(ui.window.session().dirty());
  CHECK(ui.window.viewport().selected_semantic() == selection);
  ui.file("open", output);
  REQUIRE(ui.window.session().project());
  CHECK(ui.snapshot() == checked(serialize_project_to_json(expected.project)));
  structured_test::check_source_structure(*ui.window.session().project(), imported);
  REQUIRE(ui.window.session().project()->part_document_count() == 1);
  CHECK(ui.window.session().project()->part_documents().front().is_reference_part() ==
        (mode == StepImportMode::Reference));
  const auto actual_centers = centers(*ui.window.session().project());
  const auto supplier_centers = centers(structured_test::supplier_project());
  REQUIRE(actual_centers.size() == supplier_centers.size());
  for (std::size_t i = 0; i < actual_centers.size(); ++i) {
    CHECK(actual_centers[i].x == Approx(supplier_centers[i].x).margin(1e-6));
    CHECK(actual_centers[i].y == Approx(supplier_centers[i].y).margin(1e-6));
    CHECK(actual_centers[i].z == Approx(supplier_centers[i].z).margin(1e-6));
  }
  ui.capture(prefix + "-opened");
  const auto flattened = directory.filePath("flat.step"),
             structured = directory.filePath("roundtrip.step");
  ui.file("export_assembly_flat_step", flattened);
  ui.file("export_assembly_structured_step", structured);
  CHECK(QFile::exists(flattened));
  CHECK(QFile::exists(structured));
  CHECK(bytes(asset) == original_asset);
  // The original document for this second preview now consumes the source in
  // five real occurrences. Invalidation must clear its derived scene too,
  // rather than merely restore now-stale imported presentations.
  const auto imported_snapshot = ui.snapshot();
  REQUIRE(ui.window.session().derived_results_fresh());
  REQUIRE(ui.window.viewport().presentation_count() >= 5);
  ui.begin();
  ui.prepare(QString::fromStdString(source.id().value()));
  const auto changed = original_asset + QByteArray("\n/* later supplier revision */\n");
  write(asset, changed);
  ui.flush();
  ui.window.refresh();
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK_FALSE(ui.window.session().derived_results_fresh());
  CHECK(ui.window.session().recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(ui.window.viewport().presentation_count() == 0);
  CHECK(ui.snapshot() == imported_snapshot);
  CHECK_FALSE(ui.window.session().dirty());
  ui.capture(prefix + "-consumed-stale-rejected");
  ui.button("cancel");
  CHECK(ui.window.session().undo_label().empty()); // Open starts a fresh history.
  CHECK(ui.snapshot() == imported_snapshot);
  CHECK(ui.window.viewport().presentation_count() == 0);
  CHECK(bytes(asset) == changed);
  write(asset, original_asset);
  ui.file("open", registry);
  ui.begin();
  ui.prepare(QString::fromStdString(source.id().value()));
  write(asset, changed);
  ui.flush();
  ui.window.refresh();
  CHECK_FALSE(ui.window.viewport().feature_preview_active());
  CHECK_FALSE(ui.field<QPushButton>("apply").isEnabled());
  CHECK(ui.snapshot() == original);
  CHECK_FALSE(ui.window.session().dirty());
  ui.capture(prefix + "-stale-rejected");
  ui.button("cancel");
  CHECK(ui.window.session().undo_label().empty());
  CHECK(ui.snapshot() == original);
  CHECK(bytes(asset) == changed);
}
