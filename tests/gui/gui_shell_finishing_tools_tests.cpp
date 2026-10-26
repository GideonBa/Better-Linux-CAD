#include "blcad/core/part_document_json.hpp"
#include "blcad/core/rectangular_extrude_profile.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

using namespace blcad;
using namespace blcad::gui;

namespace {
using Command = ShellFeatureTools::Command;
std::string snapshot(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void box(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Finishing")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({0, 0}));
  REQUIRE(window.sketch_tools().add_pick({30, 20}));
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(10));
  REQUIRE(window.feature_tools().apply());
  window.resize(1280, 800);
  window.show();
  window.viewport().fit_all();
  qApp->processEvents();
}
void capture(ShellWindow& window, QString name) {
  const auto path = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (path.isEmpty())
    return;
  REQUIRE(QDir().mkpath(path));
  qApp->processEvents();
  CHECK(window.grab().save(QDir(path).filePath(name + QStringLiteral(".png"))));
}
std::size_t pick_role(ShellWindow& window, std::string_view role) {
  auto& tools = window.feature_tools();
  const auto targets = tools.finishing_selection_items();
  const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
    return target.semantic_id.ends_with(role);
  });
  REQUIRE(found != targets.end());
  REQUIRE(window.viewport().select_semantic(found->semantic_id));
  qApp->processEvents();
  return static_cast<std::size_t>(std::distance(targets.begin(), found));
}
QAction& action(ShellWindow& window, const char* id) {
  auto* result = window.feature_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(result);
  return *result;
}
} // namespace

TEST_CASE("Shell finishing ribbon creates real geometry with candidate-only parameters and atomic "
          "history",
          "[gui][shell-finishing][shell-features][block136-finishing]") {
  ShellWindow window;
  box(window);
  Command command = Command::Fillet;
  const char* id = "fillet";
  std::string role = "top_front";
  SECTION("fillet") {}
  SECTION("chamfer") {
    command = Command::Chamfer;
    id = "chamfer";
  }
  SECTION("shell") {
    command = Command::Shell;
    id = "shell";
    role = "top";
  }
  SECTION("draft") {
    command = Command::Draft;
    id = "draft";
    role = "right";
  }
  const auto before = snapshot(window);
  const auto parameter_count = window.session().part_document()->parameters().size();
  const auto mask = window.viewport().selection_filter_mask();
  const auto initial_presentations = window.viewport().presentation_count();
  auto& tools = window.feature_tools();
  auto& start = action(window, id);
  REQUIRE(start.isEnabled());
  start.trigger();
  REQUIRE(tools.active());
  CHECK(tools.command() == command);
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  CHECK_FALSE(apply->isEnabled());
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(window) == before);
  CHECK(tools.finishing_target_editor().count() ==
        (command == Command::Fillet || command == Command::Chamfer ? 12 : 6));
  pick_role(window, role);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(window.viewport().feature_preview_replaces("body/body.1"));
  REQUIRE(apply->isEnabled());
  REQUIRE(tools.set_numeric_text(command == Command::Draft ? QStringLiteral("4 deg")
                                                           : QStringLiteral("1.5 mm")));
  capture(window, QStringLiteral("finishing-") + id + QStringLiteral("-preview"));
  CHECK(snapshot(window) == before);
  apply->click();
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.viewport().feature_preview_replaces("body/body.1"));
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().presentation_count() == initial_presentations);
  CHECK(window.session().part_document()->parameters().size() == parameter_count + 1U);
  const auto* shape = window.session().part_shape_cache()->find_body_shape(BodyId("body.1"));
  REQUIRE(shape);
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(*shape);
  CHECK(summary.solid_count == 1U);
  CHECK(summary.volume_mm3 > 0);
  CHECK(std::abs(summary.volume_mm3 - 6000) > 1);
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.save_to(dir.filePath("finishing.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("finishing.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE("Finishing multi-edge selection and every chamfer mode survive real Apply",
          "[gui][shell-finishing][block136-finishing]") {
  ShellWindow window;
  box(window);
  auto& tools = window.feature_tools();
  int mode = 0;
  SECTION("equal distance") {}
  SECTION("two distances") {
    mode = 1;
  }
  SECTION("distance angle") {
    mode = 2;
  }
  const auto before = snapshot(window);
  const auto count = window.session().part_document()->parameters().size();
  REQUIRE(tools.begin_finishing(Command::Chamfer));
  tools.finishing_mode_editor().setCurrentIndex(mode);
  pick_role(window, "top_front");
  pick_role(window, "top_back");
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.coordinator().finishing().edge_count() == 2U);
  if (mode == 1) {
    const auto handles = tools.coordinator().finishing().handles();
    REQUIRE(handles.size() == 2U);
    CHECK(std::abs(dot(handles[0].axis, handles[1].axis)) < 1e-8);
  }
  if (mode > 0) {
    REQUIRE(tools.set_secondary_numeric_text(mode == 2 ? QStringLiteral("30 deg")
                                                       : QStringLiteral("0.5 mm")));
    CHECK(tools
              .set_secondary_numeric_text(mode == 2 ? QStringLiteral("3 mm")
                                                    : QStringLiteral("3 deg"))
              .has_error());
  }
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto& part = *window.session().part_document();
  REQUIRE(part.chamfer_features().size() == 1U);
  CHECK(part.chamfer_features().front().edges().size() == 2U);
  CHECK(part.chamfer_features().front().mode() == static_cast<ChamferMode>(mode));
  CHECK(part.parameters().size() == count + (mode == 0 ? 1U : 2U));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Finishing cancellation and rejected numeric edits preserve the last valid model",
          "[gui][shell-finishing][block136-finishing]") {
  ShellWindow window;
  box(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto history = std::string(window.session().undo_label());
  const auto mask = window.viewport().selection_filter_mask();
  REQUIRE(tools.begin_finishing(Command::Fillet));
  pick_role(window, "top_front");
  CHECK_FALSE(tools.set_numeric_text(QStringLiteral("2 deg")));
  CHECK_FALSE(tools.set_numeric_value(1000));
  CHECK(tools.coordinator().finishing().first_value() == Catch::Approx(1));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(window.session().can_undo());
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &escape);
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(tools.finishing_selection_items().empty());
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == history);
}

TEST_CASE("Shell direction and target toggling update a disposable preview",
          "[gui][shell-finishing][block136-finishing]") {
  ShellWindow window;
  box(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_finishing(Command::Shell));
  pick_role(window, "top");
  REQUIRE(window.viewport().feature_preview_active());
  tools.finishing_mode_editor().setCurrentIndex(1);
  CHECK(tools.coordinator().shell_draft().direction() == ShellDirection::Outward);
  REQUIRE(window.viewport().feature_preview_active());
  pick_role(window, "top");
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(window) == before);
  tools.cancel();
  CHECK(snapshot(window) == before);
}

TEST_CASE("Pending finishing parameters reject collisions without mutating a session",
          "[gui][shell-finishing][interactive-finishing][block136-finishing]") {
  ShellWindow window;
  box(window);
  const auto& part = *window.session().part_document();
  const auto body = part.bodies().front().id();
  const auto parameter = part.parameters().front();
  const auto before = snapshot(window);
  GuiInteractiveFinishingController controller;
  CHECK_FALSE(controller.begin_fillet(window.session(), body, FeatureId("fillet.collision"), "F",
                                      parameter.id(), {parameter}));
  CHECK_FALSE(controller.active());
  auto wrong =
      Parameter::create_angle(ParameterId("angle.wrong"), "Wrong", Quantity::angle_deg(3).value())
          .value();
  CHECK_FALSE(controller.begin_fillet(window.session(), body, FeatureId("fillet.wrong"), "F",
                                      wrong.id(), {wrong}));
  CHECK(snapshot(window) == before);
}

TEST_CASE("Native finishing picks actual body subshapes and couples drag with numeric refinement",
          "[gui][shell-finishing][native-shell-finishing][block136-finishing]") {
  if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
    SKIP("Requires the native xcb viewer for real OCCT subshape detection");
  ShellWindow window;
  box(window);
  REQUIRE(window.viewport().native_viewer_available());
  auto& tools = window.feature_tools();
  bool shell = false;
  SECTION("edge and radius handle") {}
  SECTION("removal face and thickness handle") {
    shell = true;
  }
  REQUIRE(tools.begin_finishing(shell ? Command::Shell : Command::Fillet));
  qApp->processEvents();
  capture(window, QStringLiteral("native-finishing-before-pick"));
  auto screen = window.viewport().model_to_screen(shell ? Point3{15, 10, 10} : Point3{15, 20, 10});
  REQUIRE(screen);
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point, Qt::MouseButtons buttons) {
    const QPointF local(point.x, point.y);
    QMouseEvent mouse(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &mouse);
    qApp->processEvents();
  };
  send(QEvent::MouseButtonPress, screen.value(), Qt::LeftButton);
  send(QEvent::MouseButtonRelease, screen.value(), Qt::NoButton);
  INFO("screen=" << screen.value().x << "," << screen.value().y);
  INFO("selected=" << (window.viewport().selected_semantic()
                           ? window.viewport().selected_semantic()->semantic_id
                           : "none"));
  INFO("active handle=" << tools.manipulators().active_handle_id());
  REQUIRE(tools.finishing_target_editor().selectedItems().size() == 1);
  REQUIRE(window.viewport().feature_preview_active());
  const auto before = snapshot(window);
  REQUIRE_FALSE(tools.manipulators().presentations().empty());
  const auto handle = tools.manipulators().presentations().front().endpoint;
  send(QEvent::MouseButtonPress, handle, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  REQUIRE(tools.set_numeric_text(QStringLiteral("1.25 mm")));
  send(QEvent::MouseMove, {handle.x + 8, handle.y}, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, {handle.x + 8, handle.y}, Qt::NoButton);
  CHECK(tools.numeric_editor().text().toDouble() == Catch::Approx(1.25));
  CHECK(snapshot(window) == before);
  capture(window, shell ? QStringLiteral("native-shell-pick-drag")
                        : QStringLiteral("native-fillet-pick-drag"));
  REQUIRE(tools.apply());
}

TEST_CASE("Changing a finishing Body clears old picks and applies only to the new target",
          "[gui][shell-finishing][block136-finishing]") {
  ShellWindow window;
  box(window);
  auto& tools = window.feature_tools();
  const auto sketch = window.session().part_document()->sketches().front();
  REQUIRE(tools.select_profile(sketch.id(), sketch.closed_profiles().front().id()));
  REQUIRE(tools.begin_extrude());
  REQUIRE(tools.set_numeric_value(15));
  REQUIRE(tools.apply());
  const auto before = snapshot(window);
  REQUIRE(tools.begin_finishing(Command::Fillet));
  REQUIRE(tools.finishing_body_editor().count() == 2);
  pick_role(window, "top_front");
  REQUIRE(window.viewport().feature_preview_active());
  const auto old_targets = tools.finishing_selection_items();
  tools.finishing_body_editor().setCurrentIndex(1);
  CHECK(tools.finishing_target_editor().selectedItems().empty());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.viewport().select_semantic(old_targets.front().semantic_id));
  pick_role(window, "top_front");
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto& feature = window.session().part_document()->fillet_features().front();
  CHECK(feature.target_body() == BodyId("body.2"));
  const auto first = geometry::RectangleExtrusionAdapter{}.summarize(
      *window.session().part_shape_cache()->find_body_shape(BodyId("body.1")));
  CHECK(first.volume_mm3 == Catch::Approx(6000));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Circular rim finishing and overlapping Body Union form a reversible saved workflow",
          "[gui][shell-finishing][shell-body][curved-finishing][curved-union-workflow]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Rounded union")));
  window.resize(1380, 900);
  window.show();
  const auto cylinder = [&](Point2 center, double radius) {
    REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
    REQUIRE(window.create_sketch_on_selection());
    REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
    REQUIRE(window.sketch_tools().add_pick(center));
    REQUIRE(window.sketch_tools().add_pick({center.x + radius, center.y}));
    REQUIRE(window.finish_active_sketch());
    REQUIRE(window.feature_tools().begin_extrude());
    REQUIRE(window.feature_tools().set_numeric_value(8));
    REQUIRE(window.feature_tools().apply());
    window.viewport().fit_all();
    qApp->processEvents();
  };
  cylinder({20, 0}, 10);
  auto& tools = window.feature_tools();
  const auto pick_circle = [&](double z, bool native) {
    const auto built = geometry::FinishingSelectionBuilder::build(
        *window.session().part_document(), *window.session().part_shape_cache(), BodyId("body.1"),
        tools.command() == Command::Fillet ? PartFeatureInputRole::FilletEdge
                                           : PartFeatureInputRole::ChamferEdge);
    REQUIRE(built);
    const auto& targets = built.value();
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const auto& target) {
      const auto* witness =
          std::get_if<ResultEdgeReference>(&std::get<EdgeReference>(target.reference).source());
      return witness && witness->curve_type() == 1 && std::abs(target.anchor.z - z) < 0.01;
    });
    REQUIRE(found != targets.end());
    if (native && QApplication::platformName() == QStringLiteral("xcb")) {
      const auto screen = window.viewport().model_to_screen({20, 10, z});
      REQUIRE(screen);
      const QPointF local(screen.value().x, screen.value().y);
      for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(
            type, local, window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
            type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&window.viewport(), &event);
        qApp->processEvents();
      }
      REQUIRE(tools.finishing_target_editor().selectedItems().size() == 1);
    } else
      REQUIRE(window.viewport().select_semantic(found->semantic_id));
  };
  const auto before_round = snapshot(window);
  REQUIRE(tools.begin_finishing(Command::Fillet));
  pick_circle(8, true);
  REQUIRE(tools.set_numeric_value(1));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before_round);
  capture(window, QStringLiteral("curved-rim-fillet-preview"));
  REQUIRE(tools.apply());
  const auto rounded = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before_round);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == rounded);
  REQUIRE(tools.begin_finishing(Command::Chamfer));
  pick_circle(0, false);
  REQUIRE(tools.set_numeric_value(0.5));
  REQUIRE(tools.apply());
  REQUIRE(window.session().part_document()->fillet_features().size() == 1);
  REQUIRE(window.session().part_document()->chamfer_features().size() == 1);
  cylinder({30, 0}, 6);
  const auto before_union = snapshot(window);
  REQUIRE(tools.begin_body_operation(Command::BodyBoolean));
  REQUIRE(tools.boolean_operation_editor().currentIndex() == 0);
  tools.boolean_target_editor().setCurrentIndex(tools.boolean_target_editor().findData("body.1"));
  REQUIRE(window.viewport().select_semantic("body/body.2"));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before_union);
  capture(window, QStringLiteral("curved-bodies-union-preview"));
  REQUIRE(tools.apply());
  const auto* shape = window.session().part_shape_cache()->find_body_shape(BodyId("body.1"));
  REQUIRE(shape);
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(*shape);
  CHECK(summary.solid_count == 1);
  CHECK(summary.volume_mm3 > 2500);
  CHECK(summary.volume_mm3 < 3450);
  CHECK_FALSE(window.session().part_shape_cache()->find_body_shape(BodyId("body.2")));
  const auto united = snapshot(window);
  auto restored_union = deserialize_part_document_from_json(united);
  INFO((restored_union ? "Union history is readable" : restored_union.error().message()));
  REQUIRE(restored_union);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before_union);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == united);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("rounded-union.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(directory.filePath("rounded-union.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == united);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*reopened.part_shape_cache()->find_body_shape(BodyId("body.1")))
            .volume_mm3 == Catch::Approx(summary.volume_mm3));
}

TEST_CASE(
    "A rectangle around an editable rational ellipse extrudes only the selected outer material",
    "[gui][shell-finishing][sketch-curved-regions][nested-spline-region]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Spline aperture")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  auto& sketch_tools = window.sketch_tools();
  REQUIRE(sketch_tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(sketch_tools.add_pick({-20, -15}));
  REQUIRE(sketch_tools.add_pick({20, 15}));
  REQUIRE(sketch_tools.begin_tool(GuiSketchCreateTool::Ellipse));
  REQUIRE(sketch_tools.add_pick({0, 0}));
  REQUIRE(sketch_tools.add_pick({8, 0}));
  REQUIRE(sketch_tools.add_pick({0, 4}));
  const auto sketch_id = *window.active_sketch();
  const auto authored = *window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(authored.closed_profiles().size() == 1);
  REQUIRE(authored.arc_closed_profiles().size() == 1);
  const auto outer_id = authored.closed_profiles().front().id();
  const auto inner_id = authored.arc_closed_profiles().front().id();
  REQUIRE(window.finish_active_sketch());
  window.resize(1360, 900);
  window.show();
  window.viewport().fit_all();
  qApp->processEvents();
  const auto before = snapshot(window);
  auto& tools = window.feature_tools();
  REQUIRE(tools.select_profile(sketch_id, inner_id));
  REQUIRE(tools.begin_extrude());
  REQUIRE(tools.set_numeric_value(6));
  REQUIRE(window.viewport().feature_preview_active());
  tools.cancel();
  CHECK(snapshot(window) == before);
  REQUIRE(tools.select_profile(sketch_id, outer_id));
  REQUIRE(tools.begin_extrude());
  REQUIRE(tools.set_numeric_value(6));
  REQUIRE(window.viewport().feature_preview_active());
  capture(window, QStringLiteral("nested-spline-outer-material-preview"));
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto& part = *window.session().part_document();
  const auto* shape =
      window.session().part_shape_cache()->find_body_shape(part.bodies().front().id());
  REQUIRE(shape);
  const auto volume = geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
  CHECK(volume == Catch::Approx((40 * 30 - std::acos(-1.0) * 8 * 4) * 6).epsilon(1e-8));
  const auto* unchanged = part.find_sketch(sketch_id);
  REQUIRE(unchanged);
  for (const auto& spline : authored.spline_segments()) {
    const auto* actual = unchanged->find_spline_segment(spline.id());
    REQUIRE(actual);
    CHECK(actual->curve().poles == spline.curve().poles);
    CHECK(actual->weights() == spline.weights());
  }
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("spline-aperture.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(directory.filePath("spline-aperture.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*reopened.part_shape_cache()->find_body_shape(part.bodies().front().id()))
            .volume_mm3 == Catch::Approx(volume));
}
