#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_dimension_authoring_json.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_profile_materialization.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::gui;
namespace {
using Kind = GuiSketchOperationKind;
void enter(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Operations")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Seed geometry", [id](PartDocument& part) -> Result<std::size_t> {
        Sketch sketch = *part.find_sketch(id);
        auto added =
            sketch.add_entity(LineSegment::create(SketchEntityId("line"), {0, 0}, {10, 0}).value());
        if (!added)
          return added;
        added = sketch.add_entity(SketchPoint::create(SketchEntityId("point"), {30, 40}).value());
        if (!added)
          return added;
        return part.update_sketch(std::move(sketch));
      }));
  window.refresh();
  window.sketch_tools().publish_scene();
}
std::string json(ShellWindow& window) {
  return serialize_part_document_to_json(*window.session().part_document()).value();
}
void select_line(ShellWindow& window, const std::string& id = "line") {
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/" + id}));
  window.sketch_tools().refresh_enablement();
}
} // namespace

TEST_CASE("Sketch operation previews cancel cleanly and every transform commits one history entry",
          "[gui][sketch-transform-pattern][sketch-offset-project]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  GuiSketchOperationRequest r;
  r.entities = {SketchEntityId("line")};
  Point2 expected_start{}, expected_end{};
  std::size_t expected_lines = 1;
  SECTION("move") {
    r.kind = Kind::Move;
    r.first = {3, 4};
    expected_start = {3, 4};
    expected_end = {13, 4};
  }
  SECTION("rotate") {
    r.kind = Kind::Rotate;
    r.value = std::numbers::pi / 2;
    expected_end = {0, 10};
  }
  SECTION("scale") {
    r.kind = Kind::Scale;
    r.value = 2;
    expected_end = {20, 0};
  }
  SECTION("copy") {
    r.kind = Kind::Copy;
    r.first = {0, 10};
    expected_lines = 2;
    expected_start = {0, 10};
    expected_end = {10, 10};
  }
  SECTION("mirror") {
    r.kind = Kind::Mirror;
    r.first = {0, 5};
    r.second = {1, 5};
    expected_lines = 2;
    expected_start = {0, 10};
    expected_end = {10, 10};
  }
  SECTION("rectangular pattern") {
    r.kind = Kind::RectangularPattern;
    r.first = {20, 0};
    r.second = {0, 10};
    r.first_count = 2;
    r.second_count = 2;
    expected_lines = 4;
    expected_start = {20, 10};
    expected_end = {30, 10};
  }
  SECTION("circular pattern") {
    r.kind = Kind::CircularPattern;
    r.first_count = 4;
    r.value = 2 * std::numbers::pi;
    expected_lines = 4;
    expected_end = {0, -10};
  }
  SECTION("offset") {
    r.kind = Kind::Offset;
    r.value = 3;
    expected_lines = 2;
    expected_start = {0, 3};
    expected_end = {10, 3};
  }
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("operation.blcad.json").toStdString()));
  const auto before = json(window);
  const auto old_label = std::string(window.session().undo_label());
  auto preview = window.sketch_tools().preview_operation(r);
  INFO((preview.has_error() ? preview.error().message() : ""));
  REQUIRE(preview);
  CHECK(window.viewport().sketch_operation_preview_count() ==
        (expected_lines == 1 ? 1 : expected_lines - 1));
  CHECK(json(window) == before);
  CHECK_FALSE(window.session().dirty());
  CHECK_FALSE(window.viewport().sketch_selection_enabled());
  window.sketch_tools().cancel_operation_preview();
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
  CHECK(window.viewport().sketch_selection_enabled());
  CHECK(json(window) == before);
  CHECK(window.session().undo_label() == old_label);
  auto applied = window.sketch_tools().apply_operation(r);
  INFO((applied.has_error() ? applied.error().message() : ""));
  REQUIRE(applied);
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
  CHECK(window.session().dirty());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch);
  REQUIRE(sketch->line_segments().size() == expected_lines);
  REQUIRE(sketch->find_point(SketchEntityId("point")));
  const auto& last = sketch->line_segments().back();
  CHECK(last.start().x == Catch::Approx(expected_start.x).margin(1e-8));
  CHECK(last.start().y == Catch::Approx(expected_start.y).margin(1e-8));
  CHECK(last.end().x == Catch::Approx(expected_end.x).margin(1e-8));
  CHECK(last.end().y == Catch::Approx(expected_end.y).margin(1e-8));
  const auto after = json(window);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  REQUIRE(window.session().redo());
  CHECK(json(window) == after);
  REQUIRE(window.session().save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("operation.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE("Sketch transforms respect fixed geometry constraints and driving dimensions atomically",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  GuiSketchOperationRequest r;
  r.entities = {SketchEntityId("line")};
  SECTION("fixed line rejects a translation") {
    REQUIRE(window.sketch_tools().apply_constraint(SketchSolverConstraintKind::Fixed));
    r.kind = Kind::Move;
    r.first = {2, 0};
  }
  SECTION("horizontal line rejects a quarter rotation") {
    REQUIRE(window.sketch_tools().apply_constraint(SketchSolverConstraintKind::Horizontal));
    r.kind = Kind::Rotate;
    r.value = std::numbers::pi / 2;
  }
  SECTION("driving length rejects scaling") {
    REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
    r.kind = Kind::Scale;
    r.value = 2;
  }
  const auto before = json(window);
  const auto label = std::string(window.session().undo_label());
  CHECK_FALSE(window.sketch_tools().preview_operation(r));
  CHECK_FALSE(window.sketch_tools().apply_operation(r));
  CHECK(json(window) == before);
  CHECK(window.session().undo_label() == label);
  CHECK(window.viewport().sketch_selection_enabled());
}

TEST_CASE("Sketch copies report catalog relationships and preserve exact curved profiles",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  GuiSketchOperationRequest copy;
  copy.kind = Kind::Copy;
  copy.entities = {SketchEntityId("line")};
  copy.first = {0, 20};
  auto preview = window.sketch_tools().preview_operation(copy);
  REQUIRE(preview);
  CHECK(preview.value().uncopied_references.size() == 1U);
  REQUIRE(window.sketch_tools().apply_operation(copy));
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Ellipse));
  REQUIRE(tools.add_pick({30, 0}));
  REQUIRE(tools.add_pick({50, 0}));
  REQUIRE(tools.add_pick({30, 10}));
  tools.cancel_tool();
  const auto* source = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(source->arc_closed_profiles().size() == 1U);
  const auto profile = source->arc_closed_profiles().front().id();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + profile.value()}));
  GuiSketchOperationRequest mirror;
  mirror.kind = Kind::Mirror;
  mirror.second = {0, 1};
  REQUIRE(tools.apply_operation(mirror));
  const auto* after = window.session().part_document()->find_sketch(*window.active_sketch());
  CHECK(after->arc_closed_profiles().size() == 2U);
  REQUIRE(after->spline_segments().size() == 8U);
  for (const auto& spline : after->spline_segments())
    CHECK(spline.rational());
  const auto mirrored_profile = after->arc_closed_profiles().back().id();
  const auto sketch_id = *window.active_sketch();
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().select_profile(sketch_id, mirrored_profile));
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  REQUIRE(window.feature_tools().apply());
  REQUIRE(window.session().part_shape_cache()->final_shape());
}

TEST_CASE("Associative projection and break link are previewed persisted and undoable",
          "[gui][sketch-project][sketch-offset-project]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Construction sources", [](PartDocument& part) -> Result<std::size_t> {
        auto added =
            part.add_construction_line(ConstructionLine::create_explicit(
                                           ConstructionLineId("axis"), "Axis", {0, 5, 0}, {1, 0, 0})
                                           .value());
        if (!added)
          return added;
        return part.add_construction_point(
            ConstructionPoint::create_explicit(ConstructionPointId("origin"), "Point", {2, 3, 0})
                .value());
      }));
  GuiSketchOperationRequest project;
  project.kind = Kind::ProjectLine;
  project.projected_line = ProjectedSketchLine::create_from_construction_line(
                               SketchEntityId("reference"), ConstructionLineId("axis"))
                               .value();
  const auto before = json(window);
  REQUIRE(window.sketch_tools().preview_operation(project));
  CHECK(json(window) == before);
  REQUIRE(window.sketch_tools().apply_operation(project));
  const auto projected = json(window);
  auto committed_scene = geometry::ViewportSceneBuilder{}.build_part(
      *window.session().part_document(), *window.session().part_shape_cache());
  REQUIRE(committed_scene);
  CHECK(std::any_of(committed_scene.value().begin(), committed_scene.value().end(),
                    [&](const auto& item) {
                      return item.semantic_id == "sketch/" + id.value() + "/entity/reference";
                    }));
  auto point = project;
  point.kind = Kind::ProjectPoint;
  point.projected_line.reset();
  point.projected_point = ProjectedSketchPoint::create_from_construction_point(
                              SketchEntityId("reference.point"), ConstructionPointId("origin"))
                              .value();
  REQUIRE(window.sketch_tools().apply_operation(point));
  REQUIRE(window.session().undo());
  CHECK(json(window) == projected);
  GuiSketchOperationRequest detach;
  detach.kind = Kind::BreakLinkLine;
  detach.entities = {SketchEntityId("reference")};
  detach.value = 40;
  REQUIRE(window.sketch_tools().apply_operation(detach));
  const auto* result = window.session().part_document()->find_sketch(id);
  CHECK(result->projected_lines().empty());
  const auto* line = result->find_line_segment(SketchEntityId("reference"));
  REQUIRE(line);
  CHECK(line->start() == Point2{-20, 5});
  CHECK(line->end() == Point2{20, 5});
  REQUIRE(result->find_point(SketchEntityId("point")));
  REQUIRE(window.session().undo());
  CHECK(json(window) == projected);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  project.projected_line = ProjectedSketchLine::create_from_construction_line(
                               SketchEntityId("missing"), ConstructionLineId("missing"))
                               .value();
  CHECK_FALSE(window.sketch_tools().preview_operation(project));
  CHECK_FALSE(window.sketch_tools().apply_operation(project));
  CHECK(json(window) == before);
}

TEST_CASE("Sketch operation ribbon drives live numeric preview Apply and Escape",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  auto* action = window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.move"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  bool accept = false;
  SECTION("apply") {
    accept = true;
  }
  SECTION("cancel") {}
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog != nullptr);
    if (!dialog)
      return;
    auto* x = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("sketch.operation.x"));
    CHECK(x != nullptr);
    if (!x) {
      dialog->reject();
      return;
    }
    x->setValue(25);
    CHECK(json(window) == before);
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(controls != nullptr);
    visited = true;
    if (accept && controls)
      controls->button(QDialogButtonBox::Ok)->click();
    else
      dialog->reject();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK_FALSE(window.session().task().active());
  CHECK(window.viewport().sketch_selection_enabled());
  if (accept) {
    const auto* line = window.session()
                           .part_document()
                           ->find_sketch(*window.active_sketch())
                           ->find_line_segment(SketchEntityId("line"));
    REQUIRE(line);
    CHECK(line->start() == Point2{25, 0});
    REQUIRE(window.session().undo());
  }
  CHECK(json(window) == before);
}

TEST_CASE("Sketch numeric operation editor removes invalid previews and permits correction",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.scale"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog);
    if (!dialog)
      return;
    auto* factor = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("sketch.operation.value"));
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(factor);
    CHECK(controls);
    if (!factor || !controls) {
      dialog->reject();
      return;
    }
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    factor->setValue(0);
    CHECK_FALSE(controls->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(window.viewport().sketch_operation_preview_count() == 0U);
    CHECK(json(window) == before);
    factor->setValue(3);
    CHECK(controls->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK(window.session()
            .part_document()
            ->find_sketch(*window.active_sketch())
            ->find_line_segment(SketchEntityId("line"))
            ->end() == Point2{30, 0});
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Sketch projection chooser applies an associative source and persists its identity",
          "[gui][sketch-project]") {
  ShellWindow window;
  enter(window);
  REQUIRE(window.session().commit_part_transaction("Construction line", [](PartDocument& part) {
    return part.add_construction_line(ConstructionLine::create_explicit(ConstructionLineId("axis"),
                                                                        "Projection axis",
                                                                        {0, 7, 0}, {1, 0, 0})
                                          .value());
  }));
  window.refresh();
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.project"));
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog);
    if (!dialog)
      return;
    auto* source = dialog->findChild<QComboBox*>(QStringLiteral("sketch.operation.source"));
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(source);
    CHECK(controls);
    if (!source || !controls) {
      dialog->reject();
      return;
    }
    CHECK(source->count() == 1);
    CHECK(json(window) == before);
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  action->trigger();
  REQUIRE(visited);
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch->projected_lines().size() == 1U);
  CHECK(sketch->projected_lines().front().construction_line() == ConstructionLineId("axis"));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("projected.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("projected.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == json(window));
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Sketch rigid transforms preserve compatible driving intent and reject stale selections",
          "[gui][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto sketch_id = *window.active_sketch();
  const auto catalog = window.session().sketch_dimension_catalog(sketch_id).value();
  const auto before = json(window);
  GuiSketchOperationRequest move;
  move.kind = Kind::Move;
  move.entities = {SketchEntityId("line")};
  move.first = {5, 7};
  auto result = window.sketch_tools().apply_operation(move);
  INFO((result.has_error() ? result.error().message() : ""));
  REQUIRE(result);
  CHECK(window.session()
            .part_document()
            ->find_sketch(sketch_id)
            ->find_line_segment(SketchEntityId("line"))
            ->start() == Point2{5, 7});
  CHECK(window.session().sketch_dimension_catalog(sketch_id).value().dimensions() ==
        catalog.dimensions());
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  move.entities = {SketchEntityId("missing")};
  CHECK_FALSE(window.sketch_tools().preview_operation(move));
  CHECK_FALSE(window.sketch_tools().apply_operation(move));
  CHECK(json(window) == before);
}

namespace {
void capture_ui(QWidget& widget, const QString& name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  QDir().mkpath(directory);
  QEventLoop frame;
  QTimer::singleShot(150, &frame, &QEventLoop::quit);
  frame.exec();
  const auto capture = widget.grab();
  CHECK(capture.save(directory + '/' + name + ".png"));
}
} // namespace

TEST_CASE("Sketch ribbon remains usable in a normal desktop window",
          "[gui][sketch-ui-review][ribbon]") {
  ShellWindow window;
  enter(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  capture_ui(window, "sketch-ribbon");
  CAPTURE(window.width(), window.minimumSizeHint().width(),
          window.ribbon().minimumSizeHint().width());
  CHECK(window.width() <= 1280);
  CHECK(window.viewport().width() >= 600);
  if (QApplication::platformName() == QStringLiteral("xcb"))
    CHECK(window.viewport().native_viewer_available());
  auto* page = window.ribbon().findChild<QScrollArea*>(QStringLiteral("ribbon.page.1"));
  REQUIRE(page);
  CHECK(page->horizontalScrollBar()->maximum() > 0);
  page->horizontalScrollBar()->setValue(page->horizontalScrollBar()->maximum());
  qApp->processEvents();
  capture_ui(window, "sketch-ribbon-right");
  for (auto* button : page->findChildren<QToolButton*>()) {
    if (button->text() != QStringLiteral("Snap"))
      continue;
    CHECK(page->viewport()->rect().contains(
        QRect(button->mapTo(page->viewport(), QPoint{}), button->size())));
  }
}

TEST_CASE("Sketch operation buttons reject unsupported and mixed selections",
          "[gui][sketch-ui-review][sketch-transform-pattern]") {
  ShellWindow window;
  enter(window);
  auto* move = window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.move"));
  REQUIRE(move);
  select_line(window, "point");
  CHECK_FALSE(move->isEnabled());
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction(
      "Unsupported circle selection", [id](PartDocument& part) -> Result<std::size_t> {
        auto p = Parameter::create_length(ParameterId("diameter"), "Diameter",
                                          Quantity::length_mm(20).value());
        if (!p)
          return Result<std::size_t>::failure(p.error());
        auto added = part.add_parameter(p.value());
        if (!added)
          return added;
        auto sketch = *part.find_sketch(id);
        auto profile = CircleProfile::create(ProfileId("circle"), ParameterId("diameter"), {30, 0});
        if (!profile)
          return Result<std::size_t>::failure(profile.error());
        added = sketch.add_profile(profile.value());
        if (!added)
          return added;
        return part.update_sketch(std::move(sketch));
      }));
  select_line(window);
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "profile/circle"}));
  window.sketch_tools().refresh_enablement();
  CHECK_FALSE(move->isEnabled());
  const auto before = json(window);
  GuiSketchOperationRequest request;
  request.kind = Kind::Move;
  request.first = {5, 0};
  CHECK_FALSE(window.sketch_tools().apply_operation(request));
  CHECK(json(window) == before);
}

namespace {
void click_widget(QWidget& widget, QPointF local) {
  const QPointF global(widget.mapToGlobal(local.toPoint()));
  QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&widget, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
  QApplication::sendEvent(&widget, &release);
  qApp->processEvents();
}
void escape_dialog(QDialog& dialog) {
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&dialog, &escape);
}
} // namespace

TEST_CASE("Visible Sketch ribbon buttons complete every numeric operation and restore the viewport",
          "[gui][sketch-ui-review][sketch-transform-pattern][sketch-offset-project]") {
  ShellWindow window;
  enter(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  const auto sketch_id = *window.active_sketch();
  auto point = window.viewport().sketch_plane_to_screen({5, 0});
  REQUIRE(point);
  click_widget(window.viewport(), {point.value().x, point.value().y});
  REQUIRE_FALSE(window.session().selection().empty());
  const char* command = "move";
  bool cancel = false;
  std::size_t expected_lines = 1;
  SECTION("move") {}
  SECTION("rotate") {
    command = "rotate";
  }
  SECTION("scale") {
    command = "scale";
  }
  SECTION("copy") {
    command = "copy";
    expected_lines = 2;
  }
  SECTION("mirror") {
    command = "mirror";
    expected_lines = 2;
  }
  SECTION("rectangular pattern") {
    command = "rectangular_pattern";
    expected_lines = 6;
  }
  SECTION("circular pattern") {
    command = "circular_pattern";
    expected_lines = 3;
  }
  SECTION("offset") {
    command = "offset";
    expected_lines = 2;
  }
  SECTION("escape cancels preview") {
    cancel = true;
  }
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.") + command);
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  QToolButton* button = nullptr;
  for (auto* candidate : window.ribbon().findChildren<QToolButton*>())
    if (candidate->defaultAction() == action)
      button = candidate;
  REQUIRE(button);
  auto* page = window.ribbon().findChild<QScrollArea*>(QStringLiteral("ribbon.page.1"));
  REQUIRE(page);
  page->ensureWidgetVisible(button);
  qApp->processEvents();
  REQUIRE(page->viewport()->rect().contains(
      QRect(button->mapTo(page->viewport(), QPoint{}), button->size())));
  const auto before = json(window);
  bool visited = false;
  // Bound modal failures so a broken Apply button cannot hang the suite.
  QTimer::singleShot(3000, &window, [&] {
    if (auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"))) {
      CHECK_FALSE("operation dialog failed to close");
      dialog->reject();
    }
  });
  QTimer::singleShot(50, &window, [&] {
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sketch.operation.dialog"));
    CHECK(dialog);
    if (!dialog)
      return;
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    CHECK(controls);
    if (!controls) {
      dialog->reject();
      return;
    }
    CHECK(controls->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(window.viewport().sketch_operation_preview_count() > 0U);
    CHECK(json(window) == before);
    if (QApplication::platformName() == QStringLiteral("xcb")) {
      // Native decorations and final placement arrive asynchronously from the
      // window manager, especially while another GUI suite is rendering.
      QElapsedTimer placement;
      placement.start();
      while (!dialog->screen()->availableGeometry().contains(dialog->frameGeometry()) &&
             placement.elapsed() < 500) {
        QEventLoop frame;
        QTimer::singleShot(10, &frame, &QEventLoop::quit);
        frame.exec();
      }
      INFO("command=" << command << " frame=" << dialog->frameGeometry().x() << ','
                      << dialog->frameGeometry().y() << ' ' << dialog->frameGeometry().width()
                      << 'x' << dialog->frameGeometry().height());
      CHECK(dialog->screen()->availableGeometry().contains(dialog->frameGeometry()));
    } else {
      // The offscreen backend reports an 800x800 virtual screen even when the
      // synthetic parent is wider. Native xcb verifies actual placement.
      CHECK(dialog->width() <= dialog->screen()->availableGeometry().width());
      CHECK(dialog->height() <= dialog->screen()->availableGeometry().height());
    }
    capture_ui(window, QStringLiteral("preview-") + command);
    capture_ui(*dialog, QStringLiteral("dialog-") + command);
    visited = true;
    if (cancel)
      escape_dialog(*dialog);
    else
      click_widget(*controls->button(QDialogButtonBox::Ok),
                   controls->button(QDialogButtonBox::Ok)->rect().center());
  });
  click_widget(*button, button->rect().center());
  REQUIRE(visited);
  CHECK_FALSE(window.session().task().active());
  CHECK(window.viewport().sketch_selection_enabled());
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
  if (cancel)
    CHECK(json(window) == before);
  else {
    CHECK(window.session().part_document()->find_sketch(sketch_id)->line_segments().size() ==
          expected_lines);
    const auto after = json(window);
    REQUIRE(window.undo());
    CHECK(json(window) == before);
    REQUIRE(window.redo());
    CHECK(json(window) == after);
  }
  capture_ui(window, QStringLiteral("finished-") + command);
}

namespace {
void pointer_at(ShellWindow& window, Point2 point, QEvent::Type type) {
  auto screen = window.viewport().sketch_plane_to_screen(point);
  REQUIRE(screen);
  QPointF local(screen.value().x, screen.value().y);
  QMouseEvent event(type, local, QPointF(window.viewport().mapToGlobal(local.toPoint())),
                    type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                    type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                    Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &event);
}
void pick_at(ShellWindow& window, Point2 point) {
  pointer_at(window, point, QEvent::MouseButtonPress);
  pointer_at(window, point, QEvent::MouseButtonRelease);
}
void seed_pair_and_axis(ShellWindow& window) {
  const auto id = *window.active_sketch();
  REQUIRE(window.session().commit_part_transaction("Symmetry sources", [id](PartDocument& part) {
    auto sketch = *part.find_sketch(id);
    for (auto line : {LineSegment::create(SketchEntityId("axis"), {20, -30}, {20, 30},
                                          SketchLineRole::Centerline)
                          .value(),
                      LineSegment::create(SketchEntityId("left"), {10, 5}, {10, 15}).value(),
                      LineSegment::create(SketchEntityId("right"), {31, 5}, {32, 15}).value()}) {
      auto added = sketch.add_entity(line);
      if (!added)
        return added;
    }
    return part.update_sketch(std::move(sketch));
  }));
  window.sketch_tools().publish_scene();
}
} // namespace

TEST_CASE(
    "Transform editor accepts multiple picks and mouse drag without changing history until Apply",
    "[gui][sketch-transform-pattern][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  seed_pair_and_axis(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  window.session().selection().clear();
  window.sketch_tools().refresh_enablement();
  const auto before = json(window);
  auto* action = window.sketch_tools().findChild<QAction*>("sketch.operation.move");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  bool visited = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = window.findChild<QDialog*>("sketch.operation.dialog");
    REQUIRE(dialog);
    CHECK_FALSE(dialog->isModal());
    pick_at(window, {10, 10});
    pick_at(window, {31.5, 10});
    auto* drag = dialog->findChild<QPushButton*>("sketch.operation.drag");
    REQUIRE(drag);
    drag->click();
    pointer_at(window, {40, 40}, QEvent::MouseButtonPress);
    REQUIRE(window.viewport().sketch_snap_result());
    const auto start = window.viewport().sketch_snap_result()->snapped_point;
    pointer_at(window, {50, 50}, QEvent::MouseMove);
    pointer_at(window, {50, 50}, QEvent::MouseButtonRelease);
    REQUIRE(window.viewport().sketch_snap_result());
    const auto end = window.viewport().sketch_snap_result()->snapped_point;
    CHECK(json(window) == before);
    CHECK(window.viewport().sketch_operation_preview_count() == 2U);
    auto* x = dialog->findChild<QDoubleSpinBox*>("sketch.operation.x");
    auto* y = dialog->findChild<QDoubleSpinBox*>("sketch.operation.y");
    REQUIRE(x);
    REQUIRE(y);
    CHECK(x->value() == Catch::Approx(end.x - start.x).margin(1e-5));
    CHECK(y->value() == Catch::Approx(end.y - start.y).margin(1e-5));
    // Native rays are pixel-quantized. Refine the mouse placement with exact numeric input.
    x->setValue(10);
    y->setValue(10);
    CHECK(json(window) == before);
    CHECK(window.viewport().sketch_operation_preview_count() == 2U);
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(controls);
    REQUIRE(controls->button(QDialogButtonBox::Ok)->isEnabled());
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  action->trigger();
  REQUIRE(visited);
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  CHECK(sketch->find_line_segment(SketchEntityId("left"))->start() == Point2{20, 15});
  CHECK(sketch->find_line_segment(SketchEntityId("right"))->end() == Point2{42, 25});
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Mirror and symmetry editors pick a centerline then multiple elements",
          "[gui][sketch-transform-pattern][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  seed_pair_and_axis(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  bool symmetric = false;
  SECTION("mirror creates copies") {}
  SECTION("symmetry constrains existing elements") {
    symmetric = true;
  }
  QAction* action = nullptr;
  for (auto* candidate : window.sketch_tools().findChildren<QAction*>())
    if (symmetric ? candidate->text() == QStringLiteral("Symmetric")
                  : candidate->objectName() == QStringLiteral("sketch.operation.mirror"))
      action = candidate;
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = window.findChild<QDialog*>("sketch.operation.dialog");
    REQUIRE(dialog);
    pick_at(window, {20, 20});
    pick_at(window, {10, 10});
    pick_at(window, {31.5, 10});
    CHECK(json(window) == before);
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(controls);
    CHECK(controls->button(QDialogButtonBox::Ok)->isEnabled());
    visited = true;
    if (controls->button(QDialogButtonBox::Ok)->isEnabled())
      controls->button(QDialogButtonBox::Ok)->click();
    else
      dialog->reject();
  });
  action->trigger();
  REQUIRE(visited);
  const auto id = *window.active_sketch();
  const auto* sketch = window.session().part_document()->find_sketch(id);
  CHECK(sketch->find_line_segment(SketchEntityId("axis"))->role() == SketchLineRole::Centerline);
  if (symmetric) {
    auto catalog = window.session().sketch_constraint_catalog(id);
    REQUIRE(catalog);
    REQUIRE(catalog.value().constraints().size() == 2U);
    const auto* a = sketch->find_line_segment(SketchEntityId("left"));
    const auto* b = sketch->find_line_segment(SketchEntityId("right"));
    CHECK(a->start().x + b->start().x == Catch::Approx(40).margin(1e-7));
    CHECK(a->end().x + b->end().x == Catch::Approx(40).margin(1e-7));
  } else
    CHECK(sketch->line_segments().size() == 6U);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  if (symmetric)
    CHECK(window.session().sketch_constraint_catalog(id).value().constraints().empty());
}

TEST_CASE(
    "Sketch helper line roles and splines persist while helper geometry stays out of profiles",
    "[gui][sketch-create][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  const auto id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  for (auto tool : {GuiSketchCreateTool::Centerline, GuiSketchCreateTool::ConstructionLine}) {
    REQUIRE(tools.begin_tool(tool));
    const bool center = tool == GuiSketchCreateTool::Centerline;
    REQUIRE(tools.add_pick({-10, center ? -10.0 : -20.0}));
    REQUIRE(tools.add_pick({10, center ? 10.0 : -20.0}));
    tools.cancel_tool();
  }
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  for (Point2 p : {Point2{0, 10}, Point2{10, 20}, Point2{20, -10}, Point2{30, 10}})
    REQUIRE(tools.add_pick(p));
  REQUIRE(tools.finish_tool());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch->spline_segments().size() == 3U);
  CHECK_FALSE(sketch->spline_segments().front().rational());
  CHECK(sketch->line_segments().back().role() == SketchLineRole::Construction);
  auto topology = SketchTopology::migrate_legacy(*sketch);
  REQUIRE(topology);
  CHECK(topology.value()
            .find_entity("entity/" + sketch->line_segments().back().id().value())
            ->construction());
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*window.session().part_document(),
                                                           *window.session().part_shape_cache());
  REQUIRE(scene);
  CHECK(std::count_if(scene.value().begin(), scene.value().end(), [](const auto& i) {
          return i.line_role == SketchLineRole::Centerline ||
                 i.line_role == SketchLineRole::Construction;
        }) == 2);
  CHECK_FALSE(tools.findChild<QAction*>("sketch.create.elliptical_arc"));
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  capture_ui(window, "helper-line-styles");
  auto closed = *sketch;
  REQUIRE(
      closed.add_entity(LineSegment::create(SketchEntityId("close.1"), {10, 0}, {10, 20}).value()));
  REQUIRE(
      closed.add_entity(LineSegment::create(SketchEntityId("close.2"), {10, 20}, {0, 20}).value()));
  REQUIRE(
      closed.add_entity(LineSegment::create(SketchEntityId("close.3"), {0, 20}, {0, 0}).value()));
  auto region =
      geometry::SketchRegionFinder{}.find_single_region(*window.session().part_document(), closed);
  REQUIRE(region);
  CHECK(region.value().line_segments.size() == 4U);
  const auto helper_id = sketch->line_segments().back().id();
  select_line(window, helper_id.value());
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Fixed));
  CHECK(window.session()
            .part_document()
            ->find_sketch(id)
            ->find_line_segment(helper_id)
            ->construction());
  const auto saved = json(window);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("helpers.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("helpers.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == saved);
}

TEST_CASE("Split accepts line midpoint hits through the actual viewport",
          "[gui][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  const auto before = json(window);
  window.sketch_tools().begin_modify_tool(SketchModifyKind::Split);
  pick_at(window, {5, 0});
  window.sketch_tools().cancel_tool();
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(sketch->line_segments().size() == 2U);
  CHECK(sketch->find_line_segment(SketchEntityId("line"))->end() == Point2{5, 0});
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE(
    "Projection chooser includes other sketches and projects finite curves in plane coordinates",
    "[gui][sketch-project][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  REQUIRE(window.session().commit_part_transaction("Projection source", [](PartDocument& part) {
    auto source = Sketch::create(SketchId("source"), "Source", DatumPlaneId("datum.xy"));
    if (!source)
      return Result<std::size_t>::failure(source.error());
    auto added = source.value().add_entity(
        LineSegment::create(SketchEntityId("edge"), {3, 7}, {13, 7}).value());
    if (!added)
      return added;
    return part.add_sketch(std::move(source.value()));
  }));
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = window.findChild<QDialog*>("sketch.operation.dialog");
    REQUIRE(dialog);
    auto* source = dialog->findChild<QComboBox*>("sketch.operation.source");
    REQUIRE(source);
    REQUIRE(source->count() == 1);
    CHECK(window.viewport().sketch_operation_preview_count() == 1U);
    CHECK(json(window) == before);
    auto* controls = dialog->findChild<QDialogButtonBox*>();
    REQUIRE(controls);
    visited = true;
    controls->button(QDialogButtonBox::Ok)->click();
  });
  auto* action = window.sketch_tools().findChild<QAction*>("sketch.operation.project");
  REQUIRE(action);
  action->trigger();
  REQUIRE(visited);
  const auto* result = window.session().part_document()->find_sketch(*window.active_sketch());
  REQUIRE(result->line_segments().size() == 2U);
  CHECK(result->line_segments().back().start() == Point2{3, 7});
  CHECK(result->line_segments().back().end() == Point2{13, 7});
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Every numeric transform receives a live drag through the modeless editor",
          "[gui][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  const char* command = "copy";
  Point2 anchor{40, 40}, destination{50, 50};
  const char* field = "x";
  double expected = 10;
  SECTION("copy") {}
  SECTION("rotate") {
    command = "rotate";
    anchor = {40, 40};
    destination = {-40, 40};
    field = "value";
    expected = 90;
  }
  SECTION("scale") {
    command = "scale";
    anchor = {20, 20};
    destination = {40, 40};
    field = "value";
    expected = 2;
  }
  SECTION("rectangular pattern") {
    command = "rectangular_pattern";
  }
  SECTION("circular pattern") {
    command = "circular_pattern";
    anchor = {40, 40};
    destination = {-40, 40};
    field = "value";
    expected = 90;
  }
  SECTION("offset") {
    command = "offset";
    destination = {40, 50};
    field = "value";
    expected = 10;
  }
  auto* action =
      window.sketch_tools().findChild<QAction*>(QStringLiteral("sketch.operation.") + command);
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  bool visited = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = window.findChild<QDialog*>("sketch.operation.dialog");
    REQUIRE(dialog);
    pointer_at(window, anchor, QEvent::MouseButtonPress);
    pointer_at(window, destination, QEvent::MouseMove);
    pointer_at(window, destination, QEvent::MouseButtonRelease);
    auto* input = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("sketch.operation.") + field);
    REQUIRE(input);
    CHECK(input->value() == Catch::Approx(expected).margin(1e-5));
    CHECK(window.viewport().sketch_operation_preview_count() > 0U);
    CHECK(json(window) == before);
    pointer_at(window, {45, 50}, QEvent::MouseMove);
    CHECK(input->value() == Catch::Approx(expected).margin(1e-5));
    visited = true;
    dialog->reject();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK(json(window) == before);
  CHECK_FALSE(window.session().task().active());
  CHECK(window.viewport().sketch_operation_preview_count() == 0U);
}

TEST_CASE("Disconnected line offsets commit together and preserve helper roles",
          "[gui][sketch-offset-project][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  seed_pair_and_axis(window);
  const auto before = json(window);
  GuiSketchOperationRequest offset;
  offset.kind = Kind::Offset;
  offset.entities = {SketchEntityId("line"), SketchEntityId("axis")};
  offset.value = 3;
  auto preview = window.sketch_tools().preview_operation(offset);
  REQUIRE(preview);
  CHECK(preview.value().created_entities.size() == 2U);
  CHECK(json(window) == before);
  REQUIRE(window.sketch_tools().apply_operation(offset));
  const auto* sketch = window.session().part_document()->find_sketch(*window.active_sketch());
  CHECK(sketch->line_segments().size() == 6U);
  CHECK(sketch->line_segments().back().role() == SketchLineRole::Centerline);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

TEST_CASE("Symmetry respects fixed geometry and never leaves a partial constraint batch",
          "[gui][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  seed_pair_and_axis(window);
  for (const auto* id : {"left", "right"}) {
    select_line(window, id);
    REQUIRE(window.sketch_tools().apply_constraint(SketchSolverConstraintKind::Fixed));
  }
  const auto before = json(window);
  const auto old_history = std::string(window.session().undo_label());
  const auto id = *window.active_sketch();
  const auto catalog = window.session().sketch_constraint_catalog(id).value();
  GuiSketchOperationRequest symmetry;
  symmetry.kind = Kind::Symmetric;
  symmetry.axis = SketchEntityId("axis");
  symmetry.entities = {SketchEntityId("left"), SketchEntityId("right")};
  CHECK_FALSE(window.sketch_tools().preview_operation(symmetry));
  CHECK_FALSE(window.sketch_tools().apply_operation(symmetry));
  CHECK(json(window) == before);
  CHECK(window.session().undo_label() == old_history);
  CHECK(window.session().sketch_constraint_catalog(id).value() == catalog);
}

TEST_CASE("Oblique circle projection preserves exact rational conics away from the origin",
          "[gui][sketch-project][sketch-mouse-operations]") {
  ShellWindow window;
  enter(window);
  REQUIRE(window.session().commit_part_transaction("Oblique circle source", [](PartDocument& part) {
    const double a = std::sqrt(.5);
    auto plane = ConstructionPlane::create_explicit(ConstructionPlaneId("tilted"), "Tilted",
                                                    {4, 6, 10}, {1, 0, 0}, {0, a, a}, {0, -a, a});
    if (!plane)
      return Result<std::size_t>::failure(plane.error());
    auto added = part.add_construction_plane(plane.value());
    if (!added)
      return added;
    auto parameter = Parameter::create_length(ParameterId("circle.diameter"), "Diameter",
                                              Quantity::length_mm(20).value());
    if (!parameter)
      return Result<std::size_t>::failure(parameter.error());
    added = part.add_parameter(parameter.value());
    if (!added)
      return added;
    auto source = Sketch::create(SketchId("source.circle"), "Source", DatumPlaneId("tilted"));
    if (!source)
      return Result<std::size_t>::failure(source.error());
    added = source.value().add_profile(
        CircleProfile::create(ProfileId("circle"), ParameterId("circle.diameter"), {5, 7}).value());
    if (!added)
      return added;
    return part.add_sketch(source.value());
  }));
  GuiSketchOperationRequest project;
  project.kind = Kind::ProjectGeometry;
  project.source_sketch = SketchId("source.circle");
  project.source_entity = SketchEntityId("circle");
  const auto before = json(window);
  auto preview = window.sketch_tools().preview_operation(project);
  REQUIRE(preview);
  REQUIRE(preview.value().candidate.spline_segments().size() == 4U);
  for (const auto& spline : preview.value().candidate.spline_segments()) {
    CHECK(spline.rational());
    for (double t : {.1, .4, .8}) {
      const auto p = spline.curve().point(t);
      const double dx = p.x - 9, dy = p.y - (6 + 7 * std::sqrt(.5));
      CHECK(dx * dx / 100 + dy * dy / 50 == Catch::Approx(1).margin(1e-12));
    }
  }
  CHECK(json(window) == before);
  REQUIRE(window.sketch_tools().apply_operation(project));
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
}

namespace {
void finish_spline_by_right_click(ShellWindow& window) {
  QPointF local(window.viewport().rect().center());
  QMouseEvent press(QEvent::MouseButtonPress, local,
                    QPointF(window.viewport().mapToGlobal(local.toPoint())), Qt::RightButton,
                    Qt::RightButton, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &press);
  QMouseEvent release(QEvent::MouseButtonRelease, local,
                      QPointF(window.viewport().mapToGlobal(local.toPoint())), Qt::RightButton,
                      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&window.viewport(), &release);
  qApp->processEvents();
}
void enter_empty_spline_sketch(ShellWindow& window) {
  REQUIRE(window.new_part(QStringLiteral("Spline")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
}
} // namespace

TEST_CASE("Spline accepts unlimited fit points and right-click commits the complete curve once",
          "[gui][spline-create-workflow][sketch-create]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto id = *window.active_sketch();
  const auto before = json(window);
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  const std::vector<Point2> points{{0, 0}, {30, 0}, {50, 20}, {30, 40}, {0, 40}, {-20, 20}, {0, 0}};
  for (Point2 point : points) {
    REQUIRE(tools.add_pick(point));
    CHECK(tools.tool_active());
    CHECK(json(window) == before);
  }
  finish_spline_by_right_click(window);
  CHECK_FALSE(tools.tool_active());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch->spline_segments().size() == points.size() - 1);
  for (std::size_t i = 0; i < sketch->spline_segments().size(); ++i) {
    const auto& segment = sketch->spline_segments()[i];
    CHECK(segment.start() == points[i]);
    CHECK(segment.end() == points[i + 1]);
    const auto& next = sketch->spline_segments()[(i + 1) % sketch->spline_segments().size()];
    const auto incoming = segment.curve().derivative(1);
    const auto outgoing = next.curve().derivative(0);
    CHECK(incoming.x == Catch::Approx(outgoing.x).margin(1e-8));
    CHECK(incoming.y == Catch::Approx(outgoing.y).margin(1e-8));
  }
  const auto after = json(window);
  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  REQUIRE(window.session().redo());
  CHECK(json(window) == after);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("fit-spline.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("fit-spline.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == after);
}

TEST_CASE("Closed fitted spline contours are selectable extrusion inputs with helper geometry",
          "[gui][spline-create-workflow][extrude-revolve]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  bool close_with_line = false, helpers = false, finish_pending = false;
  SECTION("closed spline") {}
  SECTION("spline and closing line") {
    close_with_line = true;
  }
  SECTION("closed spline with crossing helper lines") {
    helpers = true;
  }
  SECTION("Finish Sketch completes pending spline") {
    finish_pending = true;
  }
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  for (Point2 p : {Point2{0, 0}, Point2{40, 0}, Point2{40, 30}, Point2{0, 30}})
    REQUIRE(tools.add_pick(p));
  if (!close_with_line)
    REQUIRE(tools.add_pick({0, 0}));
  if (!finish_pending)
    finish_spline_by_right_click(window);
  if (close_with_line) {
    REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
    REQUIRE(tools.add_pick({0, 30}));
    REQUIRE(tools.add_pick({0, 0}));
    tools.cancel_tool();
  }
  if (helpers) {
    for (auto tool : {GuiSketchCreateTool::Centerline, GuiSketchCreateTool::ConstructionLine}) {
      REQUIRE(tools.begin_tool(tool));
      REQUIRE(tools.add_pick({20, -10}));
      REQUIRE(tools.add_pick({20, 40}));
      tools.cancel_tool();
    }
  }
  if (!finish_pending) {
    auto analysis = geometry::SketchFinishService::analyze_unmaterialized(
        *window.session().part_document(), *window.session().part_document()->find_sketch(id));
    REQUIRE(analysis);
    REQUIRE(analysis.value().diagnostics.empty());
    REQUIRE(analysis.value().regions.size() == 1U);
    auto raw_profile = prepare_interactive_profile(*window.session().part_document(), id,
                                                   analysis.value().regions.front().id);
    REQUIRE(raw_profile);
  }
  REQUIRE(window.finish_active_sketch());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch->arc_closed_profiles().size() == 1U);
  REQUIRE(sketch->spline_segments().size() == (close_with_line ? 3U : 4U));
  const auto profile = sketch->arc_closed_profiles().front().id();
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*window.session().part_document(),
                                                           *window.session().part_shape_cache());
  REQUIRE(scene);
  CHECK(std::any_of(scene.value().begin(), scene.value().end(), [&](const auto& item) {
    return item.semantic_id == "sketch/" + id.value() + "/profile/" + profile.value();
  }));
  const auto before = json(window);
  REQUIRE(window.feature_tools().select_profile(id, profile));
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(10));
  CHECK(json(window) == before);
  REQUIRE(window.feature_tools().apply());
  REQUIRE(window.session().part_shape_cache()->final_shape());
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(
      *window.session().part_shape_cache()->final_shape());
  CHECK(summary.solid_count == 1U);
  CHECK(std::isfinite(summary.volume_mm3));
  CHECK(summary.volume_mm3 > 0);

  REQUIRE(window.session().undo());
  CHECK(json(window) == before);
  REQUIRE(window.session().redo());
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  REQUIRE(window.session().save_as(dir.filePath("spline-extrusion.blcad.json").toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(dir.filePath("spline-extrusion.blcad.json").toStdString()));
  CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == json(window));
}

TEST_CASE("Mouse picks close a pending spline exactly at its first fit point before extrusion",
          "[gui][spline-create-workflow][sketch-ui-review]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  for (Point2 p : {Point2{0, 0}, Point2{30, 0}, Point2{50, 20}, Point2{30, 40}, Point2{0, 40},
                   Point2{-20, 20}, Point2{.5, .5}}) {
    pick_at(window, p);
    qApp->processEvents();
    REQUIRE(tools.tool_active());
  }
  const auto id = *window.active_sketch();
  finish_spline_by_right_click(window);
  REQUIRE_FALSE(tools.tool_active());
  const auto* sketch = window.session().part_document()->find_sketch(id);
  REQUIRE(sketch->spline_segments().size() == 6U);
  CHECK(sketch->spline_segments().front().start() == sketch->spline_segments().back().end());
  capture_ui(window, "closed-fit-spline");
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(10));
  REQUIRE(window.feature_tools().apply());
  window.viewport().fit_all();
  qApp->processEvents();
  capture_ui(window, "fit-spline-extrusion");
}

TEST_CASE("Cancelling a pending spline and finishing a one-point spline leave no geometry",
          "[gui][spline-create-workflow][sketch-create]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto before = json(window);
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  REQUIRE(tools.add_pick({0, 0}));
  REQUIRE(tools.add_pick({20, 10}));
  REQUIRE(tools.add_pick({40, 0}));
  tools.cancel_tool();
  CHECK(json(window) == before);
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  REQUIRE(tools.add_pick({0, 0}));
  finish_spline_by_right_click(window);
  CHECK_FALSE(tools.tool_active());
  CHECK(json(window) == before);
}

TEST_CASE(
    "Completing an open fitted spline preserves the curve without inventing an extrusion face",
    "[gui][spline-create-workflow][sketch-create]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  REQUIRE(tools.add_pick({0, 0}));
  REQUIRE(tools.add_pick({20, 20}));
  REQUIRE(tools.add_pick({40, 0}));
  SECTION("right-click") {
    finish_spline_by_right_click(window);
  }
  SECTION("Enter") {
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&window, &enter);
  }
  CHECK_FALSE(tools.tool_active());
  REQUIRE(window.session().part_document()->find_sketch(id)->spline_segments().size() == 2U);
  REQUIRE(window.finish_active_sketch());
  CHECK(window.session().part_document()->find_sketch(id)->profile_count() == 0U);
  CHECK_FALSE(window.feature_tools().begin_extrude());
}

TEST_CASE("Right-click completes a spline without opening the canvas context menu",
          "[gui][spline-create-workflow][sketch-delivery-review]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  int context_menus = 0;
  window.viewport().set_context_menu_callback([&](QPoint) { ++context_menus; });
  finish_spline_by_right_click(window);
  REQUIRE(context_menus == 1);
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  REQUIRE(tools.add_pick({0, 0}));
  REQUIRE(tools.add_pick({20, 20}));
  REQUIRE(tools.add_pick({40, 0}));
  finish_spline_by_right_click(window);
  CHECK_FALSE(tools.tool_active());
  CHECK(context_menus == 1);
  REQUIRE(window.session()
              .part_document()
              ->find_sketch(*window.active_sketch())
              ->spline_segments()
              .size() == 2U);
  finish_spline_by_right_click(window);
  CHECK(context_menus == 2);
}

TEST_CASE("Copy preview reports dimensions that remain on the original geometry before Apply",
          "[gui][sketch-transform-pattern][sketch-delivery-review]") {
  ShellWindow window;
  enter(window);
  select_line(window);
  REQUIRE(window.sketch_tools().apply_dimension(SketchDimensionKind::Length, false));
  const auto catalog = window.session().sketch_dimension_catalog(*window.active_sketch()).value();
  REQUIRE(catalog.dimensions().size() == 1U);
  const auto dimension_id = QString::fromStdString(catalog.dimensions().front().id().value());
  select_line(window);
  const auto before = json(window);
  auto* action = window.sketch_tools().findChild<QAction*>("sketch.operation.copy");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  bool visited = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = window.findChild<QDialog*>("sketch.operation.dialog");
    REQUIRE(dialog);
    auto* status = dialog->findChild<QLabel*>("sketch.operation.status");
    REQUIRE(status);
    CHECK(status->text().contains(dimension_id));
    CHECK(json(window) == before);
    visited = true;
    dialog->reject();
  });
  action->trigger();
  REQUIRE(visited);
  CHECK(json(window) == before);
}

TEST_CASE(
    "Construction action converts generated groups with unchanged intent and one history entry",
    "[gui][construction-conversion][construction-groups]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto sketch_id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  auto tool = GuiSketchCreateTool::CornerRectangle;
  std::vector<Point2> picks{{0, 0}, {40, 30}};
  SECTION("rectangle") {}
  SECTION("circle") {
    tool = GuiSketchCreateTool::CenterRadiusCircle;
    picks = {{30, 30}, {40, 30}};
  }
  SECTION("slot") {
    tool = GuiSketchCreateTool::CenterSlot;
    picks = {{0, 0}, {30, 0}, {0, 5}};
  }
  SECTION("ellipse") {
    tool = GuiSketchCreateTool::Ellipse;
    picks = {{30, 30}, {50, 30}, {30, 40}};
  }
  REQUIRE(tools.begin_tool(tool));
  for (const auto& point : picks)
    REQUIRE(tools.add_pick(point));
  tools.cancel_tool();
  const auto* source = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(source->profile_count() == 1);
  const auto profile = !source->closed_profiles().empty() ? source->closed_profiles().front().id()
                       : !source->arc_closed_profiles().empty()
                           ? source->arc_closed_profiles().front().id()
                           : source->circle_profiles().front().id();
  const auto original = json(window);
  const auto constraints = serialize_sketch_constraint_catalog_to_json(
                               window.session().sketch_constraint_catalog(sketch_id).value())
                               .value();
  const auto dimensions = serialize_sketch_dimension_catalog_to_json(
                              window.session().sketch_dimension_catalog(sketch_id).value())
                              .value();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + profile.value()}));
  tools.refresh_enablement();
  auto* action = window.findChild<QAction*>("sketch.operation.construction");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  CHECK_FALSE(action->isChecked());
  action->trigger();
  const auto converted = json(window);
  CHECK(converted != original);
  CHECK(
      window.session().part_document()->find_sketch(sketch_id)->profile_has_construction(profile));
  CAPTURE(action->toolTip().toStdString(), window.session().selection().size(),
          window.active_sketch().has_value());
  CHECK(action->isChecked());
  CHECK(serialize_sketch_constraint_catalog_to_json(
            window.session().sketch_constraint_catalog(sketch_id).value())
            .value() == constraints);
  CHECK(serialize_sketch_dimension_catalog_to_json(
            window.session().sketch_dimension_catalog(sketch_id).value())
            .value() == dimensions);
  REQUIRE(tools.convert_construction(true)); // An unchanged role adds no duplicate history entry.
  CHECK(json(window) == converted);
  REQUIRE(window.undo());
  CHECK(json(window) == original);
  REQUIRE(window.redo());
  CHECK(json(window) == converted);
  // Undo may leave the sketch workspace; re-enter it through the persisted browser item.
  if (!window.active_sketch()) {
    auto& browser = window.model_browser();
    for (QTreeWidgetItemIterator it(&browser); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == QString::fromStdString(sketch_id.value())) {
        browser.setCurrentItem(*it);
        browser.itemDoubleClicked(*it, 0);
        break;
      }
  }
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + profile.value()}));
  tools.refresh_enablement();
  REQUIRE(tools.convert_construction(false));
  CHECK(json(window) == original);
}

TEST_CASE(
    "Mixed construction conversion retains dimensions and rejects projected selections atomically",
    "[gui][construction-conversion][construction-selection]") {
  ShellWindow window;
  enter(window);
  const auto sketch = *window.active_sketch();
  auto& tools = window.sketch_tools();
  select_line(window);
  REQUIRE(tools.apply_dimension(SketchDimensionKind::Length, false));
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({30, 30}));
  REQUIRE(tools.add_pick({40, 30}));
  tools.cancel_tool();
  const auto circle =
      window.session().part_document()->find_sketch(sketch)->circle_profiles().front().id();
  REQUIRE(window.session().commit_part_transaction("Projection source", [](PartDocument& part) {
    return part.add_construction_line(
        ConstructionLine::create_explicit(ConstructionLineId("source"), "Source", {0, 20, 0},
                                          {1, 0, 0})
            .value());
  }));
  GuiSketchOperationRequest project;
  project.kind = Kind::ProjectLine;
  project.projected_line = ProjectedSketchLine::create_from_construction_line(
                               SketchEntityId("reference"), ConstructionLineId("source"))
                               .value();
  REQUIRE(tools.apply_operation(project));
  const auto dimensions = serialize_sketch_dimension_catalog_to_json(
                              window.session().sketch_dimension_catalog(sketch).value())
                              .value();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line"}));
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + circle.value()}));
  tools.refresh_enablement();
  auto* action = window.findChild<QAction*>("sketch.operation.construction");
  REQUIRE(action);
  REQUIRE(action->isEnabled());
  const auto before = json(window);
  REQUIRE(tools.convert_construction(true));
  CHECK(window.session()
            .part_document()
            ->find_sketch(sketch)
            ->find_line_segment(SketchEntityId("line"))
            ->construction());
  CHECK(window.session()
            .part_document()
            ->find_sketch(sketch)
            ->find_circle_profile(circle)
            ->construction());
  CHECK(serialize_sketch_dimension_catalog_to_json(
            window.session().sketch_dimension_catalog(sketch).value())
            .value() == dimensions);
  const auto converted = json(window);
  REQUIRE(window.undo());
  CHECK(json(window) == before);
  REQUIRE(window.redo());
  CHECK(json(window) == converted);
  if (!window.active_sketch()) {
    auto& browser = window.model_browser();
    for (QTreeWidgetItemIterator it(&browser); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == QString::fromStdString(sketch.value())) {
        browser.itemDoubleClicked(*it, 0);
        break;
      }
  }
  window.session().selection().clear();
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/line"}));
  REQUIRE(window.session().selection().add({GuiSelectionKind::SketchEntity, "entity/reference"}));
  tools.refresh_enablement();
  CHECK_FALSE(action->isEnabled());
  CHECK(action->toolTip().contains("read-only"));
  CHECK(action->toolTip().contains("detach"));
  const std::string history(window.session().undo_label());
  auto rejected = tools.convert_construction(false);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("read-only") != std::string::npos);
  CHECK(json(window) == converted);
  CHECK(window.session().undo_label() == history);
}

TEST_CASE("Construction conversion updates implicit holes and rejects invalid downstream profiles "
          "atomically",
          "[gui][construction-conversion][construction-downstream]") {
  ShellWindow window;
  enter_empty_spline_sketch(window);
  const auto sketch = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CornerRectangle));
  REQUIRE(tools.add_pick({0, 0}));
  REQUIRE(tools.add_pick({40, 30}));
  tools.cancel_tool();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::CenterRadiusCircle));
  REQUIRE(tools.add_pick({20, 15}));
  REQUIRE(tools.add_pick({25, 15}));
  tools.cancel_tool();
  const auto outer =
      window.session().part_document()->find_sketch(sketch)->closed_profiles().front().id();
  const auto inner =
      window.session().part_document()->find_sketch(sketch)->circle_profiles().front().id();
  REQUIRE(window.finish_active_sketch());
  REQUIRE(window.feature_tools().select_profile(sketch, outer));
  REQUIRE(window.feature_tools().begin_extrude());
  REQUIRE(window.feature_tools().set_numeric_value(5));
  REQUIRE(window.feature_tools().apply());
  const auto body = window.session().part_document()->bodies().front().id();
  const auto volume = [&] {
    return geometry::RectangleExtrusionAdapter{}
        .summarize(*window.session().part_shape_cache()->find_body_shape(body))
        .volume_mm3;
  };
  const auto initial_volume = volume();
  const auto enter_existing = [&] {
    if (window.active_sketch())
      return;
    auto& browser = window.model_browser();
    for (QTreeWidgetItemIterator it(&browser); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == QString::fromStdString(sketch.value())) {
        browser.itemDoubleClicked(*it, 0);
        break;
      }
    REQUIRE(window.active_sketch() == sketch);
  };
  enter_existing();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + inner.value()}));
  REQUIRE(tools.convert_construction(true));
  CHECK(volume() == Catch::Approx(6000));
  CHECK(volume() > initial_volume);
  REQUIRE(window.undo());
  CHECK(volume() == Catch::Approx(initial_volume));
  REQUIRE(window.redo());
  CHECK(volume() == Catch::Approx(6000));
  enter_existing();
  window.session().selection().clear();
  REQUIRE(window.session().selection().add(
      {GuiSelectionKind::SketchEntity, "profile/" + outer.value()}));
  const auto before = json(window);
  const std::string history(window.session().undo_label());
  auto rejected = tools.convert_construction(true);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("Construction") != std::string::npos);
  CHECK(json(window) == before);
  CHECK(volume() == Catch::Approx(6000));
  CHECK(window.session().undo_label() == history);
  CHECK(window.feature_tools().select_profile(sketch, outer));
  CHECK_FALSE(window.feature_tools().select_profile(sketch, inner));
}

TEST_CASE("Unlinked projection inherits the selected curve role across equal profile id text",
          "[gui][construction-conversion][construction-projection]") {
  ShellWindow window;
  enter(window);
  const auto target = *window.active_sketch();
  bool arc = false, has_curve = true, curve_construction = false, circle_construction = true;
  SECTION("Normal spline shadows an auxiliary circle") {}
  SECTION("Normal arc shadows an auxiliary circle") {
    arc = true;
  }
  SECTION("Auxiliary spline shadows a normal circle") {
    curve_construction = true;
    circle_construction = false;
  }
  SECTION("An auxiliary circle alone produces auxiliary conic spans") {
    has_curve = false;
  }
  REQUIRE(window.session().commit_part_transaction(
      "Projection role sources", [&](PartDocument& part) -> Result<std::size_t> {
        auto added = part.add_parameter(Parameter::create_length(ParameterId("projection.diameter"),
                                                                 "Projection diameter",
                                                                 Quantity::length_mm(10).value())
                                            .value());
        if (!added)
          return added;
        auto source = Sketch::create(SketchId("source.projection"), "Projection source",
                                     DatumPlaneId("datum.xy"))
                          .value();
        if (has_curve) {
          added = arc ? source.add_entity(ArcSegment::create_three_point(SketchEntityId("shared"),
                                                                         {20, 0}, {25, 5}, {30, 0})
                                              .value()
                                              .with_construction(curve_construction))
                      : source.add_entity(SplineSegment::create_cubic_bezier(
                                              SketchEntityId("shared"), {20, 0}, {22, 5}, {28, 5},
                                              {30, 0}, {1, .8, .8, 1})
                                              .value()
                                              .with_construction(curve_construction));
          if (!added)
            return added;
        }
        added = source.add_profile(
            CircleProfile::create(ProfileId("shared"), ParameterId("projection.diameter"), {60, 60})
                .value()
                .with_construction(circle_construction));
        if (!added)
          return added;
        return part.add_sketch(std::move(source));
      }));
  GuiSketchOperationRequest request;
  request.kind = Kind::ProjectGeometry;
  request.source_sketch = SketchId("source.projection");
  request.source_entity = SketchEntityId("shared");
  REQUIRE(window.sketch_tools().apply_operation(request));
  const auto* projected = window.session().part_document()->find_sketch(target);
  REQUIRE_FALSE(projected->spline_segments().empty());
  for (const auto& spline : projected->spline_segments()) {
    CHECK(spline.construction() == (has_curve ? curve_construction : circle_construction));
    CHECK(spline.rational());
  }
}
