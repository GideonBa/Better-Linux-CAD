#include "../support/repair_open_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>

#include <array>

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
  return command == Command::BodyBoolean     ? "body_boolean"
         : command == Command::BodyTranslate ? "body_translate"
         : command == Command::BodyRotate    ? "body_rotate"
                                             : "body_scale";
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
  REQUIRE(window.new_part("Body context"));
  REQUIRE(window.session().commit_part_transaction(
      "Overlapping solids and surface", [](PartDocument& part) {
        REQUIRE(part.add_parameter(reference_checked(Parameter::create_length(
            ParameterId("size"), "Size", reference_checked(Quantity::length_mm(10))))));
        for (const char* id : {"a", "b"}) {
          const auto sketch_id = SketchId(std::string("sketch.") + id);
          auto sketch = reference_checked(Sketch::create(sketch_id, id, DatumPlaneId("datum.xy")));
          REQUIRE(sketch.add_profile(reference_checked(
              RectangleProfile::create(ProfileId(id), ParameterId("size"), ParameterId("size"),
                                       id[0] == 'a' ? Point2{} : Point2{5, 0}))));
          REQUIRE(part.add_sketch(std::move(sketch)));
          REQUIRE(part.add_body(reference_checked(Body::create(BodyId(id), id, BodyKind::Solid))));
          auto feature = reference_checked(Feature::create_additive_extrude(
              FeatureId(std::string("feature.") + id), id, sketch_id, ParameterId("size")));
          REQUIRE(part.add_feature(reference_checked(
              feature.with_body_result_context(reference_checked(FeatureBodyResultContext::create(
                  FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id)))))));
        }
        auto sketch =
            reference_checked(Sketch3D::create(Sketch3DId("surface"), "Surface boundary"));
        const std::array<Point3, 4> points{{{30, 0, 0}, {40, 0, 0}, {40, 10, 0}, {30, 10, 0}}};
        const auto coordinate = [](double value) {
          return reference_checked(SketchCoordinate3D::create_explicit(
              reference_checked(Quantity::linear_displacement_mm(value))));
        };
        std::vector<PathSegmentReference> segments;
        for (int i = 0; i < 4; ++i) {
          const auto p = points[i];
          REQUIRE(sketch.add_point(reference_checked(
              SketchPoint3D::create(SketchEntityId("p" + std::to_string(i)), coordinate(p.x),
                                    coordinate(p.y), coordinate(p.z)))));
        }
        for (int i = 0; i < 4; ++i) {
          const SketchEntityId line("line" + std::to_string(i));
          REQUIRE(sketch.add_line(reference_checked(
              SketchLine3D::create(line, SketchEntityId("p" + std::to_string(i)),
                                   SketchEntityId("p" + std::to_string((i + 1) % 4))))));
          segments.push_back(reference_checked(PathSegmentReference::create_sketch_3d(
              sketch.id(), line, Sketch3DPathCurveKind::Line)));
        }
        REQUIRE(part.add_sketch_3d(std::move(sketch)));
        REQUIRE(part.add_path_curve(reference_checked(
            PathCurve::create(PathCurveId("boundary"), "Boundary", segments, PathClosure::Closed,
                              PathOrientationRule::MinimumTwist))));
        REQUIRE(part.add_body(
            reference_checked(Body::create(BodyId("surface"), "Surface", BodyKind::Surface))));
        REQUIRE(part.add_surface_feature(reference_checked(FillSurfaceFeature::create(
            FeatureId("fill"), "Fill",
            {reference_checked(BoundaryCurveReference::create_path_curve(PathCurveId("boundary")))},
            BodyId("surface")))));
        return Result<std::size_t>::success(1);
      }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
ShellFeatureTools::BodyContext select(ShellWindow& window, Command command, const char* id = "b") {
  auto& filter = control<QComboBox>(window, "blcad.selection.filter");
  filter.setCurrentIndex(filter.findData(selection_kind_bit(GuiSelectionKind::Body)));
  REQUIRE(window.viewport().select_semantic(std::string("body/") + id));
  const auto source = window.feature_tools().body_context(command);
  REQUIRE(source);
  CHECK(source->body == BodyId(id));
  return *source;
}
} // namespace

TEST_CASE("Body contexts distinguish solid Boolean targets from solid and surface transforms",
          "[gui][block136-body-context]") {
  ShellWindow window;
  fixture(window);
  select(window, Command::BodyTranslate, "surface");
  CHECK_FALSE(action(window, "body_boolean").isVisible());
  for (const auto id : {"body_translate", "body_rotate", "body_scale"})
    CHECK(action(window, id).isEnabled());
  CHECK_FALSE(window.feature_tools().body_context(Command::Extrude));
  select(window, Command::BodyBoolean);
  CHECK(action(window, "body_boolean").isEnabled());
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/a"}));
  window.refresh();
  CHECK_FALSE(action(window, "body_translate").isVisible());
  control<QComboBox>(window, "blcad.selection.filter").setCurrentIndex(0);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::Face, "body/b"}));
  window.refresh();
  CHECK_FALSE(window.feature_tools().body_context(Command::BodyBoolean));
  select(window, Command::BodyTranslate);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/a"});
  REQUIRE(window.session().selection().add({GuiSelectionKind::Body, "body/b"}));
  CHECK_FALSE(window.feature_tools().body_context(Command::BodyTranslate));
}

TEST_CASE("Body context cancellation restores selection filter and scope after role collection",
          "[gui][block136-body-context]") {
  const auto command = GENERATE(Command::BodyBoolean, Command::BodyTranslate, Command::BodyRotate,
                                Command::BodyScale);
  const auto route = GENERATE(0, 1, 2);
  CAPTURE(name(command), route);
  ShellWindow window;
  fixture(window);
  const auto source = select(window, command);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"body/b"});
  const auto scope = window.viewport().selection_semantic_scope();
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  REQUIRE(tools.select_body_context(source));
  REQUIRE(tools.select_body_context(source));
  CHECK(window.session().selection().empty());
  if (command == Command::BodyBoolean) {
    CHECK(tools.boolean_target_editor().currentData().toString() == "b");
    CHECK(tools.body_source_editor().selectedItems().empty());
    CHECK_FALSE(tools.apply());
    CHECK_FALSE(window.viewport().select_semantic("body/b"));
    CHECK_FALSE(window.viewport().select_semantic("body/surface"));
    tools.body_reference_button().click();
    REQUIRE(window.viewport().select_semantic("body/a"));
    CHECK(tools.boolean_target_editor().currentData().toString() == "a");
    REQUIRE(window.viewport().select_semantic("body/b"));
  } else {
    REQUIRE(tools.body_source_editor().selectedItems().size() == 1);
    CHECK(tools.body_source_editor().selectedItems().front()->data(Qt::UserRole).toString() == "b");
    REQUIRE(window.viewport().select_semantic("body/a"));
    if (command == Command::BodyRotate) {
      tools.body_reference_button().click();
      CHECK_FALSE(window.viewport().select_semantic("body/surface"));
      CHECK_FALSE(window.viewport().select_semantic("datum-plane/datum.xy"));
      REQUIRE(window.viewport().select_semantic("datum-axis/datum.axis.x"));
      CHECK(tools.body_axis_editor().currentText().contains("X"));
    }
    REQUIRE(tools.set_numeric_value(command == Command::BodyScale ? 2 : 15));
  }
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
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));
  CHECK(window.viewport().selection_semantic_scope() == scope);
  REQUIRE(window.viewport().selected_semantic());
  CHECK(window.viewport().selected_semantic()->semantic_id == "body/b");
  REQUIRE(window.session().selection().size() == 1);
  CHECK(window.session().selection().entries().front().semantic_id == "body/b");
  CHECK(action(window, name(command)).isEnabled());
}

TEST_CASE("Body context Apply keeps exact target or surface source and one atomic history entry",
          "[gui][block136-body-context]") {
  const auto command = GENERATE(Command::BodyBoolean, Command::BodyTranslate, Command::BodyRotate,
                                Command::BodyScale);
  ShellWindow window;
  fixture(window);
  const bool boolean = command == Command::BodyBoolean;
  const auto source = select(window, command, boolean ? "b" : "surface");
  const auto before = snapshot(window);
  context(window, name(command));
  auto& tools = window.feature_tools();
  if (boolean) {
    tools.boolean_operation_editor().setCurrentIndex(1);
    tools.boolean_keep_tools_editor().setChecked(true);
    REQUIRE(window.viewport().select_semantic("body/a"));
  } else {
    REQUIRE(tools.set_numeric_value(command == Command::BodyScale ? 2 : 30));
  }
  REQUIRE(tools.apply());
  const auto* part = window.session().part_document();
  if (boolean) {
    REQUIRE(part->body_boolean_features().size() == 1);
    const auto& feature = part->body_boolean_features().front();
    CHECK(feature.target_body() == source.body);
    REQUIRE(feature.tool_bodies().size() == 1);
    CHECK(feature.tool_bodies().front() == BodyId("a"));
    const auto* shape = window.session().part_shape_cache()->find_body_shape(source.body);
    REQUIRE(shape);
    CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 == Approx(500));
  } else {
    REQUIRE(part->body_transforms().size() == 1);
    CHECK(part->body_transforms().front().body() == source.body);
    CHECK(part->find_body(source.body)->kind() == BodyKind::Surface);
  }
  CHECK_FALSE(window.viewport().selection_semantic_scope());
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
}

TEST_CASE("Body contexts reject captured foreign hidden and removed sources",
          "[gui][block136-body-context]") {
  ShellWindow window;
  fixture(window);
  const auto source = select(window, Command::BodyTranslate);
  window.viewport().grab_interaction_mouse();
  CHECK_FALSE(action(window, "body_translate").isVisible());
  action(window, "body_translate").trigger();
  CHECK_FALSE(window.session().task().active());
  window.viewport().release_interaction_mouse();
  REQUIRE(window.session().task().begin("foreign"));
  window.refresh();
  action(window, "cancel").trigger();
  CHECK(window.session().task().command_id() == "foreign");
  REQUIRE(window.session().task().cancel());
  REQUIRE(window.session().commit_part_transaction("Hide source", [](PartDocument& part) {
    return part.set_body_visibility(BodyId("b"), BodyVisibility::Hidden);
  }));
  window.refresh();
  CHECK_FALSE(window.feature_tools().body_context_available(source));
  CHECK_FALSE(action(window, "body_translate").isVisible());
  REQUIRE(window.undo());
  REQUIRE(window.undo());
  CHECK_FALSE(window.feature_tools().body_context_available(source));
  CHECK_FALSE(window.feature_tools().select_body_context(source));
}

TEST_CASE("Body context Save and replacement keep disposable drafts out of persisted intent",
          "[gui][block136-body-context]") {
  const auto command = GENERATE(Command::BodyBoolean, Command::BodyTranslate, Command::BodyRotate,
                                Command::BodyScale);
  ShellWindow window;
  fixture(window);
  QTemporaryDir directory;
  const auto path = directory.filePath("body-context.blcad");
  REQUIRE(directory.isValid());
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
  SECTION("New") {
    REQUIRE(window.new_part("Next"));
  }
  SECTION("Failed Open") {
    CHECK_FALSE(window.open_path(directory.filePath("missing.blcad")));
    CHECK(snapshot(window) == before);
  }
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
