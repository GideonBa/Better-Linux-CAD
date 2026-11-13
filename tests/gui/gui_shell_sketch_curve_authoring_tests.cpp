#include "../support/equation_curve_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_sketch_constraints.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_curve_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::equation_fixture;
namespace {
template <class T> T& field(ShellWindow& window, const char* id) {
  auto* result = window.findChild<T*>(id);
  REQUIRE(result);
  return *result;
}
void events() {
  QGuiApplication::sync();
  qApp->processEvents();
}
void show(ShellWindow& window) {
  window.resize(1280, 800);
  if (QApplication::platformName() == "xcb") {
    const auto area =
        QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
    window.move(area.topLeft());
    window.resize(std::min(1280, area.width()), std::min(800, area.height()));
  }
  window.show();
  window.raise();
  window.activateWindow();
  events();
  if (QApplication::platformName() == "xcb")
    REQUIRE(window.viewport().native_viewer_available());
}
std::string snapshot(const ShellWindow& window) {
  return f::checked(serialize_part_document_to_json(*window.session().part_document()));
}
void enter(ShellWindow& window) {
  REQUIRE(window.new_part("Curve authoring"));
  REQUIRE(window.session().commit_part_transaction("Curve parameter", [](PartDocument& part) {
    return part.add_parameter(f::checked(
        Parameter::create_length(ParameterId("r"), "Radius", f::checked(Quantity::length_mm(2)))));
  }));
  window.refresh();
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.active_sketch());
}
void set(ShellWindow& window, const char* id, const char* value) {
  field<QLineEdit>(window, id).setText(value);
}
void button(ShellWindow& window, const char* id) {
  field<QPushButton>(window, id).click();
  events();
}
void key(QWidget& widget, int value, Qt::KeyboardModifiers modifiers = {}) {
  widget.setFocus();
  events();
  QKeyEvent press(QEvent::KeyPress, value, modifiers);
  QApplication::sendEvent(&widget, &press);
  QKeyEvent release(QEvent::KeyRelease, value, modifiers);
  QApplication::sendEvent(&widget, &release);
  events();
}
void capture(ShellWindow& window, const char* name) {
  const auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (root.isEmpty())
    return;
  REQUIRE(QDir().mkpath(root));
  events();
  REQUIRE(window.grab().save(root + '/' + name + ".png"));
}
} // namespace
TEST_CASE("Equation authoring previews typed bindings and commits exact raw intent once",
          "[gui][parity-b162][equation-authoring]") {
  ShellWindow window;
  show(window);
  enter(window);
  const auto sketch = *window.active_sketch();
  const auto before = snapshot(window);
  auto& tools = window.sketch_tools().curve_tools();
  tools.equation_action().trigger();
  REQUIRE(tools.active());
  auto& x = field<QLineEdit>(window, "blcad.equation.x");
  auto& y = field<QLineEdit>(window, "blcad.equation.y");
  key(x, Qt::Key_Tab);
  CHECK(y.hasFocus());
  key(y, Qt::Key_Backtab, Qt::ShiftModifier);
  CHECK(x.hasFocus());
  x.clear();
  x.setFocus();
  events();
  auto& parameter = field<QComboBox>(window, "blcad.equation.parameter");
  parameter.setCurrentIndex(parameter.findData("r"));
  button(window, "blcad.equation.insert");
  CHECK(x.text() == "@{r}");
  x.insert("*cos(u)");
  CHECK(x.text() == "@{r}*cos(u)");
  set(window, "blcad.equation.y", "@{r}*sin(u)");
  button(window, "blcad.sketch.curve.preview");
  INFO(field<QLabel>(window, "blcad.sketch.curve.summary").text().toStdString());
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK(field<QLabel>(window, "blcad.sketch.curve.differential").text().contains("Derived start"));
  set(window, "blcad.equation.tolerance", "0");
  key(field<QLineEdit>(window, "blcad.equation.tolerance"), Qt::Key_Return, Qt::ControlModifier);
  CHECK(tools.active());
  CHECK_FALSE(field<QPushButton>(window, "blcad.sketch.curve.apply").isEnabled());
  CHECK(snapshot(window) == before);
  set(window, "blcad.equation.tolerance", "0.0005");
  button(window, "blcad.sketch.curve.preview");
  button(window, "blcad.sketch.curve.fit");
  capture(window, "equation-authoring-preview");
  button(window, "blcad.sketch.curve.apply");
  REQUIRE_FALSE(tools.active());
  const auto after = snapshot(window);
  CHECK(after != before);
  const auto* source = window.session().part_document()->find_sketch(sketch)->find_equation_curve(
      SketchEntityId("equation.1"));
  REQUIRE(source);
  CHECK(source->coordinates()[0] == "@{r}*cos(u)");
  CHECK(source->tolerance_mm() == .0005);
  const auto topology = f::checked(
      SketchTopology::migrate_legacy(*window.session().part_document()->find_sketch(sketch)));
  CHECK(topology.find_entity("entity/equation.1")->points().empty());
  const auto scene = f::checked(GuiSketchInteractionSceneBuilder{}.build(
      *window.session().part_document(), *window.session().part_document()->find_sketch(sketch)));
  CHECK(std::any_of(scene.curves.begin(), scene.curves.end(), [](const auto& curve) {
    return curve.kind == GuiSketchCurveKind::Equation && curve.polyline.size() > 10;
  }));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == after);
  window.refresh();
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("equation.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(f::checked(serialize_part_document_to_json(*reopened.part_document())) == after);
}
TEST_CASE("Equation edit corrections and cancellation preserve the committed model and domain",
          "[gui][parity-b162][equation-authoring]") {
  ShellWindow window;
  show(window);
  enter(window);
  auto& tools = window.sketch_tools().curve_tools();
  REQUIRE(tools.begin_equation());
  REQUIRE(tools.preview());
  REQUIRE(tools.apply());
  const auto sketch = *window.active_sketch();
  const auto semantic = "sketch/" + sketch.value() + "/entity/equation.1";
  REQUIRE(window.viewport().select_semantic(semantic));
  const auto before = snapshot(window);
  REQUIRE(tools.begin_equation(true));
  CHECK_FALSE(window.finish_active_sketch());
  CHECK(tools.active());
  set(window, "blcad.equation.upper", "6.283185307179586");
  CHECK_FALSE(tools.preview());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.viewport().feature_preview_active());
  set(window, "blcad.equation.lower", "pi/3");
  set(window, "blcad.equation.upper", "pi/3+2*pi");
  set(window, "blcad.equation.tolerance", "0.0003");
  REQUIRE(tools.preview());
  CHECK(snapshot(window) == before);
  key(field<QLineEdit>(window, "blcad.equation.upper"), Qt::Key_Escape);
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == before);
  REQUIRE(window.viewport().select_semantic(semantic));
  REQUIRE(tools.begin_equation(true));
  set(window, "blcad.equation.x", "1mm/(u-3)");
  CHECK_FALSE(tools.preview());
  CHECK(snapshot(window) == before);
  set(window, "blcad.equation.x", "10mm*cos(u)");
  REQUIRE(tools.preview());
  key(field<QLineEdit>(window, "blcad.equation.x"), Qt::Key_Return, Qt::ControlModifier);
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Equation modes and real model changes keep typed source and freshness contracts",
          "[gui][parity-b162][equation-authoring]") {
  ShellWindow window;
  show(window);
  enter(window);
  auto& tools = window.sketch_tools().curve_tools();
  for (int mode : {0, 1, 2}) {
    REQUIRE(tools.begin_equation());
    field<QComboBox>(window, "blcad.equation.mode").setCurrentIndex(mode);
    if (mode == 1)
      set(window, "blcad.equation.x", "@{r}");
    if (mode == 2) {
      set(window, "blcad.equation.x", "u*1mm");
      set(window, "blcad.equation.y", "sin(u)*1mm");
      field<QCheckBox>(window, "blcad.equation.closed").setChecked(false);
    }
    const auto before = snapshot(window);
    auto result = tools.preview();
    INFO((result.has_error() ? result.error().message() : "valid mode"));
    REQUIRE(result);
    CHECK(snapshot(window) == before);
    REQUIRE(tools.apply());
  }
  const auto sketch = *window.active_sketch();
  const auto& equations = window.session().part_document()->find_sketch(sketch)->equation_curves();
  REQUIRE(equations.size() == 3);
  CHECK(equations[0].mode() == EquationCurveMode::Cartesian2D);
  CHECK(equations[0].variable().unit == CurveUnit::Length);
  CHECK(equations[1].mode() == EquationCurveMode::Polar2D);
  CHECK(equations[1].variable().unit == CurveUnit::Angle);
  CHECK(equations[2].mode() == EquationCurveMode::Parametric2D);
  REQUIRE(window.viewport().select_semantic("sketch/" + sketch.value() + "/entity/equation.2"));
  REQUIRE(tools.begin_equation(true));
  REQUIRE(window.session().commit_part_transaction("Change equation input", [](PartDocument& part) {
    auto changed = part.set_parameter_value(ParameterId("r"), f::checked(Quantity::length_mm(3)));
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto changed = snapshot(window);
  CHECK_FALSE(tools.preview());
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == changed);
  CHECK(window.session().undo_label() == "Change equation input");
}
namespace {
void run_command(ShellWindow& window, const QString& title) {
  bool found = false;
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, [&] {
    if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
      modal->reject();
  });
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog)
      return;
    auto* query = dialog->findChild<QLineEdit*>("blcad.command_search.query");
    auto* results = dialog->findChild<QTreeWidget*>("blcad.command_search.results");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!query || !results || !buttons) {
      dialog->reject();
      return;
    }
    query->setText(title);
    for (int i = 0; i < results->topLevelItemCount(); ++i)
      if (results->topLevelItem(i)->text(0) == title) {
        results->setCurrentItem(results->topLevelItem(i));
        found = buttons->button(QDialogButtonBox::Ok)->isEnabled();
        if (found)
          buttons->button(QDialogButtonBox::Ok)->click();
        else
          dialog->reject();
        return;
      }
    dialog->reject();
  });
  watchdog.start(3000);
  window.ribbon().show_command_search();
  watchdog.stop();
  REQUIRE(found);
}
void choose(ShellWindow& window, const char* control, const char* value) {
  auto& combo = field<QComboBox>(window, control);
  const int index = combo.findData(QString::fromUtf8(value));
  REQUIRE(index >= 0);
  combo.setCurrentIndex(index);
}
void fixed(ShellWindow& window, const SketchId& sketch, const SketchPointId& point,
           const char* id) {
  const auto intent = f::checked(
      SketchConstraintIntent::create(SketchConstraintId(id), SketchSolverConstraintKind::Fixed,
                                     {f::checked(SketchConstraintIntentTarget::point(point))}));
  auto controller =
      f::checked(GuiSketchConstraintController::create(window.session(), sketch, intent));
  REQUIRE(controller.preview().authoring.accepted);
  REQUIRE(controller.commit(window.session()));
}
SketchId smooth_source(ShellWindow& window) {
  enter(window);
  const auto id = *window.active_sketch();
  REQUIRE(
      window.session().commit_part_transaction("Smooth source curves", [id](PartDocument& part) {
        auto sketch = *part.find_sketch(id);
        REQUIRE(sketch.add_entity(f::checked(SplineSegment::create_cubic_bezier(
            SketchEntityId("spline"), {0, 0}, {2, 0}, {4, 2}, {6, 2}))));
        REQUIRE(sketch.add_entity(
            f::checked(LineSegment::create(SketchEntityId("bottom"), {0, 0}, {-4, 0}))));
        REQUIRE(sketch.add_entity(
            f::checked(LineSegment::create(SketchEntityId("top"), {6, 2}, {-4, 2}))));
        REQUIRE(sketch.add_entity(
            f::checked(LineSegment::create(SketchEntityId("side"), {-4, 2}, {-4, 0}))));
        REQUIRE(sketch.add_profile(f::checked(ArcClosedProfile::create(
            ProfileId("smooth.profile"), {SketchEntityId("bottom"), SketchEntityId("spline"),
                                          SketchEntityId("top"), SketchEntityId("side")}))));
        return part.update_sketch(sketch);
      }));
  const auto topology = f::checked(
      SketchTopology::migrate_legacy(*window.session().part_document()->find_sketch(id)));
  const auto spline = topology.find_entity("entity/spline")->points(),
             line = topology.find_entity("entity/bottom")->points();
  fixed(window, id, spline[0], "fixed.start");
  fixed(window, id, spline[1], "fixed.control");
  fixed(window, id, spline[3], "fixed.end");
  fixed(window, id, line[1], "fixed.line");
  window.sketch_tools().activate();
  window.refresh();
  return id;
}
void mouse(OcctViewport& viewport, QEvent::Type type, GuiSketchScreenPoint p,
           Qt::MouseButton button, Qt::MouseButtons buttons) {
  const QPointF local(p.x, p.y), global(viewport.mapToGlobal(local.toPoint()));
  QMouseEvent event(type, local, global, button, buttons, Qt::NoModifier);
  QApplication::sendEvent(&viewport, &event);
  events();
}
Point2 drag(ShellWindow& window, Point2 start, Point2 intermediate, Point2 released) {
  const auto a = f::checked(window.viewport().sketch_plane_to_screen(start));
  const auto b = f::checked(window.viewport().sketch_plane_to_screen(intermediate));
  const auto c = f::checked(window.viewport().sketch_plane_to_screen(released));
  // Native picking rounds DIPs to drawable pixels. Independently intersect
  // the actual release ray with this fixture's XY plane; solver output must
  // match that final input, not the preceding move or an ideal subpixel point.
  const auto ray = f::checked(window.viewport().screen_to_view_ray(c));
  REQUIRE(std::abs(ray.direction.z) > 1e-12);
  const double distance = -ray.origin.z / ray.direction.z;
  const Point2 expected{ray.origin.x + distance * ray.direction.x,
                        ray.origin.y + distance * ray.direction.y};
  const double pixels_per_mm =
      std::hypot(c.x - a.x, c.y - a.y) / std::hypot(released.x - start.x, released.y - start.y);
  CHECK(expected.x == Catch::Approx(released.x).margin(1 / pixels_per_mm));
  mouse(window.viewport(), QEvent::MouseMove, a, Qt::NoButton, Qt::NoButton);
  mouse(window.viewport(), QEvent::MouseButtonPress, a, Qt::LeftButton, Qt::LeftButton);
  mouse(window.viewport(), QEvent::MouseMove, b, Qt::NoButton, Qt::LeftButton);
  mouse(window.viewport(), QEvent::MouseButtonRelease, c, Qt::LeftButton, Qt::NoButton);
  return expected;
}
double body_volume(ShellWindow& window) {
  const auto& bodies = window.session().part_document()->bodies();
  REQUIRE(bodies.size() == 1);
  const auto* shape = window.session().part_shape_cache()->find_body_shape(bodies[0].id());
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
void extrude(ShellWindow& window, const SketchId& sketch) {
  REQUIRE(window.finish_active_sketch());
  const auto* source = window.session().part_document()->find_sketch(sketch);
  REQUIRE(source->arc_closed_profiles().size() == 1);
  REQUIRE(window.viewport().select_semantic("sketch/" + sketch.value() + "/profile/" +
                                            source->arc_closed_profiles()[0].id().value()));
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  REQUIRE(window.feature_tools().apply());
  window.refresh();
  events();
}
void reopen_sketch(ShellWindow& window, const SketchId& sketch) {
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString().toStdString() == sketch.value()) {
      window.model_browser().itemDoubleClicked(*it, 0);
      events();
      REQUIRE(window.active_sketch() == sketch);
      return;
    }
  FAIL("Source sketch browser row unavailable");
}
void footer(ShellWindow& window) {
  for (const auto* id : {"blcad.sketch.curve.preview", "blcad.sketch.curve.fit",
                         "blcad.sketch.curve.apply", "blcad.sketch.curve.cancel"}) {
    auto& value = field<QPushButton>(window, id);
    REQUIRE(value.isVisible());
    CHECK(window.rect().contains(QRect(value.mapTo(&window, QPoint(0, 0)), value.size())));
  }
}
} // namespace
TEST_CASE("Smooth task rejects conflicts, restores cancellation and publishes one accepted catalog",
          "[gui][parity-b162][smooth-task]") {
  ShellWindow window;
  show(window);
  const auto sketch = smooth_source(window);
  auto& tools = window.sketch_tools().curve_tools();
  const auto before = snapshot(window);
  const auto catalog = f::checked(window.session().sketch_constraint_catalog(sketch));
  const auto select = [&] {
    REQUIRE(tools.smooth_action().isEnabled());
    tools.smooth_action().trigger();
    REQUIRE(tools.active());
    CHECK_FALSE(tools.smooth_action().isEnabled());
    CHECK_FALSE(tools.equation_action().isEnabled());
    choose(window, "blcad.smooth.a", "spline");
    choose(window, "blcad.smooth.b", "bottom");
  };
  select();
  field<QComboBox>(window, "blcad.smooth.a_end").setCurrentIndex(1);
  CHECK_FALSE(tools.preview());
  CHECK(snapshot(window) == before);
  CHECK(f::checked(window.session().sketch_constraint_catalog(sketch)) == catalog);
  field<QComboBox>(window, "blcad.smooth.a_end").setCurrentIndex(0);
  REQUIRE(tools.preview());
  CHECK(snapshot(window) == before);
  button(window, "blcad.sketch.curve.cancel");
  CHECK_FALSE(tools.active());
  CHECK(tools.smooth_action().isEnabled());
  CHECK(tools.equation_action().isEnabled());
  CHECK(snapshot(window) == before);
  CHECK(f::checked(window.session().sketch_constraint_catalog(sketch)) == catalog);
  select();
  REQUIRE(tools.preview());
  REQUIRE(tools.apply());
  const auto after = snapshot(window);
  const auto accepted = f::checked(window.session().sketch_constraint_catalog(sketch));
  CHECK(accepted.constraints().size() == catalog.constraints().size() + 1);
  CHECK(window.session()
            .part_document()
            ->find_sketch(sketch)
            ->find_spline_segment(SketchEntityId("spline"))
            ->control2()
            .y == Catch::Approx(0).margin(1e-7));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  CHECK(f::checked(window.session().sketch_constraint_catalog(sketch)) == catalog);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == after);
  CHECK(f::checked(window.session().sketch_constraint_catalog(sketch)) == accepted);
}
TEST_CASE(
    "Native Smooth corrects endpoint conflict and carries constrained grip release into a solid",
    "[gui][parity-b162][native-curve-workflow][smooth-authoring]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native Smooth authoring and constrained pointer grips");
  ShellWindow window;
  show(window);
  const auto sketch = smooth_source(window);
  auto& tools = window.sketch_tools().curve_tools();
  const auto before = snapshot(window);
  run_command(window, "Smooth (G2)");
  REQUIRE(tools.active());
  choose(window, "blcad.smooth.a", "spline");
  choose(window, "blcad.smooth.b", "bottom");
  field<QComboBox>(window, "blcad.smooth.a_end").setCurrentIndex(1);
  CHECK_FALSE(tools.preview());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(field<QPushButton>(window, "blcad.sketch.curve.apply").isEnabled());
  field<QComboBox>(window, "blcad.smooth.a_end").setCurrentIndex(0);
  auto preview = tools.preview();
  INFO((preview.has_error() ? preview.error().message() : "G2 preview"));
  REQUIRE(preview);
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().sketch_drag_handle_count() == 0);
  footer(window);
  button(window, "blcad.sketch.curve.fit");
  capture(window, "smooth-native-preview");
  button(window, "blcad.sketch.curve.apply");
  REQUIRE_FALSE(tools.active());
  const auto constrained = snapshot(window);
  const auto* curve = window.session().part_document()->find_sketch(sketch)->find_spline_segment(
      SketchEntityId("spline"));
  REQUIRE(curve);
  CHECK(curve->control2().x == Catch::Approx(4).margin(1e-7));
  CHECK(curve->control2().y == Catch::Approx(0).margin(1e-7));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == constrained);
  window.sketch_tools().activate();
  window.refresh();
  const auto final_pointer = drag(window, {4, 0}, {4.5, 0}, {5, 0});
  const auto* moved = window.session().part_document()->find_sketch(sketch)->find_spline_segment(
      SketchEntityId("spline"));
  CHECK(moved->control2().x == Catch::Approx(final_pointer.x).margin(1e-7));
  CHECK(moved->control2().y == Catch::Approx(0).margin(1e-7));
  const auto released = snapshot(window);
  CHECK(released != constrained);
  (void)drag(window, {final_pointer.x, 0}, {final_pointer.x, .5}, {final_pointer.x, 1});
  CHECK(snapshot(window) == released);
  capture(window, "smooth-native-grip");
  extrude(window, sketch);
  // Integrating the cubic boundary gives area 14.6 + .6*C2.x;
  // the actual Extrude depth is 5 mm.
  CHECK(body_volume(window) == Catch::Approx(73 + 3 * final_pointer.x).margin(1e-5));
  const auto solid = snapshot(window);
  REQUIRE(window.session().undo());
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == solid);
  CHECK(body_volume(window) == Catch::Approx(73 + 3 * final_pointer.x).margin(1e-5));
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("smooth.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(f::checked(serialize_part_document_to_json(*reopened.part_document())) == solid);
  CHECK(reopened.sketch_constraint_catalog(sketch).value().constraints().size() == 5);
}
TEST_CASE("Native equation authoring finishes a bound profile and edits downstream solid history",
          "[gui][parity-b162][native-curve-workflow]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native equation profile and downstream authoring");
  ShellWindow window;
  show(window);
  enter(window);
  const auto sketch = *window.active_sketch();
  auto& tools = window.sketch_tools().curve_tools();
  run_command(window, "Equation Curve");
  REQUIRE(tools.active());
  set(window, "blcad.equation.x", "@{r}*cos(u)");
  set(window, "blcad.equation.y", "@{r}*sin(u)");
  button(window, "blcad.sketch.curve.preview");
  footer(window);
  REQUIRE(window.viewport().feature_preview_active());
  button(window, "blcad.sketch.curve.fit");
  capture(window, "equation-native-preview");
  button(window, "blcad.sketch.curve.apply");
  REQUIRE_FALSE(tools.active());
  extrude(window, sketch);
  CHECK(body_volume(window) == Catch::Approx(20 * std::numbers::pi).margin(1e-6));
  REQUIRE(window.session().commit_part_transaction("Drive equation radius", [](PartDocument& part) {
    auto result = part.set_parameter_value(ParameterId("r"), f::checked(Quantity::length_mm(3)));
    return result ? Result<std::size_t>::success(result.value().size())
                  : Result<std::size_t>::failure(result.error());
  }));
  CHECK(body_volume(window) == Catch::Approx(45 * std::numbers::pi).margin(1e-6));
  const auto body_semantic = "body/" + window.session().part_document()->bodies()[0].id().value();
  REQUIRE(window.session().set_presentation_visible(body_semantic, false));
  window.refresh();
  reopen_sketch(window, sketch);
  REQUIRE(window.viewport().select_semantic("sketch/" + sketch.value() + "/entity/equation.1"));
  run_command(window, "Edit Equation Curve");
  REQUIRE(tools.active());
  const auto before = snapshot(window);
  set(window, "blcad.equation.lower", "pi/3");
  set(window, "blcad.equation.upper", "pi/3+2*pi");
  set(window, "blcad.equation.tolerance", "0.00025");
  REQUIRE(tools.preview());
  CHECK(snapshot(window) == before);
  button(window, "blcad.sketch.curve.fit");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.viewport().feature_preview_replaces(body_semantic));
  const auto endpoint =
      f::checked(window.viewport().sketch_plane_to_screen({1.5, 3 * std::sqrt(3.) / 2}));
  mouse(window.viewport(), QEvent::MouseMove, endpoint, Qt::NoButton, Qt::NoButton);
  REQUIRE(window.viewport().sketch_snap_result());
  CHECK(window.viewport().sketch_snap_result()->kind == GuiSketchSnapKind::Endpoint);
  CHECK(window.viewport().sketch_snap_result()->snapped_point.x ==
        Catch::Approx(1.5).margin(1e-10));
  capture(window, "equation-native-source-edit");
  key(field<QLineEdit>(window, "blcad.equation.upper"), Qt::Key_Return, Qt::ControlModifier);
  REQUIRE_FALSE(tools.active());
  const auto edited = snapshot(window);
  CHECK(edited != before);
  CHECK(body_volume(window) == Catch::Approx(45 * std::numbers::pi).margin(1e-6));
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == edited);
  window.refresh();
  window.viewport().fit_all();
  events();
  const auto pick =
      f::checked(window.viewport().sketch_plane_to_screen({-3 / std::sqrt(2.), 3 / std::sqrt(2.)}));
  mouse(window.viewport(), QEvent::MouseMove, pick, Qt::NoButton, Qt::NoButton);
  mouse(window.viewport(), QEvent::MouseButtonPress, pick, Qt::LeftButton, Qt::LeftButton);
  mouse(window.viewport(), QEvent::MouseButtonRelease, pick, Qt::LeftButton, Qt::NoButton);
  mouse(window.viewport(), QEvent::MouseButtonDblClick, pick, Qt::LeftButton, Qt::LeftButton);
  REQUIRE(tools.active());
  CHECK(field<QLineEdit>(window, "blcad.equation.lower").text() == "pi/3");
  button(window, "blcad.sketch.curve.cancel");
  CHECK(snapshot(window) == edited);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("equation-solid.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(f::checked(serialize_part_document_to_json(*reopened.part_document())) == edited);
  REQUIRE(reopened.part_document()->bodies().size() == 1);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*reopened.part_shape_cache()->find_body_shape(
                reopened.part_document()->bodies()[0].id()))
            .volume_mm3 == Catch::Approx(45 * std::numbers::pi).margin(1e-6));
}

TEST_CASE(
    "Native equation profile continues through the existing Revolve task and portable history",
    "[gui][parity-b162][native-curve-workflow]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native equation and Revolve authoring");
  ShellWindow window;
  show(window);
  enter(window);
  const auto sketch = *window.active_sketch();
  auto& tools = window.sketch_tools().curve_tools();
  run_command(window, "Equation Curve");
  REQUIRE(tools.active());
  set(window, "blcad.equation.x", "10mm+@{r}*cos(u)");
  set(window, "blcad.equation.y", "@{r}*sin(u)");
  button(window, "blcad.sketch.curve.preview");
  REQUIRE(window.viewport().feature_preview_active());
  button(window, "blcad.sketch.curve.apply");
  REQUIRE(window.finish_active_sketch());
  const auto* source = window.session().part_document()->find_sketch(sketch);
  REQUIRE(source->arc_closed_profiles().size() == 1);
  REQUIRE(window.feature_tools().select_profile(sketch, source->arc_closed_profiles()[0].id()));
  const auto before = snapshot(window);
  REQUIRE(window.feature_tools().begin_revolve(DatumAxisId("datum.axis.y")));
  field<QComboBox>(window, "blcad.revolve.extent").setCurrentIndex(0);
  REQUIRE(window.viewport().feature_preview_active());
  window.viewport().fit_feature_preview();
  events();
  capture(window, "equation-native-revolve");
  CHECK(snapshot(window) == before);
  REQUIRE(window.feature_tools().apply());
  CHECK(body_volume(window) ==
        Catch::Approx(80 * std::numbers::pi * std::numbers::pi).margin(1e-5));
  const auto after = snapshot(window);
  REQUIRE(window.session().undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.session().redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("equation-torus.blcad.json").toStdString();
  REQUIRE(window.session().save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(f::checked(serialize_part_document_to_json(*reopened.part_document())) == after);
  REQUIRE(reopened.part_document()->bodies().size() == 1);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*reopened.part_shape_cache()->find_body_shape(
                reopened.part_document()->bodies()[0].id()))
            .volume_mm3 == Catch::Approx(80 * std::numbers::pi * std::numbers::pi).margin(1e-5));
}
