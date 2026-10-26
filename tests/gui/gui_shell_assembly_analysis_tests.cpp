#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
template <typename T> T& control(ShellWindow& window, const char* name) {
  auto* widget = window.findChild<T*>(QStringLiteral("blcad.inspect.") + name);
  REQUIRE(widget);
  return *widget;
}
Project example() {
  return checked(read_project_json_file(std::filesystem::path(BLCAD_SOURCE_DIR) / "examples" /
                                        "revolute_joint.blcad.project.json"));
}
Project repeated_project() {
  const auto original = example();
  auto root = checked(AssemblyDocument::create(DocumentId("assembly.root"), "Root"));
  auto child = checked(AssemblyDocument::create(DocumentId("assembly.child"), "Repeated child"));
  REQUIRE(root.add_member_part(DocumentId("part.revolute_plate")));
  REQUIRE(child.add_member_part(DocumentId("part.revolute_plate")));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("component.anchor"), "Anchor", DocumentId("part.revolute_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Grounded, {}))));
  REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("component.repeated"), "Repeated plate",
      DocumentId("part.revolute_plate"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {}))));
  for (const auto& [id, x] :
       {std::pair{"occurrence.left", 0.0}, std::pair{"occurrence.right", 40.0}})
    REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
        SubassemblyInstanceId(id), id, child.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, {{x, 0, 0}, {}}))));
  auto project = checked(Project::create(DocumentId("project.analysis"), "Analysis", root));
  REQUIRE(project.add_child_assembly_document(child));
  REQUIRE(project.add_part_document(original.part_documents().front()));
  REQUIRE(project.add_cross_hierarchy_joint(checked(AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.cross"), "Cross Revolute", AssemblyJointType::Revolute,
      checked(AssemblyHierarchyConstraintEndpoint::create(
          {}, ComponentInstanceId("component.anchor"), "feature.hole.seat")),
      checked(AssemblyHierarchyConstraintEndpoint::create(
          {SubassemblyInstanceId("occurrence.left")}, ComponentInstanceId("component.repeated"),
          "feature.hole.seat")),
      AssemblyJointState::Active, checked(Quantity::angle_deg(-90, "test")),
      checked(Quantity::angle_deg(90, "test")), checked(Quantity::angle_deg(0, "test"))))));
  REQUIRE(project.validate_assembly_structure());
  return project;
}
void open(ShellWindow& window, QTemporaryDir& directory, const Project& project) {
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("analysis.blcad.project.json");
  REQUIRE(write_project_json_file(project, path.toStdString()));
  REQUIRE(window.open_path(path));
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
}
std::string snapshot(const ShellWindow& window) {
  return checked(serialize_project_to_json(*window.session().project()));
}
void run(ShellWindow& window) {
  control<QPushButton>(window, "run").click();
  qApp->processEvents();
}
void cell(ShellWindow& window, int row, int column) {
  auto& table = control<QTableWidget>(window, "results");
  auto* item = table.item(row, column);
  REQUIRE(item);
  for (auto* ancestor = table.parentWidget(); ancestor; ancestor = ancestor->parentWidget())
    if (auto* scroll = qobject_cast<QScrollArea*>(ancestor))
      scroll->ensureWidgetVisible(&table);
  table.scrollToItem(item);
  qApp->processEvents();
  const QPoint point = table.visualItemRect(item).center();
  REQUIRE(table.viewport()->rect().contains(point));
  const auto* hit = window.childAt(table.viewport()->mapTo(&window, point));
  REQUIRE((hit == table.viewport() || (hit && table.viewport()->isAncestorOf(hit))));
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, table.viewport()->mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(table.viewport(), &event);
  }
  qApp->processEvents();
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->kind == GuiSelectionKind::Component);
  CHECK(window.viewport().selected_semantic()->semantic_id ==
        item->data(Qt::UserRole + 1).toString().toStdString());
  CHECK(window.session().selection().contains(GuiSelectionKind::Component,
                                              window.viewport().selected_semantic()->semantic_id));
}
void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  CHECK(window.grab().save(QDir(directory).filePath(QString::fromUtf8(name) + ".png")));
}
} // namespace

TEST_CASE("Shell Revolute sampling validates limits preserves direction and keeps source intent",
          "[gui][block136-assembly-analysis]") {
  ShellWindow window;
  QTemporaryDir directory;
  auto source = example();
  const auto& joint = source.assembly().joints().front();
  REQUIRE(source.assembly().add_joint(checked(AssemblyJoint::create(
      AssemblyJointId("joint.inactive"), "Inactive", AssemblyJointType::Revolute, joint.target_a(),
      joint.target_b(), AssemblyJointState::Inactive, checked(Quantity::angle_deg(-10, "test")),
      checked(Quantity::angle_deg(10, "test")), checked(Quantity::angle_deg(0, "test"))))));
  open(window, directory, source);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  REQUIRE(window.inspect_tools().begin_analysis());
  control<QComboBox>(window, "kind").setCurrentIndex(4);
  auto& chooser = control<QComboBox>(window, "sweep_joint");
  REQUIRE(chooser.count() == 1);
  CHECK(chooser.currentData(Qt::UserRole + 1).toString() == "joint.revolute");
  CHECK(chooser.currentData(Qt::UserRole + 2).toInt() == 0);
  auto& samples = control<QSpinBox>(window, "sweep_samples");
  CHECK(samples.minimum() == 2);
  CHECK(samples.maximum() == 1001);
  auto& start = control<QDoubleSpinBox>(window, "sweep_start");
  auto& end = control<QDoubleSpinBox>(window, "sweep_end");
  CHECK(start.value() == -90);
  CHECK(end.value() == 90);
  samples.setValue(3);
  start.setValue(60);
  end.setValue(-60);
  run(window);
  auto& table = control<QTableWidget>(window, "results");
  REQUIRE(table.rowCount() == 3);
  for (int row = 0; row < 3; ++row)
    CHECK(table.item(row, 2)->data(Qt::UserRole).toDouble() == 60 - row * 60);
  CHECK(control<QLabel>(window, "summary").text().contains("3 samples"));
  CHECK(control<QLabel>(window, "summary").text().contains("Intermediate positions"));
  cell(window, 1, 1);
  const auto linked = window.viewport().selected_semantic()->semantic_id;
  window.report_error(Error::validation("test.analysis", "A diagnostic does not change geometry"));
  window.refresh();
  REQUIRE(window.inspect_tools().active());
  REQUIRE(table.rowCount() == 3);
  CHECK(snapshot(window) == before);
  window.viewport().clear_selection();
  cell(window, 1, 1);
  CHECK(window.viewport().selected_semantic()->semantic_id == linked);
  end.setValue(100);
  CHECK(table.rowCount() == 0);
  CHECK_FALSE(window.viewport().selected_semantic());
  run(window);
  CHECK(table.rowCount() == 0);
  CHECK(control<QLabel>(window, "summary").text().contains("limits"));
  REQUIRE(window.inspect_tools().active());
  CHECK(snapshot(window) == before);
  end.setValue(0);
  run(window);
  REQUIRE(table.rowCount() == 3);
  CHECK(table.item(2, 2)->data(Qt::UserRole).toDouble() == 0);
  capture(window, "assembly-analysis-root-sweep");
  control<QPushButton>(window, "close").click();
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.session().dirty());
  CHECK(window.session().undo_label() == undo);
  CHECK_FALSE(window.session().can_redo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.save_to({}));
  CHECK(checked(
            read_project_json_file(directory.filePath("analysis.blcad.project.json").toStdString()))
            .assembly()
            .find_joint(AssemblyJointId("joint.revolute"))
            ->coordinate_deg() == 0);
}

TEST_CASE(
    "Analysis links repeated occurrence results and samples cross hierarchy without moving poses",
    "[gui][block136-assembly-analysis]") {
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, repeated_project());
  const auto before = snapshot(window);
  const std::string root = "component/assembly.root/component.anchor";
  const std::string left = "component/assembly.child/occurrence.left/component.repeated";
  const std::string right = "component/assembly.child/occurrence.right/component.repeated";
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{root});
  REQUIRE(window.viewport().select_semantic(root));
  const auto prior_scope = window.viewport().selection_semantic_scope();
  const auto prior_mask = window.viewport().selection_filter_mask();
  REQUIRE(window.inspect_tools().begin_analysis());
  control<QComboBox>(window, "kind").setCurrentIndex(2);
  run(window);
  window.viewport().fit_all();
  qApp->processEvents();
  auto& table = control<QTableWidget>(window, "results");
  REQUIRE(table.rowCount() == 3);
  bool left_seen = false, right_seen = false;
  for (int row = 0; row < table.rowCount(); ++row)
    for (int column = 0; column < 2; ++column) {
      cell(window, row, column);
      const auto selected = window.viewport().selected_semantic()->semantic_id;
      left_seen |= selected == left;
      right_seen |= selected == right;
    }
  CHECK(left_seen);
  CHECK(right_seen);
  capture(window, "assembly-analysis-repeated-occurrence");
  control<QComboBox>(window, "kind").setCurrentIndex(1);
  CHECK(table.rowCount() == 0);
  CHECK_FALSE(window.viewport().selected_semantic());
  control<QDoubleSpinBox>(window, "clearance").setValue(100);
  run(window);
  REQUIRE(table.rowCount() == 3);
  cell(window, 0, 0);
  control<QDoubleSpinBox>(window, "clearance").setValue(0.1);
  CHECK(table.rowCount() == 0);
  CHECK_FALSE(window.viewport().selected_semantic());
  control<QComboBox>(window, "kind").setCurrentIndex(4);
  auto& chooser = control<QComboBox>(window, "sweep_joint");
  REQUIRE(chooser.count() == 1);
  CHECK(chooser.currentData(Qt::UserRole + 1).toString() == "joint.cross");
  CHECK(chooser.currentData(Qt::UserRole + 2).toInt() == 1);
  control<QDoubleSpinBox>(window, "sweep_start").setValue(0);
  control<QDoubleSpinBox>(window, "sweep_end").setValue(45);
  control<QSpinBox>(window, "sweep_samples").setValue(2);
  run(window);
  REQUIRE(table.rowCount() == 6);
  CHECK(table.item(0, 2)->data(Qt::UserRole).toDouble() == 0);
  CHECK(table.item(5, 2)->data(Qt::UserRole).toDouble() == 45);
  cell(window, 5, 1);
  CHECK(window.viewport().selected_semantic()->semantic_id == right);
  capture(window, "assembly-analysis-cross-sweep");
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.session().can_undo());
  auto* dock = window.findChild<QDockWidget*>("blcad.shell.inspect_task");
  REQUIRE(dock);
  dock->close();
  CHECK_FALSE(window.inspect_tools().active());
  CHECK(table.rowCount() == 0);
  CHECK(window.viewport().selection_semantic_scope() == prior_scope);
  CHECK(window.viewport().selection_filter_mask() == prior_mask);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == root);
  CHECK(snapshot(window) == before);
}

TEST_CASE("Analysis clears occurrence highlights on Escape stale geometry history and document "
          "replacement",
          "[gui][block136-assembly-analysis]") {
  for (const std::string route : {"escape", "stale", "mutation", "undo", "new"}) {
    INFO(route);
    ShellWindow window;
    QTemporaryDir directory;
    open(window, directory, example());
    REQUIRE(window.session().commit_project_transaction("Translate plate", [](Project& project) {
      return project.assembly().set_component_instance_transform(
          ComponentInstanceId("component.plate.free"), {{1, 0, 0}, {}});
    }));
    window.refresh();
    REQUIRE(window.save_to({}));
    REQUIRE(window.inspect_tools().begin_analysis());
    control<QComboBox>(window, "kind").setCurrentIndex(2);
    run(window);
    REQUIRE(control<QTableWidget>(window, "results").rowCount() == 1);
    cell(window, 0, 1);
    if (route == "escape") {
      QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QApplication::sendEvent(&window, &escape);
    } else if (route == "stale") {
      window.session().set_derived_results_fresh(false);
      window.refresh();
    } else if (route == "mutation") {
      REQUIRE(window.session().commit_project_transaction(
          "Move analyzed occurrence", [](Project& project) {
            return project.assembly().set_component_instance_transform(
                ComponentInstanceId("component.plate.free"), {{7, 0, 0}, {}});
          }));
      window.refresh();
    } else if (route == "undo") {
      REQUIRE(window.undo());
    } else {
      REQUIRE(window.new_project("Replacement"));
    }
    CHECK_FALSE(window.inspect_tools().active());
    CHECK_FALSE(window.session().task().active());
    CHECK(control<QTableWidget>(window, "results").rowCount() == 0);
    CHECK_FALSE(window.viewport().selected_semantic());
    CHECK_FALSE(window.viewport().selection_semantic_scope());
    CHECK(window.viewport().selection_filter_mask() == 0xFFFFFFFFU);
  }
}

TEST_CASE("Sweep without an active supported joint fails cleanly and keeps the document editable",
          "[gui][block136-assembly-analysis]") {
  ShellWindow window;
  REQUIRE(window.new_project("No joint"));
  const auto before = snapshot(window);
  REQUIRE(window.inspect_tools().begin_analysis());
  control<QComboBox>(window, "kind").setCurrentIndex(4);
  REQUIRE(control<QComboBox>(window, "sweep_joint").count() == 0);
  run(window);
  CHECK(control<QTableWidget>(window, "results").rowCount() == 0);
  CHECK(control<QLabel>(window, "summary").text().contains("Select Active Revolute Joint"));
  CHECK(snapshot(window) == before);
  control<QPushButton>(window, "close").click();
  CHECK_FALSE(window.session().task().active());
}

TEST_CASE("Analysis restores the idle assembly target scene before restoring preselection",
          "[gui][block136-assembly-analysis]") {
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, example());
  auto* filter = window.findChild<QComboBox*>("blcad.selection.filter");
  REQUIRE(filter);
  filter->setCurrentIndex(filter->findData(selection_kind_bit(GuiSelectionKind::AssemblyTarget)));
  const auto targets =
      checked(geometry::AssemblyInteractionGeometry::targets(*window.session().project()));
  REQUIRE_FALSE(targets.empty());
  const auto target = targets.front().presentation.semantic_id;
  REQUIRE(window.viewport().select_semantic(target));
  const auto selected = window.viewport().selected_semantic();
  REQUIRE(window.inspect_tools().begin_analysis());
  CHECK(window.viewport().selection_filter_mask() ==
        selection_kind_bit(GuiSelectionKind::Component));
  run(window);
  REQUIRE(control<QTableWidget>(window, "results").rowCount() == 1);
  cell(window, 0, 0);
  control<QPushButton>(window, "close").click();
  CHECK(window.viewport().selection_filter_mask() ==
        selection_kind_bit(GuiSelectionKind::AssemblyTarget));
  CHECK(window.viewport().selected_semantic() == selected);
  REQUIRE(window.session().selection().size() == 1);
  CHECK(window.session().selection().entries().front() == *selected);
}

TEST_CASE("Native analysis occurrence links visibly highlight the selected repeated component",
          "[gui][block136-assembly-analysis][native-assembly-analysis-highlight]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT framebuffer rendering");
  ShellWindow window;
  QTemporaryDir directory;
  open(window, directory, repeated_project());
  REQUIRE(window.viewport().native_viewer_available());
  REQUIRE(window.inspect_tools().begin_analysis());
  control<QComboBox>(window, "kind").setCurrentIndex(2);
  run(window);
  auto& table = control<QTableWidget>(window, "results");
  REQUIRE(table.rowCount() == 3);
  const std::string selected = "component/assembly.child/occurrence.right/component.repeated";
  int row = -1, column = -1;
  for (int candidate = 0; candidate < table.rowCount(); ++candidate)
    for (int side = 0; side < 2; ++side)
      if (table.item(candidate, side)->data(Qt::UserRole + 1).toString().toStdString() ==
          selected) {
        row = candidate;
        column = side;
      }
  REQUIRE(row >= 0);
  auto* surface = window.viewport().findChild<QOpenGLWidget*>("blcad.occt_viewport.render_surface");
  REQUIRE(surface);
  REQUIRE(surface->isValid());
  const auto frame = [&] {
    QEventLoop rendered;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool swapped = false;
    QObject::connect(surface, &QOpenGLWidget::frameSwapped, &rendered, [&] {
      swapped = true;
      rendered.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &rendered, &QEventLoop::quit);
    surface->update();
    deadline.start(3000);
    rendered.exec();
    REQUIRE(swapped);
    const auto image = surface->grabFramebuffer().convertToFormat(QImage::Format_RGB32);
    REQUIRE_FALSE(image.isNull());
    return image;
  };
  const auto changed_pixels = [](const QImage& first, const QImage& second) {
    REQUIRE(first.size() == second.size());
    std::size_t changed = 0;
    for (int y = 0; y < first.height(); ++y)
      for (int x = 0; x < first.width(); ++x) {
        const auto a = first.pixel(x, y), b = second.pixel(x, y);
        if (std::abs(qRed(a) - qRed(b)) > 10 || std::abs(qGreen(a) - qGreen(b)) > 10 ||
            std::abs(qBlue(a) - qBlue(b)) > 10)
          ++changed;
      }
    return changed;
  };
  auto& close = control<QPushButton>(window, "close");
  QCursor::setPos(close.mapToGlobal(close.rect().center()));
  window.viewport().set_standard_view(GuiStandardView::Top);
  window.viewport().fit_all();
  window.viewport().clear_selection();
  const auto before = frame();
  CHECK(changed_pixels(before, frame()) == 0);
  capture(window, "assembly-analysis-highlight-before");
  cell(window, row, column);
  REQUIRE(window.viewport().selected_semantic()->semantic_id == selected);
  const auto highlighted = frame();
  const auto changed = changed_pixels(before, highlighted);
  INFO("Native highlighted model pixels: " << changed);
  CHECK(changed > 100);
  capture(window, "assembly-analysis-highlight-selected");
  window.viewport().clear_selection();
  CHECK(changed_pixels(before, frame()) == 0);
  // Clicking the same current table cell must restore the native highlight too.
  cell(window, row, column);
  CHECK(changed_pixels(before, frame()) > 100);
  control<QComboBox>(window, "kind").setCurrentIndex(1);
  REQUIRE_FALSE(window.viewport().selected_semantic());
  CHECK(changed_pixels(before, frame()) == 0);
  capture(window, "assembly-analysis-highlight-cleared");
}
