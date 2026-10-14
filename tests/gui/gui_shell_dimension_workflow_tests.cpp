#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTemporaryDir>

using namespace blcad;
using namespace blcad::gui;

namespace {
void make_line(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Dimensions")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({0.0, 0.0}));
  REQUIRE(tools.add_pick({30.0, 0.0}));
  tools.cancel_tool();
}
void select_line(ShellWindow& window) {
  auto topology = SketchTopology::migrate_legacy(
      *window.session().part_document()->find_sketch(*window.active_sketch()));
  REQUIRE(topology);
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, topology.value().entities().front().id()}));
}
SketchDimensionIntent dimension_of(ShellWindow& window) {
  auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch());
  REQUIRE(catalog);
  REQUIRE(catalog.value().dimensions().size() == 1U);
  return catalog.value().dimensions().front();
}
void select_dimension(ShellWindow& window, SketchDimensionId id) {
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity,
      "sketch/" + window.active_sketch()->value() + "/dimension/" + id.value()}));
}
void key(QWidget& widget, int code) {
  QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
  QApplication::sendEvent(&widget, &event);
  qApp->processEvents();
}
void mouse(OcctViewport& viewport, QEvent::Type type, Point2 plane) {
  auto screen = viewport.sketch_plane_to_screen(plane);
  REQUIRE(screen);
  const QPointF local(screen.value().x, screen.value().y);
  QMouseEvent event(type, local, viewport.mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                    type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&viewport, &event);
  qApp->processEvents();
}
}

TEST_CASE("Shell dimension creation owns its parameter and rejects duplicates atomically",
          "[gui][shell-sketch][sketch-dimension-workflow]") {
  ShellWindow window;
  make_line(window);
  const SketchId sketch = *window.active_sketch();
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  REQUIRE(window.session().part_document()->find_parameter(ParameterId("d1")));
  const auto dimension = dimension_of(window);
  const auto history = window.session().undo_label();
  select_line(window);
  REQUIRE_FALSE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  CHECK(window.session().part_document()->find_parameter(ParameterId("d2")) == nullptr);
  CHECK(window.session().undo_label() == history);
  REQUIRE(window.session().undo());
  CHECK(window.session().part_document()->find_parameter(ParameterId("d1")) == nullptr);
  CHECK(window.session().sketch_dimension_catalog(sketch).value().dimensions().empty());
  CHECK(window.session().part_document()->find_sketch(sketch)->line_segments().size() == 1U);
  REQUIRE(window.session().redo());
  CHECK(dimension_of(window) == dimension);
}

TEST_CASE("Dimension placement roundtrips with history without changing geometry",
          "[gui][shell-sketch][sketch-dimension-workflow]") {
  ShellWindow window;
  make_line(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto dimension = dimension_of(window);
  const SketchId sketch = *window.active_sketch();
  auto annotations = GuiSketchDimensionController::accepted_annotations(window.session(), sketch);
  REQUIRE(annotations);
  const Point2 original = annotations.value().front().point;
  const auto source = *window.session().part_document()->find_sketch(sketch);
  REQUIRE(GuiSketchDimensionController::set_label_offset(window.session(), sketch,
                                                         dimension.id(), {12.5, -9.0}));
  annotations = GuiSketchDimensionController::accepted_annotations(window.session(), sketch);
  REQUIRE(annotations);
  CHECK(annotations.value().front().point.x == Catch::Approx(original.x + 12.5));
  CHECK(annotations.value().front().point.y == Catch::Approx(original.y - 9.0));
  const auto& unchanged = window.session().part_document()->find_sketch(sketch)->line_segments();
  REQUIRE(unchanged.size() == source.line_segments().size());
  CHECK(unchanged.front().start() == source.line_segments().front().start());
  CHECK(unchanged.front().end() == source.line_segments().front().end());
  REQUIRE(window.session().undo());
  CHECK(dimension_of(window).label_offset() == Point2{});
  REQUIRE(window.session().redo());
  CHECK(dimension_of(window).label_offset() == Point2{12.5, -9.0});
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("part.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  auto restored = reopened.sketch_dimension_catalog(sketch);
  REQUIRE(restored);
  CHECK(restored.value().find(dimension.id())->label_offset() == Point2{12.5, -9.0});
}

TEST_CASE("Dimension editor commits on Enter and cancels on Escape",
          "[gui][shell-sketch][sketch-dimension-workflow]") {
  ShellWindow window;
  make_line(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto dimension = dimension_of(window);
  select_dimension(window, dimension.id());
  REQUIRE(window.sketch_tools().edit_selected_dimension_interactive());
  auto* editor = window.viewport().findChild<QLineEdit*>("sketch.dimension.editor");
  REQUIRE(editor);
  REQUIRE_FALSE(editor->isHidden());
  editor->setText("45 mm");
  key(*editor, Qt::Key_Return);
  CHECK(editor->isHidden());
  CHECK(window.session().part_document()->find_parameter(*dimension.parameter())->value().millimeters()
        == Catch::Approx(45.0));
  select_dimension(window, dimension.id());
  REQUIRE(window.sketch_tools().edit_selected_dimension_interactive());
  editor->setText("invalid");
  key(*editor, Qt::Key_Return);
  CHECK_FALSE(editor->isHidden());
  key(*editor, Qt::Key_Escape);
  CHECK(editor->isHidden());
  CHECK(window.session().part_document()->find_parameter(*dimension.parameter())->value().millimeters()
        == Catch::Approx(45.0));
}

TEST_CASE("Dimension drag uses the release sample and Escape restores placement",
          "[gui][shell-sketch][sketch-dimension-workflow]") {
  ShellWindow window;
  make_line(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto dimension = dimension_of(window);
  const auto sketch = *window.active_sketch();
  REQUIRE(GuiSketchDimensionController::set_label_offset(window.session(), sketch,
                                                         dimension.id(), {0.0, 20.0}));
  window.sketch_tools().publish_scene();
  auto annotation = GuiSketchDimensionController::accepted_annotations(window.session(), sketch);
  REQUIRE(annotation);
  const auto start = annotation.value().front().point;
  mouse(window.viewport(), QEvent::MouseButtonPress, start);
  mouse(window.viewport(), QEvent::MouseMove, {start.x + 3, start.y + 4});
  CHECK(dimension_of(window).label_offset() == Point2{0, 20});
  mouse(window.viewport(), QEvent::MouseButtonRelease, {start.x + 6, start.y + 8});
  CHECK(dimension_of(window).label_offset().x == Catch::Approx(6));
  CHECK(dimension_of(window).label_offset().y == Catch::Approx(28));
  REQUIRE(window.session().undo());
  window.sketch_tools().publish_scene();
  mouse(window.viewport(), QEvent::MouseButtonPress, start);
  mouse(window.viewport(), QEvent::MouseMove, {start.x + 3, start.y + 4});
  key(window, Qt::Key_Escape);
  mouse(window.viewport(), QEvent::MouseButtonRelease, {start.x + 6, start.y + 8});
  CHECK(dimension_of(window).label_offset() == Point2{0, 20});
}

TEST_CASE("Dimension JSON validates placement and retains historical defaults",
          "[gui][sketch-dimension-workflow]") {
  auto dimension = SketchDimensionIntent::create(SketchDimensionId("dim"),
      SketchDimensionKind::Length, SketchDimensionMode::Reference,
      {SketchConstraintIntentTarget::entity("entity/line").value()});
  REQUIRE(dimension);
  auto value = detail::sketch_dimension_intent_to_json(dimension.value());
  CHECK_FALSE(value.contains("label_offset_mm"));
  CHECK(detail::sketch_dimension_intent_from_json(value).value() == dimension.value());
  for (const auto& invalid : {nlohmann::json(nullptr), nlohmann::json::array({1}),
                              nlohmann::json::array({"x", 1})}) {
    value["label_offset_mm"] = invalid;
    CHECK_FALSE(detail::sketch_dimension_intent_from_json(value));
  }
  auto moved = dimension.value().with_label_offset({2, 3});
  REQUIRE(moved);
  value = detail::sketch_dimension_intent_to_json(moved.value());
  CHECK(detail::sketch_dimension_intent_from_json(value).value() == moved.value());
  CHECK(moved.value().with_parameter(ParameterId("p")).value().label_offset() == Point2{2, 3});
}

TEST_CASE("Dimension expressions convert direct bindings atomically and retain label placement",
          "[gui][shell-sketch][sketch-dimension-workflow]") {
  ShellWindow window;
  make_line(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto dimension = dimension_of(window);
  REQUIRE(GuiSketchDimensionController::set_label_offset(window.session(), *window.active_sketch(),
                                                         dimension.id(), {3, 8}));
  select_dimension(window, dimension.id());
  REQUIRE_FALSE(window.sketch_tools().edit_selected_dimension("=missing * 2"));
  CHECK(window.session().part_document()->find_parameter(ParameterId("d_expr_1")) == nullptr);
  REQUIRE(window.sketch_tools().edit_selected_dimension("=d1 * 2"));
  auto changed = dimension_of(window);
  REQUIRE(changed.parameter() != dimension.parameter());
  CHECK(changed.label_offset() == Point2{3, 8});
  CHECK(window.session().part_document()->find_parameter(*changed.parameter())->value().millimeters()
        == Catch::Approx(60));
  REQUIRE(window.session().undo());
  CHECK(dimension_of(window).parameter() == dimension.parameter());
  CHECK(window.session().part_document()->find_parameter(ParameterId("d_expr_1")) == nullptr);
}
