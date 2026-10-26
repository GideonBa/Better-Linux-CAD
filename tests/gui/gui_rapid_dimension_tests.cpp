#include "blcad/gui/gui_rapid_dimension.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QToolButton>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace blcad;
using namespace blcad::gui;

namespace {
void events() {
  QApplication::processEvents();
}
void mouse(QWidget& widget, QEvent::Type type, QPointF local,
           Qt::MouseButton button = Qt::LeftButton) {
  QMouseEvent event(type, local, widget.mapToGlobal(local.toPoint()),
                    type == QEvent::MouseMove ? Qt::NoButton : button,
                    type == QEvent::MouseButtonPress ? button : Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&widget, &event);
  events();
}
void pointer(ShellWindow& window, Point2 point, bool click = false) {
  auto screen = window.viewport().sketch_plane_to_screen(point);
  REQUIRE(screen);
  QPointF local(screen.value().x, screen.value().y);
  mouse(window.viewport(), QEvent::MouseMove, local);
  if (click) {
    mouse(window.viewport(), QEvent::MouseButtonPress, local);
    mouse(window.viewport(), QEvent::MouseButtonRelease, local);
  }
}
void key(QWidget& widget, int code) {
  QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
  QApplication::sendEvent(&widget, &event);
  events();
}
void rapid_button(ShellWindow& window) {
  for (auto* button : window.findChildren<QToolButton*>()) {
    if (!button->defaultAction() ||
        button->defaultAction()->objectName() != "sketch.dimension.rapid")
      continue;
    REQUIRE(button->isEnabled());
    mouse(*button, QEvent::MouseButtonPress, button->rect().center());
    mouse(*button, QEvent::MouseButtonRelease, button->rect().center());
    REQUIRE(window.sketch_tools().rapid_dimension_active());
    return;
  }
  FAIL("Rapid Dimension ribbon button missing");
}
void start(ShellWindow& window) {
  window.resize(1280, 800);
  window.show();
  events();
  REQUIRE(window.new_part(QStringLiteral("Rapid dimension machining plate")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  events();
}
void create(ShellWindow& window, GuiSketchCreateTool tool, std::initializer_list<Point2> points) {
  REQUIRE(window.sketch_tools().begin_tool(tool));
  for (auto point : points)
    REQUIRE(window.sketch_tools().add_pick(point));
  window.sketch_tools().cancel_tool();
  events();
}
SketchDimensionCatalog catalog(ShellWindow& window) {
  auto value = window.session().sketch_dimension_catalog(*window.active_sketch());
  REQUIRE(value);
  return value.value();
}
SketchDimensionIntent find_kind(ShellWindow& window, SketchDimensionKind kind) {
  const auto current = catalog(window);
  for (const auto& dimension : current.dimensions())
    if (dimension.kind() == kind)
      return dimension;
  FAIL("Expected dimension family was not created");
  throw std::runtime_error("missing dimension");
}
std::string annotation_label(ShellWindow& window, SketchDimensionId id) {
  auto annotations =
      GuiSketchDimensionController::accepted_annotations(window.session(), *window.active_sketch());
  REQUIRE(annotations);
  for (const auto& annotation : annotations.value())
    if (annotation.candidate_id == id.value())
      return annotation.label;
  FAIL("Expected dimension annotation was not published");
  throw std::runtime_error("missing annotation");
}
void capture(ShellWindow& window, const char* name) {
  const QString directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(window.grab().save(directory + "/" + name + ".png"));
}
} // namespace

TEST_CASE("Rapid Dimension infers projected and aligned distances from placement",
          "[gui][rapid-dimension]") {
  ShellWindow window;
  start(window);
  create(window, GuiSketchCreateTool::Line, {{10, 10}, {40, 30}});
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  const auto& line = sketch->line_segments().front();
  auto target = SketchConstraintIntentTarget::entity("entity/" + line.id().value()).value();
  for (auto [position, expected] : std::vector<std::pair<Point2, SketchDimensionKind>>{
           {{25, 45}, SketchDimensionKind::HorizontalDistance},
           {{55, 20}, SketchDimensionKind::VerticalDistance},
           {{55, 45}, SketchDimensionKind::Length}}) {
    auto inferred = GuiRapidDimensionInference::infer(*window.session().part_document(),
                                                      topology.value(), {target}, position);
    REQUIRE(inferred);
    REQUIRE(inferred.value());
    CHECK(inferred.value()->intent.kind() == expected);
    CHECK(inferred.value()->glyph.anchor == position);
  }
  auto origin = SketchConstraintIntentTarget::point(SketchPointId("reference/origin")).value();
  const auto* entity = topology.value().find_entity(target.id());
  REQUIRE(entity);
  auto endpoint = SketchConstraintIntentTarget::point(entity->points().back()).value();
  auto between = GuiRapidDimensionInference::infer(*window.session().part_document(),
                                                   topology.value(), {origin, endpoint}, {20, -10});
  REQUIRE(between);
  REQUIRE(between.value());
  CHECK(between.value()->intent.kind() == SketchDimensionKind::HorizontalDistance);
  CHECK(between.value()->glyph.value_text.find("40.000") != std::string::npos);
  create(window, GuiSketchCreateTool::Line, {{10, 80}, {40, 80}});
  create(window, GuiSketchCreateTool::Line, {{15, 92}, {35, 92}});
  const auto* updated = window.session().part_document()->find_sketch(*window.active_sketch());
  topology = SketchTopology::migrate_legacy(*updated);
  REQUIRE(topology);
  std::vector<SketchConstraintIntentTarget> parallel;
  for (std::size_t i = 1; i < 3; ++i)
    parallel.push_back(
        SketchConstraintIntentTarget::entity("entity/" + updated->line_segments()[i].id().value())
            .value());
  auto distance = GuiRapidDimensionInference::infer(*window.session().part_document(),
                                                    topology.value(), parallel, {50, 85});
  REQUIRE(distance);
  REQUIRE(distance.value());
  CHECK(distance.value()->intent.kind() == SketchDimensionKind::VerticalDistance);
  CHECK(distance.value()->glyph.value_text.find("12.000") != std::string::npos);
  auto angle = GuiRapidDimensionInference::infer(*window.session().part_document(),
                                                 topology.value(), {target, parallel[0]}, {40, 45});
  REQUIRE(angle);
  REQUIRE(angle.value());
  CHECK(angle.value()->intent.kind() == SketchDimensionKind::Angle);
}

TEST_CASE("Rapid Dimension combines hover inference circle edits placement history and reopen",
          "[gui][shell][rapid-dimension][shell-combined-rapid-dimension]") {
  ShellWindow window;
  start(window);
  create(window, GuiSketchCreateTool::Line, {{10, 10}, {40, 30}});
  create(window, GuiSketchCreateTool::CenterRadiusCircle, {{65, 25}, {73, 25}});
  create(window, GuiSketchCreateTool::ThreePointArc, {{-30, 15}, {-20, 25}, {-10, 15}});
  const auto sketch = *window.active_sketch();
  const auto circle_id =
      window.session().part_document()->find_sketch(sketch)->circle_profiles().front().id();
  rapid_button(window);
  pointer(window, {25, 20});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::Length);
  pointer(window, {25, 20}, true);
  pointer(window, {25, 45});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::HorizontalDistance);
  CHECK(catalog(window).dimensions().empty());
  const Point2 placement = window.sketch_tools().rapid_dimension_candidate()->glyph.anchor;
  const auto placement_screen = window.viewport().sketch_plane_to_screen(placement);
  const auto requested_screen = window.viewport().sketch_plane_to_screen({25, 45});
  REQUIRE(placement_screen);
  REQUIRE(requested_screen);
  // Native picking rounds the screen ray to pixels; commit must retain the preview anchor.
  CHECK(std::hypot(placement_screen.value().x - requested_screen.value().x,
                   placement_screen.value().y - requested_screen.value().y) <= 1.5);
  capture(window, "rapid-distance-preview");
  pointer(window, {25, 45}, true);
  REQUIRE(catalog(window).dimensions().size() == 1U);
  window.sketch_tools().cancel_tool();
  CHECK(window.session().undo_label() == "Add sketch dimension");
  rapid_button(window);
  auto horizontal = find_kind(window, SketchDimensionKind::HorizontalDistance);
  auto annotations = GuiSketchDimensionController::accepted_annotations(window.session(), sketch);
  REQUIRE(annotations);
  CHECK(annotations.value().front().point.x == Catch::Approx(placement.x));
  CHECK(annotations.value().front().point.y == Catch::Approx(placement.y));
  pointer(window, {73, 25});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::Diameter);
  CHECK(GuiSketchDimensionController::annotation_label(
            window.sketch_tools().rapid_dimension_candidate()->glyph) == "Ø (16.000 mm)");
  pointer(window, {73, 25}, true);
  pointer(window, {84, 42}, true);
  REQUIRE(catalog(window).dimensions().size() == 2U);
  auto diameter = find_kind(window, SketchDimensionKind::Diameter);
  CHECK(annotation_label(window, diameter.id()) == "Ø 16.000 mm");
  const auto* circle =
      window.session().part_document()->find_sketch(sketch)->find_circle_profile(circle_id);
  REQUIRE(circle);
  CHECK(diameter.parameter() == circle->diameter_parameter());
  CHECK(window.session()
            .part_document()
            ->find_parameter(*diameter.parameter())
            ->value()
            .millimeters() == Catch::Approx(16));
  // Switch tool, edit the actual placed badge, reject bad input, then recover.
  window.sketch_tools().cancel_tool();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity,
       "sketch/" + sketch.value() + "/dimension/" + diameter.id().value()}));
  REQUIRE(window.sketch_tools().edit_selected_dimension_interactive());
  auto* editor = window.viewport().findChild<QLineEdit*>("sketch.dimension.editor");
  REQUIRE(editor);
  editor->setText("invalid");
  key(*editor, Qt::Key_Return);
  CHECK_FALSE(editor->isHidden());
  editor->setText("20 mm");
  key(*editor, Qt::Key_Return);
  CHECK(editor->isHidden());
  CHECK(window.session()
            .part_document()
            ->find_parameter(*diameter.parameter())
            ->value()
            .millimeters() == Catch::Approx(20));
  // An arc defaults to radius. Its hovered curve is distinct from the arc's fit points.
  rapid_button(window);
  pointer(window, {-27.071067812, 22.071067812});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::Radius);
  CHECK(GuiSketchDimensionController::annotation_label(
            window.sketch_tools().rapid_dimension_candidate()->glyph) == "R (10.000 mm)");
  pointer(window, {-27.071067812, 22.071067812}, true);
  pointer(window, {-45, 45}, true);
  REQUIRE(catalog(window).dimensions().size() == 3U);
  CHECK(annotation_label(window, find_kind(window, SketchDimensionKind::Radius).id()) ==
        "R 10.000 mm");
  capture(window, "rapid-mixed-dimensions-result");
  // A new draft is disposable; cancellation preserves all three dimensions.
  pointer(window, {75, 25}, true);
  key(window, Qt::Key_Escape);
  key(window, Qt::Key_Escape);
  CHECK_FALSE(window.sketch_tools().rapid_dimension_active());
  CHECK(catalog(window).dimensions().size() == 3U);
  REQUIRE(window.session().undo());
  CHECK(catalog(window).dimensions().size() == 2U);
  REQUIRE(window.session().undo());
  CHECK(window.session()
            .part_document()
            ->find_parameter(*diameter.parameter())
            ->value()
            .millimeters() == Catch::Approx(16));
  REQUIRE(window.session().redo());
  REQUIRE(window.session().redo());
  const auto accepted = catalog(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("rapid.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  auto restored = reopened.sketch_dimension_catalog(sketch);
  REQUIRE(restored);
  CHECK(restored.value() == accepted);
  CHECK(reopened.part_document()->find_parameter(*diameter.parameter())->value().millimeters() ==
        Catch::Approx(20));
}

TEST_CASE("Full circle radius and diameter dimensions share one authoritative size",
          "[gui][rapid-dimension][circle-dimension-workflow]") {
  ShellWindow window;
  start(window);
  create(window, GuiSketchCreateTool::CenterRadiusCircle, {{30, 30}, {40, 30}});
  const auto sketch = *window.active_sketch();
  const auto circle =
      window.session().part_document()->find_sketch(sketch)->circle_profiles().front();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + circle.id().value()}));
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Radius, false));
  auto radius = find_kind(window, SketchDimensionKind::Radius);
  REQUIRE(GuiSketchDimensionController::edit_literal(window.session(), sketch, radius.id(),
                                                     Quantity::length_mm(12, "test").value()));
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(24));
  auto topology =
      SketchTopology::migrate_legacy(*window.session().part_document()->find_sketch(sketch));
  REQUIRE(topology);
  auto measured = SketchDimensionMeasurementEvaluator::measure(topology.value(), radius,
                                                               window.session().part_document());
  REQUIRE(measured);
  CHECK(measured.value().value.millimeters() == Catch::Approx(12));
  REQUIRE(window.session().undo());
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(20));
  REQUIRE(window.session().redo());
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(24));
}

TEST_CASE("Rapid circle radius toggle places and edits a single reversible size",
          "[gui][rapid-dimension][shell-combined-rapid-dimension]") {
  ShellWindow window;
  start(window);
  create(window, GuiSketchCreateTool::CenterRadiusCircle, {{30, 30}, {40, 30}});
  const auto sketch = *window.active_sketch();
  const auto circle =
      window.session().part_document()->find_sketch(sketch)->circle_profiles().front();
  rapid_button(window);
  pointer(window, {40, 30});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::Diameter);
  pointer(window, {40, 30}, true);
  rapid_button(window);
  pointer(window, {50, 50});
  REQUIRE(window.sketch_tools().rapid_dimension_candidate());
  CHECK(window.sketch_tools().rapid_dimension_candidate()->intent.kind() ==
        SketchDimensionKind::Radius);
  pointer(window, {50, 50}, true);
  REQUIRE(catalog(window).dimensions().size() == 1U);
  const auto radius = find_kind(window, SketchDimensionKind::Radius);
  CHECK(annotation_label(window, radius.id()) == "R 10.000 mm");
  window.sketch_tools().cancel_tool();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity,
       "sketch/" + sketch.value() + "/dimension/" + radius.id().value()}));
  REQUIRE(window.sketch_tools().edit_selected_dimension_interactive());
  auto* editor = window.viewport().findChild<QLineEdit*>("sketch.dimension.editor");
  REQUIRE(editor);
  CHECK(editor->text().toDouble() == Catch::Approx(10));
  editor->setText("12 mm");
  key(*editor, Qt::Key_Return);
  CHECK(editor->isHidden());
  CHECK(annotation_label(window, radius.id()) == "R 12.000 mm");
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(24));
  REQUIRE(window.undo());
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(20));
  REQUIRE(window.redo());
  CHECK(window.session()
            .part_document()
            ->find_parameter(circle.diameter_parameter())
            ->value()
            .millimeters() == Catch::Approx(24));
}
