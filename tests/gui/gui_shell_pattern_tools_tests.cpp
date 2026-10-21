#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cmath>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellFeatureTools::Command;
std::string snapshot(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void boxes(ShellWindow& window, bool second = false) {
  REQUIRE(window.new_part(QStringLiteral("Body patterns")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick({24, -4}));
  REQUIRE(window.sketch_tools().add_pick({32, 4}));
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(8));
  REQUIRE(window.feature_tools().apply());
  if (second) {
    REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
    REQUIRE(window.create_sketch_on_selection());
    REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
    REQUIRE(window.sketch_tools().add_pick({24, 20}));
    REQUIRE(window.sketch_tools().add_pick({32, 28}));
    REQUIRE(window.finish_active_sketch());
    REQUIRE(window.feature_tools().begin_extrude());
    REQUIRE(window.feature_tools().set_numeric_value(8));
    REQUIRE(window.feature_tools().apply());
  }
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  qApp->processEvents();
}
QAction& action(ShellWindow& window, const char* name) {
  auto* found = window.feature_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + name);
  REQUIRE(found);
  return *found;
}
void pick_body(ShellWindow& window, const char* id = "body.1") {
  REQUIRE(window.viewport().select_semantic(std::string("body/") + id));
  qApp->processEvents();
}
geometry::ShapeSummary result_summary(ShellWindow& window) {
  const auto* shape = window.session().part_shape_cache()->find_body_shape(
      window.session().part_document()->bodies().back().id());
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape);
}
void capture(ShellWindow& window, QString name) {
  const auto path = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (path.isEmpty())
    return;
  REQUIRE(QDir().mkpath(path));
  qApp->processEvents();
  CHECK(window.grab().save(QDir(path).filePath(name + ".png")));
}
} // namespace

TEST_CASE("Body pattern ribbon previews and commits real geometry with all new inputs in one "
          "history entry",
          "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  boxes(window);
  Command command = Command::LinearPattern;
  const char* id = "linear_pattern";
  std::size_t new_parameters = 2, solids = 3;
  SECTION("linear") {}
  SECTION("circular") {
    command = Command::CircularPattern;
    id = "circular_pattern";
    new_parameters = 1;
  }
  SECTION("mirror") {
    command = Command::BodyMirror;
    id = "body_mirror";
    new_parameters = 0;
    solids = 1;
  }
  const auto before = snapshot(window);
  const auto bodies = window.session().part_document()->bodies().size();
  const auto parameters = window.session().part_document()->parameters().size();
  const auto mask = window.viewport().selection_filter_mask();
  auto& tools = window.feature_tools();
  REQUIRE(action(window, id).isEnabled());
  action(window, id).trigger();
  REQUIRE(tools.active());
  CHECK(tools.command() == command);
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  CHECK_FALSE(apply->isEnabled());
  CHECK_FALSE(tools.apply());
  pick_body(window);
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(apply->isEnabled());
  CHECK(snapshot(window) == before);
  if (command == Command::LinearPattern)
    REQUIRE(tools.set_numeric_text(QStringLiteral("40 mm")));
  if (command == Command::CircularPattern)
    REQUIRE(tools.set_numeric_text(QStringLiteral("270 deg")));
  capture(window, QStringLiteral("body-") + id + QStringLiteral("-preview"));
  apply->click();
  REQUIRE_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.session().part_document()->bodies().size() == bodies + 1);
  CHECK(window.session().part_document()->parameters().size() == parameters + new_parameters);
  const auto summary = result_summary(window);
  CHECK(summary.solid_count == solids);
  CHECK(summary.volume_mm3 == Catch::Approx(512.0 * solids).margin(1e-5));
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("patterns.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(directory.filePath("patterns.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE(
    "Pattern modes and multiple Body sources retain exact scope and can be removed independently",
    "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  boxes(window, true);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  Command command = Command::LinearPattern;
  SECTION("linear total extent and negative direction") {}
  SECTION("circular multiple bodies") {
    command = Command::CircularPattern;
  }
  SECTION("mirror multiple bodies") {
    command = Command::BodyMirror;
  }
  REQUIRE(tools.begin_pattern(command));
  pick_body(window);
  pick_body(window, "body.2");
  REQUIRE(tools.pattern_source_editor().selectedItems().size() == 2);
  if (command == Command::LinearPattern) {
    tools.pattern_extent_editor().setCurrentIndex(1);
    tools.pattern_direction_editor().setCurrentIndex(1);
    REQUIRE(tools.set_numeric_value(80));
  }
  if (command != Command::BodyMirror)
    REQUIRE(tools.set_pattern_count(4));
  REQUIRE(window.viewport().feature_preview_active());
  pick_body(window, "body.2");
  CHECK(tools.pattern_source_editor().selectedItems().size() == 1);
  pick_body(window, "body.2");
  CHECK(tools.pattern_source_editor().selectedItems().size() == 2);
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto& part = *window.session().part_document();
  const auto summary = result_summary(window);
  CHECK(summary.solid_count == (command == Command::BodyMirror ? 2U : 8U));
  if (command == Command::LinearPattern) {
    REQUIRE(part.linear_pattern_features().size() == 1);
    const auto& pattern = part.linear_pattern_features().front();
    CHECK(pattern.sources().size() == 2);
    CHECK(pattern.extent_mode() == LinearPatternExtentMode::TotalExtent);
    CHECK(pattern.direction_sign() == PatternDirectionSign::Negative);
    CHECK(part.find_parameter(pattern.count_parameter())->value().count_value() == 4);
    CHECK(part.find_parameter(pattern.extent_parameter())->value().millimeters() == 80);
  }
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Pattern rejection restores numeric and count candidates while cancellation leaves no "
          "artifacts",
          "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  boxes(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto history = std::string(window.session().undo_label());
  REQUIRE(tools.begin_pattern(Command::LinearPattern));
  pick_body(window);
  CHECK_FALSE(tools.set_numeric_text(QStringLiteral("20 deg")));
  CHECK_FALSE(tools.set_numeric_value(-1));
  CHECK_FALSE(tools.set_pattern_count(1));
  CHECK_FALSE(tools.set_pattern_count(1001));
  GuiViewportManipulatorCandidate invalid;
  invalid.handle_id = std::string(kPatternCountHandleId);
  invalid.value_kind = GuiViewportManipulatorValueKind::Count;
  invalid.scalar_value = 1001;
  invalid.count_value = 1001;
  CHECK_FALSE(tools.accept_manipulator_candidate(invalid));
  CHECK(tools.coordinator().pattern_mirror().count() == 3);
  CHECK(tools.pattern_count_editor().value() == 3);
  CHECK(tools.coordinator().pattern_mirror().spacing() == Catch::Approx(40));
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&tools.numeric_editor(), &escape);
  CHECK_FALSE(tools.active());
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == history);
}

TEST_CASE("Pattern references can be picked without discarding the source selection",
          "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  boxes(window);
  auto& tools = window.feature_tools();
  const auto mask = window.viewport().selection_filter_mask();
  REQUIRE(tools.begin_pattern(Command::BodyMirror));
  pick_body(window);
  tools.pattern_reference_button().click();
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Datum));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK(tools.pattern_source_editor().selectedItems().size() == 1);
  CHECK_FALSE(tools.pattern_reference_button().isChecked());
  CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Body));
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto& reference =
      window.session().part_document()->mirror_features().front().mirror_plane();
  REQUIRE(std::holds_alternative<DatumPlaneId>(reference.source()));
  CHECK(std::get<DatumPlaneId>(reference.source()) == DatumPlaneId("datum.xy"));
  CHECK(window.viewport().selection_filter_mask() == mask);
}

TEST_CASE("Pending pattern inputs reject collisions before preview and leave the session unchanged",
          "[gui][shell-patterns][interactive-pattern-mirror][block136-patterns]") {
  ShellWindow window;
  boxes(window);
  const auto before = snapshot(window);
  const auto existing = window.session().part_document()->bodies().front();
  GuiInteractivePatternMirrorController controller;
  auto plane = PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                  DatumPlaneId("datum.yz"))
                   .value();
  CHECK_FALSE(controller.begin_mirror(window.session(), FeatureId("collision"), "Collision", plane,
                                      existing.id(), existing));
  CHECK_FALSE(controller.active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Native pattern source toggles remain selectable through a live preview and handles "
          "accept numeric refinement",
          "[gui][shell-patterns][native-shell-patterns][block136-patterns]") {
  if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
    SKIP("Requires native OCCT Body detection");
  ShellWindow window;
  boxes(window);
  REQUIRE(window.viewport().native_viewer_available());
  auto& tools = window.feature_tools();
  bool circular = false, count = false;
  SECTION("spacing handle") {}
  SECTION("angle handle") {
    circular = true;
  }
  SECTION("count handle") {
    count = true;
  }
  REQUIRE(tools.begin_pattern(circular ? Command::CircularPattern : Command::LinearPattern));
  qApp->processEvents();
  // Let Qt present the resized framebuffer before using its pixel coordinates.
  QEventLoop frame;
  QTimer::singleShot(40, &frame, &QEventLoop::quit);
  frame.exec();
  INFO("circular=" << circular << ", count=" << count);
  INFO("initial sources=" << tools.pattern_source_editor().selectedItems().size());
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint position,
                        Qt::MouseButtons buttons) {
    const QPointF local(position.x, position.y);
    QMouseEvent mouse(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &mouse);
    qApp->processEvents();
  };
  GuiSketchScreenPoint source_screen;
  const auto click_source = [&] {
    auto screen = window.viewport().model_to_screen({26, 2, 8});
    REQUIRE(screen);
    source_screen = screen.value();
    send(QEvent::MouseButtonPress, screen.value(), Qt::LeftButton);
    send(QEvent::MouseButtonRelease, screen.value(), Qt::NoButton);
  };
  click_source();
  INFO("screen=" << source_screen.x << "," << source_screen.y);
  INFO("viewport=" << window.viewport().width() << "," << window.viewport().height());
  INFO("selected=" << (window.viewport().selected_semantic()
                           ? window.viewport().selected_semantic()->semantic_id
                           : "none"));
  REQUIRE(tools.pattern_source_editor().selectedItems().size() == 1);
  REQUIRE(window.viewport().feature_preview_active());
  click_source();
  REQUIRE(tools.pattern_source_editor().selectedItems().empty());
  CHECK_FALSE(window.viewport().feature_preview_active());
  click_source();
  REQUIRE(tools.pattern_source_editor().selectedItems().size() == 1);
  const auto before = snapshot(window);
  const auto& presentations = tools.manipulators().presentations();
  const auto found = std::find_if(presentations.begin(), presentations.end(), [&](const auto& h) {
    return h.handle_id == (count      ? kPatternCountHandleId
                           : circular ? kPatternAngleHandleId
                                      : kPatternSpacingHandleId);
  });
  REQUIRE(found != presentations.end());
  const auto endpoint = found->endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  if (count)
    REQUIRE(tools.set_pattern_count(4));
  else
    REQUIRE(tools.set_numeric_text(circular ? QStringLiteral("180 deg") : QStringLiteral("35 mm")));
  send(QEvent::MouseMove, {endpoint.x + 6, endpoint.y}, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, {endpoint.x + 6, endpoint.y}, Qt::NoButton);
  if (count)
    CHECK(tools.coordinator().pattern_mirror().count() == 4);
  else
    CHECK(tools.numeric_editor().text().toDouble() == Catch::Approx(circular ? 180 : 35));
  CHECK(snapshot(window) == before);
  capture(window, count      ? QStringLiteral("native-pattern-count")
                  : circular ? QStringLiteral("native-pattern-angle")
                             : QStringLiteral("native-pattern-spacing"));
  REQUIRE(tools.apply());
}

TEST_CASE("Body patterns accept a closed spline extrusion without rectangular topology roles",
          "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Curved source")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::Spline));
  for (Point2 p : {Point2{24, -4}, Point2{32, -4}, Point2{32, 4}, Point2{24, 4}, Point2{24, -4}})
    REQUIRE(window.sketch_tools().add_pick(p));
  REQUIRE(window.sketch_tools().finish_tool());
  REQUIRE(window.finish_active_sketch());
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_extrude());
  REQUIRE(tools.set_numeric_value(8));
  REQUIRE(tools.apply());
  const auto source = result_summary(window);
  const auto before = snapshot(window);
  REQUIRE(tools.begin_pattern(Command::LinearPattern));
  pick_body(window);
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto result = result_summary(window);
  CHECK(result.solid_count == 3);
  CHECK(result.volume_mm3 == Catch::Approx(source.volume_mm3 * 3).margin(1e-5));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Pattern actions reject empty or hidden source contexts without creating pending records",
          "[gui][shell-patterns][block136-patterns]") {
  ShellWindow window;
  bool hidden = false;
  SECTION("empty Part") {
    REQUIRE(window.new_part(QStringLiteral("Empty")));
  }
  SECTION("hidden Body") {
    hidden = true;
    boxes(window);
    REQUIRE(window.session().commit_part_transaction("Hide source", [](PartDocument& part) {
      return part.set_body_visibility(BodyId("body.1"), BodyVisibility::Hidden);
    }));
    window.refresh();
  }
  const auto before = snapshot(window);
  for (const char* name : {"linear_pattern", "circular_pattern", "body_mirror"})
    CHECK_FALSE(action(window, name).isEnabled());
  CHECK_FALSE(window.feature_tools().begin_pattern(Command::LinearPattern));
  CHECK_FALSE(window.feature_tools().active());
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
  if (hidden)
    CHECK(window.session().part_document()->bodies().size() == 1);
}
