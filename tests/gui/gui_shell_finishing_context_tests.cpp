#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;
using blcad::test::reference_checked;
using Catch::Approx;
namespace {
using Command = ShellFeatureTools::Command;
template <typename T> T& control(ShellWindow& window, const char* id) {
  auto* value = window.findChild<T*>(QString::fromLatin1(id));
  REQUIRE(value);
  return *value;
}
std::string snapshot(ShellWindow& window) {
  return reference_checked(serialize_part_document_to_json(*window.session().part_document()));
}
bool edges(Command command) {
  return command == Command::Fillet || command == Command::Chamfer;
}
const char* name(Command command) {
  return command == Command::Fillet    ? "fillet"
         : command == Command::Chamfer ? "chamfer"
         : command == Command::Shell   ? "shell"
                                       : "draft";
}
QAction& action(ShellWindow& window, const char* id) {
  return control<QAction>(window, (std::string("blcad.action.context_") + id).c_str());
}
void context(ShellWindow& window, const char* id) {
  auto& button = control<QToolButton>(window, (std::string("blcad.context.") + id).c_str());
  REQUIRE(button.isVisible());
  REQUIRE(button.isEnabled());
  button.click();
  qApp->processEvents();
}
void fixture(ShellWindow& window) {
  REQUIRE(window.new_part("Finishing context"));
  REQUIRE(window.session().commit_part_transaction("Two boxes", [](PartDocument& part) {
    for (const auto& [id, value] : {std::pair{"width", 30.0}, {"height", 20.0}, {"depth", 10.0}})
      REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
    for (const char* id : {"a", "b"}) {
      const auto sketch_id = SketchId(std::string("sketch.") + id);
      auto sketch = reference_checked(Sketch::create(sketch_id, id, DatumPlaneId("datum.xy")));
      REQUIRE(sketch.add_profile(reference_checked(
          RectangleProfile::create(ProfileId(id), ParameterId("width"), ParameterId("height"),
                                   {id[0] == 'a' ? 0.0 : 60.0, 0}))));
      REQUIRE(part.add_sketch(std::move(sketch)));
      REQUIRE(part.add_body(reference_checked(Body::create(BodyId(id), id, BodyKind::Solid))));
      auto feature = reference_checked(Feature::create_additive_extrude(
          FeatureId(std::string("feature.") + id), id, sketch_id, ParameterId("depth")));
      REQUIRE(part.add_feature(reference_checked(
          feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
              FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id)))))));
    }
    return Result<std::size_t>::success(1);
  }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
void filter(ShellWindow& window, GuiSelectionKind kind) {
  auto& input = control<QComboBox>(window, "blcad.selection.filter");
  const int row = input.findData(selection_kind_bit(kind));
  REQUIRE(row >= 0);
  input.setCurrentIndex(row);
  qApp->processEvents();
}
geometry::FinishingSelectionTarget target(ShellWindow& window, Command command,
                                          const BodyId& body = BodyId("b")) {
  auto targets = reference_checked(geometry::FinishingSelectionBuilder::build(
      *window.session().part_document(), *window.session().part_shape_cache(), body,
      edges(command) ? PartFeatureInputRole::FilletEdge : PartFeatureInputRole::ShellRemovalFace));
  const std::string role = edges(command)              ? "top_front"
                           : command == Command::Shell ? "top"
                                                       : "right";
  const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& item) {
    return item.label.ends_with(" / " + role);
  });
  REQUIRE(found != targets.end());
  return *found;
}
ShellFeatureTools::FinishingContext select(ShellWindow& window, Command command) {
  filter(window, edges(command) ? GuiSelectionKind::Edge : GuiSelectionKind::Face);
  REQUIRE(window.viewport().select_semantic(target(window, command).semantic_id));
  qApp->processEvents();
  const auto source = window.feature_tools().finishing_context(command);
  REQUIRE(source);
  CHECK(source->body == BodyId("b"));
  return *source;
}
} // namespace

TEST_CASE(
    "Idle finishing contexts require one verified current edge or face under its model filter",
    "[gui][block136-finishing-context]") {
  ShellWindow window;
  fixture(window);
  CHECK(window.feature_tools().finishing_selection_items().empty());
  REQUIRE(window.viewport().select_semantic("body/b"));
  CHECK_FALSE(action(window, "fillet").isVisible());
  const auto before = snapshot(window);
  const auto source = select(window, Command::Fillet);
  CHECK(window.feature_tools().finishing_selection_items().size() == 24);
  CHECK(action(window, "fillet").isEnabled());
  CHECK(action(window, "chamfer").isEnabled());
  CHECK_FALSE(action(window, "shell").isVisible());
  CHECK_FALSE(action(window, "extrude").isVisible());
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::Edge, target(window, Command::Fillet, BodyId("a")).semantic_id}));
  window.refresh();
  CHECK_FALSE(action(window, "fillet").isVisible());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::Edge, target(window, Command::Shell).semantic_id}));
  window.refresh();
  CHECK_FALSE(window.feature_tools().finishing_context(Command::Fillet));
  action(window, "fillet").trigger();
  CHECK_FALSE(window.session().task().active());
  filter(window, GuiSelectionKind::Face);
  CHECK(window.feature_tools().finishing_selection_items().size() == 12);
  CHECK_FALSE(window.viewport().select_semantic(source.semantic_id));
  select(window, Command::Shell);
  CHECK(action(window, "shell").isEnabled());
  CHECK(action(window, "draft").isEnabled());
  CHECK_FALSE(action(window, "fillet").isVisible());
  CHECK(snapshot(window) == before);
  // Reusing the task form must preserve full controls after a hidden mode row
  // becomes visible, including the dependent second Chamfer value.
  for (const auto command : {Command::Draft, Command::Shell, Command::Fillet, Command::Chamfer}) {
    select(window, command);
    context(window, name(command));
    auto& mode = control<QComboBox>(window, "blcad.finishing.mode");
    if (command == Command::Shell || command == Command::Chamfer) {
      REQUIRE(mode.isVisible());
      CHECK(mode.height() >= mode.minimumSizeHint().height());
    }
    if (command == Command::Chamfer) {
      mode.setCurrentIndex(1);
      qApp->processEvents();
      auto& second = control<QLineEdit>(window, "blcad.finishing.secondary");
      REQUIRE(second.isVisible());
      const auto* row = second.parentWidget();
      const auto* controls = row->parentWidget();
      INFO("Form preferred/minimum/current height: " << controls->layout()->sizeHint().height()
                                                     << "/" << controls->minimumHeight() << "/"
                                                     << controls->height());
      CHECK(controls->rect().contains(row->geometry()));
      CHECK(second.height() >= second.minimumSizeHint().height());
      CHECK(second.parentWidget()->rect().contains(second.geometry()));
    }
    context(window, "cancel");
  }
  CHECK(snapshot(window) == before);
}

TEST_CASE(
    "Finishing contexts seed the second Body and every cancel path restores full preselection",
    "[gui][block136-finishing-context]") {
  const auto command = GENERATE(Command::Fillet, Command::Chamfer, Command::Shell, Command::Draft);
  const auto route = GENERATE(0, 1, 2);
  CAPTURE(name(command), route);
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  const auto other = target(window, command, BodyId("a"));
  window.viewport().set_selection_semantic_scope(
      std::vector<std::string>{source.semantic_id, other.semantic_id});
  const auto entries = window.session().selection().entries();
  REQUIRE(entries.size() == 1);
  const auto viewport = window.viewport().selected_semantic();
  REQUIRE(viewport);
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  CHECK(tools.command() == command);
  CHECK(tools.finishing_body_editor().currentData().toString() == "b");
  CHECK(tools.finishing_target_editor().selectedItems().size() == 1);
  CHECK(window.session().selection().empty());
  CHECK_FALSE(window.viewport().selected_semantic());
  CHECK_FALSE(window.viewport().select_semantic(other.semantic_id));
  CHECK_FALSE(tools.accept_finishing_selection(
      {edges(command) ? GuiSelectionKind::Edge : GuiSelectionKind::Face, other.semantic_id}));
  REQUIRE(tools.set_numeric_text(command == Command::Draft ? "4 deg" : "1.5 mm"));
  CHECK(snapshot(window) == before);
  if (route == 0)
    context(window, "cancel");
  else if (route == 1)
    control<QPushButton>(window, "blcad.feature.cancel").click();
  else {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == viewport->semantic_id);
  REQUIRE(window.session().selection().entries().size() == entries.size());
  CHECK(window.session().selection().entries()[0].kind == entries[0].kind);
  CHECK(window.session().selection().entries()[0].semantic_id == entries[0].semantic_id);
  CHECK(window.session().selection().entries()[0].candidate_id == entries[0].candidate_id);
  CHECK(action(window, name(command)).isEnabled());
}

TEST_CASE("Finishing context Apply rejects an empty target and publishes exactly one atomic edit",
          "[gui][block136-finishing-context]") {
  const auto command = GENERATE(Command::Fillet, Command::Chamfer, Command::Shell, Command::Draft);
  CAPTURE(name(command));
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  const auto before = snapshot(window);
  const auto parameters = window.session().part_document()->parameters().size();
  context(window, name(command));
  auto& tools = window.feature_tools();
  tools.finishing_target_editor().clearSelection();
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.select_finishing_context(source));
  REQUIRE(tools.select_finishing_context(source)); // Idempotent first-input handoff.
  CHECK(tools.finishing_target_editor().selectedItems().size() == 1);
  REQUIRE(tools.apply());
  const auto* part = window.session().part_document();
  CHECK(part->parameters().size() == parameters + 1);
  if (command == Command::Fillet)
    CHECK(part->fillet_features().back().target_body() == BodyId("b"));
  else if (command == Command::Chamfer)
    CHECK(part->chamfer_features().back().target_body() == BodyId("b"));
  else if (command == Command::Shell)
    CHECK(part->shell_features().back().target_body() == BodyId("b"));
  else
    CHECK(part->draft_features().back().target_body() == BodyId("b"));
  const auto* unchanged = window.session().part_shape_cache()->find_body_shape(BodyId("a"));
  REQUIRE(unchanged);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*unchanged).volume_mm3 ==
        Approx(6000).margin(1e-5));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}

TEST_CASE("Finishing contexts guard capture foreign tasks hidden Bodies and removed producers",
          "[gui][block136-finishing-context]") {
  ShellWindow window;
  fixture(window);
  const auto source = select(window, Command::Fillet);
  const auto before = snapshot(window);
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(action(window, "fillet").isVisible());
  action(window, "fillet").trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  CHECK(action(window, "fillet").isEnabled());
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  CHECK(window.feature_tools().finishing_selection_items().empty());
  CHECK_FALSE(action(window, "fillet").isVisible());
  action(window, "cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  window.refresh();
  CHECK(snapshot(window) == before);
  select(window, Command::Fillet);
  REQUIRE(window.session().commit_part_transaction("Hide target", [](PartDocument& part) {
    return part.set_body_visibility(BodyId("b"), BodyVisibility::Hidden);
  }));
  window.refresh();
  CHECK_FALSE(window.feature_tools().finishing_context_available(source));
  CHECK_FALSE(action(window, "fillet").isVisible());
  REQUIRE(window.undo());
  REQUIRE(window.undo()); // Remove the fixture's producing features and Bodies.
  CHECK_FALSE(window.feature_tools().finishing_context_available(source));
  CHECK(window.feature_tools().finishing_selection_items().empty());
}

TEST_CASE("Finishing context Save and document replacement preserve committed intent only",
          "[gui][block136-finishing-context]") {
  const auto command = GENERATE(Command::Fillet, Command::Chamfer, Command::Shell, Command::Draft);
  CAPTURE(name(command));
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("finishing-context.blcad");
  REQUIRE(window.save_to(path));
  select(window, command);
  const auto before = snapshot(window);
  context(window, name(command));
  REQUIRE(window.save_to(path));
  CHECK(window.session().task().active());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(reference_checked(serialize_part_document_to_json(*reopened.part_document())) == before);
  SECTION("Open") {
    REQUIRE(window.open_path(path));
  }
  SECTION("New Part") {
    REQUIRE(window.new_part("Next"));
  }
  SECTION("Failed Open") {
    CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
    CHECK(snapshot(window) == before);
  }
  SECTION("Cancelled replacement") {
    context(window, "cancel");
    REQUIRE(window.session().commit_part_transaction("Unsaved", [](PartDocument& part) {
      return part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId("unsaved"), "Unsaved", reference_checked(Quantity::length_mm(7)))));
    }));
    window.refresh();
    select(window, command);
    const auto dirty = snapshot(window);
    context(window, name(command));
    bool answered = false;
    QTimer::singleShot(0, [&] {
      if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        answered = true;
        dialog->button(QMessageBox::Cancel)->click();
      }
    });
    CHECK_FALSE(window.new_part("Cancelled"));
    REQUIRE(answered);
    CHECK(window.session().task().active());
    CHECK(action(window, "cancel").isEnabled());
    context(window, "cancel");
    CHECK(snapshot(window) == dirty);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
