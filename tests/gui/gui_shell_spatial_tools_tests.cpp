#include "blcad/core/part_document_json.hpp"
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
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Command = ShellFeatureTools::Command;
std::string snapshot(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void fixture(ShellWindow& window, bool target = false) {
  REQUIRE(window.new_part(QStringLiteral("Sweep and Loft")));
  auto setup = window.session().commit_part_transaction(
      "Spatial inputs", [target](PartDocument& part) -> Result<std::size_t> {
        const auto checked = [](auto value) {
          REQUIRE(value);
          return value.value();
        };
        REQUIRE(part.add_parameter(checked(Parameter::create_length(
            ParameterId("width"), "Width", checked(Quantity::length_mm(4))))));
        REQUIRE(part.add_parameter(checked(Parameter::create_length(
            ParameterId("depth"), "Depth", checked(Quantity::length_mm(20))))));
        REQUIRE(part.add_parameter(checked(Parameter::create_length(
            ParameterId("base.width"), "Base width", checked(Quantity::length_mm(12))))));
        for (int z : {0, 10, 20}) {
          const std::string name = z == 0 ? "lower" : z == 10 ? "middle" : "upper";
          const DatumPlaneId plane(z == 0 ? "datum.xy" : "plane." + name);
          if (z != 0)
            REQUIRE(part.add_construction_plane(checked(ConstructionPlane::create_explicit(
                ConstructionPlaneId(plane.value()), name, {0, 0, static_cast<double>(z)}, {1, 0, 0},
                {0, 1, 0}, {0, 0, 1}))));
          auto sketch = checked(Sketch::create(SketchId(name), name, plane));
          REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
              ProfileId("profile." + name), ParameterId("width"), ParameterId("width"), {30, 0}))));
          REQUIRE(part.add_sketch(std::move(sketch)));
        }
        for (const auto& [id, point] :
             std::vector<std::pair<std::string, Point3>>{{"start", {30, 0, 0}},
                                                         {"end", {30, 0, 20}},
                                                         {"guide.start", {32, 0, 0}},
                                                         {"guide.end", {32, 0, 20}},
                                                         {"rail.start", {28, -2, 0}},
                                                         {"rail.end", {28, -2, 20}}})
          REQUIRE(part.add_construction_point(
              checked(ConstructionPoint::create_explicit(ConstructionPointId(id), id, point))));
        for (const std::string name : {"main", "guide", "rail"}) {
          const std::string prefix = name == "main" ? "" : name + ".";
          auto relation = checked(ConstructionRelation::create_line_through_two_points(
              ConstructionRelationId("relation." + name), ConstructionPointId(prefix + "start"),
              ConstructionPointId(prefix + "end")));
          REQUIRE(part.add_construction_line(checked(ConstructionLine::create_through_two_points(
              ConstructionLineId("line." + name), name, relation))));
          REQUIRE(part.add_path_curve(
              checked(PathCurve::create(PathCurveId("path." + name), name,
                                        {checked(PathSegmentReference::create_construction_line(
                                            ConstructionLineId("line." + name)))},
                                        PathClosure::Open, PathOrientationRule::MinimumTwist))));
        }
        for (int z : {0, 20}) {
          const std::string name = "open." + std::to_string(z);
          auto sketch = checked(Sketch::create(SketchId(name), name,
                                               DatumPlaneId(z == 0 ? "datum.xy" : "plane.upper")));
          REQUIRE(sketch.add_entity(
              checked(LineSegment::create(SketchEntityId("line"), {28, 0}, {32, 0}))));
          REQUIRE(part.add_sketch(std::move(sketch)));
          REQUIRE(part.add_path_curve(checked(PathCurve::create(
              PathCurveId("path." + name), name,
              {checked(PathSegmentReference::create_planar(SketchId(name), SketchEntityId("line"),
                                                           PlanarPathCurveKind::Line))},
              PathClosure::Open, PathOrientationRule::MinimumTwist))));
        }
        REQUIRE(part.add_construction_plane(checked(
            ConstructionPlane::create_explicit(ConstructionPlaneId("plane.chain"), "Chain plane",
                                               {30, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
        auto chain =
            checked(Sketch::create(SketchId("chain"), "chain", DatumPlaneId("plane.chain")));
        REQUIRE(
            chain.add_entity(checked(LineSegment::create(SketchEntityId("a"), {0, 0}, {0, 10}))));
        REQUIRE(
            chain.add_entity(checked(LineSegment::create(SketchEntityId("b"), {0, 20}, {0, 10}))));
        REQUIRE(part.add_sketch(std::move(chain)));
        if (target) {
          auto base = checked(Sketch::create(SketchId("base"), "Base", DatumPlaneId("datum.xy")));
          REQUIRE(base.add_profile(checked(RectangleProfile::create(
              ProfileId("base"), ParameterId("base.width"), ParameterId("base.width"), {30, 0}))));
          REQUIRE(part.add_sketch(std::move(base)));
          REQUIRE(
              part.add_body(checked(Body::create(BodyId("target"), "Target", BodyKind::Solid))));
          auto feature = checked(Feature::create_additive_extrude(
              FeatureId("feature.base"), "Base", SketchId("base"), ParameterId("depth")));
          auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                  std::nullopt, BodyId("target")));
          REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
        }
        return Result<std::size_t>::success(1);
      });
  if (!setup)
    INFO(setup.error().message());
  REQUIRE(setup);
  window.refresh();
  window.resize(1280, 960);
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
int source(ShellFeatureTools& tools, const QString& label) {
  const int index = tools.spatial_source_editor().findText(label, Qt::MatchStartsWith);
  REQUIRE(index >= 0);
  return tools.spatial_source_editor().itemData(index).toInt();
}
void combo(QComboBox& input, const char* id) {
  const int index = input.findData(QString::fromUtf8(id));
  REQUIRE(index >= 0);
  input.setCurrentIndex(index);
}
void capture(ShellWindow& window, const QString& name) {
  const auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(window.grab().save(QDir(dir).filePath(name + ".png")));
}
geometry::ShapeSummary result(ShellWindow& window, bool cut = false) {
  const auto& part = *window.session().part_document();
  const auto* shape = window.session().part_shape_cache()->find_body_shape(
      cut ? BodyId("target") : part.bodies().back().id());
  REQUIRE(shape);
  return geometry::RectangleExtrusionAdapter{}.summarize(*shape);
}
void history(ShellWindow& window, const std::string& before) {
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.save_to(dir.filePath("spatial.blcad.json")));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(dir.filePath("spatial.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*opened.part_document()).value() == after);
}
void add_sections(ShellFeatureTools& tools, bool surface = false, bool middle = false) {
  REQUIRE(tools.append_spatial_input(
      source(tools, surface ? QStringLiteral("open.0") : QStringLiteral("lower /"))));
  if (middle)
    REQUIRE(tools.append_spatial_input(source(tools, QStringLiteral("middle /"))));
  REQUIRE(tools.append_spatial_input(
      source(tools, surface ? QStringLiteral("open.20") : QStringLiteral("upper /"))));
}
} // namespace

TEST_CASE("Path creation previews connected ordered segments and persists reversal atomically",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  action(window, "path_curve").trigger();
  REQUIRE(tools.active());
  CHECK_FALSE(tools.apply());
  REQUIRE(tools.append_spatial_input(source(tools, QStringLiteral("chain / a"))));
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.append_spatial_input(source(tools, QStringLiteral("chain / b"))));
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK_FALSE(tools.apply());
  tools.spatial_order_editor().item(1)->setCheckState(Qt::Checked);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK_FALSE(tools.append_spatial_input(source(tools, QStringLiteral("chain / a"))));
  capture(window, QStringLiteral("spatial-path"));
  REQUIRE(tools.apply());
  const auto& path = window.session().part_document()->path_curves().back();
  REQUIRE(path.segments().size() == 2);
  CHECK(path.segments()[1].reversed());
  CHECK(window.viewport().selection_filter_mask() == mask);
  REQUIRE(window.viewport().select_semantic("path/" + path.id().value()));
  history(window, before);
}

TEST_CASE("Sweep and Loft author real solid cut and surface results through their task controls",
          "[gui][shell-spatial][block136-spatial]") {
  bool loft = false;
  int mode = 0;
  SECTION("solid sweep") {}
  SECTION("sweep cut") {
    mode = 1;
  }
  SECTION("surface sweep") {
    mode = 2;
  }
  SECTION("solid loft") {
    loft = true;
  }
  SECTION("loft cut") {
    loft = true;
    mode = 1;
  }
  SECTION("surface loft") {
    loft = true;
    mode = 2;
  }
  ShellWindow window;
  fixture(window, mode == 1);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto body_count = window.session().part_document()->bodies().size();
  action(window, loft ? "loft" : "sweep").trigger();
  REQUIRE(tools.active());
  tools.spatial_mode_editor().setCurrentIndex(mode);
  if (loft)
    add_sections(tools, mode == 2);
  else {
    tools.spatial_source_editor().setCurrentIndex(tools.spatial_source_editor().findData(
        source(tools, mode == 2 ? QStringLiteral("open.0") : QStringLiteral("lower /"))));
    combo(tools.spatial_path_editor(), "path.main");
  }
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().feature_preview_replaces("body/target") == (mode == 1));
  capture(window, QStringLiteral("spatial-%1-%2").arg(loft ? "loft" : "sweep").arg(mode));
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  REQUIRE(apply->isEnabled());
  apply->click();
  REQUIRE_FALSE(tools.active());
  CHECK(window.session().part_document()->bodies().size() == body_count + (mode == 1 ? 0 : 1));
  const auto summary = result(window, mode == 1);
  CHECK_FALSE(summary.is_null);
  CHECK(summary.solid_count == (mode == 2 ? 0U : 1U));
  if (mode != 2)
    CHECK(summary.volume_mm3 == Catch::Approx(mode == 1 ? 2560 : 320).margin(1e-5));
  history(window, before);
}

TEST_CASE("Loft reorders sections through the list model and retains guides and continuity",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_spatial(Command::Loft));
  add_sections(tools, false, true);
  REQUIRE(window.viewport().feature_preview_active());
  auto* model = tools.spatial_order_editor().model();
  REQUIRE(model->moveRows({}, 0, 1, {}, 3)); // middle, upper, lower
  CHECK(snapshot(window) == before);
  REQUIRE(tools.move_spatial_input(0, 1)); // upper, middle, lower
  combo(tools.spatial_path_editor(), "path.main");
  // A reversed station order also requires a reversed center path; clear it to preview the
  // reordered sections.
  tools.spatial_path_editor().setCurrentIndex(0);
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto& sections = window.session().part_document()->loft_features().back().sections();
  CHECK(std::get<ProfileRegionReference>(sections.front().source()).sketch() == SketchId("upper"));
  CHECK(std::get<ProfileRegionReference>(sections.back().source()).sketch() == SketchId("lower"));
  history(window, before);
}

TEST_CASE("Sweep twist and guide conflicts and unsupported Loft continuity fail without artifacts",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  const auto undo = std::string(window.session().undo_label());
  SECTION("twist shares numeric validation and rolls back invalid units") {
    REQUIRE(tools.begin_spatial(Command::Sweep));
    combo(tools.spatial_path_editor(), "path.main");
    REQUIRE(tools.set_numeric_text(QStringLiteral("30 deg")));
    CHECK_FALSE(tools.set_numeric_text(QStringLiteral("5 mm")));
    CHECK(tools.coordinator().path_sweep().twist() == 30);
    combo(tools.spatial_guide_editor(), "path.guide");
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(tools.apply());
    tools.sweep_twist_editor().setChecked(false);
    REQUIRE(window.viewport().feature_preview_active());
  }
  SECTION("G2 cannot silently degrade to G1") {
    REQUIRE(tools.begin_spatial(Command::Loft));
    add_sections(tools);
    tools.spatial_continuity_editor().setCurrentIndex(2);
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(tools.apply());
    tools.spatial_continuity_editor().setCurrentIndex(0);
    REQUIRE(window.viewport().feature_preview_active());
  }
  SECTION("closed path must actually close") {
    REQUIRE(tools.begin_spatial(Command::PathCurve));
    REQUIRE(tools.append_spatial_input(source(tools, QStringLiteral("chain / a"))));
    tools.spatial_closed_editor().setChecked(true);
    CHECK_FALSE(window.viewport().feature_preview_active());
    CHECK_FALSE(tools.apply());
  }
  CHECK(snapshot(window) == before);
  tools.cancel();
  CHECK(snapshot(window) == before);
  CHECK(window.session().undo_label() == undo);
  CHECK_FALSE(window.viewport().feature_preview_active());
}

TEST_CASE("Loft center path and ordered boundary guides commit with verified G1 continuity",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  const auto before = snapshot(window);
  REQUIRE(tools.begin_spatial(Command::Loft));
  add_sections(tools, false, true);
  combo(tools.spatial_path_editor(), "path.main");
  combo(tools.spatial_guide_editor(), "path.rail");
  tools.spatial_add_guide_button().click();
  tools.spatial_continuity_editor().setCurrentIndex(1);
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto& feature = window.session().part_document()->loft_features().back();
  CHECK(feature.path_curve() == PathCurveId("path.main"));
  REQUIRE(feature.guide_curves().size() == 1);
  CHECK(feature.guide_curves().front() == PathCurveId("path.rail"));
  CHECK(feature.continuity() == LoftContinuity::G1);
  CHECK(result(window).volume_mm3 == Catch::Approx(320).margin(1e-5));
  history(window, before);
}

TEST_CASE("GUI-fitted closed splines remain usable as Sweep profiles",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::Spline));
  for (Point2 point :
       {Point2{28, -2}, Point2{32, -2}, Point2{32, 2}, Point2{28, 2}, Point2{28, -2}})
    REQUIRE(window.sketch_tools().add_pick(point));
  REQUIRE(window.sketch_tools().finish_tool());
  REQUIRE(window.finish_active_sketch());
  auto& tools = window.feature_tools();
  const auto selected = tools.selected_profile();
  REQUIRE(selected);
  const auto before = snapshot(window);
  REQUIRE(tools.begin_spatial(Command::Sweep));
  combo(tools.spatial_path_editor(), "path.main");
  REQUIRE(window.viewport().feature_preview_active());
  REQUIRE(tools.apply());
  const auto& source = std::get<ProfileRegionReference>(
      window.session().part_document()->sweep_features().back().profile().source());
  CHECK(source.sketch() == selected->first);
  CHECK(source.profile() == selected->second);
  CHECK(result(window).solid_count == 1);
  CHECK(result(window).volume_mm3 > 0);
  history(window, before);
}

TEST_CASE("Spatial commands reject empty contexts and colliding pending Bodies",
          "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Empty")));
  const auto before = snapshot(window);
  for (const char* name : {"path_curve", "sweep", "loft"})
    CHECK_FALSE(action(window, name).isEnabled());
  CHECK_FALSE(window.feature_tools().begin_spatial(Command::Sweep));
  CHECK_FALSE(window.session().task().active());
  CHECK(snapshot(window) == before);
  ShellWindow populated_window;
  fixture(populated_window, true);
  const auto populated = snapshot(populated_window);
  GuiInteractiveLoftController controller;
  const auto body = populated_window.session().part_document()->bodies().front();
  CHECK_FALSE(controller.begin_loft(populated_window.session(), FeatureId("collision"), "Collision",
                                    LoftFeatureKind::Loft, body.id(), body));
  CHECK_FALSE(controller.active());
  CHECK(snapshot(populated_window) == populated);
}

TEST_CASE("Native spatial tools pick profiles and paths and refine a Sweep twist drag",
          "[gui][shell-spatial][block136-spatial][native-shell-spatial]") {
  if (QApplication::platformName() != QStringLiteral("xcb"))
    SKIP("Requires native OCCT spatial picking");
  ShellWindow window;
  fixture(window);
  auto& tools = window.feature_tools();
  bool loft = false;
  SECTION("Sweep profile trajectory and twist") {}
  SECTION("Loft ordered profile clicks") {
    loft = true;
  }
  REQUIRE(tools.begin_spatial(loft ? Command::Loft : Command::Sweep));
  QEventLoop frame;
  QTimer::singleShot(40, &frame, &QEventLoop::quit);
  frame.exec();
  const auto send = [&](QEvent::Type type, GuiSketchScreenPoint point, Qt::MouseButtons buttons) {
    const QPointF local(point.x, point.y);
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  };
  const auto click = [&](Point3 point) {
    const auto screen = window.viewport().model_to_screen(point);
    REQUIRE(screen);
    send(QEvent::MouseButtonPress, screen.value(), Qt::LeftButton);
    send(QEvent::MouseButtonRelease, screen.value(), Qt::NoButton);
  };
  const auto before = snapshot(window);
  if (loft) {
    click({29, -1, 0});
    click({29, -1, 20});
    REQUIRE(tools.spatial_order_editor().count() == 2);
    REQUIRE(window.viewport().feature_preview_active());
  } else {
    tools.spatial_source_editor().setCurrentIndex(
        tools.spatial_source_editor().findData(source(tools, QStringLiteral("upper /"))));
    click({29, -1, 0});
    CHECK(tools.spatial_source_editor().currentText().startsWith("lower /"));
    combo(tools.spatial_path_editor(), "path.guide");
    tools.spatial_pick_editor().setCurrentIndex(1);
    click({30, 0, 6});
    REQUIRE(tools.spatial_path_editor().currentData().toString() == "path.main");
    REQUIRE(window.viewport().feature_preview_active());
    const auto presentations = tools.manipulators().presentations();
    REQUIRE(presentations.size() == 1);
    const auto endpoint = presentations.front().endpoint;
    send(QEvent::MouseButtonPress, endpoint, Qt::LeftButton);
    REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
    REQUIRE(tools.set_numeric_text(QStringLiteral("30 deg")));
    send(QEvent::MouseMove, {endpoint.x + 5, endpoint.y}, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, {endpoint.x + 5, endpoint.y}, Qt::NoButton);
    CHECK(tools.coordinator().path_sweep().twist() == 30);
  }
  CHECK(snapshot(window) == before);
  capture(window, loft ? QStringLiteral("native-loft") : QStringLiteral("native-sweep"));
  auto* apply = window.findChild<QPushButton*>("blcad.feature.apply");
  REQUIRE(apply);
  REQUIRE(apply->isVisible());
  REQUIRE(window.childAt(apply->mapTo(&window, apply->rect().center())) == apply);
  auto* inputs = window.findChild<QScrollArea*>("blcad.feature.inputs_scroll");
  REQUIRE(inputs);
  CHECK_FALSE(inputs->horizontalScrollBar()->isVisible());
  apply->click();
  REQUIRE_FALSE(tools.active());
}

TEST_CASE(
    "Spatial pick roles exclude overlapping foreign owners and restore the prior selection scope",
    "[gui][shell-spatial][block136-spatial]") {
  ShellWindow window;
  fixture(window);
  const auto before = snapshot(window);
  window.viewport().set_selection_semantic_scope(std::vector<std::string>{"datum-plane/datum.xy"});
  const auto scope = window.viewport().selection_semantic_scope();
  const auto mask = window.viewport().selection_filter_mask();
  auto& tools = window.feature_tools();
  REQUIRE(tools.begin_spatial(Command::Sweep));
  combo(tools.spatial_path_editor(), "path.guide");
  tools.spatial_pick_editor().setCurrentIndex(1);
  CHECK_FALSE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.viewport().select_semantic("sketch/lower/profile/profile.lower"));
  REQUIRE(window.viewport().select_semantic("path/path.main"));
  CHECK(tools.spatial_path_editor().currentData().toString() == "path.main");
  REQUIRE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
  tools.cancel();
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(window.viewport().selection_filter_mask() == mask);
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  CHECK_FALSE(window.viewport().select_semantic("path/path.main"));
  CHECK(snapshot(window) == before);
}
