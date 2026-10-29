#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/gui/gui_sketch_annotation_layout.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QApplication>
#include <QCursor>
#include <QWheelEvent>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStatusBar>
#include <QScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <functional>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}

void events() {
  QApplication::processEvents();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  QApplication::processEvents();
}

bool await_ui(const std::function<bool()>& ready) {
  const bool native = QApplication::platformName() == "xcb";
  int stable_turns = 0;
  if (native || !ready()) {
    QEventLoop loop;
    QTimer poll, deadline;
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
      if (ready()) {
        if (!native || ++stable_turns >= 2)
          loop.quit();
      } else {
        stable_turns = 0;
      }
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(5);
    deadline.start(2000);
    loop.exec();
  }
  return ready() && (!native || stable_turns >= 2);
}

// Only fixture preparation uses authoring APIs. Selection, editing and deletion
// below enter through the actual viewport mouse/key route, without seeded selection.
struct DimensionUi {
  ShellWindow window;
  SketchId sketch_id{"fixture.sketch"};

  explicit DimensionUi(bool oblique = false) {
    window.resize(1280, 800);
    if (QApplication::platformName() == "xcb") {
      auto* screen = QGuiApplication::screenAt(QPoint(0, 0));
      REQUIRE(screen);
      const auto available = screen->availableGeometry().adjusted(12, 12, -12, -12);
      window.move(available.topLeft());
      window.resize(std::min(1280, available.width()), std::min(800, available.height()));
    }
    window.show();
    window.raise();
    window.activateWindow();
    if (QApplication::platformName() == "xcb")
      QGuiApplication::sync();
    events();
    REQUIRE(await_ui([&] { return QApplication::activeWindow() == &window; }));
    if (QApplication::platformName() == "xcb")
      REQUIRE(window.viewport().native_viewer_available());
    REQUIRE(window.new_part("Dimension input"));
    if (oblique) {
      REQUIRE(window.session().commit_part_transaction(
          "Oblique dimension plane", [](PartDocument& part) {
            return part.add_construction_plane(checked(ConstructionPlane::create_explicit(
                ConstructionPlaneId("dimension.plane"), "Dimension plane", {5, 7, 9}, {.8, 0, -.6},
                {0, 1, 0}, {.6, 0, .8})));
          }));
      window.refresh();
    }
    REQUIRE(window.viewport().select_semantic(oblique ? "construction-plane/dimension.plane"
                                                      : "datum-plane/datum.xy"));
    REQUIRE(window.create_sketch_on_selection());
    sketch_id = *window.active_sketch();
  }

  SketchId sketch() {
    return sketch_id;
  }
  SketchTopology topology() {
    return checked(
        SketchTopology::migrate_legacy(*window.session().part_document()->find_sketch(sketch())));
  }
  SketchDimensionCatalog catalog() {
    return checked(window.session().sketch_dimension_catalog(sketch()));
  }
  std::string snapshot() {
    return checked(serialize_part_document_to_json(*window.session().part_document())) +
           checked(serialize_sketch_dimension_catalog_to_json(catalog())) +
           checked(serialize_sketch_constraint_catalog_to_json(
               checked(window.session().sketch_constraint_catalog(sketch()))));
  }
  void curve(GuiSketchCreateTool kind, Point2 a, Point2 b) {
    REQUIRE(window.sketch_tools().begin_tool(kind));
    REQUIRE(window.sketch_tools().add_pick(a));
    REQUIRE(window.sketch_tools().add_pick(b));
    window.sketch_tools().cancel_tool();
  }
  void clear() {
    window.session().selection().clear();
    window.viewport().clear_selection();
    window.sketch_tools().publish_scene();
    pointer(window.viewport().rect().center());
    window.viewport().setFocus();
    events();
    REQUIRE(await_ui(
        [&] { return QApplication::activeWindow() == &window && window.viewport().hasFocus(); }));
  }
  SketchDimensionIntent make(SketchDimensionKind kind, bool reference = false) {
    if (kind == SketchDimensionKind::Radius || kind == SketchDimensionKind::Diameter)
      curve(GuiSketchCreateTool::CenterRadiusCircle, {0, 0}, {12, 0});
    else {
      curve(GuiSketchCreateTool::Line, {0, 0}, {30, 0});
      if (kind == SketchDimensionKind::Angle)
        curve(GuiSketchCreateTool::Line, {0, 0}, {20, 20});
    }
    window.session().selection().clear();
    const auto source = topology();
    for (const auto& entity : source.entities()) {
      if (!entity.reference())
        REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, entity.id()}));
    }
    checked(window.sketch_tools().apply_dimension(kind, reference));
    auto dimension = catalog().dimensions().back();
    REQUIRE(GuiSketchDimensionController::set_label_offset(window.session(), sketch(),
                                                           dimension.id(), {0, 35}));
    clear();
    return *catalog().find(dimension.id());
  }
  GuiSketchAnnotationPrimitive annotation(const SketchDimensionId& id) {
    auto annotations =
        checked(GuiSketchDimensionController::accepted_annotations(window.session(), sketch()));
    for (const auto& annotation : annotations)
      if (annotation.candidate_id == id.value())
        return annotation;
    FAIL("dimension annotation missing");
    return {};
  }
  void place_visible_label(const SketchDimensionId& id, Point2 desired) {
    auto plane = checked(GuiSketchWorkbench{}.plane_view(window.session(), sketch()));
    auto mapping = checked(GuiSketchPlaneMapping::create(
        plane, [&](GuiSketchScreenPoint p) { return window.viewport().screen_to_view_ray(p); },
        [&](Point3 p) { return window.viewport().model_to_screen(p); }));
    const auto shown = checked(window.viewport().sketch_dimension_screen_layout(annotation(id)));
    const auto current = checked(mapping.screen_to_plane(shown.label_anchor));
    const auto offset = catalog().find(id)->label_offset();
    REQUIRE(GuiSketchDimensionController::set_label_offset(
        window.session(), sketch(), id,
        {offset.x + desired.x - current.x, offset.y + desired.y - current.y}));
  }
  QPointF label(const SketchDimensionId& id, double horizontal = 0) {
    const auto a = annotation(id);
    const auto center = checked(window.viewport().sketch_dimension_screen_layout(a)).label_anchor;
    const auto layout = sketch_annotation_layout(QString::fromStdString(a.label), true);
    // Stay one DIP inside the painted border, beyond the old 12-DIP pick disk.
    const double dx = horizontal * (layout.bounds.width() * 0.5 - 1.0);
    if (horizontal != 0)
      REQUIRE(std::abs(dx) > 12);
    const QPointF point(center.x + dx, center.y);
    REQUIRE(window.viewport().rect().contains(point.toPoint()));
    return point;
  }
  void mouse(QEvent::Type type, QPointF local, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      modifiers);
    QApplication::sendEvent(&window.viewport(), &event);
  }
  void pointer(QPointF point) {
    if (QApplication::platformName() == "xcb") {
      // Native enter/focus events must refer to the same position as the
      // synthetic button event, and finish before a press can start a drag.
      QCursor::setPos(window.viewport().mapToGlobal(point.toPoint()));
      QGuiApplication::sync();
      events();
    }
  }
  void click(QPointF point, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    pointer(point);
    // Deliver a complete click before flushing native events, as the combined
    // shell workflows do. A queued OS MouseMove at the old physical cursor
    // position must not turn a synthetic stationary click into a label drag.
    mouse(QEvent::MouseButtonPress, point, modifiers);
    mouse(QEvent::MouseButtonRelease, point, modifiers);
    events();
  }
  QLineEdit& double_click(QPointF point, bool preceding_click = true) {
    if (preceding_click)
      click(point);
    else
      pointer(point);
    mouse(QEvent::MouseButtonDblClick, point);
    mouse(QEvent::MouseButtonRelease, point);
    events();
    auto* editor = window.viewport().findChild<QLineEdit*>("sketch.dimension.editor");
    REQUIRE(editor);
    return *editor;
  }
  void key(QWidget& target, int code, const QString& text = {}) {
    REQUIRE(await_ui([&] { return target.hasFocus() && QApplication::activeWindow() == &window; }));
    QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier, text);
    QApplication::sendEvent(&target, &press);
    QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier, text);
    QApplication::sendEvent(&target, &release);
    events();
  }
  void type(QLineEdit& editor, const QString& text) {
    // Drain native activation before selecting text; a delayed FocusIn must
    // not change selection halfway through a synthetic keyboard sequence.
    if (QApplication::platformName() == "xcb") {
      QGuiApplication::sync();
      events();
      REQUIRE(
          await_ui([&] { return editor.hasFocus() && QApplication::activeWindow() == &window; }));
    }
    editor.selectAll();
    for (const auto character : text)
      key(editor, character.unicode(), QString(character));
    REQUIRE(editor.text() == text);
  }
  double value(const SketchDimensionId& id) {
    const auto dimension = *catalog().find(id);
    const double value = checked(SketchDimensionSolverAdapter::parameter_value(
        *window.session().part_document(), dimension));
    const auto measured = checked(SketchDimensionMeasurementEvaluator::measure(
                                      topology(), dimension, window.session().part_document()))
                              .value;
    CHECK((dimension.kind() == SketchDimensionKind::Angle
               ? measured.degrees()
               : measured.millimeters()) == Catch::Approx(value));
    return value;
  }
  void roundtrip() {
    const auto before = snapshot();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = directory.filePath("dimensions.blcad.json").toStdString();
    REQUIRE(window.session().save_as(path));
    GuiDocumentSession reopened;
    REQUIRE(reopened.open_file(path));
    CHECK(checked(serialize_part_document_to_json(*reopened.part_document())) +
              checked(serialize_sketch_dimension_catalog_to_json(
                  checked(reopened.sketch_dimension_catalog(sketch())))) +
              checked(serialize_sketch_constraint_catalog_to_json(
                  checked(reopened.sketch_constraint_catalog(sketch())))) ==
          before);
    for (const auto& body : window.session().part_document()->bodies()) {
      const auto* source = window.session().part_shape_cache()->find_body_shape(body.id());
      if (!source)
        continue;
      const auto* restored = reopened.part_shape_cache()->find_body_shape(body.id());
      REQUIRE(restored);
      CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*restored).volume_mm3 ==
            Catch::Approx(geometry::RectangleExtrusionAdapter{}.summarize(*source).volume_mm3));
    }
  }
  void capture(const QString& name) {
    const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
    if (directory.isEmpty())
      return;
    REQUIRE(QDir().mkpath(directory));
    REQUIRE(window.grab().save(directory + "/" + name + ".png"));
  }
};
} // namespace

TEST_CASE("Visible dimension edges double click edit atomically through mouse and keyboard",
          "[gui][shell][dimension-label-input]") {
  const auto kind = GENERATE(SketchDimensionKind::Length, SketchDimensionKind::Radius,
                             SketchDimensionKind::Diameter, SketchDimensionKind::Angle);
  const auto edge = GENERATE(-1.0, 0.0, 1.0);
  CAPTURE(kind, edge);
  DimensionUi ui;
  const auto dimension = ui.make(kind);
  const auto before = ui.snapshot();
  const double old_value = ui.value(dimension.id());
  auto& editor = ui.double_click(ui.label(dimension.id(), edge));
  REQUIRE(editor.isVisible());
  CHECK(editor.text().toDouble() == Catch::Approx(old_value));
  ui.type(editor, kind == SketchDimensionKind::Angle ? "60 deg" : "42 mm");
  INFO("Typed editor text: " << editor.text().toStdString());
  ui.key(editor, Qt::Key_Return);
  INFO("Editor diagnostic after Enter: " << editor.toolTip().toStdString());
  CHECK(editor.isHidden());
  CHECK(ui.value(dimension.id()) == Catch::Approx(kind == SketchDimensionKind::Angle ? 60 : 42));
  CHECK(ui.catalog().find(dimension.id())->label_offset() == Point2{0, 35});
  const auto after = ui.snapshot();
  CHECK(after != before);
  REQUIRE(ui.window.session().undo());
  CHECK(ui.snapshot() == before);
  REQUIRE(ui.window.session().redo());
  CHECK(ui.snapshot() == after);
  ui.clear();
  auto& reopened = ui.double_click(ui.label(dimension.id(), edge));
  REQUIRE(reopened.isVisible());
  const auto history = std::string(ui.window.session().undo_label());
  ui.type(reopened, "invalid");
  ui.key(reopened, Qt::Key_Return);
  CHECK(reopened.isVisible());
  CHECK_FALSE(reopened.toolTip().isEmpty());
  CHECK(ui.snapshot() == after);
  CHECK(ui.window.session().undo_label() == history);
  ui.type(reopened, "50");
  ui.key(reopened, Qt::Key_Return);
  CHECK(reopened.isHidden());
  CHECK(ui.value(dimension.id()) == Catch::Approx(50));
  REQUIRE(ui.window.session().undo());
  CHECK(ui.snapshot() == after);
  ui.clear();
  auto& cancelled = ui.double_click(ui.label(dimension.id(), edge));
  REQUIRE(cancelled.isVisible());
  ui.type(cancelled, "75");
  ui.key(cancelled, Qt::Key_Escape);
  CHECK(cancelled.isHidden());
  CHECK(ui.snapshot() == after);
  CHECK(ui.window.session().undo_label() == history);
  ui.roundtrip();
}

TEST_CASE(
    "Dimension double click resolves the clicked rectangle over geometry without preselection",
    "[gui][shell][dimension-label-input]") {
  const auto kind = GENERATE(SketchDimensionKind::Length, SketchDimensionKind::Radius,
                             SketchDimensionKind::Diameter, SketchDimensionKind::Angle);
  DimensionUi ui;
  const auto dimension = ui.make(kind);
  REQUIRE(GuiSketchDimensionController::set_label_offset(ui.window.session(), ui.sketch(),
                                                         dimension.id(), {}));
  const auto point = ui.annotation(dimension.id()).point;
  // A real curve and its midpoint lie underneath each family's value badge.
  ui.curve(GuiSketchCreateTool::Line, {point.x - 20, point.y}, {point.x + 20, point.y});
  ui.clear();
  auto& editor = ui.double_click(ui.label(dimension.id()), false);
  REQUIRE(editor.isVisible());
  ui.key(editor, Qt::Key_Escape);
  ui.click(ui.label(dimension.id()));
  const auto selected = ui.window.session().selection().entries();
  REQUIRE(selected.size() == 1U);
  CHECK(selected.front().semantic_id.ends_with("/dimension/" + dimension.id().value()));
  // An unrelated double click must not edit the previously selected dimension.
  auto& hidden = ui.double_click({20, 20}, false);
  CHECK(hidden.isHidden());
}

TEST_CASE("Reference label double click explains why it is read only",
          "[gui][shell][dimension-label-input]") {
  DimensionUi ui;
  const auto dimension = ui.make(SketchDimensionKind::Radius, true);
  const auto before = ui.snapshot();
  auto& editor = ui.double_click(ui.label(dimension.id(), 1));
  CHECK(editor.isHidden());
  CHECK(ui.window.statusBar()->currentMessage().contains("read-only", Qt::CaseInsensitive));
  CHECK(ui.snapshot() == before);
}

TEST_CASE("Label click then actual Delete or Backspace removes only the dimension",
          "[gui][shell][dimension-label-input][dimension-label-delete]") {
  const auto kind = GENERATE(SketchDimensionKind::Length, SketchDimensionKind::Radius,
                             SketchDimensionKind::Diameter, SketchDimensionKind::Angle);
  const bool reference = GENERATE(false, true);
  const int code = GENERATE(int(Qt::Key_Delete), int(Qt::Key_Backspace));
  CAPTURE(kind, reference, code);
  DimensionUi ui;
  const auto dimension = ui.make(kind, reference);
  const auto before = ui.snapshot();
  const auto source = ui.topology();
  const auto constraints = checked(ui.window.session().sketch_constraint_catalog(ui.sketch()));
  ui.click(ui.label(dimension.id(), -1));
  const auto selected = ui.window.session().selection().entries();
  REQUIRE(selected.size() == 1U);
  REQUIRE(selected.front().semantic_id.ends_with("/dimension/" + dimension.id().value()));
  ui.key(ui.window.viewport(), code);
  CHECK(ui.catalog().dimensions().empty());
  CHECK(ui.topology() == source);
  CHECK(checked(ui.window.session().sketch_constraint_catalog(ui.sketch())) == constraints);
  if (dimension.parameter())
    CHECK(ui.window.session().part_document()->find_parameter(*dimension.parameter()));
  const auto after = ui.snapshot();
  CHECK(after != before);
  REQUIRE(ui.window.session().undo());
  CHECK(ui.snapshot() == before);
  REQUIRE(ui.window.session().redo());
  CHECK(ui.snapshot() == after);
  ui.roundtrip();
}

TEST_CASE(
    "Dimension labels highlight and support stacked and multiple selection on a closed profile",
    "[gui][shell][dimension-label-input][dimension-label-delete]") {
  DimensionUi ui;
  ui.curve(GuiSketchCreateTool::CornerRectangle, {-30, -20}, {30, 20});
  std::vector<SketchDimensionId> ids;
  const auto topology = ui.topology();
  for (const auto& entity : topology.entities()) {
    if (entity.reference() || entity.kind() != SketchTopologyEntityKind::Line || ids.size() == 3U)
      continue;
    ui.window.session().selection().clear();
    REQUIRE(ui.window.session().selection().add({GuiSelectionKind::SketchEntity, entity.id()}));
    checked(ui.window.sketch_tools().apply_dimension(SketchDimensionKind::Length, !ids.empty()));
    ids.push_back(ui.catalog().dimensions().back().id());
  }
  REQUIRE(ids.size() == 3U);
  // Intentionally stack two labels exactly. Real clicks cycle them in stable ID order.
  const auto anchor = ui.annotation(ids[0]).point;
  for (std::size_t i = 0; i < ids.size(); ++i)
    ui.place_visible_label(ids[i], {anchor.x + (i == 2 ? 45 : 0), anchor.y + 40});
  const auto driver = *ui.catalog().find(ids[0])->parameter();
  const double dependent_value = ui.value(ids[0]) * 2;
  REQUIRE(ui.window.session().commit_part_transaction("Dependent formula", [&](PartDocument& part) {
    return part.add_parameter(checked(Parameter::create_expression(
        ParameterId("retained.formula"), "Retained formula", ParameterType::Length,
        driver.value() + " * 2", checked(Quantity::length_mm(dependent_value, "fixture")))));
  }));
  ui.clear();
  const auto before = ui.snapshot();
  const auto source = ui.topology();
  const auto constraints = checked(ui.window.session().sketch_constraint_catalog(ui.sketch()));
  const auto point = ui.label(ids[0]);
  ui.click(point);
  REQUIRE(ui.window.session().selection().entries().size() == 1U);
  CHECK(ui.window.session().selection().entries().front().semantic_id.ends_with(ids[0].value()));
  auto* overlay = ui.window.viewport().findChild<QWidget*>("blcad.sketch.overlay");
  REQUIRE(overlay);
  const QImage selected = overlay->grab().toImage();
  const auto fill = overlay->palette().color(QPalette::Highlight);
  const double dpr = selected.devicePixelRatio();
  int highlight_pixels = 0;
  for (int y = int((point.y() - 8) * dpr); y < int((point.y() + 8) * dpr); ++y)
    for (int x = int((point.x() - 20) * dpr); x < int((point.x() + 20) * dpr); ++x)
      if (selected.valid(x, y) && selected.pixelColor(x, y).rgb() == fill.rgb())
        ++highlight_pixels;
  CHECK(highlight_pixels > 30);
  ui.capture("dimension-selected-overlap");
  ui.click(point);
  REQUIRE(ui.window.session().selection().entries().size() == 1U);
  CHECK(ui.window.session().selection().entries().front().semantic_id.ends_with(ids[1].value()));
  ui.click(ui.label(ids[2]), Qt::ControlModifier);
  REQUIRE(ui.window.session().selection().entries().size() == 2U);
  ui.key(ui.window.viewport(), Qt::Key_Delete);
  REQUIRE(ui.catalog().dimensions().size() == 1U);
  CHECK(ui.catalog().find(ids[0]));
  CHECK(ui.topology() == source);
  CHECK(checked(ui.window.session().sketch_constraint_catalog(ui.sketch())) == constraints);
  CHECK(ui.window.session().part_document()->find_parameter(driver));
  CHECK(ui.window.session().part_document()->find_parameter(ParameterId("retained.formula")));
  const auto after = ui.snapshot();
  REQUIRE(ui.window.session().undo());
  CHECK(ui.snapshot() == before);
  REQUIRE(ui.window.session().redo());
  CHECK(ui.snapshot() == after);
  // Delete the driving dimension as well: its parameter remains a formula source.
  ui.clear();
  ui.click(ui.label(ids[0]));
  ui.key(ui.window.viewport(), Qt::Key_Backspace);
  CHECK(ui.catalog().dimensions().empty());
  REQUIRE(ui.window.session().recompute());
  CHECK(ui.window.session().part_document()->find_parameter(driver));
  CHECK(ui.window.session()
            .part_document()
            ->find_parameter(ParameterId("retained.formula"))
            ->value()
            .millimeters() == Catch::Approx(dependent_value));
  CHECK(ui.topology() == source);
  ui.roundtrip();
}

TEST_CASE(
    "Double clicked circle radius recomputes a downstream solid and survives history and reopening",
    "[gui][shell][dimension-label-input]") {
  DimensionUi ui;
  const auto dimension = ui.make(SketchDimensionKind::Radius);
  REQUIRE(ui.window.finish_active_sketch());
  auto& tools = ui.window.feature_tools();
  REQUIRE(tools.begin_extrude());
  const auto body = tools.coordinator().extrude().result_body_id();
  REQUIRE(tools.set_numeric_value(8));
  REQUIRE(tools.apply());
  const auto volume = [&] {
    const auto* shape = ui.window.session().part_shape_cache()->find_body_shape(body);
    REQUIRE(shape);
    return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
  };
  constexpr double pi = 3.14159265358979323846;
  CHECK(volume() == Catch::Approx(pi * 12 * 12 * 8));
  bool entered = false;
  for (QTreeWidgetItemIterator it(&ui.window.model_browser()); *it; ++it) {
    if ((*it)->data(0, Qt::UserRole).toString() == QString::fromStdString(ui.sketch().value())) {
      ui.window.model_browser().itemDoubleClicked(*it, 0);
      entered = true;
      break;
    }
  }
  REQUIRE(entered);
  REQUIRE(ui.window.active_sketch() == ui.sketch());
  ui.clear();
  const auto before = ui.snapshot();
  auto& editor = ui.double_click(ui.label(dimension.id(), 1));
  REQUIRE(editor.isVisible());
  ui.type(editor, "15 mm");
  ui.key(editor, Qt::Key_Return);
  CHECK(volume() == Catch::Approx(pi * 15 * 15 * 8));
  const auto after = ui.snapshot();
  REQUIRE(ui.window.session().undo());
  CHECK(ui.snapshot() == before);
  CHECK(volume() == Catch::Approx(pi * 12 * 12 * 8));
  REQUIRE(ui.window.session().redo());
  CHECK(ui.snapshot() == after);
  CHECK(volume() == Catch::Approx(pi * 15 * 15 * 8));
  ui.roundtrip();
}

TEST_CASE("Measured annotation rectangles take precedence and stay DIP-sized across plane mappings",
          "[gui][sketch-hit-test][dimension-label-input]") {
  const double scale = GENERATE(0.2, 1.0, 8.0);
  const auto layout = sketch_annotation_layout("12345.678 mm", true);
  GuiSketchInteractionScene scene;
  scene.curves.push_back({"line", "line", GuiSketchCurveKind::Line, {{-100, 0}, {100, 0}}});
  scene.points.push_back({"point", "point", {0, 0}});
  GuiSketchAnnotationPrimitive annotation{"dimension/a",  "a", {0, 0}, GuiSketchHitKind::Dimension,
                                          "12345.678 mm", {}};
  annotation.label_bounds_dip = GuiSketchScreenRect{
      {layout.bounds.left(), layout.bounds.top()}, {layout.bounds.right(), layout.bounds.bottom()}};
  scene.annotations.push_back(annotation);
  annotation.semantic_id = "dimension/b";
  annotation.candidate_id = "b";
  scene.annotations.push_back(annotation);
  GuiSketchPlaneView plane{DatumPlaneId("oblique"), {7, 11, 13}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  auto mapping = checked(GuiSketchPlaneMapping::create_orthographic(plane, 500, 400, {}, scale));
  auto controller = checked(GuiSketchInteractionController::create(mapping, scene));
  const auto center = checked(mapping.plane_to_screen({0, 0}));
  for (const double dx : {layout.bounds.left(), 0.0, layout.bounds.right()}) {
    const auto hits = checked(controller.hits_at({center.x + dx, center.y}));
    REQUIRE(hits.size() >= 2U);
    CHECK(hits[0].semantic_id == "dimension/a");
    CHECK(hits[0].label_hit);
    CHECK(hits[1].semantic_id == "dimension/b");
  }
  CHECK(checked(controller.cycle_hit(center))->semantic_id == "dimension/a");
  CHECK(checked(controller.cycle_hit(center))->semantic_id == "dimension/b");
  CHECK(checked(controller.cycle_hit(center))->semantic_id == "point");
  const auto outside = checked(controller.hits_at(
      {center.x + layout.bounds.right() + 2, center.y + layout.bounds.bottom() + 2}));
  CHECK(std::none_of(outside.begin(), outside.end(),
                     [](const auto& hit) { return hit.kind == GuiSketchHitKind::Dimension; }));
}

TEST_CASE("Native neighboring dimension families remain attached through label movement zoom and "
          "oblique planes",
          "[gui][integration][dimension-layout-native]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Qt/OCCT dimension painting and pointer input");
  const bool oblique = GENERATE(false, true);
  CAPTURE(oblique);
  DimensionUi ui(oblique);
  REQUIRE(ui.window.session().commit_part_transaction(
      "Dimension presentation fixture", [&](PartDocument& part) {
        REQUIRE(part.add_parameter(checked(Parameter::create_length(
            ParameterId("hole.diameter"), "Hole diameter", checked(Quantity::length_mm(14))))));
        const auto* original = part.find_sketch(ui.sketch());
        REQUIRE(original);
        auto sketch =
            checked(Sketch::create(original->id(), original->name(), original->workplane()));
        const std::vector<Point2> corners{{-35, -25}, {35, -25}, {35, 25}, {-35, 25}};
        for (std::size_t i = 0; i < corners.size(); ++i)
          REQUIRE(sketch.add_entity(checked(LineSegment::create(
              SketchEntityId("plate." + std::to_string(i)), corners[i], corners[(i + 1) % 4]))));
        for (const auto& line : std::vector<std::pair<const char*, std::pair<Point2, Point2>>>{
                 {"offset", {{-30, -20}, {-10, -8}}},
                 {"angle.base", {{-5, -18}, {15, -18}}},
                 {"angle.sloped", {{-5, -18}, {10, -3}}}})
          REQUIRE(sketch.add_entity(checked(LineSegment::create(
              SketchEntityId(line.first), line.second.first, line.second.second))));
        const double h = 6 / std::sqrt(2.0);
        REQUIRE(sketch.add_entity(checked(ArcSegment::create_three_point(
            SketchEntityId("left.arc"), {-24, 12}, {-18 - h, 12 + h}, {-18, 18}))));
        REQUIRE(sketch.add_profile(checked(
            CircleProfile::create(ProfileId("hole"), ParameterId("hole.diameter"), {22, 12}))));
        return part.update_sketch(std::move(sketch));
      }));
  const auto topology = ui.topology();
  const auto point_targets = [&](const char* name) {
    const auto* line = topology.find_entity(name);
    REQUIRE(line);
    return std::vector{checked(SketchConstraintIntentTarget::point(line->points()[0])),
                       checked(SketchConstraintIntentTarget::point(line->points()[1]))};
  };
  const auto entity = [](const char* name) {
    return checked(SketchConstraintIntentTarget::entity(name));
  };
  const auto circle =
      std::find_if(topology.entities().begin(), topology.entities().end(), [](const auto& e) {
        return e.kind() == SketchTopologyEntityKind::CircleProfile;
      });
  REQUIRE(circle != topology.entities().end());
  std::vector<SketchDimensionId> ids;
  const auto dimension = [&](const char* id, SketchDimensionKind kind,
                             std::vector<SketchConstraintIntentTarget> targets, Point2 label) {
    auto intent = checked(SketchDimensionIntent::create(
        SketchDimensionId(id), kind, SketchDimensionMode::Reference, std::move(targets)));
    checked(GuiSketchDimensionController::add(ui.window.session(), ui.sketch(), intent));
    const auto initial = ui.annotation(intent.id()).point;
    REQUIRE(GuiSketchDimensionController::set_label_offset(
        ui.window.session(), ui.sketch(), intent.id(), {label.x - initial.x, label.y - initial.y}));
    ids.push_back(intent.id());
  };
  dimension("horizontal", SketchDimensionKind::HorizontalDistance, point_targets("entity/offset"),
            {-20, -35});
  dimension("vertical", SketchDimensionKind::VerticalDistance, point_targets("entity/offset"),
            {-40, -14});
  dimension("aligned", SketchDimensionKind::AlignedDistance, point_targets("entity/offset"),
            {0, -30});
  dimension("length", SketchDimensionKind::Length, {entity("entity/plate.0")}, {0, -65});
  dimension("angle", SketchDimensionKind::Angle,
            {entity("entity/angle.base"), entity("entity/angle.sloped")}, {18, -3});
  dimension("radius", SketchDimensionKind::Radius, {entity("entity/left.arc")}, {-50, 22});
  dimension("arc-length", SketchDimensionKind::ArcLength, {entity("entity/left.arc")}, {-20, 46});
  dimension("diameter", SketchDimensionKind::Diameter,
            {checked(SketchConstraintIntentTarget::entity(circle->id()))}, {65, 20});
  REQUIRE(ui.catalog().dimensions().size() == 8);
  ui.clear();
  ui.window.viewport().fit_all();
  events();
  const auto zoom = [&](int delta) {
    const auto local = QPointF(ui.window.viewport().rect().center());
    QWheelEvent event(local, ui.window.viewport().mapToGlobal(local.toPoint()), {}, {0, delta},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&ui.window.viewport(), &event);
    events();
  };
  zoom(-240);
  const auto before = ui.snapshot();
  const auto prefix = oblique ? QStringLiteral("dimension-layout-oblique")
                              : QStringLiteral("dimension-layout-normal");
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto a =
        checked(ui.window.viewport().sketch_dimension_screen_layout(ui.annotation(ids[i])));
    REQUIRE(a.text_bounds);
    for (std::size_t j = i + 1; j < ids.size(); ++j) {
      const auto b =
          checked(ui.window.viewport().sketch_dimension_screen_layout(ui.annotation(ids[j])));
      REQUIRE(b.text_bounds);
      CAPTURE(ids[i].value(), ids[j].value());
      CHECK((a.text_bounds->right() < b.text_bounds->left() ||
             b.text_bounds->right() < a.text_bounds->left() ||
             a.text_bounds->bottom() < b.text_bounds->top() ||
             b.text_bounds->bottom() < a.text_bounds->top()));
    }
  }
  for (const auto& id : ids) {
    ui.clear();
    ui.click(ui.label(id));
    const auto& selected = ui.window.session().selection().entries();
    REQUIRE(selected.size() == 1);
    CHECK(selected.front().semantic_id.ends_with("/dimension/" + id.value()));
    CHECK(ui.snapshot() == before);
  }
  ui.capture(prefix + "-neighbors");
  ui.clear();
  const auto line_pick = checked(ui.window.viewport().sketch_plane_to_screen({-28, -35}));
  ui.click({line_pick.x, line_pick.y});
  REQUIRE(ui.window.session().selection().entries().size() == 1);
  CHECK(ui.window.session().selection().entries().front().semantic_id.ends_with(
      "/dimension/horizontal"));
  ui.clear();
  const auto false_chord = checked(ui.window.viewport().sketch_plane_to_screen({-20, -14}));
  ui.click({false_chord.x, false_chord.y});
  CHECK(std::none_of(ui.window.session().selection().entries().begin(),
                     ui.window.session().selection().entries().end(), [](const auto& selection) {
                       return selection.semantic_id.ends_with("/dimension/horizontal");
                     }));
  CHECK(ui.snapshot() == before);
  ui.clear();
  const auto initial_offset = ui.catalog().find(SketchDimensionId("horizontal"))->label_offset();
  const auto start = ui.label(SketchDimensionId("horizontal"));
  const auto end = checked(ui.window.viewport().sketch_plane_to_screen({-18, -40}));
  QCursor::setPos(ui.window.viewport().mapToGlobal(start.toPoint()));
  QGuiApplication::sync();
  events();
  ui.mouse(QEvent::MouseButtonPress, start);
  QCursor::setPos(ui.window.viewport().mapToGlobal(QPoint(qRound(end.x), qRound(end.y))));
  ui.mouse(QEvent::MouseMove, {end.x, end.y});
  events();
  CHECK(ui.snapshot() == before);
  ui.capture(prefix + "-drag-preview");
  ui.mouse(QEvent::MouseButtonRelease, {end.x, end.y});
  events();
  const auto moved = ui.catalog().find(SketchDimensionId("horizontal"))->label_offset();
  CHECK(std::hypot(moved.x - initial_offset.x, moved.y - initial_offset.y) > 2);
  CHECK(ui.topology() == topology);
  const auto after = ui.snapshot();
  REQUIRE(after != before);
  REQUIRE(ui.window.undo());
  CHECK(ui.snapshot() == before);
  REQUIRE(ui.window.redo());
  CHECK(ui.snapshot() == after);
  zoom(120);
  CHECK(ui.snapshot() == after);
  ui.clear();
  ui.click(ui.label(SketchDimensionId("horizontal")));
  REQUIRE(ui.window.session().selection().entries().size() == 1);
  CHECK(ui.window.session().selection().entries().front().semantic_id.ends_with(
      "/dimension/horizontal"));
  ui.capture(prefix + "-zoomed");
  ui.roundtrip();
}
