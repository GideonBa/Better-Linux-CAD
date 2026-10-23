#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace {
using C = ShellAssemblyTools::Command;
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
QString example() {
  return QStringLiteral(BLCAD_SOURCE_DIR) + "/examples/revolute_joint.blcad.project.json";
}
std::string snapshot(ShellWindow& w) {
  return checked(serialize_project_to_json(*w.session().project()));
}
void fixture(ShellWindow& window, bool components = true, bool motion = false) {
  GuiDocumentSession source;
  REQUIRE(source.open_file(example().toStdString()));
  auto part = source.project()->part_documents().front();
  REQUIRE(window.new_project("Assembly inputs"));
  auto setup = window.session().commit_project_transaction(
      "Assembly inputs", [part, components, motion](Project& project) {
        REQUIRE(project.add_part_document(part));
        if (components) {
          REQUIRE(project.assembly().add_member_part(part.id()));
          REQUIRE(project.assembly().add_component_instance(checked(ComponentInstance::create(
              ComponentInstanceId("fixed"), "Fixed", part.id(), ComponentVisibility::Visible,
              ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
          REQUIRE(project.assembly().add_component_instance(checked(ComponentInstance::create(
              ComponentInstanceId("moving"), "Moving", part.id(), ComponentVisibility::Visible,
              ComponentSuppressionState::Active, ComponentGroundingState::Free,
              motion ? RigidTransform{} : RigidTransform{{30, 0, 10}, {}}))));
        }
        if (motion) {
          auto slot = checked(AssemblyJointCoordinateSlot::create(
              AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateKind::Angular,
              checked(Quantity::angle_deg(0)), checked(Quantity::angle_deg(-90)),
              checked(Quantity::angle_deg(90))));
          REQUIRE(project.assembly().add_joint(checked(AssemblyJoint::create(
              AssemblyJointId("joint.fixture"), "Revolute", AssemblyJointType::Revolute,
              checked(AssemblyConstraintTarget::create(ComponentInstanceId("fixed"),
                                                       "feature.hole.seat")),
              checked(AssemblyConstraintTarget::create(ComponentInstanceId("moving"),
                                                       "feature.hole.seat")),
              AssemblyJointState::Active, {slot}))));
        }
        return Result<std::size_t>::success(1);
      });
  REQUIRE(setup);
  window.refresh();
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  qApp->processEvents();
}
void combo(QComboBox& input, const QString& key) {
  int i = input.findData(key);
  REQUIRE(i >= 0);
  input.setCurrentIndex(i);
}
QAction& action(ShellWindow& w, const char* id) {
  auto* a = w.assembly_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(a);
  return *a;
}
QPushButton& button(ShellWindow& w, const char* id) {
  auto* b = w.findChild<QPushButton*>(QStringLiteral("blcad.assembly.") + id);
  REQUIRE(b);
  return *b;
}
int target(ShellAssemblyTools& tools, const char* component, const char* semantic) {
  auto found = std::find_if(tools.targets().begin(), tools.targets().end(), [&](const auto& value) {
    return value.reference.component_instance() == ComponentInstanceId(component) &&
           value.reference.semantic_reference() == semantic;
  });
  REQUIRE(found != tools.targets().end());
  return std::distance(tools.targets().begin(), found);
}
void reference(ShellAssemblyTools& tools, QComboBox& editor, const char* component,
               const char* semantic) {
  auto i = target(tools, component, semantic);
  auto row = editor.findData(i);
  REQUIRE(row > 0);
  editor.setCurrentIndex(row);
}
void capture(ShellWindow& w, const QString& name) {
  auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(name + ".png")));
}
void history(ShellWindow& w, const std::string& before) {
  auto after = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == after);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(w.save_to(dir.filePath("assembly.blcad.project.json")));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(dir.filePath("assembly.blcad.project.json").toStdString()));
  CHECK(checked(serialize_project_to_json(*opened.project())) == after);
}
} // namespace
TEST_CASE("Assembly insertion provisions one occurrence and member Part only on Apply",
          "[gui][block136-assembly]") {
  ShellWindow window;
  fixture(window, false);
  auto& tools = window.assembly_tools();
  auto before = snapshot(window);
  REQUIRE(action(window, "assembly_insert_component").isEnabled());
  action(window, "assembly_insert_component").trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.apply());
  combo(tools.definition_editor(), "part.revolute_plate");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(tools.manipulators().handles().size() == 3);
  tools.placement_editor(0).setValue(25);
  tools.placement_editor(5).setValue(30);
  tools.grounding_editor().setChecked(true);
  CHECK(snapshot(window) == before);
  capture(window, "assembly-insert");
  REQUIRE(tools.apply());
  const auto& assembly = window.session().project()->assembly();
  REQUIRE(assembly.component_instance_count() == 1);
  CHECK(assembly.member_part_count() == 1);
  CHECK(assembly.component_instances().front().transform().translation_mm.x == 25);
  CHECK(assembly.component_instances().front().transform().rotation_deg.z == 30);
  CHECK(assembly.component_instances().front().grounding_state() ==
        ComponentGroundingState::Grounded);
  history(window, before);
}
TEST_CASE(
    "Assembly placement handles hidden and suppressed occurrences and restores preview on Cancel",
    "[gui][block136-assembly]") {
  bool suppress = false;
  SECTION("Hidden") {}
  SECTION("Suppressed") {
    suppress = true;
  }
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  REQUIRE(tools.begin(C::Placement));
  combo(tools.occurrence_editor(), "component:moving");
  REQUIRE(window.viewport().feature_preview_active());
  tools.placement_editor(1).setValue(12);
  if (suppress)
    tools.suppression_editor().setChecked(true);
  else
    tools.visibility_editor().setChecked(false);
  CHECK(window.viewport().feature_preview_count() == 0);
  CHECK(window.viewport().feature_preview_replaces(
      "component/" + window.session().project()->assembly().id().value() + "/moving"));
  CHECK(button(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  tools.cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK_FALSE(window.viewport().feature_preview_replaces(
      "component/" + window.session().project()->assembly().id().value() + "/moving"));
  CHECK(snapshot(window) == before);
  REQUIRE(tools.begin(C::Placement));
  combo(tools.occurrence_editor(), "component:moving");
  if (suppress)
    tools.suppression_editor().setChecked(true);
  else
    tools.visibility_editor().setChecked(false);
  REQUIRE(tools.apply());
  history(window, before);
}
TEST_CASE("Assembly relationship filters semantic endpoints and applies the solved pose atomically",
          "[gui][block136-assembly]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(C::Relationship));
  reference(tools, tools.first_editor(), "fixed", "feature.base_extrude.top");
  for (int row = 1; row < tools.second_editor().count(); ++row)
    CHECK(tools.targets()
              .at(tools.second_editor().itemData(row).toInt())
              .reference.component_instance() == ComponentInstanceId("moving"));
  tools.pick_editor().setCurrentIndex(1);
  CHECK_FALSE(window.viewport().select_semantic(
      tools.targets()
          .at(target(tools, "fixed", "feature.base_extrude.bottom"))
          .presentation.semantic_id));
  reference(tools, tools.second_editor(), "moving", "feature.base_extrude.bottom");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  capture(window, "assembly-mate");
  REQUIRE(tools.apply());
  CHECK(window.session().project()->assembly().constraint_count() == 1);
  CHECK(window.session()
            .project()
            ->assembly()
            .find_component_instance(ComponentInstanceId("moving"))
            ->transform()
            .translation_mm.z == Catch::Approx(2).margin(1e-5));
  history(window, before);
  REQUIRE(tools.begin(C::Solve));
  REQUIRE(button(window, "apply").isEnabled());
  const auto solved = snapshot(window);
  REQUIRE(tools.apply());
  CHECK(window.session().project()->assembly().constraint_count() ==
        1); // No extra relationship is authored by Solve.
  history(window, solved);
}
TEST_CASE("Assembly joint kinds retain ordered typed coordinates and oriented semantic targets",
          "[gui][block136-assembly]") {
  int kind = 0;
  SECTION("Revolute") {}
  SECTION("Prismatic") {
    kind = 1;
  }
  SECTION("Cylindrical") {
    kind = 2;
  }
  SECTION("Planar") {
    kind = 3;
  }
  SECTION("Spherical") {
    kind = 4;
  }
  ShellWindow window;
  fixture(window, true, true);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(C::Joint));
  tools.joint_type_editor().setCurrentIndex(kind);
  const char* semantic = "feature.hole.seat";
  if (kind == 4) {
    auto found = std::find_if(tools.targets().begin(), tools.targets().end(), [](const auto& item) {
      return item.reference.component_instance() == ComponentInstanceId("fixed") &&
             std::holds_alternative<geometry::AssemblyPointTargetDescriptor>(
                 item.resolved.descriptor);
    });
    REQUIRE(found != tools.targets().end());
    semantic = found->reference.semantic_reference().c_str();
    reference(tools, tools.first_editor(), "fixed", semantic);
    reference(tools, tools.second_editor(), "moving", semantic);
  } else {
    reference(tools, tools.first_editor(), "fixed", semantic);
    reference(tools, tools.second_editor(), "moving", semantic);
  }
  CAPTURE(kind);
  INFO(window.findChild<QLabel*>("blcad.assembly.summary")->text().toStdString());
  REQUIRE(button(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto& joint = window.session().project()->assembly().joints().back();
  CHECK(joint.type() == static_cast<AssemblyJointType>(kind));
  CHECK(joint.coordinate_slots().size() == (kind == 4 ? 0 : kind == 3 ? 3 : kind == 2 ? 2 : 1));
  history(window, before);
}
TEST_CASE(
    "Assembly motion slider numeric limits and preview leave authored state untouched until Apply",
    "[gui][block136-assembly]") {
  ShellWindow window;
  fixture(window, true, true);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(C::Motion));
  combo(tools.joint_editor(), "joint.fixture");
  REQUIRE(window.viewport().feature_preview_active());
  tools.slider().setValue(750);
  CHECK(tools.value_editor().value() == Catch::Approx(45));
  tools.value_editor().setValue(120);
  CHECK(tools.value_editor().value() == Catch::Approx(45));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  capture(window, "assembly-motion");
  REQUIRE(tools.apply());
  CHECK(window.session()
            .project()
            ->assembly()
            .find_joint(AssemblyJointId("joint.fixture"))
            ->coordinate_deg() == Catch::Approx(45));
  history(window, before);
}
TEST_CASE("Assembly definitions load atomically and subassembly insertion has a posed preview",
          "[gui][block136-assembly]") {
  ShellWindow window;
  REQUIRE(window.new_project("Definitions"));
  auto& tools = window.assembly_tools();
  auto before = snapshot(window);
  REQUIRE(tools.import_definition(example()));
  CHECK(window.session().project()->child_assembly_document_count() == 1);
  history(window, before);
  const auto imported = snapshot(window);
  CHECK_FALSE(tools.import_definition(example()));
  CHECK(snapshot(window) == imported);
  REQUIRE(tools.begin(C::InsertSubassembly));
  combo(tools.definition_editor(), "assembly.revolute_joint");
  tools.placement_editor(0).setValue(50);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == imported);
  capture(window, "assembly-subassembly");
  REQUIRE(tools.apply());
  CHECK(window.session().project()->assembly().subassembly_instance_count() == 1);
  history(window, imported);
  const auto placed = snapshot(window);
  const auto id = window.session().project()->assembly().subassembly_instances().front().id();
  REQUIRE(tools.begin(C::Placement));
  combo(tools.occurrence_editor(), "subassembly:" + QString::fromStdString(id.value()));
  tools.placement_editor(0).setValue(75);
  tools.visibility_editor().setChecked(false);
  CHECK(window.viewport().feature_preview_count() == 0);
  CHECK(snapshot(window) == placed);
  REQUIRE(tools.apply());
  CHECK(window.session().project()->assembly().subassembly_instances().front().visibility() ==
        ComponentVisibility::Hidden);
  history(window, placed);
}
TEST_CASE(
    "Assembly Part definition import reuses identical intent and rejects conflicts atomically",
    "[gui][block136-assembly]") {
  GuiDocumentSession source;
  REQUIRE(source.open_file(example().toStdString()));
  const auto& part = source.project()->part_documents().front();
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("part.blcad.json");
  auto write = [&](const PartDocument& definition) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto json = checked(serialize_part_document_to_json(definition));
    REQUIRE(file.write(QByteArray::fromStdString(json)) == static_cast<qint64>(json.size()));
  };
  write(part);
  ShellWindow window;
  REQUIRE(window.new_project("Part imports"));
  const auto before = snapshot(window);
  auto& tools = window.assembly_tools();
  REQUIRE(tools.import_definition(path));
  history(window, before);
  REQUIRE(tools.import_definition(example()));
  CHECK(window.session().project()->part_document_count() == 1);
  const auto imported = snapshot(window);
  write(checked(PartDocument::create(part.id(), "Conflicting definition")));
  CHECK_FALSE(tools.import_definition(path));
  CHECK(snapshot(window) == imported);
}
TEST_CASE("Assembly rejects stale candidates missing definitions and incomplete relationships",
          "[gui][block136-assembly]") {
  ShellWindow empty;
  CHECK_FALSE(empty.assembly_tools().begin(C::Placement));
  REQUIRE(empty.new_project("Empty"));
  CHECK_FALSE(action(empty, "assembly_placement").isEnabled());
  CHECK_FALSE(empty.assembly_tools().import_definition("/tmp/blcad-no-such-definition.json"));
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  auto before = snapshot(window);
  REQUIRE(tools.begin(C::Relationship));
  CHECK_FALSE(tools.apply());
  tools.cancel();
  CHECK(snapshot(window) == before);
  REQUIRE(tools.begin(C::Placement));
  combo(tools.occurrence_editor(), "component:moving");
  REQUIRE(window.session().commit_project_transaction("External pose", [](Project& p) {
    return p.assembly().set_component_instance_transform(ComponentInstanceId("fixed"),
                                                         {{1, 0, 0}, {}});
  }));
  CHECK_FALSE(tools.apply());
  window.refresh();
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
TEST_CASE(
    "Native Assembly picks an occurrence then drags translates rotates and refines numeric input",
    "[gui][block136-assembly][native-shell-assembly]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT Assembly picking");
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  auto& start = action(window, "assembly_placement");
  QToolButton* open = nullptr;
  for (auto* candidate : window.findChildren<QToolButton*>())
    if (candidate->defaultAction() == &start)
      open = candidate;
  REQUIRE(open);
  QWidget* ancestor = open->parentWidget();
  while (ancestor && !qobject_cast<QScrollArea*>(ancestor))
    ancestor = ancestor->parentWidget();
  REQUIRE(ancestor);
  static_cast<QScrollArea*>(ancestor)->ensureWidgetVisible(open);
  qApp->processEvents();
  open->click();
  REQUIRE(tools.active());
  combo(tools.occurrence_editor(), "component:moving");
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  auto send = [&](QEvent::Type type, GuiSketchScreenPoint p, Qt::MouseButtons buttons) {
    QPointF local(p.x, p.y);
    QMouseEvent e(type, local, window.viewport().mapToGlobal(local.toPoint()),
                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                  Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &e);
    qApp->processEvents();
  };
  const auto pick = checked(window.viewport().model_to_screen({3, 4, 2}));
  send(QEvent::MouseButtonPress, pick, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, pick, Qt::NoButton);
  REQUIRE(tools.occurrence_editor().currentData().toString() == "component:fixed");
  REQUIRE(window.model_browser().currentItem());
  CHECK(window.model_browser().currentItem()->text(0) == "Fixed");
  auto presentations = tools.manipulators().presentations();
  auto x = std::find_if(presentations.begin(), presentations.end(), [](const auto& p) {
    return p.handle_id == "assembly.placement.translate.x";
  });
  REQUIRE(x != presentations.end());
  auto endpoint = x->endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  send(QEvent::MouseMove, {endpoint.x + 20, endpoint.y}, Qt::LeftButton);
  REQUIRE(tools.set_drag_text("15 mm"));
  send(QEvent::MouseButtonRelease, {endpoint.x + 20, endpoint.y}, Qt::NoButton);
  CHECK(tools.placement_editor(0).value() == 15);
  tools.placement_drag_editor().setCurrentIndex(3);
  presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1);
  endpoint = presentations.front().endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  REQUIRE(tools.set_drag_text("30 deg"));
  send(QEvent::MouseButtonRelease, endpoint, Qt::NoButton);
  CHECK(tools.placement_editor(5).value() == 30);
  endpoint = tools.manipulators().presentations().front().endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE(tools.set_drag_text("60 deg"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &escape);
  CHECK(tools.placement_editor(5).value() == 30);
  CHECK(snapshot(window) == before);
  capture(window, "native-assembly-placement");
  auto& apply = button(window, "apply");
  REQUIRE(apply.isVisible());
  REQUIRE(window.childAt(apply.mapTo(&window, apply.rect().center())) == &apply);
  apply.click();
  REQUIRE_FALSE(tools.active());
  history(window, before);
}
TEST_CASE("Native Assembly picks a compatibility-filtered second relationship reference",
          "[gui][block136-assembly][native-shell-assembly]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT Assembly reference picking");
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(C::Relationship));
  reference(tools, tools.first_editor(), "fixed", "feature.base_extrude.top");
  tools.pick_editor().setCurrentIndex(1);
  window.viewport().set_standard_view(GuiStandardView::Bottom);
  window.viewport().fit_all();
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  const auto screen = checked(window.viewport().model_to_screen({33, 4, 10}));
  const QPointF local(screen.x, screen.y);
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  }
  REQUIRE(tools.second_editor().currentIndex() > 0);
  const auto& selected = tools.targets().at(tools.second_editor().currentData().toInt());
  CHECK(selected.reference.component_instance() == ComponentInstanceId("moving"));
  CHECK(std::holds_alternative<geometry::AssemblyPlanarTargetDescriptor>(
      selected.resolved.descriptor));
  INFO(window.findChild<QLabel*>("blcad.assembly.summary")->text().toStdString());
  REQUIRE(button(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  capture(window, "native-assembly-relationship");
  REQUIRE(tools.apply());
  history(window, before);
}
TEST_CASE("Native Assembly joint motion drives an angular handle with typed refinement",
          "[gui][block136-assembly][native-shell-assembly]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT Assembly motion");
  ShellWindow window;
  fixture(window, true, true);
  auto& tools = window.assembly_tools();
  auto before = snapshot(window);
  REQUIRE(tools.begin(C::Motion));
  combo(tools.joint_editor(), "joint.fixture");
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1);
  auto endpoint = presentations.front().endpoint;
  auto send = [&](QEvent::Type type) {
    QPointF p(endpoint.x, endpoint.y);
    QMouseEvent e(type, p, window.viewport().mapToGlobal(p.toPoint()), Qt::LeftButton,
                  type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &e);
    qApp->processEvents();
  };
  send(QEvent::MouseButtonPress);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  REQUIRE(tools.set_drag_text("25 deg"));
  send(QEvent::MouseButtonRelease);
  CHECK(tools.value_editor().value() == 25);
  CHECK(snapshot(window) == before);
  capture(window, "native-assembly-motion");
  REQUIRE(tools.apply());
  history(window, before);
}
