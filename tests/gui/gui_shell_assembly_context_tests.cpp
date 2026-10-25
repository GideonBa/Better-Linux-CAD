#include "blcad/core/project_json.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellAssemblyTools::Command;
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
template <typename T> T& control(ShellWindow& window, const std::string& id) {
  auto* value = window.findChild<T*>(QString::fromStdString(id));
  REQUIRE(value);
  return *value;
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_project_to_json(*window.session().project()));
}
void fixture(ShellWindow& window) {
  REQUIRE(window.open_path(QStringLiteral(BLCAD_SOURCE_DIR) +
                           "/examples/revolute_joint.blcad.project.json"));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
const char* name(Command command) {
  return command == Command::Placement      ? "assembly_placement"
         : command == Command::Motion       ? "assembly_motion"
         : command == Command::Solve        ? "assembly_solve"
         : command == Command::Relationship ? "assembly_relationship"
                                            : "assembly_joint";
}
void click(ShellWindow& window, const char* id) {
  auto& button = control<QToolButton>(window, std::string("blcad.context.") + id);
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  button.click();
  qApp->processEvents();
}
ShellAssemblyTools::Context select(ShellWindow& window, Command command) {
  auto& filter = control<QComboBox>(window, "blcad.selection.filter");
  const bool reference = command == Command::Relationship || command == Command::Joint;
  filter.setCurrentIndex(filter.findData(selection_kind_bit(
      reference ? GuiSelectionKind::AssemblyTarget : GuiSelectionKind::Component)));
  const auto& assembly = window.session().project()->assembly();
  if (reference) {
    const auto targets =
        checked(geometry::AssemblyInteractionGeometry::targets(*window.session().project()));
    const auto target = std::find_if(targets.begin(), targets.end(), [&](const auto& item) {
      return item.reference.component_instance() == assembly.component_instances().back().id() &&
             item.reference.semantic_reference() == "feature.hole.seat";
    });
    REQUIRE(target != targets.end());
    REQUIRE(window.viewport().select_semantic(target->presentation.semantic_id));
  } else {
    REQUIRE(window.viewport().select_semantic("component/" + assembly.id().value() + "/" +
                                              assembly.component_instances().back().id().value()));
  }
  const auto source = window.assembly_tools().context(command);
  REQUIRE(source);
  return *source;
}
} // namespace

TEST_CASE("Assembly contexts hand off exact occurrence or semantic target and restore cancellation",
          "[gui][block136-assembly-context]") {
  const auto command = GENERATE(Command::Placement, Command::Relationship, Command::Joint,
                                Command::Motion, Command::Solve);
  const auto route = GENERATE(0, 1, 2);
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  window.viewport().set_selection_semantic_scope(
      std::vector<std::string>{source.selection.semantic_id});
  const auto scope = window.viewport().selection_semantic_scope();
  click(window, name(command));
  auto& owner = window.assembly_tools();
  REQUIRE(owner.active());
  REQUIRE(owner.select_context(source));
  REQUIRE(owner.select_context(source));
  auto wrong_kind = source.selection;
  wrong_kind.kind = GuiSelectionKind::Body;
  CHECK_FALSE(owner.accept_selection(wrong_kind));
  CHECK(window.session().selection().empty());
  if (command == Command::Placement)
    CHECK(owner.occurrence_editor().currentData().toString().toStdString() ==
          "component:" + source.source);
  if (command == Command::Motion)
    CHECK(owner.joint_editor().currentData().toString().toStdString() == source.source);
  if (command == Command::Joint || command == Command::Relationship) {
    const auto index = owner.first_editor().currentData().toInt();
    REQUIRE(index >= 0);
    CHECK(owner.targets().at(index).presentation.semantic_id == source.source);
    CHECK(owner.second_editor().currentData().toInt() == -1);
    CHECK(owner.pick_editor().currentIndex() == 1);
    CHECK_FALSE(owner.apply());
  }
  CHECK(snapshot(window) == before);
  if (route == 0)
    click(window, "cancel");
  else if (route == 1)
    control<QPushButton>(window, "blcad.assembly.cancel").click();
  else {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  CHECK_FALSE(owner.active());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(window.session().selection().size() == 1);
  CHECK(window.session().selection().entries().front() == source.selection);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(*window.viewport().selected_semantic() == source.selection);
}

TEST_CASE(
    "Assembly contexts reject wrong identities multiple picks stale sources and foreign tasks",
    "[gui][block136-assembly-context]") {
  ShellWindow window;
  fixture(window);
  auto source = select(window, Command::Placement);
  auto altered = source;
  altered.source = "missing";
  CHECK_FALSE(window.assembly_tools().context_available(altered));
  altered = source;
  altered.selection.kind = GuiSelectionKind::Body;
  CHECK_FALSE(window.assembly_tools().context_available(altered));
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"missing"});
  REQUIRE(window.session().selection().add(source.selection));
  CHECK_FALSE(window.assembly_tools().context(Command::Placement));
  window.viewport().set_selection_semantic_scope(std::nullopt);
  select(window, Command::Placement);
  REQUIRE(window.session().selection().add({GuiSelectionKind::Component, "another"}));
  CHECK_FALSE(window.assembly_tools().context(Command::Placement));
  select(window, Command::Placement);
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  control<QAction>(window, "blcad.action.context_assembly_placement").trigger();
  control<QAction>(window, "blcad.action.context_cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  const auto motion = select(window, Command::Motion);
  REQUIRE(
      window.session().commit_project_transaction("Second candidate joint", [](Project& project) {
        const auto original = project.assembly().joints().front();
        return project.assembly().add_joint(
            checked(AssemblyJoint::create(AssemblyJointId("joint.ambiguous"), "Another joint",
                                          original.type(), original.target_a(), original.target_b(),
                                          original.state(), original.coordinate_slots())));
      }));
  window.refresh();
  CHECK_FALSE(window.assembly_tools().context_available(motion));
  CHECK_FALSE(control<QAction>(window, "blcad.action.context_assembly_motion").isVisible());
  REQUIRE(window.undo());
  CHECK(window.assembly_tools().context_available(motion));
  const ComponentInstanceId component(source.source);
  REQUIRE(
      window.session().commit_project_transaction("Hide occurrence", [component](Project& project) {
        return project.assembly().set_component_instance_visibility(component,
                                                                    ComponentVisibility::Hidden);
      }));
  window.refresh();
  CHECK_FALSE(window.assembly_tools().context_available(source));
  CHECK_FALSE(window.assembly_tools().select_context(source));
}

TEST_CASE("Assembly context motion publishes one atomic pose and canonical history",
          "[gui][block136-assembly-context]") {
  ShellWindow window;
  fixture(window);
  const auto source = select(window, Command::Motion);
  const auto before = snapshot(window);
  click(window, name(Command::Motion));
  window.assembly_tools().value_editor().setValue(30);
  CHECK(snapshot(window) == before);
  REQUIRE(window.assembly_tools().apply());
  const auto after = snapshot(window);
  CHECK(after != before);
  CHECK_FALSE(window.viewport().selection_semantic_scope());
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("context.blcad.project.json");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == after);
}

TEST_CASE("Assembly context drafts do not leak into Save or survive document replacement",
          "[gui][block136-assembly-context]") {
  const auto command = GENERATE(Command::Placement, Command::Relationship, Command::Joint,
                                Command::Motion, Command::Solve);
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("context.blcad.project.json");
  REQUIRE(window.save_to(path));
  select(window, command);
  const auto before = snapshot(window);
  click(window, name(command));
  REQUIRE(window.save_to(path));
  CHECK(window.session().task().active());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_project_to_json(*reopened.project())) == before);
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("New") {
    REQUIRE(window.new_project("Next"));
  }
  SECTION("Failed Open") {
    CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
    CHECK(snapshot(window) == before);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
