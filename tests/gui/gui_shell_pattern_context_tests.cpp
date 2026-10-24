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
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <numbers>

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
const char* name(Command command) {
  return command == Command::LinearPattern     ? "linear_pattern"
         : command == Command::CircularPattern ? "circular_pattern"
                                               : "body_mirror";
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
  REQUIRE(window.new_part("Pattern context"));
  REQUIRE(window.session().commit_part_transaction("Box and cylinder", [](PartDocument& part) {
    for (const auto& [id, value] : {std::pair{"width", 10.0}, {"diameter", 8.0}, {"depth", 10.0}})
      REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
          ParameterId(id), id, reference_checked(Quantity::length_mm(value))))));
    for (const char* id : {"a", "b"}) {
      const auto sketch_id = SketchId(std::string("sketch.") + id);
      auto sketch = reference_checked(Sketch::create(sketch_id, id, DatumPlaneId("datum.xy")));
      if (id[0] == 'a')
        REQUIRE(sketch.add_profile(reference_checked(
            RectangleProfile::create(ProfileId(id), ParameterId("width"), ParameterId("width")))));
      else
        REQUIRE(sketch.add_profile(reference_checked(
            CircleProfile::create(ProfileId(id), ParameterId("diameter"), {30, 24}))));
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
ShellFeatureTools::PatternContext select(ShellWindow& window) {
  auto& filter = control<QComboBox>(window, "blcad.selection.filter");
  filter.setCurrentIndex(filter.findData(selection_kind_bit(GuiSelectionKind::Body)));
  REQUIRE(window.viewport().select_semantic("body/b"));
  const auto source = window.feature_tools().pattern_context();
  REQUIRE(source);
  CHECK(source->body == BodyId("b"));
  return *source;
}
} // namespace

TEST_CASE("Pattern contexts require exactly one current solid Body and accept curved sources",
          "[gui][block136-pattern-context]") {
  ShellWindow window;
  fixture(window);
  const auto before = snapshot(window);
  const auto source = select(window);
  const auto finishing = reference_checked(geometry::FinishingSelectionBuilder::build(
      *window.session().part_document(), *window.session().part_shape_cache(), source.body,
      PartFeatureInputRole::FilletEdge));
  CHECK(finishing.empty()); // Cylinder eligibility does not depend on rectangular roles.
  for (const auto id : {"linear_pattern", "circular_pattern", "body_mirror"})
    CHECK(action(window, id).isEnabled());
  CHECK_FALSE(action(window, "fillet").isVisible());
  CHECK_FALSE(action(window, "extrude").isVisible());
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/a"}));
  window.refresh();
  CHECK_FALSE(action(window, "linear_pattern").isVisible());
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/missing"}));
  window.refresh();
  CHECK_FALSE(window.feature_tools().pattern_context());
  auto& filter = control<QComboBox>(window, "blcad.selection.filter");
  filter.setCurrentIndex(0);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::Face, "body/b"}));
  window.refresh();
  CHECK_FALSE(window.feature_tools().pattern_context());
  action(window, "linear_pattern").trigger();
  CHECK_FALSE(window.session().task().active());
  select(window);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/a"});
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/b"}));
  window.refresh();
  CHECK_FALSE(window.feature_tools().pattern_context());
  window.viewport().set_selection_semantic_scope(std::nullopt);
  filter.setCurrentIndex(filter.findData(selection_kind_bit(GuiSelectionKind::Face)));
  CHECK_FALSE(window.feature_tools().pattern_context());
  CHECK(snapshot(window) == before);
}

TEST_CASE(
    "Pattern context cancellation restores complete selection after task-owned reference picks",
    "[gui][block136-pattern-context]") {
  const auto command =
      GENERATE(Command::LinearPattern, Command::CircularPattern, Command::BodyMirror);
  const auto route = GENERATE(0, 1, 2);
  CAPTURE(name(command), route);
  ShellWindow window;
  fixture(window);
  const auto source = select(window);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/b"});
  const auto entries = window.session().selection().entries();
  const auto viewport = window.viewport().selected_semantic();
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  REQUIRE(tools.pattern_source_editor().selectedItems().size() == 1);
  CHECK(tools.pattern_source_editor().selectedItems().front()->data(Qt::UserRole).toString() ==
        "b");
  CHECK(window.session().selection().empty());
  CHECK_FALSE(window.viewport().selected_semantic());
  REQUIRE(window.viewport().select_semantic("body/a"));
  CHECK(tools.pattern_source_editor().selectedItems().size() == 2);
  REQUIRE(tools.select_pattern_context(source));
  REQUIRE(tools.select_pattern_context(source));
  CHECK(tools.pattern_source_editor().selectedItems().size() == 1);
  tools.pattern_reference_button().click();
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Datum));
  CHECK_FALSE(window.viewport().select_semantic("body/b"));
  REQUIRE(window.viewport().select_semantic(
      command == Command::BodyMirror ? "datum-plane/datum.yz" : "datum-axis/datum.axis.z"));
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));
  if (command != Command::BodyMirror)
    REQUIRE(tools.set_numeric_text(command == Command::LinearPattern ? "45 mm" : "270 deg"));
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
  CHECK_FALSE(window.viewport().select_semantic("body/a"));
  CHECK(action(window, name(command)).isEnabled());
}

TEST_CASE("Pattern context Apply publishes only its exact typed source in one atomic history entry",
          "[gui][block136-pattern-context]") {
  const auto command =
      GENERATE(Command::LinearPattern, Command::CircularPattern, Command::BodyMirror);
  CAPTURE(name(command));
  ShellWindow window;
  fixture(window);
  const auto source = select(window);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/b"});
  const auto scope = window.viewport().selection_semantic_scope();
  const auto before = snapshot(window);
  const auto parameters = window.session().part_document()->parameters().size();
  context(window, name(command));
  auto& tools = window.feature_tools();
  tools.pattern_source_editor().clearSelection();
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.select_pattern_context(source));
  REQUIRE(tools.apply());
  const auto* part = window.session().part_document();
  const auto check_source = [](const auto& sources) {
    REQUIRE(sources.size() == 1);
    CHECK(std::get<BodyId>(sources.front().source()) == BodyId("b"));
  };
  if (command == Command::LinearPattern)
    check_source(part->linear_pattern_features().back().sources());
  else if (command == Command::CircularPattern)
    check_source(part->circular_pattern_features().back().sources());
  else
    check_source(part->mirror_features().back().sources());
  CHECK(part->parameters().size() == parameters + (command == Command::LinearPattern     ? 2
                                                   : command == Command::CircularPattern ? 1
                                                                                         : 0));
  CHECK(part->bodies().size() == 3);
  const auto* shape =
      window.session().part_shape_cache()->find_body_shape(part->bodies().back().id());
  REQUIRE(shape);
  const auto result = geometry::RectangleExtrusionAdapter{}.summarize(*shape);
  const auto count = command == Command::BodyMirror ? 1 : 3;
  CHECK(result.solid_count == count);
  CHECK(result.volume_mm3 == Approx(count * 160 * std::numbers::pi).margin(1e-5));
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}

TEST_CASE("Pattern contexts guard mouse capture foreign tasks hidden sources and removed producers",
          "[gui][block136-pattern-context]") {
  ShellWindow window;
  fixture(window);
  const auto source = select(window);
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(action(window, "linear_pattern").isVisible());
  action(window, "linear_pattern").trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  CHECK_FALSE(action(window, "body_mirror").isVisible());
  action(window, "cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  window.refresh();
  REQUIRE(window.session().commit_part_transaction("Hide cylinder", [](PartDocument& part) {
    return part.set_body_visibility(BodyId("b"), BodyVisibility::Hidden);
  }));
  window.refresh();
  CHECK_FALSE(window.feature_tools().pattern_context_available(source));
  CHECK_FALSE(action(window, "linear_pattern").isVisible());
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK_FALSE(window.feature_tools().pattern_context_available(source));
  CHECK_FALSE(window.feature_tools().select_pattern_context(source));
}

TEST_CASE("Pattern context Save and replacement keep disposable drafts outside persisted intent",
          "[gui][block136-pattern-context]") {
  const auto command =
      GENERATE(Command::LinearPattern, Command::CircularPattern, Command::BodyMirror);
  CAPTURE(name(command));
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("pattern-context.blcad");
  REQUIRE(window.save_to(path));
  select(window);
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
    select(window);
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
    context(window, "cancel");
    CHECK(snapshot(window) == dirty);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
