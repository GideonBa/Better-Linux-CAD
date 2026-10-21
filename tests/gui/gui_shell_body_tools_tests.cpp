#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellFeatureTools::Command;
std::string snapshot(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void box(ShellWindow& window, Point2 origin) {
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(window.sketch_tools().add_pick(origin));
  REQUIRE(window.sketch_tools().add_pick({origin.x + 8, origin.y + 8}));
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(8));
  REQUIRE(window.feature_tools().apply());
}
void bodies(ShellWindow& window, bool overlap = true) {
  REQUIRE(window.new_part(QStringLiteral("Body operations")));
  box(window, {24, -4});
  box(window, overlap ? Point2{28, -4} : Point2{24, 20});
  window.resize(1280, 900);
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  qApp->processEvents();
}
QAction& action(ShellWindow& window, const char* id) {
  auto* found = window.feature_tools().findChild<QAction*>(QStringLiteral("blcad.action.") + id);
  REQUIRE(found);
  return *found;
}
void pick(ShellWindow& window, const char* id) {
  REQUIRE(window.viewport().select_semantic(std::string("body/") + id));
  qApp->processEvents();
}
const geometry::GeometryShape& shape(ShellWindow& window, const BodyId& body) {
  const auto* found = window.session().part_shape_cache()->find_body_shape(body);
  REQUIRE(found);
  return *found;
}
void capture(ShellWindow& window, const QString& name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir().mkpath(directory));
  CHECK(window.grab().save(QDir(directory).filePath(name + ".png")));
}
void history_and_save(ShellWindow& window, const std::string& before) {
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(window.save_to(directory.filePath("body.blcad.json")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(directory.filePath("body.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}
} // namespace

TEST_CASE("Boolean ribbon commits real Add Cut Common results atomically in both result modes",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window);
  int operation = 0;
  double volume = 768;
  bool new_body = false, keep = false;
  SECTION("Add modify target consume tools") {}
  SECTION("Subtract modify target keep tools") {
    operation = 1;
    volume = 256;
    keep = true;
  }
  SECTION("Intersect new body consume tools") {
    operation = 2;
    volume = 256;
    new_body = true;
  }
  SECTION("Add new body keep tools") {
    new_body = true;
    keep = true;
  }
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  REQUIRE(action(window, "body_boolean").isEnabled());
  action(window, "body_boolean").trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.apply());
  tools.boolean_operation_editor().setCurrentIndex(operation);
  tools.boolean_result_editor().setCurrentIndex(new_body ? 1 : 0);
  tools.boolean_keep_tools_editor().setChecked(keep);
  pick(window, "body.2");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(window.viewport().feature_preview_replaces("body/body.1") == !new_body);
  CHECK(window.viewport().feature_preview_replaces("body/body.2") == !keep);
  CHECK(snapshot(window) == before);
  capture(window, QStringLiteral("body-boolean-%1-%2").arg(operation).arg(new_body));
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  REQUIRE(apply->isEnabled());
  apply->click();
  REQUIRE_FALSE(tools.active());
  const auto& part = *window.session().part_document();
  REQUIRE(part.body_boolean_features().size() == 1);
  const auto& feature = part.body_boolean_features().front();
  CHECK(feature.operation() == static_cast<BodyBooleanOperation>(operation));
  CHECK(feature.keep_tool_bodies() == keep);
  CHECK(part.bodies().size() == (new_body ? 3 : 2));
  CHECK((window.session().part_shape_cache()->find_body_shape(BodyId("body.2")) != nullptr) ==
        keep);
  const BodyId result = new_body ? *feature.produced_body() : feature.target_body();
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(shape(window, result)).volume_mm3 ==
        Catch::Approx(volume));
  CHECK(window.viewport().selection_filter_mask() == mask);
  history_and_save(window, before);
}

TEST_CASE("Body translation rotation and scaling share a multi-body preview and one undo entry",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  Command command = Command::BodyTranslate;
  const char* id = "body_translate";
  SECTION("translate XYZ") {}
  SECTION("rotate about Z") {
    command = Command::BodyRotate;
    id = "body_rotate";
  }
  SECTION("scale about explicit center") {
    command = Command::BodyScale;
    id = "body_scale";
  }
  const auto before = snapshot(window);
  auto& tools = window.feature_tools();
  REQUIRE(action(window, id).isEnabled());
  action(window, id).trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.apply());
  pick(window, "body.1");
  pick(window, "body.2");
  REQUIRE(tools.body_source_editor().selectedItems().size() == 2);
  if (command == Command::BodyTranslate) {
    REQUIRE(tools.set_numeric_text(QStringLiteral("10 mm")));
    REQUIRE(tools.set_translation_text(1, QStringLiteral("-3 mm")));
    REQUIRE(tools.set_translation_text(2, QStringLiteral("5 mm")));
  } else if (command == Command::BodyRotate) {
    REQUIRE(tools.set_numeric_text(QStringLiteral("90 deg")));
  } else {
    REQUIRE(tools.set_scale_center_text(0, QStringLiteral("28 mm")));
    REQUIRE(tools.set_scale_center_text(2, QStringLiteral("4 mm")));
    REQUIRE(tools.set_numeric_text(QStringLiteral("2")));
  }
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(window.viewport().feature_preview_count() == 2);
  CHECK(window.viewport().feature_preview_replaces("body/body.1"));
  CHECK(window.viewport().feature_preview_replaces("body/body.2"));
  CHECK(snapshot(window) == before);
  capture(window, QStringLiteral("body-%1-preview").arg(id));
  REQUIRE(tools.apply());
  const auto& part = *window.session().part_document();
  REQUIRE(part.body_transforms().size() == 2);
  for (const auto& transform : part.body_transforms()) {
    const auto& result = shape(window, transform.body());
    const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(result);
    CHECK(summary.volume_mm3 == Catch::Approx(command == Command::BodyScale ? 4096 : 512));
    const auto bounds = geometry::BodyTransformAdapter{}.bounds(result);
    REQUIRE(bounds);
    if (command == Command::BodyTranslate) {
      CHECK(transform.translation_mm().x == 10);
      CHECK(transform.translation_mm().y == -3);
      CHECK(transform.translation_mm().z == 5);
      CHECK(bounds.value().minimum.x == Catch::Approx(34).margin(1e-6));
    } else if (command == Command::BodyRotate) {
      CHECK(transform.angle_deg() == 90);
      CHECK(bounds.value().minimum.y == Catch::Approx(24).margin(1e-6));
    } else {
      CHECK(transform.scale_factor() == 2);
      CHECK(transform.scale_center().x == 28);
      CHECK(bounds.value().minimum.x == Catch::Approx(20).margin(1e-6));
    }
  }
  history_and_save(window, before);
}

TEST_CASE("Invalid body edits and empty Boolean results leave no document or cache mutation",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  auto& tools = window.feature_tools();
  SECTION("scale is positive and dimensionless") {
    REQUIRE(tools.begin_body_operation(Command::BodyScale));
    pick(window, "body.1");
    REQUIRE(tools.set_numeric_value(2));
    CHECK_FALSE(tools.set_numeric_text(QStringLiteral("3 mm")));
    CHECK_FALSE(tools.set_numeric_value(0));
    CHECK_FALSE(tools.set_numeric_value(-1));
    CHECK(tools.coordinator().body_operation().scale_factor() == 2);
    REQUIRE(window.viewport().feature_preview_active());
  }
  SECTION("translation rejects a wrong unit") {
    REQUIRE(tools.begin_body_operation(Command::BodyTranslate));
    pick(window, "body.1");
    REQUIRE(tools.set_translation_text(1, QStringLiteral("6 mm")));
    CHECK_FALSE(tools.set_translation_text(1, QStringLiteral("5 deg")));
    CHECK(tools.translation_editor(1).text().toDouble() == 6);
  }
  SECTION("empty intersection blocks Apply and clears the preview") {
    REQUIRE(tools.begin_body_operation(Command::BodyBoolean));
    tools.boolean_operation_editor().setCurrentIndex(2);
    tools.boolean_result_editor().setCurrentIndex(1);
    pick(window, "body.2");
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(tools.apply());
    CHECK(window.session().part_document()->bodies().size() == 2);
    tools.boolean_operation_editor().setCurrentIndex(0);
    REQUIRE(window.viewport().feature_preview_active());
    tools.body_source_editor().clearSelection();
    CHECK_FALSE(window.viewport().feature_preview_active());
  }
  CHECK(snapshot(window) == before);
  tools.cancel();
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(window.viewport().feature_preview_replaces("body/body.1"));
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
}

TEST_CASE("Changing Boolean target and rotation reference preserves unrelated picks",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  auto& tools = window.feature_tools();
  SECTION("Boolean target has an explicit model pick mode") {
    REQUIRE(tools.begin_body_operation(Command::BodyBoolean));
    tools.body_reference_button().click();
    pick(window, "body.2");
    CHECK(tools.boolean_target_editor().currentData().toString() == "body.2");
    CHECK_FALSE(tools.body_reference_button().isChecked());
    pick(window, "body.1");
    REQUIRE(tools.apply());
    CHECK(window.session().part_document()->body_boolean_features().front().target_body() ==
          BodyId("body.2"));
  }
  SECTION("rotation reference suspends the grips and returns to Body selection") {
    REQUIRE(tools.begin_body_operation(Command::BodyRotate));
    pick(window, "body.1");
    tools.body_reference_button().click();
    CHECK(window.viewport().selection_filter_mask() == selection_kind_bit(GuiSelectionKind::Datum));
    REQUIRE(window.viewport().select_semantic("datum-axis/datum.axis.x"));
    CHECK(tools.body_source_editor().selectedItems().size() == 1);
    CHECK_FALSE(tools.body_reference_button().isChecked());
    REQUIRE(tools.set_numeric_value(30));
    REQUIRE(tools.apply());
    const auto& axis = *window.session().part_document()->body_transforms().back().rotation_axis();
    CHECK(axis.datum_axis() == DatumAxisId("datum.axis.x"));
  }
}

TEST_CASE("Body transform stack and ownership flags remain explicit across repeated commands",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  auto& tools = window.feature_tools();
  REQUIRE(
      window.session().commit_part_transaction("Associate source sketch", [](PartDocument& part) {
        return part.add_sketch_ownership(SketchOwnership::create(part.sketches().front().id(),
                                                                 BodyId("body.1"),
                                                                 SketchAssociation::DrivesBody)
                                             .value());
      }));
  REQUIRE(tools.begin_body_operation(Command::BodyTranslate));
  pick(window, "body.1");
  tools.body_owned_sketches_editor().setChecked(true);
  tools.body_owned_construction_editor().setChecked(true);
  REQUIRE(tools.set_numeric_value(10));
  REQUIRE(tools.apply());
  const auto& owned_sketch = window.session().part_document()->sketches().front();
  const auto owned_frame = geometry::WorkplaneResolver{}.resolve_for_sketch(
      *window.session().part_document(), owned_sketch, *window.session().part_shape_cache());
  REQUIRE(owned_frame);
  CHECK(owned_frame.value().origin.x == Catch::Approx(10));
  const auto before_rotation = snapshot(window);
  REQUIRE(tools.begin_body_operation(Command::BodyRotate));
  if (tools.body_source_editor().selectedItems().empty())
    pick(window, "body.1");
  REQUIRE(tools.set_numeric_value(90));
  REQUIRE(tools.apply());
  const auto& transforms = window.session().part_document()->body_transforms();
  REQUIRE(transforms.size() == 2);
  CHECK(transforms[0].apply_to_owned_sketches());
  CHECK(transforms[0].apply_to_owned_construction_geometry());
  CHECK_FALSE(transforms[1].apply_to_owned_sketches());
  CHECK_FALSE(transforms[1].apply_to_owned_construction_geometry());
  const auto bounds = geometry::BodyTransformAdapter{}.bounds(shape(window, BodyId("body.1")));
  REQUIRE(bounds);
  CHECK(bounds.value().minimum.y == Catch::Approx(34).margin(1e-6));
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before_rotation);
}

TEST_CASE("Native Body transform grips support drag refinement and Escape rollback",
          "[gui][shell-body][block136-body][native-shell-body]") {
  if (QApplication::platformName() != QStringLiteral("xcb"))
    SKIP("Requires native OCCT Body picking");
  ShellWindow window;
  bodies(window, false);
  Command command = Command::BodyTranslate;
  const char* handle = "transform.translate.y";
  SECTION("translation Y") {}
  SECTION("rotation angle") {
    command = Command::BodyRotate;
    handle = "transform.angle";
  }
  SECTION("unitless scale factor") {
    command = Command::BodyScale;
    handle = "transform.scale";
  }
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_body_operation(command));
  QEventLoop frame;
  QTimer::singleShot(40, &frame, &QEventLoop::quit);
  frame.exec();
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point, Qt::MouseButtons buttons) {
    QPointF local(point.x, point.y);
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  };
  const auto source = window.viewport().model_to_screen({26, 2, 8});
  REQUIRE(source);
  send(QEvent::MouseButtonPress, source.value(), Qt::LeftButton);
  send(QEvent::MouseButtonRelease, source.value(), Qt::NoButton);
  REQUIRE(tools.body_source_editor().selectedItems().size() == 1);
  REQUIRE(window.viewport().feature_preview_active());
  const auto second = window.viewport().model_to_screen({26, 26, 8});
  REQUIRE(second);
  send(QEvent::MouseButtonPress, second.value(), Qt::LeftButton);
  send(QEvent::MouseButtonRelease, second.value(), Qt::NoButton);
  REQUIRE(tools.body_source_editor().selectedItems().size() == 2);
  CHECK(window.viewport().feature_preview_count() == 2);
  const auto before = snapshot(window);
  const auto presentations = tools.manipulators().presentations();
  auto found = std::find_if(presentations.begin(), presentations.end(),
                            [&](const auto& p) { return p.handle_id == handle; });
  REQUIRE(found != presentations.end());
  const auto endpoint = found->endpoint;
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE(tools.manipulators().active_handle_id() == handle);
  send(QEvent::MouseMove, {endpoint.x + 12, endpoint.y - 8}, Qt::LeftButton);
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &escape);
  REQUIRE(tools.manipulators().active_handle_id().empty());
  CHECK(tools.coordinator().body_operation().translation().y == 0);
  CHECK(tools.coordinator().body_operation().angle() == 0);
  if (command == Command::BodyScale)
    CHECK(tools.coordinator().body_operation().scale_factor() == 1);
  send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
  REQUIRE(tools.manipulators().active_handle_id() == handle);
  if (command == Command::BodyTranslate)
    REQUIRE(tools.set_translation_text(1, QStringLiteral("12 mm")));
  else
    REQUIRE(tools.set_numeric_text(command == Command::BodyRotate ? QStringLiteral("45 deg")
                                                                  : QStringLiteral("1.5")));
  send(QEvent::MouseMove, {endpoint.x + 5, endpoint.y}, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, {endpoint.x + 5, endpoint.y}, Qt::NoButton);
  CHECK(snapshot(window) == before);
  if (command == Command::BodyTranslate)
    CHECK(tools.coordinator().body_operation().translation().y == 12);
  else if (command == Command::BodyRotate)
    CHECK(tools.coordinator().body_operation().angle() == 45);
  else
    CHECK(tools.coordinator().body_operation().scale_factor() == 1.5);
  capture(window, QStringLiteral("native-body-%1").arg(static_cast<int>(command)));
  REQUIRE(tools.apply());
  CHECK(window.session().part_document()->body_transforms().size() == 2);
}

TEST_CASE("Body operation input collisions and late batch failure stay transactional",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  GuiInteractiveBodyOperationController controller;
  SECTION("new result collides with an existing Body") {
    const auto before = snapshot(window);
    const auto body = window.session().part_document()->bodies().front();
    CHECK_FALSE(controller.begin_boolean(window.session(), FeatureId("collision"),
                                         BodyBooleanOperation::Add, BodyId("body.2"),
                                         BodyBooleanResultMode::NewBody, body.id(), body));
    CHECK_FALSE(controller.active());
    CHECK(snapshot(window) == before);
  }
  SECTION("a missing second producer cannot commit the first transform") {
    REQUIRE(window.session().commit_part_transaction("Empty source record", [](PartDocument& part) {
      return part.add_body(Body::create(BodyId("empty"), "Empty", BodyKind::Solid).value());
    }));
    const auto before = snapshot(window);
    const auto undo = std::string(window.session().undo_label());
    REQUIRE(
        controller.begin_translate(window.session(), BodyTransformId("batch.1"), BodyId("body.1")));
    REQUIRE(controller.add_transform_body(window.session(), BodyTransformId("batch.2"),
                                          BodyId("empty")));
    controller.set_translation({10, 0, 0});
    CHECK_FALSE(controller.preview_shapes(window.session()));
    CHECK_FALSE(controller.apply(window.session()));
    CHECK(snapshot(window) == before);
    CHECK(window.session().undo_label() == undo);
    const auto bounds = geometry::BodyTransformAdapter{}.bounds(shape(window, BodyId("body.1")));
    REQUIRE(bounds);
    CHECK(bounds.value().minimum.x == Catch::Approx(24).margin(1e-6));
  }
}

TEST_CASE("Body operation actions reject unavailable source contexts",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Unavailable bodies")));
  bool one_body = false;
  SECTION("empty Part") {}
  SECTION("one Body only enables transforms") {
    one_body = true;
    box(window, {24, -4});
  }
  SECTION("hidden Bodies are unavailable") {
    box(window, {24, -4});
    REQUIRE(window.session().commit_part_transaction("Hide", [](PartDocument& part) {
      return part.set_body_visibility(BodyId("body.1"), BodyVisibility::Hidden);
    }));
    window.refresh();
  }
  CHECK_FALSE(action(window, "body_boolean").isEnabled());
  for (const char* id : {"body_translate", "body_rotate", "body_scale"})
    CHECK(action(window, id).isEnabled() == one_body);
  const auto before = snapshot(window);
  CHECK_FALSE(window.feature_tools().begin_body_operation(Command::BodyBoolean));
  CHECK_FALSE(window.feature_tools().active());
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Native Boolean role picking previews a cut before committing its result",
          "[gui][shell-body][block136-body][native-shell-body]") {
  if (QApplication::platformName() != QStringLiteral("xcb"))
    SKIP("Requires native OCCT Boolean role picking");
  ShellWindow window;
  bodies(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_body_operation(Command::BodyBoolean));
  tools.body_reference_button().click();
  QEventLoop frame;
  QTimer::singleShot(40, &frame, &QEventLoop::quit);
  frame.exec();
  const auto click = [&](Point3 point) {
    const auto screen = window.viewport().model_to_screen(point);
    REQUIRE(screen);
    const QPointF local(screen.value().x, screen.value().y);
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
      QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                        type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                        Qt::NoModifier);
      QApplication::sendEvent(&window.viewport(), &event);
      qApp->processEvents();
    }
  };
  click({34, 2, 8});
  REQUIRE(tools.boolean_target_editor().currentData().toString() == "body.2");
  REQUIRE_FALSE(tools.body_reference_button().isChecked());
  click({26, 2, 8});
  REQUIRE(tools.body_source_editor().selectedItems().size() == 1);
  tools.boolean_operation_editor().setCurrentIndex(1);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(window.viewport().feature_preview_replaces("body/body.1"));
  CHECK(window.viewport().feature_preview_replaces("body/body.2"));
  CHECK(snapshot(window) == before);
  capture(window, QStringLiteral("native-body-boolean-cut"));
  REQUIRE(tools.apply());
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(shape(window, BodyId("body.2"))).volume_mm3 ==
      Catch::Approx(256));
}

TEST_CASE("Repeated translation drags accumulate and scale drags use a dimensionless factor",
          "[gui][shell-body][block136-body]") {
  ShellWindow window;
  bodies(window, false);
  GuiInteractiveBodyOperationController controller;
  GuiViewportManipulatorLayer layer;
  GuiFeatureManipulatorFrame frame;
  frame.axis = {1, 0, 0};
  controller.set_manipulator_frame(frame);
  GuiViewportCameraBookmark camera;
  camera.eye = {0, 0, 100};
  camera.target = {};
  camera.up_direction = {0, 1, 0};
  camera.scale = 200;
  camera.projection = GuiViewportProjection::Orthographic;
  auto mapping = GuiViewportManipulatorMapping::create_camera(camera, 200, 200);
  REQUIRE(mapping);
  REQUIRE(layer.set_mapping(mapping.value()));
  SECTION("translation starts from its current component value on every drag") {
    REQUIRE(
        controller.begin_translate(window.session(), BodyTransformId("drag"), BodyId("body.1")));
    controller.set_translation({10, -3, 5});
    for (double expected : {20.0, 30.0}) {
      REQUIRE(layer.set_handles({controller.handles().front()}));
      REQUIRE(layer.begin_drag({140, 100}));
      const auto candidate = layer.end_drag({150, 100});
      REQUIRE(candidate);
      REQUIRE(controller.apply_manipulator(*candidate));
      CHECK(controller.translation().x == Catch::Approx(expected));
      CHECK(controller.translation().y == -3);
      CHECK(controller.translation().z == 5);
    }
  }
  SECTION("25 model millimeters change the scale factor by one without a unit suffix") {
    REQUIRE(controller.begin_uniform_scale(window.session(), BodyTransformId("scale"),
                                           BodyId("body.1")));
    REQUIRE(layer.set_handles(controller.handles()));
    REQUIRE(layer.begin_drag({140, 100}));
    auto candidate = layer.end_drag({165, 100});
    REQUIRE(candidate);
    REQUIRE(controller.apply_manipulator(*candidate));
    CHECK(controller.scale_factor() == Catch::Approx(2));
    REQUIRE(layer.set_handles(controller.handles()));
    REQUIRE(layer.begin_drag({140, 100}));
    CHECK_FALSE(layer.set_numeric_input("3 mm"));
    REQUIRE(layer.set_numeric_input("1.5"));
    candidate = layer.end_drag({170, 100});
    REQUIRE(candidate);
    REQUIRE(controller.apply_manipulator(*candidate));
    CHECK(controller.scale_factor() == Catch::Approx(1.5));
    CHECK(layer.numeric_text().find("mm") == std::string::npos);
  }
  REQUIRE(controller.apply(window.session()));
}
