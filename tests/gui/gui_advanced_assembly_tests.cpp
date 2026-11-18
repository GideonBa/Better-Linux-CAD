#include "../support/adaptive_project_fixture.hpp"
#include "../support/advanced_assembly_fixture.hpp"
#include "../support/assembly_hole_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_physical_properties.hpp"
#include "blcad/gui/shell/shell_advanced_assembly_tools.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
using Catch::Approx;
namespace f = blcad::test::advanced_assembly;
namespace h = blcad::test::assembly_hole;
namespace {
using Tools = ShellAdvancedAssemblyTools;
template <class T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
template <class T> T& field(ShellWindow& window, const char* id) {
  auto* value = window.findChild<T*>(QStringLiteral("blcad.advanced_assembly.") + id);
  REQUIRE(value);
  return *value;
}
void reveal(QWidget& widget) {
  for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget())
    if (auto* area = qobject_cast<QScrollArea*>(parent))
      area->ensureWidgetVisible(&widget);
  QApplication::processEvents();
}
void click(QPushButton& button) {
  reveal(button);
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  QTest::mouseClick(&button, Qt::LeftButton);
  QApplication::processEvents();
}
void focus(QWidget& widget) {
  widget.setFocus();
  QApplication::processEvents();
  const auto* actual = QApplication::focusWidget();
  REQUIRE(actual);
  REQUIRE((actual == &widget || widget.isAncestorOf(actual)));
}
void choose(QComboBox& combo, int index) {
  REQUIRE(index >= 0);
  reveal(combo);
  focus(combo);
  QTest::keyClick(&combo, Qt::Key_Home);
  for (int i = 0; i < index; ++i)
    QTest::keyClick(&combo, Qt::Key_Down);
  REQUIRE(combo.currentIndex() == index);
}
void choose_data(QComboBox& combo, const QString& id) {
  choose(combo, combo.findData(id));
}
void choose_text(QComboBox& combo, const QString& text) {
  int found = -1;
  for (int i = 0; i < combo.count(); ++i)
    if (combo.itemText(i).contains(text)) {
      found = i;
      break;
    }
  REQUIRE(found >= 0);
  choose(combo, found);
}
void type(QLineEdit& input, const QString& text) {
  reveal(input);
  focus(input);
  QTest::keyClick(&input, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClicks(&input, text);
  QTest::keyClick(&input, Qt::Key_Return);
}
void number(QDoubleSpinBox& input, double value) {
  reveal(input);
  focus(input);
  QTest::keyClick(&input, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClicks(&input, QString::number(value, 'g', 12));
  QTest::keyClick(&input, Qt::Key_Return);
  CHECK(input.value() == Approx(value));
}
void toggle(QTableWidget& table, int row, int column) {
  table.scrollTo(table.model()->index(row, column));
  reveal(table);
  table.setCurrentCell(row, column);
  focus(table);
  const auto before = table.item(row, column)->checkState();
  QTest::keyClick(&table, Qt::Key_Space);
  CHECK(table.item(row, column)->checkState() != before);
}
std::string snapshot(const GuiDocumentSession& session) {
  return checked(serialize_project_to_json(*session.project()));
}
std::string bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll().toStdString();
}
QString seed(ShellWindow& window, QTemporaryDir& directory, Project project) {
  const auto path = directory.filePath("assembly.blcad.project.json");
  REQUIRE(write_project_json_file(project, path.toStdString()));
  REQUIRE(window.open_path(path));
  const bool compact = qEnvironmentVariableIsSet("BLCAD_UI_COMPACT");
  window.resize(compact ? 1024 : 1280,
                std::min(compact ? 640 : 800, window.screen()->availableGeometry().height() - 40));
  window.show();
  if (QApplication::platformName() == "xcb") {
    window.raise();
    window.activateWindow();
    REQUIRE(QTest::qWaitForWindowActive(&window));
  }
  QApplication::processEvents();
  window.viewport().fit_all();
  return path;
}
void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  QApplication::processEvents();
  const auto image = window.grab();
  CHECK(image.width() == qRound(window.width() * window.devicePixelRatioF()));
  REQUIRE(image.save(directory + "/" + name + ".png"));
}
void move(ShellWindow& window, const char* path, double amount) {
  auto& tools = window.advanced_assembly_tools();
  tools.action(Tools::Mode::Flexible).trigger();
  REQUIRE(tools.active());
  choose_text(tools.occurrence_editor(), QString::fromUtf8(path));
  choose(tools.flexibility_editor(), 1);
  choose_data(tools.joint_editor(), "slider");
  number(tools.coordinate_value_editor(), amount);
  click(field<QPushButton>(window, "preview"));
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
  capture(window, "flexible-occurrence-preview");
  click(field<QPushButton>(window, "apply"));
  REQUIRE_FALSE(tools.active());
  CHECK(window.session().undo_label() == "Set Flexible Occurrence Pose");
}
void cross_hierarchy_axes(ShellWindow& window) {
  auto& tools = window.assembly_tools();
  const auto camera = window.viewport().camera_bookmark();
  auto action = [&](const char* id) {
    auto* command = window.findChild<QAction*>(QStringLiteral("blcad.action.") + id);
    REQUIRE(command);
    REQUIRE(command->isEnabled());
    command->trigger();
    QApplication::processEvents();
  };
  auto control = [&](const char* id) -> QPushButton& {
    auto* button = window.findChild<QPushButton*>(QStringLiteral("blcad.assembly.") + id);
    REQUIRE(button);
    return *button;
  };
  action("assembly_cross_relationship");
  choose(tools.relationship_editor(), static_cast<int>(AssemblyConstraintType::Angle));
  auto target = [&](const char* path) {
    const auto& targets = tools.hierarchy_targets();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& entry) {
      return entry.endpoint.occurrence_path() == std::vector{SubassemblyInstanceId(path)} &&
             entry.endpoint.component_instance() == ComponentInstanceId("rod") &&
             entry.endpoint.semantic_reference() == "hole.axis";
    });
    REQUIRE(found != targets.end());
    return int(found - targets.begin());
  };
  const auto left = target("left"), right = target("right");
  const auto& left_target = tools.hierarchy_targets()[std::size_t(left)].target;
  const auto& right_target = tools.hierarchy_targets()[std::size_t(right)].target;
  CHECK(left_target.anchor.x == Approx(100).margin(1e-6));
  CHECK(left_target.anchor.z == Approx(10).margin(1e-6));
  CHECK(right_target.anchor.x == Approx(-70).margin(1e-6));
  CHECK(right_target.anchor.z == Approx(0).margin(1e-6));
  window.viewport().set_selection_semantic_scope(
      std::vector<std::string>{left_target.presentation.semantic_id});
  REQUIRE(window.viewport().set_plane_camera({100, 0, 15}, {0, -1, 0}, {0, 0, 1}));
  window.viewport().fit_all();
  QTest::qWait(50);
  const auto screen = checked(window.viewport().model_to_screen({100, 0, 15}));
  QTest::mouseClick(&window.viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(qRound(screen.x), qRound(screen.y)));
  QApplication::processEvents();
  REQUIRE(tools.first_editor().currentData().toInt() == left);
  // Native popup tracking must see the same desktop pointer and hovered row
  // as the actual Qt press/release over the exact rooted B choice.
  auto& second = tools.second_editor();
  reveal(second);
  focus(second);
  window.raise();
  window.activateWindow();
  REQUIRE(QTest::qWaitForWindowActive(&window));
  QCursor::setPos(second.mapToGlobal(second.rect().center()));
  QGuiApplication::sync();
  QApplication::processEvents();
  QTest::mouseClick(&second, Qt::LeftButton);
  auto* view = tools.second_editor().view();
  REQUIRE(QTest::qWaitFor([&] { return view->isVisible(); }, 2000));
  const int row = tools.second_editor().findData(right);
  REQUIRE(row >= 0);
  const auto index = tools.second_editor().model()->index(row, second.modelColumn());
  REQUIRE(index.flags().testFlag(Qt::ItemIsEnabled));
  REQUIRE(index.flags().testFlag(Qt::ItemIsSelectable));
  view->scrollTo(index, QAbstractItemView::PositionAtCenter);
  QApplication::processEvents();
  auto point = view->visualRect(index).center();
  REQUIRE(view->viewport()->rect().contains(point));
  int stable = 0, warps = 0;
  REQUIRE(QTest::qWaitFor(
      [&] {
        point = view->visualRect(index).center();
        const auto global = view->viewport()->mapToGlobal(point);
        if (QCursor::pos() != global && warps < 4) {
          ++warps;
          QCursor::setPos(global);
        }
        QGuiApplication::sync();
        QTest::mouseMove(view->viewport(), point);
        QApplication::processEvents();
        const auto actual = view->viewport()->mapFromGlobal(QCursor::pos());
        const bool ready = view->isVisible() && view->viewport()->rect().contains(point) &&
                           view->indexAt(point) == index && view->indexAt(actual) == index &&
                           view->currentIndex() == index;
        stable = ready ? stable + 1 : 0;
        return stable >= 2;
      },
      2000));
  REQUIRE(view->isVisible());
  REQUIRE(view->indexAt(point) == index);
  REQUIRE(view->currentIndex() == index);
  REQUIRE(view->screen()->availableGeometry().contains(view->viewport()->mapToGlobal(point)));
  QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, point);
  REQUIRE(
      QTest::qWaitFor([&] { return tools.second_editor().currentData().toInt() == right; }, 2000));
  number(tools.value_editor(), 90);
  REQUIRE(QTest::qWaitFor([&] { return control("apply").isEnabled(); }, 2000));
  capture(window, "flexible-hierarchy-axis-pick");
  click(control("apply"));
  REQUIRE(window.session().project()->cross_hierarchy_constraint_count() == 1);
  const auto& relation = window.session().project()->cross_hierarchy_constraints().front();
  CHECK(relation.type() == AssemblyConstraintType::Angle);
  REQUIRE(relation.angle());
  CHECK(relation.angle()->degrees() == Approx(90));
  CHECK(relation.target_a().occurrence_path() ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("left")});
  CHECK(relation.target_b().occurrence_path() ==
        std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("right")});
  action("assembly_cross_solve");
  auto* groups = window.findChild<QListWidget*>("blcad.assembly.solve_group");
  REQUIRE(groups);
  REQUIRE(groups->count() == 1);
  reveal(*groups);
  QTest::mouseClick(groups->viewport(), Qt::LeftButton, Qt::NoModifier,
                    groups->visualItemRect(groups->item(0)).center());
  QApplication::processEvents();
  auto* diagnostic = window.findChild<QLabel*>("blcad.assembly.summary");
  REQUIRE(diagnostic);
  INFO("Solve status: " << diagnostic->text().toStdString());
  INFO("Selected groups: " << groups->selectedItems().size());
  REQUIRE(groups->selectedItems().size() == 1);
  INFO("Group visible: " << groups->isVisible());
  INFO("Row reachable: " << groups->viewport()->rect().contains(
           groups->visualItemRect(groups->item(0)).center()));
  capture(window, "a01-cross-solve-diagnostic");
  REQUIRE(QTest::qWaitFor([&] { return control("apply").isEnabled(); }, 2000));
  capture(window, "flexible-hierarchy-solve-preview");
  click(control("apply"));
  CHECK(window.session().undo_label() == "Solve cross-hierarchy group");
  CHECK(checked(window.session().project()->effective_component_transform(
                    {SubassemblyInstanceId("left")}, ComponentInstanceId("rod")))
            .translation_mm.z == Approx(10).margin(1e-6));
  CHECK(checked(window.session().project()->effective_component_transform(
                    {SubassemblyInstanceId("right")}, ComponentInstanceId("rod")))
            .translation_mm.z == Approx(30).margin(1e-6));
  REQUIRE(window.viewport().restore_camera_bookmark(camera));
}
void activate_browser(ShellWindow& window, const QString& label) {
  QTreeWidgetItem* found = nullptr;
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->text(0).startsWith(label)) {
      found = *it;
      break;
    }
  REQUIRE(found);
  const auto semantic = found->data(0, Qt::UserRole).toString();
  REQUIRE(semantic.startsWith("assembly-position/"));
  const auto expected =
      AssemblyPositionId(semantic.mid(QString("assembly-position/").size()).toStdString());
  const auto find_row = [&]() -> QTreeWidgetItem* {
    for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == semantic)
        return *it;
    return nullptr;
  };
  const auto point = [&]() {
    auto* row = find_row();
    REQUIRE(row);
    for (auto* parent = row->parent(); parent; parent = parent->parent())
      parent->setExpanded(true);
    window.model_browser().scrollToItem(row);
    QApplication::processEvents();
    row = find_row();
    REQUIRE(row);
    const auto rectangle = window.model_browser().visualItemRect(row);
    REQUIRE(!rectangle.isEmpty());
    REQUIRE(window.model_browser().viewport()->rect().contains(rectangle.center()));
    return rectangle.center();
  };
  focus(window.model_browser());
  QTest::mouseClick(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier, point());
  QApplication::processEvents();
  // Selection refreshes the browser. Resolve the stable row again before the
  // actual double-click and its release.
  const auto activation_point = point();
  QTest::mouseDClick(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                     activation_point);
  QTest::mouseRelease(window.model_browser().viewport(), Qt::LeftButton, Qt::NoModifier,
                      activation_point);
  QApplication::processEvents();
  REQUIRE(QTest::qWaitFor([&] { return window.session().active_position() == expected; }, 2000));
}
std::vector<geometry::GeometryShape> imported_shapes(const QString& directory,
                                                     const char* filename) {
  auto report = checked(inspect_step_import_asset(directory.toStdString(), filename));
  REQUIRE(report.observed_sha256_digest());
  auto source = checked(StepImportSource::create(StepImportSourceId("exchange"), "Exchange",
                                                 filename, *report.observed_sha256_digest(),
                                                 StepImportMode::EditableBody));
  geometry::StepImportReader reader;
  auto imported = checked(reader.read(source, directory.toStdString()));
  return checked(reader.preview_shapes(imported));
}
double imported_volume(const QString& directory, const char* filename) {
  double volume = 0;
  for (const auto& shape : imported_shapes(directory, filename))
    volume += blcad::test::integrated_solid_volume(shape).volume_mm3;
  return volume;
}
void measure_extension(ShellWindow& window) {
  auto& tools = window.measure_tools();
  auto* action = tools.findChild<QAction*>("blcad.action.measure_distance");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  action->trigger();
  for (int role = 0; role < 2; ++role) {
    const auto label = role == 0 ? "left / sleeve" : "left / rod";
    const double z = role == 0 ? 0 : 10;
    int found = -1;
    for (std::size_t i = 0; i < tools.targets().size(); ++i) {
      const auto& target = tools.targets()[i];
      if (target.kind == geometry::MeasureSelectionKind::Plane &&
          target.label.find(label) != std::string::npos && std::abs(target.anchor.z - z) < 1e-6) {
        found = int(i);
        break;
      }
    }
    REQUIRE(found >= 0);
    choose(tools.target_editor(std::size_t(role)),
           tools.target_editor(std::size_t(role)).findData(found));
  }
  REQUIRE(tools.result());
  CHECK(tools.result()->primary_value == Approx(10).margin(1e-6));
  capture(window, "named-position-measure");
  tools.cancel();
}
} // namespace

TEST_CASE("Representation session selections preserve authored Master and "
          "independent visibility",
          "[gui][parity-b198][assembly-representation-session]") {
  QTemporaryDir directory;
  ShellWindow window;
  auto project = blcad::test::adaptive_project();
  ModelState manufacturing{ModelStateId("manufacturing"),
                           "Manufacturing",
                           {},
                           {},
                           {},
                           {{ComponentInstanceId("irrelevant"), true, std::nullopt}},
                           {}};
  REQUIRE(project.assembly().replace_model_states({manufacturing}));
  seed(window, directory, std::move(project));
  auto position = checked(
      capture_assembly_position(*window.session().project(), AssemblyPositionId("moved"), "Moved"));
  position.components.front().transform.translation_mm.z += 20;
  auto view = checked(capture_assembly_design_view(*window.session().project(),
                                                   AssemblyDesignViewId("cover"), "Cover"));
  for (auto& component : view.components)
    component.visible = component.occurrence.part == DocumentId("cover");
  REQUIRE(window.session().commit_assembly_representations({position}, {view}));
  const auto authored = snapshot(window.session());
  REQUIRE(window.session().activate_assembly_position(position.id));
  REQUIRE(window.session().activate_assembly_design_view(view.id));
  CHECK(snapshot(window.session()) == authored);
  CHECK(window.session().effective_project()->effective_position() == position.id);
  CHECK(window.session().effective_project()->effective_design_view() == view.id);
  CHECK(AssemblyLeafOccurrenceResolver{}
            .resolve(*window.session().effective_project())
            .value()
            .size() == 1);
  CHECK(
      AssemblyLeafOccurrenceResolver{}
          .resolve(*window.session().effective_project(), AssemblyLeafParticipation::PhysicalActive)
          .value()
          .size() == 4);
  REQUIRE(window.session().activate_model_state(manufacturing.id));
  CHECK(window.session().active_position() == position.id);
  CHECK(window.session().active_design_view() == view.id);
  CHECK(
      AssemblyLeafOccurrenceResolver{}
          .resolve(*window.session().effective_project(), AssemblyLeafParticipation::PhysicalActive)
          .value()
          .size() == 3);
  CHECK(snapshot(window.session()) == authored);
  REQUIRE(window.session().activate_model_state(primary_model_state_id()));
  CHECK_FALSE(window.session().commit_project_transaction(
      "Wrong Master scope", [](Project&) { return Result<std::size_t>::success(1); }));
  REQUIRE(window.session().task().begin("pending"));
  CHECK_FALSE(window.session().activate_assembly_position(master_position_id()));
  REQUIRE(window.session().task().cancel());
  REQUIRE(window.session().activate_assembly_position(master_position_id()));
  REQUIRE(window.session().activate_assembly_design_view(default_design_view_id()));
  CHECK(snapshot(window.session()) == authored);
}

TEST_CASE("Native independent cylinder controls retain child definitions and recall "
          "named positions",
          "[gui][parity-b198][native-advanced-assembly][native-assembly-flex]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT Assembly controls");
  QTemporaryDir directory;
  ShellWindow window;
  auto project = f::cylinders();
  auto* definition = project.find_assembly_document(DocumentId("child"));
  REQUIRE(definition);
  // The local geometric relationship supplies grounded sleeve authorities;
  // the separate Prismatic joint retains the independent axial drive.
  const auto coaxial = checked(AssemblyConstraint::create(
      AssemblyConstraintId("coaxial"), "Sleeve / rod coaxial", AssemblyConstraintType::Concentric,
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("sleeve"), "hole.axis")),
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("rod"), "hole.axis"))));
  REQUIRE(definition->add_constraint(coaxial));
  const auto path = seed(window, directory, std::move(project));
  REQUIRE(QTest::qWaitForWindowExposed(&window));
  const auto child = checked(serialize_assembly_document_to_json(
      *window.session().project()->find_assembly_document(DocumentId("child"))));
  move(window, "left", 10);
  move(window, "right", 30);
  cross_hierarchy_axes(window);
  CHECK(checked(serialize_assembly_document_to_json(
            *window.session().project()->find_assembly_document(DocumentId("child")))) == child);
  auto shape = checked(
      geometry::AssemblyStepExporter{}.build_posed_shape(*window.session().effective_project()));
  CHECK(blcad::test::classify_solid_point(shape, {108, 0, 15}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape, {-65, 8, 0}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape, {-80, 8, 0}) ==
        blcad::test::SolidPointLocation::Outside);
  auto& tools = window.advanced_assembly_tools();
  tools.action(Tools::Mode::Representations).trigger();
  type(tools.name_editor(), "Extended");
  click(field<QPushButton>(window, "store_position"));
  REQUIRE(window.session().project()->assembly_positions().size() == 1);
  const auto extended = window.session().project()->assembly_positions().front().id;
  move(window, "left", 20);
  const auto authored = snapshot(window.session());
  activate_browser(window, "Extended");
  CHECK(window.session().active_position() == extended);
  CHECK(snapshot(window.session()) == authored);
  shape = checked(
      geometry::AssemblyStepExporter{}.build_posed_shape(*window.session().effective_project()));
  CHECK(blcad::test::classify_solid_point(shape, {108, 0, 15}) ==
        blcad::test::SolidPointLocation::Inside);
  const double volume = 4 * std::numbers::pi * 91 * 10;
  GuiPhysicalPropertiesController physical;
  REQUIRE(physical.begin(window.session()));
  REQUIRE(physical.refresh(window.session()));
  auto properties = checked(physical.view(window.session()));
  REQUIRE(properties.known_mass);
  CHECK(properties.volume == Approx(volume).epsilon(1e-7));
  CHECK(properties.known_mass->mass == Approx(volume * 4e-6).epsilon(1e-7));
  measure_extension(window);
  auto contact = checked(GuiAnalysisExportWorkbench{}.analyze_contact(window.session()));
  CHECK(contact.component_occurrence_count == 4);
  CHECK(contact.evaluated_pair_count == 6);
  CHECK(std::count_if(contact.records.begin(), contact.records.end(), [](const auto& record) {
          return record.classification == geometry::AssemblyContactClassification::Touching;
        }) == 1);
  REQUIRE(GuiAnalysisExportWorkbench{}.export_step(
      window.session(), {GuiStepExportMode::AssemblyFlattened,
                         directory.filePath("named-position.step").toStdString()}));
  CHECK(imported_volume(directory.path(), "named-position.step") == Approx(volume).epsilon(1e-7));
  REQUIRE(GuiAnalysisExportWorkbench{}.export_step(
      window.session(), {GuiStepExportMode::AssemblyStructured,
                         directory.filePath("named-position-structured.step").toStdString()}));
  const auto structured = imported_shapes(directory.path(), "named-position-structured.step");
  double structured_volume = 0;
  bool extended_point = false;
  for (const auto& item : structured) {
    structured_volume += blcad::test::integrated_solid_volume(item).volume_mm3;
    extended_point = extended_point || blcad::test::classify_solid_point(item, {108, 0, 15}) ==
                                           blcad::test::SolidPointLocation::Inside;
  }
  CHECK(structured_volume == Approx(volume).epsilon(1e-7));
  CHECK(extended_point);
  REQUIRE(window.save_to({}));
  CHECK(snapshot(window.session()) == authored);
  CHECK(bytes(path) == authored);
  activate_browser(window, "Master");
  CHECK(window.session().active_position() == master_position_id());
  shape = checked(
      geometry::AssemblyStepExporter{}.build_posed_shape(*window.session().effective_project()));
  CHECK(blcad::test::classify_solid_point(shape, {108, 0, 15}) ==
        blcad::test::SolidPointLocation::Outside);
  tools.action(Tools::Mode::Representations).trigger();
  type(tools.name_editor(), "Left assembly only");
  for (int row = 0; row < tools.display_editor().rowCount(); ++row)
    if (tools.display_editor().item(row, 1)->text().contains("right"))
      toggle(tools.display_editor(), row, 0);
  click(field<QPushButton>(window, "preview"));
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
  capture(window, "assembly-design-view-preview");
  click(field<QPushButton>(window, "apply"));
  CHECK(window.session().active_design_view() != default_design_view_id());
  CHECK(AssemblyLeafOccurrenceResolver{}
            .resolve(*window.session().effective_project())
            .value()
            .size() == 2);
  auto mass = checked(geometry::BodyResultInspector{}.inspect_physical_properties(
      *window.session().effective_project()));
  CHECK(mass.data().occurrences.size() == 4);
  CHECK(mass.data().volume_mm3 == Approx(volume).epsilon(1e-7));
  capture(window, "assembly-design-view-active");
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  CHECK(window.session().active_position() == master_position_id());
  CHECK(window.session().active_design_view() == default_design_view_id());
  CHECK(AssemblyLeafOccurrenceResolver{}
            .resolve(*window.session().effective_project())
            .value()
            .size() == 4);
  tools.action(Tools::Mode::Representations).trigger();
  choose_data(tools.position_editor(), QString::fromStdString(extended.value()));
  type(tools.name_editor(), "Extended Recall");
  click(field<QPushButton>(window, "rename_position"));
  CHECK(window.session().project()->find_assembly_position(extended)->name == "Extended Recall");
  tools.action(Tools::Mode::Representations).trigger();
  choose_data(tools.position_editor(), QString::fromStdString(extended.value()));
  click(field<QPushButton>(window, "delete_position"));
  CHECK(window.session().project()->assembly_positions().empty());
  tools.action(Tools::Mode::Representations).trigger();
  choose_text(tools.design_view_editor(), "Left assembly only");
  click(field<QPushButton>(window, "delete_view"));
  CHECK(window.session().project()->assembly_design_views().empty());
}

TEST_CASE("Native adaptive selectors retain invalid inputs and publish one visible "
          "driver",
          "[gui][parity-b198][native-advanced-assembly][native-assembly-adaptive]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT directed design controls");
  QTemporaryDir directory;
  ShellWindow window;
  seed(window, directory, blcad::test::adaptive_project());
  REQUIRE(QTest::qWaitForWindowExposed(&window));
  auto& tools = window.advanced_assembly_tools();
  const auto before = snapshot(window.session());
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  tools.action(Tools::Mode::Adaptive).trigger();
  choose_text(tools.source_editor(), "source");
  choose_text(tools.source_geometry_editor(), "Base / top");
  choose_text(tools.target_editor(), "source");
  click(field<QPushButton>(window, "preview"));
  CHECK_FALSE(field<QPushButton>(window, "apply").isEnabled());
  CHECK(tools.active());
  CHECK(snapshot(window.session()) == before);
  choose_text(tools.target_editor(), "cover");
  choose_text(tools.target_construction_editor(), "Linked support");
  type(tools.name_editor(), "Cover follows source");
  click(field<QPushButton>(window, "preview"));
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
  CHECK(field<QLabel>(window, "dependencies").text().contains("Visible driver"));
  capture(window, "assembly-adaptive-preview");
  click(field<QPushButton>(window, "apply"));
  CHECK(window.session().undo_label() == "Edit Adaptive Assembly Reference");
  REQUIRE(window.session().project()->design_references().size() == 1);
  const auto after = snapshot(window.session());
  REQUIRE(window.undo());
  CHECK(snapshot(window.session()) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window.session()) == after);
  auto shape = checked(
      geometry::AssemblyStepExporter{}.build_posed_shape(*window.session().effective_project()));
  CHECK(blcad::test::classify_solid_point(shape, {20, 30, 6}) ==
        blcad::test::SolidPointLocation::Inside);
  tools.action(Tools::Mode::Adaptive).trigger();
  choose_text(tools.source_editor(), "cover");
  choose_text(tools.source_geometry_editor(), "Base / top");
  choose_text(tools.target_editor(), "source");
  choose_text(tools.target_construction_editor(), "Linked support");
  click(field<QPushButton>(window, "preview"));
  CHECK_FALSE(field<QPushButton>(window, "apply").isEnabled());
  CHECK(tools.active());
  CHECK(snapshot(window.session()) == after);
  click(field<QPushButton>(window, "cancel"));
  tools.action(Tools::Mode::Adaptive).trigger();
  choose(field<QComboBox>(window, "record"), 1);
  click(field<QPushButton>(window, "preview"));
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
  CHECK(window.viewport().feature_preview_active());
  tools.action(Tools::Mode::Context).trigger();
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window.session()) == after);
  tools.cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
}

TEST_CASE("Native Assembly Hole participant table coordinates pinned cover and "
          "blind-thread recipes atomically",
          "[gui][parity-b198][native-advanced-assembly][native-assembly-holes]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT Assembly Hole controls");
  QTemporaryDir directory;
  ShellWindow window;
  auto project = h::project();
  REQUIRE(project.find_part_document(DocumentId("base.part"))->add_hole_feature(h::threaded()));
  const auto path = seed(window, directory, std::move(project));
  REQUIRE(QTest::qWaitForWindowExposed(&window));
  auto& tools = window.advanced_assembly_tools();
  const auto before = snapshot(window.session());
  tools.action(Tools::Mode::AssemblyHole).trigger();
  choose_text(tools.layout_editor(), "Cover clearance");
  choose_data(field<QComboBox>(window, "pattern"), "layout.pattern");
  type(tools.name_editor(), "Cover and threaded base");
  int cover = -1, base = -1;
  auto& table = tools.participant_editor();
  for (int row = 0; row < table.rowCount(); ++row) {
    const auto label = table.item(row, 1)->text();
    if (label.endsWith("Cover"))
      cover = row;
    if (label.endsWith("Base"))
      base = row;
  }
  REQUIRE(cover >= 0);
  REQUIRE(base >= 0);
  toggle(table, cover, 0);
  toggle(table, base, 0);
  table.scrollTo(table.model()->index(cover, 2));
  choose_data(*static_cast<QComboBox*>(table.cellWidget(cover, 2)), "layout.hole");
  click(field<QPushButton>(window, "preview"));
  CHECK_FALSE(field<QPushButton>(window, "apply").isEnabled());
  CHECK(tools.active());
  CHECK(snapshot(window.session()) == before);
  table.scrollTo(table.model()->index(base, 2));
  choose_data(*static_cast<QComboBox*>(table.cellWidget(base, 2)), "mounting.hole");
  click(field<QPushButton>(window, "preview"));
  REQUIRE(field<QPushButton>(window, "apply").isEnabled());
  capture(window, "assembly-hole-participants-preview");
  click(field<QPushButton>(window, "apply"));
  CHECK(window.session().undo_label() == "Coordinate Assembly Hole Participants");
  REQUIRE(window.session().project()->assembly_hole_features().size() == 1);
  const auto* bottom = window.session().project()->find_part_document(DocumentId("base.part"));
  const auto* hole = bottom->find_hole_feature(FeatureId("mounting.hole"));
  REQUIRE(hole);
  REQUIRE(hole->thread());
  CHECK(hole->placements().size() == 2);
  CHECK(std::get<ThreadStandardProfile>(hole->thread()->profile()).table == h::pin());
  const auto* shape =
      window.session().project_part_shape_cache(bottom->id())->find_body_shape(BodyId("body.base"));
  REQUIRE(shape);
  CHECK(blcad::test::classify_solid_point(*shape, {2, -3, 6}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(*shape, {2, 3, 6}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(*shape, {0, 0, 6}) ==
        blcad::test::SolidPointLocation::Inside);
  const auto after = snapshot(window.session());
  REQUIRE(window.undo());
  CHECK(snapshot(window.session()) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window.session()) == after);
  REQUIRE(window.save_to({}));
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window.session()) == after);
  REQUIRE(GuiAnalysisExportWorkbench{}.export_step(
      window.session(), {GuiStepExportMode::AssemblyFlattened,
                         directory.filePath("coordinated.step").toStdString()}));
  const double expected = 2000 + 72 - (5.76 + 5.12) * std::numbers::pi;
  CHECK(imported_volume(directory.path(), "coordinated.step") == Approx(expected).epsilon(1e-7));
}

TEST_CASE("Flexible occurrence task keeps preview disposable and publishes one root edit",
          "[gui][advanced-assembly-controls][advanced-flex-controls][parity-b198]") {
  QTemporaryDir directory;
  ShellWindow window;
  seed(window, directory, f::cylinders());
  const auto before = snapshot(window.session());
  auto& tools = window.advanced_assembly_tools();
  REQUIRE(tools.begin(Tools::Mode::Flexible));
  choose_text(tools.occurrence_editor(), "left");
  tools.flexibility_editor().setCurrentIndex(1);
  choose_data(tools.joint_editor(), "slider");
  tools.coordinate_value_editor().setValue(10);
  auto preview = tools.preview();
  INFO((preview ? std::string{} : preview.error().message()));
  REQUIRE(preview);
  CHECK(snapshot(window.session()) == before);
  REQUIRE(tools.apply());
  CHECK(window.session().project()->flexible_occurrence({SubassemblyInstanceId("left")}));
  CHECK_FALSE(window.session().project()->flexible_occurrence({SubassemblyInstanceId("right")}));
  CHECK(window.session().undo_label() == "Set Flexible Occurrence Pose");
  REQUIRE(window.undo());
  CHECK(snapshot(window.session()) == before);
}

TEST_CASE("Adaptive task applies its visible source driver and retains preview isolation",
          "[gui][advanced-assembly-controls][advanced-adaptive-controls][parity-b198]") {
  QTemporaryDir directory;
  ShellWindow window;
  seed(window, directory, blcad::test::adaptive_project());
  const auto before = snapshot(window.session());
  auto& tools = window.advanced_assembly_tools();
  REQUIRE(tools.begin(Tools::Mode::Adaptive));
  choose_text(tools.source_editor(), "source");
  choose_text(tools.source_geometry_editor(), "Base / top");
  choose_text(tools.target_editor(), "cover");
  choose_text(tools.target_construction_editor(), "Linked support");
  auto preview = tools.preview();
  INFO((preview ? std::string{} : preview.error().message()));
  REQUIRE(preview);
  CHECK(snapshot(window.session()) == before);
  REQUIRE(tools.apply());
  REQUIRE(window.session().project()->design_references().size() == 1);
  CHECK(window.session().project()->design_references().front().driver().part ==
        DocumentId("cover"));
  CHECK(window.session().undo_label() == "Edit Adaptive Assembly Reference");
  REQUIRE(window.undo());
  CHECK(snapshot(window.session()) == before);
}

TEST_CASE("Assembly Hole task applies checked participant recipes in one root transaction",
          "[gui][advanced-assembly-controls][advanced-hole-controls][parity-b198]") {
  QTemporaryDir directory;
  ShellWindow window;
  auto project = h::project();
  REQUIRE(project.find_part_document(DocumentId("base.part"))->add_hole_feature(h::threaded()));
  seed(window, directory, std::move(project));
  const auto before = snapshot(window.session());
  auto& tools = window.advanced_assembly_tools();
  REQUIRE(tools.begin(Tools::Mode::AssemblyHole));
  choose_text(tools.layout_editor(), "Cover clearance");
  choose_data(field<QComboBox>(window, "pattern"), "layout.pattern");
  auto& table = tools.participant_editor();
  for (int row = 0; row < table.rowCount(); ++row) {
    const auto label = table.item(row, 1)->text();
    const auto recipe = label.endsWith("Cover")  ? "layout.hole"
                        : label.endsWith("Base") ? "mounting.hole"
                                                 : "";
    if (!*recipe)
      continue;
    table.item(row, 0)->setCheckState(Qt::Checked);
    auto& combo = *static_cast<QComboBox*>(table.cellWidget(row, 2));
    combo.setCurrentIndex(combo.findData(recipe));
  }
  auto preview = tools.preview();
  INFO((preview ? std::string{} : preview.error().message()));
  REQUIRE(preview);
  CHECK(snapshot(window.session()) == before);
  REQUIRE(tools.apply());
  REQUIRE(window.session().project()->assembly_hole_features().size() == 1);
  CHECK(window.session().undo_label() == "Coordinate Assembly Hole Participants");
  REQUIRE(window.undo());
  CHECK(snapshot(window.session()) == before);
}

TEST_CASE("Assembly pick catalogs compose effective state position and exact rooted display intent",
          "[gui][parity-b198][assembly-effective-target-catalog]") {
  auto project = f::cylinders();
  auto* part = project.find_part_document(DocumentId("cylinder"));
  REQUIRE(part);
  const ModelStateId long_state("long"), configuration("configured");
  REQUIRE(part->replace_model_states({{long_state,
                                       "Long",
                                       {{ParameterId("length"), checked(Quantity::length_mm(20))}},
                                       {},
                                       {},
                                       {},
                                       {}}}));
  auto* child = project.find_assembly_document(DocumentId("child"));
  REQUIRE(child);
  REQUIRE(child->set_component_instance_visibility(ComponentInstanceId("rod"),
                                                   ComponentVisibility::Hidden));
  REQUIRE(child->replace_model_states({{configuration,
                                        "Configured child",
                                        {},
                                        {},
                                        {},
                                        {{ComponentInstanceId("rod"), std::nullopt, long_state}},
                                        {}}}));
  REQUIRE(project.assembly().add_member_part(part->id()));
  for (const char* id : {"root.visible", "root.hidden"})
    REQUIRE(project.assembly().add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId(id), id, part->id(),
        std::string(id) == "root.visible" ? ComponentVisibility::Visible
                                          : ComponentVisibility::Hidden))));
  REQUIRE(project.assembly().replace_model_states(
      {{configuration,
        "Configured root",
        {},
        {},
        {},
        {{ComponentInstanceId("root.hidden"), std::nullopt, long_state}},
        {{SubassemblyInstanceId("right"), std::nullopt, configuration}}}}));
  auto position = checked(capture_assembly_position(project, AssemblyPositionId("moved"), "Moved"));
  for (auto& component : position.components)
    if (component.component == ComponentInstanceId("root.hidden"))
      component.transform.translation_mm.z = 25;
  for (auto& assembly : position.subassemblies)
    if (assembly.subassembly == SubassemblyInstanceId("right"))
      assembly.transform.translation_mm.x += 20;
  auto view =
      checked(capture_assembly_design_view(project, AssemblyDesignViewId("shown"), "Shown"));
  for (auto& component : view.components) {
    if (component.occurrence.path.empty())
      component.visible = component.occurrence.component == ComponentInstanceId("root.hidden");
    else if (component.occurrence.component == ComponentInstanceId("rod"))
      component.visible = component.occurrence.path ==
                          std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("right")};
  }
  REQUIRE(project.replace_assembly_positions({position}));
  REQUIRE(project.replace_assembly_design_views({view}));
  QTemporaryDir directory;
  ShellWindow window;
  seed(window, directory, std::move(project));
  const auto authored = snapshot(window.session());
  auto& tools = window.assembly_tools();
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  REQUIRE(tools.begin(ShellAssemblyTools::Command::Relationship));
  REQUIRE_FALSE(tools.targets().empty());
  for (const auto& target : tools.targets())
    CHECK(target.reference.component_instance() == ComponentInstanceId("root.visible"));
  tools.cancel();
  REQUIRE(window.session().activate_model_state(configuration));
  REQUIRE(window.session().activate_assembly_position(position.id));
  const auto physical = checked(geometry::BodyResultInspector{}.inspect_physical_properties(
      *window.session().effective_project()));
  REQUIRE(window.session().activate_assembly_design_view(view.id));
  const auto shown_physical = checked(geometry::BodyResultInspector{}.inspect_physical_properties(
      *window.session().effective_project()));
  CHECK(shown_physical.data().occurrences.size() == 6);
  CHECK(shown_physical.data().volume_mm3 == Approx(7280 * std::numbers::pi).margin(1e-5));
  CHECK(shown_physical.data().volume_mm3 == Approx(physical.data().volume_mm3).margin(1e-7));
  REQUIRE(shown_physical.data().complete_mass());
  REQUIRE(physical.data().complete_mass());
  CHECK(shown_physical.data().complete_mass()->mass_kg ==
        Approx(physical.data().complete_mass()->mass_kg).margin(1e-10));
  REQUIRE(tools.begin(ShellAssemblyTools::Command::Relationship));
  REQUIRE_FALSE(tools.targets().empty());
  // The circular-base fixture exposes its bore through the stable hole.axis target.
  bool root_axis = false;
  for (const auto& target : tools.targets()) {
    CHECK(target.reference.component_instance() == ComponentInstanceId("root.hidden"));
    if (target.reference.semantic_reference() == "hole.axis") {
      root_axis = true;
      REQUIRE(std::holds_alternative<geometry::AssemblyCylindricalSurfaceTargetDescriptor>(
          target.resolved.descriptor));
      CHECK(target.anchor.z == Approx(25).margin(1e-6));
    }
  }
  CHECK(root_axis);
  CHECK_FALSE(window.session().activate_assembly_design_view(default_design_view_id()));
  CHECK(window.session().active_design_view() == view.id);
  tools.cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(tools.begin(ShellAssemblyTools::Command::CrossRelationship));
  bool right_axis = false, left_sleeve = false;
  for (const auto& entry : tools.hierarchy_targets()) {
    const auto& path = entry.endpoint.occurrence_path();
    const auto& component = entry.endpoint.component_instance();
    if (path.empty())
      CHECK(component == ComponentInstanceId("root.hidden"));
    if (path == std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("left")}) {
      CHECK(component == ComponentInstanceId("sleeve"));
      left_sleeve = true;
    }
    if (path == std::vector<SubassemblyInstanceId>{SubassemblyInstanceId("right")} &&
        component == ComponentInstanceId("rod") &&
        entry.endpoint.semantic_reference() == "hole.axis") {
      right_axis = true;
      REQUIRE(std::holds_alternative<geometry::AssemblyCylindricalSurfaceTargetDescriptor>(
          entry.target.resolved.descriptor));
      CHECK(entry.target.anchor.x == Approx(-80).margin(1e-6));
    }
  }
  CHECK(left_sleeve);
  CHECK(right_axis);
  tools.cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(snapshot(window.session()) == authored);
  REQUIRE(window.session().activate_assembly_design_view(default_design_view_id()));
  REQUIRE(tools.begin(ShellAssemblyTools::Command::Relationship));
  REQUIRE_FALSE(tools.targets().empty());
  for (const auto& target : tools.targets())
    CHECK(target.reference.component_instance() == ComponentInstanceId("root.visible"));
  tools.cancel();
  CHECK(snapshot(window.session()) == authored);
}
